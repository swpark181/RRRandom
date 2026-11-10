"""Give every animal friend one Mixamo motion taken from one rig (run inside Unreal, legacy FBX importer).

Usage: UnrealEditor-Cmd <project> -run=pythonscript -dpcvars=Interchange.FeatureFlags.Import.FBX=0
           "-script=ue_import_shared_anim.py <fbx>|<asset suffix>|<its rig's hips height>|<Name>=<hips height>|..."
e.g.  "...|C:/.../dragon_run.fbx|Run|20.4|Fox=27.5|Rabbit=17.5|Squirrel=21.2|Tiger=26.3|Dragon=20.4"
      -> /Game/Characters/<Name>/Anims/A_<Name>_Run

The same as ue_import_flyback.py (round 24), for any motion: Mixamo keeps one uploaded character at a time, so a motion
comes from whichever rig is there. The rigs share bone names and axes, so local rotations carry over as they are; every
bone but the hips takes its own skeleton's reference translation (bone lengths), and the hips' height is scaled by the
hips heights (scale 1, blender_rig_measure.py / FSpeciesTuning in RRRandomSpecies.cpp). The hips stay over where the
first frame has them across the ground (an "in place" motion; any drift is taken out). Bones the rig has and a skeleton
doesn't (the squirrel has no thumbs) are left out.
"""
import sys

import unreal

args = " ".join(sys.argv[1:]).split("|")
fbx, suffix, source_hips = args[0].strip(), args[1].strip(), float(args[2])
targets = [(a.split("=")[0].strip(), float(a.split("=")[1])) for a in args[3:]]
eal = unreal.EditorAssetLibrary
AL = unreal.AnimationLibrary


def log(msg):
    unreal.log(f"SHAREDANIM {msg}")


def ref_pose(skeleton):
    pose = unreal.AnimPoseExtensions.get_reference_pose(skeleton)
    # By lower case name: the track names come back lower case
    return {str(n).lower(): unreal.AnimPoseExtensions.get_ref_bone_pose(pose, n, unreal.AnimPoseSpaces.LOCAL)
            for n in unreal.AnimPoseExtensions.get_bone_names(pose)}


for name, hips_height in targets:
    folder = f"/Game/Characters/{name}"
    skeleton = unreal.load_asset(f"{folder}/SK_{name}").get_editor_property("skeleton")
    asset = f"A_{name}_{suffix}"
    if eal.does_asset_exist(f"{folder}/Anims/{asset}"):
        eal.delete_asset(f"{folder}/Anims/{asset}")
    ui = unreal.FbxImportUI()
    ui.automated_import_should_detect_type = False
    ui.import_as_skeletal = True
    ui.import_mesh = False
    ui.import_animations = True
    ui.import_materials = False
    ui.import_textures = False
    ui.skeleton = skeleton
    ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_ANIMATION
    ui.anim_sequence_import_data.set_editor_property("use_default_sample_rate", True)
    ui.anim_sequence_import_data.set_editor_property(
        "animation_length", unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
    task = unreal.AssetImportTask()
    task.filename = fbx
    task.destination_path = f"{folder}/Anims"
    task.destination_name = asset
    task.automated = True
    task.replace_existing = True
    task.save = True
    task.options = ui
    try:
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    except RuntimeError as e:
        # Tracks for bones the skeleton lacks (the squirrel's thumbs) come back as an error; the rest is imported
        log(f"{name}: importer said {str(e)[:120]}")
    paths = list(task.imported_object_paths) or [f"{folder}/Anims/{asset}"]
    anim = next((a for a in (unreal.load_asset(p) for p in paths if eal.does_asset_exist(p.split(".")[0])) if isinstance(a, unreal.AnimSequence)), None)
    if anim is None:
        log(f"{name}: nothing imported")
        continue
    if anim.get_name() != asset:
        eal.rename_asset(anim.get_path_name().split(".")[0], f"{folder}/Anims/{asset}")
        anim = unreal.load_asset(f"{folder}/Anims/{asset}")

    ref = ref_pose(skeleton)
    n = AL.get_num_keys(anim)
    tracks = [str(t) for t in AL.get_animation_track_names(anim)]
    worst = 0.0
    keys_by_bone = {bone: [AL.get_bone_pose_for_frame(anim, bone, i, False) for i in range(n)] for bone in tracks}
    anim.controller.open_bracket("Retarget shared motion", False)
    try:
        for bone in tracks:
            keys = keys_by_bone[bone]
            if bone.lower() == "hips":
                first = keys[0].translation
                pos = [unreal.Vector(first.x, first.y, k.translation.z * hips_height / source_hips) for k in keys]
                log(f"{name}: hips z {min(p.z for p in pos):.1f}..{max(p.z for p in pos):.1f}, held at ({first.x:.1f},{first.y:.1f})")
            elif bone.lower() in ref:
                own = ref[bone.lower()].translation
                worst = max(worst, max((k.translation - own).length() for k in keys))
                pos = [own] * n
            else:
                continue
            anim.controller.set_bone_track_keys(bone, pos, [k.rotation for k in keys], [k.scale3d for k in keys], False)
    finally:
        anim.controller.close_bracket(False)
    eal.save_asset(anim.get_path_name().split(".")[0])
    log(f"{name}: {asset} {AL.get_sequence_length(anim):.2f} s, {n} keys, {len(tracks)} tracks; bone lengths changed by up to {worst:.1f}")
log("DONE")
