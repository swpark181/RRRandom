"""Lay a rigged animal friend's tail out straight without parting it from the body (run in Blender, 2026-11-24, the fox).

The tails were modelled swept 40 to 60 degrees to one side from the very root. blender_tail_part.py cut the fox's off and
set it back in as a part of its own: straight and smooth, but where it went into the body showed (a line round the tail
where it was cut, and the tail standing in the trousers like a peg). This one keeps the mesh whole. Every vertex of the
old tail moves to where a new centerline puts it (straight back out of the middle of the body, the old rise and fall
kept; --yaw-end turns the far end to a side), by a share that grows from nothing at the edge of the tail's old skin to
all of it a few centimetres in, measured along the surface: the skin round the root is drawn over with the tail and
stays one surface, its texture and UVs untouched. The same share splits the skin there between the hips and the tail,
whose new, longer chain of bones starts where the old one did, inside the body.

Usage: blender -b --factory-startup --python blender_tail_straight.py -- <rig fbx> <out fbx> [--bones 8] [--blend 0.07]
           [--relax 3] [--yaw-end 0] [--yaw-from 0.1] [--yaw-to 0.8] [--mid-x auto]
  <rig fbx>: the rig blender_animal_tail.py made (mixamorig:Tail1.. off the hips; meters, +Y behind, +X the left, Z up).
  A rig with several meshes (body + muzzle part): the first mesh is the body.
  --blend: how far along the surface from the edge of the tail's old skin the move and the tail's share reach full (m).
  --relax: smoothing passes over the stretch in between (0: none).
Prints TAILSTRAIGHT lines. Look at the result with blender_tail_view.py before it goes into Unreal.
"""
import heapq
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
BLEND = opt("--blend", 0.07)
RELAX = opt("--relax", 3)
YAW_END = math.radians(opt("--yaw-end", 0.0))
YAW_FROM = opt("--yaw-from", 0.1)
YAW_TO = opt("--yaw-to", 0.8)
MID_X = opt("--mid-x", "auto")
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
mid_x = (arm.matrix_world @ arm.data.bones[prefix + "Hips"].head_local).x if MID_X == "auto" else float(MID_X)
print(f"TAILSTRAIGHT old bones {len(old_bones)}, root {tuple(round(c, 3) for c in J[0])}, body middle x {mid_x:.3f}")

new_names = [f"{prefix}Tail{i + 1}" for i in range(BONES)]
for n in new_names + [prefix + "Hips"]:
    if not mesh.vertex_groups.get(n):
        mesh.vertex_groups.new(name=n)
group_index = {g.name: g.index for g in mesh.vertex_groups}
tail_groups = {i for n, i in group_index.items() if n.startswith(prefix + "Tail")}
hips_group = group_index[prefix + "Hips"]

to_world = mesh.matrix_world
to_local = to_world.inverted()
bm = bmesh.new()
bm.from_mesh(mesh.data)
deform = bm.verts.layers.deform.verify()
tail = {v for v in bm.verts if sum(w for g, w in v[deform].items() if g in tail_groups) > 0.0}
world = {v: to_world @ v.co for v in tail}

# 1. How far each vertex of the old tail's skin is from that skin's edge, along the surface
reach = {}
heap = []
for v in tail:
    edge = [(world[v] - to_world @ e.other_vert(v).co).length for e in v.link_edges if e.other_vert(v) not in tail]
    if edge:
        reach[v] = min(edge)
        heapq.heappush(heap, (reach[v], v.index))
bm.verts.ensure_lookup_table()
while heap:
    d, i = heapq.heappop(heap)
    v = bm.verts[i]
    if d > reach.get(v, 1e9):
        continue
    for e in v.link_edges:
        o = e.other_vert(v)
        if o in tail:
            nd = d + (world[v] - world[o]).length
            if nd < reach.get(o, 1e9):
                reach[o] = nd
                heapq.heappush(heap, (nd, o.index))
share = {v: smoothstep(0.0, BLEND, reach.get(v, 1e9)) for v in tail}   # (a loose piece of the tail: all of it)

# 2. The old centerline (the old bones' line, rounded, run on to the tip) and the new one (the same rise and fall, the
# heading replaced)


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


def along(p, a, b):
    return intersect_point_line(p, a, b)[1]


far = sorted(tail, key=lambda v: -max((along(world[v], J[i], J[i + 1]) + i for i in range(len(J) - 1))))[:6]
tip_point = sum((world[v] for v in far), Vector()) / len(far)
old_line = catmull(J + [tip_point])
old_acc = [0.0]
for i in range(1, len(old_line)):
    old_acc.append(old_acc[-1] + (old_line[i] - old_line[i - 1]).length)
length = old_acc[-1]


def frame(t):
    side = UP.cross(t)
    side = side.normalized() if side.length > 1e-4 else Vector((1.0, 0.0, 0.0))
    return t, side, t.cross(side)


new_line = [Vector((mid_x, J[0].y, J[0].z))]
old_t, new_t = [], []
for i in range(len(old_line) - 1):
    t = old_line[i + 1] - old_line[i]
    step = t.length
    t = t.normalized()
    pitch = math.asin(max(-1.0, min(1.0, t.z)))
    yaw = YAW_END * smoothstep(YAW_FROM, YAW_TO, (old_acc[i] + step * 0.5) / length)
    n = Vector((-math.sin(yaw) * math.cos(pitch), math.cos(yaw) * math.cos(pitch), math.sin(pitch)))
    old_t.append(t)
    new_t.append(n)
    new_line.append(new_line[-1] + n * step)


def at(s):
    i = len(old_line) - 2
    for k in range(len(old_line) - 1):
        if s < old_acc[k + 1]:
            i = k
            break
    d = s - old_acc[i]
    return (old_line[i] + old_t[i] * d, frame(old_t[i])), (new_line[i] + new_t[i] * d, frame(new_t[i]))


def moved(p):
    """Where a point of the old tail goes, and how far along the tail it is (the ends run on)."""
    best = (1e9, 0.0)
    for i in range(len(old_line) - 1):
        t = max(0.0, min(1.0, along(p, old_line[i], old_line[i + 1])))
        d = (p - (old_line[i] + (old_line[i + 1] - old_line[i]) * t)).length_squared
        if d < best[0]:
            best = (d, old_acc[i] + t * (old_acc[i + 1] - old_acc[i]))
    s = best[1]
    if s <= 1e-6:
        s = (p - old_line[0]).dot(old_t[0])
    elif s >= length - 1e-6:
        s = length + (p - old_line[-1]).dot(old_t[-1])
    (op, (ot, oside, oup)), (np_, (nt, nside, nup)) = at(s)
    o = p - op
    return np_ + nt * o.dot(ot) + nside * o.dot(oside) + nup * o.dot(oup), s


# 3. Move the tail, by its share
params = {}
most = 0.0
for v in tail:
    q, s = moved(world[v])
    params[v] = s
    v.co = to_local @ world[v].lerp(q, share[v])
    if share[v] < 1.0:
        most = max(most, (q - world[v]).length * share[v])
between = [v for v in tail if 0.0 < share[v] < 1.0]
for _ in range(RELAX):
    target = {v: sum((e.other_vert(v).co for e in v.link_edges), Vector()) / len(v.link_edges) for v in between}
    for v, c in target.items():
        v.co = v.co.lerp(c, 0.4)
print(f"TAILSTRAIGHT tail {len(tail)} vertices, {length * 100:.0f} cm; {len(between)} between the body and the tail moved up to {most * 100:.1f} cm; "
      f"tip was {tuple(round(c, 3) for c in old_line[-1])}, now {tuple(round(c, 3) for c in new_line[-1])}")

# 4. The skin: the tail's share to its own bones (each vertex between the two bones whose middles it lies between),
# the rest to the hips
joints_s = [length * k / BONES for k in range(BONES + 1)]
mids_s = [(joints_s[k] + joints_s[k + 1]) * 0.5 for k in range(BONES)]
for v in tail:
    s, b = params[v], share[v]
    d = v[deform]
    d.clear()
    if b < 0.999:
        d[hips_group] = 1.0 - b
    if b < 0.001:
        continue
    if s <= mids_s[0]:
        d[group_index[new_names[0]]] = b
    elif s >= mids_s[-1]:
        d[group_index[new_names[-1]]] = b
    else:
        k = max(i for i in range(BONES - 1) if mids_s[i] <= s)
        t = (s - mids_s[k]) / (mids_s[k + 1] - mids_s[k])
        if t < 0.999:
            d[group_index[new_names[k]]] = b * (1.0 - t)
        if t > 0.001:
            d[group_index[new_names[k + 1]]] = b * t

bm.normal_update()
bm.to_mesh(mesh.data)
bm.free()
mesh.data.update()

# 5. The bones: the old chain out, the new one in along the new centerline
bpy.context.view_layer.objects.active = arm
bpy.ops.object.mode_set(mode="EDIT")
to_arm = arm.matrix_world.inverted()
edit_bones = arm.data.edit_bones
for n in old_bones:
    edit_bones.remove(edit_bones[n])
parent = edit_bones[prefix + "Hips"]
points = [at(s)[1][0] for s in joints_s]
for k, n in enumerate(new_names):
    bone = edit_bones.new(n)
    bone.head = to_arm @ points[k]
    bone.tail = to_arm @ points[k + 1]
    bone.align_roll(parent.z_axis)
    bone.parent = parent
    bone.use_connect = k > 0
    parent = bone
bpy.ops.object.mode_set(mode="OBJECT")
for g in list(mesh.vertex_groups):
    if g.name.startswith(prefix + "Tail") and g.name not in new_names:
        mesh.vertex_groups.remove(g)
print(f"TAILSTRAIGHT bones {BONES}: " + " ".join(f"{p.x:.3f},{p.y:.3f},{p.z:.3f}" for p in points))

bpy.ops.export_scene.fbx(
    filepath=out, use_selection=False, object_types={"ARMATURE", "MESH"}, add_leaf_bones=False, bake_anim=False,
    primary_bone_axis="Y", secondary_bone_axis="X", use_armature_deform_only=False, mesh_smooth_type="OFF",
    path_mode="STRIP")
print(f"TAILSTRAIGHT wrote {out}: {len(mesh.data.vertices)} vertices, {len(mesh.data.polygons)} faces")
