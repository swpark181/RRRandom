"""Render the robot's cut parts each in its own flat color, from the front, the left side, behind and above at 3/4,
to check where the cuts fell (run inside Blender).

Usage: blender -b --factory-startup --python blender_parts_colors.py -- <out png> <part fbx> [<part fbx> ...]
Front on +X, pivot at the middle of the bottom (blender_robot_parts.py / blender_robot_joints.py output).
"""
import colorsys
import sys

import bpy
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
out, parts = args[0], args[1:]
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
engines = {e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items}
scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in engines else "BLENDER_EEVEE"
scene.render.resolution_x = scene.render.resolution_y = 600
scene.view_settings.view_transform = "Standard"
world = bpy.data.worlds.new("W")
world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.25, 0.25, 0.27, 1)
world.node_tree.nodes["Background"].inputs[1].default_value = 1.5
scene.world = world
sun = bpy.data.objects.new("S", bpy.data.lights.new("S", "SUN"))
sun.data.energy = 3
sun.rotation_euler = (0.9, 0.2, 0.6)
scene.collection.objects.link(sun)
top = 0.0
for i, path in enumerate(parts):
    bpy.ops.object.select_all(action="DESELECT")
    bpy.ops.import_scene.fbx(filepath=path)
    mat = bpy.data.materials.new(f"M{i}")
    mat.use_nodes = True
    r, g, b = colorsys.hsv_to_rgb(i / max(len(parts), 1), 0.75, 0.95)
    mat.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (r, g, b, 1)
    for o in bpy.context.selected_objects:
        if o.type == "MESH":
            o.data.materials.clear()
            o.data.materials.append(mat)
            top = max(top, max((o.matrix_world @ v.co).z for v in o.data.vertices))
    print("PART", i, path, (r, g, b))
cam = bpy.data.objects.new("C", bpy.data.cameras.new("C"))
scene.collection.objects.link(cam)
scene.camera = cam
cam.data.type = "ORTHO"
cam.data.ortho_scale = top * 1.15
mid = Vector((0, 0, top / 2))
import os
tiles = []
for name, d in (("front", Vector((1, 0, 0))), ("left", Vector((0, 1, 0))), ("back", Vector((-1, 0, 0))),
                ("high", Vector((1, 0.8, 0.9)).normalized())):
    cam.location = mid + d * 20
    cam.rotation_euler = (-d).to_track_quat("-Z", "Y").to_euler()
    f = os.path.join(os.path.dirname(out), f"_parts_{name}.png")
    scene.render.filepath = f
    bpy.ops.render.render(write_still=True)
    tiles.append(f)
import numpy as np
canvas = np.zeros((600, 600 * len(tiles), 4), dtype=np.float32)
for i, f in enumerate(tiles):
    im = bpy.data.images.load(f)
    a = np.empty(600 * 600 * 4, dtype=np.float32)
    im.pixels.foreach_get(a)
    canvas[:, i * 600:(i + 1) * 600] = a.reshape(600, 600, 4)
sheet = bpy.data.images.new("sheet", 600 * len(tiles), 600)
sheet.pixels.foreach_set(canvas.ravel())
sheet.filepath_raw = out
sheet.file_format = "PNG"
sheet.save()
print("PARTS sheet", out)
