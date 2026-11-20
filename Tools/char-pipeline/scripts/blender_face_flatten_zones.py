"""Smooth the dents and creases out of chosen face zones of a Tripo mesh (run inside Blender), depth only.

2026-11-20, the tiger's blank-face T-pose Smart Mesh: Tripo turned the faint closed-eye shading of the input pictures
into creases (a groove across each eye place, a brow bulge above it) and a smile groove round the muzzle. Expression
textures need those places smooth. Here each front-facing vertex inside soft ellipses (drawn on the front render of
blender_face_audit.py, whose <prefix>_cam.json gives the camera) keeps its X and Z (so the face outline in front view is
unchanged) and only its depth (Y) is relaxed toward its neighbours' (Taubin lambda/mu steps, so the face does not shrink
flat), weighted by the ellipse's soft edge. "keep" ellipses (the nose) are held still. UVs and texture stay.

Usage: blender -b --factory-startup --python blender_face_flatten_zones.py -- <fbx> <cam json> <out fbx>
        --zones "cx,cy,rx,ry;..." [--keep "cx,cy,rx,ry;..."] [--feather 0.35] [--iters 80] [--px 1024]
  Ellipses are in pixels of the audit's front render (px wide; default 1024).
"""
import json
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

argv = sys.argv[sys.argv.index("--") + 1:]
src, cam_json, out = argv[:3]
opts = dict(zip(argv[3::2], argv[4::2]))


def ellipses(text):
    return [tuple(float(v) for v in e.split(",")) for e in text.split(";") if e.strip()] if text else []


zones = ellipses(opts.get("--zones", ""))
keeps = ellipses(opts.get("--keep", ""))
feather = float(opts.get("--feather", 0.35))
iters = int(opts.get("--iters", 80))
px = float(opts.get("--px", 1024))
cam = json.load(open(cam_json))

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
meshes = [o for o in bpy.data.objects if o.type == "MESH"]
bpy.ops.object.select_all(action="DESELECT")
for o in meshes:
    o.select_set(True)
bpy.context.view_layer.objects.active = meshes[0]
if len(meshes) > 1:
    bpy.ops.object.join()
obj = bpy.context.view_layer.objects.active
# Work in the world frame the audit measured in; the object's own transform is put back before export
mw = obj.matrix_world.copy()
me = obj.data
co = np.array([mw @ v.co for v in me.vertices])
bm = bmesh.new()
bm.from_mesh(me)
bm.verts.ensure_lookup_table()
world_bm = bm.copy()
for v, c in zip(world_bm.verts, co):
    v.co = Vector(c)
bvh = BVHTree.FromBMesh(world_bm)

ortho = cam["ortho"]
cx0, cz0 = cam["center"][0], cam["center"][2]
# image pixel of each vertex in the front view (camera on -Y looking +Y, image right = +X, image up = +Z)
ix = ((co[:, 0] - cx0) / ortho + 0.5) * px
iy = (0.5 - (co[:, 2] - cz0) / ortho) * px


def weight(list_, soft):
    w = np.zeros(len(co))
    for (ex, ey, rx, ry) in list_:
        d = np.sqrt(((ix - ex) / rx) ** 2 + ((iy - ey) / ry) ** 2)
        w = np.maximum(w, np.clip((1.0 - d) / max(soft, 1e-3), 0.0, 1.0))
    return w


w = weight(zones, feather)
hold = weight(keeps, 0.25)
w = w * (1.0 - hold)
# only what the front camera sees: the first hit along +Y from in front of the vertex is the vertex itself
y_front = co[:, 1].min() - 1.0
visible = np.zeros(len(co), bool)
for i in np.nonzero(w > 0)[0]:
    hit = bvh.ray_cast(Vector((co[i, 0], y_front, co[i, 2])), Vector((0, 1, 0)))
    visible[i] = hit[0] is not None and abs(hit[0].y - co[i, 1]) < 0.004
w = w * visible
print(f"FLATTEN {np.count_nonzero(w > 0)} vertices weighted, {np.count_nonzero(w > 0.99)} fully")

nbrs = [[e.other_vert(v).index for e in v.link_edges] for v in bm.verts]
y = co[:, 1].copy()
y0 = y.copy()
# --fill: the eye places Tripo dished in are filled up to a smooth quadric fitted to the face around them (in the face
# window --window, outside every zone and keep ellipse), before the relaxing
fills = ellipses(opts.get("--fill", ""))
if fills:
    win = weight(ellipses(opts.get("--window", "512,400,420,340")), 0.01) > 0
    wf = weight(fills, feather)
    seen = np.zeros(len(co), bool)
    for i in np.nonzero(win)[0]:
        hit = bvh.ray_cast(Vector((co[i, 0], y_front, co[i, 2])), Vector((0, 1, 0)))
        seen[i] = hit[0] is not None and abs(hit[0].y - co[i, 1]) < 0.004
    ring = win & seen & (weight(fills + zones + keeps, 0.01) <= 0)
    X, Z = co[ring, 0], co[ring, 2]
    A = np.c_[np.ones_like(X), X, Z, X * X, X * Z, Z * Z]
    coef, *_ = np.linalg.lstsq(A, co[ring, 1], rcond=None)
    tgt = np.c_[np.ones(len(co)), co[:, 0], co[:, 2], co[:, 0] ** 2, co[:, 0] * co[:, 2], co[:, 2] ** 2] @ coef
    fsel = (wf > 0) & seen
    # only raise (toward the camera = smaller y): a dish comes up, a bump in the zone is left to the relaxing
    lift = np.minimum(tgt - y, 0.0)
    y[fsel] = y[fsel] + wf[fsel] * lift[fsel]
    print(f"FLATTEN fill: quadric from {ring.sum()} ring vertices, {fsel.sum()} filled, max lift {-lift[fsel].min() * 100:.2f}")
    w = np.maximum(w, wf * fsel * 0.6)
active = np.nonzero(w > 0)[0]
for it in range(iters):
    for lam in (0.6, -0.62):
        avg = y.copy()
        for i in active:
            n = nbrs[i]
            if n:
                avg[i] = y[n].mean()
        y[active] = y[active] + lam * w[active] * (avg[active] - y[active])
moved = np.abs(y - y0)
print(f"FLATTEN moved max {moved.max() * 100:.2f} cm-units of a 100 tall body (x{1 / (co[:, 2].max() - co[:, 2].min()):.2f}), mean over weighted {moved[active].mean() * 100:.3f}")
co[:, 1] = y
inv = mw.inverted()
for v, c in zip(me.vertices, co):
    v.co = inv @ Vector(c)
me.update()
bpy.ops.export_scene.fbx(filepath=out, use_selection=False, object_types={"MESH"}, path_mode="COPY", embed_textures=True)
print("FLATTEN wrote", out)
