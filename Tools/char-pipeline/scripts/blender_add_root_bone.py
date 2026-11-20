"""Add a root bone above the hips of a Mixamo rig (run inside Blender), so Unreal can carry root motion on it.

Usage: blender -b --factory-startup --python blender_add_root_bone.py -- <rig fbx in> <rig fbx out>
The new bone "mixamorig:Root" sits on the floor straight under the hips (the hips need not be over the armature's
origin), lying along the mesh's front so that Unreal sees it unrotated, and becomes the parent of "mixamorig:Hips". Nothing else changes: same mesh, weights and bone names (Unreal drops the "mixamorig:" prefix).
Exported with the settings blender_fox_tail.py uses.
"""
import sys
import bpy

src, out = sys.argv[-2], sys.argv[-1]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
bpy.context.view_layer.objects.active = arm
bpy.ops.object.mode_set(mode="EDIT")
eb = arm.data.edit_bones
hips = eb["mixamorig:Hips"]
length = max(hips.length, 1e-3)
root = eb.new("mixamorig:Root")
# armature space is Y up here (the armature object carries the FBX axis turn and the 0.01 scale)
# In Unreal the root must come out unrotated (X right-left, Y front, Z up): Pose Search reads the trajectory in the root
# bone's own space and keeps only its X and Y, so a root standing up like the other Mixamo bones (rolled 90 degrees) hid
# all forward and backward movement from the search (found 2026-11-20: the fox ran backwards on the spot standing still).
# Armature space is Y up here; lying along its -Z with its own Z up, the bone comes into Unreal with no rotation at all
# (along +Z it came in turned 180 degrees about the up axis: checked with mm_gasp_import_fox.py).
root.head = (hips.head.x, 0.0, hips.head.z)
root.tail = (hips.head.x, 0.0, hips.head.z - length)
from mathutils import Vector as _V
root.align_roll(_V((0.0, 1.0, 0.0)))
hips.use_connect = False
hips.parent = root
print("ROOT hips head", tuple(round(v, 3) for v in hips.head), "root tail", tuple(round(v, 3) for v in root.tail),
      "armature rot", tuple(round(v, 3) for v in arm.rotation_euler), "scale", tuple(round(v, 4) for v in arm.scale))
bpy.ops.object.mode_set(mode="OBJECT")
from mathutils import Vector
for m in (o for o in bpy.data.objects if o.type == "MESH"):
    pts = [m.matrix_world @ Vector(c) for c in m.bound_box]
    lo = [round(min(p[i] for p in pts), 3) for i in range(3)]
    hi = [round(max(p[i] for p in pts), 3) for i in range(3)]
    print("ROOT mesh", m.name, "world bounds", lo, hi)
rb = arm.data.bones["mixamorig:Root"]
print("ROOT root head world", tuple(round(v, 3) for v in arm.matrix_world @ rb.head_local),
      "hips head world", tuple(round(v, 3) for v in arm.matrix_world @ arm.data.bones["mixamorig:Hips"].head_local))
tops =[b.name for b in arm.data.bones if b.parent is None]
print("ROOT top-level bones now:", tops, "total", len(arm.data.bones))
bpy.ops.export_scene.fbx(
    filepath=out, use_selection=False, object_types={"ARMATURE", "MESH"}, add_leaf_bones=False, bake_anim=False,
    primary_bone_axis="Y", secondary_bone_axis="X", use_armature_deform_only=False, mesh_smooth_type="OFF",
    path_mode="STRIP")
print(f"ROOT wrote {out}")
