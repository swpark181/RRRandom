"""Render a rigged character's rest pose from the side and from behind over a 10 cm grid, to read off where its tail runs
(run inside Blender). Coordinates as Unreal's mesh space in cm: X = the character's left, -Y = backward, Z = up.

Usage: blender -b --factory-startup --python blender_rest_grid.py -- <fbx> <out prefix>
Writes <out prefix>_side.png (looking at its left side: backward = right of the picture) and <out prefix>_back.png
(from behind: its left = left of the picture). Grid lines every 10 cm, thicker every 50; the red ones are 0.
"""
import math
import sys

import bpy
from mathutils import Vector

src, out = sys.argv[sys.argv.index("--") + 1:][:2]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
arm.animation_data_clear()
for action in list(bpy.data.actions):
    bpy.data.actions.remove(action)
arm.data.pose_position = "REST"
scene = bpy.context.scene


def material(name, rgb):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    bsdf = m.node_tree.nodes["Principled BSDF"]
    bsdf.inputs["Base Color"].default_value = (*rgb, 1)
    bsdf.inputs["Emission Color"].default_value = (*rgb, 1)
    bsdf.inputs["Emission Strength"].default_value = 1.0
    return m


grey, dark, red = material("g", (0.2, 0.6, 1.0)), material("d", (0.0, 0.1, 0.6)), material("r", (1, 0, 0))


def bar(loc, size, mat):
    bpy.ops.mesh.primitive_cube_add(location=loc)
    o = bpy.context.active_object
    o.scale = Vector(size) / 2
    o.data.materials.append(mat)
    return o


def lines(view):
    made = []
    # view "side": a plane of lines at x = -0.6 (behind the model from a camera at -X... the camera is at +X)
    for i in range(-6, 11):
        v = i * 0.1
        mat = red if i == 0 else (dark if i % 5 == 0 else grey)
        w = 0.004 if i % 5 else 0.007
        if view == "side":
            made.append(bar((-0.7, 0, v), (0.001, 2.0, w), mat))      # horizontal: z = v
            made.append(bar((-0.7, v, 0.4), (0.001, w, 2.0), mat))    # vertical: blender y = v (UE y = -v)
        else:
            made.append(bar((0, -0.7, v), (2.0, 0.001, w), mat))
            made.append(bar((v, -0.7, 0.4), (w, 0.001, 2.0), mat))
    return made


engines = {e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items}
scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in engines else "BLENDER_EEVEE"
scene.render.resolution_x = scene.render.resolution_y = 900
scene.view_settings.view_transform = "Standard"
world = bpy.data.worlds.new("W")
scene.world = world
world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (1, 1, 1, 1)
sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
sun.data.energy = 3
sun.rotation_euler = (math.radians(50), 0, math.radians(40))
scene.collection.objects.link(sun)
cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
scene.collection.objects.link(cam)
cam.data.type = "ORTHO"
cam.data.ortho_scale = 1.3
scene.camera = cam
for view in ("side", "back"):
    made = lines(view)
    if view == "side":
        # Camera at +X looking toward -X: Blender +Y (the character's back) is on the right
        cam.location = (3, 0.15, 0.45)
        cam.rotation_euler = (math.radians(90), 0, math.radians(90))
    else:
        # Camera at +Y (behind) looking toward -Y: Blender +X is on the left... the character's left (UE +X) is Blender -X
        cam.location = (0, 3, 0.45)
        cam.rotation_euler = (math.radians(90), 0, math.radians(180))
    scene.render.filepath = f"{out}_{view}.png"
    bpy.ops.render.render(write_still=True)
    for o in made:
        bpy.data.objects.remove(o)
print(f"GRID wrote {out}_side.png, {out}_back.png")
