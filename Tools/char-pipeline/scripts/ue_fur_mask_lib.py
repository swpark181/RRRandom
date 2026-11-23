"""A face texture and its fur mask travel together (used by the Unreal import scripts; 2026-11-23).

The user: "캐릭터 입이나 눈 위에 털이 생기지 않도록 별도의 uv맵 같은거 만들어서 얼굴 텍스쳐 갈아낄때 마다 같이 적용해줘."
Every texture that carries a face (/Game/Characters/<Name>/Textures/T_<Name>_BaseColor, each expression's
T_<Name>_Expr_<X>) has a twin, <texture>_FurMask, beside it: 1 = fur, 0 = bare (blender_fur_mask.py). M_<Name>Fur reads
it through its "FurMaskTex" texture parameter. Whatever brings a face texture in calls texture_with_mask() here, so the
twin is made from that very picture (Blender is started for it: Unreal's Python has no numpy) and comes in with it:
  ue_import_basecolor.py   the game's face replaced            -> T_<Name>_BaseColor_FurMask (same asset, the fur follows)
  ue_import_fur_shells.py  the fur (re)built                    -> the base mask made if missing, wired into M_<Name>Fur
  ue_cine_expressions.py   the cinematic's expression textures  -> T_<Name>_Expr_<X>_FurMask in M_<Name>Fur_Expr_<X>
Only animal friends with "rig" and "mask" in fur_chars.json have masks (now: the fox); for the others these return None
and nothing changes.

Not a commandlet script itself: load it with load_lib() below, copied into each script (the commandlet runs a script
with exec, and the scripts' folder is not on sys.path).
"""
import json
import os
import subprocess

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
BLENDER = os.environ.get("RR_BLENDER", "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe")
SUFFIX = "_FurMask"


def log(msg):
    unreal.log_warning(f"FURMASKLIB {msg}")


def config(name):
    with open(os.path.join(HERE, "fur_chars.json"), encoding="utf-8") as f:
        return json.load(f).get(name) or {}


def has_mask(name):
    cfg = config(name)
    return bool(cfg.get("rig") and "mask" in cfg)


def mask_png_of(image):
    return os.path.splitext(image)[0] + SUFFIX + ".png"


def make_mask(name, image, force=False):
    """The mask picture of a face picture, beside it (<image>_FurMask.png); made again when the picture, the rig, the
    options or the script are newer. None when the animal friend has no mask."""
    cfg = config(name)
    if not has_mask(name):
        return None
    rig = os.path.join(ROOT, cfg["rig"]).replace("\\", "/")
    script = os.path.join(HERE, "blender_fur_mask.py")
    out = mask_png_of(image)
    newest = max(os.path.getmtime(p) for p in (image, rig, script, os.path.join(HERE, "fur_chars.json")))
    if not force and os.path.exists(out) and os.path.getmtime(out) >= newest:
        return out
    cmd = [BLENDER, "-b", "--factory-startup", "--python", script, "--", rig, image, out] + list(cfg["mask"])
    run = subprocess.run(cmd, capture_output=True, text=True, timeout=900)
    lines = [ln for ln in run.stdout.splitlines() if ln.startswith("FURMASK")]
    if not lines or not lines[-1].startswith("FURMASK wrote") or not os.path.exists(out):
        raise RuntimeError(f"no fur mask for {image}: {run.stdout[-600:]} {run.stderr[-600:]}")
    log(f"{os.path.basename(out)}: {'; '.join(ln[8:] for ln in lines[:-1] if 'texels' in ln)}")
    return out


def import_mask(png, dest, asset_name):
    """The mask picture as a linear, single-channel texture asset."""
    task = unreal.AssetImportTask()
    task.filename = png
    task.destination_path = dest
    task.destination_name = asset_name
    task.automated = True
    task.replace_existing = True
    task.save = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    tex = unreal.load_asset(f"{dest}/{asset_name}")
    tex.set_editor_property("srgb", False)
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
    unreal.EditorAssetLibrary.save_asset(f"{dest}/{asset_name}")
    return tex


def texture_with_mask(name, image, dest, tex_asset_name, force=False):
    """Call after bringing <image> in as <dest>/<tex_asset_name>: makes and brings in its twin. Returns the mask
    texture asset, or None when this animal friend has no fur mask."""
    png = make_mask(name, image, force)
    if not png:
        return None
    tex = import_mask(png, dest, tex_asset_name + SUFFIX)
    log(f"{dest}/{tex_asset_name}{SUFFIX} <- {png}")
    return tex


def mask_of_asset(name, tex_path):
    """The mask twin of a texture asset already in the project, made (again) from the file the texture was imported
    from, so a twin left over from an older face never stays; when that file is gone, the twin already beside the
    texture, if any. None when this animal friend has no mask or none can be made."""
    if not has_mask(name):
        return None
    eal = unreal.EditorAssetLibrary
    tex = unreal.load_asset(tex_path)
    data = tex.get_editor_property("asset_import_data") if tex else None
    src = data.get_first_filename() if data else ""
    if src and os.path.exists(src):
        dest, asset = tex_path.rsplit("/", 1)
        return texture_with_mask(name, src, dest, asset)
    if eal.does_asset_exist(tex_path + SUFFIX):
        log(f"{tex_path}: its source picture '{src}' is gone, keeping the fur mask already there")
        return unreal.load_asset(tex_path + SUFFIX)
    log(f"{tex_path}: its source picture '{src}' is gone, no fur mask made")
    return None


def fur_material_reads_mask(name):
    """Whether M_<Name>Fur already has the "FurMaskTex" parameter (older ones don't: rebuild with "<Name>|||")."""
    mat = unreal.load_asset(f"/Game/Characters/{name}/M_{name}Fur")
    if not mat:
        return False
    return "FurMaskTex" in [str(n) for n in unreal.MaterialEditingLibrary.get_texture_parameter_names(mat)]
