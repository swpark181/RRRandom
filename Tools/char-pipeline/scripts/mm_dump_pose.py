"""Log a clip's Root and Hips transforms (local and world, with and without root motion extraction) at a few times.
Usage: ... "-script=mm_dump_pose.py <anim asset path>" (run inside Unreal)."""
import sys
import unreal

seq = unreal.load_asset(sys.argv[1])
ext = unreal.AnimPoseExtensions


def f(t):
    r = t.rotation.rotator()
    return (f"loc ({t.translation.x:.1f},{t.translation.y:.1f},{t.translation.z:.1f}) rot (p{r.pitch:.1f},y{r.yaw:.1f},r{r.roll:.1f}) "
            f"scale ({t.scale3d.x:.3f},{t.scale3d.y:.3f},{t.scale3d.z:.3f})")


ref = ext.get_reference_pose(seq.get_skeleton())
for b in ("Root", "Hips"):
    unreal.log(f"MM ref {b} local {f(ext.get_bone_pose(ref, b, unreal.AnimPoseSpaces.LOCAL))}")
for extract in (False, True):
    o = unreal.AnimPoseEvaluationOptions()
    o.extract_root_motion = extract
    for t in (0.0, seq.get_play_length() * 0.5):
        p = ext.get_anim_pose_at_time(seq, t, o)
        for b in ("Root", "Hips"):
            unreal.log(f"MM extract={extract} t={t:.2f} {b} local {f(ext.get_bone_pose(p, b, unreal.AnimPoseSpaces.LOCAL))} | "
                       f"world {f(ext.get_bone_pose(p, b, unreal.AnimPoseSpaces.WORLD))}")
unreal.log(f"MM root motion {seq.get_editor_property('enable_root_motion')} lock {seq.get_editor_property('root_motion_root_lock')} "
           f"force lock {seq.get_editor_property('force_root_lock')} retarget source {seq.get_editor_property('retarget_source')} additive {seq.get_editor_property('additive_anim_type')}")
unreal.log("MM MMDONE dump")
