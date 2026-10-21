"""Rig a new character on another character's Mixamo rig when Mixamo's auto-rigger can't be had (run in Blender): the
donor's bones move to the new body's joints, and the new mesh takes the donor's skin weights from where it lies on the
donor body bent to the same joints. Bone names, parents and axes stay Mixamo's, so Mixamo animations carry over the way
ue_import_shared_anim.py carries them (local rotations; every skeleton keeps its own bone lengths).

Made for the red fox (the fox girl, round 68) on 2026-10-21, when the auto-rigger failed with "Unknown error while
generating motion" on every upload, the tiger that rigged fine on 2026-10-14 included (as on 2026-10-15, round 44).

Usage: blender -b --factory-startup --python blender_fit_rig.py -- <donor rig fbx> <donor obj> <new obj> <joints txt>
       <out fbx> [preview prefix]
  <donor rig fbx>: a Mixamo "With Skin" export (input/tiger/mixamo/tiger_SK.fbx), its mesh the whole of <donor obj>
  (blender_tripo_to_obj.py's OBJ that went to Mixamo). <new obj>: the new character's blender_tripo_to_obj.py OBJ.
  <joints txt>: one bone per line, "<bone without mixamorig:> x y z" where its head goes, in the donor rig's Blender
  frame (meters; it faces -Y, +X is its left, Z up). "MIRROR <x>" mirrors every Right* bone with no Left* line across
  x = <x> (the body's middle). Read the joints off pictures of the new body (front, side, close-ups on a grid).

How: a thin-plate spline through the donor's joints to the new ones bends the donor's mesh into roughly the new body;
each new vertex takes the weights of the nearest point of that bent surface (among nearby faces facing its way, so an
arm's inside doesn't pick up the body's side), four biggest kept. The bones only move: each keeps the donor's
orientation (see below), so the donor's Mixamo motions play on the new body as the same turns from its own rest pose.
The preview colors each vertex by its strongest bone
and renders the rest pose and a test pose (arms up, a step, head turned, bent at the waist) from the front and side.
"""
import colorsys
import math
import sys

import bpy
import numpy as np
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree
from mathutils.geometry import barycentric_transform

args = sys.argv[sys.argv.index("--") + 1:]
donor_fbx, donor_obj, new_obj, joints_txt, out = args[:5]
preview = args[5] if len(args) > 5 else None
MAX_INFLUENCES = 4

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=donor_fbx)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
arm.animation_data_clear()
for action in list(bpy.data.actions):
    bpy.data.actions.remove(action)
arm.data.pose_position = "REST"
old = next(o for o in bpy.data.objects if o.type == "MESH")
prefix = next(b.name for b in arm.data.bones if b.name.endswith("Hips")).replace("Hips", "")

# The joints, by bone name without the prefix
targets, mirror_x = {}, None
for line in open(joints_txt, encoding="utf-8"):
    parts = line.split("#")[0].split()
    if not parts:
        continue
    if parts[0] == "MIRROR":
        mirror_x = float(parts[1])
    else:
        targets[parts[0]] = Vector(tuple(float(c) for c in parts[1:4]))
if mirror_x is not None:
    for name, p in list(targets.items()):
        twin = "Left" + name[5:] if name.startswith("Right") else None
        if twin and twin not in targets:
            targets[twin] = Vector((2 * mirror_x - p.x, p.y, p.z))
missing = [b.name for b in arm.data.bones if b.name[len(prefix):] not in targets]
if missing:
    print(f"FITRIG no joint given for {missing}: they follow the spline")

w2a = arm.matrix_world.inverted()


def head_of(b):
    return arm.matrix_world @ b.head_local


# Thin-plate spline (kernel r, plus an affine part) from the donor's joints to the new ones
src = np.array([head_of(b)[:] for b in arm.data.bones if b.name[len(prefix):] in targets])
dst = np.array([targets[b.name[len(prefix):]][:] for b in arm.data.bones if b.name[len(prefix):] in targets])
keep = [i for i in range(len(src)) if all(np.linalg.norm(src[i] - src[j]) > 1e-4 for j in range(i))]
src, dst = src[keep], dst[keep]
n = len(src)
K = np.linalg.norm(src[:, None] - src[None], axis=2)
P = np.hstack([np.ones((n, 1)), src])
A = np.zeros((n + 4, n + 4))
A[:n, :n] = K + 1e-6 * np.eye(n)
A[:n, n:] = P
A[n:, :n] = P.T
coef = np.linalg.solve(A, np.vstack([dst, np.zeros((4, 3))]))


def warp(points):
    points = np.atleast_2d(points)
    k = np.linalg.norm(points[:, None] - src[None], axis=2)
    return k @ coef[:n] + np.hstack([np.ones((len(points), 1)), points]) @ coef[n:]


# The donor's body, bent onto the new joints
old_pts = np.array([(old.matrix_world @ v.co)[:] for v in old.data.vertices])
bent = warp(old_pts)
tris = [tuple(p.vertices) for p in old.data.polygons]
tree = BVHTree.FromPolygons([Vector(p) for p in bent], tris)


def bounds(ps):
    return (Vector((min(p.x for p in ps), min(p.y for p in ps), min(p.z for p in ps))),
            Vector((max(p.x for p in ps), max(p.y for p in ps), max(p.z for p in ps))))


def load_obj(path):
    before = set(bpy.data.objects)
    bpy.ops.wm.obj_import(filepath=path, forward_axis="NEGATIVE_Z", up_axis="Y")
    return next(o for o in bpy.data.objects if o not in before)


# The OBJs go into the rig's frame the way the donor OBJ maps onto the donor rig's mesh
ref = load_obj(donor_obj)
lo_r, hi_r = bounds([Vector(p) for p in old_pts])
lo_o, hi_o = bounds([ref.matrix_world @ v.co for v in ref.data.vertices])
scale = (hi_r.z - lo_r.z) / (hi_o.z - lo_o.z)
to_rig = Matrix.Translation(lo_r) @ Matrix.Scale(scale, 4) @ Matrix.Translation(-lo_o)
bpy.data.objects.remove(ref, do_unlink=True)
height = hi_r.z - lo_r.z
new = load_obj(new_obj)
new.data.transform(to_rig @ new.matrix_world)
new.matrix_world = Matrix.Identity(4)
new.data.update()
dists = sorted(tree.find_nearest(v.co)[3] / height for v in new.data.vertices)
print(f"FITRIG x{scale:.5f}; new body off the bent donor body (of the height): median {dists[len(dists) // 2]:.4f}"
      f" p90 {dists[int(len(dists) * 0.9)]:.4f} max {dists[-1]:.4f}")

# The bones: heads onto the joints, each bone turned exactly as the donor's (only moved). The animations carry over as
# local rotations, which are the donor's rest rotations times the motion: a bone pointed anew (head to its child's new
# head) would get the donor's direction back on top, so the red fox's head drooped 18 degrees (the tiger's head bone leans
# forward to its big head's top) and her run hunched over. Kept as the donor's, every bone turns from her own rest pose
# by what the donor's turns from its.
bpy.context.view_layer.objects.active = arm
bpy.ops.object.mode_set(mode="EDIT")
eb = arm.data.edit_bones
heads = {b.name: arm.matrix_world @ b.head for b in eb}
for b in eb:
    name = b.name[len(prefix):]
    h = targets.get(name, Vector(warp(np.array(heads[b.name][:]))[0]))
    offset = w2a @ h - b.head
    roll = b.roll
    b.use_connect = False
    b.head = b.head + offset
    b.tail = b.tail + offset
    b.roll = roll
bpy.ops.object.mode_set(mode="OBJECT")
worst = max((head_of(arm.data.bones[prefix + k]) - v).length for k, v in targets.items() if prefix + k in arm.data.bones)
print(f"FITRIG {len(arm.data.bones)} bones moved; heads off their joints by at most {worst * 1000:.2f} mm")

# Weights from the nearest bent donor face that faces the same way
old_groups = {g.index: g.name for g in old.vertex_groups}
weights_of = [{old_groups[g.group]: g.weight for g in v.groups} for v in old.data.vertices]
groups = {name: new.vertex_groups.new(name=name) for name in old_groups.values()}
turned = 0
for v in new.data.vertices:
    loc, normal, face, dist = tree.find_nearest(v.co)
    if normal.dot(v.normal) < 0.2:
        best = None
        for l2, n2, f2, d2 in tree.find_nearest_range(v.co, dist * 1.6 + 0.01 * height):
            if n2.dot(v.normal) >= 0.2 and (best is None or d2 < best[3]):
                best = (l2, n2, f2, d2)
        if best:
            loc, normal, face, dist = best
            turned += 1
    corners = tris[face][:3]
    a, b, c = (Vector(bent[i]) for i in corners)
    bary = barycentric_transform(loc, a, b, c, Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1)))
    mix = {}
    for i, share in zip(corners, bary):
        share = max(0.0, share)
        for name, w in weights_of[i].items():
            mix[name] = mix.get(name, 0.0) + w * share
    top = sorted(mix.items(), key=lambda kv: -kv[1])[:MAX_INFLUENCES]
    total = sum(w for _, w in top) or 1.0
    for name, w in top:
        if w / total > 0.005:
            groups[name].add([v.index], w / total, "REPLACE")
print(f"FITRIG {len(new.data.vertices)} vertices weighted ({turned} from a face behind the nearest one)")

# Limbs along their own joints. The donor's skin (Mixamo's auto-rig) gives a chibi's whole shin to the foot bone and the
# lower forearm to the hand, which only works with the donor's own proportions: on the remade fox, rabbit, squirrel and
# tiger the shins folded as the ankles turned and the forearms creased ("발 위치를 너무 높게 잡아서 폴리곤이 접혀보임. 팔도
# 마찬가지", 2026-10-21). Each vertex keeps how much it follows a limb in all, and that share is spread again by where it
# lies along the limb's joints: each bone owns its stretch, blending into the next over LIMB_BLEND either side of a joint.
# Below the ankle only the foot and toe follow; past the wrist the hand and fingers keep the donor's split.
LIMB_BLEND = 0.012


def chain_points(names):
    return [arm.matrix_world @ arm.data.bones[prefix + n].head_local for n in names]


def along(p, pts):
    """Arc length of p's projection onto the polyline pts (nearest segment), and that segment's index."""
    best, s0, out_s, out_k = None, 0.0, 0.0, 0
    for k in range(len(pts) - 1):
        a, b = pts[k], pts[k + 1]
        ab = b - a
        t = max(0.0, min(1.0, (p - a).dot(ab) / max(ab.length_squared, 1e-12)))
        d = (a + ab * t - p).length
        if best is None or d < best:
            best, out_s, out_k = d, s0 + t * ab.length, k
        s0 += ab.length
    return out_s, out_k


def spread(s, joints_s, n_bones):
    """Weights of n_bones bones over arc length s: bone k owns [joints_s[k], joints_s[k+1]], blended at joints."""
    w = [0.0] * n_bones
    for k in range(n_bones):
        lo = joints_s[k] - LIMB_BLEND if k > 0 else -1e9
        hi = joints_s[k + 1] + LIMB_BLEND if k < n_bones - 1 else 1e9
        if lo <= s <= hi:
            a = 1.0 if k == 0 else min(1.0, (s - lo) / (2 * LIMB_BLEND))
            b = 1.0 if k == n_bones - 1 else min(1.0, (hi - s) / (2 * LIMB_BLEND))
            w[k] = max(0.0, min(a, b))
    total = sum(w) or 1.0
    return [x / total for x in w]


group_index = {g.name: g.index for g in new.vertex_groups}
limb_fixed = 0
for side in ("Left", "Right"):
    for kind, joints, bones, all_bones in (
            ("leg", ["UpLeg", "Leg", "Foot", "ToeBase", "Toe_End"], ["UpLeg", "Leg", "Foot", "ToeBase"],
             ["UpLeg", "Leg", "Foot", "ToeBase", "Toe_End"]),
            ("arm", ["Arm", "ForeArm", "Hand", "HandIndex1"], ["Arm", "ForeArm", "Hand"],
             ["Arm", "ForeArm", "Hand"] + [f"Hand{f}{i}" for f in ("Thumb", "Index") for i in range(1, 5)])):
        if any(prefix + side + n not in arm.data.bones for n in joints):
            continue
        pts = chain_points([side + n for n in joints])
        js = [0.0]
        for k in range(len(pts) - 1):
            js.append(js[-1] + (pts[k + 1] - pts[k]).length)
        names = [prefix + side + n for n in all_bones if prefix + side + n in group_index]
        idx_of = {group_index[n]: n for n in names}
        ankle_z = pts[2].z
        for v in new.data.vertices:
            limb = {idx_of[g.group]: g.weight for g in v.groups if g.group in idx_of and g.weight > 0}
            total = sum(limb.values())
            if total < 1e-4:
                continue
            p = new.matrix_world @ v.co
            if kind == "leg" and p.z < ankle_z - 0.005:
                # Sole and toes: the foot chain only
                s, _ = along(p, pts[2:])
                s += js[2]
            else:
                s, _ = along(p, pts)
            if kind == "arm" and s > js[2] + LIMB_BLEND:
                continue  # the hand and fingers as the donor had them
            w = spread(s, js, len(bones))
            for n in names:
                groups[n].remove([v.index])
            for n, x in zip(bones, w):
                if x * total > 0.002:
                    if prefix + side + n not in groups:
                        groups[prefix + side + n] = new.vertex_groups.new(name=prefix + side + n)
                    groups[prefix + side + n].add([v.index], x * total, "REPLACE")
            limb_fixed += 1
print(f"FITRIG limbs reskinned along their joints: {limb_fixed} vertices")

old_name = old.name
bpy.data.objects.remove(old, do_unlink=True)
new.name = old_name
new.parent = arm
new.matrix_parent_inverse = arm.matrix_world.inverted()
mod = new.modifiers.new("Armature", "ARMATURE")
mod.object = arm
bpy.ops.export_scene.fbx(
    filepath=out, use_selection=False, object_types={"ARMATURE", "MESH"}, add_leaf_bones=False, bake_anim=False,
    primary_bone_axis="Y", secondary_bone_axis="X", use_armature_deform_only=False, mesh_smooth_type="OFF",
    path_mode="STRIP")
print(f"FITRIG wrote {out}")

if preview:
    # Each vertex in its strongest bone's color
    names = sorted(groups)
    color_of = {name: colorsys.hsv_to_rgb((i * 0.618) % 1.0, 0.65, 0.95) for i, name in enumerate(names)}
    index_name = {g.index: g.name for g in new.vertex_groups}
    attr = new.data.color_attributes.new("Bone", "FLOAT_COLOR", "POINT")
    for v in new.data.vertices:
        g = max(v.groups, key=lambda g: g.weight, default=None)
        rgb = color_of[index_name[g.group]] if g else (0.2, 0.2, 0.2)
        attr.data[v.index].color = (*rgb, 1.0)
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "VERTEX"
    scene.render.resolution_x = scene.render.resolution_y = 600
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    scene.collection.objects.link(cam)
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = height * 1.15
    scene.camera = cam
    mid = (lo_r + hi_r) / 2
    arm.data.pose_position = "POSE"
    pb = arm.pose.bones
    poses = {
        "rest": [],
        "pose": [("LeftArm", "x", -60), ("RightArm", "x", -60), ("LeftForeArm", "z", 40), ("RightForeArm", "z", -40),
                 ("LeftUpLeg", "x", -45), ("LeftLeg", "x", 60), ("RightUpLeg", "x", 20), ("Spine1", "x", 20),
                 ("Head", "y", 30), ("RightHandIndex1", "x", 50), ("RightHandIndex2", "x", 50)],
    }
    for pose, turns in poses.items():
        for b in pb:
            b.rotation_mode = "XYZ"
            b.rotation_euler = (0, 0, 0)
        for bone, axis, deg in turns:
            b = pb.get(prefix + bone)
            if b:
                setattr(b.rotation_euler, axis, math.radians(deg))
        bpy.context.view_layer.update()
        for view, yaw in (("front", 0), ("side", 90), ("back", 180)):
            r = math.radians(yaw)
            cam.location = Vector((mid.x + math.sin(r) * 5 * height, mid.y - math.cos(r) * 5 * height, mid.z))
            cam.rotation_euler = (math.radians(90), 0, r)
            cam.data.clip_end = 20 * height
            scene.render.filepath = f"{preview}_{pose}_{view}.png"
            bpy.ops.render.render(write_still=True)
    print("FITRIG preview done")
