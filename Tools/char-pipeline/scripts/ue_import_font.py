"""Import TTF files as Font Face assets with their data inline (run inside Unreal).

Usage: UnrealEditor-Cmd <project> -run=pythonscript "-script=ue_import_font.py <dest folder>|<ttf>=<asset name>|..."
  e.g. /Game/UI/Fonts|C:/work/char-pipeline/input/fonts_nunito/Nunito-Black.ttf=FF_NunitoBlack

The HUD (RRRandomUIStyle) builds a runtime font from these faces, so the canvas draws them crisp at any size.
"""
import sys

import unreal

args = " ".join(sys.argv[1:]).split("|")
folder, entries = args[0], [a.rsplit("=", 1) for a in args[1:]]
eal = unreal.EditorAssetLibrary
for ttf, name in entries:
    if eal.does_asset_exist(f"{folder}/{name}"):
        eal.delete_asset(f"{folder}/{name}")
    task = unreal.AssetImportTask()
    task.filename = ttf
    task.destination_path = folder
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    face = unreal.load_asset(f"{folder}/{name}")
    if not face:
        unreal.log_error(f"FONTFACE failed {ttf}")
        continue
    face.set_editor_property("loading_policy", unreal.FontLoadingPolicy.INLINE)
    eal.save_asset(face.get_path_name())
    unreal.log(f"FONTFACE {face.get_path_name()} {type(face).__name__}")
