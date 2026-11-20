# Cinematic (2026-11-19): bakes "run + swing" for the tiger inside Unreal: a copy of a swing animation whose hips and legs
# are replaced by a run looped under it (the upper body, Spine and up, arms and hands, stays the swing's), the way the
# game lays a melee blow over the run at runtime (URRRandomBodyAnimInstance) but as one clip a Level Sequence can play.
# RR_MERGE="<swing asset>|<run asset>|<new name>|<run rate>;..." (assets in /Game/Characters/Tiger/Anims).
# Run with UnrealEditor-Cmd -run=pythonscript.
import os

import unreal

FOLDER = "/Game/Characters/Tiger/Anims"
AL = unreal.AnimationLibrary
eal = unreal.EditorAssetLibrary
LOWER = ("hips", "upleg", "leg", "foot", "toebase", "toe_end")


def lower(bone):
    b = bone.lower()
    return b == "hips" or any(b.endswith(k) for k in LOWER[1:]) and "arm" not in b


for job in (j for j in os.environ.get("RR_MERGE", "").split(";") if j.strip()):
    swing_name, run_name, new_name, rate = (p.strip() for p in job.split("|"))
    rate = float(rate)
    if eal.does_asset_exist(f"{FOLDER}/{new_name}"):
        eal.delete_asset(f"{FOLDER}/{new_name}")
    eal.duplicate_asset(f"{FOLDER}/{swing_name}", f"{FOLDER}/{new_name}")
    new = unreal.load_asset(f"{FOLDER}/{new_name}")
    run = unreal.load_asset(f"{FOLDER}/{run_name}")
    n = AL.get_num_keys(new)
    rn = AL.get_num_keys(run)
    swapped = []
    for bone in (str(t) for t in AL.get_animation_track_names(new)):
        if not lower(bone):
            continue
        keys = [AL.get_bone_pose_for_frame(run, bone, int(i * rate) % max(rn - 1, 1), False) for i in range(n)]
        new.controller.set_bone_track_keys(bone, [k.translation for k in keys], [k.rotation for k in keys], [k.scale3d for k in keys], False)
        swapped.append(bone)
    eal.save_asset(f"{FOLDER}/{new_name}")
    unreal.log(f"CINEMERGE {new_name}: {n} keys, lower body from {run_name} x{rate}: {', '.join(swapped)}")
unreal.log("CINEMERGE DONE")
