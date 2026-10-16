"""Cut the parts the Mixamo auto-rigger gets wrong out of a character's Mixamo OBJ, for a cleaner body rig (run in Blender).

Usage: blender -b --factory-startup --python blender_strip_parts.py -- <picked fbx> <obj> <out obj> <names>
  <picked fbx>: a rigged FBX of the same mesh where blender_animal_tail.py gave those parts their own bones;
  <obj>: the OBJ that went to Mixamo (blender_tripo_to_obj.py); <names>: comma separated bone name parts, e.g.
  "Tail,WingL,WingR". Every OBJ vertex whose matching vertex in the FBX is mostly (>= half) on those bones goes.

The dragon's tail lies on the ground by its feet and its wings sit behind its arms: the auto-rigger put a foot bone in
the tail and skinned the wings to the arms. Rigged without them, the body comes out right; blender_merge_parts.py then
gives the whole mesh the body's weights. The OBJ keeps blender_tripo_to_obj.py's frame (100 tall, Y up, facing +Z)
and its material lines; the FBX frame (meters, Z up, offset by Mixamo) is matched by the bounds.
"""
import sys

import bpy
from mathutils import Vector
from mathutils.kdtree import KDTree

picked_fbx, obj, out, names = sys.argv[sys.argv.index("--") + 1:][:4]
parts = names.split(",")
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=picked_fbx)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
arm.animation_data_clear()
arm.data.pose_position = "REST"
rig = next(o for o in bpy.data.objects if o.type == "MESH")
groups = {g.index: g.name for g in rig.vertex_groups}
rig_pts = [rig.matrix_world @ v.co for v in rig.data.vertices]
cut = [sum(g.weight for g in v.groups if any(p in groups[g.group] for p in parts)) >= 0.5 for v in rig.data.vertices]

bpy.ops.wm.obj_import(filepath=obj, forward_axis="NEGATIVE_Z", up_axis="Y")
mesh = next(o for o in bpy.data.objects if o.type == "MESH" and o != rig)
pts = [mesh.matrix_world @ v.co for v in mesh.data.vertices]


def bounds(ps):
    lo = Vector((min(p.x for p in ps), min(p.y for p in ps), min(p.z for p in ps)))
    hi = Vector((max(p.x for p in ps), max(p.y for p in ps), max(p.z for p in ps)))
    return lo, hi


lo_r, hi_r = bounds(rig_pts)
lo_o, hi_o = bounds(pts)
scale = (hi_r.z - lo_r.z) / (hi_o.z - lo_o.z)
tree = KDTree(len(rig_pts))
for i, p in enumerate(rig_pts):
    tree.insert(p, i)
tree.balance()
worst = 0.0
gone = []
for i, p in enumerate(pts):
    q = lo_r + (p - lo_o) * scale
    co, j, dist = tree.find(q)
    worst = max(worst, dist)
    if cut[j]:
        gone.append(i)
print(f"STRIP matched {len(pts)} vertices (worst {worst * 100:.2f} cm apart), cutting {len(gone)}")

for v in mesh.data.vertices:
    v.select = v.index in set(gone)
bpy.context.view_layer.objects.active = mesh
for o in bpy.data.objects:
    o.select_set(o == mesh)
bpy.ops.object.mode_set(mode="EDIT")
bpy.ops.mesh.select_mode(type="VERT")
bpy.ops.object.mode_set(mode="OBJECT")
for v in mesh.data.vertices:
    v.select = v.index in set(gone)
bpy.ops.object.mode_set(mode="EDIT")
bpy.ops.mesh.delete(type="VERT")
bpy.ops.object.mode_set(mode="OBJECT")
bpy.ops.wm.obj_export(filepath=out, export_selected_objects=True, export_materials=False, export_uv=True,
                      export_normals=True, forward_axis="NEGATIVE_Z", up_axis="Y")
with open(obj, encoding="utf-8") as f:
    head = [line for line in f if line.startswith(("mtllib", "usemtl"))]
with open(out, encoding="utf-8") as f:
    body = [line for line in f if not line.startswith(("mtllib", "usemtl"))]
with open(out, "w", encoding="utf-8") as f:
    f.writelines(head)
    f.writelines(body)
print(f"STRIP wrote {out}: {len(mesh.data.vertices)} vertices left")
