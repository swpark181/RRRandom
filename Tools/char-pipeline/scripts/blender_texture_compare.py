"""Render one Tripo mesh with several base color textures from the same views, a row per texture (run inside Blender).

Usage: blender -b --factory-startup --python blender_texture_compare.py -- <out png> <fbx> <texture> [<texture> ...]
          [--views front,back,left,low,waist,head_back] [--px 512]
       --views turn8 = the eight-direction turnaround (front, front_left, left, back_left, back, back_right, right,
       front_right); left/right name the side of the IMAGE she faces, as in the 8-direction concept art.

Views: front (looking at the face), back, left (her left side), low (from below the waist, the angle that shows a
belt's underside), waist (close on the torso), head_back (close on the back of the head). The mesh's front is found
from where the texture's eyes are not needed: Tripo FBX for Blender stand on +Z and face -Y.
"""
import math
import sys

import bpy
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:]
views = ["front", "back", "left", "low"]
px = 512
rest = []
i = 0
while i < len(argv):
    if argv[i] == "--views":
        views = argv[i + 1].replace("turn8", "front,front_left,left,back_left,back,back_right,right,front_right").split(",")
        i += 2
    elif argv[i] == "--px":
        px = int(argv[i + 1])
        i += 2
    else:
        rest.append(argv[i])
        i += 1
out, fbx, textures = rest[0], rest[1], rest[2:]

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
engines = {e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items}
scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in engines else "BLENDER_EEVEE"
scene.render.resolution_x = px
scene.render.resolution_y = px
scene.view_settings.view_transform = "Standard"
world = bpy.data.worlds.new("World")
world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.25, 0.25, 0.27, 1.0)
world.node_tree.nodes["Background"].inputs[1].default_value = 1.6
scene.world = world
sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
sun.data.energy = 2.0
sun.rotation_euler = (math.radians(50), 0, math.radians(-30))
scene.collection.objects.link(sun)

bpy.ops.import_scene.fbx(filepath=fbx)
mesh = next(o for o in bpy.context.selected_objects if o.type == "MESH")
corners = [mesh.matrix_world @ Vector(c) for c in mesh.bound_box]
lo = Vector((min(c.x for c in corners), min(c.y for c in corners), min(c.z for c in corners)))
hi = Vector((max(c.x for c in corners), max(c.y for c in corners), max(c.z for c in corners)))
center = (lo + hi) / 2
height = hi.z - lo.z
print(f"VIEWS bounds {lo} .. {hi}")

mat = bpy.data.materials.new("M")
mat.use_nodes = True
tex = mat.node_tree.nodes.new("ShaderNodeTexImage")
bsdf = mat.node_tree.nodes["Principled BSDF"]
mat.node_tree.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
bsdf.inputs["Roughness"].default_value = 0.9
mesh.data.materials.clear()
mesh.data.materials.append(mat)

cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
scene.collection.objects.link(cam)
scene.camera = cam
cam.data.type = "ORTHO"

waist_z = lo.z + height * 0.36
head_z = lo.z + height * 0.72
VIEWS = {
    "front": (Vector((0, -1, 0)), center, height * 1.05),
    "back": (Vector((0, 1, 0)), center, height * 1.05),
    "left": (Vector((1, 0, 0)), center, height * 1.05),
    "right": (Vector((-1, 0, 0)), center, height * 1.05),
    "front_left": (Vector((1, -1, 0)).normalized(), center, height * 1.05),
    "back_left": (Vector((1, 1, 0)).normalized(), center, height * 1.05),
    "back_right": (Vector((-1, 1, 0)).normalized(), center, height * 1.05),
    "front_right": (Vector((-1, -1, 0)).normalized(), center, height * 1.05),
    "low": (Vector((0, -1, -0.9)).normalized(), Vector((center.x, center.y, waist_z)), height * 0.6),
    "waist": (Vector((0, -1, -0.15)).normalized(), Vector((center.x, center.y, waist_z + height * 0.05)), height * 0.42),
    "chest": (Vector((0, -1, 0)), Vector((center.x, center.y, lo.z + height * 0.44)), height * 0.3),
    "sleeve": (Vector((0.7, -1, 0)).normalized(), Vector((center.x + height * 0.12, center.y, lo.z + height * 0.44)),
               height * 0.2),
    "belt_low": (Vector((0, -1, -0.9)).normalized(), Vector((center.x, center.y, waist_z + height * 0.04)), height * 0.32),
    "pocket": (Vector((0.5, -1, -0.5)).normalized(), Vector((center.x + height * 0.12, center.y, waist_z - height * 0.02)),
               height * 0.2),
    "hem": (Vector((0, -1, -0.2)).normalized(), Vector((center.x, center.y, lo.z + height * 0.17)), height * 0.3),
    "hem_back": (Vector((0, 1, -0.2)).normalized(), Vector((center.x, center.y, lo.z + height * 0.17)), height * 0.3),
    "under": (Vector((0, -0.25, -1)).normalized(), Vector((center.x, center.y, waist_z)), height * 0.55),
    "under_l": (Vector((0.6, -0.5, -1)).normalized(), Vector((center.x + height * 0.12, center.y, waist_z)), height * 0.3),
    "under_r": (Vector((-0.6, -0.5, -1)).normalized(), Vector((center.x - height * 0.12, center.y, waist_z)), height * 0.3),
    # her right arm is on -X (she faces -Y); her left on +X
    "arm_r_back": (Vector((-0.5, 1, 0.1)).normalized(), Vector((center.x - height * 0.2, center.y, lo.z + height * 0.36)),
                   height * 0.32),
    "arm_r_out": (Vector((-1, 0.2, 0)).normalized(), Vector((center.x - height * 0.2, center.y, lo.z + height * 0.36)),
                  height * 0.32),
    "arm_r_close": (Vector((-0.5, 1, 0.1)).normalized(), Vector((center.x - height * 0.235, center.y, lo.z + height * 0.38)),
                    height * 0.12),
    "arm_l_close": (Vector((0.5, 1, 0.1)).normalized(), Vector((center.x + height * 0.235, center.y, lo.z + height * 0.38)),
                    height * 0.12),
    "arm_l_back": (Vector((0.5, 1, 0.1)).normalized(), Vector((center.x + height * 0.2, center.y, lo.z + height * 0.36)),
                   height * 0.32),
    # paws close (her right = -X), from the front-below and from underneath: Tripo leaves stray gold bits there
    "hand_r": (Vector((-0.3, -1, -0.5)).normalized(),
               Vector((center.x - height * 0.215, center.y - height * 0.1, lo.z + height * 0.31)), height * 0.16),
    "hand_l": (Vector((0.3, -1, -0.5)).normalized(),
               Vector((center.x + height * 0.215, center.y - height * 0.1, lo.z + height * 0.31)), height * 0.16),
    "hand_r_under": (Vector((-0.2, -0.2, -1)).normalized(),
                     Vector((center.x - height * 0.215, center.y - height * 0.1, lo.z + height * 0.31)), height * 0.16),
    "hand_l_under": (Vector((0.2, -0.2, -1)).normalized(),
                     Vector((center.x + height * 0.215, center.y - height * 0.1, lo.z + height * 0.31)), height * 0.16),
    "tail_back": (Vector((0, 1, 0.15)).normalized(), Vector((center.x, center.y, lo.z + height * 0.14)), height * 0.36),
    "tail_side": (Vector((1, 0.4, 0.1)).normalized(), Vector((center.x, center.y + height * 0.12, lo.z + height * 0.14)),
                  height * 0.42),
    "collar": (Vector((0, -1, 0.2)).normalized(), Vector((center.x, center.y, lo.z + height * 0.5)), height * 0.2),
    "chin_under":(Vector((0, -0.5, -1)).normalized(), Vector((center.x, center.y, lo.z + height * 0.52)), height * 0.4),
    "face": (Vector((0, -1, 0)), Vector((center.x, center.y, head_z)), height * 0.45),
    "head_back": (Vector((0, 1, 0.25)).normalized(), Vector((center.x, center.y, head_z)), height * 0.55),
}

import os
tmp_dir = os.path.join(os.path.dirname(os.path.abspath(out)), "_views_tmp")
os.makedirs(tmp_dir, exist_ok=True)
tiles = []
for ti, path in enumerate(textures):
    tex.image = bpy.data.images.load(path)
    row = []
    for v in views:
        direction, target, scale = VIEWS[v]
        cam.location = target + direction * height * 3
        cam.rotation_euler = (-direction).to_track_quat("-Z", "Y").to_euler()
        cam.data.ortho_scale = scale
        f = os.path.join(tmp_dir, f"t{ti}_{v}.png")
        scene.render.filepath = f
        bpy.ops.render.render(write_still=True)
        row.append(f)
    tiles.append(row)

# Paste the tiles into one sheet
sheet = bpy.data.images.new("sheet", px * len(views), px * len(textures))
import numpy as np
canvas = np.zeros((px * len(textures), px * len(views), 4), dtype=np.float32)
for r, row in enumerate(tiles):
    for c, f in enumerate(row):
        im = bpy.data.images.load(f)
        a = np.empty(px * px * 4, dtype=np.float32)
        im.pixels.foreach_get(a)
        y = (len(textures) - 1 - r) * px
        canvas[y:y + px, c * px:(c + 1) * px] = a.reshape(px, px, 4)
sheet.pixels.foreach_set(canvas.ravel())
sheet.filepath_raw = out
sheet.file_format = "PNG"
sheet.save()
print(f"VIEWS wrote {out}")
