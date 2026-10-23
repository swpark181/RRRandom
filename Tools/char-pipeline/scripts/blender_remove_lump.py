"""Cut a stray one-sided lump off a Tripo mesh and close the hole (run in Blender): the rabbit A-3 Free Retry had a
second cotton tail hanging under her right hand beside the shorts (the concept's front view showed the real tail
peeking out at her side, and Tripo built it there as well).

The character is nearly mirror-symmetric, so the lump is what has nothing on the other side: every vertex in the box
that is farther than <gap> from the mirrored other side goes. Pouches, shorts and hands have a twin and stay.
The hole is closed with a cap that is filled with points and relaxed (rim held) into a smooth skin. The big cap gets its
own spot in the unused part of the texture atlas, painted texel by texel from the mirrored twin's surface, so it shows
the same denim / pouch as the other side; tiny caps take one flat colour each.
Tried and dropped: a flat cap (faceted, a dent at the cuff), cap points snapped onto the twin (spiky), one colour per
cap triangle (blotchy patches of pouch brown on the denim).

Usage: blender -b --factory-startup --python blender_remove_lump.py -- <tripo fbx> <base color> <out fbx>
       <out base color png> "<x0,x1,y0,y1,z0,z1>" [gap 1.2]
  The box is in blender_tripo_to_obj.py's OBJ frame (100 tall, feet at 0 and centred, Y up, facing +Z, +X = its left),
  so it can be read off blender_obj_grid.py plots; the lump must lie on one side (x0..x1 both < 0 or both > 0).
  Leftover pieces under 50 vertices go too (Tripo's 3-vertex crumbs make Mixamo fail). The FBX comes out 1 m tall like
  Tripo's, Z up, facing -Y (texture paths stripped; use the new base color with it).
"""
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree
from mathutils.geometry import barycentric_transform
from mathutils.kdtree import KDTree

args = sys.argv[sys.argv.index("--") + 1:]
src, tex_in, out, tex_out, box = args[:5]
gap = float(args[5]) if len(args) > 5 else 1.2
x0, x1, y0, y1, z0, z1 = (float(c) for c in box.split(","))
side = -1.0 if x1 <= 0 else 1.0

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

# The OBJ frame of blender_tripo_to_obj.py
co = np.array([v.co[:] for v in me.vertices])
lo, hi = co.min(0), co.max(0)
scale = 100.0 / (hi[2] - lo[2])
feet = co[co[:, 2] < lo[2] + (hi[2] - lo[2]) * 0.06]
mid = (feet.min(0) + feet.max(0)) / 2


def to_obj(p):
    return np.c_[(p[:, 0] - mid[0]) * scale, (p[:, 2] - lo[2]) * scale, -(p[:, 1] - mid[1]) * scale]


P = to_obj(co)
twin = np.nonzero(P[:, 0] * side < 0)[0]          # vertices on the other side, mirrored
kd = KDTree(len(twin))
for i in twin:
    kd.insert((-P[i, 0], P[i, 1], P[i, 2]), int(i))
kd.balance()
inbox = (P[:, 0] >= x0) & (P[:, 0] <= x1) & (P[:, 1] >= y0) & (P[:, 1] <= y1) & (P[:, 2] >= z0) & (P[:, 2] <= z1)
far = np.array([kd.find(P[i])[2] > gap if inbox[i] else False for i in range(len(P))])
print(f"LUMP {int(inbox.sum())} in the box, {int(far.sum())} have no mirror twin within {gap} and go")
uvd = me.uv_layers.active.data
img = bpy.data.images.load(tex_in)
W, H = img.size
px = np.array(img.pixels[:], dtype=np.float32).reshape(H, W, img.channels)

# Where the lump was fused on, its last ring sits close to the twin surface but still has the lump's near-white fur
# (white slivers at the cuff): grow the cut two rings through near-white vertices
col = np.zeros((len(co), 3)); cnt = np.zeros(len(co))
for loop in me.loops:
    u, v = uvd[loop.index].uv
    col[loop.vertex_index] += px[min(H - 1, max(0, int(v * H))), min(W - 1, max(0, int(u * W))), :3]
    cnt[loop.vertex_index] += 1
col /= np.maximum(cnt, 1)[:, None]
whiteish = (col.min(1) > 0.7) & (col.max(1) - col.min(1) < 0.15)
nbr = [[] for _ in range(len(co))]
for e in me.edges:
    a, b = e.vertices
    nbr[a].append(b); nbr[b].append(a)
for _ in range(2):
    ring = [j for i in np.nonzero(far)[0] for j in nbr[i] if not far[j] and inbox[j] and whiteish[j]]
    far[ring] = True
print(f"LUMP {int(far.sum())} go after growing through the lump's white fur")

# The original mesh, mirrored, with its UVs: where the cap's colours come from
polys = [list(p.vertices)[:3] for p in me.polygons]
poly_uv = [[Vector((*uvd[i].uv, 0)) for i in list(p.loop_indices)[:3]] for p in me.polygons]
mco = co.copy(); mco[:, 0] = 2 * mid[0] - mco[:, 0]
bvh = BVHTree.FromPolygons(mco.tolist(), polys)
uv_of = {}
for loop in me.loops:
    uv_of.setdefault(loop.vertex_index, tuple(uvd[loop.index].uv))

# Unused room in the atlas: the biggest empty square at 1/8 of the texture's resolution
N = W // 8
cov = np.zeros((N, N), bool)
for p in me.polygons:
    t = np.array([uvd[i].uv[:] for i in p.loop_indices]) * N
    a = np.floor(t.min(0)).astype(int) - 1; b = np.ceil(t.max(0)).astype(int) + 1
    cov[max(a[1], 0):min(b[1], N), max(a[0], 0):min(b[0], N)] = True
d = np.zeros((N, N), int); room = (0, 0, 0)
for y in range(N):
    for x in range(N):
        if not cov[y, x]:
            d[y, x] = 1 + (min(d[y - 1, x], d[y, x - 1], d[y - 1, x - 1]) if x and y else 0)
            if d[y, x] > room[0]:
                room = (d[y, x], x, y)
size = room[0] * 8 - 8                             # texels, with a 4-texel margin all round
ox, oy = (room[1] - room[0] + 1) * 8 + 4, (room[2] - room[0] + 1) * 8 + 4
print(f"LUMP free square of {size} texels at {ox},{oy}")

bm = bmesh.new()
bm.from_mesh(me)
bm.verts.ensure_lookup_table()
bmesh.ops.delete(bm, geom=[bm.verts[i] for i in np.nonzero(far)[0]], context="VERTS")

seen = set(); pieces = []
for v in bm.verts:
    if v in seen:
        continue
    stack = [v]; seen.add(v); part = []
    while stack:
        a = stack.pop(); part.append(a)
        for e in a.link_edges:
            b = e.other_vert(a)
            if b not in seen:
                seen.add(b); stack.append(b)
    pieces.append(part)
small = [v for p in pieces if len(p) < 50 for v in p]
print(f"LUMP pieces {sorted(len(p) for p in pieces)[-4:]}, dropping {len(small)} verts in small ones")
bmesh.ops.delete(bm, geom=small, context="VERTS")

edges = [e for e in bm.edges if e.is_boundary]
left = set(edges)
while left:
    stack = [left.pop()]; loop = list(stack)
    while stack:
        e = stack.pop()
        for v in e.verts:
            for e2 in v.link_edges:
                if e2 in left:
                    left.discard(e2); stack.append(e2); loop.append(e2)
    q = to_obj(np.array([v.co[:] for e in loop for v in e.verts]))
    print(f"LUMP hole of {len(loop)} edges, x {q[:, 0].min():.1f}..{q[:, 0].max():.1f} y {q[:, 1].min():.1f}..{q[:, 1].max():.1f}"
          f" z {q[:, 2].min():.1f}..{q[:, 2].max():.1f}")

old = set(bm.faces)
rim = {v for e in edges for v in e.verts}
bmesh.ops.holes_fill(bm, edges=edges, sides=0)
bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if f not in old])
for _ in range(3):
    bmesh.ops.poke(bm, faces=[f for f in bm.faces if f not in old])
    cap = [f for f in bm.faces if f not in old]
    inner = [e for e in {e for f in cap for e in f.edges} if all(g not in old for g in e.link_faces)]
    bmesh.ops.beautify_fill(bm, faces=cap, edges=inner)
free = list({v for f in bm.faces if f not in old for v in f.verts} - rim)
for _ in range(60):
    pos = [sum((e.other_vert(v).co for e in v.link_edges), Vector()) / len(v.link_edges) for v in free]
    for v, p in zip(free, pos):
        v.co = p
print(f"LUMP cap: {len(free)} new points relaxed")

new = [f for f in bm.faces if f not in old]
groups = []; todo = set(new)
while todo:
    f0 = todo.pop(); g = [f0]; stack = [f0]
    while stack:
        f = stack.pop()
        for e in f.edges:
            for f2 in e.link_faces:
                if f2 in todo:
                    todo.discard(f2); g.append(f2); stack.append(f2)
    groups.append(g)
big = max(groups, key=len)
uvl = bm.loops.layers.uv.active
for g in groups:
    if g is big:
        continue
    for f in g:
        c = to_obj(np.array([f.calc_center_median()[:]]))[0]
        j = kd.find((c[0], c[1], c[2]))[1]
        for l in f.loops:
            l[uvl].uv = uv_of[j]

# Big cap: flatten onto its best-fit plane into the free square, then paint each texel from the mirrored twin
cv = list({v for f in big for v in f.verts})
C = np.array([v.co[:] for v in cv]); c0 = C.mean(0)
axes = np.linalg.svd(C - c0)[2][:2]
flat = (C - c0) @ axes.T
fmin = flat.min(0); s = size / (flat.max(0) - fmin).max()
tex_uv = {v: ((flat[k] - fmin) * s + (ox, oy)) for k, v in enumerate(cv)}
for f in big:
    for l in f.loops:
        l[uvl].uv = tex_uv[l.vert] / (W, H)
outpx = px.copy()
painted = np.zeros((H, W), bool)
for f in big:
    T = [tex_uv[v] for v in f.verts]; Q = [v.co.copy() for v in f.verts]
    a = np.floor(np.min(T, 0)).astype(int); b = np.ceil(np.max(T, 0)).astype(int)
    t0, t1, t2 = (Vector((t[0], t[1], 0)) for t in T)
    for ty in range(a[1], b[1] + 1):
        for tx in range(a[0], b[0] + 1):
            pt = Vector((tx + 0.5, ty + 0.5, 0))
            # barycentric inside test in texel space
            v0, v1, v2 = t1 - t0, t2 - t0, pt - t0
            den = v0.x * v1.y - v1.x * v0.y
            if abs(den) < 1e-9:
                continue
            u = (v2.x * v1.y - v1.x * v2.y) / den; w = (v0.x * v2.y - v2.x * v0.y) / den
            if u < -0.05 or w < -0.05 or u + w > 1.05:
                continue
            p3 = Q[0] + (Q[1] - Q[0]) * u + (Q[2] - Q[0]) * w
            loc, _, k, _ = bvh.find_nearest(p3)
            if loc is None:
                continue
            tri = polys[k]
            src_uv = barycentric_transform(loc, *(Vector(mco[i]) for i in tri), *poly_uv[k])
            sx = min(W - 1, max(0, int(src_uv.x * W))); sy = min(H - 1, max(0, int(src_uv.y * H)))
            if 0 <= tx < W and 0 <= ty < H:
                outpx[ty, tx] = px[sy, sx]; painted[ty, tx] = True
# bleed the painted texels 4 out so filtering never reaches the old atlas background
for _ in range(4):
    grow = painted.copy()
    for dy, dx in ((0, 1), (0, -1), (1, 0), (-1, 0)):
        sh = np.roll(np.roll(painted, dy, 0), dx, 1) & ~grow
        src_px = np.roll(np.roll(outpx, dy, 0), dx, 1)
        outpx[sh] = src_px[sh]; grow |= sh
    painted = grow
print(f"LUMP painted {int(painted.sum())} texels for the cap ({len(big)} faces, {s:.1f} texels per unit)")
out_img = bpy.data.images.new("basecolor_fix", W, H)
if img.channels == 3:
    outpx = np.concatenate([outpx, np.ones((H, W, 1), np.float32)], 2)
out_img.pixels = outpx.ravel()
out_img.filepath_raw = tex_out; out_img.file_format = "PNG"; out_img.save()

print(f"LUMP filled with {len(new)} faces; boundary edges left {sum(1 for e in bm.edges if e.is_boundary)}")
bm.to_mesh(me)
bm.free()
for p in me.polygons:
    p.use_smooth = True
print(f"LUMP now {len(me.vertices)} verts, {len(me.polygons)} faces")
bpy.ops.export_scene.fbx(filepath=out, use_selection=False, object_types={"MESH"}, path_mode="STRIP")
print(f"LUMP wrote {out}, {tex_out}")
