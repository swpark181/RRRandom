"""Smooth spiky flaps flat in one region of a Tripo mesh (run in Blender), picked by box and base colour.

The film-style fox's long chest ruff was modelled as spiky flaps over the T-shirt collar. Retexturing it from images with
a short collar (Tripo Texture tab) painted the flaps white, but they still stuck out like shards. Here the vertices in a
box whose base colour matches (by default the white shirt) are relaxed toward the average of their neighbours many
times, while everything else stays put, so thin flaps pull back into the surface around them. UVs and the texture are
untouched; only positions move.

Usage: blender -b --factory-startup --python blender_smooth_region.py -- <tripo fbx> <base color> <out fbx>
       "<x0,x1,y0,y1,z0,z1>" [iterations 40] [min light 0.72] [max saturation 0.10]
  The box is in blender_tripo_to_obj.py's OBJ frame (100 tall, feet at 0 and centred, Y up, facing +Z, +X = its left).
  The FBX comes out 1 m tall like Tripo's, Z up, facing -Y (texture paths stripped; keep using the same base color).
"""
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
src, tex, out, box = args[:4]
iters = int(args[4]) if len(args) > 4 else 40
min_light = float(args[5]) if len(args) > 5 else 0.72
max_sat = float(args[6]) if len(args) > 6 else 0.10
x0, x1, y0, y1, z0, z1 = (float(c) for c in box.split(","))

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
meshes = [o for o in bpy.data.objects if o.type == "MESH"]
for o in bpy.data.objects:
    o.select_set(o in meshes)
bpy.context.view_layer.objects.active = meshes[0]
bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
if len(meshes) > 1:
    bpy.ops.object.join()
ob = bpy.context.view_layer.objects.active
for o in [o for o in bpy.data.objects if o.type != "MESH"]:
    bpy.data.objects.remove(o)
me = ob.data

co = np.array([v.co[:] for v in me.vertices])
lo, hi = co.min(0), co.max(0)
scale = 100.0 / (hi[2] - lo[2])
feet = co[co[:, 2] < lo[2] + (hi[2] - lo[2]) * 0.06]
mid = (feet.min(0) + feet.max(0)) / 2
P = np.c_[(co[:, 0] - mid[0]) * scale, (co[:, 2] - lo[2]) * scale, -(co[:, 1] - mid[1]) * scale]

img = bpy.data.images.load(tex)
W, H = img.size
px = np.array(img.pixels[:], dtype=np.float32).reshape(H, W, img.channels)[:, :, :3]
uvd = me.uv_layers.active.data
col = np.zeros((len(co), 3)); cnt = np.zeros(len(co))
for loop in me.loops:
    u, v = uvd[loop.index].uv
    col[loop.vertex_index] += px[min(H - 1, max(0, int(v * H))), min(W - 1, max(0, int(u * W)))]
    cnt[loop.vertex_index] += 1
col /= np.maximum(cnt, 1)[:, None]
inbox = (P[:, 0] >= x0) & (P[:, 0] <= x1) & (P[:, 1] >= y0) & (P[:, 1] <= y1) & (P[:, 2] >= z0) & (P[:, 2] <= z1)
match = (col.min(1) >= min_light) & ((col.max(1) - col.min(1)) <= max_sat)
pick = inbox & match
print(f"SMOOTH {int(inbox.sum())} in the box, {int(pick.sum())} match the colour; {iters} passes")

bm = bmesh.new()
bm.from_mesh(me)
bm.verts.ensure_lookup_table()
free = [bm.verts[i] for i in np.nonzero(pick)[0]]
before = np.array([v.co[:] for v in free])
for _ in range(iters):
    pos = [sum((e.other_vert(v).co for e in v.link_edges), Vector()) / max(1, len(v.link_edges)) for v in free]
    for v, p in zip(free, pos):
        v.co = v.co.lerp(p, 0.6)
moved = np.linalg.norm(np.array([v.co[:] for v in free]) - before, axis=1) * scale
print(f"SMOOTH moved up to {moved.max():.2f} units (mean {moved.mean():.2f})")
bm.to_mesh(me)
bm.free()
bpy.ops.export_scene.fbx(filepath=out, use_selection=False, object_types={"MESH"}, path_mode="STRIP")
print(f"SMOOTH wrote {out}")
