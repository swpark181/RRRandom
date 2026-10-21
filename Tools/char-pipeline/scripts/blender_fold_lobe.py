"""Fold a stray second lobe of a Tripo mesh into the main one (run in Blender): the red fox's tail forks behind her into
a lobe going straight back and one going back and to her left (the concept's front view had the tail out to the side,
the back view hanging down, and Tripo made both), so from behind she had two tails. The side lobe is turned about a
vertical axis onto the main lobe's direction and shrunk a little toward the pivot, so it ends up inside the main lobe;
the turn fades in across the fork, so nothing tears where the two lobes meet.

Usage: blender -b --factory-startup --python blender_fold_lobe.py -- <in obj> <out obj> <pivot x,y> <main deg>
       <side deg> <shrink> [zmax] [preview png]
  In the frame blender_tripo_to_obj.py's OBJ has on the Mixamo rig (m, divided by 100; it faces -Y, +X is its left, Z up).
  <pivot x,y>: where the lobes part (seen from above). <main deg>, <side deg>: the lobes' directions from the pivot,
  in degrees from +Y (behind it) toward +X. Vertices whose direction from the pivot is past halfway from the main lobe to
  the side lobe (fading in over a fifth of the gap on either side) and below zmax (default 0.33) are the side lobe; they
  turn by (main - side) and their distance from the pivot shrinks by <shrink> (0.85 = 15 %). Both fade in over the first
  0.04 m from the pivot. The OBJ keeps the input's mtllib/usemtl lines. The preview is a view from above, side lobe in red.
"""
import math
import sys

import bpy
from mathutils import Matrix, Vector

args = sys.argv[sys.argv.index("--") + 1:]
src, out = args[0], args[1]
pivot = Vector(tuple(float(c) for c in args[2].split(",")))
main_deg, side_deg, shrink = float(args[3]), float(args[4]), float(args[5])
zmax = float(args[6]) if len(args) > 6 else 0.33
preview = args[7] if len(args) > 7 else None
SCALE = 0.01


def smooth(t):
    t = min(1.0, max(0.0, t))
    return t * t * (3 - 2 * t)


bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.wm.obj_import(filepath=src, forward_axis="NEGATIVE_Z", up_axis="Y")
obj = next(o for o in bpy.data.objects if o.type == "MESH")
obj.data.transform(Matrix.Scale(SCALE, 4) @ obj.matrix_world)
obj.matrix_world = Matrix.Identity(4)
mid = (main_deg + side_deg) / 2
band = abs(side_deg - main_deg) / 5
sign = 1 if side_deg > main_deg else -1
turn = math.radians(main_deg - side_deg)
weights = []
moved = 0
for v in obj.data.vertices:
    d = v.co.xy - pivot
    w = 0.0
    if v.co.z < zmax and d.y > -0.02:
        ang = math.degrees(math.atan2(d.x, d.y))
        w = smooth((sign * (ang - mid) + band) / (2 * band)) * smooth(d.length / 0.04)
        # Fade out toward zmax too, so the hips above aren't touched
        w *= smooth((zmax - v.co.z) / 0.05)
    weights.append(w)
    if w > 0:
        a = turn * w
        c, s = math.cos(a), math.sin(a)
        # atan2(x, y) grows toward +X, so a positive angle turns +Y toward +X: clockwise seen from above
        rx = c * d.x + s * d.y
        ry = -s * d.x + c * d.y
        k = 1 - (1 - shrink) * w
        v.co.x = pivot.x + rx * k
        v.co.y = pivot.y + ry * k
        moved += 1
print(f"FOLDLOBE {moved} vertices moved, {sum(1 for w in weights if w > 0.99)} fully")

if preview:
    attr = obj.data.color_attributes.new("W", "FLOAT_COLOR", "POINT")
    for i, w in enumerate(weights):
        attr.data[i].color = (0.7 + 0.3 * w, 0.7 - 0.5 * w, 0.7 - 0.5 * w, 1.0)
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.color_type = "VERTEX"
    scene.render.resolution_x = scene.render.resolution_y = 600
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    scene.collection.objects.link(cam)
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = 0.6
    cam.data.clip_start = 2.0 - zmax
    cam.location = (pivot.x + 0.05, pivot.y + 0.1, 2.0)
    scene.camera = cam
    scene.render.filepath = preview
    bpy.ops.render.render(write_still=True)

obj.data.transform(Matrix.Scale(1 / SCALE, 4))
bpy.ops.wm.obj_export(filepath=out, export_selected_objects=False, export_materials=False, export_uv=True,
                      export_normals=True, forward_axis="NEGATIVE_Z", up_axis="Y")
header = [line for line in open(src, encoding="utf-8") if line.startswith(("mtllib", "usemtl"))]
with open(out, "r", encoding="utf-8") as f:
    body = [line for line in f if not line.startswith(("mtllib", "usemtl"))]
with open(out, "w", encoding="utf-8") as f:
    f.writelines(header + body)
print(f"FOLDLOBE wrote {out}")
