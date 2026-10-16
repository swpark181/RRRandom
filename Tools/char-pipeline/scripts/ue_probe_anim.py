"""Probe animation-editing API and reference pose for the lab fox (run inside Unreal)."""
import unreal

out = []
a = unreal.load_asset("/Game/Characters/Fox/Anims/Fox_RifleWalk")
out.append("anim methods: " + ", ".join(m for m in dir(a) if any(k in m.lower() for k in ("controller", "model", "root", "frame_rate", "rate"))))
for name in ("AnimationDataController", "AnimationDataModel", "AnimPoseExtensions", "AnimationLibrary"):
    cls = getattr(unreal, name, None)
    out.append(f"## {name}: " + (", ".join(m for m in dir(cls) if not m.startswith("_") and any(k in m.lower() for k in ("key", "track", "pose", "root", "set_", "get_", "frame"))) if cls else "MISSING"))
try:
    skel = unreal.load_asset("/Game/Characters/Fox/Fox_RifleIdle_Skeleton")
    ref = unreal.AnimPoseExtensions.get_reference_pose(skel)
    for b in ("hips", "head", "lefthand", "righthand", "lefttoebase"):
        out.append(f"ref {b} world {unreal.AnimPoseExtensions.get_bone_pose(ref, b, unreal.AnimPoseSpaces.WORLD).translation} local {unreal.AnimPoseExtensions.get_bone_pose(ref, b, unreal.AnimPoseSpaces.LOCAL).translation}")
except Exception as e:
    out.append(f"ref pose error: {e}")
try:
    pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(a, 0.0, unreal.AnimPoseEvaluationOptions())
    for b in ("hips", "lefttoebase", "righttoebase"):
        out.append(f"walk t0 {b} world {unreal.AnimPoseExtensions.get_bone_pose(pose, b, unreal.AnimPoseSpaces.WORLD).translation}")
except Exception as e:
    out.append(f"anim pose error: {e}")
open(r"C:\work\char-pipeline\output\ue_anim_probe.txt", "w", encoding="utf-8").write("\n".join(out))
