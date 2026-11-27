"""Put a newly made tail (a Tripo model with its own texture) onto a rigged animal friend in place of its old one, joined
to the body's skin (run in Blender; 2026-11-24, the fox: the user wanted to see a newly generated tail next to the
re-laid one of blender_tail_straight.py).

Usage: blender -b --factory-startup --python blender_tail_graft.py -- <rig fbx> <new tail fbx> <new tail basecolor>
           <body basecolor png> <tail_ref.obj> <out fbx> <out basecolor png> [--cut 0.075] [--gap 0.03] [--rect x,y,size]
           [--no-colour]
  <rig fbx>: blender_tail_straight.py's rig (the tail straight, Tail1.. along it). <tail_ref.obj>: that tail alone,
  blender_tail_refs.py's (the pictures the new tail was made from were of it turned a quarter turn, its length along +X).
  The new tail is turned back, sized and moved onto the old one (a few rounds of closest points), the old tail is cut
  off --cut m along its first bone and the new one --gap m further on, and the two open ends are joined with a band of
  faces. The new tail takes the old chain's bones by where each vertex lies along it.
  Its texture goes into the body's own: the new tail's UVs are moved as they are into the biggest empty square of the
  body's texture (--rect to give the square: the second run, on the parts rig, must use the first run's) and its picture
  is pasted there, its colours pulled to the old tail's (mean and spread, per channel; --no-colour: as it came).
Prints TAILGRAFT lines, the square among them.
"""
import math
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector, kdtree
from mathutils.bvhtree import BVHTree
from mathutils.geometry import intersect_point_line

args = sys.argv[sys.argv.index("--") + 1:]
rig_fbx, tail_fbx, tail_tex, body_tex, ref_obj, out_fbx, out_tex = args[:7]


def opt(name, default):
    return type(default)(args[args.index(name) + 1]) if name in args else default


CUT = opt("--cut", 0.075)
GAP = opt("--gap", 0.03)
RECT = opt("--rect", "")
EASE = opt("--ease", 0.14)
GRID = 512


def smoothstep(e0, e1, x):
    t = max(0.0, min(1.0, (x - e0) / (e1 - e0)))
    return t * t * (3.0 - 2.0 * t)

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=rig_fbx)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
body = next(o for o in bpy.data.objects if o.type == "MESH")
arm.animation_data_clear()
arm.data.pose_position = "REST"
prefix = next(b.name for b in arm.data.bones if b.name.endswith("Hips")).replace("Hips", "")
bones = sorted((b.name for b in arm.data.bones if b.name.startswith(prefix + "Tail")), key=lambda n: int(n.replace(prefix + "Tail", "")))
J = [arm.matrix_world @ arm.data.bones[bones[0]].head_local] + [arm.matrix_world @ arm.data.bones[n].tail_local for n in bones]
axis = (J[1] - J[0]).normalized()
acc = [0.0]
for i in range(1, len(J)):
    acc.append(acc[-1] + (J[i] - J[i - 1]).length)
length = acc[-1]

# ---- the reference tail and the new one, fitted onto it ----
before = set(bpy.data.objects)
bpy.ops.wm.obj_import(filepath=ref_obj, forward_axis="Y", up_axis="Z")
ref = next(o for o in bpy.data.objects if o not in before)
ref_points = [ref.matrix_world @ v.co for v in ref.data.vertices] + [ref.matrix_world @ p.center for p in ref.data.polygons]
ref_surface = BVHTree.FromPolygons([tuple(ref.matrix_world @ v.co) for v in ref.data.vertices], [tuple(p.vertices) for p in ref.data.polygons])
bpy.data.objects.remove(ref)
tree = kdtree.KDTree(len(ref_points))
for i, p in enumerate(ref_points):
    tree.insert(p, i)
tree.balance()
target = np.array([tuple(p) for p in ref_points])

before = set(bpy.data.objects)
bpy.ops.import_scene.fbx(filepath=tail_fbx)
new = next(o for o in bpy.data.objects if o not in before and o.type == "MESH")
src = np.array([tuple(new.matrix_world @ v.co) for v in new.data.vertices])


def fit(points, turn_deg):
    """Similarity transform (4x4) taking the new tail's points onto the reference, starting from a turn about Z."""
    c, s = math.cos(math.radians(turn_deg)), math.sin(math.radians(turn_deg))
    m = np.eye(4)
    m[:3, :3] = np.array([[c, -s, 0.0], [s, c, 0.0], [0.0, 0.0, 1.0]])
    p = points @ m[:3, :3].T
    scale = (target.max(0) - target.min(0)).max() / (p.max(0) - p.min(0)).max()
    m[:3, :3] *= scale
    p = points @ m[:3, :3].T
    m[:3, 3] = (target.max(0) + target.min(0)) / 2 - (p.max(0) + p.min(0)) / 2
    err = 0.0
    for _ in range(40):
        p = points @ m[:3, :3].T + m[:3, 3]
        near = np.array([tuple(tree.find(Vector(q))[0]) for q in p])
        err = float(np.linalg.norm(near - p, axis=1).mean())
        mp, mq = p.mean(0), near.mean(0)
        a, b = p - mp, near - mq
        u, sv, vt = np.linalg.svd(a.T @ b)
        d = np.sign(np.linalg.det(vt.T @ u.T))
        r = vt.T @ np.diag([1.0, 1.0, d]) @ u.T
        k = (sv * [1.0, 1.0, d]).sum() / (a * a).sum()
        step = np.eye(4)
        step[:3, :3] = k * r
        step[:3, 3] = mq - k * r @ mp
        m = step @ m
    return m, err


fits = [(fit(src, turn), turn) for turn in (90.0, 270.0, 0.0, 180.0)]
(m_fit, err), turn = min(fits, key=lambda f: f[0][1])
print("TAILGRAFT fit: " + ", ".join(f"turn {t:.0f} -> {e * 100:.2f} cm" for (_, e), t in fits) + f"; took {turn:.0f}, scale {np.linalg.norm(m_fit[:3, 0]):.3f}")
new.matrix_world = Matrix(m_fit.tolist()) @ new.matrix_world
bpy.ops.object.select_all(action="DESELECT")
new.select_set(True)
bpy.context.view_layer.objects.active = new
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
# Toward its root the new tail takes the old tail's surface (all of it where it is cut, none of it EASE m further out):
# it came out thicker there than the stump it is joined to, and the join showed as a step
eased = 0
for v in new.data.vertices:
    w = 1.0 - smoothstep(-0.01, EASE, (v.co - J[0]).dot(axis) - (CUT + GAP))
    if w > 0.0:
        on_old = ref_surface.find_nearest(v.co)[0]
        if on_old is not None:
            v.co = v.co.lerp(on_old, w)
            eased += 1
print(f"TAILGRAFT {eased} vertices of the new tail eased onto the old tail's surface near the join")

# ---- the texture: where in the body's picture the new tail's goes ----
body_img = bpy.data.images.load(body_tex)
size = body_img.size[0]
uv_name = body.data.uv_layers.active.name
new.data.uv_layers[0].name = uv_name
old_tail_groups = {g.index for g in body.vertex_groups if g.name.startswith(prefix + "Tail")}


def tail_share(v, deform, groups):
    ws = v[deform]
    total = sum(ws.values())
    return sum(w for g, w in ws.items() if g in groups) / total if total > 0.0 else 0.0


def raster(tris, n):
    """Cells of an n x n grid touched by UV triangles (bounding boxes: generous, which is what an 'is it free' test wants)."""
    grid = np.zeros((n, n), bool)
    for t in tris:
        lo = np.clip(np.floor(t.min(0) * n).astype(int) - 1, 0, n - 1)
        hi = np.clip(np.ceil(t.max(0) * n).astype(int) + 1, 0, n)
        grid[lo[1]:hi[1], lo[0]:hi[0]] = True
    return grid


def islands(tris, n):
    """Texels of an n x n picture inside UV triangles."""
    mask = np.zeros((n, n), bool)
    for t in tris:
        t = t[:3] * n
        lo = np.clip(np.floor(t.min(0)).astype(int) - 1, 0, n - 1)
        hi = np.clip(np.ceil(t.max(0)).astype(int) + 1, 1, n)
        xs, ys = np.meshgrid(np.arange(lo[0], hi[0]) + 0.5, np.arange(lo[1], hi[1]) + 0.5)
        d = (t[1, 1] - t[2, 1]) * (t[0, 0] - t[2, 0]) + (t[2, 0] - t[1, 0]) * (t[0, 1] - t[2, 1])
        if abs(d) < 1e-9:
            continue
        a = ((t[1, 1] - t[2, 1]) * (xs - t[2, 0]) + (t[2, 0] - t[1, 0]) * (ys - t[2, 1])) / d
        b = ((t[2, 1] - t[0, 1]) * (xs - t[2, 0]) + (t[0, 0] - t[2, 0]) * (ys - t[2, 1])) / d
        mask[lo[1]:hi[1], lo[0]:hi[0]] |= (a >= -0.02) & (b >= -0.02) & (a + b <= 1.02)
    return mask


plane_old = J[0] + axis * CUT
plane_new = J[0] + axis * (CUT + GAP)
bm = bmesh.new()
bm.from_mesh(body.data)
deform = bm.verts.layers.deform.verify()
uv = bm.loops.layers.uv.verify()
to_world = body.matrix_world
to_local = to_world.inverted()

# 1. the old tail off, past the cut
tail_faces = [f for f in bm.faces if all(tail_share(v, deform, old_tail_groups) > 0.3 for v in f.verts)]
old_uv = [np.array([tuple(l[uv].uv) for l in f.loops][:3]) for f in tail_faces if all(((to_world @ v.co) - plane_old).dot(axis) > 0.0 for v in f.verts)]
geom = list({v for f in tail_faces for v in f.verts}) + list({e for f in tail_faces for e in f.edges}) + tail_faces
bmesh.ops.bisect_plane(bm, geom=geom, dist=1e-6, plane_co=to_local @ plane_old, plane_no=(to_local.to_3x3() @ axis).normalized(),
                       clear_inner=False, clear_outer=True)
bm.verts.ensure_lookup_table()
old_ring = [e for e in bm.edges if e.is_boundary and all(abs(((to_world @ v.co) - plane_old).dot(axis)) < 1e-4 and ((to_world @ v.co) - plane_old).length < 0.25 for v in e.verts)]
print(f"TAILGRAFT old tail cut {CUT * 100:.1f} cm along its first bone: ring of {len(old_ring)} edges, {len(old_uv)} faces gone")
if RECT:
    rx, ry, rs = (float(c) for c in RECT.split(","))
else:
    used = raster([np.array([tuple(l[uv].uv) for l in f.loops]) for f in bm.faces], GRID)
    best = np.zeros((GRID, GRID), int)
    for y in range(GRID):
        for x in range(GRID):
            if not used[y, x]:
                best[y, x] = 1 + (min(best[y - 1, x], best[y, x - 1], best[y - 1, x - 1]) if x and y else 0)
    y, x = np.unravel_index(best.argmax(), best.shape)
    cells = int(best[y, x])
    rs = (cells - 2) / GRID
    rx, ry = (x - cells + 2) / GRID, (y - cells + 2) / GRID
print(f"TAILGRAFT texture square --rect {rx:.5f},{ry:.5f},{rs:.5f} ({rs * size:.0f} px of {size})")

# 2. the new tail in: its root off before the second plane, its UVs into the square, its skin the old chain's
new_bm = bmesh.new()
new_bm.from_mesh(new.data)
bmesh.ops.bisect_plane(new_bm, geom=new_bm.verts[:] + new_bm.edges[:] + new_bm.faces[:], dist=1e-6, plane_co=plane_new, plane_no=axis,
                       clear_inner=True, clear_outer=False)
new_uv = new_bm.loops.layers.uv.verify()
new_tris = [np.array([tuple(l[new_uv].uv) for l in f.loops]) for f in new_bm.faces]
group_index = {g.name: g.index for g in body.vertex_groups}


def skin(v, p):
    best = (1e9, 0.0)
    for i in range(len(J) - 1):
        _, t = intersect_point_line(p, J[i], J[i + 1])
        t = min(1.0, t) if i == 0 else (max(0.0, t) if i == len(J) - 2 else max(0.0, min(1.0, t)))
        d = (p - (J[i] + (J[i + 1] - J[i]) * t)).length
        if d < best[0]:
            best = (d, acc[i] + t * (acc[i + 1] - acc[i]))
    s = best[1]
    mids = [(acc[k] + acc[k + 1]) * 0.5 for k in range(len(bones))]
    d = v[deform]
    d.clear()
    if s <= mids[0]:
        d[group_index[bones[0]]] = 1.0
    elif s >= mids[-1]:
        d[group_index[bones[-1]]] = 1.0
    else:
        k = max(i for i in range(len(bones) - 1) if mids[i] <= s)
        t = (s - mids[k]) / (mids[k + 1] - mids[k])
        if t < 0.999:
            d[group_index[bones[k]]] = 1.0 - t
        if t > 0.001:
            d[group_index[bones[k + 1]]] = t


made = {}
material = old_ring[0].link_faces[0].material_index
new_faces = []
for f in new_bm.faces:
    corners = []
    for l in f.loops:
        if l.vert not in made:
            nv = bm.verts.new(to_local @ l.vert.co)
            skin(nv, l.vert.co)
            made[l.vert] = nv
        corners.append(made[l.vert])
    face = bm.faces.new(corners)
    face.smooth = True
    face.material_index = material
    for l, src_l in zip(face.loops, f.loops):
        l[uv].uv = (rx + src_l[new_uv].uv.x * rs, ry + src_l[new_uv].uv.y * rs)
    new_faces.append(face)
new_ring = [e for e in bm.edges if e.is_boundary and all(v in set(made.values()) for v in e.verts)]
new_bm.free()
bpy.data.objects.remove(new)

# 3. the two open ends joined, the band wearing the new tail's skin (each strip the new face next to it, folded back)
band = bmesh.ops.bridge_loops(bm, edges=old_ring + new_ring)["faces"]
new_face_set = set(new_faces)
ring_new_verts = {v for e in new_ring for v in e.verts}
for face in band:
    face.smooth = True
    face.material_index = material
    # one face of the new tail lends the strip its place in the picture: the one sharing its edge on the new ring, or
    # any at its corner there. (Corners taken from different faces made strips that ran across the picture, through the
    # white of the tip: pale specks round the join.)
    lender = None
    for e in face.edges:
        if all(v in ring_new_verts for v in e.verts):
            lender = next((f for f in e.link_faces if f in new_face_set), None)
    if lender is None:
        corner = next((v for v in face.verts if v in ring_new_verts), None)
        lender = next((f for f in corner.link_faces if f in new_face_set), None) if corner else None
    if lender is None:
        continue
    lent = {l.vert: l[uv].uv.copy() for l in lender.loops}
    spare = max(lender.loops, key=lambda fl: (to_world @ fl.vert.co - plane_new).dot(axis))[uv].uv.copy()
    for l in face.loops:
        l[uv].uv = lent.get(l.vert, spare)
# (no smoothing of the band: it drew the tube in to a waist, which read as a step at the join)
print(f"TAILGRAFT new tail {len(new_faces)} faces in, ring of {len(new_ring)} edges, joined with {len(band)} faces")

bm.normal_update()
bm.to_mesh(body.data)
bm.free()
body.data.update()

# ---- the picture: the new tail's pasted into the square, its colours pulled to the old tail's ----
tail_img = bpy.data.images.load(tail_tex)
side = int(round(rs * size))
tail_img.scale(side, side)
tp = np.empty(side * side * 4, dtype=np.float32)
tail_img.pixels.foreach_get(tp)
tp = tp.reshape(side, side, 4)
bp = np.empty(size * size * 4, dtype=np.float32)
body_img.pixels.foreach_get(bp)
bp = bp.reshape(size, size, 4)
if "--no-colour" not in args:
    old_mask = islands(old_uv, size)
    new_mask = islands(new_tris, side)
    old_px, new_px = bp[old_mask][:, :3], tp[new_mask][:, :3]
    if len(old_px) > 100 and len(new_px) > 100:
        mo, so, mn, sn = old_px.mean(0), old_px.std(0), new_px.mean(0), new_px.std(0)
        tp[:, :, :3] = np.clip((tp[:, :, :3] - mn) * (so / np.maximum(sn, 1e-3)) + mo, 0.0, 1.0)
        print(f"TAILGRAFT colours: new tail mean {np.round(mn, 3)} spread {np.round(sn, 3)} -> the old tail's {np.round(mo, 3)} / {np.round(so, 3)}")
# (the picture's empty parts take the colour of the nearest painted texel, so nothing pale shows along the islands' edges
# at this smaller size or in the game's smaller copies of it)
filled = islands(new_tris, side)
for _ in range(24):
    grown = np.zeros_like(filled)
    total = np.zeros_like(tp[:, :, :3])
    count = np.zeros((side, side), np.float32)
    for dy, dx in ((0, 1), (0, -1), (1, 0), (-1, 0)):
        shifted = np.roll(filled, (dy, dx), (0, 1))
        total += np.roll(tp[:, :, :3], (dy, dx), (0, 1)) * shifted[:, :, None]
        count += shifted
        grown |= shifted
    fresh = grown & ~filled
    tp[fresh, :3] = total[fresh] / count[fresh][:, None]
    filled |= fresh
x0, y0 = int(round(rx * size)), int(round(ry * size))
bp[y0:y0 + side, x0:x0 + side, :3] = tp[:, :, :3]
bp[y0:y0 + side, x0:x0 + side, 3] = 1.0
out_img = bpy.data.images.new("out", size, size, alpha=True)
out_img.pixels.foreach_set(bp.reshape(-1))
out_img.filepath_raw = out_tex
out_img.file_format = "PNG"
out_img.save()
print(f"TAILGRAFT wrote {out_tex}")

bpy.ops.object.select_all(action="DESELECT")
bpy.ops.export_scene.fbx(
    filepath=out_fbx, use_selection=False, object_types={"ARMATURE", "MESH"}, add_leaf_bones=False, bake_anim=False,
    primary_bone_axis="Y", secondary_bone_axis="X", use_armature_deform_only=False, mesh_smooth_type="OFF",
    path_mode="STRIP")
print(f"TAILGRAFT wrote {out_fbx}: {len(body.data.vertices)} vertices, {len(body.data.polygons)} faces")
