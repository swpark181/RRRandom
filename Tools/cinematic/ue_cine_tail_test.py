# Fox vs tiger cinematic (2026-11-24, the user: "귀, 꼬리 연기를 넣을 수 있는 방법 찾아봐"): a test that the tail can act.
# The rigs have four tail bones (Tail1-4 under Hips) that no clip animates: the game bends them by code
# (URRRandomBodyAnimInstance), and the cinematic's animals, which play clips alone, leave them at the reference pose.
# This copies a clip, gives it tracks for the tail bones and keys a wag with the tail lifted, to see it in an audition.
#   RR_TAIL_SRC = the clip to copy (default A_Fox_Board_Sit) -> <same folder>/A_Fox_Board_TestTail
import math
import os

import unreal

AL = unreal.AnimationLibrary
ext = unreal.AnimPoseExtensions
eal = unreal.EditorAssetLibrary
SRC = os.environ.get("RR_TAIL_SRC", "/Game/Characters/Fox/Anims/A_Fox_Board_Sit")
DST = SRC.rsplit("/", 1)[0] + "/A_Fox_Board_TestTail"
TAIL = ["Tail1", "Tail2", "Tail3", "Tail4"]


def qmul(a, b):
    return unreal.Quat(a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y, a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
                       a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w, a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z)


def qinv(a):
    return unreal.Quat(-a.x, -a.y, -a.z, a.w)


def about(axis, deg):
    h = math.radians(deg) / 2.0
    return unreal.Quat(axis[0] * math.sin(h), axis[1] * math.sin(h), axis[2] * math.sin(h), math.cos(h))


if eal.does_asset_exist(DST):
    eal.delete_asset(DST)
eal.duplicate_asset(SRC, DST)
anim = unreal.load_asset(DST)
ref = ext.get_reference_pose(anim.get_editor_property("skeleton"))
n = AL.get_num_keys(anim)
fps = (n - 1) / anim.get_play_length()
ctl = anim.controller
have = {str(t) for t in AL.get_animation_track_names(anim)}
for i, bone in enumerate(TAIL):
    if bone not in have:
        ok = ctl.add_bone_curve(bone, False)
        unreal.log(f"CINEBUILD tail track {bone}: added {ok}")
    local = ext.get_bone_pose(ref, bone, unreal.AnimPoseSpaces.LOCAL)
    parent = "Hips" if i == 0 else TAIL[i - 1]
    par = ext.get_bone_pose(ref, parent, unreal.AnimPoseSpaces.WORLD).rotation
    pos, rot, scl = [], [], []
    for k in range(n):
        t = k / fps
        # (the game's own: the tail points down the mesh's -Y; a swing turns it about Z, a lift about X; more toward the tip)
        share = (i + 1) / len(TAIL)
        swing = 34.0 * share * math.sin(2.0 * math.pi * t / 0.7 - 0.6 * i)
        lift = 16.0 * share
        delta = qmul(about((0, 0, 1), -swing), about((1, 0, 0), -lift))
        pos.append(local.translation)
        rot.append(qmul(qmul(qmul(qinv(par), delta), par), local.rotation))
        scl.append(local.scale3d)
    ctl.set_bone_track_keys(bone, pos, rot, scl, False)
eal.save_asset(DST)
unreal.log(f"CINEBUILD tail test: {DST}, {n} keys, tracks now {len(AL.get_animation_track_names(anim))}")
unreal.log("CINEBUILD DONE")
