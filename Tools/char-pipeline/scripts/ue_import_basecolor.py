"""Replace only an animal friend's base colour texture (run inside Unreal; 2026-11-20, the fox's face repainted).

Usage: UnrealEditor-Cmd <project> -run=pythonscript "-script=ue_import_basecolor.py <Name>|<image>"
  e.g. Fox|C:/work/char-pipeline/input/fox_game/tripo/fox_game_basecolor.png

Imports the image over /Game/Characters/<Name>/Textures/T_<Name>_BaseColor (same asset, so M_<Name> and M_<Name>Fur keep
pointing at it). The mesh, the rig and the animations are not touched. Use it only when the UVs are the same.

2026-11-23: the face texture's fur mask comes in with it (T_<Name>_BaseColor_FurMask, made from this very image:
ue_fur_mask_lib.py, blender_fur_mask.py), for animal friends that have one (fur_chars.json): the fur stays off the
eyes and the mouth wherever the new face has them. If M_<Name>Fur is an older one that doesn't read a mask yet, it is
rebuilt (ue_import_fur_shells.py "<Name>|||", material only).
"""
import importlib.util
import os
import sys

import unreal


def load_lib():
    here = os.path.dirname(os.path.abspath(__file__))
    spec = importlib.util.spec_from_file_location("ue_fur_mask_lib", os.path.join(here, "ue_fur_mask_lib.py"))
    lib = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(lib)
    return lib


furlib = load_lib()
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
mask = furlib.texture_with_mask(name, image, dest, f"T_{name}_BaseColor")
fur_mat = f"/Game/Characters/{name}/M_{name}Fur"
if mask and unreal.EditorAssetLibrary.does_asset_exist(fur_mat) and not furlib.fur_material_reads_mask(name):
    one = os.path.join(os.path.dirname(os.path.abspath(__file__)), "ue_import_fur_shells.py")
    old_argv = sys.argv
    sys.argv = [one, f"{name}|||"]
    try:
        with open(one, encoding="utf-8") as f:
            exec(compile(f.read(), one, "exec"), {"__name__": "__main__", "__file__": one})
    finally:
        sys.argv = old_argv
unreal.log_warning(f"BASECOLOR fur mask: {mask.get_path_name() if mask else 'none for this animal friend'}")
unreal.log_warning(f"BASECOLOR DONE {path} <- {image} ({tex.blueprint_get_size_x()}x{tex.blueprint_get_size_y()})")
