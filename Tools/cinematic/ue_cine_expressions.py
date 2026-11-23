# Cinematic (2026-11-19): the expression textures (char-pipeline/scripts/blender_expression.py bakes) into Unreal as
# /Game/Cinematics/FoxVsTiger/Expressions/T_<Species>_Expr_<Name>, and for each a copy of the species' body material with
# its base color texture swapped (URRRandomCineLibrary::ReplaceMaterialTexture): M_<Species>_Expr_<Name>.
# Run with UnrealEditor-Cmd -run=pythonscript.
#
# 2026-11-23 (the user: "캐릭터 입이나 눈 위에 털이 생기지 않도록 별도의 uv맵 같은거 만들어서 얼굴 텍스쳐 갈아낄때 마다
# 같이 적용해줘"): every expression texture brings its own fur mask, T_<Species>_Expr_<Name>_FurMask (made from that
# picture by char-pipeline/scripts/blender_fur_mask.py through ue_fur_mask_lib.py; the .png is left beside the
# expression's), and the expression's fur material M_<Species>Fur_Expr_<Name> takes both: the expression as its base
# colour and that mask in place of the game face's (T_<Species>_BaseColor_FurMask). So wide scared eyes, a blink or an
# open mouth each keep the fur off exactly where they are. Species without a fur mask (fur_chars.json) are as before.
import importlib.util
import os

import unreal

# (2026-11-20: RR_CINE_EXPR_SRC = the folder of T_<Species>_Expr_<Name>.png; the remade fox and tiger: input/expressions_new)
SRC = os.environ.get("RR_CINE_EXPR_SRC", "C:/work/char-pipeline/input/expressions")
SCRIPTS = os.environ.get("RR_CHAR_SCRIPTS", "C:/work/char-pipeline/scripts")
DEST = "/Game/Cinematics/FoxVsTiger/Expressions"
spec = importlib.util.spec_from_file_location("ue_fur_mask_lib", os.path.join(SCRIPTS, "ue_fur_mask_lib.py"))
furlib = importlib.util.module_from_spec(spec)
spec.loader.exec_module(furlib)
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
for f in sorted(os.listdir(SRC)):
    if not (f.startswith("T_") and "_Expr_" in f and f.endswith(".png")) or f.endswith(furlib.SUFFIX + ".png"):
        continue
    name = f[:-4]
    species = name.split("_")[1]
    task = unreal.AssetImportTask()
    task.filename = f"{SRC}/{f}"
    task.destination_path = DEST
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    tools.import_asset_tasks([task])
    tex = unreal.load_asset(f"{DEST}/{name}")
    mask = furlib.texture_with_mask(species, f"{SRC}/{f}", DEST, name)
    mat_name = name.replace("T_", "M_", 1)
    if eal.does_asset_exist(f"{DEST}/{mat_name}"):
        eal.delete_asset(f"{DEST}/{mat_name}")
    eal.duplicate_asset(f"/Game/Characters/{species}/M_{species}", f"{DEST}/{mat_name}")
    mat = unreal.load_asset(f"{DEST}/{mat_name}")
    base = unreal.load_asset(f"/Game/Characters/{species}/Textures/T_{species}_BaseColor")
    n = unreal.RRRandomCineLibrary.replace_material_texture(mat, base, tex)
    eal.save_asset(f"{DEST}/{mat_name}")
    # (2026-11-20) the shell fur's material with the same expression: the strands over the mouth draw the body texture
    fur_src = f"/Game/Characters/{species}/M_{species}Fur"
    if eal.does_asset_exist(fur_src):
        fur_name = mat_name.replace(f"M_{species}_", f"M_{species}Fur_", 1)
        if eal.does_asset_exist(f"{DEST}/{fur_name}"):
            eal.delete_asset(f"{DEST}/{fur_name}")
        eal.duplicate_asset(fur_src, f"{DEST}/{fur_name}")
        fur = unreal.load_asset(f"{DEST}/{fur_name}")
        fn = unreal.RRRandomCineLibrary.replace_material_texture(fur, base, tex)
        mn = 0
        if mask:
            base_mask = furlib.mask_of_asset(species, f"/Game/Characters/{species}/Textures/T_{species}_BaseColor")
            mn = unreal.RRRandomCineLibrary.replace_material_texture(fur, base_mask, mask) if base_mask else 0
            if mn == 0:
                unreal.log_warning(f"CINEEXPR {fur_name}: M_{species}Fur reads no fur mask; rebuild it first "
                                   f"(ue_import_fur_shells.py \"{species}|||\")")
        eal.save_asset(f"{DEST}/{fur_name}")
        unreal.log(f"CINEEXPR {fur_name}: {fn} texture sample(s) now {name}"
                   + (f", {mn} fur mask sample(s) now {name}{furlib.SUFFIX}" if mask else " (this species has no fur mask)"))
    unreal.log(f"CINEEXPR {mat_name}: {n} texture sample(s) now {name}")
unreal.log("CINEEXPR DONE")
