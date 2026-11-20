"""Motion matching test, step 3 (run inside Unreal, in the Game Animation Sample project): retarget the sample's standing
locomotion onto the fox that has a root bone (/Game/Characters/FoxMM, made by mm_gasp_import_fox.py).

- finishes the fox's IK rig: chain names like the sample's rig (LeftToe/RightToe, Root), full-body IK for the feet
- makes the retargeter RTG_UEFN_to_FoxMM (default op stack: pelvis motion, FK chains, IK rig, root motion, curves),
  maps the chains by name and lines the fox's T-pose up with the mannequin's pose
- retargets every animation of the chosen Pose Search databases (read from gasp_dbs.json, written by
  mm_gasp_dump_dbs.py) into /Game/Characters/FoxMM/Anims as A_FoxMM_<name>, without notifies (they point at the
  sample's sound blueprints) and with root motion on
- writes <out json>: {database name: {schema, animations: [fox asset name, ...]}} and logs how far the root travels in
  a few clips next to the source, to see that root motion came across

Usage: UnrealEditor-Cmd GameAnimationSample.uproject -run=pythonscript "-script=mm_gasp_retarget_fox.py <gasp_dbs.json> <out json>"
"""
import json
import sys

import os

import unreal

# 2026-11-20: the animal friend, e.g. RR_MM_NAME=Tiger (default the fox, the first one)
MM = os.environ.get("RR_MM_NAME", "Fox") + "MM"

dbs_json, out_json = sys.argv[1], sys.argv[2]
DEST = f"/Game/Characters/{MM}"
ANIMS = f"{DEST}/Anims"
MMROOT = "/Game/Characters/UEFN_Mannequin/Animations/MotionMatchingData/Databases"
SRC, TGT = unreal.RetargetSourceOrTarget.SOURCE, unreal.RetargetSourceOrTarget.TARGET
eal = unreal.EditorAssetLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
reg = unreal.AssetRegistryHelpers.get_asset_registry()


def log(msg):
    unreal.log(f"MM {msg}")


def chosen(path):
    name = path.split("/")[-1]
    if "/Sparse/" in path and "_Stand_" in name:
        return True
    return name in ("PSD_Dense_Stand_Idles", "PSD_Extreme_Sparse_Stand_TurnInPlace")


# ---- 1. the fox's IK rig
mesh = unreal.load_asset(f"{DEST}/SK_{MM}")
rig = unreal.load_asset(f"{DEST}/IK_{MM}")
ctrl = unreal.IKRigController.get_controller(rig)
names = [str(c.chain_name) for c in ctrl.get_retarget_chains()]
for old, new in (("LeftFoot", "LeftToe"), ("RightFoot", "RightToe")):
    if old in names:
        log(f"chain {old} -> {ctrl.rename_retarget_chain(old, new)}")
if "Root" not in names:
    log(f"chain added: {ctrl.add_retarget_chain('Root', 'Root', 'Root', 'None')}")
log(f"retarget root {ctrl.get_retarget_root()} root motion bone {ctrl.get_root_motion_bone()}")
try:
    log(f"auto full-body IK {ctrl.apply_auto_fbik()}")
except Exception as e:  # noqa: BLE001
    log(f"auto full-body IK failed: {e}")
log("fox chains: " + " ".join(f"{c.chain_name}({ctrl.get_retarget_chain_start_bone(c.chain_name)}>"
                               f"{ctrl.get_retarget_chain_end_bone(c.chain_name)},{ctrl.get_retarget_chain_goal(c.chain_name)})"
                               for c in ctrl.get_retarget_chains()))

# ---- 2. the retargeter
src_rig = unreal.load_asset("/Game/Characters/UEFN_Mannequin/Rigs/IK_UEFN_Mannequin")
src_mesh = unreal.IKRigController.get_controller(src_rig).get_skeletal_mesh()
log(f"source mesh {src_mesh.get_path_name() if src_mesh else None}")
rtg_path = f"{DEST}/RTG_UEFN_to_{MM}"
rtg = unreal.load_asset(rtg_path) or asset_tools.create_asset(f"RTG_UEFN_to_{MM}", DEST, unreal.IKRetargeter, unreal.IKRetargetFactory())
rc = unreal.IKRetargeterController.get_controller(rtg)
rc.set_ik_rig(SRC, src_rig)
rc.set_ik_rig(TGT, rig)
rc.set_preview_mesh(SRC, src_mesh)
rc.set_preview_mesh(TGT, mesh)


def op_names():
    out, i = [], 0
    while i < 40:
        n = str(rc.get_op_name(i))
        if n in ("None", ""):
            break
        out.append(n)
        i += 1
    return out


if not op_names():
    rc.add_default_ops()
rc.assign_ik_rig_to_all_ops(SRC, src_rig)
rc.assign_ik_rig_to_all_ops(TGT, rig)
for i in range(len(op_names())):
    rc.run_op_initial_setup(i)
log(f"ops: {op_names()}")
# The root motion op takes the source rig's "root motion bone", which the sample's rig has set to the pelvis: the fox's
# root then rode at the mannequin's pelvis height and wobbled with it (found 2026-11-20: the fox sank into the floor once
# the root was locked in the game). The mannequin's real root bone instead, and the fox's root kept on the ground.
for i, n in enumerate(op_names()):
    if "Root Motion" in n:
        oc = rc.get_op_controller(i)
        log(f"root motion op was: source {oc.get_source_root_bone()} target {oc.get_target_root_bone()} pelvis {oc.get_target_pelvis_bone()}")
        oc.set_source_root_bone("root")
        oc.set_target_root_bone("Root")
        oc.set_target_pelvis_bone("Hips")
        s = oc.get_settings()
        s.root_height_source = unreal.RootMotionHeightSource.SNAP_TO_GROUND
        oc.set_settings(s)
        log(f"root motion op now: source {oc.get_source_root_bone()} target {oc.get_target_root_bone()} pelvis {oc.get_target_pelvis_bone()}")
rc.auto_map_chains(unreal.AutoMapChainType.EXACT, True)
rc.auto_map_chains(unreal.AutoMapChainType.FUZZY, False)
log("chain map: " + " ".join(f"{c.chain_name}<-{rc.get_source_chain(c.chain_name)}" for c in ctrl.get_retarget_chains()))
rc.auto_align_all_bones(TGT)
eal.save_asset(rig.get_path_name())
eal.save_asset(rtg.get_path_name())

# ---- 3. which animations
dbs = json.load(open(dbs_json, encoding="utf-8"))
plan, wanted = {}, {}
for path, e in sorted(dbs.items()):
    if not chosen(path):
        continue
    name = path.split("/")[-1]
    items = [a["path"] for a in e["animations"] if a["path"] and a["class"] == "AnimSequence" and "Crouch" not in a["path"]]
    plan[name] = {"schema": e["schema"].split("/")[-1], "source": items}
    for p in items:
        wanted[p] = True
log(f"databases {len(plan)}, animations to retarget {len(wanted)}")

if eal.does_directory_exist(ANIMS):
    eal.delete_directory(ANIMS)
inputs = unreal.IKRetargetBatchOperationInputs()
inputs.assets_to_retarget = [reg.get_asset_by_object_path(f"{p}.{p.split('/')[-1]}") for p in wanted]
inputs.source_mesh = src_mesh
inputs.target_mesh = mesh
inputs.ik_retarget_asset = rtg
inputs.search = "M_Neutral_"
inputs.replace = ""
inputs.prefix = f"A_{MM}_"
inputs.suffix = ""
inputs.target_path = ANIMS
inputs.use_source_path = False
inputs.include_referenced_assets = False
inputs.overwrite_existing_files = True
made = unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
log(f"retargeted {len(made)} assets")


def fox_name(src_path):
    return f"A_{MM}_" + src_path.split("/")[-1].replace("M_Neutral_", "")


# ---- 4. clean the results up and check them
al = unreal.AnimationLibrary
missing, deps_seen = [], set()
for p in wanted:
    fp = f"{ANIMS}/{fox_name(p)}"
    seq = unreal.load_asset(fp)
    if not seq:
        missing.append(fox_name(p))
        continue
    al.remove_all_animation_notify_tracks(seq)
    try:
        seq.set_editor_property("asset_user_data", [])
    except Exception:  # noqa: BLE001
        pass
    seq.set_editor_property("enable_root_motion", True)
    eal.save_asset(fp)
opts = unreal.AssetRegistryDependencyOptions()
for p in list(wanted)[:400]:
    for d in reg.get_dependencies(f"{ANIMS}/{fox_name(p)}", opts) or []:
        d = str(d)
        if not d.startswith("/Script/") and not d.startswith(DEST):
            deps_seen.add(d)
log(f"missing {missing}")
log(f"outside dependencies of the fox clips: {sorted(deps_seen)[:30]}")


def travel(seq, bone):
    o = unreal.AnimPoseEvaluationOptions()
    o.extract_root_motion = False
    a = unreal.AnimPoseExtensions.get_anim_pose_at_time(seq, 0.0, o)
    b = unreal.AnimPoseExtensions.get_anim_pose_at_time(seq, seq.get_play_length(), o)
    ta = unreal.AnimPoseExtensions.get_bone_pose(a, bone, unreal.AnimPoseSpaces.WORLD).translation
    tb = unreal.AnimPoseExtensions.get_bone_pose(b, bone, unreal.AnimPoseSpaces.WORLD).translation
    return (round(tb.x - ta.x, 1), round(tb.y - ta.y, 1), round(tb.z - ta.z, 1))


for short in ("Run_Loop_F", "Walk_Loop_F", "Sprint_Loop_F", "Run_Loop_LL", "Run_Stop_F_Lfoot", "Stand_Idle_Loop"):
    sp = next((p for p in wanted if p.endswith("M_Neutral_" + short)), None)
    if not sp:
        log(f"check {short}: not in the set")
        continue
    s, f = unreal.load_asset(sp), unreal.load_asset(f"{ANIMS}/{fox_name(sp)}")
    if not f:
        continue
    log(f"check {short}: {s.get_play_length():.2f}s source root {travel(s, 'root')} pelvis {travel(s, 'pelvis')} | "
        f"fox {f.get_play_length():.2f}s Root {travel(f, 'Root')} Hips {travel(f, 'Hips')} "
        f"root motion {f.get_editor_property('enable_root_motion')}")

# Absolute check (the travel above looked right while the root itself was wrong): in every clip's first frame the root
# must be on the ground and the hips about their reference height above it
ref = unreal.AnimPoseExtensions.get_reference_pose(mesh.skeleton)
ref_hips = unreal.AnimPoseExtensions.get_bone_pose(ref, "Hips", unreal.AnimPoseSpaces.WORLD).translation.z
o = unreal.AnimPoseEvaluationOptions()
o.extract_root_motion = False
off = []
for p in wanted:
    f = unreal.load_asset(f"{ANIMS}/{fox_name(p)}")
    pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(f, 0.0, o)
    root = unreal.AnimPoseExtensions.get_bone_pose(pose, "Root", unreal.AnimPoseSpaces.WORLD)
    hips = unreal.AnimPoseExtensions.get_bone_pose(pose, "Hips", unreal.AnimPoseSpaces.WORLD)
    local = unreal.AnimPoseExtensions.get_bone_pose(pose, "Hips", unreal.AnimPoseSpaces.LOCAL).translation
    if abs(root.translation.z) > 0.5 or abs(hips.translation.z - ref_hips) > ref_hips * 0.5:
        off.append(f"{fox_name(p)} root z {root.translation.z:.1f} hips z {hips.translation.z:.1f}")
    if p.endswith("M_Neutral_Run_Loop_F"):
        r = root.rotation.rotator()
        log(f"Run_Loop_F first frame: root loc ({root.translation.x:.1f},{root.translation.y:.1f},{root.translation.z:.1f}) "
            f"rot (p{r.pitch:.1f},y{r.yaw:.1f},r{r.roll:.1f}), hips local ({local.x:.1f},{local.y:.1f},{local.z:.1f}), "
            f"hips world z {hips.translation.z:.1f} (reference {ref_hips:.1f})")
log(f"ROOT CHECK: {len(off)} clips with the root off the ground or the hips far from their height: {off[:8]}")

out = {name: {"schema": e["schema"], "animations": [fox_name(p) for p in e["source"]]} for name, e in plan.items()}
json.dump(out, open(out_json, "w", encoding="utf-8"), indent=1)
eal.save_directory(DEST)
log(f"MMDONE retarget {len(wanted) - len(missing)} clips -> {ANIMS}, list {out_json}")
