"""Friend-or-foe silhouette lines: a post-process material (run inside Unreal).

Usage: UnrealEditor-Cmd <project> -run=pythonscript "-script=ue_make_team_outline.py /Game/PostProcess/M_PP_TeamOutline"

Round 99 (the user: "피아식별이 어려운 문제 해결"). Both teams are the same animal friends, so who is on which side is
drawn round them: every animal friend (and star robot) writes a custom stencil that says what it is to the one looking
(ARRRandomCharacter::UpdateOutlineStencil): 1 a teammate, 2 an opponent, 3 the viewer's own (no line). Pixels outside
every character with a visible character pixel within "OutlineWidth" pixels take that character's line: "EnemyColor"
(red) or "AllyColor" (blue), an opponent's over a teammate's. A neighbor only counts where its custom depth isn't behind
the scene there, so cover hides the line with the body (no seeing through walls). "Brightness" scales the colors (the
material runs before tonemapping; the eye adaptation is undone so the colors hold in sun and shade), "EnemyOpacity" /
"AllyOpacity" how solid the lines are.
Material nodes only (like ue_make_toon.py's silhouette, which this takes after). Needs r.CustomDepth=3.
The game sets "OutlineWidth" from the screen's height (3 pixels on a 720 pixel tall screen).
"""
import sys

import unreal

path = " ".join(sys.argv[1:]).strip()
folder, name = path.rsplit("/", 1)
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
ST = unreal.SceneTextureId


def log(msg):
    unreal.log(f"TEAMOUTLINE {msg}")


if eal.does_asset_exist(path):
    eal.delete_asset(path)
pp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
pp.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
# Before temporal anti-aliasing (TSR), so the lines come out smooth and steady (as the toon material did)
pp.set_editor_property("blendable_location", unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_DOF)

X = [-2600]


def n(cls, y=0, **props):
    e = mel.create_material_expression(pp, cls, X[0], y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def link(a, a_out, b, b_in):
    if not mel.connect_material_expressions(a, a_out, b, b_in):
        raise RuntimeError(f"could not connect into {b_in}")


def param(pname, value, y):
    return n(unreal.MaterialExpressionScalarParameter, y, parameter_name=pname, default_value=value)


def vparam(pname, rgb, y):
    p = n(unreal.MaterialExpressionVectorParameter, y, parameter_name=pname, default_value=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1))
    m = n(unreal.MaterialExpressionComponentMask, y + 120, r=True, g=True, b=True, a=False)
    link(p, "", m, "")
    return m


def op(cls, a, b, y=0):
    e = n(cls, y)
    link(a, "", e, "A")
    link(b, "", e, "B")
    return e


def const_op(cls, a, const_b, y=0):
    e = n(cls, y, const_b=float(const_b))
    link(a, "", e, "A")
    return e


def one(cls, a, y=0):
    e = n(cls, y)
    link(a, "", e, "")
    return e


def red(tex_id, uv_node, y):
    s = n(unreal.MaterialExpressionSceneTexture, y, scene_texture_id=tex_id)
    if uv_node is not None:
        link(uv_node, "", s, "UVs")
    r = n(unreal.MaterialExpressionComponentMask, y + 60, r=True, g=False, b=False, a=False)
    link(s, "Color", r, "")
    return r


DEPTH_SLACK = 40.0


def is_value(stencil, value, y):
    # 1 where the stencil is this value, 0 one or more away: saturate(1 - |s - value|)
    d = const_op(unreal.MaterialExpressionSubtract, stencil, value, y)
    a = one(unreal.MaterialExpressionAbs, d, y)
    return one(unreal.MaterialExpressionSaturate, one(unreal.MaterialExpressionOneMinus, a, y), y)


def visible(custom_depth, scene_depth, y):
    # 1 where the character's pixel isn't behind the scene's. The slack is wide (DEPTH_SLACK units): the fur shells drawn
    # over a body write no custom depth, so they are a little nearer than the body they belong to; with a slack of 2 every
    # fur pixel counted as "outside the character" and the line's color speckled the whole coat
    gap = op(unreal.MaterialExpressionSubtract, scene_depth, custom_depth, y)
    return one(unreal.MaterialExpressionSaturate, const_op(unreal.MaterialExpressionMultiply, const_op(unreal.MaterialExpressionAdd, gap, DEPTH_SLACK, y), 1000.0, y), y)


uv = n(unreal.MaterialExpressionTextureCoordinate, 0)
view = n(unreal.MaterialExpressionViewSize, 200)
width = param("OutlineWidth", 3.0, 400)
X[0] += 300
step = op(unreal.MaterialExpressionDivide, width, view, 300)

allies, enemies = [], []
# A ring at the full width and one at half of it, so the line is solid all the way in
rings = [(1.0, ((1, 0), (-1, 0), (0, 1), (0, -1), (0.707, 0.707), (-0.707, 0.707), (0.707, -0.707), (-0.707, -0.707))),
         (0.5, ((0.924, 0.383), (-0.383, 0.924), (-0.924, -0.383), (0.383, -0.924)))]
row = 0
for share, ways in rings:
    for dx, dy in ways:
        y = 800 + row * 420
        row += 1
        X[0] = -2000
        d = n(unreal.MaterialExpressionConstant2Vector, y, r=float(dx * share), g=float(dy * share))
        X[0] += 250
        uvd = op(unreal.MaterialExpressionAdd, uv, op(unreal.MaterialExpressionMultiply, step, d, y), y)
        X[0] += 300
        st = red(ST.PPI_CUSTOM_STENCIL, uvd, y)
        cd = red(ST.PPI_CUSTOM_DEPTH, uvd, y + 130)
        sd = red(ST.PPI_SCENE_DEPTH, uvd, y + 260)
        X[0] += 300
        vis = visible(cd, sd, y + 200)
        X[0] += 300
        allies.append(op(unreal.MaterialExpressionMultiply, is_value(st, 1.0, y), vis, y))
        enemies.append(op(unreal.MaterialExpressionMultiply, is_value(st, 2.0, y + 100), vis, y + 100))


def most(nodes, y):
    best = nodes[0]
    for other in nodes[1:]:
        best = op(unreal.MaterialExpressionMax, best, other, y)
    return best


X[0] = -400
ally = most(allies, 600)
enemy = most(enemies, 900)

# This pixel is outside every character it can see (a character behind cover doesn't count as one)
X[0] = -1400
center_st = red(ST.PPI_CUSTOM_STENCIL, None, -400)
center_vis = visible(red(ST.PPI_CUSTOM_DEPTH, None, -280), red(ST.PPI_SCENE_DEPTH, None, -160), -220)
X[0] = -800
inside = op(unreal.MaterialExpressionMultiply, one(unreal.MaterialExpressionSaturate, center_st, -400), center_vis, -400)
outside = one(unreal.MaterialExpressionOneMinus, inside, -400)

X[0] = -100
enemy_alpha = op(unreal.MaterialExpressionMultiply, op(unreal.MaterialExpressionMultiply, enemy, outside, 900), param("EnemyOpacity", 1.0, 1100), 900)
ally_alpha = op(unreal.MaterialExpressionMultiply, op(unreal.MaterialExpressionMultiply, ally, outside, 600), param("AllyOpacity", 0.9, 700), 600)

X[0] = 300
brightness = param("Brightness", 1.0, 300)


def steady(color, y):
    # The material runs before tonemapping, where the scene is scaled by the eye adaptation: undone here, so the line is
    # the same strong color in sun and shade (scaled as it was, a bright scene's exposure washed the red out to pink)
    e = n(unreal.MaterialExpressionEyeAdaptationInverse, y)
    for pin in ("LightValue", "LightValueInput", "Light Value"):
        if mel.connect_material_expressions(color, "", e, pin):
            return e
    raise RuntimeError("could not connect into the eye adaptation inverse")


ally_color = steady(op(unreal.MaterialExpressionMultiply, vparam("AllyColor", (0.02, 0.32, 1.0), 400), brightness, 450), 450)
enemy_color = steady(op(unreal.MaterialExpressionMultiply, vparam("EnemyColor", (1.0, 0.0, 0.06), 800), brightness, 850), 850)
scene = n(unreal.MaterialExpressionSceneTexture, 0, scene_texture_id=ST.PPI_POST_PROCESS_INPUT0)
scene_rgb = n(unreal.MaterialExpressionComponentMask, 100, r=True, g=True, b=True, a=False)
link(scene, "Color", scene_rgb, "")
X[0] = 700
with_ally = n(unreal.MaterialExpressionLinearInterpolate, 300)
link(scene_rgb, "", with_ally, "A")
link(ally_color, "", with_ally, "B")
link(ally_alpha, "", with_ally, "Alpha")
X[0] = 950
final = n(unreal.MaterialExpressionLinearInterpolate, 300)
link(with_ally, "", final, "A")
link(enemy_color, "", final, "B")
link(enemy_alpha, "", final, "Alpha")
mel.connect_material_property(final, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

mel.recompile_material(pp)
eal.save_asset(path)
log(f"made {path}")
log("DONE")
