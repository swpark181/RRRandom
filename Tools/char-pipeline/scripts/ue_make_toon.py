"""Cartoon look: flatten a character material and build a toon post-process material (run inside Unreal).

Usage: UnrealEditor-Cmd <project> -run=pythonscript "-script=ue_make_toon.py <character material>|<post-process material>"
  e.g. /Game/Characters/Fox/M_Fox|/Game/PostProcess/M_PP_Toon   (empty character material: post process only)

Character material (edited in place so references keep working): base color texture tinted toward "DiffuseColor"
by "TintStrength" (as before), but no normal map, no metal, full roughness and no specular, so light only
shapes it in broad areas the post process can cut into bands.

Post process (Scene Color After DOF, so temporal anti-aliasing smooths the lines), plus an instance
MI_<name> next to it for tuning in the editor; rebuilding keeps the instance's tuned values:
- Cel bands: light = scene color / base color. Pixels lit above "LitThreshold" show base color x "LitLevel",
  the rest base color x "ShadowTint"; "BandSoftness" blurs the step a little. Pixels with almost no base color
  (unlit, like the outline overlay) or far brighter than lit (glows, effects) pass through unchanged.
- Ink lines: a neighbor clearly closer in depth (one clean line on the far side of each contour) or the Laplacian of
  world normals (creases), "LineWidth" pixels apart, over "DepthLineThreshold" / "NormalLineThreshold" (on characters,
  finer "CharacterDepthLineThreshold" / "CharacterNormalLineThreshold"), painted "InkColor" or, by
  "InkFromBaseColor", the surface's own
  color x "InkDarken" (default: the own color, Genshin style). No ink next to unlit pixels (glows).
- Rim light: character pixels within "RimWidth" pixels of the silhouette brighten by "RimStrength" x "RimColor".
- Silhouette: characters write a nonzero custom stencil; pixels outside every character with a visible character
  pixel within "OutlineWidth" pixels get "SilhouetteColor". Ink is left off characters' outer boundaries, so the
  silhouette is that one even line and inner edges keep the ink. (Teams are told apart by the HUD, not by color here.)
  Needs r.CustomDepth=3 (custom depth with stencil) in the project's renderer settings.
"""
import sys

import unreal

char_path, pp_path = " ".join(sys.argv[1:]).split("|")
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
ST = unreal.SceneTextureId


def log(msg):
    unreal.log(f"TOON {msg}")


# ---------- Character material, flattened ----------
# Base color (texture x team tint) stays as ue_setup_fox.py built it. Clearing the graph trips an engine
# assertion in a commandlet, so the other inputs are just reconnected to constants; the old texture
# samples stay in the graph unconnected and don't end up in the shader.
# Leave the character path empty ("|<pp path>") to only rebuild the post process.
def node(m, cls, x, y):
    return mel.create_material_expression(m, cls, x, y)


if char_path:
    mat = unreal.load_asset(char_path)
    for prop, value in ((unreal.MaterialProperty.MP_ROUGHNESS, 1.0), (unreal.MaterialProperty.MP_SPECULAR, 0.0),
                        (unreal.MaterialProperty.MP_METALLIC, 0.0)):
        c = node(mat, unreal.MaterialExpressionConstant, -250, 200)
        c.set_editor_property("r", value)
        mel.connect_material_property(c, "", prop)
    flat = node(mat, unreal.MaterialExpressionConstant3Vector, -250, 400)
    flat.set_editor_property("constant", unreal.LinearColor(0, 0, 1, 1))
    mel.connect_material_property(flat, "", unreal.MaterialProperty.MP_NORMAL)
    mel.recompile_material(mat)
    eal.save_asset(char_path)
    log(f"flattened {char_path}")

# ---------- Toon post process ----------
folder, name = pp_path.rsplit("/", 1)
mi_name = "MI_" + name[2:] if name.startswith("M_") else name + "_Inst"
mi_path = f"{folder}/{mi_name}"

# The instance holds the values tuned in the editor: remember its overrides, rebuild, then put them back
saved_scalars, saved_vectors = {}, {}
if eal.does_asset_exist(mi_path):
    old = unreal.load_asset(mi_path)
    for p in old.get_editor_property("scalar_parameter_values"):
        saved_scalars[str(p.get_editor_property("parameter_info").get_editor_property("name"))] = p.get_editor_property("parameter_value")
    for p in old.get_editor_property("vector_parameter_values"):
        saved_vectors[str(p.get_editor_property("parameter_info").get_editor_property("name"))] = p.get_editor_property("parameter_value")
    eal.delete_asset(mi_path)
if eal.does_asset_exist(pp_path):
    eal.delete_asset(pp_path)
pp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
pp.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
# Before temporal anti-aliasing (TSR), so the ink lines come out smooth and steady instead of jagged and crawling
pp.set_editor_property("blendable_location", unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_DOF)

X = [-2400]  # running column so nodes don't pile up in the editor


def n(cls, y=0, **props):
    e = node(pp, cls, X[0], y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def link(a, a_out, b, b_in):
    mel.connect_material_expressions(a, a_out, b, b_in)


def param(pname, value, y):
    p = n(unreal.MaterialExpressionScalarParameter, y, parameter_name=pname, default_value=value)
    return p


def vparam(pname, rgb, y):
    return n(unreal.MaterialExpressionVectorParameter, y, parameter_name=pname,
             default_value=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1))


def op(cls, a, b, y=0, a_out="", b_out=""):
    e = n(cls, y)
    link(a, a_out, e, "A")
    link(b, b_out, e, "B")
    return e


def scene(tex_id, uv, y):
    s = n(unreal.MaterialExpressionSceneTexture, y, scene_texture_id=tex_id)
    if uv is not None:
        link(uv, "", s, "UVs")
    return s


# Screen UV and one-pixel steps
uv = n(unreal.MaterialExpressionTextureCoordinate, 0)
view = n(unreal.MaterialExpressionViewSize, 200)
width = param("LineWidth", 2.5, 300)
X[0] += 250
inv = n(unreal.MaterialExpressionDivide, 200)
link(width, "", inv, "A")
link(view, "", inv, "B")
step_x = n(unreal.MaterialExpressionComponentMask, 150, r=True, g=False, b=False, a=False)
step_y = n(unreal.MaterialExpressionComponentMask, 250, r=False, g=True, b=False, a=False)
link(inv, "", step_x, "")
link(inv, "", step_y, "")
X[0] += 250
zero = n(unreal.MaterialExpressionConstant, 400, r=0.0)
off = {}
for key, comp, sign in (("l", step_x, -1), ("r", step_x, 1), ("u", step_y, -1), ("d", step_y, 1)):
    s = n(unreal.MaterialExpressionMultiply, 500 + len(off) * 60, const_b=float(sign))
    link(comp, "", s, "A")
    v = n(unreal.MaterialExpressionAppendVector, 500 + len(off) * 60)
    if key in "lr":
        link(s, "", v, "A"); link(zero, "", v, "B")
    else:
        link(zero, "", v, "A"); link(s, "", v, "B")
    off[key] = v
X[0] += 250
uvs = {}
for key, v in off.items():
    a = n(unreal.MaterialExpressionAdd, 700 + len(uvs) * 60)
    link(uv, "", a, "A"); link(v, "", a, "B")
    uvs[key] = a
X[0] += 250

# Diagonal neighbors at the same distance, for the depth lines
diag_uvs = []
for i, (sx, sy) in enumerate(((1, -1), (-1, -1), (1, 1), (-1, 1))):
    ax = n(unreal.MaterialExpressionMultiply, 800 + i * 60, const_b=0.707 * sx)
    link(step_x, "", ax, "A")
    ay = n(unreal.MaterialExpressionMultiply, 830 + i * 60, const_b=0.707 * sy)
    link(step_y, "", ay, "A")
    v = n(unreal.MaterialExpressionAppendVector, 800 + i * 60)
    link(ax, "", v, "A"); link(ay, "", v, "B")
    a = n(unreal.MaterialExpressionAdd, 800 + i * 60)
    link(uv, "", a, "A"); link(v, "", a, "B")
    diag_uvs.append(a)
X[0] += 250


def rmask(tex_id, uv_node, y):
    r = n(unreal.MaterialExpressionComponentMask, y, r=True, g=False, b=False, a=False)
    link(scene(tex_id, uv_node, y), "Color", r, "")
    return r


# One-sided depth lines: a pixel is inked when a neighbor within LineWidth is clearly closer, so each contour
# gets one clean line of that width on its far side (around an arm over the body, on the body), instead of the
# double, broken line a Laplacian draws on both sides
dc_r = rmask(ST.PPI_SCENE_DEPTH, None, 900)
closer = [op(unreal.MaterialExpressionSubtract, dc_r, rmask(ST.PPI_SCENE_DEPTH, u, 1000 + i * 70), 1000 + i * 70)
          for i, u in enumerate([uvs[k] for k in "lrud"] + diag_uvs)]
X[0] += 250
closest = closer[0]
for c in closer[1:]:
    closest = op(unreal.MaterialExpressionMax, closest, c, 1200)
X[0] += 250
drel = op(unreal.MaterialExpressionDivide, closest, dc_r, 1000)

# Normal Laplacian, for creases with no depth step (cover corners)
nc = scene(ST.PPI_WORLD_NORMAL, None, 1500)
ns = [scene(ST.PPI_WORLD_NORMAL, uvs[k], 1600 + i * 100) for i, k in enumerate("lrud")]
X[0] += 250
na = op(unreal.MaterialExpressionAdd, ns[0], ns[1], 1600, "Color", "Color")
nb = op(unreal.MaterialExpressionAdd, ns[2], ns[3], 1660, "Color", "Color")
X[0] += 250
nsum = op(unreal.MaterialExpressionAdd, na, nb, 1630)
n4 = n(unreal.MaterialExpressionMultiply, 1500, const_b=4.0)
link(nc, "Color", n4, "A")
X[0] += 250
nlap = op(unreal.MaterialExpressionSubtract, nsum, n4, 1600)
X[0] += 250
nlap3 = n(unreal.MaterialExpressionComponentMask, 1600, r=True, g=True, b=True, a=False)
link(nlap, "", nlap3, "")
X[0] += 250
nlen = n(unreal.MaterialExpressionLength, 1600)
link(nlap3, "", nlen, "")
X[0] += 250
# Characters (custom stencil set) get their own, finer thresholds: the world's are tuned for cover corners and
# would miss an arm over the body or an ear over the head
world_dthr = param("DepthLineThreshold", 0.004, 1100)
world_nthr = param("NormalLineThreshold", 1.2, 1700)
char_dthr = param("CharacterDepthLineThreshold", 0.0015, 1200)
char_nthr = param("CharacterNormalLineThreshold", 10.0, 1800)
char_st = n(unreal.MaterialExpressionComponentMask, 1450, r=True, g=False, b=False, a=False)
link(scene(ST.PPI_CUSTOM_STENCIL, None, 1450), "Color", char_st, "")
X[0] += 250
is_char = n(unreal.MaterialExpressionSaturate, 1450)
link(char_st, "", is_char, "")
X[0] += 250
dthr = n(unreal.MaterialExpressionLinearInterpolate, 1100)
link(world_dthr, "", dthr, "A"); link(char_dthr, "", dthr, "B"); link(is_char, "", dthr, "Alpha")
nthr = n(unreal.MaterialExpressionLinearInterpolate, 1700)
link(world_nthr, "", nthr, "A"); link(char_nthr, "", nthr, "B"); link(is_char, "", nthr, "Alpha")
X[0] += 250
dedge = op(unreal.MaterialExpressionDivide, drel, dthr, 1000)
nedge = op(unreal.MaterialExpressionDivide, nlen, nthr, 1600)
X[0] += 250
edge = op(unreal.MaterialExpressionMax, dedge, nedge, 1300)
X[0] += 250
# Below the threshold no line; a little above, a full line
edge_lo = n(unreal.MaterialExpressionSubtract, 1300, const_b=1.0)
link(edge, "", edge_lo, "A")
X[0] += 250
edge_k = n(unreal.MaterialExpressionMultiply, 1300, const_b=2.0)
link(edge_lo, "", edge_k, "A")
X[0] += 250
ink_alpha = n(unreal.MaterialExpressionSaturate, 1300)
link(edge_k, "", ink_alpha, "")

# Cel bands
X[0] = -2400
scol = scene(ST.PPI_POST_PROCESS_INPUT0, None, -900)
bcol = scene(ST.PPI_BASE_COLOR, None, -700)
X[0] += 250
s_rgb = n(unreal.MaterialExpressionComponentMask, -900, r=True, g=True, b=True, a=False)
link(scol, "Color", s_rgb, "")
b_rgb = n(unreal.MaterialExpressionComponentMask, -700, r=True, g=True, b=True, a=False)
link(bcol, "Color", b_rgb, "")
X[0] += 250
lum_w = n(unreal.MaterialExpressionConstant3Vector, -800, constant=unreal.LinearColor(0.299, 0.587, 0.114, 1))
X[0] += 250
s_lum = op(unreal.MaterialExpressionDotProduct, s_rgb, lum_w, -900)
b_lum = op(unreal.MaterialExpressionDotProduct, b_rgb, lum_w, -700)
X[0] += 250
b_lum_safe = n(unreal.MaterialExpressionMax, -700, const_b=0.001)
link(b_lum, "", b_lum_safe, "A")
X[0] += 250
light = op(unreal.MaterialExpressionDivide, s_lum, b_lum_safe, -800)
X[0] += 250
thr = param("LitThreshold", 0.5, -1000)
soft = param("BandSoftness", 0.05, -1100)
X[0] += 250
lo = op(unreal.MaterialExpressionSubtract, thr, soft, -1050)
hi = op(unreal.MaterialExpressionAdd, thr, soft, -950)
X[0] += 250
band = n(unreal.MaterialExpressionSmoothStep, -900)
link(lo, "", band, "Min"); link(hi, "", band, "Max"); link(light, "", band, "Value")
X[0] += 250
lit = param("LitLevel", 1.35, -1300)
shadow = vparam("ShadowTint", (0.98, 0.78, 0.86), -1200)
X[0] += 250
lit3 = n(unreal.MaterialExpressionAppendVector, -1300)
lit2 = n(unreal.MaterialExpressionAppendVector, -1350)
link(lit, "", lit2, "A"); link(lit, "", lit2, "B")
link(lit2, "", lit3, "A"); link(lit, "", lit3, "B")
shadow3 = n(unreal.MaterialExpressionComponentMask, -1200, r=True, g=True, b=True, a=False)
link(shadow, "", shadow3, "")
X[0] += 250
level = n(unreal.MaterialExpressionLinearInterpolate, -1100)
link(shadow3, "", level, "A"); link(lit3, "", level, "B"); link(band, "", level, "Alpha")
X[0] += 250
toon = op(unreal.MaterialExpressionMultiply, b_rgb, level, -800)
X[0] += 250
# Pass through: almost no base color (unlit), or far brighter than lit (glows, effects)
glow = param("GlowThreshold", 4.0, -500)
X[0] += 250
# Steep ramps instead of If nodes: 1 when base luminance < 0.01, 1 when light > GlowThreshold
unlit_gap = n(unreal.MaterialExpressionOneMinus, -600)
unlit_scaled = n(unreal.MaterialExpressionMultiply, -600, const_b=100.0)
link(b_lum, "", unlit_scaled, "A")
link(unlit_scaled, "", unlit_gap, "")
glow_gap = op(unreal.MaterialExpressionSubtract, light, glow, -500)
X[0] += 250
glow_scaled = n(unreal.MaterialExpressionMultiply, -500, const_b=10.0)
link(glow_gap, "", glow_scaled, "A")
X[0] += 250
is_unlit = n(unreal.MaterialExpressionSaturate, -600)
link(unlit_gap, "", is_unlit, "")
is_glow = n(unreal.MaterialExpressionSaturate, -500)
link(glow_scaled, "", is_glow, "")
X[0] += 250
keep = op(unreal.MaterialExpressionMax, is_unlit, is_glow, -550)
X[0] += 250
shaded = n(unreal.MaterialExpressionLinearInterpolate, -700)
link(toon, "", shaded, "A"); link(s_rgb, "", shaded, "B"); link(keep, "", shaded, "Alpha")
X[0] += 250
# Inner lines: InkColor, or the surface's own color darkened by InkDarken, mixed by InkFromBaseColor
ink_fixed = vparam("InkColor", (0.03, 0.025, 0.04), -300)
ink_from_base = param("InkFromBaseColor", 1.0, -200)
ink_darken = param("InkDarken", 0.3, -100)
X[0] += 250
ink_fixed3 = n(unreal.MaterialExpressionComponentMask, -300, r=True, g=True, b=True, a=False)
link(ink_fixed, "", ink_fixed3, "")
ink_own = op(unreal.MaterialExpressionMultiply, b_rgb, ink_darken, -100)
X[0] += 250
ink = n(unreal.MaterialExpressionLinearInterpolate, -200)
link(ink_fixed3, "", ink, "A"); link(ink_own, "", ink, "B"); link(ink_from_base, "", ink, "Alpha")

# No ink next to unlit pixels (glows, effects): they have no surface to ink. Unlit = almost no base color.
X[0] = 1200
def base_lum_at(key, y):
    rgb = n(unreal.MaterialExpressionComponentMask, y, r=True, g=True, b=True, a=False)
    link(scene(ST.PPI_BASE_COLOR, uvs[key], y), "Color", rgb, "")
    return op(unreal.MaterialExpressionDotProduct, rgb, lum_w, y)


near_lums = [b_lum] + [base_lum_at(k, 2100 + i * 100) for i, k in enumerate("lrud")]
X[0] += 250
m1 = op(unreal.MaterialExpressionMin, near_lums[0], near_lums[1], 2100)
m2 = op(unreal.MaterialExpressionMin, near_lums[2], near_lums[3], 2200)
X[0] += 250
m3 = op(unreal.MaterialExpressionMin, m1, m2, 2150)
X[0] += 250
darkest = op(unreal.MaterialExpressionMin, m3, near_lums[4], 2200)
X[0] += 250
lit_scaled = n(unreal.MaterialExpressionMultiply, 2200, const_b=100.0)
link(darkest, "", lit_scaled, "A")
X[0] += 250
not_unlit = n(unreal.MaterialExpressionSaturate, 2200)
link(lit_scaled, "", not_unlit, "")
X[0] += 250
line_lit = op(unreal.MaterialExpressionMultiply, ink_alpha, not_unlit, 1800)

# ---------- Silhouette from the custom stencil ----------
# Characters write a nonzero custom stencil. The line goes on pixels outside every character that have a visible
# character pixel within "OutlineWidth" pixels, so it wraps the whole silhouette evenly but never an arm in front
# of the body; inner edges keep the ink. A neighbor only counts where its custom depth isn't behind the scene
# there, so cover hides the line with the body.
X[0] = -2400


def red(tex_id, uv_node, y):
    r = n(unreal.MaterialExpressionComponentMask, y, r=True, g=False, b=False, a=False)
    link(scene(tex_id, uv_node, y), "Color", r, "")
    return r


ow = param("OutlineWidth", 2.5, 2700)
X[0] += 250
ow_step = op(unreal.MaterialExpressionDivide, ow, view, 2700)
X[0] += 250
neighbor_teams = []
for i, (dx, dy) in enumerate(((1, 0), (-1, 0), (0, 1), (0, -1), (0.707, 0.707), (-0.707, 0.707), (0.707, -0.707), (-0.707, -0.707))):
    y = 2800 + i * 300
    X[0] = -1900
    d = n(unreal.MaterialExpressionConstant2Vector, y, r=float(dx), g=float(dy))
    X[0] += 250
    o = op(unreal.MaterialExpressionMultiply, ow_step, d, y)
    X[0] += 250
    uvd = op(unreal.MaterialExpressionAdd, uv, o, y)
    X[0] += 250
    st = red(ST.PPI_CUSTOM_STENCIL, uvd, y)
    cd = red(ST.PPI_CUSTOM_DEPTH, uvd, y + 100)
    sd = red(ST.PPI_SCENE_DEPTH, uvd, y + 200)
    X[0] += 250
    gap = op(unreal.MaterialExpressionSubtract, sd, cd, y + 150)
    X[0] += 250
    gap_ok = n(unreal.MaterialExpressionAdd, y + 150, const_b=2.0)  # depth precision slack
    link(gap, "", gap_ok, "A")
    X[0] += 250
    gap_k = n(unreal.MaterialExpressionMultiply, y + 150, const_b=1000.0)
    link(gap_ok, "", gap_k, "A")
    X[0] += 250
    visible = n(unreal.MaterialExpressionSaturate, y + 150)
    link(gap_k, "", visible, "")
    X[0] += 250
    neighbor_teams.append(op(unreal.MaterialExpressionMultiply, st, visible, y))
X[0] += 250
team = neighbor_teams[0]
for t in neighbor_teams[1:]:
    team = op(unreal.MaterialExpressionMax, team, t, 3500)
X[0] += 250
center_st = red(ST.PPI_CUSTOM_STENCIL, None, 2600)
X[0] += 250
center_char = n(unreal.MaterialExpressionSaturate, 2600)
link(center_st, "", center_char, "")
X[0] += 250
outside = n(unreal.MaterialExpressionOneMinus, 2600)
link(center_char, "", outside, "")
has_team = n(unreal.MaterialExpressionSaturate, 3500)
link(team, "", has_team, "")
X[0] += 250
outline_alpha = op(unreal.MaterialExpressionMultiply, has_team, outside, 3000)
silhouette_col = vparam("SilhouetteColor", (0.09, 0.04, 0.03), 3700)
X[0] += 250
outline_col = n(unreal.MaterialExpressionComponentMask, 3700, r=True, g=True, b=True, a=False)
link(silhouette_col, "", outline_col, "")

# No ink on a character's outer boundary (either side): that edge belongs to the silhouette line
X[0] += 250
inner_diffs = []
for i, k in enumerate("lrud"):
    s = red(ST.PPI_CUSTOM_STENCIL, uvs[k], 4000 + i * 100)
    sat = n(unreal.MaterialExpressionSaturate, 4000 + i * 100)
    link(s, "", sat, "")
    diff = op(unreal.MaterialExpressionSubtract, sat, center_char, 4000 + i * 100)
    a = n(unreal.MaterialExpressionAbs, 4000 + i * 100)
    link(diff, "", a, "")
    inner_diffs.append(a)
X[0] += 250
boundary = op(unreal.MaterialExpressionMax, op(unreal.MaterialExpressionMax, inner_diffs[0], inner_diffs[1], 4000),
              op(unreal.MaterialExpressionMax, inner_diffs[2], inner_diffs[3], 4200), 4100)
X[0] += 250
not_boundary = n(unreal.MaterialExpressionOneMinus, 4100)
link(boundary, "", not_boundary, "")
X[0] += 250
line = op(unreal.MaterialExpressionMultiply, line_lit, not_boundary, 1800)

# Rim light, anime style: character pixels within "RimWidth" pixels of the silhouette get brighter by
# "RimStrength", tinted "RimColor" (just inside the silhouette line)
X[0] = -1900
rim_w = param("RimWidth", 3.0, 4600)
rim_strength = param("RimStrength", 0.45, 4700)
rim_color = vparam("RimColor", (1.0, 0.92, 0.82), 4800)
X[0] += 250
rim_step = op(unreal.MaterialExpressionDivide, rim_w, view, 4600)
rim_color3 = n(unreal.MaterialExpressionComponentMask, 4800, r=True, g=True, b=True, a=False)
link(rim_color, "", rim_color3, "")
outsides = []
for i, (dx, dy) in enumerate(((1, 0), (-1, 0), (0, 1), (0, -1))):
    y = 4900 + i * 120
    X[0] = -1400
    d = n(unreal.MaterialExpressionConstant2Vector, y, r=float(dx), g=float(dy))
    X[0] += 250
    o = op(unreal.MaterialExpressionMultiply, rim_step, d, y)
    X[0] += 250
    uvd = op(unreal.MaterialExpressionAdd, uv, o, y)
    X[0] += 250
    s = n(unreal.MaterialExpressionSaturate, y)
    link(red(ST.PPI_CUSTOM_STENCIL, uvd, y), "", s, "")
    X[0] += 250
    out = n(unreal.MaterialExpressionOneMinus, y)
    link(s, "", out, "")
    outsides.append(out)
X[0] += 250
near_edge = op(unreal.MaterialExpressionMax, op(unreal.MaterialExpressionMax, outsides[0], outsides[1], 4900),
               op(unreal.MaterialExpressionMax, outsides[2], outsides[3], 5100), 5000)
X[0] += 250
rim_alpha = op(unreal.MaterialExpressionMultiply, near_edge, center_char, 5000)
X[0] += 250
rim_amt = op(unreal.MaterialExpressionMultiply, rim_alpha, rim_strength, 5000)
X[0] += 250
rim_light = op(unreal.MaterialExpressionMultiply, b_rgb, rim_color3, 5100)
X[0] += 250
rim_add = op(unreal.MaterialExpressionMultiply, rim_light, rim_amt, 5050)
X[0] += 250
rimmed = op(unreal.MaterialExpressionAdd, shaded, rim_add, 4000)

X[0] += 250
inked = n(unreal.MaterialExpressionLinearInterpolate, -500)
link(rimmed, "", inked, "A"); link(ink, "", inked, "B"); link(line, "", inked, "Alpha")
X[0] += 250
final = n(unreal.MaterialExpressionLinearInterpolate, -500)
link(inked, "", final, "A"); link(outline_col, "", final, "B"); link(outline_alpha, "", final, "Alpha")
mel.connect_material_property(final, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

mel.recompile_material(pp)
eal.save_asset(pp_path)
log(f"made {pp_path}")

mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    mi_name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
mel.set_material_instance_parent(mi, pp)
for pname, value in saved_scalars.items():
    mel.set_material_instance_scalar_parameter_value(mi, pname, value)
for pname, value in saved_vectors.items():
    mel.set_material_instance_vector_parameter_value(mi, pname, value)
mel.update_material_instance(mi)
eal.save_asset(mi_path)
log(f"made {mi_path} (kept {len(saved_scalars) + len(saved_vectors)} tuned values)")
