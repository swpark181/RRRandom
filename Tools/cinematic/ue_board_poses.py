# Storyboard draft (2026-11-23): rough stand-in poses for the new cuts, made from clips the fox already has (the way
# ue_cine_aim_low.py lowers the aiming arms): a copy of a clip, optionally frozen at one moment, with some bones turned
# in component space (X = the body's side-to-side axis: + raises what points forward, so + on a thigh brings the leg
# forward; Z = up: + turns to the fox's right). Only for the storyboard's stills; the real motions come later.
# Run with UnrealEditor-Cmd -run=pythonscript.
import math

import unreal

FOLDER = "/Game/Characters/Fox/Anims"
AL = unreal.AnimationLibrary
eal = unreal.EditorAssetLibrary
AXES = {"X": (1.0, 0.0, 0.0), "Y": (0.0, 1.0, 0.0), "Z": (0.0, 0.0, 1.0)}

# (new clip, source clip, freeze at seconds or None, [(bone, axis, degrees)])
POSES = [
    # cut 1: running, looking back over the right shoulder
    ("A_Fox_Board_RunLookBack", "A_Fox_Run", None, [("Spine2", "Z", 10), ("Neck", "Z", 20), ("Head", "Z", 40)]),
    # cut 3: hands up against the wall
    ("A_Fox_Board_WallHit", "A_Fox_Cine_Terrified", 1.0, [("LeftArm", "X", 45), ("RightArm", "X", 45), ("Head", "X", 12)]),
    # cuts 3-5: slumped on the floor, legs out in front, hands up at the chest
    ("A_Fox_Board_Sit", "A_Fox_Cine_Terrified", 1.0, [("LeftUpLeg", "X", 78), ("RightUpLeg", "X", 78), ("Spine", "X", 8), ("Head", "X", -8)]),
    # cut 5: the same, looking up at the die over its head
    ("A_Fox_Board_SitLookUp", "A_Fox_Cine_Terrified", 1.0, [("LeftUpLeg", "X", 78), ("RightUpLeg", "X", 78), ("Spine", "X", 8), ("Neck", "X", 14), ("Head", "X", 30)]),
]


def qmul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by, aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw, aw * bw - ax * bx - ay * by - az * bz)


def qinv(a):
    return (-a[0], -a[1], -a[2], a[3])


def qof(q):
    return (q.x, q.y, q.z, q.w)


for new, src, freeze, turns in POSES:
    if eal.does_asset_exist(f"{FOLDER}/{new}"):
        eal.delete_asset(f"{FOLDER}/{new}")
    eal.duplicate_asset(f"{FOLDER}/{src}", f"{FOLDER}/{new}")
    anim = unreal.load_asset(f"{FOLDER}/{new}")
    n = AL.get_num_keys(anim)
    tracks = [str(t) for t in AL.get_animation_track_names(anim)]
    low = [t.lower() for t in tracks]
    if freeze is not None:
        k = min(n - 1, int(round(freeze / anim.get_play_length() * (n - 1))))
        for b in tracks:
            p = AL.get_bone_pose_for_frame(anim, b, k, False)
            anim.controller.set_bone_track_keys(b, [p.translation] * n, [p.rotation] * n, [p.scale3d] * n, False)
    for bone, axis, deg in turns:
        name = next(t for t in tracks if t.lower().endswith(bone.lower()))
        ax = AXES[axis]
        h = math.radians(deg) / 2.0
        dq = (ax[0] * math.sin(h), ax[1] * math.sin(h), ax[2] * math.sin(h), math.cos(h))
        path = [str(p) for p in AL.find_bone_path_to_root(anim, name)]
        chain = [tracks[low.index(b.lower())] for b in reversed(path[1:]) if b.lower() in low]
        pos, rot, scl = [], [], []
        for i in range(n):
            parent = (0.0, 0.0, 0.0, 1.0)
            for b in chain:
                parent = qmul(parent, qof(AL.get_bone_pose_for_frame(anim, b, i, False).rotation))
            local = AL.get_bone_pose_for_frame(anim, name, i, False)
            q = qmul(qmul(qmul(qinv(parent), dq), parent), qof(local.rotation))
            norm = math.sqrt(sum(c * c for c in q))
            pos.append(local.translation)
            rot.append(unreal.Quat(*(c / norm for c in q)))
            scl.append(local.scale3d)
        anim.controller.set_bone_track_keys(name, pos, rot, scl, False)
    eal.save_asset(f"{FOLDER}/{new}")
    unreal.log(f"BOARD pose {new}: from {src}, {n} keys, frozen at {freeze}, turned {turns}")
unreal.log("BOARD DONE")
