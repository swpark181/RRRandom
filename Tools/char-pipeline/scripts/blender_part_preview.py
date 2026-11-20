"""Pictures of a body + face part FBX from blender_split_part.py (run inside Blender), 2026-11-20: the head lit and
textured from the front, three-quarter and side, the same with the part tinted, and with the part pulled forward.

Usage: blender -b --factory-startup --python blender_part_preview.py -- <parts fbx> <basecolor> <out png prefix>
           [--px 600] [--pull 0.10] [--span 0.62]
Writes <prefix>_<lit|tint|pull>_<front|q34|side>.png
"""
import sys

import bpy
import numpy as np
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:]
fbx, tex, prefix = argv[:3]
opts = dict(zip(argv[3::2], argv[4::2]))
px = int(opts.get("--px", 600))
pull = float(opts.get("--pull", 0.10))
span = float(opts.get("--span", 0.62))

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
bpy.ops.import_scene.fbx(filepath=fbx)
for a in [o for o in bpy.data.objects if o.type == "ARMATURE"]:
    a.animation_data_clear()
    a.data.pose_position = "REST"
meshes = [o for o in bpy.data.objects if o.type == "MESH"]
body = next(o for o in meshes if o.name.startswith("Body"))
part = next(o for o in meshes if o.name.startswith("Muzzle"))
img = bpy.data.images.load(tex)


def mat(name, tint):
    # (the viewport's texture shading shows the image itself, so the tint is painted into a copy of it)
    im = img
    if tint > 0:
        im = img.copy()
        p = np.array(img.pixels[:], dtype=np.float32).reshape(-1, 4)
        p[:, :3] = p[:, :3] * (1 - tint) + np.array([0.2, 0.55, 1.0]) * tint
        im.pixels[:] = p.ravel().tolist()
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    t = nt.nodes.new("ShaderNodeTexImage")
    t.image = im
    nt.links.new(t.outputs["Color"], nt.nodes["Principled BSDF"].inputs["Base Color"])
    nt.nodes.active = t
    return m


plain = mat("plain", 0.0)
tinted = mat("tinted", 0.45)
for o in meshes:
    o.data.materials.clear()
    o.data.materials.append(plain)
verts = np.array([body.matrix_world @ v.co for v in body.data.vertices])
lo, hi = verts[:, 2].min(), verts[:, 2].max()
h = hi - lo
head = verts[verts[:, 2] > lo + h * 0.55]
cx = (head[:, 0].min() + head[:, 0].max()) / 2
fz = lo + h * 0.64
face = verts[np.abs(verts[:, 2] - fz) < h * 0.12]
cy = (face[:, 1].min() + face[:, 1].max()) / 2
target = Vector((cx, cy, fz))

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
VIEWS = {"front": Vector((0, -1, 0)), "q34": Vector((1, -1, 0.05)).normalized(), "side": Vector((1, 0, 0))}
part_base = part.matrix_world.copy()
for mode in ("lit", "tint", "pull"):
    part.data.materials[0] = tinted if mode != "lit" else plain
    part.matrix_world = part_base
    if mode == "pull":
        m = part_base.copy()
        m.translation = m.translation + Vector((0, -pull * h, 0))
        part.matrix_world = m
    for name, d in VIEWS.items():
        cam.location = target + d * h * 4
        cam.rotation_euler = (-d).to_track_quat("-Z", "Y").to_euler()
        scene.render.filepath = f"{prefix}_{mode}_{name}.png"
        bpy.ops.render.render(write_still=True)
print("PREVIEW done", prefix)
