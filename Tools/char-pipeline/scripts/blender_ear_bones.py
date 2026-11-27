"""Give an animal friend's ears (or fins) bones of their own, under the head (2026-11-25, the user: "다람쥐는 작업에서
제외하고 나머지는 권장하는 순서로 작업해서 적용해줘" - fox, red fox, rabbit, tiger, dragon).

Usage: blender -b -P blender_ear_bones.py -- <rig fbx> <out fbx> --ear "<side>|<base x,y,z>|<tip x,y,z>|<bones>" [--ear ...]
           [--blend 0.3] [--reach 0.8] [--head cx,cy,cz,rx,ry,rz] [--apart 0.03] [--check <png prefix>]

<rig fbx>: the animal friend's game rig (armature + its mesh or meshes, e.g. Body + Muzzle), in Blender's frame (the
model faces -Y). Each --ear: <side> L or R (the animal's own left or right, checked against its LeftArm bone), a point in
the middle of where the ear meets the head and a point at its tip (read off blender_head_grid.py's pictures), and how
many bones. The ear is the surface on the tip's side of the plane through the base point square to base -> tip, joined
to the vertex nearest the tip, no further from the base -> tip line than --reach times its length (so the plane cannot
run on into the head). Along it, the distance over the surface from the edge where it meets the head; its middle line =
the centres of 12 rings of that distance (follows a folded ear). Bones <prefix>Ear<side>1..N along that line, parented
to the head, and an unweighted <prefix>Ear<side>End at the tip (the game reads the ear's direction from it). Skin:
shared between neighbouring bones as on the tails, and over the first --blend of the ear's length from its edge with
the weights it had (the head's), so the bend there is soft. Meshes that do not reach the tip are left as they were.

Writes <out fbx> (the same export settings as blender_reskin.py). --check: also renders the head from the front and the
side with each ear's skin painted (bone by bone, alternating colours) to <png prefix>_front.png / _side.png.
"""
import heapq
import math
import sys

import bmesh
import bpy
from mathutils import Vector, kdtree

args = sys.argv[sys.argv.index("--") + 1:]
rig_fbx, out_fbx = args[:2]


def opt(name, default):
    return type(default)(args[args.index(name) + 1]) if name in args else default


EARS = [args[i + 1] for i, a in enumerate(args) if a == "--ear"]
BLEND = opt("--blend", 0.3)
REACH = opt("--reach", 0.8)
CHECK = opt("--check", "")
# (2026-11-25, the rabbit's folded ear hangs beside its head: a plane and a reach alone cannot cut it off) the skull as an
# ellipsoid "cx,cy,cz,rx,ry,rz" nothing inside of which is ear, and how far each ear's skin must stay on its own side of
# the head bone's middle (so the crown between two ears joins neither)
HEAD = [float(c) for c in opt("--head", "").split(",")] if "--head" in args else None
APART = opt("--apart", -1.0)
# (the dragon's fins sit right under its horns) half-spaces "nx,ny,nz,px,py,pz" that are never ear: the side n points to of the
# plane through p (repeat --cut for more)
CUTS = [[float(c) for c in args[i + 1].split(",")] for i, a in enumerate(args) if a == "--cut"]


def vec(s):
    return Vector([float(c) for c in s.split(",")])


def smoothstep(e0, e1, x):
    t = max(0.0, min(1.0, (x - e0) / (e1 - e0))) if e1 > e0 else 1.0
    return t * t * (3.0 - 2.0 * t)


bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=rig_fbx)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
arm.animation_data_clear()
arm.data.pose_position = "REST"
meshes = [o for o in bpy.data.objects if o.type == "MESH"]
head_bone = next(b for b in arm.data.bones if b.name.endswith("Head"))
prefix = head_bone.name[:-len("Head")]
left_arm = arm.data.bones.get(prefix + "LeftArm")
left_x = (arm.matrix_world @ left_arm.head_local).x - (arm.matrix_world @ head_bone.head_local).x if left_arm else None
print(f"EARS rig {rig_fbx}: {len(arm.data.bones)} bones, prefix '{prefix}', meshes {[m.name for m in meshes]}, "
      f"the animal's left is {'+' if (left_x or 0) > 0 else '-'}X")

plans = []   # (side, names, end name, joints in world, per mesh {vertex index: (s, share)})
for spec in EARS:
    side, base_s, tip_s, nb = spec.split("|")
    base, tip, nb = vec(base_s), vec(tip_s), int(nb)
    axis = tip - base
    length0 = axis.length
    u = axis / length0
    if left_x is not None and (side == "L") != ((tip.x - (arm.matrix_world @ head_bone.head_local).x) * left_x > 0):
        print(f"EARS WARNING ear {side}: its tip is on the animal's other side")
    hit = None
    for m in meshes:
        mw = m.matrix_world
        co = [mw @ v.co for v in m.data.vertices]
        near = min(range(len(co)), key=lambda i: (co[i] - tip).length_squared)
        d = (co[near] - tip).length
        if hit is None or d < hit[2]:
            hit = (m, co, d, near)
    m, co, d, seed = hit
    mid_x = (arm.matrix_world @ head_bone.head_local).x
    if d > 0.03:
        raise SystemExit(f"ear {side}: no mesh within 3 cm of the tip {tuple(tip)} (nearest {d:.3f})")
    bm = bmesh.new()
    bm.from_mesh(m.data)
    bm.verts.ensure_lookup_table()

    def inside(i):
        p = co[i] - base
        along = p.dot(u)
        if along <= 0.0 or (p - u * along).length >= REACH * length0:
            return False
        if HEAD and sum(((co[i][k] - HEAD[k]) / HEAD[3 + k]) ** 2 for k in range(3)) < 1.0:
            return False
        if any(sum((co[i][k] - c[3 + k]) * c[k] for k in range(3)) > 0.0 for c in CUTS):
            return False
        return APART < 0.0 or (co[i].x - mid_x) * (1.0 if tip.x > mid_x else -1.0) > APART

    region = {seed}
    todo = [seed]
    while todo:
        i = todo.pop()
        for e in bm.verts[i].link_edges:
            j = e.other_vert(bm.verts[i]).index
            if j not in region and inside(j):
                region.add(j)
                todo.append(j)
    # Loose pieces lying against the ear (not joined to it by an edge: the rabbit's upright ear has a spike at its tip)
    # go with it, at the distance of the ear skin nearest them - left on the head, they tore off when the ear moved
    near_tree = kdtree.KDTree(len(region))
    for i in region:
        near_tree.insert(co[i], i)
    near_tree.balance()
    loose = {}
    for i in range(len(co)):
        if i in region or i in loose or not inside(i):
            continue
        _, j, d = near_tree.find(co[i])
        if d > 0.015:
            continue
        piece, todo = {i}, [i]
        while todo:
            k = todo.pop()
            for e in bm.verts[k].link_edges:
                n = e.other_vert(bm.verts[k]).index
                if n not in piece and n not in region and inside(n):
                    piece.add(n)
                    todo.append(n)
        for k in piece:
            loose[k] = near_tree.find(co[k])[1]
    # distance over the surface from the edge where the ear meets the head
    dist = {}
    heap = []
    for i in region:
        if any(e.other_vert(bm.verts[i]).index not in region for e in bm.verts[i].link_edges):
            dist[i] = max(0.0, (co[i] - base).dot(u))
            heapq.heappush(heap, (dist[i], i))
    while heap:
        di, i = heapq.heappop(heap)
        if di > dist[i]:
            continue
        for e in bm.verts[i].link_edges:
            j = e.other_vert(bm.verts[i]).index
            if j in region:
                nd = di + (co[i] - co[j]).length
                if nd < dist.get(j, 1e9):
                    dist[j] = nd
                    heapq.heappush(heap, (nd, j))
    bm.free()
    s_max = max(dist.values())
    for k, j in loose.items():
        dist[k] = dist.get(j, 0.0)
    region |= set(loose)
    rings = 12
    acc = [[Vector(), 0] for _ in range(rings)]
    for i, s in dist.items():
        k = min(rings - 1, int(s / s_max * rings))
        acc[k][0] += co[i]
        acc[k][1] += 1
    line = [a[0] / a[1] for a in acc if a[1]]
    far = max(dist, key=dist.get)
    line.append(co[far])
    # (start at the middle of the ring where it meets the head, a little under the first ring's centre)
    seg = [0.0]
    for a, b in zip(line, line[1:]):
        seg.append(seg[-1] + (b - a).length)
    length = seg[-1]

    def point(t):
        x = t * length
        for k in range(len(line) - 1):
            if x <= seg[k + 1] or k == len(line) - 2:
                return line[k].lerp(line[k + 1], (x - seg[k]) / max(seg[k + 1] - seg[k], 1e-9))

    joints = [point(k / nb) for k in range(nb + 1)]
    names = [f"{prefix}Ear{side}{k + 1}" for k in range(nb)]
    weights = {i: (dist[i] / s_max, smoothstep(0.0, BLEND, dist[i] / s_max)) for i in region}
    plans.append((side, names, f"{prefix}Ear{side}End", joints, m, weights))
    print(f"EARS ear {side}: {len(region)} vertices of {m.name}, ({len(loose)} loose) {length * 100:.1f} cm along its middle "
          f"({s_max * 100:.1f} cm over the surface), {nb} bone(s), from {tuple(round(c, 3) for c in joints[0])} "
          f"to {tuple(round(c, 3) for c in joints[-1])}")

# ---- the bones ----
bpy.ops.object.select_all(action="DESELECT")
bpy.context.view_layer.objects.active = arm
bpy.ops.object.mode_set(mode="EDIT")
to_arm = arm.matrix_world.inverted()
eb = arm.data.edit_bones
for b in [b for b in eb if b.name.startswith(prefix + "Ear")]:
    eb.remove(b)
head_eb = eb[head_bone.name]
for side, names, end, joints, m, weights in plans:
    parent = head_eb
    for k, n in enumerate(names + [end]):
        bone = eb.new(n)
        if k < len(names):
            bone.head, bone.tail = to_arm @ joints[k], to_arm @ joints[k + 1]
        else:
            d = (joints[-1] - joints[-2]) * 0.25
            bone.head, bone.tail = to_arm @ joints[-1], to_arm @ (joints[-1] + d)
        bone.align_roll(head_eb.z_axis)
        bone.parent = parent
        bone.use_connect = k > 0
        bone.use_deform = k < len(names)
        parent = bone
bpy.ops.object.mode_set(mode="OBJECT")

# ---- the skin ----
for side, names, end, joints, m, weights in plans:
    for n in names:
        if n not in m.vertex_groups:
            m.vertex_groups.new(name=n)
    gi = {g.name: g.index for g in m.vertex_groups}
    groups = list(m.vertex_groups)
    nb = len(names)
    mids = [(k + 0.5) / nb for k in range(nb)]
    for i, (t, b) in weights.items():
        v = m.data.vertices[i]
        old = {g.group: g.weight for g in v.groups if g.weight > 0.0}
        total = sum(old.values())
        if total <= 0.0:
            old, total = {gi[head_bone.name]: 1.0}, 1.0
        new = {g: w / total * (1.0 - b) for g, w in old.items()}
        if t <= mids[0]:
            new[gi[names[0]]] = new.get(gi[names[0]], 0.0) + b
        elif t >= mids[-1]:
            new[gi[names[-1]]] = new.get(gi[names[-1]], 0.0) + b
        else:
            k = max(j for j in range(nb - 1) if mids[j] <= t)
            f = (t - mids[k]) / (mids[k + 1] - mids[k])
            new[gi[names[k]]] = new.get(gi[names[k]], 0.0) + b * (1.0 - f)
            new[gi[names[k + 1]]] = new.get(gi[names[k + 1]], 0.0) + b * f
        for g in old:
            groups[g].remove([i])
        for g, w in new.items():
            if w > 1e-4:
                groups[g].add([i], w, "REPLACE")

for o in meshes:
    if not any(md.type == "ARMATURE" for md in o.modifiers):
        md = o.modifiers.new("Armature", "ARMATURE")
        md.object = arm
bpy.ops.export_scene.fbx(
    filepath=out_fbx, use_selection=False, object_types={"ARMATURE", "MESH"}, add_leaf_bones=False, bake_anim=False,
    primary_bone_axis="Y", secondary_bone_axis="X", use_armature_deform_only=False, mesh_smooth_type="OFF",
    path_mode="STRIP")
print(f"EARS wrote {out_fbx}: {len(arm.data.bones)} bones")

if CHECK:
    PAL = [(1.0, 0.15, 0.75, 1.0), (0.1, 0.85, 1.0, 1.0), (1.0, 0.85, 0.1, 1.0), (0.4, 1.0, 0.3, 1.0)]
    for o in meshes:
        o.data.materials.clear()
        grey = bpy.data.materials.new("grey")
        grey.diffuse_color = (0.75, 0.75, 0.75, 1.0)
        o.data.materials.append(grey)
        for c in PAL:
            mt = bpy.data.materials.new("c")
            mt.diffuse_color = c
            o.data.materials.append(mt)
        gi = {g.name: g.index for g in o.vertex_groups}
        ear_groups = {gi[n]: k for _, names, _, _, mm, _ in plans if mm == o for k, n in enumerate(names) if n in gi}
        for p in o.data.polygons:
            best = {}
            for vi in p.vertices:
                for g in o.data.vertices[vi].groups:
                    if g.group in ear_groups:
                        best[g.group] = best.get(g.group, 0.0) + g.weight
            if best:
                g, w = max(best.items(), key=lambda x: x[1])
                if w / len(p.vertices) > 0.3:
                    p.material_index = 1 + ear_groups[g] % 4
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.color_type = "MATERIAL"
    scene.render.resolution_x = scene.render.resolution_y = 700
    scene.world = bpy.data.worlds.new("w")
    scene.world.color = (0.15, 0.15, 0.17)
    cd = bpy.data.cameras.new("c")
    cd.type = "ORTHO"
    allj = [j for p in plans for j in p[3]]
    mid = sum(allj, Vector()) / len(allj)
    span = max((j - mid).length for j in allj)
    cd.ortho_scale = max(0.35, span * 3.2)
    cam = bpy.data.objects.new("c", cd)
    scene.collection.objects.link(cam)
    scene.camera = cam
    for name, off in (("front", Vector((0, -1, 0))), ("side", Vector((1, 0, 0))), ("back", Vector((0, 1, 0)))):
        cam.location = mid + off * 3.0
        cam.rotation_euler = (-off).to_track_quat("-Z", "Y").to_euler()
        scene.render.filepath = f"{CHECK}_{name}.png"
        bpy.ops.render.render(write_still=True)
    print(f"EARS check pictures {CHECK}_front/side/back.png")
