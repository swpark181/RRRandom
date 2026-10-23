"""Make an animal friend's fur read fluffier (run inside Unreal; edits the material in place).

Usage: UnrealEditor-Cmd <project> -run=pythonscript "-script=ue_fur_rim.py /Game/Characters/RedFox/M_RedFox"

Round 75 (the Midjourney C-3 red fox, user: "털이 복실거리는 느낌" → material only, then "조금만 더" → B + A). The toon
post process (ue_make_toon.py) draws ink on characters from their world normals, so a fur normal map would sprinkle ink
all over the body; the fur is done in Emissive and Opacity Mask instead, which the post process leaves alone.

Fur mask: texels that are clearly red ((R - max(G, B) - "FurMaskFrom") x "FurMaskSharpness") or clearly white fur
((min(R, G, B) - "FurWhiteFrom") x 8, times how grey they are, so the cream shorts stay out): the red fur, pink inner ears,
white cheeks and chin. The mint top, cream shorts, brown belt and black paws are left alone. "FurWhiteFrom" 2 drops white.

1. Rim glow (A):  emissive += base color x "FurRimColor" x (Fresnel ^ "FurRimPower" x "FurRimStrength" + "FurSheen") x mask
   The base color is the painted fur, so the edges light up strand by strand, like backlit fluff.
2. Fuzzy edge (B): the material becomes Masked and, where the surface turns away from the camera (Fresnel ^
   "FurEdgePower"), pixels are cut where a fine UV noise ("FurEdgeTiling" cells across the texture) is low:
   opacity = 1 - edge x "FurEdgeAmount" x (1 - noise) x mask, clipped at 0.5. The silhouette turns into stray tufts and the
   toon silhouette line follows them. "FurEdgeAmount" 0 turns it off. The noise sits on the UVs, so it moves with the
   skin instead of swimming as she walks.
Whatever fed Emissive before (the hit flash, ue_fox_hitflash.py) is kept and added to. Running it again replaces the
earlier fur nodes rather than stacking them (the old ones stay in the graph unconnected).
"""
import sys

import unreal

args = " ".join(sys.argv[1:]).split("|")
path = args[0].strip()
DITHER = "dither" in [a.strip() for a in args[1:]]  # "<material>|dither": dithered opacity mask (needs temporal AA)
mel = unreal.MaterialEditingLibrary
U = unreal
EMISSIVE = U.MaterialProperty.MP_EMISSIVE_COLOR
OPACITY_MASK = U.MaterialProperty.MP_OPACITY_MASK


def log(msg):
    unreal.log(f"FURRIM {msg}")


mat = unreal.load_asset(path)
source = mel.get_material_property_input_node(mat, EMISSIVE)
source_out = mel.get_material_property_input_node_output_name(mat, EMISSIVE) if source else ""
if isinstance(source, U.MaterialExpressionAdd) and source.get_editor_property("desc") == "FurRim":
    source = mel.get_inputs_for_material_expression(mat, source)[0]
    source_out = ""
log(f"emissive came from {source.get_name() if source else 'nothing'}")

# The base color texture ue_import_animal.py put next to the material (Textures/T_<Name>_BaseColor), sampled again here
folder, mat_name = path.rsplit("/", 1)
tex = unreal.load_asset(f"{folder}/Textures/T_{mat_name[2:]}_BaseColor")
if tex is None:
    raise RuntimeError("no base color texture in " + path)


def node(cls, x, y, **props):
    n = mel.create_material_expression(mat, cls, x, y)
    for key, value in props.items():
        n.set_editor_property(key, value)
    return n


def link(a, a_out, b, b_in):
    if not mel.connect_material_expressions(a, a_out, b, b_in):
        raise RuntimeError(f"could not connect {a.get_name()} into {b.get_name()}.{b_in}")


def param(name, value, x, y):
    return node(U.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=value)


def op(cls, a, b, x, y, a_out="", b_out=""):
    n = node(cls, x, y)
    link(a, a_out, n, "A")
    link(b, b_out, n, "B")
    return n


def one(cls, a, x, y, a_out=""):
    n = node(cls, x, y)
    link(a, a_out, n, "")
    return n


X, Y = -2200, 1400
base = node(U.MaterialExpressionTextureSample, X, Y, texture=tex, sampler_type=U.MaterialSamplerType.SAMPLERTYPE_COLOR)
ch = {}
for i, c in enumerate("rgb"):
    ch[c] = node(U.MaterialExpressionComponentMask, X + 250, Y + i * 80, r=c == "r", g=c == "g", b=c == "b", a=False)
    link(base, "RGB", ch[c], "")

# Red fur: (R - max(G, B) - from) x sharpness
gb_max = op(U.MaterialExpressionMax, ch["g"], ch["b"], X + 450, Y + 120)
redness = op(U.MaterialExpressionSubtract, ch["r"], gb_max, X + 600, Y + 40)
shifted = op(U.MaterialExpressionSubtract, redness, param("FurMaskFrom", 0.18, X + 600, Y + 160), X + 780, Y + 60)
red_mask = one(U.MaterialExpressionSaturate,
               op(U.MaterialExpressionMultiply, shifted, param("FurMaskSharpness", 6.0, X + 780, Y + 180), X + 950, Y + 80),
               X + 1100, Y + 80)
# White fur: (min(R, G, B) - from) x 8, times greyness (1 - (max - min) x 6)
rgb_min = op(U.MaterialExpressionMin, ch["r"], op(U.MaterialExpressionMin, ch["g"], ch["b"], X + 450, Y + 260), X + 600, Y + 260)
rgb_max = op(U.MaterialExpressionMax, ch["r"], gb_max, X + 600, Y + 340)
bright = op(U.MaterialExpressionSubtract, rgb_min, param("FurWhiteFrom", 0.78, X + 600, Y + 420), X + 780, Y + 300)
bright = one(U.MaterialExpressionSaturate, op(U.MaterialExpressionMultiply, bright, node(U.MaterialExpressionConstant, X + 780, Y + 400, r=8.0), X + 950, Y + 300), X + 1100, Y + 300)
spread = op(U.MaterialExpressionMultiply, op(U.MaterialExpressionSubtract, rgb_max, rgb_min, X + 780, Y + 480),
            node(U.MaterialExpressionConstant, X + 780, Y + 560, r=6.0), X + 950, Y + 480)
grey = one(U.MaterialExpressionOneMinus, spread, X + 1100, Y + 480)
white_mask = one(U.MaterialExpressionSaturate, op(U.MaterialExpressionMultiply, bright, grey, X + 1250, Y + 380), X + 1400, Y + 380)
fur_mask = op(U.MaterialExpressionMax, red_mask, white_mask, X + 1550, Y + 200)

# 1. Rim glow: Fresnel ^ power x strength + sheen
fresnel = node(U.MaterialExpressionFresnel, X + 250, Y + 700)
link(param("FurRimPower", 3.5, X + 50, Y + 760), "", fresnel, "ExponentIn")
rim = op(U.MaterialExpressionMultiply, fresnel, param("FurRimStrength", 1.3, X + 250, Y + 820), X + 450, Y + 720)
amount = op(U.MaterialExpressionAdd, rim, param("FurSheen", 0.06, X + 450, Y + 840), X + 600, Y + 740)
tint = node(U.MaterialExpressionVectorParameter, X + 600, Y + 860, parameter_name="FurRimColor",
            default_value=U.LinearColor(1.0, 0.85, 0.82, 1.0))
lit = op(U.MaterialExpressionMultiply, op(U.MaterialExpressionMultiply, base, tint, X + 780, Y + 680, a_out="RGB"), amount,
         X + 950, Y + 700)
glow = op(U.MaterialExpressionMultiply, lit, fur_mask, X + 1700, Y + 500)
total = node(U.MaterialExpressionAdd, X + 1900, Y + 300, desc="FurRim")
if source:
    link(source, source_out, total, "A")
else:
    link(node(U.MaterialExpressionConstant3Vector, X + 1700, Y + 300, constant=U.LinearColor(0, 0, 0, 1)), "", total, "A")
link(glow, "", total, "B")
mel.connect_material_property(total, "", EMISSIVE)

# 2. Fuzzy edge. edge = how far the surface has turned from the camera past "FurEdgeStart", over "FurEdgeWidth"
#    ((1 - N.V - start) / width, clamped: 0 face on, 1 at the outline); a pixel is cut where noise < edge x amount, so
#    the outermost band loses most of its pixels, thinning into tufts a few pixels deep. opacity = noise - edge x amount x
#    mask + 0.5, clipped at 0.5. (A first try cut only where Fresnel^4 x (1 - noise) > 0.5: the last one or two pixels,
#    invisible in the game.)
fres1 = node(U.MaterialExpressionFresnel, X + 250, Y + 1050)
link(node(U.MaterialExpressionConstant, X + 50, Y + 1110, r=1.0), "", fres1, "ExponentIn")
edge = op(U.MaterialExpressionSubtract, fres1, param("FurEdgeStart", 0.5, X + 250, Y + 1150), X + 450, Y + 1060)
edge = op(U.MaterialExpressionDivide, edge, param("FurEdgeWidth", 0.45, X + 450, Y + 1160), X + 600, Y + 1070)
edge = one(U.MaterialExpressionSaturate, edge, X + 750, Y + 1070)
uv = node(U.MaterialExpressionTextureCoordinate, X + 50, Y + 1250)
uv_scaled = op(U.MaterialExpressionMultiply, uv, param("FurEdgeTiling", 220.0, X + 50, Y + 1330), X + 250, Y + 1260)
pos = op(U.MaterialExpressionAppendVector, uv_scaled, node(U.MaterialExpressionConstant, X + 250, Y + 1360, r=0.0), X + 450, Y + 1280)
noise = node(U.MaterialExpressionNoise, X + 600, Y + 1250, scale=1.0, levels=1, output_min=0.0, output_max=1.0, turbulence=False)
if not mel.connect_material_expressions(pos, "", noise, ""):  # its first input is Position
    link(pos, "", noise, "Position")
# Round 75 fix (user: "자글거리는거 해결해줘"): at 600 cells the tufts were smaller than a pixel and every edge pixel flipped
# on or off by itself. Now the cut fades out wherever a noise cell covers fewer than ~2.5 pixels (footprint = how many cells
# one pixel spans, from screen derivatives of the scaled UVs), so distant views keep a clean outline, and the opacity is a
# soft ramp ("FurEdgeSoftness") dithered by the temporal AA instead of a hard 0/1 step.
dx = node(U.MaterialExpressionDDX, X + 450, Y + 1420)
link(uv_scaled, "", dx, "")
dy = node(U.MaterialExpressionDDY, X + 450, Y + 1500)
link(uv_scaled, "", dy, "")
foot = op(U.MaterialExpressionAdd,
          one(U.MaterialExpressionSquareRoot, op(U.MaterialExpressionDotProduct, dx, dx, X + 600, Y + 1420), X + 750, Y + 1420),
          one(U.MaterialExpressionSquareRoot, op(U.MaterialExpressionDotProduct, dy, dy, X + 600, Y + 1500), X + 750, Y + 1500),
          X + 900, Y + 1460)
near = op(U.MaterialExpressionSubtract, param("FurEdgeFadeFrom", 0.8, X + 900, Y + 1560), foot, X + 1050, Y + 1480)
near = one(U.MaterialExpressionSaturate, op(U.MaterialExpressionDivide, near, node(U.MaterialExpressionConstant, X + 1050, Y + 1580, r=0.4), X + 1200, Y + 1480), X + 1350, Y + 1480)
cut = op(U.MaterialExpressionMultiply, edge, param("FurEdgeAmount", 0.8, X + 750, Y + 1170), X + 950, Y + 1080)
cut = op(U.MaterialExpressionMultiply, cut, fur_mask, X + 1700, Y + 1100)
cut = op(U.MaterialExpressionMultiply, cut, near, X + 1700, Y + 1250)
# hole = (cut - noise) x softness, clamped: exactly 0 wherever noise >= cut, so the inside of the body stays fully opaque
# (an earlier ramp centred on 0.5 left the whole body at partial opacity and the dither speckled it white)
hole = op(U.MaterialExpressionSubtract, cut, noise, X + 1850, Y + 1150)
hole = op(U.MaterialExpressionMultiply, hole, param("FurEdgeSoftness", 3.0, X + 1850, Y + 1300), X + 2000, Y + 1150)
hole = one(U.MaterialExpressionSaturate, hole, X + 2150, Y + 1150)
keep = one(U.MaterialExpressionOneMinus, hole, X + 2300, Y + 1150)
mel.connect_material_property(keep, "", OPACITY_MASK)
mat.set_editor_property("dither_opacity_mask", DITHER)
mat.set_editor_property("blend_mode", U.BlendMode.BLEND_MASKED)
mat.set_editor_property("opacity_mask_clip_value", 0.5)

mel.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset(path)
log(f"DONE (texture {tex.get_name()}, masked)")
