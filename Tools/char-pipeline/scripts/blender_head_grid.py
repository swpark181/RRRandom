"""Render an animal friend's head from the front, the side, behind and above with a world grid on top, to read where its
ears (or fins) start and end (2026-11-25, ear bones for the fox, red fox, rabbit, tiger and dragon).

Usage: blender -b -P blender_head_grid.py -- <rig fbx> <out prefix> [--size 0.5] [--px 900] [--mark "x,y,z;x,y,z"]
          [--color "<vertex group prefix>"]

Writes <out prefix>_front.png, _side.png (from the animal's left, +X), _back.png, _top.png. Grid lines every 1 cm (faint),
5 cm (grey) and 10 cm (red on X, green on Y, blue on Z), centred on the Head bone, which is printed with the mapping.
--mark: points drawn as yellow crosses (to check picked ear bases and tips). --color: faces whose vertices are weighted
over half to groups starting with this text are painted magenta (to check an ear's weights after blender_ear_bones.py).
The model faces -Y in Blender (the game's +Y); the head bone is mixamorig:Head or Head.
"""
import math
import sys

import bpy
import numpy as np
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
rig, out = args[:2]


def opt(name, default):
    return type(default)(args[args.index(name) + 1]) if name in args else default


SIZE = opt("--size", 0.5)
PX = opt("--px", 900)
MARKS = [Vector([float(c) for c in p.split(",")]) for p in opt("--mark", "").split(";") if p.strip()]
COLOR = opt("--color", "")

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=rig)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
arm.data.pose_position = "REST"
hb = next(b for b in arm.data.bones if b.name.endswith("Head"))
head = arm.matrix_world @ hb.head_local
centre = head + Vector((0.0, 0.0, SIZE * opt("--up", 0.25)))
print(f"GRID head bone at {tuple(round(c, 4) for c in head)}, view centre {tuple(round(c, 4) for c in centre)}, {SIZE} m across {PX} px")

for o in [o for o in bpy.data.objects if o.type == "MESH"]:
    mat = bpy.data.materials.new("grey")
    mat.diffuse_color = (0.8, 0.8, 0.8, 1.0)
    o.data.materials.clear()
    o.data.materials.append(mat)
    if COLOR:
        pink = bpy.data.materials.new("pink")
        pink.diffuse_color = (1.0, 0.1, 0.8, 1.0)
        o.data.materials.append(pink)
        ids = {g.index for g in o.vertex_groups if g.name.replace("mixamorig:", "").startswith(COLOR)}
        share = [sum(g.weight for g in v.groups if g.group in ids) for v in o.data.vertices]
        for p in o.data.polygons:
            p.material_index = 1 if sum(share[i] for i in p.vertices) / len(p.vertices) > 0.5 else 0

scene = bpy.context.scene
scene.render.engine = "BLENDER_WORKBENCH"
scene.display.shading.light = "STUDIO"
scene.display.shading.color_type = "MATERIAL"
scene.display.shading.show_cavity = True
scene.render.resolution_x = scene.render.resolution_y = PX
scene.render.film_transparent = False
scene.world = bpy.data.worlds.new("w")
scene.world.color = (0.15, 0.15, 0.17)
cam_data = bpy.data.cameras.new("cam")
cam_data.type = "ORTHO"
cam_data.ortho_scale = SIZE
cam = bpy.data.objects.new("cam", cam_data)
scene.collection.objects.link(cam)
scene.camera = cam

# view name: (camera offset direction, image right axis, image up axis)
VIEWS = {
    "front": (Vector((0, -1, 0)), Vector((1, 0, 0)), Vector((0, 0, 1))),
    "side": (Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))),
    "back": (Vector((0, 1, 0)), Vector((-1, 0, 0)), Vector((0, 0, 1))),
    "top": (Vector((0, 0, 1)), Vector((1, 0, 0)), Vector((0, 1, 0))),
}
AXIS_COLOR = [(1.0, 0.25, 0.25), (0.25, 1.0, 0.25), (0.35, 0.55, 1.0)]
for name, (off, right, up) in VIEWS.items():
    cam.location = centre + off * 3.0
    fwd = -off
    cam.rotation_euler = fwd.to_track_quat("-Z", "Y" if name != "top" else "Y").to_euler()
    if name == "top":
        cam.rotation_euler = (0.0, 0.0, 0.0)
    path = f"{out}_{name}.png"
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    img = bpy.data.images.load(path)
    px = np.array(img.pixels[:], dtype=np.float32).reshape(PX, PX, 4)   # rows bottom to top
    per = PX / SIZE

    def to_px(p):
        d = p - centre
        return d.dot(right) * per + PX / 2, d.dot(up) * per + PX / 2

    # grid: for the image's horizontal world axis and vertical world axis
    for axis_vec, horizontal in ((right, True), (up, False)):
        k = next(i for i in range(3) if abs(axis_vec[i]) > 0.5)
        sign = axis_vec[k]
        c0 = centre[k]
        lo = c0 - SIZE / 2
        for cm in range(int(math.floor(lo * 100)), int(math.ceil((lo + SIZE) * 100)) + 1):
            w = cm / 100.0
            pos = int(round((w - c0) * sign * per + PX / 2))
            if not 0 <= pos < PX:
                continue
            if cm % 10 == 0:
                col, a = AXIS_COLOR[k], 0.9
            elif cm % 5 == 0:
                col, a = (0.6, 0.6, 0.6), 0.6
            else:
                col, a = (0.5, 0.5, 0.5), 0.18
            sl = (slice(None), pos) if horizontal else (pos, slice(None))
            px[sl][..., :3] = px[sl][..., :3] * (1 - a) + np.array(col) * a
    for m in MARKS:
        x, y = to_px(m)
        x, y = int(round(x)), int(round(y))
        for d in range(-9, 10):
            for (cx, cy) in ((x + d, y), (x, y + d)):
                if 0 <= cx < PX and 0 <= cy < PX:
                    px[cy, cx, :3] = (1.0, 0.95, 0.1)
    img.pixels[:] = px.ravel()
    img.save()
    hx = to_px(head)
    print(f"GRID {name}: right = {tuple(right)}, up = {tuple(up)}, head bone at px {hx[0]:.0f},{PX - hx[1]:.0f} from top-left -> {path}")
