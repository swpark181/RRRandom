"""Run in GameAnimationSample to export the four missing side loops; then in RRRandom to index them."""
import os

import unreal

names = [f"{gait}_Loop_{side}" for gait in ("Walk", "Run") for side in ("LR", "RR")]
rig_name = os.environ.get("RR_MM_NAME", "Fox") + "MM"
target = f"/Game/Characters/{rig_name}"
library = unreal.EditorAssetLibrary
if unreal.Paths.get_project_file_path().endswith("GameAnimationSample.uproject"):
    rig = unreal.load_asset("/Game/Characters/UEFN_Mannequin/Rigs/IK_UEFN_Mannequin")
    inputs = unreal.IKRetargetBatchOperationInputs()
    inputs.source_mesh = unreal.IKRigController.get_controller(rig).get_skeletal_mesh()
    inputs.target_mesh = unreal.load_asset(target + f"/SK_{rig_name}")
    inputs.ik_retarget_asset = unreal.load_asset(target + f"/RTG_UEFN_to_{rig_name}")
    inputs.target_path = target + "/Anims"
    inputs.search = "M_Neutral_"
    inputs.replace = f"A_{rig_name}_"
    inputs.use_source_path = False
    inputs.include_referenced_assets = False
    inputs.overwrite_existing_files = False
    missing = [n for n in names if not library.does_asset_exist(target+f"/Anims/A_{rig_name}_"+n)]
    inputs.assets_to_retarget = [library.find_asset_data(
        "/Game/Characters/UEFN_Mannequin/Animations/"+n.split("_")[0]+"/M_Neutral_"+n) for n in missing]
    if missing:
        unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
    for n in names:
        seq = unreal.load_asset(target+f"/Anims/A_{rig_name}_"+n)
        assert seq and seq.get_play_length() > 0
        if n in missing:
            unreal.AnimationLibrary.remove_all_animation_notify_tracks(seq)
            seq.set_editor_property("asset_user_data", [])
            seq.set_editor_property("enable_root_motion", True)
            seq.set_editor_property("rate_scale", 1.0)
            library.save_loaded_asset(seq)
else:
    clips = [unreal.load_asset(target+f"/Anims/A_{rig_name}_"+n) for n in names]
    assert all(clips)
    result = unreal.RRRandomMotionMatchLibrary.add_clips(unreal.load_asset(target+f"/PSD_{rig_name}"), clips)
    assert result >= 4, result
    unreal.log(f"{rig_name} STRAFE database contains {result} entries")
unreal.log(f"{rig_name} STRAFE complete")
