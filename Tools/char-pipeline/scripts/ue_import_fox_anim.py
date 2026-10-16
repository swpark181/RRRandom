"""Adds one more Mixamo animation of the fox to the project (run inside Unreal, legacy FBX importer).

Usage: UnrealEditor-Cmd <project> -run=pythonscript -dpcvars=Interchange.FeatureFlags.Import.FBX=0
           "-script=ue_import_fox_anim.py <fbx>|<asset name>[|inplace]"
e.g.  "-script=ue_import_fox_anim.py C:/.../House Dancing.fbx|A_Fox_Victory|inplace"

Imports only the animation (a With Skin download's mesh is skipped) onto /Game/Characters/Fox/SK_Fox's skeleton,
into /Game/Characters/Fox/Anims under the given name. "inplace" takes the hips' straight-line drift out (start to end)
and keeps the rest of their movement, like ue_setup_fox.py does for the walk. Logs the length and the hips' path.
"""
import sys

import unreal

args = " ".join(sys.argv[1:]).split("|")
fbx, name = args[0], args[1]
in_place = len(args) > 2 and args[2].strip() == "inplace"
eal = unreal.EditorAssetLibrary
FOX = "/Game/Characters/Fox"
DEST = f"{FOX}/Anims"


def log(msg):
    unreal.log(f"FOXANIM {msg}")


skeleton = unreal.load_asset(f"{FOX}/SK_Fox").get_editor_property("skeleton")
target = f"{DEST}/{name}"
if eal.does_asset_exist(target):
    eal.delete_asset(target)

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
task.destination_path = DEST
task.destination_name = name
task.automated = True
task.replace_existing = True
task.save = True
task.options = ui
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
paths = list(task.imported_object_paths)
log(f"imported -> {paths}")
anim = next(a for a in (unreal.load_asset(p) for p in paths) if isinstance(a, unreal.AnimSequence))
if anim.get_path_name().split(".")[0] != target:
    eal.rename_asset(anim.get_path_name().split(".")[0], target)
    anim = unreal.load_asset(target)
if anim.get_editor_property("skeleton") != skeleton:
    raise RuntimeError("the animation is on another skeleton")

bone = "Hips"
n = unreal.AnimationLibrary.get_num_keys(anim)
keys = [unreal.AnimationLibrary.get_bone_pose_for_frame(anim, bone, i, False) for i in range(n)]
pos = [k.translation for k in keys]
xs, ys, zs = [p.x for p in pos], [p.y for p in pos], [p.z for p in pos]
log(f"{name}: {unreal.AnimationLibrary.get_sequence_length(anim):.2f} s, {n} keys; hips x {min(xs):.1f}..{max(xs):.1f} "
    f"y {min(ys):.1f}..{max(ys):.1f} z {min(zs):.1f}..{max(zs):.1f}, start ({pos[0].x:.1f},{pos[0].y:.1f}) end ({pos[-1].x:.1f},{pos[-1].y:.1f})")
if in_place and n > 1:
    dx, dy = pos[-1].x - pos[0].x, pos[-1].y - pos[0].y
    fixed = [unreal.Vector(p.x - dx * i / (n - 1), p.y - dy * i / (n - 1), p.z) for i, p in enumerate(pos)]
    anim.controller.set_bone_track_keys(bone, fixed, [k.rotation for k in keys], [k.scale3d for k in keys], False)
    log(f"in place: took out drift ({dx:.1f},{dy:.1f})")
eal.save_asset(target)
log("DONE")
