"""Replace only an animal friend's base colour texture (run inside Unreal; 2026-11-20, the fox's face repainted).

Usage: UnrealEditor-Cmd <project> -run=pythonscript "-script=ue_import_basecolor.py <Name>|<image>"
  e.g. Fox|C:/work/char-pipeline/input/fox_game/tripo/fox_game_basecolor.png

Imports the image over /Game/Characters/<Name>/Textures/T_<Name>_BaseColor (same asset, so M_<Name> and M_<Name>Fur keep
pointing at it). The mesh, the rig and the animations are not touched. Use it only when the UVs are the same.
"""
import sys

import unreal

name, image = " ".join(sys.argv[1:]).split("|")[:2]
dest = f"/Game/Characters/{name}/Textures"
path = f"{dest}/T_{name}_BaseColor"
if not unreal.EditorAssetLibrary.does_asset_exist(path):
    raise RuntimeError(f"no {path}: import the animal first (ue_import_animal.py)")
task = unreal.AssetImportTask()
task.filename = image
task.destination_path = dest
task.destination_name = f"T_{name}_BaseColor"
task.automated = True
task.replace_existing = True
task.save = True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
tex = unreal.load_asset(path)
tex.set_editor_property("srgb", True)
unreal.EditorAssetLibrary.save_asset(path)
unreal.log_warning(f"BASECOLOR DONE {path} <- {image} ({tex.blueprint_get_size_x()}x{tex.blueprint_get_size_y()})")
