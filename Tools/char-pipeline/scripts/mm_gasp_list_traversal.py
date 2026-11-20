"""List the sample's traversal clips: length, where the root ends (so: how high and how far the move goes) and when
it gets there (run inside Unreal, in the Game Animation Sample project)."""
import unreal

reg = unreal.AssetRegistryHelpers.get_asset_registry()
ext = unreal.AnimPoseExtensions
o = unreal.AnimPoseEvaluationOptions()
o.extract_root_motion = False
for ad in sorted(reg.get_assets_by_path("/Game/Characters/UEFN_Mannequin/Animations/Traversal", recursive=True), key=lambda a: str(a.asset_name)):
    if str(ad.asset_class_path.asset_name) != "AnimSequence" or "M_Neutral" not in str(ad.asset_name) or "Catch" in str(ad.asset_name):
        continue
    seq = ad.get_asset()
    n = seq.get_play_length()
    pts = []
    for i in range(11):
        t = n * i / 10
        r = ext.get_bone_pose(ext.get_anim_pose_at_time(seq, t, o), "root", unreal.AnimPoseSpaces.WORLD).translation
        pts.append(f"{r.y:.0f}/{r.z:.0f}")
    unreal.log(f"MM {ad.asset_name}: {n:.2f}s root forward/up at tenths: {' '.join(pts)}")
unreal.log("MM MMDONE list")
