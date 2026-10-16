"""Hologram look for the tiger spirit's strike area (run inside Unreal): /Game/Effects/DiceFX/M_FXHolo.

Usage: UnrealEditor-Cmd <project> -run=pythonscript "-script=ue_make_holo.py"

For the engine's /Engine/BasicShapes/Cylinder (radius 50, height 100, centered), scaled to the area: unlit,
translucent (covering the floor by the pattern's strength, so it reads on pale ground), two-sided, drawn over everything. In the cylinder's own space (H = 0 at its bottom .. 1 at its top, R = 0 middle .. 1 rim):
- the wall glows strongest at the ground and fades out going up, crossed by bright scanlines drifting upward;
- the bottom cap (the floor of the area) is a faint fill with rings rippling outward, a bright rim, and dashes turning
  round the rim; the top cap is gone (the wall's fade).
Emissive = "Color" x "Glow" x (0.6 + that), opacity = that, with "Opacity" (0..1) on everything so the game can fade it in and out.
"""
import unreal

U = unreal
eal = U.EditorAssetLibrary
mel = U.MaterialEditingLibrary
path = "/Game/Effects/DiceFX/M_FXHolo"
if eal.does_asset_exist(path):
    eal.delete_asset(path)
mat = U.AssetToolsHelpers.get_asset_tools().create_asset("M_FXHolo", "/Game/Effects/DiceFX", U.Material, U.MaterialFactoryNew())
# Translucent rather than additive: added light vanished on Dice Land's pale floor. Drawn over everything (the giant
# stands right by it)
mat.set_editor_property("blend_mode", U.BlendMode.BLEND_TRANSLUCENT)
mat.set_editor_property("shading_model", U.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property("two_sided", True)
mat.set_editor_property("disable_depth_test", True)
X = [-2400]


def n(cls, **props):
    e = mel.create_material_expression(mat, cls, X[0], len(mat.get_editor_property("expressions") if False else []) * 0)
    X[0] += 120
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def link(a, b, pin, out=""):
    if not mel.connect_material_expressions(a, out, b, pin):
        raise RuntimeError(f"link {a.get_name()} -> {b.get_name()}.{pin}")


def op(cls, a, b, a_out="", b_out=""):
    e = n(cls)
    link(a, e, "A", a_out)
    link(b, e, "B", b_out)
    return e


def const(v):
    return n(U.MaterialExpressionConstant, r=v)


# Position in the cylinder's own space: local = TransformPosition(world -> local)
world = n(U.MaterialExpressionWorldPosition)
local = n(U.MaterialExpressionTransformPosition, transform_source_type=U.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD,
          transform_type=U.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
link(world, local, "")
lz = n(U.MaterialExpressionComponentMask, r=False, g=False, b=True, a=False)
link(local, lz, "")
lxy = n(U.MaterialExpressionComponentMask, r=True, g=True, b=False, a=False)
link(local, lxy, "")
# H = z / 100 + 0.5, R = |xy| / 50
h = op(U.MaterialExpressionAdd, op(U.MaterialExpressionMultiply, lz, const(0.01)), const(0.5))
rlen = n(U.MaterialExpressionLength)
link(lxy, rlen, "")
r = op(U.MaterialExpressionMultiply, rlen, const(0.02))
time = n(U.MaterialExpressionTime)

# Which surface: the caps face up/down (|normal z| = 1), the wall sideways
normal = n(U.MaterialExpressionVertexNormalWS)
nz = n(U.MaterialExpressionComponentMask, r=False, g=False, b=True, a=False)
link(normal, nz, "")
cap = n(U.MaterialExpressionAbs)
link(nz, cap, "")
wallness = op(U.MaterialExpressionSubtract, const(1.0), cap)

# Wall: (1-H)^2 x (0.45 + scanlines)
up = op(U.MaterialExpressionSubtract, const(1.0), h)
up = n(U.MaterialExpressionSaturate) if False else up
fade = op(U.MaterialExpressionMultiply, up, up)
scan_phase = op(U.MaterialExpressionSubtract, op(U.MaterialExpressionMultiply, h, const(9.0)), op(U.MaterialExpressionMultiply, time, const(1.2)))
scan_frac = n(U.MaterialExpressionFrac)
link(scan_phase, scan_frac, "")
# a thin bright band at the start of each step: max(0, 1 - frac*6)
scan = n(U.MaterialExpressionMax)
link(op(U.MaterialExpressionSubtract, const(1.0), op(U.MaterialExpressionMultiply, scan_frac, const(6.0))), scan, "A")
link(const(0.0), scan, "B")
wall = op(U.MaterialExpressionMultiply, fade, op(U.MaterialExpressionAdd, const(0.35), op(U.MaterialExpressionMultiply, scan, const(0.8))))
wall = op(U.MaterialExpressionMultiply, wall, wallness)

# Floor (only the bottom cap: normal z < 0 -> use the cap where H is low)
low = n(U.MaterialExpressionMax)
link(op(U.MaterialExpressionSubtract, const(1.0), op(U.MaterialExpressionMultiply, h, const(4.0))), low, "A")
link(const(0.0), low, "B")
ripple_phase = op(U.MaterialExpressionSubtract, op(U.MaterialExpressionMultiply, r, const(5.0)), time)
ripple_frac = n(U.MaterialExpressionFrac)
link(ripple_phase, ripple_frac, "")
ripple = n(U.MaterialExpressionMax)
link(op(U.MaterialExpressionSubtract, const(1.0), op(U.MaterialExpressionMultiply, ripple_frac, const(4.0))), ripple, "A")
link(const(0.0), ripple, "B")
# rim: bright where R > 0.88
rim = n(U.MaterialExpressionMax)
link(op(U.MaterialExpressionMultiply, op(U.MaterialExpressionSubtract, r, const(0.88)), const(10.0)), rim, "A")
link(const(0.0), rim, "B")
# dashes turning round the rim: frac(angle * 12 / 2pi + time * 0.4) < 0.5, near the rim
ax = n(U.MaterialExpressionComponentMask, r=True, g=False, b=False, a=False)
link(local, ax, "")
ay = n(U.MaterialExpressionComponentMask, r=False, g=True, b=False, a=False)
link(local, ay, "")
angle = n(U.MaterialExpressionArctangent2Fast)
link(ay, angle, "Y")
link(ax, angle, "X")
dash_frac = n(U.MaterialExpressionFrac)
link(op(U.MaterialExpressionAdd, op(U.MaterialExpressionMultiply, angle, const(12.0 / 6.2832)), op(U.MaterialExpressionMultiply, time, const(0.4))), dash_frac, "")
dash = n(U.MaterialExpressionRound)
link(dash_frac, dash, "")
dash_band = n(U.MaterialExpressionMax)
link(op(U.MaterialExpressionMultiply, op(U.MaterialExpressionSubtract, r, const(0.84)), const(12.0)), dash_band, "A")
link(const(0.0), dash_band, "B")
dash_band_c = n(U.MaterialExpressionMin)
link(dash_band, dash_band_c, "A")
link(const(1.0), dash_band_c, "B")
dashes = op(U.MaterialExpressionMultiply, op(U.MaterialExpressionMultiply, dash, dash_band_c), const(0.6))
floor_sum = op(U.MaterialExpressionAdd, op(U.MaterialExpressionAdd, op(U.MaterialExpressionAdd, const(0.12), op(U.MaterialExpressionMultiply, ripple, const(0.4))), rim), dashes)
floor = op(U.MaterialExpressionMultiply, op(U.MaterialExpressionMultiply, floor_sum, cap), low)

amount = op(U.MaterialExpressionAdd, wall, floor)
color = n(U.MaterialExpressionVectorParameter, parameter_name="Color", default_value=U.LinearColor(1.0, 0.32, 0.02, 1.0))
glow = n(U.MaterialExpressionScalarParameter, parameter_name="Glow", default_value=2.2)
opacity = n(U.MaterialExpressionScalarParameter, parameter_name="Opacity", default_value=1.0)
# Bright where the pattern is (brighter still on its strongest lines), covering the floor by as much
lit = op(U.MaterialExpressionMultiply, op(U.MaterialExpressionMultiply, color, glow), op(U.MaterialExpressionAdd, const(0.6), amount))
mel.connect_material_property(lit, "", U.MaterialProperty.MP_EMISSIVE_COLOR)
cover = n(U.MaterialExpressionSaturate)
link(op(U.MaterialExpressionMultiply, op(U.MaterialExpressionMultiply, amount, const(2.2)), opacity), cover, "")
mel.connect_material_property(cover, "", U.MaterialProperty.MP_OPACITY)
mel.recompile_material(mat)
eal.save_asset(path)
unreal.log("HOLO DONE")
