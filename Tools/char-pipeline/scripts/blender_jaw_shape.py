"""Give a new head the lower face of an old head of the same character: its real SHAPE, wrapped on (run inside Blender),
2026-11-20.

The blank-face tiger's lower face was one wide round ball; the old tiger had a small mouth over a narrow chin with the
cheek fur flaring out beside it. Moving only the relief (blender_face_relief_transfer.py) kept the ball, and the user
said the mouth and the white round it looked nothing like the old one. So inside a 3D box of the face frame
(blender_face_audit.py: head middle = 0, body height = 1) the new mesh, cut finer, is wrapped onto the old surface:
each round every vertex moves part of the way to the nearest old point and the moves are averaged with the neighbours'
(so the mesh slides on without tearing: plain snapping along rays tore it), then a few tight rounds. The result is
mixed with the untouched shape over a wide soft edge. The old head is first slid front-to-back so the two faces agree
just above the box; its whiskers are left out. UVs and bone weights are kept (the cut interpolates them); shading
normals are the new shape's own in the box, the file's outside, mixed at the edge.

Usage:
  blender -b --factory-startup --python blender_jaw_shape.py -- <new rig fbx> <old rig fbx> <out fbx>
        [--region "x,ylo,yhi,zlo,zhi"] [--feather 0.05] [--cuts 2] [--rounds 40] [--tight 6] [--reach 0.2]
        [--offset auto|<y>] [--feather-low <feather>]
"""
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

argv = sys.argv[sys.argv.index("--") + 1:]
new_fbx, old_fbx, out = argv[:3]
opts = dict(zip(argv[3::2], argv[4::2]))
rx, ylo, yhi, zlo, zhi = (float(c) for c in opts.get("--region", "0.17,-0.40,0.02,-0.10,0.03").split(","))
feather = float(opts.get("--feather", 0.05))
cuts = int(opts.get("--cuts", 2))
rounds = int(opts.get("--rounds", 40))
tight = int(opts.get("--tight", 6))
reach = float(opts.get("--reach", 0.2))
feather_low = float(opts.get("--feather-low", feather))  # the soft edge below (short, so a shirt collar under the chin stays put)


def frame(verts):
    lo, hi = verts[:, 2].min(), verts[:, 2].max()
    h = hi - lo
    head = verts[verts[:, 2] > lo + h * 0.55]
    cx = (head[:, 0].min() + head[:, 0].max()) / 2
    fz = lo + h * 0.66
    face = verts[np.abs(verts[:, 2] - fz) < h * 0.12]
    cy = (face[:, 1].min() + face[:, 1].max()) / 2
    return np.array([cx, cy, fz]), h


def ramp(d, f=None):
    return np.clip(1.0 - d / (f or feather), 0.0, 1.0)


def weight(q):
    w = ramp(np.abs(q[..., 0]) - rx)
    w = w * ramp(ylo - q[..., 1]) * ramp(q[..., 1] - yhi)
    w = w * ramp(zlo - q[..., 2], feather_low) * ramp(q[..., 2] - zhi)
    return w * w * (3 - 2 * w)


# ---- the old head: whiskers out, in the face frame
bpy.ops.wm.read_factory_settings(use_empty=True)
before = set(bpy.data.objects)
bpy.ops.import_scene.fbx(filepath=old_fbx)
old = max([o for o in bpy.data.objects if o not in before and o.type == "MESH"], key=lambda o: len(o.data.polygons))
bm = bmesh.new()
bm.from_mesh(old.data)
bm.transform(old.matrix_world)
bm.verts.ensure_lookup_table()
ov = np.array([v.co for v in bm.verts])
o_centre, o_h = frame(ov)
comp_size = {}
seen = set()
for v in bm.verts:
    if v in seen:
        continue
    comp, stack = [], [v]
    seen.add(v)
    while stack:
        a = stack.pop()
        comp.append(a)
        for e in a.link_edges:
            b = e.other_vert(a)
            if b not in seen:
                seen.add(b)
                stack.append(b)
    for a in comp:
        comp_size[a] = len(comp)
q_old = lambda co: (np.array(co) - o_centre) / o_h
drop, thin_front = set(), []
for f in bm.faces:
    if min(comp_size[v] for v in f.verts) < 50:
        drop.add(f)
        continue
    c = q_old(f.calc_center_median())
    if len(f.verts) == 3 and c[1] < -0.10 and abs(c[2]) < 0.08:
        a, b, cc = (v.co for v in f.verts)
        longest = max((a - b).length, (b - cc).length, (cc - a).length)
        if longest > 0 and 2 * f.calc_area() / longest < 0.3 * longest:
            thin_front.append(f)
thin_set = set(thin_front)
base = [f for f in bm.faces if f not in drop and f not in thin_set]
base_bvh = BVHTree.FromPolygons([v.co for v in bm.verts], [[v.index for v in f.verts] for f in base])
for f in thin_front:
    if max(base_bvh.find_nearest(v.co)[3] or 0.0 for v in f.verts) > 0.004 * o_h:
        drop.add(f)
bmesh.ops.delete(bm, geom=list(drop), context="FACES")
bmesh.ops.triangulate(bm, faces=list(bm.faces))
bm.verts.ensure_lookup_table()
bm.verts.index_update()
OP = np.array([q_old(v.co) for v in bm.verts])
otris = [[v.index for v in f.verts] for f in bm.faces]
bm.free()
print(f"JAW old head: {len(drop)} whisker faces left out")
for o in [o for o in bpy.data.objects if o not in before]:
    bpy.data.objects.remove(o)

# ---- the new rig
bpy.ops.import_scene.fbx(filepath=new_fbx)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
arm.animation_data_clear()
for action in list(bpy.data.actions):
    bpy.data.actions.remove(action)
arm.data.pose_position = "REST"
mesh = max([o for o in bpy.data.objects if o.type == "MESH"], key=lambda o: len(o.data.polygons))
me = mesh.data
mw = mesh.matrix_world.copy()
inv = mw.inverted()
n_centre, n_h = frame(np.array([mw @ v.co for v in me.vertices]))

# slide the old head front-to-back so the two faces agree just above the box (under the eyes, the nose bridge)
bm = bmesh.new()
bm.from_mesh(me)
bm.transform(mw)
bmesh.ops.triangulate(bm, faces=list(bm.faces))
new_bvh0 = BVHTree.FromPolygons([Vector((np.array(v.co) - n_centre) / n_h) for v in bm.verts], [[v.index for v in f.verts] for f in bm.faces])
bm.free()
old_bvh = BVHTree.FromPolygons([Vector(p) for p in OP], otris)
if opts.get("--offset", "auto") == "auto":
    diffs = []
    for x in np.linspace(-rx, rx, 25):
        for z in np.linspace(zhi + 0.01, zhi + 0.04, 6):
            a = new_bvh0.ray_cast(Vector((x, -2, z)), Vector((0, 1, 0)))[0]
            b = old_bvh.ray_cast(Vector((x, -2, z)), Vector((0, 1, 0)))[0]
            if a is not None and b is not None:
                diffs.append(a.y - b.y)
    offset = float(np.median(diffs)) if diffs else 0.0
else:
    offset = float(opts["--offset"])
print(f"JAW old head slid {offset:+.4f} front-to-back (body heights) to meet the new face above the box")
OP[:, 1] += offset
old_bvh = BVHTree.FromPolygons([Vector(p) for p in OP], otris)

orig_n = np.zeros((len(me.vertices), 3))
for lp, cnr in zip(me.loops, me.corner_normals):
    orig_n[lp.vertex_index] += np.array(cnr.vector)
bm = bmesh.new()
bm.from_mesh(me)
nlay = bm.verts.layers.float_vector.new("n0")
for v in bm.verts:
    v[nlay] = Vector(orig_n[v.index])


def vq(v):
    return (np.array(mw @ v.co) - n_centre) / n_h


n_before = len(bm.verts)
if cuts > 0:
    faces = [f for f in bm.faces if max(weight(vq(v)) for v in f.verts) > 0]
    edges = list({e for f in faces for e in f.edges})
    bmesh.ops.subdivide_edges(bm, edges=edges, cuts=cuts, use_grid_fill=True)
    bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 3])
bm.verts.ensure_lookup_table()
bm.verts.index_update()
verts = list(bm.verts)
P0 = np.array([vq(v) for v in verts])
W = weight(P0)
act = np.nonzero(W > 0)[0]
is_act = W > 0
nbr = [[e.other_vert(v).index for e in v.link_edges] for v in verts]
nbr_act = {i: [n for n in nbr[i] if is_act[n]] for i in act}
print(f"JAW new mesh {n_before} -> {len(verts)} vertices, {len(act)} in the box")

U = np.zeros_like(P0)


def project(step, smooth_passes, lam):
    global U
    for i in act:
        p = P0[i] + U[i]
        loc = old_bvh.find_nearest(Vector(p), reach)[0]
        if loc is not None:
            U[i] += step * (np.array(loc) - p)
    for _ in range(smooth_passes):
        avg = U.copy()
        for i in act:
            n = nbr_act[i]
            if n:
                avg[i] = U[i] * (1 - lam) + U[n].mean(0) * lam
        U = avg


for k in range(rounds):
    project(0.6, 2, 0.5)
for k in range(tight):
    project(0.8, 1, 0.15)
gap = []
for i in act:
    p = P0[i] + U[i]
    d = old_bvh.find_nearest(Vector(p), reach)[3]
    if d is not None and W[i] > 0.99:
        gap.append(d)
print(f"JAW wrapped: moved up to {np.linalg.norm(U, axis=1).max():.4f}; left off the old surface {np.mean(gap):.5f} on average, "
      f"{np.max(gap):.5f} at most (body heights)")
for i in act:
    verts[i].co = inv @ Vector((P0[i] + U[i] * W[i]) * n_h + n_centre)
bm.normal_update()
new_n = {}
for i, v in enumerate(verts):
    n_file = Vector(v[nlay])
    if n_file.length < 1e-6:
        n_file = v.normal.copy()
    n_file.normalize()
    new_n[v.index] = (n_file * (1 - W[i]) + v.normal * W[i]).normalized() if W[i] > 0 else n_file
bm.verts.layers.float_vector.remove(nlay)
bm.to_mesh(me)
bm.free()
me.update()
me.normals_split_custom_set([tuple(new_n[lp.vertex_index]) for lp in me.loops])
me.update()
loose = sum(1 for v in me.vertices if not v.groups)
print(f"JAW {len(me.vertices)} vertices, {loose} without bone weights; offset {offset:+.5f}")
bpy.ops.export_scene.fbx(
    filepath=out, use_selection=False, object_types={"ARMATURE", "MESH"}, add_leaf_bones=False, bake_anim=False,
    primary_bone_axis="Y", secondary_bone_axis="X", use_armature_deform_only=False, mesh_smooth_type="OFF",
    path_mode="STRIP")
print(f"JAW wrote {out}")
