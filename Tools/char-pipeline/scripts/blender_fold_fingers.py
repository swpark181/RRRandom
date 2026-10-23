"""Fold the finger bones' skin weights into the hand bones (run in Blender).

Round 77, the film-style dragon: its arm joints were moved with blender_fit_rig.py (Mixamo put the arm joint below the
elbow), and the finger bones, given no joints, followed the spline into the wrong places, so finger animation tore the
small claws apart. The paws then ride on <Side>Hand alone: every HandThumb*/HandIndex*/... weight is added to the hand.

Usage: blender -b --factory-startup --python blender_fold_fingers.py -- <rig fbx> <out fbx>
  Written like blender_animal_tail.py writes.
"""
import sys

import bpy

args = sys.argv[sys.argv.index("--") + 1:]
src, out = args[0], args[1]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
mesh = next(o for o in bpy.data.objects if o.type == "MESH")
groups = {g.index: g.name for g in mesh.vertex_groups}
hands = {side: mesh.vertex_groups.get(f"mixamorig:{side}Hand") for side in ("Left", "Right")}
moved = 0
for v in mesh.data.vertices:
    for g in list(v.groups):
        name = groups[g.group]
        for side, hand in hands.items():
            if hand and name.startswith(f"mixamorig:{side}Hand") and name != hand.name:
                hand.add([v.index], g.weight, "ADD")
                mesh.vertex_groups[g.group].remove([v.index])
                moved += 1
print(f"FOLD {moved} finger weights moved onto the hands")
bpy.ops.export_scene.fbx(
    filepath=out, use_selection=False, object_types={"ARMATURE", "MESH"}, add_leaf_bones=False, bake_anim=False,
    primary_bone_axis="Y", secondary_bone_axis="X", use_armature_deform_only=False, mesh_smooth_type="OFF",
    path_mode="STRIP")
print(f"FOLD wrote {out}")
