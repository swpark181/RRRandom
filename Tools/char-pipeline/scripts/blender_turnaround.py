"""Render a model (Tripo FBX with its textures) from the front, back, left side and top, plus a 3/4 view (run in Blender).

Usage: blender -b --factory-startup --python blender_turnaround.py -- <fbx> <out prefix> [size]
Writes <out prefix>_front|back|side|top|34.png. The model's front is taken to face -Y (Tripo's Blender-preset FBX);
the side view looks at its left side, so it faces the left of the picture like the concept art.
"""
import math
import sys

import bpy
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
src, out = args[0], args[1]
size = int(args[2]) if len(args) > 2 else 800
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
meshes = [o for o in bpy.data.objects if o.type == "MESH"]
pts = [o.matrix_world @ v.co for o in meshes for v in o.data.vertices]
lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
mid = (lo + hi) / 2
span = max(hi - lo)
print(f"TURN bounds {hi.x - lo.x:.3f} x {hi.y - lo.y:.3f} x {hi.z - lo.z:.3f}")

scene = bpy.context.scene
engines = {e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items}
scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in engines else "BLENDER_EEVEE"
scene.render.resolution_x = scene.render.resolution_y = size
scene.view_settings.view_transform = "Standard"
world = bpy.data.worlds.new("W")
scene.world = world
world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.75, 0.75, 0.78, 1)
world.node_tree.nodes["Background"].inputs[1].default_value = 0.8
sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
sun.data.energy = 2.5
sun.rotation_euler = (math.radians(45), 0, math.radians(30))
scene.collection.objects.link(sun)
cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
scene.collection.objects.link(cam)
cam.data.type = "ORTHO"
cam.data.ortho_scale = span * 1.1
scene.camera = cam
d = span * 3
views = {
    "front": (0, -d, 0),
    "back": (0, d, 0),
    "side": (d, 0, 0),
    "top": (0, 0.001, d),
    "34": (d * 0.7, -d * 0.7, d * 0.35),
}
for name, offset in views.items():
    cam.location = mid + Vector(offset)
    # Aimed at the middle of the model, its up kept up (the top view's up is the model's back)
    cam.rotation_euler = (-Vector(offset)).to_track_quat("-Z", "Y").to_euler()
    scene.render.filepath = f"{out}_{name}.png"
    bpy.ops.render.render(write_still=True)
print("TURN done")
