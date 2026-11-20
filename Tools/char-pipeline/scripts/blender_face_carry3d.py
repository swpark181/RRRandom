"""Carry part of an old model's texture onto a new model of the same character in 3D (run inside Blender), 2026-11-20.

The blank-face tiger's lower face was first carried over by a front projection (blender_feature_transfer.py +
blender_expression.py bake): fine where the face looks at the camera, but the sides of the muzzle and the chin smeared into
streaks and the pasted oval showed an edge. This instead finds, for every texel of the new mesh inside a 3D region, the
old surface right next to it (along the new surface's normal, both meshes in the face frame of blender_face_audit.py: head
middle = 0, body height = 1) and takes the old texture's colour there. Old whisker pieces (small loose parts, and grey
needle faces in front of the muzzle) are left out of the old mesh so they are not painted on.

Usage:
  blender -b --factory-startup --python blender_face_carry3d.py -- <new fbx> <new basecolor> <old fbx> <old basecolor>
        <out png> [--region "x,ylo,yhi,zlo,zhi"] [--feather 0.02] [--reach 0.05] [--front 0.35,0.75] [--smooth 0] [--colour-match 0] [--debug <png>]
  region: |x| < x, y (front = negative) from ylo to yhi, z from zlo to zhi, all in the face frame; feather softens every
  side (fades out over that distance outside the box). front lo,hi: where the surface faces the front more than hi
  (cosine), the old colour is looked up straight from the front, below lo along the normal, mixed between. colour-match 1 scales the old colours so the region's edge band
  matches the new texture's average there.
"""
import math
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

argv = sys.argv[sys.argv.index("--") + 1:]
new_fbx, new_tex, old_fbx, old_tex, out = argv[:5]
opts = dict(zip(argv[5::2], argv[6::2]))
rx, ylo, yhi, zlo, zhi = (float(c) for c in opts.get("--region", "0.30,-0.40,0.04,-0.085,0.035").split(","))
feather = float(opts.get("--feather", 0.02))
reach = float(opts.get("--reach", 0.05))
colour_match = opts.get("--colour-match", "0") == "1"
smooth = int(opts.get("--smooth", 0))
front_lo, front_hi = (float(c) for c in opts.get("--front", "0.35,0.75").split(","))


def load(path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=path)
    meshes = [o for o in bpy.data.objects if o not in before and o.type == "MESH"]
    return max(meshes, key=lambda o: len(o.data.polygons))


def frame(verts):
    # blender_face_audit.py's face frame: the head's middle across and front to back, 0.66 of the height up
    lo, hi = verts[:, 2].min(), verts[:, 2].max()
    h = hi - lo
    head = verts[verts[:, 2] > lo + h * 0.55]
    cx = (head[:, 0].min() + head[:, 0].max()) / 2
    fz = lo + h * 0.66
    face = verts[np.abs(verts[:, 2] - fz) < h * 0.12]
    cy = (face[:, 1].min() + face[:, 1].max()) / 2
    return np.array([cx, cy, fz]), h


def image(path):
    im = bpy.data.images.load(path)
    w, h = im.size
    return np.array(im.pixels[:], dtype=np.float32).reshape(h, w, im.channels)[:, :, :3].copy(), w, h


def tri_data(obj):
    me = obj.data
    me.calc_loop_triangles()
    mw = np.array(obj.matrix_world)
    v = np.array([x.co for x in me.vertices], dtype=np.float64)
    v = (np.c_[v, np.ones(len(v))] @ mw.T)[:, :3]
    vn = np.array([x.normal for x in me.vertices], dtype=np.float64) @ mw[:3, :3].T
    vn /= np.linalg.norm(vn, axis=1, keepdims=True) + 1e-12
    uvl = me.uv_layers.active.data
    tris = me.loop_triangles
    vidx = np.array([t.vertices[:] for t in tris])
    uvs = np.array([[uvl[l].uv[:] for l in t.loops] for t in tris])
    return v, vn, vidx, uvs


bpy.ops.wm.read_factory_settings(use_empty=True)
new_obj = load(new_fbx)
old_obj = load(old_fbx)
nv, nvn, ntri, nuv = tri_data(new_obj)
ov, _, otri, ouv = tri_data(old_obj)
n_centre, n_h = frame(nv)
o_centre, o_h = frame(ov)
if smooth > 0:
    # look up on a smoothed copy of the old surface: its carved creases (the mouth under an overhanging lip, the muzzle
    # pads' edges) close up, so the colour inside them shrinks to a line instead of smearing over the smooth new face
    for m in list(old_obj.modifiers):
        old_obj.modifiers.remove(m)  # (the rig's Armature modifier: keep the rest shape)
    mod = old_obj.modifiers.new("Smooth", "SMOOTH")
    mod.factor = 0.5
    mod.iterations = smooth
    deps = bpy.context.evaluated_depsgraph_get()
    ev = old_obj.evaluated_get(deps).to_mesh()
    sm = np.array([x.co for x in ev.vertices], dtype=np.float64)
    mw = np.array(old_obj.matrix_world)
    ov = (np.c_[sm, np.ones(len(sm))] @ mw.T)[:, :3]
print(f"CARRY new frame {n_centre.round(3)} h {n_h:.3f}, old frame {o_centre.round(3)} h {o_h:.3f}")
nq = (nv - n_centre) / n_h          # new vertices in the face frame
oq = (ov - o_centre) / o_h          # old vertices in the face frame

tex, tw, th = image(new_tex)
otex, ow, oh = image(old_tex)

# old faces to leave out: small loose pieces and grey needle triangles in front of the muzzle (the whiskers)
bm = bmesh.new()
bm.from_mesh(old_obj.data)
bm.verts.ensure_lookup_table()
comp_size = np.zeros(len(bm.verts), int)
seen = np.zeros(len(bm.verts), bool)
for v in bm.verts:
    if seen[v.index]:
        continue
    comp, stack = [], [v]
    seen[v.index] = True
    while stack:
        a = stack.pop()
        comp.append(a.index)
        for e in a.link_edges:
            b = e.other_vert(a)
            if not seen[b.index]:
                seen[b.index] = True
                stack.append(b)
    comp_size[comp] = len(comp)
valence = np.array([len(v.link_faces) for v in bm.verts])
bm.free()
P = oq[otri]
e = np.stack([P[:, 1] - P[:, 0], P[:, 2] - P[:, 1], P[:, 0] - P[:, 2]], 1)
longest = np.linalg.norm(e, axis=2).max(1)
area2 = np.linalg.norm(np.cross(P[:, 1] - P[:, 0], P[:, 2] - P[:, 0]), axis=1)
needle = area2 / np.maximum(longest, 1e-9) < 0.2 * longest
cu = np.clip((ouv.mean(1)[:, 0] * ow).astype(int), 0, ow - 1)
cv = np.clip((ouv.mean(1)[:, 1] * oh).astype(int), 0, oh - 1)
col = otex[cv, cu]
grey = (col.max(1) - col.min(1) < 0.09) & (col.mean(1) < 0.55)
thin = area2 / np.maximum(longest, 1e-9) < 0.3 * longest  # whisker strands: long thin triangles on low-valence vertices
cen = P.mean(1)
in_front = (cen[:, 1] < -0.10) & (np.abs(cen[:, 2]) < 0.08)
# UV islands (triangles joined across an edge whose two ends have the same UVs on both sides): whiskers stuck onto the
# muzzle are their own tiny islands in the atlas even where they share vertices with the face
parent = list(range(len(otri)))


def find(i):
    while parent[i] != i:
        parent[i] = parent[parent[i]]
        i = parent[i]
    return i


edges = {}
for t in range(len(otri)):
    for k in range(3):
        a, b = otri[t, k], otri[t, (k + 1) % 3]
        ua, ub = tuple(np.round(ouv[t, k], 5)), tuple(np.round(ouv[t, (k + 1) % 3], 5))
        key = (a, b) if a < b else (b, a)
        val = (ua, ub) if a < b else (ub, ua)
        other = edges.get(key)
        if other is None:
            edges[key] = (t, val)
        elif other[1] == val:
            ra, rb = find(t), find(other[0])
            if ra != rb:
                parent[ra] = rb
roots = np.array([find(t) for t in range(len(otri))])
_, inv, counts = np.unique(roots, return_inverse=True, return_counts=True)
island = counts[inv]
small_island = island < int(opts.get("--island", 40))
drop = (comp_size[otri].min(1) < 50) | (needle & grey & in_front) | (small_island & in_front) 
# thin triangles that stand off the rest of the surface (a whisker strand, even one glued on at its root)
base_idx = np.nonzero(~(thin & in_front))[0]
base_bvh = BVHTree.FromPolygons([Vector(p) for p in oq], [tuple(t) for t in otri[base_idx]], all_triangles=True)
for t in np.nonzero(thin & in_front)[0]:
    far = max(base_bvh.find_nearest(Vector(oq[v]))[3] or 0.0 for v in otri[t])
    if far > float(opts.get("--standoff", 0.004)):
        drop[t] = True
print(f"CARRY {int((small_island & in_front).sum())} triangles in small UV islands on the muzzle")
keep = np.nonzero(~drop)[0]
print(f"CARRY old mesh: {len(otri)} triangles, {int(drop.sum())} whisker triangles left out")
bvh = BVHTree.FromPolygons([Vector(p) for p in oq], [tuple(t) for t in otri[keep]], all_triangles=True)


def ramp(d):
    # 1 inside (d <= 0), fading to 0 at d = feather
    return np.clip(1.0 - d / feather, 0.0, 1.0)


def weight(q):
    w = ramp(np.abs(q[..., 0]) - rx)
    w *= ramp(ylo - q[..., 1]) * ramp(q[..., 1] - yhi)
    w *= ramp(zlo - q[..., 2]) * ramp(q[..., 2] - zhi)
    return w * w * (3 - 2 * w)


# which new triangles can be touched at all
TQ = nq[ntri]
tw_max = weight(TQ).max(1)
cand = np.nonzero(tw_max > 0)[0]
print(f"CARRY {len(cand)} of {len(ntri)} new triangles in the region")

pos_list, nrm_list, w_list, tex_idx = [], [], [], []
written = np.zeros((th, tw), bool)
for t in cand:
    u = nuv[t, :, 0] * tw
    v = nuv[t, :, 1] * th
    u0, u1 = int(max(0, math.floor(u.min() - 1))), int(min(tw - 1, math.ceil(u.max() + 1)))
    v0, v1 = int(max(0, math.floor(v.min() - 1))), int(min(th - 1, math.ceil(v.max() + 1)))
    if u1 < u0 or v1 < v0:
        continue
    gu, gv = np.meshgrid(np.arange(u0, u1 + 1) + 0.5, np.arange(v0, v1 + 1) + 0.5)
    d = (v[1] - v[2]) * (u[0] - u[2]) + (u[2] - u[1]) * (v[0] - v[2])
    if abs(d) < 1e-12:
        continue
    a = ((v[1] - v[2]) * (gu - u[2]) + (u[2] - u[1]) * (gv - v[2])) / d
    b = ((v[2] - v[0]) * (gu - u[2]) + (u[0] - u[2]) * (gv - v[2])) / d
    c = 1 - a - b
    # a texel-wide margin round the triangle, so the island edges are covered too
    tol = 1.5 / max(1.0, abs(d) ** 0.5)
    inside = (a >= -tol) & (b >= -tol) & (c >= -tol) & ~written[v0:v1 + 1, u0:u1 + 1]
    if not inside.any():
        continue
    bary = np.stack([a[inside], b[inside], c[inside]], 1)
    bary = np.clip(bary, 0, None)
    bary /= bary.sum(1, keepdims=True)
    q = bary @ TQ[t]
    nn = bary @ nvn[ntri[t]]
    w = weight(q)
    if not (w > 0).any():
        continue
    rows, cols = np.nonzero(inside)
    rows += v0
    cols += u0
    written[rows, cols] = True
    sel = w > 0
    pos_list.append(q[sel])
    nrm_list.append(nn[sel])
    w_list.append(w[sel])
    tex_idx.append(np.stack([rows[sel], cols[sel]], 1))
pos = np.concatenate(pos_list)
nrm = np.concatenate(nrm_list)
nrm /= np.linalg.norm(nrm, axis=1, keepdims=True) + 1e-12
wts = np.concatenate(w_list)
idx = np.concatenate(tex_idx)
print(f"CARRY {len(pos)} texels to look up")

# old surface next to each texel: from outside in along the normal, else from inside out, else the nearest point
hit_tri = np.full(len(pos), -1)
offset = np.zeros(len(pos))  # how far the old surface is out from the new one along the look-up (debug)
hit_pos = np.zeros((len(pos), 3))
# the look-up direction: straight from the front where the new surface faces the front (the carved mouth and lips of
# the old mesh read right only from there, as in its front picture), along the normal on the sides (no smearing)
facing = -nrm[:, 1]
s = np.clip((facing - front_lo) / max(1e-6, front_hi - front_lo), 0, 1)[:, None]
look = nrm * (1 - s) + np.array([0.0, -1.0, 0.0]) * s
look /= np.linalg.norm(look, axis=1, keepdims=True) + 1e-12
for i in range(len(pos)):
    p = Vector(pos[i])
    n = Vector(look[i])
    loc, _, fi, _ = bvh.ray_cast(p + n * reach, -n, reach * 2)
    if loc is None:
        loc, _, fi, _ = bvh.ray_cast(p - n * reach, n, reach * 2)
    if loc is None:
        loc, _, fi, dist = bvh.find_nearest(p, reach)
    if loc is not None:
        hit_tri[i] = keep[fi]
        hit_pos[i] = loc
        offset[i] = (Vector(loc) - p).dot(n)
ok = hit_tri >= 0
print(f"CARRY found the old surface for {ok.mean() * 100:.1f}% of them")

# old UV at each hit (barycentric in its triangle), bilinear sample of the old texture
T = oq[otri[hit_tri[ok]]]
v0_, v1_ = T[:, 1] - T[:, 0], T[:, 2] - T[:, 0]
vp = hit_pos[ok] - T[:, 0]
d00 = (v0_ * v0_).sum(1)
d01 = (v0_ * v1_).sum(1)
d11 = (v1_ * v1_).sum(1)
d20 = (vp * v0_).sum(1)
d21 = (vp * v1_).sum(1)
den = np.where(np.abs(d00 * d11 - d01 * d01) < 1e-18, 1e-18, d00 * d11 - d01 * d01)
bb = (d11 * d20 - d01 * d21) / den
cc = (d00 * d21 - d01 * d20) / den
aa = 1 - bb - cc
UV = ouv[hit_tri[ok]]
uv = aa[:, None] * UV[:, 0] + bb[:, None] * UV[:, 1] + cc[:, None] * UV[:, 2]
fx = np.clip(uv[:, 0] * ow - 0.5, 0, ow - 1.001)
fy = np.clip(uv[:, 1] * oh - 0.5, 0, oh - 1.001)
x0 = fx.astype(int)
y0 = fy.astype(int)
ax = (fx - x0)[:, None]
ay = (fy - y0)[:, None]
old_col = (otex[y0, x0] * (1 - ax) * (1 - ay) + otex[y0, x0 + 1] * ax * (1 - ay)
           + otex[y0 + 1, x0] * (1 - ax) * ay + otex[y0 + 1, x0 + 1] * ax * ay)
rr, cc_ = idx[ok, 0], idx[ok, 1]
w_ok = wts[ok][:, None]
new_col = tex[rr, cc_]

if colour_match:
    # the edge band (weight 0.15..0.6): scale the old colours per channel so they meet the new texture there
    band = (w_ok[:, 0] > 0.15) & (w_ok[:, 0] < 0.6)
    if band.sum() > 500:
        gain = np.clip(new_col[band].mean(0) / np.maximum(old_col[band].mean(0), 1e-3), 0.85, 1.15)
        print(f"CARRY colour gain {gain.round(3)}")
        old_col = np.clip(old_col * gain, 0, 1)
tex[rr, cc_] = new_col * (1 - w_ok) + old_col * w_ok
print(f"CARRY painted {int((w_ok[:, 0] > 0.5).sum())} texels fully, {len(rr)} in all")

img = bpy.data.images.new("out", tw, th)
img.pixels = np.concatenate([tex, np.ones((th, tw, 1), np.float32)], 2).ravel().tolist()
img.filepath_raw = out
img.file_format = "PNG"
img.save()
if "--debug" in opts:
    dbg = np.zeros((th, tw, 3), np.float32)
    dbg[rr, cc_, 0] = w_ok[:, 0]
    dbg[rr, cc_, 1] = np.clip(offset[ok] / reach, 0, 1)  # green = old surface out in front
    dbg[rr, cc_, 2] = np.clip(-offset[ok] / reach, 0, 1)  # blue = behind
    di = bpy.data.images.new("dbg", tw, th)
    di.pixels = np.concatenate([dbg, np.ones((th, tw, 1), np.float32)], 2).ravel().tolist()
    di.filepath_raw = opts["--debug"]
    di.file_format = "PNG"
    di.save()
print(f"CARRY wrote {out}")
