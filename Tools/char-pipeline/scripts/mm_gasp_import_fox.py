"""Motion matching test, step 1 (run inside Unreal, in the Game Animation Sample project, legacy FBX importer).

Brings the fox rig that has a root bone (blender_add_root_bone.py) into the sample project as /Game/Characters/FoxMM/SK_FoxMM,
logs its bones, logs how the sample's own IK rig and one of its retargeters are set up, and makes an IK rig for the fox
with Unreal's automatic retarget definition, logging what it came up with.

Usage: UnrealEditor-Cmd GameAnimationSample.uproject -run=pythonscript "-script=mm_gasp_import_fox.py <fox_rig_root.fbx>"
"""
import sys

import os

import unreal

# 2026-11-20: the animal friend, e.g. RR_MM_NAME=Tiger (default the fox, the first one)
MM = os.environ.get("RR_MM_NAME", "Fox") + "MM"

fbx = sys.argv[1]
DEST = f"/Game/Characters/{MM}"
eal = unreal.EditorAssetLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(msg):
    unreal.log(f"MM {msg}")


def chains_of(rig):
    ctrl = unreal.IKRigController.get_controller(rig)
    out = []
    for c in ctrl.get_retarget_chains():
        n = c.chain_name
        out.append(f"{n}:{ctrl.get_retarget_chain_start_bone(n)}>{ctrl.get_retarget_chain_end_bone(n)}:{ctrl.get_retarget_chain_goal(n)}")
    return ctrl, out


# 1. the fox mesh with its root bone
if eal.does_directory_exist(DEST):
    eal.delete_directory(DEST)
ui = unreal.FbxImportUI()
ui.automated_import_should_detect_type = False
ui.import_as_skeletal = True
ui.import_mesh = True
ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
ui.import_animations = False
ui.import_materials = False
ui.import_textures = False
ui.create_physics_asset = False
task = unreal.AssetImportTask()
task.filename = fbx
task.destination_path = DEST
task.destination_name = f"SK_{MM}"
task.automated = True
task.replace_existing = True
task.save = True
task.options = ui
asset_tools.import_asset_tasks([task])
log(f"imported {list(task.imported_object_paths)}")
mesh = unreal.load_asset(f"{DEST}/SK_{MM}")
skel = mesh.skeleton
log(f"mesh {mesh.get_path_name()} skeleton {skel.get_path_name()}")
try:
    pose = skel.get_reference_pose()
    names = [str(n) for n in pose.get_bone_names()]
    log(f"bones {len(names)}: {' '.join(names)}")
    for b in ("Root", "Hips"):
        t = pose.get_ref_bone_pose(b, unreal.AnimPoseSpaces.WORLD)
        log(f"ref {b} world loc {t.translation} rot {t.rotation.rotator()}")
except Exception as e:  # noqa: BLE001
    log(f"bone listing failed: {e}")

# 2. how the sample sets its own rigs up
src_rig = unreal.load_asset("/Game/Characters/UEFN_Mannequin/Rigs/IK_UEFN_Mannequin")
sctrl, schains = chains_of(src_rig)
log(f"SOURCE rig chains {len(schains)}: {' | '.join(schains)}")
log(f"SOURCE retarget root {sctrl.get_retarget_root()} root motion bone {sctrl.get_root_motion_bone()}")
for path in ("/Game/Characters/UE5_Mannequins/Rigs/RTG_UEFN_to_UE5_Mannequin", "/Game/Characters/Echo/Rigs/RTG_UEFN_to_Echo"):
    rtg = unreal.load_asset(path)
    if not rtg:
        log(f"no {path}")
        continue
    rc = unreal.IKRetargeterController.get_controller(rtg)
    ops = []
    i = 0
    while True:
        n = rc.get_op_name(i)
        if str(n) in ("None", ""):
            break
        ops.append(f"{i}:{n}(on={rc.get_retarget_op_enabled(i)},parent={rc.get_parent_op_by_name(n)})")
        i += 1
        if i > 40:
            break
    log(f"RTG {path.split('/')[-1]} ops: {' '.join(ops)}")
    trig = rc.get_ik_rig(unreal.RetargetSourceOrTarget.TARGET)
    if trig:
        _, tch = chains_of(trig)
        log(f"RTG target rig {trig.get_name()} chains: {' | '.join(tch)}")

# 3. the fox's IK rig, automatic definition
rig = asset_tools.create_asset(f"IK_{MM}", DEST, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
ctrl = unreal.IKRigController.get_controller(rig)
log(f"set mesh {ctrl.set_skeletal_mesh(mesh)}")
log(f"auto definition {ctrl.apply_auto_generated_retarget_definition()}")
_, fch = chains_of(rig)
log(f"FOX rig chains {len(fch)}: {' | '.join(fch)}")
log(f"FOX retarget root {ctrl.get_retarget_root()} root motion bone {ctrl.get_root_motion_bone()}")
eal.save_directory(DEST)
log("MMDONE import")
