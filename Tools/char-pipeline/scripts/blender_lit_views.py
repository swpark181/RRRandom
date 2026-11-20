"""Lit, textured pictures of a head from the front, three-quarter and side (run inside Blender), 2026-11-20: to compare
the old and the new tiger's chin as the game shows them (shape AND texture together; blender_face_audit.py's textured
pictures are unlit and its clay ones untextured).

Usage: blender -b --factory-startup --python blender_lit_views.py -- <fbx> <basecolor> <out prefix>
           [--px 700] [--cz 0.63] [--span 0.5] [--views front,q34,side,q34l,low]
Every mesh of the file is shown with the one texture. Writes <prefix>_lit_<view>.png.
"""
import sys

import bpy
import numpy as np
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:]
fbx, tex, prefix = argv[:3]
opts = dict(zip(argv[3::2], argv[4::2]))
px = int(opts.get("--px", 700))
cz = float(opts.get("--cz", 0.63))
span = float(opts.get("--span", 0.5))
views = opts.get("--views", "front,q34,side").split(",")

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
bpy.ops.import_scene.fbx(filepath=fbx)
for a in [o for o in bpy.data.objects if o.type == "ARMATURE"]:
    a.animation_data_clear()
    a.data.pose_position = "REST"
meshes = [o for o in bpy.data.objects if o.type == "MESH"]
m = bpy.data.materials.new("M")
m.use_nodes = True
t = m.node_tree.nodes.new("ShaderNodeTexImage")
t.image = bpy.data.images.load(tex)
m.node_tree.links.new(t.outputs["Color"], m.node_tree.nodes["Principled BSDF"].inputs["Base Color"])
m.node_tree.nodes.active = t
for o in meshes:
    o.data.materials.clear()
    o.data.materials.append(m)
big = max(meshes, key=lambda o: len(o.data.polygons))
verts = np.array([o.matrix_world @ v.co for o in meshes for v in o.data.vertices])
lo, hi = verts[:, 2].min(), verts[:, 2].max()
h = hi - lo
head = verts[verts[:, 2] > lo + h * 0.55]
cx = (head[:, 0].min() + head[:, 0].max()) / 2
fz = lo + h * 0.66
face = verts[np.abs(verts[:, 2] - fz) < h * 0.12]
cy = (face[:, 1].min() + face[:, 1].max()) / 2
target = Vector((cx, cy, lo + h * cz))
cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
scene.collection.objects.link(cam)
scene.camera = cam
cam.data.type = "ORTHO"
cam.data.ortho_scale = span * h
scene.render.engine = "BLENDER_WORKBENCH"
scene.render.resolution_x = scene.render.resolution_y = px
scene.view_settings.view_transform = "Standard"
world = bpy.data.worlds.new("W")
world.color = (0.78, 0.79, 0.81)
scene.world = world
sh = scene.display.shading
sh.light = "STUDIO"
sh.color_type = "TEXTURE"
sh.show_cavity = True
sh.cavity_type = "BOTH"
VIEWS = {"front": Vector((0, -1, 0)), "q34": Vector((1, -1, 0.05)).normalized(), "side": Vector((1, 0, 0)),
         "q34l": Vector((-1, -1, 0.05)).normalized(), "low": Vector((0, -1, -0.45)).normalized(),
         "high": Vector((0, -1, 0.45)).normalized()}
for name in views:
    d = VIEWS[name]
    cam.location = target + d * h * 4
    cam.rotation_euler = (-d).to_track_quat("-Z", "Y").to_euler()
    scene.render.filepath = f"{prefix}_lit_{name}.png"
    bpy.ops.render.render(write_still=True)
print("LIT done", prefix)
