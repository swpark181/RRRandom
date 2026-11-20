"""Cut a swappable face part (the muzzle: nose underside, mouth, chin, whiskers) out of a rigged animal friend (run inside
Blender), 2026-11-20. User: "나중에 다른 표정이나 말을 할 때는 해당 부분만 다른 파츠를 써야함".

The faces whose middle lies inside an ellipse of the front view (face frame of blender_face_audit.py: head middle = 0,
body height = 1) and in front of the head's middle, plus every small loose piece (the whiskers) around it, become their
own mesh object with their own material (slot "Muzzle"); the rest keeps slot "Body". The cut runs along existing edges,
so both sides of the rim keep the same vertices, weights and shading normals: put together they look like one mesh.

Writes three FBX files (armature + meshes, exported like blender_head_weights.py):
  <out>_parts.fbx   body + muzzle: the game mesh (one skeletal mesh, two material slots; the game can hide the Muzzle
                    section and show another muzzle part on the same skeleton in its place)
  <out>_muzzle.fbx  the muzzle alone: the default part, and the starting point for other expressions / mouth shapes
  <out>_body.fbx    the body alone (for the shell fur, so the fur stops at the part's rim)

Usage: blender -b --factory-startup --python blender_split_part.py -- <rig fbx> <out prefix>
           [--ellipse cx,cz,rx,rz] [--front 0.0] [--loose 100]
"""
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:]
src, prefix = argv[:2]
opts = dict(zip(argv[2::2], argv[3::2]))
ecx, ecz, erx, erz = (float(c) for c in opts.get("--ellipse", "0,-0.02,0.17,0.085").split(","))
front = float(opts.get("--front", 0.0))
loose_max = int(opts.get("--loose", 100))

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
arm.animation_data_clear()
for action in list(bpy.data.actions):
    bpy.data.actions.remove(action)
arm.data.pose_position = "REST"
body = max([o for o in bpy.data.objects if o.type == "MESH"], key=lambda o: len(o.data.polygons))
mw = body.matrix_world
verts = np.array([mw @ v.co for v in body.data.vertices])
lo, hi = verts[:, 2].min(), verts[:, 2].max()
h = hi - lo
head = verts[verts[:, 2] > lo + h * 0.55]
cx = (head[:, 0].min() + head[:, 0].max()) / 2
fz = lo + h * 0.66
face = verts[np.abs(verts[:, 2] - fz) < h * 0.12]
cy = (face[:, 1].min() + face[:, 1].max()) / 2
centre = np.array([cx, cy, fz])

bm = bmesh.new()
bm.from_mesh(body.data)
bm.verts.ensure_lookup_table()
bm.faces.ensure_lookup_table()
# loose pieces (whiskers)
comp_of = {}
seen = set()
comps = []
for v in bm.verts:
    if v in seen:
        continue
    comp, stack = [], [v]
    seen.add(v)
    while stack:
        a = stack.pop()
        comp.append(a)
        for e in a.link_edges:
            b = e.other_vert(a)
            if b not in seen:
                seen.add(b)
                stack.append(b)
    comps.append(comp)


def inside(co, grow=1.0):
    q = (np.array(mw @ co) - centre) / h
    return bool(((q[0] - ecx) / (erx * grow)) ** 2 + ((q[2] - ecz) / (erz * grow)) ** 2 < 1.0 and q[1] < front)


picked = 0
for f in bm.faces:
    f.select = inside(f.calc_center_median())
    picked += f.select
whiskers = 0
for comp in comps:
    if len(comp) < loose_max and any(inside(v.co, 1.6) for v in comp[:1]):
        for v in comp:
            for f in v.link_faces:
                f.select = True
        whiskers += 1
bm.to_mesh(body.data)
bm.free()
print(f"PART {picked} faces in the ellipse, {whiskers} loose pieces (whiskers) with them")

# materials: the body's own slot is "Body", the part gets a copy called "Muzzle"
mat_body = body.data.materials[0] if body.data.materials else bpy.data.materials.new("Body")
mat_body.name = "Body"
if not body.data.materials:
    body.data.materials.append(mat_body)
bpy.context.view_layer.objects.active = body
body.select_set(True)
bpy.ops.object.mode_set(mode="EDIT")
bpy.ops.mesh.separate(type="SELECTED")
bpy.ops.object.mode_set(mode="OBJECT")
part = next(o for o in bpy.data.objects if o.type == "MESH" and o is not body)
part.name = "Muzzle"
body.name = "Body"
mat_part = bpy.data.materials.new("Muzzle")
part.data.materials.clear()
part.data.materials.append(mat_part)
rim = 0
pbm = bmesh.new()
pbm.from_mesh(part.data)
rim = sum(1 for e in pbm.edges if e.is_boundary)
pbm.free()
print(f"PART muzzle {len(part.data.polygons)} faces, {len(part.data.vertices)} vertices, rim {rim} edges; "
      f"body {len(body.data.polygons)} faces; muzzle has custom normals: {part.data.has_custom_normals}")


def export(path, objs):
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    arm.select_set(True)
    bpy.ops.export_scene.fbx(
        filepath=path, use_selection=True, object_types={"ARMATURE", "MESH"}, add_leaf_bones=False, bake_anim=False,
        primary_bone_axis="Y", secondary_bone_axis="X", use_armature_deform_only=False, mesh_smooth_type="OFF",
        path_mode="STRIP")
    print(f"PART wrote {path}")


export(prefix + "_parts.fbx", [body, part])
export(prefix + "_muzzle.fbx", [part])
export(prefix + "_body.fbx", [body])
