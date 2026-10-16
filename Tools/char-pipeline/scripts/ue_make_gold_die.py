"""The tiger's ultimate die look (run inside Unreal): /Game/Props/Dice/M_DiceGold, polished glowing gold for SM_Dice's
body slots (the pips keep M_DicePip, which URRRandomOverheadDieComponent tints deep red while the ultimate die is up).

Usage: UnrealEditor-Cmd <project> -run=pythonscript "-script=ue_make_gold_die.py"

Base color gold, metallic, fairly smooth; emissive = the gold x "Glow" (0.35), which the component pulses.
"""
import unreal

U = unreal
eal = U.EditorAssetLibrary
mel = U.MaterialEditingLibrary
path = "/Game/Props/Dice/M_DiceGold"
if eal.does_asset_exist(path):
    eal.delete_asset(path)
mat = U.AssetToolsHelpers.get_asset_tools().create_asset("M_DiceGold", "/Game/Props/Dice", U.Material, U.MaterialFactoryNew())


def node(cls, x, y, **props):
    n = mel.create_material_expression(mat, cls, x, y)
    for key, value in props.items():
        n.set_editor_property(key, value)
    return n


gold = node(U.MaterialExpressionVectorParameter, -600, 0, parameter_name="Color", default_value=U.LinearColor(0.85, 0.42, 0.03, 1.0))
glow = node(U.MaterialExpressionScalarParameter, -600, 250, parameter_name="Glow", default_value=0.35)
lit = node(U.MaterialExpressionMultiply, -350, 200)
mel.connect_material_expressions(gold, "", lit, "A")
mel.connect_material_expressions(glow, "", lit, "B")
mel.connect_material_property(gold, "", U.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(lit, "", U.MaterialProperty.MP_EMISSIVE_COLOR)
mel.connect_material_property(node(U.MaterialExpressionConstant, -350, 350, r=0.9), "", U.MaterialProperty.MP_METALLIC)
mel.connect_material_property(node(U.MaterialExpressionConstant, -350, 450, r=0.35), "", U.MaterialProperty.MP_ROUGHNESS)
mel.recompile_material(mat)
eal.save_asset(path)
mesh = U.load_asset("/Game/Props/Dice/SM_Dice")
unreal.log("GOLDDIE slots " + ", ".join(f"{s.get_editor_property('material_slot_name')}={s.get_editor_property('material_interface').get_name() if s.get_editor_property('material_interface') else None}" for s in mesh.get_editor_property("static_materials")))
unreal.log("GOLDDIE DONE")
