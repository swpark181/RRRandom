"""Firework spark material for the results screen (run inside Unreal).

Usage: UnrealEditor-Cmd <project> -run=pythonscript "-script=ue_make_fireworks.py /Game/Effects/Fireworks/M_FireworkSpark"

Unlit and glowing: emissive = per-instance custom data 0..2 (the spark's color times its brightness, set by
ARRRandomFirework each frame) x "Glow". Unlit, so the toon post process passes it through untouched (black base color).
"""
import sys

import unreal

path = " ".join(sys.argv[1:]).strip()
folder, name = path.rsplit("/", 1)
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
U = unreal


def log(msg):
    unreal.log(f"FIREWORKS {msg}")


if eal.does_asset_exist(path):
    eal.delete_asset(path)
mat = U.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, U.Material, U.MaterialFactoryNew())
mat.set_editor_property("shading_model", U.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property("used_with_instanced_static_meshes", True)


def node(cls, x, y, **props):
    n = mel.create_material_expression(mat, cls, x, y)
    for key, value in props.items():
        n.set_editor_property(key, value)
    return n


channels = [node(U.MaterialExpressionPerInstanceCustomData, -600, -150 + 120 * i, data_index=i, const_default_value=1.0)
            for i in range(3)]
rg = node(U.MaterialExpressionAppendVector, -400, -100)
mel.connect_material_expressions(channels[0], "", rg, "A")
mel.connect_material_expressions(channels[1], "", rg, "B")
rgb = node(U.MaterialExpressionAppendVector, -250, -50)
mel.connect_material_expressions(rg, "", rgb, "A")
mel.connect_material_expressions(channels[2], "", rgb, "B")
glow = node(U.MaterialExpressionScalarParameter, -250, 100, parameter_name="Glow", default_value=6.0)
emissive = node(U.MaterialExpressionMultiply, -80, 0)
mel.connect_material_expressions(rgb, "", emissive, "A")
mel.connect_material_expressions(glow, "", emissive, "B")
mel.connect_material_property(emissive, "", U.MaterialProperty.MP_EMISSIVE_COLOR)
mel.recompile_material(mat)
eal.save_asset(path)
log(f"saved {path}")
