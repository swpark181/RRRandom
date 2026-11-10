"""Dice Town's mantle lava (run inside Unreal): /Game/Effects/Lava/M_Lava, on the boxes of lava that fill a crater once it
breaks through the island's floor (ARRRandomLava, round 89).

Usage: UnrealEditor-Cmd <project> -run=pythonscript "-script=C:/work/char-pipeline/scripts/ue_make_lava.py"

Opaque, unlit, for instanced static meshes: glowing candy jam rather than real rock lava. Two blotchy noises in world
space (so neighbouring boxes run on into each other) drift slowly over the surface: one mixes a deep strawberry pink into
orange, the other, sharpened, lays bright gold veins over it. Emissive = that x "Glow", kept under 1 (any brighter and it washes out to pastel under the sunny sky).
"""
import unreal

U = unreal
eal = U.EditorAssetLibrary
mel = U.MaterialEditingLibrary
path = "/Game/Effects/Lava/M_Lava"
if eal.does_asset_exist(path):
    eal.delete_asset(path)
mat = U.AssetToolsHelpers.get_asset_tools().create_asset("M_Lava", "/Game/Effects/Lava", U.Material, U.MaterialFactoryNew())
mat.set_editor_property("shading_model", U.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property("used_with_instanced_static_meshes", True)


def node(cls, x, y, **props):
    n = mel.create_material_expression(mat, cls, x, y)
    for key, value in props.items():
        n.set_editor_property(key, value)
    return n


def link(a, b, pin, a_pin=""):
    mel.connect_material_expressions(a, a_pin, b, pin)


def op(cls, a, b, x, y):
    n = node(cls, x, y)
    link(a, n, "A")
    link(b, n, "B")
    return n


def const(value, x, y):
    return node(U.MaterialExpressionConstant, x, y, r=value)


def const3(r, g, b, x, y):
    return node(U.MaterialExpressionConstant3Vector, x, y, constant=U.LinearColor(r, g, b, 1.0))


glow = node(U.MaterialExpressionScalarParameter, -400, 300, parameter_name="Glow", default_value=0.6)
pink = node(U.MaterialExpressionVectorParameter, -1000, -300, parameter_name="Deep", default_value=U.LinearColor(0.85, 0.06, 0.16, 1.0))
orange = node(U.MaterialExpressionVectorParameter, -1000, -150, parameter_name="Hot", default_value=U.LinearColor(1.0, 0.32, 0.03, 1.0))
gold = node(U.MaterialExpressionVectorParameter, -1000, 0, parameter_name="Vein", default_value=U.LinearColor(1.0, 0.6, 0.1, 1.0))

# World position, drifting: the blotches crawl one way, the veins another (and a little faster)
world = node(U.MaterialExpressionWorldPosition, -2000, 200)
time = node(U.MaterialExpressionTime, -2000, 400)
drift_a = op(U.MaterialExpressionMultiply, time, const3(18.0, 11.0, 6.0, -1850, 480), -1700, 420)
drift_b = op(U.MaterialExpressionMultiply, time, const3(-27.0, 14.0, -9.0, -1850, 600), -1700, 560)
pos_a = op(U.MaterialExpressionAdd, world, drift_a, -1550, 300)
pos_b = op(U.MaterialExpressionAdd, world, drift_b, -1550, 520)

blotch = node(U.MaterialExpressionNoise, -1350, 300, scale=0.004, levels=3, output_min=0.0, output_max=1.0)
link(pos_a, blotch, "Position")
veins = node(U.MaterialExpressionNoise, -1350, 520, scale=0.009, levels=2, output_min=0.0, output_max=1.0)
link(pos_b, veins, "Position")

# Pink to orange by the blotches; gold where the veins peak (cut and sharpened)
base = node(U.MaterialExpressionLinearInterpolate, -800, -200)
link(pink, base, "A")
link(orange, base, "B")
link(blotch, base, "Alpha")
cut = op(U.MaterialExpressionSubtract, veins, const(0.55, -1150, 640), -1000, 560)
sharp = node(U.MaterialExpressionSaturate, -850, 560)
link(op(U.MaterialExpressionMultiply, cut, const(3.0, -1000, 680), -900, 640), sharp, "")
lava = node(U.MaterialExpressionLinearInterpolate, -600, -50)
link(base, lava, "A")
link(gold, lava, "B")
link(sharp, lava, "Alpha")

lit = op(U.MaterialExpressionMultiply, lava, glow, -300, 0)
mel.connect_material_property(lit, "", U.MaterialProperty.MP_EMISSIVE_COLOR)

mel.recompile_material(mat)
eal.save_asset(mat.get_path_name())
unreal.log("LAVA M_Lava saved")
