# Cinematic (2026-11-19, fifth pass): the fox's two-handed pistol aim with the arms lowered, for the close-up (cut 7).
# Mixamo's Pistol Aim holds the gun at eye level, so from the lowered close-up camera the forearms and pistol covered the
# fox's mouth. This copies A_Fox_Cine_PistolAim to A_Fox_Cine_PistolAimLow and turns both arms (LeftArm / RightArm, so
# the forearms, hands and gun go with them) down about the body's side-to-side axis by RR_AIMLOW_DEG degrees, at every
# key: in component space (the chain's own rotations multiplied from the root), so the hands stay together.
# Run with UnrealEditor-Cmd -run=pythonscript.
import os

import unreal

FOLDER = "/Game/Characters/Fox/Anims"
SRC, NEW = "A_Fox_Cine_PistolAim", "A_Fox_Cine_PistolAimLow"
DEG = float(os.environ.get("RR_AIMLOW_DEG", "25"))
AXIS = os.environ.get("RR_AIMLOW_AXIS", "X")   # the body's side-to-side axis in the mesh's component space
AL = unreal.AnimationLibrary
eal = unreal.EditorAssetLibrary

if eal.does_asset_exist(f"{FOLDER}/{NEW}"):
    eal.delete_asset(f"{FOLDER}/{NEW}")
eal.duplicate_asset(f"{FOLDER}/{SRC}", f"{FOLDER}/{NEW}")
anim = unreal.load_asset(f"{FOLDER}/{NEW}")
n = AL.get_num_keys(anim)
tracks = [str(t) for t in AL.get_animation_track_names(anim)]
unreal.log(f"CINEAIMLOW tracks: {tracks}")
axis = {"X": unreal.Vector(1, 0, 0), "Y": unreal.Vector(0, 1, 0), "Z": unreal.Vector(0, 0, 1)}[AXIS.lstrip("-")]
sign = -1.0 if AXIS.startswith("-") else 1.0
import math
half = sign * math.radians(DEG) / 2.0
delta = (axis.x * math.sin(half), axis.y * math.sin(half), axis.z * math.sin(half), math.cos(half))


# quaternions as (x, y, z, w) tuples, multiplied as Unreal's FQuat (A * B: B first, then A)
def qmul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by, aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw, aw * bw - ax * bx - ay * by - az * bz)


def qinv(a):
    return (-a[0], -a[1], -a[2], a[3])


def qof(q):
    return (q.x, q.y, q.z, q.w)
skeleton = anim.get_editor_property("skeleton")


def parent_chain(bone):
    """The bone's ancestors from the root down (names), from the skeleton's reference."""
    path = AL.find_bone_path_to_root(anim, bone)
    names = [str(p) for p in path]
    # (the call lists the bone itself first and the root last)
    return list(reversed(names[1:]))


def turn(bone, delta_q, what):
    low = [t.lower() for t in tracks]
    chain = [tracks[low.index(b.lower())] for b in parent_chain(bone) if b.lower() in low]
    pos, rot, scl = [], [], []
    for i in range(n):
        parent = (0.0, 0.0, 0.0, 1.0)
        for b in chain:
            parent = qmul(parent, qof(AL.get_bone_pose_for_frame(anim, b, i, False).rotation))
        local = AL.get_bone_pose_for_frame(anim, bone, i, False)
        q = qmul(qmul(qmul(qinv(parent), delta_q), parent), qof(local.rotation))
        norm = math.sqrt(sum(c * c for c in q))
        pos.append(local.translation)
        rot.append(unreal.Quat(*(c / norm for c in q)))
        scl.append(local.scale3d)
    anim.controller.set_bone_track_keys(bone, pos, rot, scl, False)
    unreal.log(f"CINEAIMLOW {bone}: chain {chain}, {n} keys turned {what}")


for side in ("Left", "Right"):
    turn(next(t for t in tracks if t.lower() == f"{side}arm".lower()), delta, f"{sign * DEG} deg about {AXIS}")
# 2026-11-20 (the remade fox: its mouth is under a longer snout, and the aim pose bows the head, so the grin hid from
# the close-up): RR_AIMLOW_EXTRA="Head=18;Neck=6" turns those bones too, by their own degrees about the same axis
for item in (e for e in os.environ.get("RR_AIMLOW_EXTRA", "").split(";") if "=" in e):
    name, deg = item.split("=")
    h = sign * math.radians(float(deg)) / 2.0
    dq = (axis.x * math.sin(h), axis.y * math.sin(h), axis.z * math.sin(h), math.cos(h))
    turn(next(t for t in tracks if t.lower().endswith(name.strip().lower())), dq, f"{deg} deg about {AXIS}")
eal.save_asset(f"{FOLDER}/{NEW}")
unreal.log("CINEAIMLOW DONE")
