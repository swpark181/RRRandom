"""Prints the height of a model's top surface along its middle line (run inside Blender), to find seats and cockpits.

Usage: blender -b --factory-startup --python blender_top_profile.py -- <fbx> [step] [y]
For X from the back to the front of the model, every <step> Unreal units (default 10), casts a ray straight down at
Y = <y> (default 0) and prints the first hit's height, in Unreal units above the model's bottom: an open cockpit shows
as a dip between the higher rims around it. Units follow blender_vehicles.py (its FBX: 1 Blender unit = 1 m = 100 UU).
"""
import sys

import bpy
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
fbx = args[0]
step = float(args[1]) if len(args) > 1 else 10.0
y = float(args[2]) if len(args) > 2 else 0.0

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=fbx)
obj = next(o for o in bpy.data.objects if o.type == "MESH")
depsgraph = bpy.context.evaluated_depsgraph_get()
pts = [obj.matrix_world @ v.co for v in obj.data.vertices]
lo_x, hi_x = min(p.x for p in pts), max(p.x for p in pts)
lo_z, hi_z = min(p.z for p in pts), max(p.z for p in pts)
print(f"PROFILE {fbx}: x {lo_x * 100:.0f}..{hi_x * 100:.0f}, height {(hi_z - lo_z) * 100:.0f}")
x = lo_x
while x <= hi_x:
    hit, loc, *_ = bpy.context.scene.ray_cast(depsgraph, Vector((x, y / 100.0, hi_z + 1.0)), Vector((0, 0, -1)))
    print(f"PROFILE x {x * 100:6.0f}  top {((loc.z - lo_z) * 100) if hit else float('nan'):6.0f}")
    x += step / 100.0
