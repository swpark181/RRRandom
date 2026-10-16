"""Render the game's die spinning, as a sprite sheet for the title screen (run inside Blender).

Usage: blender -b --factory-startup --python blender_dice_spin_sheet.py -- <SM_Dice.fbx> <base color png> <out png> [frame px]
  e.g. ... -- C:/work/char-pipeline/output/dice/SM_Dice.fbx C:/.../colorful_cube_3d_model_basecolor.PNG
            C:/work/char-pipeline/output/ui/T_TitleDiceSpin.png 160

One full turn about a tilted axis in FRAMES frames (a cube looks the same after a full turn, so the loop is seamless),
laid out COLUMNS across, left to right then top to bottom, on a transparent background. The body wears the Tripo
rainbow texture (slot "Dice", a little self-lit so it glows like the die in the title art), the pips (slot "Pip")
are white.
"""
import math
import sys

import bpy
import numpy as np
from mathutils import Matrix, Vector

args = sys.argv[sys.argv.index("--") + 1:]
src, texture, out = args[0], args[1], args[2]
size = int(args[3]) if len(args) > 3 else 160
FRAMES = 24
COLUMNS = 6
AXIS = Vector((0.35, 0.55, 1.0)).normalized()
GLOW = 0.35

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
die = next(o for o in bpy.data.objects if o.type == "MESH")
die.location = (0.0, 0.0, 0.0)
die.rotation_mode = "QUATERNION"
base_rotation = die.rotation_quaternion.copy()
image = bpy.data.images.load(texture)

for slot in die.material_slots:
    mat = bpy.data.materials.new(slot.name)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    bsdf = nodes["Principled BSDF"]
    bsdf.inputs["Roughness"].default_value = 0.45
    if slot.name.startswith("Pip"):
        bsdf.inputs["Base Color"].default_value = (1.0, 1.0, 1.0, 1.0)
        bsdf.inputs["Emission Color"].default_value = (1.0, 1.0, 1.0, 1.0)
        bsdf.inputs["Emission Strength"].default_value = GLOW
    else:
        tex = nodes.new("ShaderNodeTexImage")
        tex.image = image
        mat.node_tree.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
        mat.node_tree.links.new(tex.outputs["Color"], bsdf.inputs["Emission Color"])
        bsdf.inputs["Emission Strength"].default_value = GLOW
    slot.material = mat

scene = bpy.context.scene
cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
scene.collection.objects.link(cam)
cam.data.type = "ORTHO"
radius = max(die.dimensions) * 0.5 * math.sqrt(3)
cam.data.ortho_scale = radius * 2.15
cam.location = (0.0, -6.0, 1.5)
cam.rotation_euler = (math.radians(76), 0.0, 0.0)
scene.camera = cam
for rot, energy in (((math.radians(40), 0.0, math.radians(25)), 4.0), ((math.radians(65), 0.0, math.radians(-140)), 1.5)):
    sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
    sun.data.energy = energy
    sun.rotation_euler = rot
    scene.collection.objects.link(sun)
world = bpy.data.worlds.new("World")
scene.world = world
world.use_nodes = True
world.node_tree.nodes["Background"].inputs[1].default_value = 0.8

engines = {e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items}
scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in engines else "BLENDER_EEVEE"
scene.render.film_transparent = True
scene.render.resolution_x = scene.render.resolution_y = size
scene.render.image_settings.file_format = "PNG"
scene.render.image_settings.color_mode = "RGBA"
scene.view_settings.view_transform = "Standard"

rows = math.ceil(FRAMES / COLUMNS)
sheet = np.zeros((rows * size, COLUMNS * size, 4), dtype=np.float32)
frame_path = out.replace(".png", "_frame.png")
for frame in range(FRAMES):
    die.rotation_quaternion = Matrix.Rotation(2 * math.pi * frame / FRAMES, 4, AXIS).to_quaternion() @ base_rotation
    scene.render.filepath = frame_path
    bpy.ops.render.render(write_still=True)
    rendered = bpy.data.images.load(frame_path, check_existing=False)
    pixels = np.array(rendered.pixels[:], dtype=np.float32).reshape(size, size, 4)
    bpy.data.images.remove(rendered)
    # Blender images are stored bottom row first; the sheet's first row is at its top
    row, col = divmod(frame, COLUMNS)
    top = (rows - 1 - row) * size
    sheet[top:top + size, col * size:(col + 1) * size] = pixels

result = bpy.data.images.new("Sheet", COLUMNS * size, rows * size, alpha=True)
result.pixels = sheet.ravel()
result.filepath_raw = out
result.file_format = "PNG"
result.save()
print(f"SPIN {out}: {FRAMES} frames of {size}px, {COLUMNS} x {rows}")
