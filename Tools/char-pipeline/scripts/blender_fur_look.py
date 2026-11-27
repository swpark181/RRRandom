"""Render a body FBX with its shell-fur FBX on it, textured, from given sides, to check fur coverage before Unreal
(2026-11-25, the rabbit's round tail). The fur layers are drawn with the body texture, the strand cut-out ignored, so a
furred patch shows as a thicker, puffier surface.
Usage: blender -b -P blender_fur_look.py -- <body fbx> <fur fbx> <basecolor> <png prefix> <cx,cy,cz> [<size 0.45>]
Writes <prefix>_side.png (from +X), _back.png (from +Y), _q.png (back three-quarter)."""
import math
import sys

import bpy
from mathutils import Vector

a = sys.argv[sys.argv.index("--") + 1:]
body, fur, tex, out, mid = a[:5]
size = float(a[5]) if len(a) > 5 else 0.45
mid = Vector([float(c) for c in mid.split(",")])
bpy.ops.wm.read_factory_settings(use_empty=True)
for p in (body, fur):
    bpy.ops.import_scene.fbx(filepath=p)
img = bpy.data.images.load(tex)
for o in [o for o in bpy.data.objects if o.type == "MESH"]:
    mt = bpy.data.materials.new("t")
    mt.use_nodes = True
    tn = mt.node_tree.nodes.new("ShaderNodeTexImage")
    tn.image = img
    mt.node_tree.links.new(tn.outputs["Color"], mt.node_tree.nodes["Principled BSDF"].inputs["Base Color"])
    o.data.materials.clear()
    o.data.materials.append(mt)
    if o.data.uv_layers:
        o.data.uv_layers.active = o.data.uv_layers[0]
        o.data.uv_layers[0].active_render = True
scene = bpy.context.scene
scene.render.engine = "BLENDER_WORKBENCH"
scene.display.shading.color_type = "TEXTURE"
scene.render.resolution_x = scene.render.resolution_y = 600
scene.world = bpy.data.worlds.new("w")
scene.world.color = (0.15, 0.15, 0.17)
cd = bpy.data.cameras.new("c")
cd.type = "ORTHO"
cd.ortho_scale = size
cam = bpy.data.objects.new("c", cd)
scene.collection.objects.link(cam)
scene.camera = cam
for name, off in (("side", Vector((1, 0, 0))), ("back", Vector((0, 1, 0))), ("q", Vector((0.7, 0.7, 0.25)).normalized())):
    cam.location = mid + off * 3.0
    cam.rotation_euler = (-off).to_track_quat("-Z", "Y").to_euler()
    scene.render.filepath = f"{out}_{name}.png"
    bpy.ops.render.render(write_still=True)
print(f"FURLOOK {out}_side/back/q.png")
