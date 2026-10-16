"""The squirrel's acorns (ARRRandomAcornRain), two meshes so each part takes its own color (run inside Blender).

Usage: blender -b --factory-startup --python blender_acorn.py -- <out dir>

SM_AcornNut: a plump nut, wider than tall at the top and coming to a soft point below, 60 units tall.
SM_AcornCap: the cupule on top, a slightly bumpy dome with a rim, and a little stem. Both share the nut's origin (its
middle), Z up, smooth shading, so the game places one transform for both.
"""
import math
import os
import sys

import bmesh
import bpy
from mathutils import Vector

out = sys.argv[sys.argv.index("--") + 1]
os.makedirs(out, exist_ok=True)


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def export(name):
    for o in bpy.context.scene.objects:
        o.select_set(True)
    bpy.ops.export_scene.fbx(filepath=os.path.join(out, name + ".fbx"), use_selection=True, mesh_smooth_type="FACE",
                             apply_scale_options="FBX_SCALE_NONE", global_scale=1.0)


def shade_smooth(obj):
    for p in obj.data.polygons:
        p.use_smooth = True


# Nut: a UV sphere, its lower half pulled into a soft point
reset()
bpy.ops.mesh.primitive_uv_sphere_add(segments=32, ring_count=20, radius=0.25)
nut = bpy.context.active_object
for v in nut.data.vertices:
    z = v.co.z
    if z < 0:
        t = -z / 0.25
        v.co.z = z * 1.45
        v.co.x *= 1.0 - 0.35 * t * t
        v.co.y *= 1.0 - 0.35 * t * t
    else:
        v.co.z = z * 0.75
nut.location = (0, 0, 0)
shade_smooth(nut)
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
nut.name = "SM_AcornNut"
export("SM_AcornNut")

# Cap: a dome over the top with a rolled rim and little bumps, and a stem
reset()
bpy.ops.mesh.primitive_uv_sphere_add(segments=32, ring_count=16, radius=0.29)
cap = bpy.context.active_object
bm = bmesh.new()
bm.from_mesh(cap.data)
for v in list(bm.verts):
    if v.co.z < -0.02:
        bm.verts.remove(v)
for v in bm.verts:
    angle = math.atan2(v.co.y, v.co.x)
    bump = 1.0 + 0.04 * math.sin(angle * 9) * math.sin(v.co.z * 60)
    v.co.x *= bump
    v.co.y *= bump
    v.co.z = v.co.z * 0.6 + 0.12
bm.to_mesh(cap.data)
bm.free()
solid = cap.modifiers.new("Solid", "SOLIDIFY")
solid.thickness = 0.03
bpy.ops.object.modifier_apply(modifier="Solid")
bpy.ops.mesh.primitive_cylinder_add(vertices=12, radius=0.035, depth=0.14, location=(0.0, 0.0, 0.34), rotation=(0.25, 0, 0))
stem = bpy.context.active_object
for o in (cap, stem):
    o.select_set(True)
bpy.context.view_layer.objects.active = cap
bpy.ops.object.join()
shade_smooth(cap)
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
cap.name = "SM_AcornCap"
export("SM_AcornCap")
print("ACORN wrote", out)
