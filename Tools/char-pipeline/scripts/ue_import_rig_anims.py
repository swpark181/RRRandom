"""Give an animal friend rigged in Blender (blender_fit_rig.py: no Mixamo rig of its own, so no Mixamo exports) another
species' whole Mixamo set (run inside Unreal, legacy FBX importer).

Usage: UnrealEditor-Cmd <project> -run=pythonscript -dpcvars=Interchange.FeatureFlags.Import.FBX=0
           "-script=ue_import_rig_anims.py <Name>|<its hips height>|<source mixamo dir>|<source name>|<source hips height>"
e.g.  "...|RedFox|23.5|C:/work/char-pipeline/input/tiger/mixamo|tiger|26.3"

First ue_import_animal.py <Name>|<dir with <name>_SK.fbx = the Blender rig>|<base color> makes SK_<Name>, its skeleton
and M_<Name> (it finds no animations there). This then imports <source name>_SK.fbx (the idle) and every file of
ue_import_animal.py's FILES from the source's folder onto SK_<Name>'s skeleton as Anims/A_<Name>_<What>, the way
ue_import_shared_anim.py carries a motion across rigs: local rotations as they are (the rigs share Mixamo's bone names
and axes), every bone but the hips at its own skeleton's reference translation (bone lengths), and the hips' motion
scaled by the hips heights (scale 1, blender_rig_measure.py / FSpeciesTuning). Unlike there, the hips keep their sway;
what ue_import_animal.py makes stay in place (walk, victory, dances) has its start-to-end drift taken out the same way.
The run and the fly-back still come from ue_import_shared_anim.py, like every species'.

Extra "|<file suffix>=<What>" arguments import only those instead (as they move: nothing taken out), for a species'
own motions (the rabbit's moon hop from input/rabbit/mixamo_extra on the old rabbit's rig, hips 17.5; the tiger's roar and
jump attack, whose forward leap ARRRandomTigerSpirit relies on):
      "...|Rabbit|15|C:/work/char-pipeline/input/rabbit/mixamo_extra|rabbit|17.5|jumpup=JumpUp|frontflip=FrontFlip|..."
"""
import os
import sys

import unreal

args = " ".join(sys.argv[1:]).split("|")
name, hips_height = args[0].strip(), float(args[1])
source_dir, source, source_hips = args[2].strip(), args[3].strip(), float(args[4])
dest = f"/Game/Characters/{name}"
eal = unreal.EditorAssetLibrary
AL = unreal.AnimationLibrary
# file suffix -> (asset name, take the hips' drift out): ue_import_animal.py's
FILES = {
    "SK": ("Idle", False),
    "walk": ("Walk", True), "fire": ("Fire", False), "death": ("Death", False), "victory": ("Victory", True),
    "aim_idle": ("AimIdle", False), "run_f": ("AimRunF", False), "run_b": ("AimRunB", False), "run_l": ("AimRunL", False),
    "run_r": ("AimRunR", False), "dance1_twist": ("Dance1", True), "dance2_shimmy": ("Dance2", True),
    "dance3_runningman": ("Dance3", True), "dance4_chicken": ("Dance4", True), "defeat1_defeated": ("Defeat1", False),
    "defeat2_crying": ("Defeat2", False), "defeat3_sad": ("Defeat3", False),
}
extra = [a.strip().split("=") for a in args[5:] if "=" in a]
if extra:
    FILES = {suffix: (what, False) for suffix, what in extra}


def log(msg):
    unreal.log(f"RIGANIMS {msg}")


skeleton = unreal.load_asset(f"{dest}/SK_{name}").get_editor_property("skeleton")
pose = unreal.AnimPoseExtensions.get_reference_pose(skeleton)
# By lower case name: the track names come back lower case
ref = {str(n).lower(): unreal.AnimPoseExtensions.get_ref_bone_pose(pose, n, unreal.AnimPoseSpaces.LOCAL)
       for n in unreal.AnimPoseExtensions.get_bone_names(pose)}
ratio = hips_height / source_hips

for suffix, (what, in_place) in FILES.items():
    src = f"{source_dir}/{source}_{suffix}.fbx"
    if not os.path.exists(src):
        log(f"missing {src}")
        continue
    asset = f"A_{name}_{what}"
    if eal.does_asset_exist(f"{dest}/Anims/{asset}"):
        eal.delete_asset(f"{dest}/Anims/{asset}")
    ui = unreal.FbxImportUI()
    ui.automated_import_should_detect_type = False
    ui.import_as_skeletal = True
    ui.import_mesh = False
    ui.import_animations = True
    ui.import_materials = False
    ui.import_textures = False
    ui.skeleton = skeleton
    ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_ANIMATION
    ui.anim_sequence_import_data.set_editor_property(
        "animation_length", unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
    task = unreal.AssetImportTask()
    task.filename = src
    task.destination_path = f"{dest}/Anims"
    task.destination_name = asset
    task.automated = True
    task.replace_existing = True
    task.save = True
    task.options = ui
    try:
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    except RuntimeError as e:
        log(f"{asset}: importer said {str(e)[:120]}")
    paths = list(task.imported_object_paths) or [f"{dest}/Anims/{asset}"]
    anim = next((a for a in (unreal.load_asset(p) for p in paths if eal.does_asset_exist(p.split(".")[0]))
                 if isinstance(a, unreal.AnimSequence)), None)
    if anim is None:
        log(f"{asset}: nothing imported from {src}")
        continue
    if anim.get_name() != asset:
        eal.rename_asset(anim.get_path_name().split(".")[0], f"{dest}/Anims/{asset}")
        anim = unreal.load_asset(f"{dest}/Anims/{asset}")

    n = AL.get_num_keys(anim)
    tracks = [str(t) for t in AL.get_animation_track_names(anim)]
    line = f"{asset}: {AL.get_sequence_length(anim):.2f} s, {n} keys, {len(tracks)} tracks"
    for bone in tracks:
        keys = [AL.get_bone_pose_for_frame(anim, bone, i, False) for i in range(n)]
        if bone.lower() == "hips":
            pos = [k.translation * ratio for k in keys]
            if in_place and n > 1:
                dx, dy = pos[-1].x - pos[0].x, pos[-1].y - pos[0].y
                pos = [unreal.Vector(p.x - dx * i / (n - 1), p.y - dy * i / (n - 1), p.z) for i, p in enumerate(pos)]
                line += f", drift ({dx:.1f},{dy:.1f}) taken out"
            line += f", hips z {min(p.z for p in pos):.1f}..{max(p.z for p in pos):.1f}"
        elif bone.lower() in ref:
            pos = [ref[bone.lower()].translation] * n
        else:
            continue
        anim.controller.set_bone_track_keys(bone, pos, [k.rotation for k in keys], [k.scale3d for k in keys], False)
    eal.save_asset(anim.get_path_name().split(".")[0])
    log(line)
for a in eal.list_assets(f"{dest}/Anims", recursive=False):
    if unreal.load_asset(a).get_editor_property("skeleton") != skeleton:
        raise RuntimeError(f"{a} is on another skeleton")
log("DONE")
