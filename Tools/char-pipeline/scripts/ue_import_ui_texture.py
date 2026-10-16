"""Import PNG/JPEG images as HUD textures (run inside Unreal).

Usage: UnrealEditor-Cmd <project> -run=pythonscript "-script=ue_import_ui_texture.py <dest folder>|<image>=<asset name>|..."
  e.g. /Game/UI|C:/work/char-pipeline/output/ui/T_FoxPortrait.png=T_FoxPortrait

UI texture group, no mipmaps, sRGB, alpha kept, so the canvas draws them crisp at their own size.
"""
import sys

import unreal

args = " ".join(sys.argv[1:]).split("|")
folder, entries = args[0], [a.rsplit("=", 1) for a in args[1:]]
eal = unreal.EditorAssetLibrary
for image, name in entries:
    if eal.does_asset_exist(f"{folder}/{name}"):
        eal.delete_asset(f"{folder}/{name}")
    task = unreal.AssetImportTask()
    task.filename = image
    task.destination_path = folder
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    tex = unreal.load_asset(f"{folder}/{name}")
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    tex.set_editor_property("srgb", True)
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    eal.save_asset(tex.get_path_name())
    unreal.log(f"UITEX {tex.get_path_name()} {tex.blueprint_get_size_x()}x{tex.blueprint_get_size_y()}")
