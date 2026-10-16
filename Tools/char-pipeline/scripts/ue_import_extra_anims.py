"""Add more Mixamo animations to one brawler, as they are (run inside Unreal, legacy FBX importer).

Usage: UnrealEditor-Cmd <project> -run=pythonscript -dpcvars=Interchange.FeatureFlags.Import.FBX=0
           "-script=ue_import_extra_anims.py <Name>|<fbx>=<asset name>|<fbx>=<asset name>..."
e.g.  "...|Tiger|C:/.../tiger_roar.fbx=A_Tiger_Roar|C:/.../tiger_jumpattack.fbx=A_Tiger_JumpAttack"

Onto /Game/Characters/<Name>/SK_<Name>'s skeleton, into its Anims folder. Nothing is moved in place: the tiger's ultimate
(ARRRandomTigerSpirit) relies on the jump attack's forward leap being in it. Delete an existing asset's file first
(replacing a loaded animation leaves it without compression settings). Logs each one's length.
"""
import sys

import unreal

args = " ".join(sys.argv[1:]).split("|")
name = args[0].strip()
folder = f"/Game/Characters/{name}"
skeleton = unreal.load_asset(f"{folder}/SK_{name}").get_editor_property("skeleton")
for pair in args[1:]:
    fbx, asset = (p.strip() for p in pair.rsplit("=", 1))
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
        unreal.log(f"EXTRAANIM {asset}: nothing imported")
        continue
    if anim.get_name() != asset:
        unreal.EditorAssetLibrary.rename_asset(anim.get_path_name().split(".")[0], f"{folder}/Anims/{asset}")
        anim = unreal.load_asset(f"{folder}/Anims/{asset}")
    unreal.EditorAssetLibrary.save_asset(anim.get_path_name().split(".")[0])
    unreal.log(f"EXTRAANIM {asset}: {unreal.AnimationLibrary.get_sequence_length(anim):.2f} s")
unreal.log("EXTRAANIM DONE")
