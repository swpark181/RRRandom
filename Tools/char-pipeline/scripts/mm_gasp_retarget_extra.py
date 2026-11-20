"""Motion matching set, extra clips (run inside Unreal, in the Game Animation Sample project): retarget single sample
clips onto the fox with the retargeter mm_gasp_retarget_fox.py made, into their own folder (so they stay out of the
Pose Search database, which takes everything in Anims).

Usage: UnrealEditor-Cmd GameAnimationSample.uproject -run=pythonscript
    "-script=mm_gasp_retarget_extra.py <target folder under /Game/Characters/<Name>MM>|<source asset path>=<new name>|..."
e.g. "Traversal|/Game/.../M_Neutral_Traversal_Mantle_1_0_stand_F_Lfoot=A_FoxMM_ClimbLow|...=A_FoxMM_ClimbHigh"
Notifies are removed (they point at the sample's sound blueprints), root motion stays on. Logs each clip's length and
where its root goes (forward / up) at tenths of its length.
"""
import sys

import os

import unreal

# 2026-11-20: the animal friend, e.g. RR_MM_NAME=Tiger (default the fox, the first one)
MM = os.environ.get("RR_MM_NAME", "Fox") + "MM"

args = " ".join(sys.argv[1:]).split("|")
DEST = f"/Game/Characters/{MM}"
folder = f"{DEST}/{args[0].strip()}"
pairs = [a.split("=") for a in args[1:] if "=" in a]
eal = unreal.EditorAssetLibrary
reg = unreal.AssetRegistryHelpers.get_asset_registry()
ext = unreal.AnimPoseExtensions


def log(msg):
    unreal.log(f"MM {msg}")


mesh = unreal.load_asset(f"{DEST}/SK_{MM}")
rtg = unreal.load_asset(f"{DEST}/RTG_UEFN_to_{MM}")
src_rig = unreal.load_asset("/Game/Characters/UEFN_Mannequin/Rigs/IK_UEFN_Mannequin")
src_mesh = unreal.IKRigController.get_controller(src_rig).get_skeletal_mesh()
rc = unreal.IKRetargeterController.get_controller(rtg)
# (the same root motion setting as the main retarget: the mannequin's real root bone, the fox's root on the ground)
i = 0
while i < 40:
    n = str(rc.get_op_name(i))
    if n in ("None", ""):
        break
    if "Root Motion" in n:
        oc = rc.get_op_controller(i)
        oc.set_source_root_bone("root")
        oc.set_target_root_bone("Root")
        oc.set_target_pelvis_bone("Hips")
        s = oc.get_settings()
        # a climb's root leaves the ground: keep its height (the main set snaps it to the floor)
        s.root_height_source = unreal.RootMotionHeightSource.COPY_HEIGHT_FROM_SOURCE
        oc.set_settings(s)
    i += 1

o = unreal.AnimPoseEvaluationOptions()
o.extract_root_motion = False
for src, name in pairs:
    src, name = src.strip(), name.strip()
    if eal.does_asset_exist(f"{folder}/{name}"):
        eal.delete_asset(f"{folder}/{name}")
    inputs = unreal.IKRetargetBatchOperationInputs()
    inputs.assets_to_retarget = [reg.get_asset_by_object_path(f"{src}.{src.split('/')[-1]}")]
    inputs.source_mesh = src_mesh
    inputs.target_mesh = mesh
    inputs.ik_retarget_asset = rtg
    inputs.search = src.split("/")[-1]
    inputs.replace = name
    inputs.prefix = ""
    inputs.suffix = ""
    inputs.target_path = folder
    inputs.use_source_path = False
    inputs.include_referenced_assets = False
    inputs.overwrite_existing_files = True
    made = unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
    seq = unreal.load_asset(f"{folder}/{name}")
    if not seq:
        log(f"FAILED {name} (made {[str(m.asset_name) for m in made]})")
        continue
    unreal.AnimationLibrary.remove_all_animation_notify_tracks(seq)
    seq.set_editor_property("enable_root_motion", True)
    eal.save_asset(f"{folder}/{name}")
    n = seq.get_play_length()
    pts = []
    for k in range(11):
        r = ext.get_bone_pose(ext.get_anim_pose_at_time(seq, n * k / 10, o), "Root", unreal.AnimPoseSpaces.WORLD).translation
        pts.append(f"{r.y:.0f}/{r.z:.0f}")
    log(f"{name}: {n:.2f}s root forward/up at tenths: {' '.join(pts)}")
# back to the main set's setting
i = 0
while i < 40:
    n = str(rc.get_op_name(i))
    if n in ("None", ""):
        break
    if "Root Motion" in n:
        oc = rc.get_op_controller(i)
        s = oc.get_settings()
        s.root_height_source = unreal.RootMotionHeightSource.SNAP_TO_GROUND
        oc.set_settings(s)
    i += 1
eal.save_asset(rtg.get_path_name())
log("MMDONE extra")
