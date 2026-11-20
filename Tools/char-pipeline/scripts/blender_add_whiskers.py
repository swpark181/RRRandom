"""Add whiskers to a rigged animal friend (run inside Blender): thin tapered three-sided strands from points on the
muzzle, skinned to the head bone, joined into the body mesh (2026-11-20, the blank-face tiger: the old Tripo tiger had
whiskers as thin triangles in its mesh, the new blank-face one had none, and the user wanted them back).

Usage:
  blender -b --factory-startup --python blender_add_whiskers.py -- <rig fbx> <cam json> <out fbx> "<x,y,deg;...>"
        [--length 0.15] [--root 0.003] [--tip 0.0006] [--back 0.45] [--droop 0.012] [--segments 8]
  Each root is a pixel x,y on the front render of blender_face_audit.py (whose <prefix>_cam.json is <cam json>) and the
  strand's angle in that view (degrees up from horizontal, pointing away from the face's middle). It starts on the
  surface found straight behind that pixel, runs outward and --back (share of the length) toward the back of the head,
  sagging --droop at its tip. Lengths are in the rig's units (the body 1 tall). Every strand vertex takes the UV the
  surface has at its root (so it is the muzzle's white) and weight 1 on the Head bone.
Exported like blender_head_weights.py.
"""
import json
import math
import sys

import bmesh
import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree
from mathutils.geometry import barycentric_transform

argv = sys.argv[sys.argv.index("--") + 1:]
src, cam_json, out, roots_text = argv[:4]
opts = dict(zip(argv[4::2], argv[5::2]))
length = float(opts.get("--length", 0.15))
r_root, r_tip = float(opts.get("--root", 0.003)), float(opts.get("--tip", 0.0006))
back, droop = float(opts.get("--back", 0.45)), float(opts.get("--droop", 0.012))
segs = int(opts.get("--segments", 8))
cam = json.load(open(cam_json))

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
arm.animation_data_clear()
for action in list(bpy.data.actions):
    bpy.data.actions.remove(action)
arm.data.pose_position = "REST"
mesh = next(o for o in bpy.data.objects if o.type == "MESH")
prefix = next(b.name for b in arm.data.bones if b.name.endswith("Hips")).replace("Hips", "")
mw = mesh.matrix_world
inv = mw.inverted()
me = mesh.data
uv_layer = me.uv_layers.active
bm = bmesh.new()
bm.from_mesh(me)
bm.transform(mw)
bm.faces.ensure_lookup_table()
bvh = BVHTree.FromBMesh(bm)
uvl = bm.loops.layers.uv.active
px = cam["res"]
ortho = cam["ortho"]
cx0, cz0 = cam["center"][0], cam["center"][2]
mid_x = cx0
y_front = min(v.co.y for v in bm.verts) - 1.0

# --tex <in png> --tex-out <out png> [--color r,g,b]: the strands' own colour, painted into an empty corner of the
# texture atlas (no UV island over it), and every strand vertex points there; without it the root's colour is used
# (the tiger's white muzzle: white strands vanished against the white face, so the old silver-grey is painted in)
patch_uv = None
if "--tex" in opts:
    import numpy as np
    img = bpy.data.images.load(opts["--tex"])
    W, H = img.size
    pix = np.array(img.pixels[:], dtype=np.float32).reshape(H, W, 4)
    cells = 64
    used = np.zeros((cells, cells), bool)
    for f in bm.faces:
        uvs = [l[uvl].uv for l in f.loops]
        u0, u1 = min(u.x for u in uvs), max(u.x for u in uvs)
        v0, v1 = min(u.y for u in uvs), max(u.y for u in uvs)
        used[int(max(0, v0) * cells) % cells: int(min(0.9999, v1) * cells) % cells + 1,
             int(max(0, u0) * cells) % cells: int(min(0.9999, u1) * cells) % cells + 1] = True
    free = np.argwhere(~used)
    if len(free):
        # the free cell farthest from any used one, so texture filtering never bleeds into it
        best = max(free.tolist(), key=lambda c: min(abs(c[0] - u[0]) + abs(c[1] - u[1]) for u in np.argwhere(used)[::7].tolist()))
        cy, cx = best
        col = [float(c) for c in opts.get("--color", "0.30,0.30,0.32").split(",")]
        y0, y1, x0, x1 = cy * H // cells, (cy + 1) * H // cells, cx * W // cells, (cx + 1) * W // cells
        pix[y0:y1, x0:x1, :3] = col
        img.pixels[:] = pix.ravel().tolist()
        img.filepath_raw = opts["--tex-out"]
        img.file_format = "PNG"
        img.save()
        patch_uv = ((cx + 0.5) / cells, (cy + 0.5) / cells)
        print(f"WHISKERS colour {col} in atlas cell {cx},{cy} -> {opts['--tex-out']}")

new_bm = bmesh.new()
uv_new = new_bm.loops.layers.uv.new("UVMap")
strands = 0
for item in [r for r in roots_text.split(";") if r.strip()]:
    x_px, y_px, deg = (float(c) for c in item.split(","))
    x = cx0 + (x_px / px - 0.5) * ortho
    z = cz0 + (0.5 - y_px / px) * ortho
    hit, normal, fi, _ = bvh.ray_cast(Vector((x, y_front, z)), Vector((0, 1, 0)))
    if hit is None:
        print(f"WHISKERS no surface behind {item}")
        continue
    f = bm.faces[fi]
    tri = [l for l in f.loops][:3]
    uvw = barycentric_transform(hit, *[l.vert.co for l in tri], *[Vector((l[uvl].uv.x, l[uvl].uv.y, 0)) for l in tri])
    uv = patch_uv or (uvw.x, uvw.y)
    side = 1.0 if x > mid_x else -1.0
    a = math.radians(deg)
    d = Vector((side * math.cos(a), back, math.sin(a))).normalized()
    start = hit - normal * 0.004
    # a frame round the strand for its three sides
    up = Vector((0, 0, 1))
    s1 = d.cross(up).normalized()
    s2 = d.cross(s1).normalized()
    rings = []
    for k in range(segs + 1):
        t = k / segs
        c = start + d * (length * t) - Vector((0, 0, droop * t * t))
        r = r_root + (r_tip - r_root) * t
        ring = [new_bm.verts.new(c + (s1 * math.cos(j * 2 * math.pi / 3) + s2 * math.sin(j * 2 * math.pi / 3)) * r) for j in range(3)]
        rings.append(ring)
    for k in range(segs):
        for j in range(3):
            a0, a1, b0, b1 = rings[k][j], rings[k][(j + 1) % 3], rings[k + 1][j], rings[k + 1][(j + 1) % 3]
            face = new_bm.faces.new((a0, a1, b1, b0))
            for l in face.loops:
                l[uv_new].uv = uv
    tip = new_bm.verts.new(start + d * (length * 1.04) - Vector((0, 0, droop * 1.08)))
    for j in range(3):
        face = new_bm.faces.new((rings[-1][j], rings[-1][(j + 1) % 3], tip))
        for l in face.loops:
            l[uv_new].uv = uv
    strands += 1

# into the body mesh: local space, same material, Head weight 1
new_bm.transform(inv)
base = len(me.vertices)
tmp = bpy.data.meshes.new("whiskers")
new_bm.to_mesh(tmp)
wob = bpy.data.objects.new("whiskers", tmp)
bpy.context.scene.collection.objects.link(wob)
for slot in mesh.material_slots:
    tmp.materials.append(slot.material)
tmp.uv_layers[0].name = uv_layer.name
grp = wob.vertex_groups.new(name=prefix + "Head")
grp.add(list(range(len(tmp.vertices))), 1.0, "REPLACE")
mod = wob.modifiers.new("Armature", "ARMATURE")
mod.object = arm
wob.parent = arm
wob.matrix_world = mw  # (after parenting: setting the parent alone would put the armature's turn and scale on top)
bpy.ops.object.select_all(action="DESELECT")
wob.select_set(True)
mesh.select_set(True)
bpy.context.view_layer.objects.active = mesh
bpy.ops.object.join()
print(f"WHISKERS {strands} strands, {len(me.vertices) - base} vertices added on {prefix}Head")
bpy.ops.export_scene.fbx(
    filepath=out, use_selection=False, object_types={"ARMATURE", "MESH"}, add_leaf_bones=False, bake_anim=False,
    primary_bone_axis="Y", secondary_bone_axis="X", use_armature_deform_only=False, mesh_smooth_type="OFF",
    path_mode="STRIP")
print(f"WHISKERS wrote {out}")
