# Round 101 (the user: "호랑이 방패 모션 / 공격 모션을 트라이포로 어울리게 다시 만들고 이상한 부분은 직접 다듬어줘").
# Tripo Animate gives the body's motion (weight shift, twist, crouch) but keeps both arms tucked at the chest whatever the
# prompt says, so the tiger never swings or raises a shield. This takes each Tripo motion (blender_split_actions.py, on
# Tripo's Mixamo-named fox rig), copies it onto the tiger's own Mixamo rig bone for bone (local rotations as they are: the
# rigs share names and axes, as ue_import_shared_anim.py assumes), and then sets the two arms by hand: at key moments
# each upper arm, forearm and hand points along a direction given in the chest's own frame (F forward, L the tiger's
# left, U up), eased between keys. The right hand carries the hammer, the left forearm the shield.
#
# usage: blender -b --factory-startup --python blender_tiger_shield_arms.py -- <tiger SK fbx> <motions dir> <out dir> [names]
#   reads <motions dir>/<name>.fbx (none for "shield_carry": the rest pose), writes <out dir>/<name>.fbx and <name>_sheet.png
import math
import os
import sys

import bpy
from mathutils import Matrix, Quaternion, Vector

args = sys.argv[sys.argv.index("--") + 1:]
tiger_fbx, motions_dir, out_dir = args[0], args[1], args[2]
only = args[3].split(",") if len(args) > 3 else None
os.makedirs(out_dir, exist_ok=True)


def D(f=0.0, l=0.0, u=0.0):
    """A direction in the chest's frame: forward, the tiger's left, up."""
    return (f, l, u)


# The stance it walks about in: hammer low in the right hand, pointing forward and a little up; the shield on the left
# forearm, held at the left side in front of the hip, its face out to the left and forward
CARRY = {
    "RightArm": D(0.10, -0.42, -0.90), "RightForeArm": D(0.80, -0.25, -0.30), "RightHand": D(0.85, -0.15, 0.35),
    "LeftArm": D(0.05, 0.45, -0.89), "LeftForeArm": D(0.80, 0.05, 0.30), "LeftHand": D(0.80, -0.20, 0.45),
}
# Braced behind the shield: the left forearm level across the chest and chin (the shield's face forward), the hammer
# cocked back by the right shoulder
BLOCK = {
    "RightArm": D(-0.45, -0.55, -0.70), "RightForeArm": D(0.35, -0.30, 0.80), "RightHand": D(0.25, -0.10, 0.95),
    "LeftArm": D(0.90, 0.25, -0.12), "LeftForeArm": D(0.18, -0.80, 0.52), "LeftHand": D(0.10, -0.70, 0.70),
}
# The shield arm during a swing: pulled back to the left side, out of the hammer's way
BACK = {"LeftArm": D(-0.20, 0.50, -0.84), "LeftForeArm": D(0.70, 0.30, 0.45), "LeftHand": D(0.70, 0.10, 0.55)}


def pose(base, **changes):
    made = dict(base)
    made.update(changes)
    return made


SWING = pose(CARRY, **BACK)
MOTIONS = {
    # (time 0..1, pose, spine twist to the tiger's left in degrees, spine lean forward in degrees)
    "shield_carry": {"source": None, "frames": 24, "keys": [(0.0, CARRY, 0, 0), (1.0, CARRY, 0, 0)]},
    "shield_block": {"source": "shield_block", "cut": (40, 71), "keys": [(0.0, BLOCK, 0, 6), (1.0, BLOCK, 0, 6)]},
    # 1: a wide forehand swing, right to left at chest height
    "hammer1": {"source": "hammer1", "keys": [
        (0.00, SWING, 0, 0),
        (0.18, pose(SWING, RightArm=D(-0.35, -0.90, 0.10), RightForeArm=D(-0.45, -0.60, 0.55), RightHand=D(-0.70, -0.45, 0.45)), -28, 0),
        (0.33, pose(SWING, RightArm=D(0.70, -0.65, 0.05), RightForeArm=D(0.97, -0.20, 0.05), RightHand=D(0.95, 0.25, 0.05)), 5, 6),
        (0.45, pose(SWING, RightArm=D(0.90, 0.30, -0.05), RightForeArm=D(0.55, 0.82, -0.05), RightHand=D(0.15, 0.98, 0.00)), 30, 8),
        (0.62, pose(SWING, RightArm=D(0.75, 0.30, -0.45), RightForeArm=D(0.60, 0.70, -0.25), RightHand=D(0.40, 0.85, 0.10)), 22, 4),
        (1.00, SWING, 0, 0)]},
    # 2: the backhand, left to right
    "hammer2": {"source": "hammer2", "keys": [
        (0.00, SWING, 0, 0),
        (0.16, pose(SWING, RightArm=D(0.80, 0.45, -0.10), RightForeArm=D(0.20, 0.90, 0.30), RightHand=D(-0.20, 0.90, 0.35)), 26, 0),
        (0.31, pose(SWING, RightArm=D(0.92, -0.20, 0.00), RightForeArm=D(0.98, -0.05, 0.05), RightHand=D(0.95, -0.25, 0.05)), 0, 6),
        (0.43, pose(SWING, RightArm=D(0.35, -0.90, 0.05), RightForeArm=D(-0.15, -0.95, 0.10), RightHand=D(-0.50, -0.85, 0.10)), -30, 6),
        (0.62, pose(SWING, RightArm=D(0.25, -0.80, -0.50), RightForeArm=D(0.30, -0.85, -0.10), RightHand=D(0.10, -0.90, 0.25)), -20, 2),
        (1.00, SWING, 0, 0)]},
    # 3: up over the head and straight down in front
    "hammer3": {"source": "hammer3", "keys": [
        (0.00, SWING, 0, 0),
        (0.24, pose(SWING, RightArm=D(-0.10, -0.28, 0.95), RightForeArm=D(-0.40, -0.10, 0.90), RightHand=D(-0.70, 0.00, 0.70)), -8, -12),
        (0.36, pose(SWING, RightArm=D(0.50, -0.22, 0.83), RightForeArm=D(0.62, -0.05, 0.78), RightHand=D(0.80, 0.00, 0.60)), 0, 0),
        (0.46, pose(SWING, RightArm=D(0.85, -0.18, -0.48), RightForeArm=D(0.78, -0.02, -0.62), RightHand=D(0.70, 0.00, -0.70)), 6, 26),
        (0.70, pose(SWING, RightArm=D(0.83, -0.18, -0.52), RightForeArm=D(0.76, -0.02, -0.65), RightHand=D(0.72, 0.00, -0.68)), 6, 24),
        (1.00, SWING, 0, 0)]},
}
ARM_BONES = ["RightArm", "RightForeArm", "RightHand", "LeftArm", "LeftForeArm", "LeftHand"]
SPINE_BONES = ["Spine", "Spine1", "Spine2"]
FINGERS = ["Index", "Middle", "Ring", "Pinky"]


def import_fbx(path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=path, automatic_bone_orientation=False)
    new = [o for o in bpy.data.objects if o not in before]
    return next(o for o in new if o.type == "ARMATURE"), [o for o in new if o.type == "MESH"], new


def bone(arm, name):
    return arm.pose.bones.get("mixamorig:" + name) or arm.pose.bones.get(name)


def ease(a):
    return a * a * (3.0 - 2.0 * a)


def blend(keys, t):
    """The pose (directions), twist and lean at time t: eased between the keys on each side."""
    for (t0, p0, y0, l0), (t1, p1, y1, l1) in zip(keys, keys[1:]):
        if t <= t1:
            a = ease(0.0 if t1 <= t0 else max(0.0, min(1.0, (t - t0) / (t1 - t0))))
            made = {}
            for name in ARM_BONES:
                v = Vector(p0[name]).normalized().lerp(Vector(p1[name]).normalized(), a)
                made[name] = v.normalized()
            return made, y0 + (y1 - y0) * a, l0 + (l1 - l0) * a
    return {n: Vector(keys[-1][1][n]).normalized() for n in ARM_BONES}, keys[-1][2], keys[-1][3]


def world_rot(arm, pb):
    return (arm.matrix_world @ pb.matrix).to_quaternion()


def set_world_rot(arm, pb, quat):
    world = arm.matrix_world @ pb.matrix
    made = Matrix.Translation(world.to_translation()) @ quat.to_matrix().to_4x4()
    scale = world.to_scale()
    made = made @ Matrix.Diagonal((scale.x, scale.y, scale.z, 1.0))
    pb.matrix = arm.matrix_world.inverted() @ made
    bpy.context.view_layer.update()


for name, spec in MOTIONS.items():
    if only and name not in only:
        continue
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.render.fps = 24
    tiger, tiger_meshes, _ = import_fbx(tiger_fbx)
    if tiger.animation_data:
        tiger.animation_data_clear()
    for pb in tiger.pose.bones:
        pb.rotation_mode = "QUATERNION"
    source = None
    first, last = 0, spec.get("frames", 48) - 1
    if spec["source"]:
        source, _, source_all = import_fbx(os.path.join(motions_dir, spec["source"] + ".fbx"))
        action = source.animation_data.action
        first, last = int(round(action.frame_range[0])), int(round(action.frame_range[1]))
        if "cut" in spec:
            first, last = spec["cut"]
        for o in source_all:
            if o.type == "MESH":
                o.hide_render = True
        for pb in source.pose.bones:
            pb.rotation_mode = "QUATERNION"
    count = last - first + 1
    bpy.context.view_layer.update()

    # The tiger's frame at rest: left = from its right shoulder to its left, up = world up, forward = left x up
    left = (tiger.matrix_world @ bone(tiger, "LeftArm").bone.head_local) - (tiger.matrix_world @ bone(tiger, "RightArm").bone.head_local)
    left.z = 0.0
    left.normalize()
    up = Vector((0.0, 0.0, 1.0))
    forward = left.cross(up).normalized()
    chest = bone(tiger, "Spine2")
    chest_rest = (tiger.matrix_world @ chest.bone.matrix_local).to_quaternion()
    hips_scale = 1.0
    if source:
        s_hips = (source.matrix_world @ bone(source, "Hips").bone.head_local).z
        t_hips = (tiger.matrix_world @ bone(tiger, "Hips").bone.head_local).z
        hips_scale = (t_hips / s_hips) * (source.matrix_world.to_scale().z / tiger.matrix_world.to_scale().z)
        print("ARMS hips", name, round(s_hips, 4), round(t_hips, 4), round(hips_scale, 3))

    for index in range(count):
        frame = first + index
        out_frame = index
        scene.frame_set(frame)
        # The body: Tripo's motion, bone for bone
        for pb in tiger.pose.bones:
            short = pb.name.split(":")[-1]
            src = bone(source, short) if source else None
            if short in ARM_BONES or any(f in short for f in FINGERS) or "Thumb" in short:
                pb.rotation_quaternion = Quaternion()
            elif src is not None:
                pb.rotation_quaternion = src.rotation_quaternion.copy()
                if short == "Hips":
                    pb.location = src.location * hips_scale
            else:
                pb.rotation_quaternion = Quaternion()
        bpy.context.view_layer.update()
        t = index / max(count - 1, 1)
        dirs, twist, lean = blend(spec["keys"], t)
        # A little more twist and lean through the spine, on top of Tripo's
        for share, short in zip((0.3, 0.35, 0.35), SPINE_BONES):
            pb = bone(tiger, short)
            turn = Quaternion(up, math.radians(twist * share)) @ Quaternion(left, math.radians(lean * share))
            set_world_rot(tiger, pb, turn @ world_rot(tiger, pb))
        # The arms: each bone along its direction in the chest's frame (from its rest pose, so its roll stays steady)
        chest_turn = world_rot(tiger, chest) @ chest_rest.inverted()
        for short in ARM_BONES:
            pb = bone(tiger, short)
            want = chest_turn @ (forward * dirs[short][0] + left * dirs[short][1] + up * dirs[short][2])
            now = world_rot(tiger, pb)
            along = now @ Vector((0.0, 1.0, 0.0))
            set_world_rot(tiger, pb, along.rotation_difference(want.normalized()) @ now)
        bpy.context.view_layer.update()
        for pb in tiger.pose.bones:
            pb.keyframe_insert("rotation_quaternion", frame=out_frame)
        bone(tiger, "Hips").keyframe_insert("location", frame=out_frame)

    # Without the source rig from here on
    if source:
        for o in source_all:
            bpy.data.objects.remove(o, do_unlink=True)
    scene.frame_start, scene.frame_end = 0, count - 1
    tiger.animation_data.action.name = name

    # The sheet: 8 frames across, from the front-right and from the left side
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "TEXTURE"
    scene.render.resolution_x = 320
    scene.render.resolution_y = 320
    scene.world = bpy.data.worlds.new("w")
    scene.world.color = (0.82, 0.84, 0.88)
    height = max((tiger.matrix_world @ pb.bone.head_local).z for pb in tiger.pose.bones)
    middle = Vector((0.0, 0.0, height * 0.6))
    shots = []
    for cam_name, yaw in (("front", 28.0), ("side", -90.0)):
        cam = bpy.data.cameras.new(cam_name)
        cam.type = "ORTHO"
        cam.ortho_scale = height * 1.45
        obj = bpy.data.objects.new(cam_name, cam)
        scene.collection.objects.link(obj)
        way = (forward * math.cos(math.radians(yaw)) - left * math.sin(math.radians(yaw)) + up * 0.12).normalized()
        obj.location = middle + way * height * 8.0
        obj.rotation_euler = (middle - obj.location).to_track_quat("-Z", "Y").to_euler()
        scene.camera = obj
        for i in range(8):
            scene.frame_set(round((count - 1) * i / 7))
            path = os.path.join(out_dir, f"_{name}_{cam_name}_{i}.png")
            scene.render.filepath = path
            bpy.ops.render.render(write_still=True)
            shots.append(path)

    bpy.ops.object.select_all(action="DESELECT")
    tiger.select_set(True)
    for m in tiger_meshes:
        m.select_set(True)
    bpy.context.view_layer.objects.active = tiger
    bpy.ops.export_scene.fbx(filepath=os.path.join(out_dir, name + ".fbx"), use_selection=True, add_leaf_bones=False,
                             bake_anim=True, bake_anim_use_all_bones=True, bake_anim_use_nla_strips=False,
                             bake_anim_use_all_actions=False, bake_anim_force_startend_keying=True, bake_anim_step=1.0,
                             bake_anim_simplify_factor=0.0, path_mode="COPY", embed_textures=False)
    print("ARMS wrote", name, count, "frames")
