"""Render one textured Tripo mesh from a raised 3/4 view on a plain light background, filling the frame - an input image
for Tripo image-to-3D (e.g. remaking an old HD model as Smart Mesh with the same look). Run inside Blender.

Usage: blender -b --factory-startup --python blender_hero_view.py -- <out png> <fbx> <basecolor> [--yaw 35] [--pitch 25]
       [--px 1024]
Tripo FBX stand on +Z; yaw 0 looks at the -Y side (Tripo's front), positive yaw turns the camera toward +X.
"""
import math
import sys

import bpy
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:]
opts = {"--yaw": 35.0, "--pitch": 25.0, "--px": 1024}
rest = []
i = 0
while i < len(argv):
    if argv[i] in opts:
        opts[argv[i]] = float(argv[i + 1])
        i += 2
    else:
        rest.append(argv[i])
        i += 1
out, fbx, tex = rest[0], rest[1], rest[2]
px = int(opts["--px"])

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
engines = {e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items}
scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in engines else "BLENDER_EEVEE"
scene.render.resolution_x = px
scene.render.resolution_y = px
scene.view_settings.view_transform = "Standard"
world = bpy.data.worlds.new("World")
world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.92, 0.92, 0.93, 1.0)
world.node_tree.nodes["Background"].inputs[1].default_value = 1.0
scene.world = world
sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
sun.data.energy = 2.5
sun.rotation_euler = (math.radians(40), 0, math.radians(-30))
scene.collection.objects.link(sun)

bpy.ops.import_scene.fbx(filepath=fbx)
meshes = [o for o in bpy.context.selected_objects if o.type == "MESH"]
img = bpy.data.images.load(tex)
for o in meshes:
    mat = bpy.data.materials.new("Tex")
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    t = nodes.new("ShaderNodeTexImage")
    t.image = img
    bsdf = nodes["Principled BSDF"]
    mat.node_tree.links.new(t.outputs["Color"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.8
    o.data.materials.clear()
    o.data.materials.append(mat)

corners = [o.matrix_world @ Vector(c) for o in meshes for c in o.bound_box]
lo = Vector((min(c.x for c in corners), min(c.y for c in corners), min(c.z for c in corners)))
hi = Vector((max(c.x for c in corners), max(c.y for c in corners), max(c.z for c in corners)))
center = (lo + hi) / 2
radius = (hi - lo).length / 2

cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
cam.data.lens = 50
scene.collection.objects.link(cam)
scene.camera = cam
yaw, pitch = math.radians(opts["--yaw"]), math.radians(opts["--pitch"])
direction = Vector((math.sin(yaw) * math.cos(pitch), -math.cos(yaw) * math.cos(pitch), math.sin(pitch)))
fov = 2 * math.atan(18 / cam.data.lens)
cam.location = center + direction * (radius / math.sin(fov / 2)) * 1.02
cam.rotation_euler = (-direction).to_track_quat("-Z", "Y").to_euler()
scene.render.filepath = out
bpy.ops.render.render(write_still=True)
print(f"HERO wrote {out}")
