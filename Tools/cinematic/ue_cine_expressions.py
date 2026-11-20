# Cinematic (2026-11-19): the expression textures (char-pipeline/scripts/blender_expression.py bakes) into Unreal as
# /Game/Cinematics/FoxVsTiger/Expressions/T_<Species>_Expr_<Name>, and for each a copy of the species' body material with
# its base color texture swapped (URRRandomCineLibrary::ReplaceMaterialTexture): M_<Species>_Expr_<Name>.
# Run with UnrealEditor-Cmd -run=pythonscript.
import os

import unreal

# (2026-11-20: RR_CINE_EXPR_SRC = the folder of T_<Species>_Expr_<Name>.png; the remade fox and tiger: input/expressions_new)
SRC = os.environ.get("RR_CINE_EXPR_SRC", "C:/work/char-pipeline/input/expressions")
DEST = "/Game/Cinematics/FoxVsTiger/Expressions"
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
for f in sorted(os.listdir(SRC)):
    if not (f.startswith("T_") and "_Expr_" in f and f.endswith(".png")):
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
        fn = unreal.RRRandomCineLibrary.replace_material_texture(unreal.load_asset(f"{DEST}/{fur_name}"), base, tex)
        eal.save_asset(f"{DEST}/{fur_name}")
        unreal.log(f"CINEEXPR {fur_name}: {fn} texture sample(s) now {name}")
    unreal.log(f"CINEEXPR {mat_name}: {n} texture sample(s) now {name}")
unreal.log("CINEEXPR DONE")
