"""Give a Mixamo-rigged animal a tail rig (run in Blender): a chain of tail bones off the hips along the tail's centerline,
with the tail skinned to it. blender_fox_tail.py did this for the fox with hand-picked vertex bounds; this one picks the
tail by distance, so it works for tails that curl up the back (the squirrel) or swing out to a side (the tiger).

Usage: blender -b --factory-startup --python blender_animal_tail.py -- <in fbx> <out fbx> <joints> [RADIUS] [BODY_SHARE] [SEAM]
       [NAME] [PARENT] [CONNECTED]
  <joints>: the centerline from the root to the tip, "x,y,z;x,y,z;..." in Blender's units of the imported Mixamo FBX
  (meters; the character faces -Y, so +Y is behind it, +X is its left, Z up). Read them off blender_rest_grid.py's
  pictures. N joints make N-1 bones mixamorig:Tail1.., the code (URRRandomBodyAnimInstance) bends Tail1..Tail4.
  NAME and PARENT (default Tail, Hips) make the same kind of chain for another part: the dragon's wings are
  WingL1.. / WingR1.. off Spine2, each run adding one chain to the FBX the run before wrote.
  CONNECTED 1 keeps only the picked vertices joined to the part's tip through other picked ones: what passes the tests
  but only touches the part through the body (the dragon's head fins, level with its wings) stays as it was.

A vertex is tail if it is nearer the centerline than RADIUS and nearer it than BODY_SHARE times its distance to every
other bone (the auto-rig's tail weights went to whatever it was near: the squirrel's mostly to its left arm and neck).
Tail vertices then follow the tail bones only, by where they fall along the centerline: each bone owns its stretch,
blending into the next over JOINT_BLEND, and the first ROOT_BLEND of the tail blends into the hips. Everything else,
and the rest of the rig, stays as it was; no animation is exported (in Unreal the mesh goes onto its own skeleton,
which gains the tail bones, and the Mixamo animations don't move them).
"""
import sys
from collections import defaultdict

import bpy
from mathutils import Vector
from mathutils.geometry import intersect_point_line

args = sys.argv[sys.argv.index("--") + 1:]
src, out = args[0], args[1]
JOINTS = [Vector(tuple(float(c) for c in p.split(","))) for p in args[2].split(";")]
RADIUS = float(args[3]) if len(args) > 3 else 0.2
BODY_SHARE = float(args[4]) if len(args) > 4 else 0.85
# Where the tail's surface runs into another part's (the squirrel's tail is one piece with the back of its head), the
# faces across the seam stretch as the two move apart: weights within SEAM (m) of those faces are smoothed across it so
# the stretch spreads out. 0 leaves the seam as it is.
SEAM = float(args[5]) if len(args) > 5 else 0.0
NAME = args[6] if len(args) > 6 else "Tail"
PARENT = args[7] if len(args) > 7 else "Hips"
CONNECTED = len(args) > 8 and args[8] == "1"
JOINT_BLEND = 0.03
ROOT_BLEND = 0.05
PREFIX = "mixamorig:"


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

# The other bones as segments, in world space, for the "nearer than the body" test
body = [(arm.matrix_world @ b.head_local, arm.matrix_world @ b.tail_local) for b in arm.data.bones]

# Bones, in armature space
bpy.context.view_layer.objects.active = arm
bpy.ops.object.mode_set(mode="EDIT")
to_arm = arm.matrix_world.inverted()
edit_bones = arm.data.edit_bones
parent = edit_bones[prefix + PARENT]
names = []
for i in range(len(JOINTS) - 1):
    bone = edit_bones.new(f"{prefix}{NAME}{i + 1}")
    bone.head = to_arm @ JOINTS[i]
    bone.tail = to_arm @ JOINTS[i + 1]
    bone.align_roll(parent.z_axis)
    bone.parent = parent
    bone.use_connect = i > 0
    parent = bone
    names.append(bone.name)
bpy.ops.object.mode_set(mode="OBJECT")

# Length along the centerline at each joint
along = [0.0]
for i in range(1, len(JOINTS)):
    along.append(along[-1] + (JOINTS[i] - JOINTS[i - 1]).length)
total = along[-1]


def segment_distance(p, a, b):
    closest, t = intersect_point_line(p, a, b)
    t = max(0.0, min(1.0, t))
    return (p - (a + (b - a) * t)).length, t


def on_tail(p):
    """Distance to the centerline and how far along it (0 .. total) the nearest point is."""
    best = (1e9, 0.0)
    for i in range(len(JOINTS) - 1):
        d, t = segment_distance(p, JOINTS[i], JOINTS[i + 1])
        if d < best[0]:
            best = (d, along[i] + t * (along[i + 1] - along[i]))
    return best


groups = {n: mesh.vertex_groups.get(n) or mesh.vertex_groups.new(name=n) for n in names + [prefix + PARENT]}
hips = groups[prefix + PARENT]
picked = {}
for v in mesh.data.vertices:
    p = mesh.matrix_world @ v.co
    d, s = on_tail(p)
    if d > RADIUS:
        continue
    if d > BODY_SHARE * min(segment_distance(p, a, b)[0] for a, b in body):
        continue
    picked[v.index] = s
if CONNECTED and picked:
    near = defaultdict(list)
    for e in mesh.data.edges:
        a, b = e.vertices
        if a in picked and b in picked:
            near[a].append(b)
            near[b].append(a)
    tip = min(picked, key=lambda i: ((mesh.matrix_world @ mesh.data.vertices[i].co) - JOINTS[-1]).length)
    keep, todo = {tip}, [tip]
    while todo:
        for j in near[todo.pop()]:
            if j not in keep:
                keep.add(j)
                todo.append(j)
    print(f"TAIL connected: {len(keep)} of {len(picked)} picked vertices joined to the tip")
    picked = {i: s for i, s in picked.items() if i in keep}
count = 0
for v in mesh.data.vertices:
    if v.index not in picked:
        continue
    s = picked[v.index]
    count += 1
    chain = {}
    for i, name in enumerate(names):
        lo = 1.0 if i == 0 else smoothstep(along[i] - JOINT_BLEND, along[i] + JOINT_BLEND, s)
        # The last bone keeps everything past its start, the tip included
        hi = 0.0 if i == len(names) - 1 else smoothstep(along[i + 1] - JOINT_BLEND, along[i + 1] + JOINT_BLEND, s)
        weight = lo * (1.0 - hi)
        if weight > 1e-4:
            chain[name] = weight
    norm = sum(chain.values())
    root = smoothstep(0.0, ROOT_BLEND, s)
    for g in list(v.groups):
        mesh.vertex_groups[g.group].remove([v.index])
    if root < 1.0:
        hips.add([v.index], 1.0 - root, "REPLACE")
    for name, weight in chain.items():
        groups[name].add([v.index], root * weight / norm, "REPLACE")
print(f"TAIL bones {names} off {PARENT}, {total * 100:.0f} cm long, {count} of {len(mesh.data.vertices)} vertices reskinned")

if SEAM > 0.0:
    group_names = {g.index: g.name for g in mesh.vertex_groups}

    def main_bone(v):
        return group_names[max(v.groups, key=lambda g: g.weight).group] if len(v.groups) else ""

    def kind(name):
        return "tail" if NAME in name else ("root" if name.endswith(PARENT) else name)

    verts = mesh.data.vertices
    kinds = [kind(main_bone(v)) for v in verts]
    seam = set()
    for poly in mesh.data.polygons:
        ks = {kinds[i] for i in poly.vertices}
        # The tail meeting anything but its root
        if "tail" in ks and len(ks - {"tail", "root"}) > 0:
            seam.update(poly.vertices)
    seam_points = [mesh.matrix_world @ verts[i].co for i in seam]
    near = [v.index for v in verts if seam_points and min(((mesh.matrix_world @ v.co) - p).length for p in seam_points) < SEAM]
    for v in verts:
        v.select = False
    for i in near:
        verts[i].select = True
    bpy.context.view_layer.objects.active = mesh
    bpy.ops.object.mode_set(mode="WEIGHT_PAINT")
    mesh.data.use_paint_mask_vertex = True
    bpy.ops.object.vertex_group_smooth(group_select_mode="ALL", factor=0.5, repeat=12, expand=0.0)
    bpy.ops.object.vertex_group_normalize_all(group_select_mode="ALL", lock_active=False)
    bpy.ops.object.mode_set(mode="OBJECT")
    print(f"TAIL seam: {len(seam)} vertices on faces across it, {len(near)} within {SEAM * 100:.0f} cm smoothed")

bpy.ops.export_scene.fbx(
    filepath=out, use_selection=False, object_types={"ARMATURE", "MESH"}, add_leaf_bones=False, bake_anim=False,
    primary_bone_axis="Y", secondary_bone_axis="X", use_armature_deform_only=False, mesh_smooth_type="OFF",
    path_mode="STRIP")
print(f"TAIL wrote {out}")
