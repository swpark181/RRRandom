"""Render a character's whole body from the front, as a style reference picture (run inside Blender).

Usage: blender -b --factory-startup --python blender_fullbody.py -- <fbx or obj> <out png> [size] [yaw degrees]

The mesh as it is in the file (the unposed Tripo model, or a Mixamo FBX's first frame), facing the camera (-Y in
Blender), on a plain white background, soft lighting so the base color carries it.
"""
import math
import sys

import bpy
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
src, out = args[0], args[1]
size = int(args[2]) if len(args) > 2 else 1024
yaw = math.radians(float(args[3])) if len(args) > 3 else 0.0

bpy.ops.wm.read_factory_settings(use_empty=True)
if src.lower().endswith(".obj"):
    bpy.ops.wm.obj_import(filepath=src)
else:
    bpy.ops.import_scene.fbx(filepath=src)
scene = bpy.context.scene
scene.frame_set(0)
depsgraph = bpy.context.evaluated_depsgraph_get()
pts = []
for m in (o for o in scene.objects if o.type == "MESH"):
    ev = m.evaluated_get(depsgraph)
    pts += [ev.matrix_world @ v.co for v in ev.to_mesh().vertices]
lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
center = (lo + hi) / 2
height = max(hi.z - lo.z, hi.x - lo.x)

cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
scene.collection.objects.link(cam)
cam.data.type = "ORTHO"
cam.data.ortho_scale = height * 1.12
dist = height * 4
cam.location = Vector((center.x + math.sin(yaw) * dist, center.y - math.cos(yaw) * dist, center.z))
cam.rotation_euler = (math.radians(90), 0.0, yaw)
cam.data.clip_end = dist * 4
scene.camera = cam
for rot, energy in (((math.radians(50), 0.0, math.radians(30)), 3.0), ((math.radians(70), 0.0, math.radians(-120)), 1.2)):
    sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
    sun.data.energy = energy
    sun.rotation_euler = rot
    scene.collection.objects.link(sun)
world = bpy.data.worlds.new("World")
scene.world = world
world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (1, 1, 1, 1.0)
world.node_tree.nodes["Background"].inputs[1].default_value = 0.7
engines = {e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items}
scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in engines else "BLENDER_EEVEE"
scene.render.film_transparent = False
scene.render.resolution_x = scene.render.resolution_y = size
scene.render.image_settings.file_format = "PNG"
scene.render.image_settings.color_mode = "RGB"
scene.view_settings.view_transform = "Standard"
scene.render.filepath = out
bpy.ops.render.render(write_still=True)
print(f"FULLBODY {out}: {size}px, bounds {lo} .. {hi}")
