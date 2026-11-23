# Storyboard draft (2026-11-23): prints what a clip does, to plan how clips are cut and joined: its length and keys, and
# at steps of RR_PROBE_STEP seconds where its Root goes (forward, side, yaw) and where its Hips are in the body's space
# (forward/side, height, yaw), and the same for RR_PROBE_BONES. RR_PROBE_ANIMS = asset paths, comma separated; RR_PROBE_TRACKS=1 also lists the bone tracks.
import math
import os

import unreal

AL = unreal.AnimationLibrary
ext = unreal.AnimPoseExtensions
opt = unreal.AnimPoseEvaluationOptions()
opt.extract_root_motion = False
step = float(os.environ.get("RR_PROBE_STEP", "0.25"))


def yaw(q):
    return math.degrees(math.atan2(2.0 * (q.w * q.z + q.x * q.y), 1.0 - 2.0 * (q.y * q.y + q.z * q.z)))


for path in (w for w in os.environ.get("RR_PROBE_ANIMS", "").split(",") if w):
    a = unreal.load_asset(path)
    if not a:
        unreal.log(f"BOARD missing {path}")
        continue
    n = a.get_play_length()
    tracks = [str(t) for t in AL.get_animation_track_names(a)]
    unreal.log(f"BOARD probe {path.split('/')[-1]}: {n:.2f}s, {AL.get_num_keys(a)} keys, {len(tracks)} tracks, skeleton {a.get_editor_property('skeleton').get_name()}")
    if os.environ.get("RR_PROBE_TRACKS"):
        unreal.log(f"BOARD   tracks: {' '.join(tracks)}")
    root = next((t for t in tracks if t.lower() == "root"), None)
    hips = next((t for t in tracks if t.lower().endswith("hips")), None)
    more = [next((t for t in tracks if t.lower() == w.lower()), None) for w in os.environ.get("RR_PROBE_BONES", "").split(",") if w]
    t = 0.0
    while t <= n + 1e-3:
        pose = ext.get_anim_pose_at_time(a, min(t, n), opt)
        line = f"BOARD   {t:5.2f}"
        if root:
            r = ext.get_bone_pose(pose, root, unreal.AnimPoseSpaces.WORLD)
            line += f" root {r.translation.y:6.1f}/{r.translation.x:5.1f} yaw {yaw(r.rotation):6.1f}"
        if hips:
            h = ext.get_bone_pose(pose, hips, unreal.AnimPoseSpaces.WORLD)
            line += f" hips {h.translation.y:6.1f}/{h.translation.x:5.1f} up {h.translation.z:5.1f} yaw {yaw(h.rotation):6.1f}"
        for b in more:
            if b:
                m = ext.get_bone_pose(pose, b, unreal.AnimPoseSpaces.WORLD).translation
                line += f" {b} {m.y:6.1f}/{m.x:5.1f} up {m.z:4.1f}"
        unreal.log(line)
        t += step
unreal.log("BOARD DONE")
