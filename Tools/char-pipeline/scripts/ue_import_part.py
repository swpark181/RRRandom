"""Bring a swappable face part of an animal friend in (run inside Unreal, legacy FBX importer; 2026-11-20, the tiger's
muzzle cut off by blender_split_part.py so expressions and talking can swap only that part later).

Usage: UnrealEditor-Cmd <project> -run=pythonscript -dpcvars=Interchange.FeatureFlags.Import.FBX=0
           "-script=ue_import_part.py <Name>|<part fbx>|<asset name>"
  <part fbx>: armature + the part mesh only (blender_split_part.py's <out>_muzzle.fbx, or an edited copy of it: an open
  mouth, a talking shape ...), same bones as the animal's rig.
  <asset name>: e.g. SK_TigerMuzzle (the default part) or SK_TigerMuzzle_Laugh.

Makes /Game/Characters/<Name>/<asset name> on SK_<Name>_Skeleton with the file's own normals (so its rim shades like the
body's) and M_<Name> on its slots. The game shows it on a leader-pose component in place of the body's "Muzzle" section.
"""
import sys

import unreal

U = unreal
name, fbx, asset = [a.strip() for a in " ".join(sys.argv[1:]).split("|")[:3]]
dest = f"/Game/Characters/{name}"
eal = U.EditorAssetLibrary
skeleton = U.load_asset(f"{dest}/SK_{name}_Skeleton")
if not skeleton:
    raise RuntimeError(f"no {dest}/SK_{name}_Skeleton")
ui = U.FbxImportUI()
ui.automated_import_should_detect_type = False
ui.import_as_skeletal = True
ui.import_mesh = True
ui.import_animations = False
ui.import_materials = False
ui.import_textures = False
ui.create_physics_asset = False
ui.mesh_type_to_import = U.FBXImportType.FBXIT_SKELETAL_MESH
ui.skeleton = skeleton
ui.skeletal_mesh_import_data.set_editor_property("normal_import_method", U.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
task = U.AssetImportTask()
task.filename = fbx
task.destination_path = dest
task.destination_name = asset
task.automated = True
task.replace_existing = True
task.save = True
task.options = ui
U.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
mesh = U.load_asset(f"{dest}/{asset}")
if not mesh or mesh.get_editor_property("skeleton") != skeleton:
    raise RuntimeError("the part didn't land on the skeleton")
materials = mesh.get_editor_property("materials")
for m in materials:
    m.set_editor_property("material_interface", U.load_asset(f"{dest}/M_{name}"))
mesh.set_editor_property("materials", materials)
eal.save_asset(mesh.get_path_name())
U.log_warning(f"PART DONE {dest}/{asset} <- {fbx}: {len(materials)} slot(s) "
              f"{[str(m.get_editor_property('material_slot_name')) for m in materials]}, bounds {mesh.get_bounds().box_extent}")
