"""Tidy the Tripo foam finger into a weapon mesh (run inside Blender).

Usage: blender -b --factory-startup --python blender_foam_finger.py -- <tripo fbx> <out fbx> <preview png>

The model stands upright (cuff at the bottom, the finger pointing up, the palm with its star facing -Y). Laid down so
the finger points along +X with the palm up (+Z, so the star shows to the high game camera), the middle of the
cuff's bottom at the origin, and scaled to 100 long like the rifle (URRRandomGunComponent scales it to
FoamFingerLength). Decimated to about TARGET_TRIS triangles; mesh and UVs only (the base color goes in on its own).
Renders a check from above (+X to the right).
"""
import math
import sys

import bpy
from mathutils import Matrix, Vector

src, out_fbx, preview = sys.argv[sys.argv.index("--") + 1:][:3]
TARGET_TRIS = 6000

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
meshes = [o for o in bpy.data.objects if o.type == "MESH"]
for o in list(bpy.data.objects):
    if o.type != "MESH":
        bpy.data.objects.remove(o, do_unlink=True)
bpy.ops.object.select_all(action="DESELECT")
for o in meshes:
    o.select_set(True)
bpy.context.view_layer.objects.active = meshes[0]
if len(meshes) > 1:
    bpy.ops.object.join()
obj = bpy.context.view_layer.objects.active
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)


def bounds():
    pts = [obj.matrix_world @ v.co for v in obj.data.vertices]
    return (Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts))),
            Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts))))


# Upright (+Z up) to lying along +X: turn -90 about Y (Z -> X); the palm (-Y) then turns up with +90 about X
obj.data.transform(Matrix.Rotation(math.radians(-90), 4, "Y"))
obj.data.transform(Matrix.Rotation(math.radians(90), 4, "X"))
# That leaves the finger along -X (checked on the render): turn it round to +X, palm still up
obj.data.transform(Matrix.Rotation(math.radians(180), 4, "Z"))
lo, hi = bounds()
# Cuff end at the origin, centered across and in height
offset = Vector((lo.x, (lo.y + hi.y) / 2, (lo.z + hi.z) / 2))
for v in obj.data.vertices:
    v.co -= offset
lo, hi = bounds()
scale = 1.0 / max(hi.x - lo.x, 1e-6)  # 100 Unreal units
for v in obj.data.vertices:
    v.co *= scale
obj.data.update()

tris = sum(len(p.vertices) - 2 for p in obj.data.polygons)
if tris > TARGET_TRIS:
    mod = obj.modifiers.new("Decimate", "DECIMATE")
    mod.ratio = TARGET_TRIS / tris
    bpy.ops.object.modifier_apply(modifier=mod.name)
lo, hi = bounds()
size = (hi - lo) * 100.0
obj.name = "SM_FoamFinger"
bpy.ops.export_scene.fbx(filepath=out_fbx, use_selection=True, object_types={"MESH"}, mesh_smooth_type="FACE",
                         path_mode="STRIP", bake_anim=False)
print(f"FINGER size {size.x:.0f} x {size.y:.0f} x {size.z:.0f}, {tris} tris")
# The fingertip (URRRandomGunComponent::FoamFingerTip): the middle of the vertices in the last 3% along X
tip = [obj.matrix_world @ v.co for v in obj.data.vertices if (obj.matrix_world @ v.co).x > hi.x - 0.03]
tip_mid = sum(tip, Vector()) / max(len(tip), 1) * 100.0
print(f"FINGER tip {tip_mid.x:.0f} {tip_mid.y:.0f} {tip_mid.z:.0f}")

scene = bpy.context.scene
cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
scene.collection.objects.link(cam)
cam.data.type = "ORTHO"
cam.data.ortho_scale = 1.3
cam.location = (0.5, 0.0, 5.0)
scene.camera = cam
sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
scene.collection.objects.link(sun)
scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in {e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items} else "BLENDER_EEVEE"
scene.render.resolution_x, scene.render.resolution_y = 800, 600
scene.render.filepath = preview
bpy.ops.render.render(write_still=True)
print(f"FINGER preview {preview}")
