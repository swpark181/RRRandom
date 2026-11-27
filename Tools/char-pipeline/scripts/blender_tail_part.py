"""Make a rigged animal friend's tail a part of its own (run in Blender, 2026-11-24, the fox first).

The tails were modelled swept 40 to 60 degrees to one side from the very root and skinned to four bones with 3 cm joints,
the first 5 cm half the hips'. Straightening one in the game by turning its bones showed a fold where it meets the body
and a corner at every joint. This cuts the tail off the body, lays it out again along a new centerline (straight back
out of the body, then curling to the side it was on), gives it a longer chain of bones with the skin shared evenly
between neighbours, and keeps body and tail one mesh on one armature: the tail's skin is the tail bones' only, and it
runs on a little way into the body past the first bone's head (the middle of the cut); the body's own stump of the
old tail is taken out and the hole closed. A rig with several meshes (body + muzzle part): the first mesh is the body.
Run it on the one-piece rig (the fur is made from that) and on the parts rig (the game's mesh): the tails come out the same.

Usage: blender -b --factory-startup --python blender_tail_part.py -- <rig fbx> <out fbx> [--bones 8] [--cut 0.05]
           [--yaw-end 75] [--yaw-from 0.1] [--yaw-to 0.8] [--mid-x auto]
  <rig fbx>: the rig blender_animal_tail.py made (mixamorig:Tail1.. off the hips; meters, +Y behind, +X the left, Z up).
  --cut: where along the old first bone the tail is cut (m from its head): past where its skin is the tail's alone.
  --yaw-*: the new centerline keeps the old one's rise and fall and replaces its heading: 0 (straight back) up to
  --yaw-from of its length, turning to --yaw-end degrees to the right by --yaw-to (negative: to the left).
  --mid-x: the body's middle (the new root's x); auto = the hips bone's.
Prints TAILPART lines; the game's code (URRRandomBodyAnimInstance) finds Tail5.. on the mesh by itself.
"""
import math
import sys

import bmesh
import bpy
from mathutils import Vector
from mathutils.geometry import intersect_point_line

args = sys.argv[sys.argv.index("--") + 1:]
src, out = args[0], args[1]


def opt(name, default):
    return type(default)(args[args.index(name) + 1]) if name in args else default


BONES = opt("--bones", 8)
CUT = opt("--cut", 0.05)
YAW_END = math.radians(opt("--yaw-end", 75.0))
YAW_FROM = opt("--yaw-from", 0.1)
YAW_TO = opt("--yaw-to", 0.8)
MID_X = opt("--mid-x", "auto")
ROOT_ROWS = 3
ROOT_DEPTH = 0.09     # how far the tail runs on into the body (m)
ROOT_NARROW = 0.6     # ...and how wide it still is at its end
UP = Vector((0.0, 0.0, 1.0))


def smoothstep(e0, e1, x):
    t = max(0.0, min(1.0, (x - e0) / (e1 - e0)))
    return t * t * (3.0 - 2.0 * t)


bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
mesh = next(o for o in bpy.data.objects if o.type == "MESH")
arm.animation_data_clear()
for action in list(bpy.data.actions):
    bpy.data.actions.remove(action)
arm.data.pose_position = "REST"
prefix = next(b.name for b in arm.data.bones if b.name.endswith("Hips")).replace("Hips", "")
old_bones = sorted((b.name for b in arm.data.bones if b.name.startswith(prefix + "Tail")), key=lambda n: int(n.replace(prefix + "Tail", "")))
J = [arm.matrix_world @ arm.data.bones[old_bones[0]].head_local] + [arm.matrix_world @ arm.data.bones[n].tail_local for n in old_bones]
hips_head = arm.matrix_world @ arm.data.bones[prefix + "Hips"].head_local
mid_x = hips_head.x if MID_X == "auto" else float(MID_X)
axis = (J[1] - J[0]).normalized()
print(f"TAILPART old bones {len(old_bones)}, root {tuple(round(c, 3) for c in J[0])}, body middle x {mid_x:.3f}")

new_names = [f"{prefix}Tail{i + 1}" for i in range(BONES)]
for n in new_names + [prefix + "Hips"]:
    if not mesh.vertex_groups.get(n):
        mesh.vertex_groups.new(name=n)
group_index = {g.name: g.index for g in mesh.vertex_groups}
tail_groups = {i for n, i in group_index.items() if n.startswith(prefix + "Tail")}

to_world = mesh.matrix_world
to_local = to_world.inverted()
bm = bmesh.new()
bm.from_mesh(mesh.data)
deform = bm.verts.layers.deform.verify()
uv = bm.loops.layers.uv.verify()

along = [0.0]
for i in range(1, len(J)):
    along.append(along[-1] + (J[i] - J[i - 1]).length)


def on_old_bones(p):
    """Distance to the old bones' line and how far along it (the ends run on) the nearest point is."""
    best = (1e9, 0.0)
    for i in range(len(J) - 1):
        _, t = intersect_point_line(p, J[i], J[i + 1])
        t = min(1.0, t) if i == 0 else (max(0.0, t) if i == len(J) - 2 else max(0.0, min(1.0, t)))
        d = (p - (J[i] + (J[i + 1] - J[i]) * t)).length
        if d < best[0]:
            best = (d, along[i] + t * (along[i + 1] - along[i]))
    return best


def tail_weight(v):
    return sum(w for g, w in v[deform].items() if g in tail_groups)


# 1. Cut the tail off: a plane across the old first bone, through the faces that are the tail's
plane_co = J[0] + axis * CUT
near_cut = [f for f in bm.faces if all(tail_weight(v) > 0.0 for v in f.verts)
            and all(on_old_bones(to_world @ v.co)[1] < along[1] + 0.03 for v in f.verts)]
geom = list({v for f in near_cut for v in f.verts}) + list({e for f in near_cut for e in f.edges}) + near_cut
cut = bmesh.ops.bisect_plane(bm, geom=geom, dist=1e-6, plane_co=to_local @ plane_co, plane_no=(to_local.to_3x3() @ axis).normalized(),
                             clear_inner=False, clear_outer=False)
cut_edges = [g for g in cut["geom_cut"] if isinstance(g, bmesh.types.BMEdge)]
bmesh.ops.split_edges(bm, edges=cut_edges)
bm.verts.ensure_lookup_table()
tip = max((v for v in bm.verts if tail_weight(v) > 0.0), key=lambda v: on_old_bones(to_world @ v.co)[1])
tail_verts, todo = {tip}, [tip]
while todo:
    for e in todo.pop().link_edges:
        for w in e.verts:
            if w not in tail_verts:
                tail_verts.add(w)
                todo.append(w)
on_plane = lambda v: abs((to_world @ v.co - plane_co).dot(axis)) < 1e-4 and (to_world @ v.co - plane_co).length < 0.2   # noqa: E731
tail_ring = [e for e in bm.edges if e.is_boundary and e.verts[0] in tail_verts and on_plane(e.verts[0]) and on_plane(e.verts[1])]
body_ring = [e for e in bm.edges if e.is_boundary and e.verts[0] not in tail_verts and on_plane(e.verts[0]) and on_plane(e.verts[1])]
print(f"TAILPART cut {len(cut_edges)} edges at {CUT * 100:.0f} cm: tail {len(tail_verts)} vertices, rings {len(tail_ring)} / {len(body_ring)} edges")
if len(tail_verts) > len(bm.verts) // 2 or not tail_ring or len(tail_ring) != len(body_ring):
    raise SystemExit("TAILPART the cut did not part the tail from the body")

# 3. The tail's old centerline: the old bones' line, rounded, run on to the tip


def polyline_param(points):
    acc = [0.0]
    for i in range(1, len(points)):
        acc.append(acc[-1] + (points[i] - points[i - 1]).length)
    return acc


def nearest_on(points, acc, p):
    best = (1e9, 0.0)
    for i in range(len(points) - 1):
        _, t = intersect_point_line(p, points[i], points[i + 1])
        t = max(0.0, min(1.0, t))
        d = (p - (points[i] + (points[i + 1] - points[i]) * t)).length_squared
        if d < best[0]:
            best = (d, acc[i] + t * (acc[i + 1] - acc[i]))
    return best[1]


def catmull(points, per=16):
    pts = [points[0] * 2 - points[1]] + list(points) + [points[-1] * 2 - points[-2]]
    dense = []
    for i in range(1, len(pts) - 2):
        p0, p1, p2, p3 = pts[i - 1], pts[i], pts[i + 1], pts[i + 2]
        for k in range(per):
            t = k / per
            dense.append(0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t + (-p0 + 3 * p1 - 3 * p2 + p3) * t * t * t))
    dense.append(points[-1])
    return dense


tip_point = sum((to_world @ v.co for v in sorted(tail_verts, key=lambda v: -on_old_bones(to_world @ v.co)[1])[:6]), Vector()) / 6
# (the middles of the tail's own slices were tried instead of the bones' line: on a tail this thick they zigzag, and the
# root came out crumpled)
line = catmull(J + [tip_point])
old_line = line
old_acc = polyline_param(old_line)
length = old_acc[-1]


def frame(t):
    side = UP.cross(t)
    side = side.normalized() if side.length > 1e-4 else Vector((1.0, 0.0, 0.0))
    return t, side, t.cross(side)


# 4. The new centerline: the same rise and fall, the heading replaced
new_line = [Vector((mid_x, J[0].y, J[0].z))]
old_t, new_t = [], []
for i in range(len(old_line) - 1):
    t = (old_line[i + 1] - old_line[i])
    step = t.length
    t = t.normalized()
    pitch = math.asin(max(-1.0, min(1.0, t.z)))
    yaw = YAW_END * smoothstep(YAW_FROM, YAW_TO, (old_acc[i] + step * 0.5) / length)
    n = Vector((-math.sin(yaw) * math.cos(pitch), math.cos(yaw) * math.cos(pitch), math.sin(pitch)))
    old_t.append(t)
    new_t.append(n)
    new_line.append(new_line[-1] + n * step)


def at(s):
    """The old and the new centerline at s (which may run past either end): point, frame."""
    i = len(old_line) - 2
    for k in range(len(old_line) - 1):
        if s < old_acc[k + 1]:
            i = k
            break
    d = s - old_acc[i]
    return (old_line[i] + old_t[i] * d, frame(old_t[i])), (new_line[i] + new_t[i] * d, frame(new_t[i]))


def moved(p):
    """Where a point of the old tail goes, and how far along the tail it is."""
    s = nearest_on(old_line, old_acc, p)
    # past the ends the nearest point is the end itself: run on along the end's direction
    if s <= 1e-6:
        s = (p - old_line[0]).dot(old_t[0])
    elif s >= length - 1e-6:
        s = length + (p - old_line[-1]).dot(old_t[-1])
    (op, (ot, oside, oup)), (np_, (nt, nside, nup)) = at(s)
    o = p - op
    return np_ + nt * o.dot(ot) + nside * o.dot(oside) + nup * o.dot(oup), s


s_ring = nearest_on(old_line, old_acc, plane_co)
params = {}
for v in tail_verts:
    q, s = moved(to_world @ v.co)
    v.co = to_local @ q
    params[v] = s
print(f"TAILPART tail {length * 100:.0f} cm along its middle, cut at {s_ring * 100:.1f} cm; tip was {tuple(round(c, 3) for c in old_line[-1])}, now {tuple(round(c, 3) for c in new_line[-1])}")

# 5. The body: what is left of the old root (the stump up to the cut and the skin around it that was partly the tail's)
# is taken out, and the hole it leaves in the rump closed flat. (Tried first: pressing the stump flat tore into the
# trousers, turning it to meet the new tail left a ledge and stretched skin. The tail's root ball covers the patch.)
old_root = {v for v in bm.verts if v not in tail_verts and tail_weight(v) > 0.0}
gone = [f for f in bm.faces if any(v in old_root for v in f.verts)]
rim = {v for f in gone for v in f.verts if v not in old_root}
bmesh.ops.delete(bm, geom=gone, context="FACES")
bmesh.ops.delete(bm, geom=[v for v in old_root if v.is_valid and not v.link_faces], context="VERTS")
hole = [e for e in bm.edges if e.is_boundary and e.verts[0] in rim and e.verts[1] in rim]
loops, left = [], set(hole)
while left:
    e = left.pop()
    loop_edges, todo = [e], [e]
    while todo:
        for v in todo.pop().verts:
            for o in v.link_edges:
                if o in left:
                    left.discard(o)
                    loop_edges.append(o)
                    todo.append(o)
    loops.append(loop_edges)
for loop_edges in loops:
    verts = {v for e in loop_edges for v in e.verts}
    centre = bm.verts.new(sum((v.co for v in verts), Vector()) / len(verts))
    weights = {}
    for v in verts:
        for g, w in v[deform].items():
            weights[g] = weights.get(g, 0.0) + w / len(verts)
    for g, w in weights.items():
        centre[deform][g] = w
    for e in loop_edges:
        loop = e.link_loops[0]
        a, b = loop.vert, loop.link_loop_next.vert
        face = bm.faces.new((b, a, centre))
        face.smooth = True
        face.material_index = loop.face.material_index
        for l in face.loops:
            l[uv].uv = (loop.link_loop_next[uv].uv if l.vert is b else loop[uv].uv).copy()
(_, _), (root, (root_t, _, _)) = at(s_ring)
inward = -root_t
print(f"TAILPART body: {len(old_root)} vertices of the old root taken out with {len(gone)} faces, the hole ({[len(l) for l in loops]} edges) closed")

# 6. The tail's skin: its own bones only, each vertex shared between the two bones whose middles it lies between
joints_s = [s_ring + (length - s_ring) * k / BONES for k in range(BONES + 1)]
mids_s = [(joints_s[k] + joints_s[k + 1]) * 0.5 for k in range(BONES)]


def skin(v, s):
    d = v[deform]
    d.clear()
    if s <= mids_s[0]:
        d[group_index[new_names[0]]] = 1.0
    elif s >= mids_s[-1]:
        d[group_index[new_names[-1]]] = 1.0
    else:
        k = max(i for i in range(BONES - 1) if mids_s[i] <= s)
        t = (s - mids_s[k]) / (mids_s[k + 1] - mids_s[k])
        if t < 0.999:
            d[group_index[new_names[k]]] = 1.0 - t
        if t > 0.001:
            d[group_index[new_names[k + 1]]] = t


for v, s in params.items():
    skin(v, s)

# 7. The tail's root: the tail run on into the body, straight along its first bone and a little narrower at the end,
# so it meets the body the way a tail does and still does whichever way it swings about the first bone's head (the
# middle of the cut). (Tried first: a cone to that point stood out of the body as a flap, half a ball as a knob.)
rows = {}
for e in tail_ring:
    for v in e.verts:
        if v in rows:
            continue
        spoke = to_world @ v.co - root
        row = [v]
        for k in range(1, ROOT_ROWS + 1):
            t = k / ROOT_ROWS
            nv = bm.verts.new(to_local @ (root + spoke * (1.0 - (1.0 - ROOT_NARROW) * t * t) + inward * ROOT_DEPTH * t))
            nv[deform][group_index[new_names[0]]] = 1.0
            row.append(nv)
        rows[v] = row
pole = bm.verts.new(to_local @ (root + inward * ROOT_DEPTH))
pole[deform][group_index[new_names[0]]] = 1.0
for e in tail_ring:
    loop = e.link_loops[0]
    a, b = loop.vert, loop.link_loop_next.vert
    uv_a, uv_b = loop[uv].uv.copy(), loop.link_loop_next[uv].uv.copy()
    # The root wears the tail's own skin: each strip of it is the tail face it grows from, folded back and forth (the
    # ring's two corners, then that face's corner furthest down the tail, then the ring's again). With the ring's
    # corners alone the strips had no area in the texture, got no fur, and the root showed as a bare band with a step
    # where the fur began: the tail looked cut there.
    far = max((l for l in loop.face.loops if l.vert is not a and l.vert is not b), key=lambda l: params.get(l.vert, 0.0))[uv].uv.copy()
    at_row = lambda k, own: own if k % 2 == 0 else far   # noqa: E731
    for k in range(ROOT_ROWS + 1):
        if k < ROOT_ROWS:
            corners = [(rows[b][k], at_row(k, uv_b)), (rows[a][k], at_row(k, uv_a)), (rows[a][k + 1], at_row(k + 1, uv_a)), (rows[b][k + 1], at_row(k + 1, uv_b))]
        else:
            corners = [(rows[b][k], at_row(k, uv_b)), (rows[a][k], at_row(k, uv_a)), (pole, at_row(k + 1, uv_a))]
        face = bm.faces.new([c[0] for c in corners])
        face.smooth = True
        face.material_index = loop.face.material_index
        for l, c in zip(face.loops, corners):
            l[uv].uv = c[1].copy()
print(f"TAILPART root: {len(rows)} vertices around run {ROOT_DEPTH * 100:.0f} cm into the body from {tuple(round(c, 3) for c in root)}")

bm.normal_update()
bm.to_mesh(mesh.data)
bm.free()
mesh.data.update()

# 8. The bones: the old chain out, the new one in along the new centerline, from the middle of the cut


def new_point(s):
    return at(s)[1][0]


bpy.context.view_layer.objects.active = arm
bpy.ops.object.mode_set(mode="EDIT")
to_arm = arm.matrix_world.inverted()
edit_bones = arm.data.edit_bones
for n in old_bones:
    edit_bones.remove(edit_bones[n])
parent = edit_bones[prefix + "Hips"]
for k, n in enumerate(new_names):
    bone = edit_bones.new(n)
    bone.head = to_arm @ new_point(joints_s[k])
    bone.tail = to_arm @ new_point(joints_s[k + 1])
    bone.align_roll(parent.z_axis)
    bone.parent = parent
    bone.use_connect = k > 0
    parent = bone
bpy.ops.object.mode_set(mode="OBJECT")
for g in list(mesh.vertex_groups):
    if g.name.startswith(prefix + "Tail") and g.name not in new_names:
        mesh.vertex_groups.remove(g)
print(f"TAILPART bones {BONES}: " + " ".join(f"{new_point(s).x:.3f},{new_point(s).y:.3f},{new_point(s).z:.3f}" for s in joints_s))

bpy.ops.export_scene.fbx(
    filepath=out, use_selection=False, object_types={"ARMATURE", "MESH"}, add_leaf_bones=False, bake_anim=False,
    primary_bone_axis="Y", secondary_bone_axis="X", use_armature_deform_only=False, mesh_smooth_type="OFF",
    path_mode="STRIP")
print(f"TAILPART wrote {out}: {len(mesh.data.vertices)} vertices, {len(mesh.data.polygons)} faces")
