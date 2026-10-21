"""Bend a rigged character's bones and render it from a few sides, to check the skinning (run inside Blender, -b).

Usage: blender -b --factory-startup --python blender_pose_check.py -- <fbx> <out png> "<bone>:<axis>:<deg>,..." [views]
  Bones without the mixamorig: prefix, axes X/Y/Z in each bone's own frame, e.g. "Tail1:X:-40,LeftUpLeg:X:-45,
  LeftLeg:X:60,RightArm:Z:40,Head:X:20". views: comma list of front, back, left, right (default front,left,back).
Renders the mesh in Workbench (solid, flat grey with cavity) with the rest pose next to the bent one for each view, so
torn faces, stretched seams and parts left behind stand out. Written for the round 72 fluffy tiger's Mixamo rig.
"""
import math
import sys

import bpy
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
src, out, bends = args[0], args[1], args[2]
views = (args[3] if len(args) > 3 else "front,left,back").split(",")

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
arm.animation_data_clear()
meshes = [o for o in bpy.data.objects if o.type == "MESH"]
prefix = next(b.name for b in arm.data.bones if b.name.endswith("Hips")).replace("Hips", "")

scene = bpy.context.scene
scene.render.engine = "BLENDER_WORKBENCH"
scene.display.shading.light = "STUDIO"
scene.display.shading.color_type = "SINGLE"
scene.display.shading.single_color = (0.75, 0.72, 0.68)
scene.display.shading.show_cavity = True
scene.render.resolution_x = 420
scene.render.resolution_y = 560
scene.render.film_transparent = False

pts = [o.matrix_world @ v.co for o in meshes for v in o.data.vertices]
lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
center = (lo + hi) / 2
size = max(hi - lo)
cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
scene.collection.objects.link(cam)
cam.data.type = "ORTHO"
cam.data.ortho_scale = size * 1.25
scene.camera = cam
# Tripo/Mixamo in Blender: the character faces -Y, its left is +X
dirs = {"front": Vector((0, -1, 0)), "back": Vector((0, 1, 0)), "left": Vector((1, 0, 0)), "right": Vector((-1, 0, 0))}


def shoot(path, d):
    cam.location = center + d * size * 3 + Vector((0, 0, size * 0.15))
    cam.rotation_euler = (center - cam.location).to_track_quat("-Z", "Y").to_euler()
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)


tmp = out.rsplit(".", 1)[0]
shots = []
for pose in ("rest", "bent"):
    if pose == "bent":
        for item in bends.split(","):
            name, axis, deg = item.split(":")
            pb = arm.pose.bones.get(prefix + name)
            if not pb:
                print(f"POSECHECK no bone {name}")
                continue
            pb.rotation_mode = "XYZ"
            rot = list(pb.rotation_euler)
            rot["XYZ".index(axis.upper())] += math.radians(float(deg))
            pb.rotation_euler = rot
        bpy.context.view_layer.update()
    for v in views:
        p = f"{tmp}_{pose}_{v}.png"
        shoot(p, dirs[v])
        shots.append(p)

# Sheet: rest row over bent row
imgs = [bpy.data.images.load(p) for p in shots]
w, h = imgs[0].size
cols = len(views)
import numpy as np
sheet = np.zeros((h * 2, w * cols, 4), np.float32)
for i, im in enumerate(imgs):
    a = np.array(im.pixels[:], np.float32).reshape(h, w, 4)
    r, c = divmod(i, cols)
    sheet[(1 - r) * h:(2 - r) * h, c * w:(c + 1) * w] = a  # Blender images are bottom-up: row 0 (rest) goes on top
res = bpy.data.images.new("sheet", w * cols, h * 2)
res.pixels[:] = sheet.ravel()
res.filepath_raw = out
res.file_format = "PNG"
res.save()
print(f"POSECHECK wrote {out}")
