"""Contact sheet of an animation FBX that carries its mesh (run inside Blender): N frames across the clip, one row from
the side and one from the front quarter, the camera following the hips so foot placing and posture can be read.

Usage: blender -b --factory-startup --python blender_anim_sheet.py -- <anim fbx> <out png> [frames=8] [texture png]
A thin floor line is drawn at the lowest point the feet reach in the whole clip.
"""
import math
import os
import sys
import tempfile

import bpy
import numpy as np
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
fbx, out = args[0], args[1]
N = int(args[2]) if len(args) > 2 else 8
tex = args[3] if len(args) > 3 else None
CELL = 300

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=fbx)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
meshes = [o for o in bpy.data.objects if o.type == "MESH"]
act = arm.animation_data.action if arm.animation_data else None
f0, f1 = (int(act.frame_range[0]), int(act.frame_range[1])) if act else (1, 1)
scene = bpy.context.scene
scene.frame_start, scene.frame_end = f0, f1


def bone_world(name):
    pb = next(b for b in arm.pose.bones if b.name.split(":")[-1] == name)
    return arm.matrix_world @ pb.head


def mesh_bounds():
    dg = bpy.context.evaluated_depsgraph_get()
    lo, hi = Vector((1e9,) * 3), Vector((-1e9,) * 3)
    for m in meshes:
        e = m.evaluated_get(dg)
        for c in e.bound_box:
            w = e.matrix_world @ Vector(c)
            lo = Vector(map(min, lo, w)); hi = Vector(map(max, hi, w))
    return lo, hi


scene.frame_set(f0); bpy.context.view_layer.update()
p0 = bone_world("Hips").copy()
lo, hi = mesh_bounds()
height = hi.z - lo.z
scene.frame_set(f1); bpy.context.view_layer.update()
p1 = bone_world("Hips").copy()
travel = Vector((p1.x - p0.x, p1.y - p0.y, 0))
# facing: from the hips towards the head's side is unknown, so use the toes: they point where the character faces
scene.frame_set(f0); bpy.context.view_layer.update()
toe = (bone_world("LeftToeBase") + bone_world("RightToeBase")) / 2 - (bone_world("LeftFoot") + bone_world("RightFoot")) / 2
fwd = Vector((toe.x, toe.y, 0)).normalized()
side = Vector((fwd.y, -fwd.x, 0))
floor = min(min(bone_world(b).z for b in ("LeftToeBase", "RightToeBase")) for _ in [0])
for f in range(f0, f1 + 1, max(1, (f1 - f0) // 40)):
    scene.frame_set(f)
    floor = min(floor, min(bone_world(b).z for b in ("LeftToeBase", "RightToeBase", "LeftFoot", "RightFoot")))

# look: flat workbench shading with the texture when there is one
scene.render.engine = "BLENDER_WORKBENCH"
scene.display.shading.light = "STUDIO"
scene.display.shading.color_type = "TEXTURE" if tex else "MATERIAL"
scene.display.shading.show_object_outline = True
scene.render.film_transparent = False
scene.world = bpy.data.worlds.new("w")
scene.world.color = (0.86, 0.88, 0.9)
scene.render.resolution_x = scene.render.resolution_y = CELL
scene.view_settings.view_transform = "Standard"
if tex:
    img = bpy.data.images.load(tex)
    mat = bpy.data.materials.new("fox")
    mat.use_nodes = True
    nt = mat.node_tree
    n = nt.nodes.new("ShaderNodeTexImage"); n.image = img
    bsdf = next(x for x in nt.nodes if x.type == "BSDF_PRINCIPLED")
    nt.links.new(n.outputs["Color"], bsdf.inputs["Base Color"])
    nt.nodes.active = n
    for m in meshes:
        m.data.materials.clear(); m.data.materials.append(mat)

cam_data = bpy.data.cameras.new("c"); cam_data.type = "ORTHO"; cam_data.ortho_scale = height * 1.25
cam = bpy.data.objects.new("c", cam_data); scene.collection.objects.link(cam); scene.camera = cam
# the floor line: a thin long box
bpy.ops.mesh.primitive_cube_add(size=1)
line = bpy.context.object
line.scale = (height * 40, height * 40, height * 0.004)
line.location = (p0.x, p0.y, floor - height * 0.004)
lm = bpy.data.materials.new("line"); lm.diffuse_color = (0.25, 0.3, 0.38, 1)
line.data.materials.append(lm)

tmp = tempfile.mkdtemp()
rows = []
for view, direction in (("side", side), ("quarter", (fwd * 0.8 + side * 0.6).normalized())):
    cells = []
    for i in range(N):
        f = round(f0 + (f1 - f0) * i / max(1, N - 1))
        scene.frame_set(f); bpy.context.view_layer.update()
        h = bone_world("Hips")
        target = Vector((h.x, h.y, floor + height * 0.5))
        cam.location = target + direction * height * 6 + Vector((0, 0, height * (0.0 if view == "side" else 0.6)))
        cam.rotation_euler = (target - cam.location).to_track_quat("-Z", "Y").to_euler()
        path = os.path.join(tmp, f"{view}_{i}.png")
        scene.render.filepath = path
        bpy.ops.render.render(write_still=True)
        im = bpy.data.images.load(path)
        px = np.empty(CELL * CELL * 4, np.float32); im.pixels.foreach_get(px)
        cells.append(px.reshape(CELL, CELL, 4)[::-1])
        bpy.data.images.remove(im)
    rows.append(np.concatenate(cells, axis=1))
sheet = np.concatenate(rows, axis=0)[::-1]
h, w = sheet.shape[:2]
res = bpy.data.images.new("sheet", w, h, alpha=True)
res.pixels.foreach_set(sheet.ravel())
res.filepath_raw = out; res.file_format = "PNG"; res.save()
print(f"SHEET {out} frames {f0}-{f1} travel {tuple(round(v, 3) for v in travel)} height {height:.3f} floor {floor:.3f} "
      f"hips over floor at start {p0.z - floor:.3f}")
