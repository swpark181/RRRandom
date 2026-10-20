"""The fox's invisibility (run inside Unreal): /Game/Effects/DiceFX/M_FXCloak, put on the body and gun while it lasts.

Usage: UnrealEditor-Cmd <project> -run=pythonscript "-script=C:/work/char-pipeline/scripts/ue_make_cloak.py"

Unlit, translucent, for skeletal and static meshes. Only the outline shows: a Fresnel rim (edges facing away from the
camera), rippling like heat haze with bands that run up the body over time, so a still body is a faint shimmer that
catches the eye once it moves. Emissive = "Color" x "Glow" x (rim + "Fill"), opacity = (rim + "Fill") x "Opacity" (set per
screen: a faint violet outline for opponents, a see-through body for itself and its team).
"""
import unreal

U = unreal
eal = U.EditorAssetLibrary
mel = U.MaterialEditingLibrary
path = "/Game/Effects/DiceFX/M_FXCloak"
if eal.does_asset_exist(path):
    eal.delete_asset(path)
mat = U.AssetToolsHelpers.get_asset_tools().create_asset("M_FXCloak", "/Game/Effects/DiceFX", U.Material, U.MaterialFactoryNew())
mat.set_editor_property("blend_mode", U.BlendMode.BLEND_TRANSLUCENT)
mat.set_editor_property("shading_model", U.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property("used_with_skeletal_mesh", True)


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


color = node(U.MaterialExpressionVectorParameter, -1200, -200, parameter_name="Color", default_value=U.LinearColor(0.5, 0.4, 0.95, 1.0))
glow = node(U.MaterialExpressionScalarParameter, -1200, -50, parameter_name="Glow", default_value=1.0)
opacity = node(U.MaterialExpressionScalarParameter, -1200, 500, parameter_name="Opacity", default_value=0.4)
fill = node(U.MaterialExpressionScalarParameter, -1200, 620, parameter_name="Fill", default_value=0.0)

# The rim: Fresnel, a little sharp
fresnel = node(U.MaterialExpressionFresnel, -1200, 150, exponent=3.0, base_reflect_fraction=0.0)

# The haze: bands up the body (world height), drifting upward, between 0.45 and 1
world = node(U.MaterialExpressionWorldPosition, -1600, 300)
height = node(U.MaterialExpressionComponentMask, -1400, 300, r=False, g=False, b=True, a=False)
link(world, height, "")
time = node(U.MaterialExpressionTime, -1600, 420)
phase = op(U.MaterialExpressionAdd, op(U.MaterialExpressionMultiply, height, const(0.09, -1400, 380), -1250, 300),
           op(U.MaterialExpressionMultiply, time, const(-6.0, -1400, 480), -1250, 420), -1100, 340)
wave = node(U.MaterialExpressionSine, -950, 340, period=6.283185)
link(phase, wave, "")
band = op(U.MaterialExpressionAdd, op(U.MaterialExpressionMultiply, wave, const(0.275, -950, 420), -800, 340), const(0.725, -800, 440), -650, 360)

rim = op(U.MaterialExpressionMultiply, fresnel, band, -650, 200)
rim_fill = op(U.MaterialExpressionAdd, rim, fill, -650, 520)
lit = op(U.MaterialExpressionMultiply, op(U.MaterialExpressionMultiply, color, glow, -900, -150), rim_fill, -400, -50)
mel.connect_material_property(lit, "", U.MaterialProperty.MP_EMISSIVE_COLOR)
see = op(U.MaterialExpressionMultiply, rim_fill, opacity, -400, 350)
mel.connect_material_property(see, "", U.MaterialProperty.MP_OPACITY)

mel.recompile_material(mat)
eal.save_asset(mat.get_path_name())
unreal.log("CLOAK M_FXCloak saved")
