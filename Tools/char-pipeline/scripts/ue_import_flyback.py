"""Give every brawler the Mixamo "Flying Back Death" (run inside Unreal, legacy FBX importer).

Usage: UnrealEditor-Cmd <project> -run=pythonscript -dpcvars=Interchange.FeatureFlags.Import.FBX=0
           "-script=ue_import_flyback.py <fbx>|<its rig's hips height>|<Name>=<hips height>|<Name>=<hips height>..."
e.g.  "...|C:/.../tiger_flyback.fbx|26.3|Fox=27.5|Rabbit=22.1|Squirrel=21.2|Tiger=26.3"

Mixamo keeps one uploaded character at a time (the tiger, when this was made), so the motion comes from one rig and goes
onto every species' skeleton as /Game/Characters/<Name>/Anims/A_<Name>_FlyBack. The Mixamo rigs share bone names and
bone axes, so the local rotations carry over as they are; the bone lengths don't: every bone but the hips takes its own
skeleton's reference translation, and the hips' height is scaled by the hips heights (scale 1, blender_rig_measure.py).
The hips' travel across the ground is taken out entirely (they stay where the first frame has them): the game throws the
brawler itself (ARRRandomCharacter's fling), and the body lands where its capsule does. Bones the rig has and a skeleton
doesn't (the squirrel has no thumbs) are left out.
"""
import sys

import unreal

args = " ".join(sys.argv[1:]).split("|")
fbx, source_hips = args[0].strip(), float(args[1])
targets = [(a.split("=")[0].strip(), float(a.split("=")[1])) for a in args[2:]]
eal = unreal.EditorAssetLibrary
AL = unreal.AnimationLibrary


def log(msg):
    unreal.log(f"FLYBACK {msg}")


def ref_pose(skeleton):
    pose = unreal.AnimPoseExtensions.get_reference_pose(skeleton)
    # By lower case name: the track names come back lower case
    return {str(n).lower(): unreal.AnimPoseExtensions.get_ref_bone_pose(pose, n, unreal.AnimPoseSpaces.LOCAL)
            for n in unreal.AnimPoseExtensions.get_bone_names(pose)}


for name, hips_height in targets:
    folder = f"/Game/Characters/{name}"
    skeleton = unreal.load_asset(f"{folder}/SK_{name}").get_editor_property("skeleton")
    asset = f"A_{name}_FlyBack"
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
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    anim = next((a for a in (unreal.load_asset(p) for p in task.imported_object_paths) if isinstance(a, unreal.AnimSequence)), None)
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
    for bone in tracks:
        keys = [AL.get_bone_pose_for_frame(anim, bone, i, False) for i in range(n)]
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
    eal.save_asset(anim.get_path_name().split(".")[0])
    log(f"{name}: {asset} {AL.get_sequence_length(anim):.2f} s, {n} keys, {len(tracks)} tracks; bone lengths changed by up to {worst:.1f}")

# How well a species' own Mixamo animation agrees with its reference pose (the bone lengths the fix above relies on)
for name, _ in targets:
    idle = unreal.load_asset(f"/Game/Characters/{name}/Anims/A_{name}_Idle")
    ref = ref_pose(idle.get_editor_property("skeleton"))
    off = max((AL.get_bone_pose_for_frame(idle, b, 0, False).translation - ref[b.lower()].translation).length()
              for b in ("LeftArm", "LeftForeArm", "Neck", "Head", "LeftLeg") if b.lower() in ref)
    log(f"check: {name}'s own idle vs its reference pose bone offsets differ by up to {off:.2f}")
log("DONE")
