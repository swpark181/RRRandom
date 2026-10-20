"""Render a tidied mesh with its Tripo base color texture from the front (+X), the right side and above (run inside Blender).

Usage: blender -b --factory-startup --python blender_textured_views.py -- <out png> <fbx>=<base color image> ...
Each mesh gets a row: looking down -X at its front, from its right side (front to the right), and from above (front
up). Checks which way a vehicle faces after blender_vehicles.py, when the untextured previews can't tell.
"""
import math
import sys

import bpy
from mathutils import Matrix

args = sys.argv[sys.argv.index("--") + 1:]
out = args[0]
items = [a.split("=") for a in args[1:]]

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in {e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items} else "BLENDER_EEVEE"
world = bpy.data.worlds.new("World")
world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.8, 0.8, 0.82, 1.0)
world.node_tree.nodes["Background"].inputs[1].default_value = 1.0
scene.world = world

objects = []
for fbx, png in items:
    bpy.ops.import_scene.fbx(filepath=fbx)
    obj = bpy.context.selected_objects[0]
    mat = bpy.data.materials.new(obj.name + "_M")
    mat.use_nodes = True
    tex = mat.node_tree.nodes.new("ShaderNodeTexImage")
    tex.image = bpy.data.images.load(png)
    bsdf = mat.node_tree.nodes["Principled BSDF"]
    mat.node_tree.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.8
    obj.data.materials.clear()
    obj.data.materials.append(mat)
    objects.append(obj)

size = max(max(o.dimensions) for o in objects)
gap = size * 1.3
views = ["front", "side", "top"]
scene.render.resolution_x, scene.render.resolution_y = 1500, 500 * len(objects)

# Copies of each mesh in a grid, each turned so one camera looking down +Y sees the wanted side
for o in objects:
    o.hide_render = True
for row, obj in enumerate(objects):
    for col, name in enumerate(views):
        c = obj.copy()
        c.data = obj.data
        scene.collection.objects.link(c)
        c.hide_render = False
        # Turn each copy so one camera looking down -Y sees the wanted side: front (+X toward camera), right side, top
        if name == "front":
            c.rotation_euler = (0.0, 0.0, math.radians(-90))
        elif name == "side":
            c.rotation_euler = (0.0, 0.0, 0.0)
        else:
            # Top toward the camera (-Y), front up (+Z)
            c.rotation_mode = "QUATERNION"
            c.rotation_quaternion = Matrix(((0, -1, 0), (0, 0, -1), (1, 0, 0))).to_quaternion()
        c.location = (col * gap, 0.0, -row * gap)
bpy.context.view_layer.update()
cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
scene.collection.objects.link(cam)
cam.data.type = "ORTHO"
cam.data.ortho_scale = gap * max(3, len(objects))
cam.location = (gap, -size * 5, (size - (len(objects) - 1) * gap) / 2)
cam.rotation_euler = (math.radians(90), 0.0, 0.0)
scene.camera = cam
sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
sun.data.energy = 3.0
scene.collection.objects.link(sun)
sun.rotation_euler = (math.radians(60), 0.0, math.radians(-20))
scene.render.filepath = out
bpy.ops.render.render(write_still=True)
print(f"VIEWS {out}")
