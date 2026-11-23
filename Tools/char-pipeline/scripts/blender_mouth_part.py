"""A muzzle part shaped to an expression's mouth (run inside Blender; 2026-11-23).

The user: "표정별로 얼굴 3d 별도로 사용하기로 했는데 입 모양에 맞게 조금씩 변형을 주면 좋을것 같다." Every expression has its own
muzzle part (blender_split_part.py's <out>_muzzle.fbx is the default one); this makes a variant of it whose surface follows
the mouth PAINTED in that expression's texture, a little:
- where the texture is dark inside the mouth ellipse (the inside of an open mouth, a mouth line), the surface sinks along
  its normal, at most --depth; the painted shape is read at the part's own vertices through their UVs, so it lines up
  with the picture whatever view the picture was baked from, and is softened over the mesh (not in the atlas, whose
  islands cut the mouth);
- --bump x,z,r,h: a small round rise (a fang tip), given by where it is seen from the front.
The mouth area is cut finer first (--cuts). The rim of the part does not move (--rim), so it still closes with the body.
Shading: each moved vertex keeps the file's smooth normal, tilted by how the surface turned.

Usage: blender -b --factory-startup --python blender_mouth_part.py -- <muzzle fbx> <expression texture> <out fbx>
         --mouth x,z,rx,rz        the ellipse seen from the front in which dark = mouth (x from the Head bone, z a height;
                                  the rigs stand on z = 0 and are 1 tall, so these are the fractions the fur mask uses)
         [--depth 0.006] [--dark 0.30] [--soft 3] (rounds of softening over the mesh) [--cuts 3] [--rim 0.012]
         [--red 0.25[,0.6]] (also sink what is red: red minus green above the first number, by the second number's
                                  share of --depth; for a tongue, which is brighter than --dark)
         [--box 2] [--gain 1]     for thin mouth lines: the half size in texels of the square read at each vertex, and what
                                  its dark share is multiplied by
         [--fill 0]               for an open mouth: close the painted shape over the mesh by this many steps, so teeth
                                  and tongue inside it sink with it (one smooth hollow instead of spikes)
         [--back 0]               0..1: sink straight back into the head instead of along the normals (1 = fully); for
                                  a wide open mouth, which otherwise rolls its lower edge up into view
         [--bump "x,z,r,h;..."]
         [--probe [--plot <png>]] write no part: only print where the painted mouth lies inside the ellipse, and draw the
                                  texture at the vertices on a 0.01 grid to place the ellipse
Prints "MOUTHPART ..." lines; the last is "MOUTHPART wrote <out>".
"""
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Vector, kdtree

argv = sys.argv[sys.argv.index("--") + 1:]
part_fbx, tex_path, out_fbx = argv[:3]


def opt(name, default, cast=float):
    return cast(argv[argv.index(name) + 1]) if name in argv else default


mx_, mz_, mrx, mrz = (float(c) for c in opt("--mouth", "0,0.5,0.07,0.03", str).split(","))
depth = opt("--depth", 0.006)
dark_at = opt("--dark", 0.30)
soft = opt("--soft", 3, int)
cuts = opt("--cuts", 3, int)
rim_keep = opt("--rim", 0.012)
bumps = [[float(c) for c in e.split(",")] for e in opt("--bump", "", str).split(";") if e]
box_r = opt("--box", 2, int)
fill = opt("--fill", 0, int)
back = opt("--back", 0.0)
gain = opt("--gain", 1.0)
red_at, red_w = ([float(c) for c in opt("--red", "", str).split(",")] + [0.6])[:2] if "--red" in argv else (9.0, 0.0)

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=part_fbx)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
arm.animation_data_clear()
arm.data.pose_position = "REST"
part = max([o for o in bpy.data.objects if o.type == "MESH"], key=lambda o: len(o.data.polygons))
mw = part.matrix_world.copy()
inv = mw.inverted()
rot = mw.to_3x3()
rot_inv = rot.inverted()
hx = next((arm.matrix_world @ b.head_local).x for b in arm.data.bones if b.name.split(":")[-1] == "Head")

img = bpy.data.images.load(tex_path)
W, Ht = img.size
pix = np.empty(W * Ht * img.channels, dtype=np.float32)
img.pixels.foreach_get(pix)
luma = pix.reshape(Ht, W, img.channels)[:, :, :3] @ np.array([0.299, 0.587, 0.114], dtype=np.float32)
# --red: the tongue and gums of an open mouth are too bright for --dark; red minus green tells them from cream or white fur
redness = pix.reshape(Ht, W, img.channels)[:, :, 0] - pix.reshape(Ht, W, img.channels)[:, :, 1]


def dark_share(uv):
    # the share of dark texels in a small square round a UV (5 x 5 texels of a 4K texture: about a millimetre).
    # A thin mouth line passes between the vertices of a coarse part: --box 8 --gain 4 looks further and counts a line
    # that only crosses the square as fully dark.
    x, y = int(uv[0] % 1.0 * W), int(uv[1] % 1.0 * Ht)
    box = luma[max(0, y - box_r):y + box_r + 1, max(0, x - box_r):x + box_r + 1]
    if not box.size:
        return 0.0
    share = min(1.0, gain * float((box < dark_at).mean()))
    if red_w > 0:
        red = redness[max(0, y - box_r):y + box_r + 1, max(0, x - box_r):x + box_r + 1]
        share = max(share, red_w * min(1.0, gain * float((red > red_at).mean())))
    return share


def in_mouth(w, grow=1.0):
    return ((w.x - hx - mx_) / (mrx * grow)) ** 2 + ((w.z - mz_) / (mrz * grow)) ** 2 < 1.0


def near_bump(w, grow=1.0):
    return any((w.x - hx - bx) ** 2 + (w.z - bz) ** 2 < (br * grow) ** 2 for bx, bz, br, bh in bumps)


# the file's smooth normals, kept through the cutting as a vertex layer
orig_n = np.zeros((len(part.data.vertices), 3))
for lp, cnr in zip(part.data.loops, part.data.corner_normals):
    orig_n[lp.vertex_index] += np.array(cnr.vector)
bm = bmesh.new()
bm.from_mesh(part.data)
n0 = bm.verts.layers.float_vector.new("n0")
for v in bm.verts:
    v[n0] = Vector(orig_n[v.index])
uv_layer = bm.loops.layers.uv.active
before = len(bm.verts)
rim_verts = {v for e in bm.edges if e.is_boundary for v in e.verts}


def front(v):  # faces the viewer in front (-Y) or downward-forward, not the back of the part
    n = (rot @ Vector(v[n0]))
    return n.length > 1e-6 and n.normalized().y < 0.35


if cuts > 0:
    faces = [f for f in bm.faces
             if all((in_mouth(mw @ v.co, 1.25) or near_bump(mw @ v.co, 2.0)) and front(v) and v not in rim_verts for v in f.verts)]
    edges = list({e for f in faces for e in f.edges})
    bmesh.ops.subdivide_edges(bm, edges=edges, cuts=cuts, use_grid_fill=True)
    bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 3])
    print(f"MOUTHPART cut {len(faces)} faces finer: {before} -> {len(bm.verts)} vertices")
bm.verts.ensure_lookup_table()
bm.verts.index_update()
bm.normal_update()
geo_before = [v.normal.copy() for v in bm.verts]
world = [mw @ v.co for v in bm.verts]

# how far from the rim: nothing moves on it, full movement from --rim inward
rim_now = [v for e in bm.edges if e.is_boundary for v in e.verts]
kd = kdtree.KDTree(len(rim_now))
for i, v in enumerate(rim_now):
    kd.insert(world[v.index], i)
kd.balance()

# the painted mouth at each vertex, softened over the mesh
m = np.zeros(len(bm.verts))
for v in bm.verts:
    if in_mouth(world[v.index]) and front(v) and v.link_loops:
        m[v.index] = max(dark_share(l[uv_layer].uv) for l in v.link_loops)
painted = int((m > 0.5).sum())
if painted:  # where the painted mouth is, in --mouth's own coordinates (to check or tighten the ellipse)
    px = [world[i].x - hx for i in np.nonzero(m > 0.5)[0]]
    pz = [world[i].z for i in np.nonzero(m > 0.5)[0]]
    print(f"MOUTHPART painted x {min(px):+.4f}..{max(px):+.4f}, z {min(pz):.4f}..{max(pz):.4f} "
          f"(z quarters {np.percentile(pz, 25):.4f} {np.percentile(pz, 50):.4f} {np.percentile(pz, 75):.4f})")
if "--probe" in argv:
    plot = opt("--plot", "", str)
    if plot:  # the texture's colours at the front vertices, drawn in --mouth's coordinates: 4000 px per unit, a grid line
        # every 0.01 (brighter every 0.05), the window = the ellipse grown 1.6 times, the ellipse itself in green
        S = 4000.0
        x0, x1, z0, z1 = mx_ - mrx * 1.6, mx_ + mrx * 1.6, mz_ - mrz * 1.6, mz_ + mrz * 1.6
        pw, ph = int((x1 - x0) * S), int((z1 - z0) * S)
        can = np.full((ph, pw, 4), (0.25, 0.25, 0.27, 1.0), dtype=np.float32)
        rgb = pix.reshape(Ht, W, img.channels)
        for v in bm.verts:
            w = world[v.index]
            if not (front(v) and v.link_loops and x0 < w.x - hx < x1 and z0 < w.z < z1):
                continue
            uv = v.link_loops[0][uv_layer].uv
            c = rgb[int(uv[1] % 1.0 * Ht), int(uv[0] % 1.0 * W), :3]
            cx, cy = int((w.x - hx - x0) * S), int((w.z - z0) * S)
            can[max(0, cy - 4):cy + 5, max(0, cx - 4):cx + 5, :3] = c
        for g in np.arange(np.ceil(x0 * 100) / 100, x1, 0.01):
            can[:, int((g - x0) * S) % pw, :3] = 0.9 if abs(g * 20 - round(g * 20)) < 1e-6 else 0.5
        for g in np.arange(np.ceil(z0 * 100) / 100, z1, 0.01):
            can[int((g - z0) * S) % ph, :, :3] = 0.9 if abs(g * 20 - round(g * 20)) < 1e-6 else 0.5
        for a in np.linspace(0, 2 * np.pi, 1500):
            ex, ez = int((mx_ + mrx * np.cos(a) - x0) * S), int((mz_ + mrz * np.sin(a) - z0) * S)
            can[max(0, ez - 1):ez + 1, max(0, ex - 1):ex + 1, :3] = (0.1, 1.0, 0.2)
        out = bpy.data.images.new("plot", pw, ph)
        out.pixels.foreach_set(can.ravel())
        out.filepath_raw = plot
        out.file_format = "PNG"
        out.save()
        print(f"MOUTHPART plot {plot}: x {x0:+.3f}..{x1:+.3f}, z {z0:.3f}..{z1:.3f} (bottom to top), {pw}x{ph}")
    print("MOUTHPART probe only, nothing written")
    sys.exit(0)
nbrs = [[e.other_vert(v).index for e in v.link_edges] for v in bm.verts]
if fill > 0:
    # an open mouth as one hollow: what the painted mouth surrounds (teeth, a bright tongue) sinks with it, so the teeth
    # do not stand up as spikes on a coarse part. Grow over the mesh, then shrink back by the same number of steps.
    for _ in range(fill):
        m = np.array([max(m[i], max(m[n]) if n else 0.0) for i, n in enumerate(nbrs)])
    for _ in range(fill):
        m = np.array([min(m[i], min(m[n]) if n else 0.0) for i, n in enumerate(nbrs)])
for _ in range(soft):
    m = np.array([0.5 * m[i] + 0.5 * (np.mean(m[n]) if n else m[i]) for i, n in enumerate(nbrs)])

moved = 0
deepest = 0.0
for v in bm.verts:
    w = world[v.index]
    n = rot @ Vector(v[n0])
    if n.length < 1e-6:
        continue
    n.normalize()
    keep = min(1.0, kd.find(w)[2] / max(rim_keep, 1e-6))
    keep = keep * keep * (3 - 2 * keep)
    t = min(1.0, max(0.0, (m[v.index] - 0.1) / 0.6))
    d = -depth * t * t * (3 - 2 * t)
    for bx, bz, br, bh in bumps:
        if front(v):
            r2 = ((w.x - hx - bx) ** 2 + (w.z - bz) ** 2) / (br * br)
            if r2 < 4.0:
                d += bh * float(np.exp(-2.0 * r2))
    d *= keep
    if abs(d) > 1e-6:
        if back > 0 and d < 0:
            # a wide open mouth wraps round the chin: sunk along the normals its lower edge rolls up into view and the
            # picture no longer looks as painted from the front. Straight back (+Y; the face looks along -Y) keeps it.
            n = (n * (1.0 - back) + Vector((0.0, -1.0, 0.0)) * back).normalized()
        v.co = inv @ (w + n * d)
        moved += 1
        deepest = min(deepest, d)
bm.normal_update()
new_n = {}
for v in bm.verts:
    turn = rot @ (v.normal - geo_before[v.index])
    if turn.length > 1e-4:
        new_n[v.index] = (rot_inv @ ((rot @ Vector(v[n0])).normalized() + turn)).normalized()
bm.verts.layers.float_vector.remove(n0)
bm.to_mesh(part.data)
bm.free()
part.data.update()
loop_n = [new_n.get(lp.vertex_index) or cnr.vector.copy() for lp, cnr in zip(part.data.loops, part.data.corner_normals)]
part.data.normals_split_custom_set([tuple(n) for n in loop_n])
part.data.update()
loose = sum(1 for v in part.data.vertices if not v.groups)
print(f"MOUTHPART {painted} vertices on the painted mouth, {moved} moved, deepest {-deepest:.4f}, "
      f"{len(new_n)} normals turned, {loose} without bone weights")

bpy.ops.object.select_all(action="DESELECT")
part.select_set(True)
arm.select_set(True)
bpy.ops.export_scene.fbx(
    filepath=out_fbx, use_selection=True, object_types={"ARMATURE", "MESH"}, add_leaf_bones=False, bake_anim=False,
    primary_bone_axis="Y", secondary_bone_axis="X", use_armature_deform_only=False, mesh_smooth_type="OFF",
    path_mode="STRIP")
print(f"MOUTHPART wrote {out_fbx}")
