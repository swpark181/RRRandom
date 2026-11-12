"""Shell fur for a rigged animal friend, as its own skinned mesh on the same skeleton (run inside Blender; round 95).

Input: the rig FBX the game mesh was made from (armature + one skinned body, e.g. input/fox_film/rig_mx/fox_rig.fbx) and
the body's base colour. Output: an FBX with the same armature and only the fur shells, to import as SK_<Name>Fur onto
SK_<Name>_Skeleton (ue_import_fur_shells.py); the game draws it over the body following its pose (URRRandomFurComponent).

Fur faces = fur texels by --mask: "orange" (the fox: R - B > 0.2, G / R > 0.38) or "cream" (the rabbit: light and pale,
brightest channel > 0.62 and saturation (max - min) / max < 0.32, which leaves out its ochre shorts, plum shirt, pink nose
and gold moon), not near the eye whites, not the belt band on the front
of the waist, small holes filled; per vertex the share of fur faces round it (w) fades the fur out at its edges.
Shells: a dark opaque base just over the skin (the roots' shade, so gaps between strands aren't bright dots), then
<layers> layers out to <length> x the body height along the vertex normal with a little sag (baked; wind, drag and
springs are added in the material from the game). Every shell vertex takes its root's bone weights.
Vertex colour (linear): R = h (0 base .. 1 tip), G = w, B = 1 on the base layer. UV0 = the body's (its texture),
UV1 = "FurUV": the fur faces unwrapped evenly, 1 UV unit = 64 strand cells of <cell> x the height, for the tiling strand
texture this also writes (<out>_strands.png: R = distance to the strand's centre in cell widths, G = the strand's length
0..1, B = a random phase; 1024 px, 64 x 64 cells, wraps).

Usage: blender -b --factory-startup --python blender_fur_shells_skel.py -- <rig fbx> <base color> <out fbx>
       [--layers 12] [--length 0.02] [--cell 0.005] [--mask orange|cream|all]
       with --mask all: [--band <z lo>,<z hi>,<torso radius>] (fractions of the height: the clothes' heights and the
       torso's radius round its middle there) [--cloth white,beige,green,dark,red] (clothing colours anywhere in that band,
       for sleeves) [--dark-head] (a black eye mask) [--iris yellow|green] (eyes with no white round them)
  --mask all is for animal friends whose fur colours are also their clothes' (a tiger's white cheeks, a white shirt):
  fur everywhere but the band's clothes, pink bare skin, the eye mask and the eyes (small white or iris clusters).
"""
import math
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:]
rig_fbx, tex_path, out_fbx = argv[:3]


def opt(name, default, cast=float):
    return cast(argv[argv.index(name) + 1]) if name in argv else default


layers = opt("--layers", 12, int)
length = opt("--length", 0.02)
cell = opt("--cell", 0.005)  # the strand spacing (x the height): in the game about 9 mm, a few pixels at play distance
mask = opt("--mask", "orange", str)
CELLS = 64

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=rig_fbx)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
body = next(o for o in bpy.data.objects if o.type == "MESH")
mw = body.matrix_world.copy()
mw3 = mw.to_3x3()

img = bpy.data.images.load(tex_path)
W, H = img.size
pix = np.empty(W * H * 4, dtype=np.float32)
img.pixels.foreach_get(pix)
pix = pix.reshape(H, W, 4)


def sample(uv):
    return pix[min(H - 1, max(0, int(uv[1] % 1.0 * H))), min(W - 1, max(0, int(uv[0] % 1.0 * W))), :3]


bm = bmesh.new()
bm.from_mesh(body.data)
bm.faces.ensure_lookup_table()
bm.verts.ensure_lookup_table()
uv_layer = bm.loops.layers.uv.active
deform = bm.verts.layers.deform.active
wco = np.array([tuple(mw @ v.co) for v in bm.verts])
wno = np.array([tuple((mw3 @ v.normal).normalized()) for v in bm.verts])
zlo, zhi = wco[:, 2].min(), wco[:, 2].max()
height = zhi - zlo

face_col = np.array([sample(sum((l[uv_layer].uv for l in f.loops), Vector((0, 0))) / len(f.loops)) for f in bm.faces])
r, g, b = face_col[:, 0], face_col[:, 1], face_col[:, 2]
mx, mn = face_col.max(1), face_col.min(1)
sat = (mx - mn) / np.maximum(mx, 1e-3)
centres = np.array([wco[[v.index for v in f.verts]].mean(0) for f in bm.faces])
zrel = (centres[:, 2] - zlo) / height
bare = np.zeros(len(centres), bool)
if mask == "all":
    # Fur everywhere but: the clothes (inside the band's torso cylinder, and clothing colours anywhere in the band),
    # pink bare skin, black eye masks, the eyes (small white clusters and iris colours on the head)
    fur = np.ones(len(centres), bool)
    band = opt("--band", "0.15,0.40,0.12", str)
    b_lo, b_hi, b_r = (float(c) for c in band.split(","))
    in_band = (zrel > b_lo) & (zrel < b_hi)
    axis = np.median(centres[in_band][:, :2], axis=0) if in_band.any() else centres[:, :2].mean(0)
    core = np.linalg.norm(centres[:, :2] - axis, axis=1) < b_r * height
    cloth_rules = {
        "white": (sat < 0.12) & (mx > 0.7),
        "beige": (sat < 0.32) & (mx > 0.6),
        "green": (g >= 0.9 * r) & (g > b),
        "dark": mx < 0.18,
        "red": (r > 0.45) & (g < 0.22) & (b < 0.22),
        "gold": (r > 0.6) & (g > 0.45) & (b < 0.25),  # buckles
        "brown": (r > g) & (g > b) & (g > 0.45 * r) & (g < 0.75 * r) & (mx < 0.75) & (sat > 0.35),  # leather belts, pouches
    }
    cloth = np.zeros(len(centres), bool)
    for name in [c for c in opt("--cloth", "", str).split(",") if c]:
        cloth |= cloth_rules[name]
    bare |= in_band & (core | cloth)
    bare |= sat > float(opt("--max-sat", 1.0))  # (a grey animal: anything coloured is skin or cloth)
    bare |= (r > 0.45) & (r - g > 0.15) & (b > 0.3) & (b < r) & (b > 0.9 * g)  # pink (blue about green: not cream): noses, inner ears, a rat's hands, feet, tail
    if "--dark-head" in argv:
        bare |= (mx < float(opt("--dark", 0.2))) & (zrel > float(opt("--dark-from", 0.55)))  # a bandit's eye mask (--dark: how dark)
    # eyes: small clusters of white (sclera) or iris colour on the head; bigger white patches are fur (a tiger's cheeks)
    iris = {"yellow": (r > 0.55) & (g > 0.45) & (b < 0.3) & (sat > 0.45),
            "green": (g > r + 0.1) & (g > b) & (sat > 0.3)}.get(opt("--iris", "", str), np.zeros(len(centres), bool))
    # (only on the front of the head: light ear backs are no eyes)
    head_front = np.percentile(centres[zrel > 0.5][:, 1], 2)
    cand = ((mn > 0.78) & (sat < 0.12) | iris) & (zrel > 0.5) & (centres[:, 1] < head_front + 0.12 * height)
    seen = np.zeros(len(centres), bool)
    eye_white = np.zeros(len(centres), bool)
    for start in np.nonzero(cand)[0]:
        if seen[start]:
            continue
        comp, stack = [], [int(start)]
        seen[start] = True
        while stack:
            fi = stack.pop()
            comp.append(fi)
            for e in bm.faces[fi].edges:
                for g2 in e.link_faces:
                    if cand[g2.index] and not seen[g2.index]:
                        seen[g2.index] = True
                        stack.append(g2.index)
        pts = centres[comp]
        size = np.linalg.norm(pts.max(0) - pts.min(0))
        if size < 0.07 * height:
            eye_white[comp] = True
            mid = pts.mean(0)
            bare |= (np.linalg.norm(centres - mid, axis=1) < size / 2 + 0.02 * height) & (zrel > 0.45)
else:
    if mask == "cream":
        fur = (mx > 0.62) & (sat < 0.32)
    else:
        fur = (r - b > 0.2) & (g / np.maximum(r, 1e-3) > 0.38)
    eye_white = (face_col.min(1) > 0.85) & (face_col.max(1) - face_col.min(1) < 0.08) & (centres[:, 2] > zlo + 0.6 * height)
    for e in centres[eye_white][:: max(1, int(eye_white.sum()) // 400)]:
        bare |= np.linalg.norm(centres - e, axis=1) < 0.035 * height
    # the waist band on the front half: the leather belt and pouches are brown like fur (the tail is behind)
    front = centres[:, 1] < (centres[:, 1].min() + centres[:, 1].max()) / 2 + 0.05 * height
    bare |= (centres[:, 2] > zlo + 0.15 * height) & (centres[:, 2] < zlo + 0.40 * height) & front
fur &= ~bare
nbrs = [[g2.index for e in f.edges for g2 in e.link_faces if g2.index != f.index] for f in bm.faces]
for _ in range(2):
    vote = np.array([fur[n].mean() if n else 0.0 for n in nbrs])
    fur = (fur | (vote >= 0.6)) & ~bare
# drop stray specks: fur patches of fewer than 12 joined faces (a few dull texels on a pink tail, a stripe's edge)
seen = np.zeros(len(fur), bool)
for start in np.nonzero(fur)[0]:
    if seen[start]:
        continue
    comp, stack = [], [int(start)]
    seen[start] = True
    while stack:
        fi = stack.pop()
        comp.append(fi)
        for n2 in nbrs[fi]:
            if fur[n2] and not seen[n2]:
                seen[n2] = True
                stack.append(n2)
    if len(comp) < 12:
        fur[comp] = False
vw = np.array([np.mean([fur[f.index] for f in v.link_faces]) if v.link_faces else 0.0 for v in bm.verts])
print(f"SHELLS fur faces {int(fur.sum())} of {len(fur)}; height {height:.3f}; eye-white faces {int(eye_white.sum())}")

fur_faces = [f for f, k in zip(bm.faces, fur) if k]
vidx = sorted({v.index for f in fur_faces for v in f.verts})
remap = {vi: i for i, vi in enumerate(vidx)}
R = len(vidx)
L = length * height
down = np.array([0.0, 0.0, -1.0])

# ---- an even unwrap of the fur faces (one layer), scaled to strand cells ----
tmp_me = bpy.data.meshes.new("FurOnce")
tmp_me.from_pydata([tuple(wco[i]) for i in vidx], [], [[remap[v.index] for v in f.verts] for f in fur_faces])
tmp_me.uv_layers.new(name="FurUV")
tmp = bpy.data.objects.new("FurOnce", tmp_me)
bpy.context.scene.collection.objects.link(tmp)
bpy.ops.object.select_all(action="DESELECT")
tmp.select_set(True)
bpy.context.view_layer.objects.active = tmp
bpy.ops.object.mode_set(mode="EDIT")
bpy.ops.mesh.select_all(action="SELECT")
bpy.ops.uv.smart_project(angle_limit=math.radians(66), island_margin=0.002, scale_to_bounds=False)
bpy.ops.object.mode_set(mode="OBJECT")
fuv = np.empty(len(tmp_me.loops) * 2, dtype=np.float32)
tmp_me.uv_layers["FurUV"].data.foreach_get("uv", fuv)
fuv = fuv.reshape(-1, 2)
# world length per UV length, over all faces
wa = ua = 0.0
for poly in tmp_me.polygons:
    pts = [Vector(wco[vidx[i]]) for i in poly.vertices]
    us = [Vector(tuple(fuv[li])) for li in poly.loop_indices]
    for k in range(1, len(pts) - 1):
        wa += ((pts[k] - pts[0]).cross(pts[k + 1] - pts[0])).length / 2
        ua += abs((us[k] - us[0]).cross(us[k + 1] - us[0])) / 2
world_per_uv = math.sqrt(wa / max(ua, 1e-12))
fuv *= world_per_uv / (CELLS * cell * height)
print(f"SHELLS FurUV: 1 uv = {CELLS} cells of {cell * height * 100:.2f} cm (body units)")
bpy.data.objects.remove(tmp)

# ---- the shells: a base, then the layers ----
hs_layers = [0.04] + [k / layers for k in range(1, layers + 1)]
verts, faces, uv0, uv1, cols, groups = [], [], [], [], [], []
for li, h in enumerate(hs_layers):
    base = li == 0
    off = li * R
    for i in vidx:
        w = vw[i]
        p = wco[i] + (wno[i] * L * h + down * L * 0.35 * h * h) * w
        verts.append(tuple(mw.inverted() @ Vector(tuple(p))))
        cols.append((h, w, 1.0 if base else 0.0, 1.0))
    loop = 0
    for f in fur_faces:
        faces.append([off + remap[v.index] for v in f.verts])
        for l in f.loops:
            uv0.append(tuple(l[uv_layer].uv))
            uv1.append(tuple(fuv[loop]))
            loop += 1

me = bpy.data.meshes.new("FurShells")
me.from_pydata(verts, [], faces)
me.shade_smooth()  # smooth like the body, or the shells light up in facets
me.uv_layers.new(name="UVMap").data.foreach_set("uv", [c for uv in uv0 for c in uv])
me.uv_layers.new(name="FurUV").data.foreach_set("uv", [c for uv in uv1 for c in uv])
ca = me.color_attributes.new("FurData", "FLOAT_COLOR", "POINT")
ca.data.foreach_set("color", [c for col in cols for c in col])
me.color_attributes.active_color = ca
shells = bpy.data.objects.new(f"{body.name}_Fur", me)
bpy.context.scene.collection.objects.link(shells)
shells.parent = arm
shells.matrix_world = mw
for vg in body.vertex_groups:
    shells.vertex_groups.new(name=vg.name)
for li in range(len(hs_layers)):
    for i in vidx:
        for gi, wt in bm.verts[i][deform].items():
            shells.vertex_groups[gi].add([li * R + remap[i]], wt, "REPLACE")
mod = shells.modifiers.new("Armature", "ARMATURE")
mod.object = arm
print(f"SHELLS {len(hs_layers)} layers (base + {layers}), {len(verts)} verts, {len(faces)} faces, fur {L * 100:.2f} cm (body units)")

# ---- export the armature and the shells only ----
bpy.data.objects.remove(body)
bpy.ops.object.select_all(action="DESELECT")
arm.select_set(True)
shells.select_set(True)
bpy.context.view_layer.objects.active = arm
bpy.ops.export_scene.fbx(
    filepath=out_fbx, use_selection=True, object_types={"ARMATURE", "MESH"}, add_leaf_bones=False, bake_anim=False,
    primary_bone_axis="Y", secondary_bone_axis="X", use_armature_deform_only=False, mesh_smooth_type="OFF",
    colors_type="LINEAR", path_mode="STRIP")
print(f"SHELLS wrote {out_fbx}")

# ---- the strand texture: jittered cells, wrapping ----
N = 1024
cpx = N // CELLS
rng = np.random.default_rng(11)
jit = rng.uniform(0.15, 0.85, (CELLS, CELLS, 2))
lens = rng.uniform(0.0, 1.0, (CELLS, CELLS))
phase = rng.uniform(0.0, 1.0, (CELLS, CELLS))
yy, xx = np.mgrid[0:N, 0:N].astype(np.float32) / cpx
cx, cy = np.floor(xx).astype(int), np.floor(yy).astype(int)
best = np.full((N, N), 9.0, np.float32)
bl = np.zeros((N, N), np.float32)
bp = np.zeros((N, N), np.float32)
for dy in (-1, 0, 1):
    for dx in (-1, 0, 1):
        ix, iy = (cx + dx) % CELLS, (cy + dy) % CELLS
        px_ = cx + dx + jit[iy, ix, 0]
        py_ = cy + dy + jit[iy, ix, 1]
        d = np.sqrt((xx - px_) ** 2 + (yy - py_) ** 2)
        m = d < best
        best[m] = d[m]
        bl[m] = lens[iy, ix][m]
        bp[m] = phase[iy, ix][m]
tex = np.stack([np.clip(best, 0, 1), bl, bp, np.ones_like(best)], axis=-1)
out_png = out_fbx[:-4] + "_strands.png"
im = bpy.data.images.new("strands", N, N, alpha=True, float_buffer=True)
im.colorspace_settings.name = "Non-Color"
im.pixels.foreach_set(tex[::-1].ravel())
im.filepath_raw = out_png
im.file_format = "PNG"
im.save()
print(f"SHELLS wrote {out_png}")
