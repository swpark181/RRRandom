"""Put a newly generated model of an animal friend onto the rig its old model has, so the skeleton, and with it every
animation, stays as it is (run in Blender; 2026-11-24, the fox generated again with a straight tail).

Usage: blender -b --factory-startup --python blender_reskin.py -- <old rig fbx> <new model fbx> <out fbx>
           [--bones 8] [--blend 0.07] [--tail-z 0.05,0.45] [--name Body] [--tail-long 1.0] [--tail-fat 0,0]
  <old rig fbx>: armature + the old skinned mesh (several meshes: the first is the body and gives the weights).
  <new model fbx>: Tripo's export of the same character from the same pictures (T-pose, the same size), its tail held
  straight out behind it (the rule since 2026-11-24).
The new mesh is sized, turned and moved onto the old body (closest points, without either tail), takes the old body's
weights from the nearest spot of its surface, and gets a new tail chain: the tail is what lies behind the old body's
rump between the --tail-z heights (shares of the height), its middle line the slices' middles, Tail1.. along it, the
skin shared between neighbouring bones and, over the first --blend m of surface from the rump, with the body's weights.
  --tail-long / --tail-fat: the tail made that many times as long from the rump, and thicker about its middle line by
  the first share by the middle of its length and by the second more by nine tenths (0.2,0.4: 1.2 times at the middle,
  1.6 times near the tip; nothing at the rump, where it joins the body). The fox's new tail came out 46 cm and pointed
  where the old one was 65 cm and bushy to the end (the user: the old tail was bigger): --tail-long 1.4 --tail-fat 0.2,0.4.
Prints RESKIN lines. Check with blender_tail_view.py / blender_pose_check.py before Unreal.
"""
import heapq
import math
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector, kdtree

args = sys.argv[sys.argv.index("--") + 1:]
old_fbx, new_fbx, out_fbx = args[:3]


def opt(name, default):
    return type(default)(args[args.index(name) + 1]) if name in args else default


BONES = opt("--bones", 8)
BLEND = opt("--blend", 0.07)
Z_LO, Z_HI = (float(c) for c in opt("--tail-z", "0.05,0.45").split(","))
LONG = opt("--tail-long", 1.0)
FAT = [float(c) for c in opt("--tail-fat", "0,0").split(",")]
NAME = opt("--name", "Body")


def smoothstep(e0, e1, x):
    t = max(0.0, min(1.0, (x - e0) / (e1 - e0)))
    return t * t * (3.0 - 2.0 * t)


bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=old_fbx)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
olds = [o for o in bpy.data.objects if o.type == "MESH"]
old = olds[0]
arm.animation_data_clear()
arm.data.pose_position = "REST"
prefix = next(b.name for b in arm.data.bones if b.name.endswith("Hips")).replace("Hips", "")
old_tail = {g.index for g in old.vertex_groups if g.name.startswith(prefix + "Tail")}
old_co = [old.matrix_world @ v.co for v in old.data.vertices]
old_body = [p for v, p in zip(old.data.vertices, old_co) if not any(g.group in old_tail and g.weight > 0.0 for g in v.groups)]
height = max(p.z for p in old_co) - min(p.z for p in old_co)
band = [p for p in old_body if Z_LO * height + 0.05 < p.z < Z_HI * height - 0.1]
mid_x = (min(p.x for p in band) + max(p.x for p in band)) * 0.5
rump_y = max(p.y for p in band if abs(p.x - mid_x) < 0.05)
print(f"RESKIN old body {len(old_body)} of {len(old_co)} vertices, height {height:.3f}, middle x {mid_x:.3f}, rump at y {rump_y:.3f}")

before = set(bpy.data.objects)
bpy.ops.import_scene.fbx(filepath=new_fbx)
new = next(o for o in bpy.data.objects if o not in before and o.type == "MESH")
src = np.array([tuple(new.matrix_world @ v.co) for v in new.data.vertices])

# ---- the new model onto the old body: start from the two heights and the heads' fronts, then closest points ----
target = np.array([tuple(p) for p in old_body])
tree = kdtree.KDTree(len(old_body))
for i, p in enumerate(old_body):
    tree.insert(p, i)
tree.balance()
m = np.eye(4)
m[:3, :3] *= (target[:, 2].max() - target[:, 2].min()) / (src[:, 2].max() - src[:, 2].min())
p0 = src @ m[:3, :3].T
m[0, 3] = mid_x - (p0[:, 0].min() + p0[:, 0].max()) / 2
m[1, 3] = target[:, 1].min() - p0[:, 1].min()
m[2, 3] = target[:, 2].min() - p0[:, 2].min()
err = 0.0
for _ in range(30):
    p = src @ m[:3, :3].T + m[:3, 3]
    # (without the new tail: what is well behind the old rump at the tail's heights)
    keep = ~((p[:, 1] > rump_y + 0.02) & (p[:, 2] > Z_LO * height) & (p[:, 2] < Z_HI * height))
    q = p[keep]
    near = np.array([tuple(tree.find(Vector(v))[0]) for v in q])
    d = np.linalg.norm(near - q, axis=1)
    err = float(d.mean())
    good = d < np.percentile(d, 90)
    a0, b0 = q[good], near[good]
    ma, mb = a0.mean(0), b0.mean(0)
    a, b = a0 - ma, b0 - mb
    u, sv, vt = np.linalg.svd(a.T @ b)
    s = np.sign(np.linalg.det(vt.T @ u.T))
    r = vt.T @ np.diag([1.0, 1.0, s]) @ u.T
    k = (sv * [1.0, 1.0, s]).sum() / (a * a).sum()
    step = np.eye(4)
    step[:3, :3] = k * r
    step[:3, 3] = mb - k * r @ ma
    m = step @ m
turn = math.degrees(math.atan2(m[1, 0], m[0, 0]))
print(f"RESKIN fit: scale {np.linalg.norm(m[:3, 0]):.4f}, turned {turn:.2f} deg about up, moved {np.round(m[:3, 3], 4)}, mean distance {err * 100:.2f} cm")
new.matrix_world = Matrix(m.tolist()) @ new.matrix_world
bpy.ops.object.select_all(action="DESELECT")
new.select_set(True)
bpy.context.view_layer.objects.active = new
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
# the old mesh's object scale (the Mixamo rigs' meshes are 100 x under a 0.01 armature): the new one the same way
new.data.transform(old.matrix_world.inverted())
new.data.update()
new.parent = old.parent
new.matrix_parent_inverse = old.matrix_parent_inverse.copy()
new.matrix_basis = old.matrix_basis.copy()
bpy.context.view_layer.update()

# ---- the old body's weights, from the nearest spot of its surface ----
for g in old.vertex_groups:
    new.vertex_groups.new(name=g.name)
bpy.ops.object.select_all(action="DESELECT")
new.select_set(True)
old.select_set(True)
bpy.context.view_layer.objects.active = old
bpy.ops.object.data_transfer(data_type="VGROUP_WEIGHTS", vert_mapping="POLYINTERP_NEAREST", layers_select_src="ALL",
                             layers_select_dst="NAME", mix_mode="REPLACE")
for g in list(new.vertex_groups):
    if g.name.startswith(prefix + "Tail"):
        new.vertex_groups.remove(g)
names = [f"{prefix}Tail{i + 1}" for i in range(BONES)]
for n in names:
    new.vertex_groups.new(name=n)
group_index = {g.name: g.index for g in new.vertex_groups}

# ---- the tail: what lies behind the rump at the tail's heights, joined to the tail's far end ----
to_world = new.matrix_world
bm = bmesh.new()
bm.from_mesh(new.data)
deform = bm.verts.layers.deform.verify()
world = {v: to_world @ v.co for v in bm.verts}
behind = {v for v in bm.verts if world[v].y > rump_y and Z_LO * height < world[v].z < Z_HI * height}
tip = max(behind, key=lambda v: world[v].y)
tail, todo = {tip}, [tip]
while todo:
    for e in todo.pop().link_edges:
        for w in e.verts:
            if w in behind and w not in tail:
                tail.add(w)
                todo.append(w)
reach, heap = {}, []
for v in tail:
    edge = [(world[v] - world[e.other_vert(v)]).length for e in v.link_edges if e.other_vert(v) not in tail]
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
share = {v: smoothstep(0.0, BLEND, reach.get(v, 1e9)) for v in tail}

# its size: longer from the rump, thicker about the middle of each slice, more so toward the far end
if LONG != 1.0 or FAT[0] or FAT[1]:
    y_end = world[tip].y
    cuts = 12
    acc_c = [[Vector(), 0] for _ in range(cuts)]
    for v in tail:
        k = min(cuts - 1, max(0, int((world[v].y - rump_y) / (y_end - rump_y) * cuts)))
        acc_c[k][0] += world[v]
        acc_c[k][1] += 1
    cs = [a[0] / a[1] if a[1] else None for a in acc_c]
    for k in range(cuts):
        if cs[k] is None:
            cs[k] = next(c for c in (cs[k - 1:k] + cs[k + 1:k + 2]) if c is not None)

    def centre(t):
        f = min(cuts - 1.001, max(0.0, t * cuts - 0.5))
        i = int(f)
        return cs[i].lerp(cs[min(cuts - 1, i + 1)], f - i)

    for v in tail:
        p = world[v]
        t = max(0.0, (p.y - rump_y) / (y_end - rump_y))
        c = centre(t)
        k = 1.0 + FAT[0] * smoothstep(0.0, 0.5, t) + FAT[1] * smoothstep(0.5, 0.9, t)
        q = Vector((c.x + (p.x - c.x) * k, rump_y + (p.y - rump_y) * LONG, c.z + (p.z - c.z) * k))
        world[v] = q
        v.co = to_world.inverted() @ q
    print(f"RESKIN tail made {LONG:g} times as long, up to {1.0 + FAT[0] + FAT[1]:g} times as thick")

# its middle line: the middles of slices along the body's back-front axis, eased
y_tip = world[tip].y
SLICES = 10
sums = [[Vector(), 0.0] for _ in range(SLICES)]
for f in bm.faces:
    if all(v in tail for v in f.verts):
        c = to_world @ f.calc_center_median()
        k = min(SLICES - 1, max(0, int((c.y - rump_y) / (y_tip - rump_y) * SLICES)))
        area = f.calc_area()
        sums[k][0] += c * area
        sums[k][1] += area
mids = [s[0] / s[1] for s in sums if s[1] > 0.0]
mids = [mids[0]] + [(mids[i - 1] + mids[i] * 2 + mids[i + 1]) / 4 for i in range(1, len(mids) - 1)] + [mids[-1]]
root = Vector((mids[0].x, rump_y - 0.03, mids[0].z))
line = [root] + mids + [world[tip].copy()]
acc = [0.0]
for i in range(1, len(line)):
    acc.append(acc[-1] + (line[i] - line[i - 1]).length)
length = acc[-1]
side = max(abs(p.x - root.x) for p in line)
rise = line[-1].z - root.z


def point(s):
    for i in range(len(line) - 1):
        if s <= acc[i + 1] or i == len(line) - 2:
            return line[i].lerp(line[i + 1], (s - acc[i]) / (acc[i + 1] - acc[i]))


def along(p):
    best = (1e9, 0.0)
    for i in range(len(line) - 1):
        a, b = line[i], line[i + 1]
        t = max(0.0, min(1.0, (p - a).dot(b - a) / (b - a).length_squared))
        d = (p - a.lerp(b, t)).length_squared
        if d < best[0]:
            best = (d, acc[i] + t * (acc[i + 1] - acc[i]))
    return best[1]


joints_s = [length * k / BONES for k in range(BONES + 1)]
mids_s = [(joints_s[k] + joints_s[k + 1]) * 0.5 for k in range(BONES)]
for v in tail:
    s, b = along(world[v]), share[v]
    d = v[deform]
    total = sum(d.values())
    if total <= 0.0:   # (nothing came over: the nearest old skin was the old tail's)
        d[group_index[prefix + "Hips"]] = total = 1.0
    for g in list(d.keys()):
        d[g] = d[g] / total * (1.0 - b) if total > 0.0 and b < 0.999 else 0.0
        if d[g] <= 0.0:
            del d[g]
    if b < 0.001:
        continue
    if s <= mids_s[0]:
        d[group_index[names[0]]] = b
    elif s >= mids_s[-1]:
        d[group_index[names[-1]]] = b
    else:
        k = max(i for i in range(BONES - 1) if mids_s[i] <= s)
        t = (s - mids_s[k]) / (mids_s[k + 1] - mids_s[k])
        if t < 0.999:
            d[group_index[names[k]]] = b * (1.0 - t)
        if t > 0.001:
            d[group_index[names[k + 1]]] = b * t
bm.to_mesh(new.data)
bm.free()
print(f"RESKIN tail {len(tail)} vertices, {length * 100:.0f} cm from {tuple(round(c, 3) for c in root)} to {tuple(round(c, 3) for c in line[-1])}; "
      f"off the middle by at most {side * 100:.1f} cm, rises {rise * 100:.1f} cm")

# ---- the armature: the old tail chain out, the new one in; the new mesh in the old one's place ----
bpy.ops.object.select_all(action="DESELECT")
bpy.context.view_layer.objects.active = arm
bpy.ops.object.mode_set(mode="EDIT")
to_arm = arm.matrix_world.inverted()
edit_bones = arm.data.edit_bones
for b in [b for b in edit_bones if b.name.startswith(prefix + "Tail")]:
    edit_bones.remove(b)
parent = edit_bones[prefix + "Hips"]
for k, n in enumerate(names):
    bone = edit_bones.new(n)
    bone.head = to_arm @ point(joints_s[k])
    bone.tail = to_arm @ point(joints_s[k + 1])
    bone.align_roll(parent.z_axis)
    bone.parent = parent
    bone.use_connect = k > 0
    parent = bone
bpy.ops.object.mode_set(mode="OBJECT")
for o in olds:
    bpy.data.objects.remove(o)
new.name = NAME
new.data.name = NAME
if new.data.materials:
    new.data.materials[0].name = NAME
mod = new.modifiers.new("Armature", "ARMATURE")
mod.object = arm
for poly in new.data.polygons:
    poly.use_smooth = True
bpy.ops.export_scene.fbx(
    filepath=out_fbx, use_selection=False, object_types={"ARMATURE", "MESH"}, add_leaf_bones=False, bake_anim=False,
    primary_bone_axis="Y", secondary_bone_axis="X", use_armature_deform_only=False, mesh_smooth_type="OFF",
    path_mode="STRIP")
print(f"RESKIN wrote {out_fbx}: {len(new.data.vertices)} vertices, {len(new.data.polygons)} faces, {len(arm.data.bones)} bones")
