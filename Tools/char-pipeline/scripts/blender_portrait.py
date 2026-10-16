"""Render a character's head-and-shoulders portrait for the HUD (run inside Blender).

Usage: blender -b --factory-startup --python blender_portrait.py -- <character fbx with textures> <out png> [size] [head x]
  e.g. ... -- C:/work/char-pipeline/input/fox/mixamo/SK_Fox.fbx C:/work/char-pipeline/output/ui/T_FoxPortrait.png 256

Front view (the Mixamo rig faces -Y in Blender), a little from the side so it reads as a face and not a mask, framed
from the shoulders to the ear tips, on a transparent background, flat-ish lighting so the base color carries it.
The rig's first animation frame is used as the pose (the rifle idle for the fox).
"""
import math
import sys

import bpy
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
src, out = args[0], args[1]
size = int(args[2]) if len(args) > 2 else 256
# Where the face is across (Blender x), when something beside the head (the squirrel's tail) would pull the average off it
head_x = float(args[3]) if len(args) > 3 else None

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
scene = bpy.context.scene
scene.frame_set(0)
meshes = [o for o in scene.objects if o.type == "MESH"]
arm = next((o for o in scene.objects if o.type == "ARMATURE"), None)

# Frame: from the neck bone up to the top of the mesh
depsgraph = bpy.context.evaluated_depsgraph_get()
pts = []
for m in meshes:
    ev = m.evaluated_get(depsgraph)
    pts += [ev.matrix_world @ v.co for v in ev.to_mesh().vertices]
top = max(p.z for p in pts)
neck = None
if arm:
    for name in ("mixamorig:Neck", "mixamorig:Spine2"):
        bone = arm.pose.bones.get(name)
        if bone:
            neck = (arm.matrix_world @ bone.head).z
            break
bottom = (neck - (top - neck) * 0.25) if neck is not None else top * 0.45
center_z = (top + bottom) / 2
head = [p for p in pts if p.z > bottom]
center_x = head_x if head_x is not None else sum(p.x for p in head) / len(head)
height = top - bottom

cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
scene.collection.objects.link(cam)
cam.data.type = "ORTHO"
cam.data.ortho_scale = height * 1.08
yaw = math.radians(18)
dist = 3.0
cam.location = Vector((center_x + math.sin(yaw) * dist, -math.cos(yaw) * dist, center_z))
cam.rotation_euler = (math.radians(90), 0.0, yaw)
scene.camera = cam

for rot, energy in (((math.radians(50), 0.0, math.radians(30)), 3.0), ((math.radians(70), 0.0, math.radians(-120)), 1.2)):
    sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
    sun.data.energy = energy
    sun.rotation_euler = rot
    scene.collection.objects.link(sun)
world = bpy.data.worlds.new("World")
scene.world = world
world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.8, 0.8, 0.8, 1.0)
world.node_tree.nodes["Background"].inputs[1].default_value = 0.6

engines = {e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items}
scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in engines else "BLENDER_EEVEE"
scene.render.film_transparent = True
scene.render.resolution_x = scene.render.resolution_y = size
scene.render.image_settings.file_format = "PNG"
scene.render.image_settings.color_mode = "RGBA"
scene.view_settings.view_transform = "Standard"
scene.render.filepath = out
bpy.ops.render.render(write_still=True)
print(f"PORTRAIT {out}: {size}px, head {bottom:.3f}..{top:.3f}, center x {center_x:.3f}")
