"""Meteor Rush's landing marker (run inside Unreal): /Game/MeteorRush/M_MRRing, for the engine's flat Plane.

Usage: UnrealEditor-Cmd <project> -run=pythonscript "-script=ue_make_meteor_ring.py"

Unlit, translucent, two-sided. From the plane's middle out to its edge (texture coordinates): a glowing band
"Thickness" wide at the edge, soft on both sides, plus a faint "Fill" over the inside. Emissive = "Color" x "Glow",
opacity = (band + fill) x "Opacity".
"""
import unreal

U = unreal
eal = U.EditorAssetLibrary
mel = U.MaterialEditingLibrary
path = "/Game/MeteorRush/M_MRRing"
if eal.does_asset_exist(path):
    eal.delete_asset(path)
mat = U.AssetToolsHelpers.get_asset_tools().create_asset("M_MRRing", "/Game/MeteorRush", U.Material, U.MaterialFactoryNew())
mat.set_editor_property("blend_mode", U.BlendMode.BLEND_TRANSLUCENT)
mat.set_editor_property("shading_model", U.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property("two_sided", True)


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


def smooth(lo, hi, value, x, y):
    n = node(U.MaterialExpressionSmoothStep, x, y)
    link(lo, n, "Min")
    link(hi, n, "Max")
    link(value, n, "Value")
    return n


# Distance from the middle: 0 there, 1 at the plane's edge
uv = node(U.MaterialExpressionTextureCoordinate, -1600, 0)
dist_node = node(U.MaterialExpressionDistance, -1400, 0)
link(uv, dist_node, "A")
link(node(U.MaterialExpressionConstant2Vector, -1600, 150, r=0.5, g=0.5), dist_node, "B")
dist = op(U.MaterialExpressionMultiply, dist_node, const(2.0, -1400, 150), -1250, 0)

thickness = node(U.MaterialExpressionScalarParameter, -1600, 300, parameter_name="Thickness", default_value=0.08)
soft = const(0.03, -1600, 420)
inner_edge = op(U.MaterialExpressionSubtract, const(1.0, -1450, 300), thickness, -1300, 300)
# The band: rising in just inside 1 - Thickness, falling out at the edge
rise = smooth(op(U.MaterialExpressionSubtract, inner_edge, soft, -1150, 260), inner_edge, dist, -950, 200)
fall = op(U.MaterialExpressionSubtract, const(1.0, -950, 380),
          smooth(op(U.MaterialExpressionSubtract, const(1.0, -1150, 420), soft, -1050, 420), const(1.0, -1150, 480), dist, -950, 440), -800, 400)
band = op(U.MaterialExpressionMultiply, rise, fall, -650, 250)
# The faint inside
inside = op(U.MaterialExpressionSubtract, const(1.0, -950, 600), smooth(inner_edge, op(U.MaterialExpressionAdd, inner_edge, soft, -1100, 640), dist, -950, 640), -800, 600)
fill = node(U.MaterialExpressionScalarParameter, -800, 760, parameter_name="Fill", default_value=0.12)
cover = op(U.MaterialExpressionAdd, band, op(U.MaterialExpressionMultiply, inside, fill, -650, 650), -500, 400)
opacity = node(U.MaterialExpressionScalarParameter, -500, 600, parameter_name="Opacity", default_value=1.0)
mel.connect_material_property(op(U.MaterialExpressionMultiply, cover, opacity, -300, 450), "", U.MaterialProperty.MP_OPACITY)

color = node(U.MaterialExpressionVectorParameter, -500, -200, parameter_name="Color", default_value=U.LinearColor(0.95, 0.4, 0.6, 1.0))
glow = node(U.MaterialExpressionScalarParameter, -500, -50, parameter_name="Glow", default_value=2.0)
mel.connect_material_property(op(U.MaterialExpressionMultiply, color, glow, -300, -100), "", U.MaterialProperty.MP_EMISSIVE_COLOR)
mel.recompile_material(mat)
eal.save_asset(path)
unreal.log("METEORRING DONE")
