"""Some of one head's face relief given to another head, inside one window of the face (run in Blender; 2026-11-20:
the blank-face fox is one flat mask between the eyes, the old fox had a nose bridge with the eyes set in beside it;
user: "눈과 눈 사이에 기존에 있던 굴곡을 조금 살려야 더 자연스러울 것 같다").

Both heads are measured from straight ahead (the depth of the front-most surface on a grid). Inside the window the
difference old - new, minus what a smooth sheet through the window's edge would give (so only the shape INSIDE counts and
the edge does not move), times --amount, pushes the new head's front vertices forward or back. The mesh there is cut
finer first (a blank face is a few big faces), keeping UVs and bone weights. The rig, the UVs and the texture stay.

  blender -b --factory-startup --python blender_face_relief_transfer.py -- <new rig fbx> <old rig fbx> <out fbx>
        --new x,z --old x,z [--scale 1.0] --window cx,cz,rx,rz [--amount 0.5] [--cuts 2] [--feather 0.012] [--max 0.02] [--blur 40] [--smooth 0] [--clear-normals] [--fill cells,keep,deepest]
  --new / --old  the same face point (between the eyes) on each head: x and z in that file's world units
  --scale        old size -> new size (the two front renders' ortho ratio)
  --window       ellipse in the NEW head's world x, z (centre and radii); --feather its soft edge, world units
  --max          the furthest a vertex may move (world units)
  --bridge A,s1,V,x0,s2,zlo,zhi,zfade   no measuring (<old rig fbx> is not read, pass "none"; --old is not needed): a ridge
                 A high and s1 wide on the line x = --new's x, a valley V deep and s2 wide at x0 to each side, full
                 between world z zlo and zhi, fading over zfade. The measured difference of two Tripo heads came out as
                 wrinkles (fur ripples, eyeballs at the window's edge); this is what the fox got.
  --blur         how much the measured difference is smoothed (passes of a 3x3 blur on a 1.5 mm grid; 40 = about 7 mm):
                 low values copy the old head's fur ripples and facets as wrinkles
  --smooth       subdivision levels both heads are rounded by before measuring (2: takes the flat faces of Tripo heads out)
Prints the relief it moved (min, max) and the vertex count before and after.
"""
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

argv = sys.argv[sys.argv.index("--") + 1:]
opt = {"scale": "1.0", "amount": "0.5", "cuts": "2", "feather": "0.012", "max": "0.02", "blur": "40"}
rest = []
i = 0
while i < len(argv):
    if argv[i] == "--clear-normals":
        i += 1
    elif argv[i].startswith("--"):
        opt[argv[i][2:]] = argv[i + 1]
        i += 2
    else:
        rest.append(argv[i])
        i += 1
new_fbx, old_fbx, out_fbx = rest[:3]
nx, nz = [float(v) for v in opt["new"].split(",")]
ox, oz = [float(v) for v in opt.get("old", "0,0").split(",")]
scale, amount, cuts = float(opt["scale"]), float(opt["amount"]), int(opt["cuts"])
feather, max_move = float(opt["feather"]), float(opt["max"])
wcx, wcz, wrx, wrz = [float(v) for v in opt["window"].split(",")]
STEP = 0.0015
SMOOTH = int(opt.get("smooth", "0"))
PAD = 0.02
xs = np.arange(wcx - wrx - PAD, wcx + wrx + PAD + STEP, STEP)
zs = np.arange(wcz - wrz - PAD, wcz + wrz + PAD + STEP, STEP)


def load(path):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=path)
    arm = next((o for o in bpy.data.objects if o.type == "ARMATURE"), None)
    if arm:
        arm.animation_data_clear()
        for action in list(bpy.data.actions):
            bpy.data.actions.remove(action)
        arm.data.pose_position = "REST"
    mesh = max((o for o in bpy.data.objects if o.type == "MESH"), key=lambda o: len(o.data.polygons))
    return arm, mesh


def front_depth(mesh, gx, gz):
    """World y of the front-most surface (the model faces -Y) at each (x, z); nan where nothing is hit."""
    mw = mesh.matrix_world
    data = mesh.data
    mod = None
    if SMOOTH > 0:
        # a Tripo face is a few big flat triangles (its normals make it look round): measured as it is, the flat faces
        # of both heads come through as lumps. Measure the rounded surface a subdivision gives instead.
        mod = mesh.modifiers.new("measure", "SUBSURF")
        mod.levels = SMOOTH
        data = mesh.evaluated_get(bpy.context.evaluated_depsgraph_get()).data
    verts = [mw @ v.co for v in data.vertices]
    tree = BVHTree.FromPolygons(verts, [p.vertices[:] for p in data.polygons])
    if mod:
        mesh.modifiers.remove(mod)
    y0 = min(v.y for v in verts) - 1.0
    out = np.full((len(gz), len(gx)), np.nan)
    for a, z in enumerate(gz):
        for b, x in enumerate(gx):
            hit = tree.ray_cast(Vector((x, y0, z)), Vector((0, 1, 0)))
            if hit[0] is not None:
                out[a, b] = hit[0].y
    return out


BRIDGE = [float(v) for v in opt["bridge"].split(",")] if "bridge" in opt else None
if BRIDGE is None:
    _, old_mesh = load(old_fbx)
    old_d = front_depth(old_mesh, ox + (xs - nx) / scale, oz + (zs - nz) / scale) * scale
    if "fill" in opt:
        # --fill cells,keep,deepest: narrow deep cuts in the old face (the tiger's carved mouth under an overhanging lip)
        # came over as a jagged trench. An opening of the depth (min then max over a square of 2*cells+1 grid steps: y is
        # the distance away, so narrow far-away spots go) fills them; keep that share of what was filled, at most deepest
        cells, keep, deepest = (float(c) for c in opt["fill"].split(","))
        cells = int(cells)
        valid = ~np.isnan(old_d)
        filled = np.where(valid, old_d, np.nanmax(old_d))

        def square(a, fn):
            p = np.pad(a, cells, mode="edge")
            out_a = a.copy()
            for dy in range(-cells, cells + 1):
                for dx in range(-cells, cells + 1):
                    out_a = fn(out_a, p[cells + dy:cells + dy + a.shape[0], cells + dx:cells + dx + a.shape[1]])
            return out_a

        opened = square(square(filled, np.minimum), np.maximum)
        cut = np.clip(filled - opened, 0.0, None)
        old_d = np.where(valid, opened + np.minimum(cut * keep, deepest), old_d)
        print(f"RELIEF filled cuts up to {cut[valid].max():.4f} deep, kept {keep} of them (at most {deepest})")
arm, mesh = load(new_fbx)
new_d = front_depth(mesh, xs, zs)

X, Z = np.meshgrid(xs, zs)
e = np.sqrt(((X - wcx) / wrx) ** 2 + ((Z - wcz) / wrz) ** 2)
inside = e < 1.0
delta = new_d - (old_d if BRIDGE is None else new_d)                     # + = the old head stands out further toward the viewer here
delta = np.where(np.isnan(delta), np.nanmedian(delta), delta)


def jacobi(val, unk, n):
    for _ in range(n):
        p = np.pad(val, 1, mode="edge")
        avg = (p[:-2, 1:-1] + p[2:, 1:-1] + p[1:-1, :-2] + p[1:-1, 2:]) * 0.25
        val = np.where(unk, avg, val)
    return val


def blur(a, r):
    for _ in range(r):
        p = np.pad(a, 1, mode="edge")
        a = (p[:-2, 1:-1] + p[2:, 1:-1] + p[1:-1, :-2] + p[1:-1, 2:] + 4 * p[1:-1, 1:-1]) / 8.0
    return a


delta = blur(delta, int(opt["blur"]))                    # the old head's fur ripples and facets are not wanted, only the big forms
def membrane(val, unk):
    """The unknown part filled smoothly from the known part round it (coarse grid first, so a big window settles)."""
    h, w = unk.shape
    if min(h, w) <= 24 or not unk.any():
        return jacobi(np.where(unk, float(val[~unk].mean()) if (~unk).any() else 0.0, val), unk, 800)
    vp = np.pad(val, ((0, h % 2), (0, w % 2)), mode="edge")
    up = np.pad(unk, ((0, h % 2), (0, w % 2)), mode="edge")
    known = (~up).astype(float)
    k4 = known[0::2, 0::2] + known[1::2, 0::2] + known[0::2, 1::2] + known[1::2, 1::2]
    v4 = (vp * known)[0::2, 0::2] + (vp * known)[1::2, 0::2] + (vp * known)[0::2, 1::2] + (vp * known)[1::2, 1::2]
    coarse = membrane(v4 / np.maximum(k4, 1.0), k4 == 0)
    fine = np.repeat(np.repeat(coarse, 2, axis=0), 2, axis=1)[:h, :w]
    return jacobi(np.where(unk, fine, val), unk, 80)


sheet = membrane(delta, inside)
t = np.clip((1.0 - e) * min(wrx, wrz) / max(feather, 1e-6), 0.0, 1.0)
t = t * t * (3 - 2 * t)
move = np.clip((delta - sheet) * amount * t, -max_move, max_move)     # + = toward the viewer (-Y)
if BRIDGE is not None:
    # a clean nose bridge instead of the measured difference: a ridge on the centre line and a shallow valley at each
    # eye's inner corner, full strength between z lo and z hi, fading out over zfade above and below
    A, s1, V, x0, s2, zlo, zhi, zfade = BRIDGE
    dx = X - nx
    across = A * np.exp(-(dx / s1) ** 2) - V * (np.exp(-((dx - x0) / s2) ** 2) + np.exp(-((dx + x0) / s2) ** 2))
    up = np.clip(np.minimum(Z - (zlo - zfade), (zhi + zfade) - Z) / zfade, 0.0, 1.0)
    move = np.clip(across * up * up * (3 - 2 * up) * t, -max_move, max_move)
print(f"RELIEF old - new inside the window, edge taken out: {np.min((delta - sheet)[inside]):+.4f} .. "
      f"{np.max((delta - sheet)[inside]):+.4f}; moved {move.min():+.4f} .. {move.max():+.4f} (amount {amount})")
row = move[int(np.argmin(np.abs(zs - nz)))]
print("RELIEF move along the eye line, x from left to right: " + " ".join(f"{v * 1000:+.1f}" for v in row[::8]) + " (mm at height 1 m)")


def grid(a, x, z):
    fx = np.clip((x - xs[0]) / STEP, 0, len(xs) - 1.001)
    fz = np.clip((z - zs[0]) / STEP, 0, len(zs) - 1.001)
    ix, iz = int(fx), int(fz)
    tx, tz = fx - ix, fz - iz
    return (a[iz, ix] * (1 - tx) * (1 - tz) + a[iz, ix + 1] * tx * (1 - tz)
            + a[iz + 1, ix] * (1 - tx) * tz + a[iz + 1, ix + 1] * tx * tz)


mw = mesh.matrix_world.copy()
inv = mw.inverted()
rot = mw.to_3x3()
rot_inv = rot.inverted()
# each vertex's shading normal as the file has it (a Tripo face is a few big flat triangles that only look smooth because
# of these), carried through the cutting as a vertex layer and tilted afterwards by the slope of the push
orig_n = np.zeros((len(mesh.data.vertices), 3))
for lp, cnr in zip(mesh.data.loops, mesh.data.corner_normals):
    orig_n[lp.vertex_index] += np.array(cnr.vector)
bm = bmesh.new()
bm.from_mesh(mesh.data)
before = len(bm.verts)
n0 = bm.verts.layers.float_vector.new("n0")
for v in bm.verts:
    v[n0] = Vector(orig_n[v.index])


def on_front(v):
    w = mw @ v.co
    if abs(w.x - wcx) > wrx + PAD * 0.5 or abs(w.z - wcz) > wrz + PAD * 0.5:
        return False
    return abs(w.y - grid(new_d, w.x, w.z)) < 0.012


if cuts > 0:
    faces = [f for f in bm.faces if all(on_front(v) for v in f.verts)]
    edges = list({ed for f in faces for ed in f.edges})
    bmesh.ops.subdivide_edges(bm, edges=edges, cuts=cuts, use_grid_fill=True)
    bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 3])
moved = 0
slope_z, slope_x = np.gradient(move, STEP)
bm.verts.index_update()
new_n = {}
for v in bm.verts:
    if on_front(v):
        w = mw @ v.co
        n = rot @ Vector(v[n0])
        if n.length > 1e-6:
            n.normalize()
            # the surface goes y -> y - move(x, z): its normal turns by the slope
            new_n[v.index] = (rot_inv @ Vector((n.x + grid(slope_x, w.x, w.z) * n.y, n.y, n.z + grid(slope_z, w.x, w.z) * n.y))).normalized()
        d = grid(move, w.x, w.z)
        if abs(d) > 1e-6:
            w.y -= d
            v.co = inv @ w
            moved += 1
bm.verts.layers.float_vector.remove(n0)
bm.to_mesh(mesh.data)
bm.free()
mesh.data.update()
loop_n = [new_n.get(lp.vertex_index) or cnr.vector.copy() for lp, cnr in zip(mesh.data.loops, mesh.data.corner_normals)]
mesh.data.normals_split_custom_set([tuple(n) for n in loop_n])
mesh.data.update()
# (the file's normals are kept everywhere else; cleared, or left to be interpolated by the cutting, the face came out streaky)
if "--clear-normals" in argv and mesh.data.has_custom_normals:
    with bpy.context.temp_override(object=mesh, active_object=mesh, selected_objects=[mesh]):
        bpy.ops.mesh.customdata_custom_splitnormals_clear()
loose = sum(1 for v in mesh.data.vertices if not v.groups) if arm else 0
print(f"RELIEF vertices {before} -> {len(mesh.data.vertices)}, {moved} moved, {loose} without bone weights")
if arm:
    bpy.ops.export_scene.fbx(
        filepath=out_fbx, use_selection=False, object_types={"ARMATURE", "MESH"}, add_leaf_bones=False, bake_anim=False,
        primary_bone_axis="Y", secondary_bone_axis="X", use_armature_deform_only=False, mesh_smooth_type="OFF",
        path_mode="STRIP")
else:
    bpy.ops.export_scene.fbx(filepath=out_fbx, use_selection=False, object_types={"MESH"}, bake_anim=False,
                             mesh_smooth_type="OFF", path_mode="STRIP")
print(f"RELIEF wrote {out_fbx}")
