"""Render a game-ready mesh (front on +X, pivot at the middle of its bottom, Unreal units = Blender metres x 100) from
the front and from its left side, orthographic at one pixel per Unreal unit, to read joint positions off a grid (run
inside Blender).

Usage: blender -b --factory-startup --python blender_measure_views.py -- <fbx> <base color> <out prefix> [<size, 720>]
Writes <prefix>_front.png (looking at its face: the robot's left, Unreal -Y, on the image right) and
<prefix>_left.png (looking at its left side: the front, +X, on the image left). In both, pixel row r is height
size - 40 - r above the feet and the middle column is 0 sideways / 0 forward. Draw the grid afterwards.
"""
import sys

import bpy
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
fbx, tex_path, prefix = args[0], args[1], args[2]
size = int(args[3]) if len(args) > 3 else 720

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
engines = {e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items}
scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in engines else "BLENDER_EEVEE"
scene.render.resolution_x = scene.render.resolution_y = size
scene.view_settings.view_transform = "Standard"
world = bpy.data.worlds.new("W")
world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.3, 0.3, 0.32, 1)
world.node_tree.nodes["Background"].inputs[1].default_value = 1.8
scene.world = world

bpy.ops.import_scene.fbx(filepath=fbx)
mat = bpy.data.materials.new("M")
mat.use_nodes = True
tex = mat.node_tree.nodes.new("ShaderNodeTexImage")
tex.image = bpy.data.images.load(tex_path)
bsdf = mat.node_tree.nodes["Principled BSDF"]
mat.node_tree.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
bsdf.inputs["Roughness"].default_value = 1.0
for o in bpy.context.selected_objects:
    if o.type == "MESH":
        o.data.materials.clear()
        o.data.materials.append(mat)

cam = bpy.data.objects.new("C", bpy.data.cameras.new("C"))
scene.collection.objects.link(cam)
scene.camera = cam
cam.data.type = "ORTHO"
cam.data.ortho_scale = size / 100.0
mid_z = (size / 2.0 - 40.0) / 100.0  # the feet 40 px above the bottom edge
for name, loc, rot in (("front", Vector((20, 0, mid_z)), (1.5708, 0, 1.5708)),
                       ("left", Vector((0, 20, mid_z)), (1.5708, 0, 3.14159))):
    cam.location = loc
    cam.rotation_euler = rot
    scene.render.filepath = f"{prefix}_{name}.png"
    bpy.ops.render.render(write_still=True)
    print("MEASURE", scene.render.filepath)
