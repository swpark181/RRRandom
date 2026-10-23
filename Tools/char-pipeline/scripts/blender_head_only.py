"""Put everything above a height on the Head bone alone (run in Blender).

Round 77, the film-style tiger rigged with blender_fit_rig.py: its long whiskers and cheek ruff stick out past the
shoulders, and the donor's weights left bits of them on the arms and spine, so turning the head tore them into shards.
Above <z>, a vertex's weights become Head 1.0 (blender_head_weights.py then smooths the neck band below it).

Usage: blender -b --factory-startup --python blender_head_only.py -- <rig fbx> <out fbx> <z>
  <z> in the rig's Blender frame (m, Z up). Written like blender_animal_tail.py writes.
"""
import sys

import bpy

args = sys.argv[sys.argv.index("--") + 1:]
src, out, zcut = args[0], args[1], float(args[2])
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
mesh = next(o for o in bpy.data.objects if o.type == "MESH")
head = next(g for g in mesh.vertex_groups if g.name.endswith(":Head") or g.name == "Head")
mw = mesh.matrix_world
n = 0
for v in mesh.data.vertices:
    if (mw @ v.co).z < zcut:
        continue
    for g in list(v.groups):
        if g.group != head.index:
            mesh.vertex_groups[g.group].remove([v.index])
    head.add([v.index], 1.0, "REPLACE")
    n += 1
print(f"HEADONLY {n} vertices above z {zcut} on Head only")
bpy.ops.export_scene.fbx(
    filepath=out, use_selection=False, object_types={"ARMATURE", "MESH"}, add_leaf_bones=False, bake_anim=False,
    primary_bone_axis="Y", secondary_bone_axis="X", use_armature_deform_only=False, mesh_smooth_type="OFF",
    path_mode="STRIP")
print(f"HEADONLY wrote {out}")
