"""Hologram arrow for picking the dragon flight's way (run inside Unreal): /Game/Effects/DiceFX/M_FXHoloArrow.

Usage: UnrealEditor-Cmd <project> -run=pythonscript "-script=ue_make_holo_arrow.py"

For the engine's /Engine/BasicShapes/Cube (100 on a side, centered) stretched into a strip along its X, from the start
(-X) to the far end (+X). The game sets "Length" (the strip's length in the world) and "Radius" (half its width). Unlit,
translucent, two-sided, drawn over everything, like M_FXHolo. In the strip's own space:
- chevrons (">") streaming from the start toward the far end ("Speed" units a second, "Spacing" apart, at least the
  strip's width), brighter toward the end, the start fading in;
- bright rails along both sides over a faint fill;
- the last stretch is an arrowhead: a triangle narrowing to a point at the far end, glowing along its slanted sides.
Emissive = "Color" x "Glow" x (0.4 + that), opacity = that x "Opacity".
"""
import unreal

U = unreal
eal = U.EditorAssetLibrary
mel = U.MaterialEditingLibrary
path = "/Game/Effects/DiceFX/M_FXHoloArrow"
if eal.does_asset_exist(path):
    eal.delete_asset(path)
mat = U.AssetToolsHelpers.get_asset_tools().create_asset("M_FXHoloArrow", "/Game/Effects/DiceFX", U.Material, U.MaterialFactoryNew())
mat.set_editor_property("blend_mode", U.BlendMode.BLEND_TRANSLUCENT)
mat.set_editor_property("shading_model", U.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property("two_sided", True)
mat.set_editor_property("disable_depth_test", True)

CODE = """float S = (Local.x / 100.0 + 0.5) * Length;
float V = saturate(abs(Local.y) / 50.0);
float W = max(Radius, 1.0);
float Head = min(W * 1.8, Length * 0.4);
float ToEnd = Length - S;
if (ToEnd < Head)
{
    float Half = ToEnd / Head;
    if (V > Half) { return 0.0; }
    float Rim = saturate(1.0 - (Half - V) * W / 20.0);
    return saturate(0.75 + Rim * 0.25);
}
float Gap = max(Spacing, W * 1.3);
float F = frac((S + V * W * 0.9 - Time * Speed) / Gap);
float Chevron = smoothstep(0.0, 0.05, F) * (1.0 - smoothstep(0.3, 0.36, F));
float Ramp = 0.7 + 0.3 * saturate(S / max(Length - Head, 1.0));
float Rail = saturate(1.0 - (1.0 - V) * W / 22.0);
float Begin = saturate(S / (W * 0.8));
return saturate((0.2 + Chevron * Ramp + Rail * 0.9) * Begin);"""


def node(cls, x, y, **props):
    made = mel.create_material_expression(mat, cls, x, y)
    for key, value in props.items():
        made.set_editor_property(key, value)
    return made


def scalar(name, value, x, y):
    return node(U.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=value)


custom = node(U.MaterialExpressionCustom, -500, 0, code=CODE, output_type=U.CustomMaterialOutputType.CMOT_FLOAT1, description="HoloArrow")
inputs = []
for name in ("Local", "Length", "Radius", "Time", "Speed", "Spacing"):
    entry = U.CustomInput()
    entry.set_editor_property("input_name", name)
    inputs.append(entry)
custom.set_editor_property("inputs", inputs)
mel.connect_material_expressions(node(U.MaterialExpressionLocalPosition, -900, -100), "", custom, "Local")
mel.connect_material_expressions(scalar("Length", 2000.0, -900, 0), "", custom, "Length")
mel.connect_material_expressions(scalar("Radius", 200.0, -900, 100), "", custom, "Radius")
mel.connect_material_expressions(node(U.MaterialExpressionTime, -900, 200), "", custom, "Time")
mel.connect_material_expressions(scalar("Speed", 900.0, -900, 300), "", custom, "Speed")
mel.connect_material_expressions(scalar("Spacing", 260.0, -900, 400), "", custom, "Spacing")

# Emissive: color x glow x (0.4 + pattern)
lift = node(U.MaterialExpressionAdd, -250, -100)
mel.connect_material_expressions(custom, "", lift, "A")
mel.connect_material_expressions(node(U.MaterialExpressionConstant, -400, -200, r=0.4), "", lift, "B")
color = node(U.MaterialExpressionVectorParameter, -400, -350, parameter_name="Color", default_value=U.LinearColor(1.0, 0.55, 0.1, 1.0))
glow = node(U.MaterialExpressionMultiply, -250, -300)
mel.connect_material_expressions(color, "", glow, "A")
mel.connect_material_expressions(scalar("Glow", 1.6, -400, -250), "", glow, "B")
lit = node(U.MaterialExpressionMultiply, -100, -200)
mel.connect_material_expressions(glow, "", lit, "A")
mel.connect_material_expressions(lift, "", lit, "B")
mel.connect_material_property(lit, "", U.MaterialProperty.MP_EMISSIVE_COLOR)
# Opacity: pattern x "Opacity"
seen = node(U.MaterialExpressionMultiply, -100, 100)
mel.connect_material_expressions(custom, "", seen, "A")
mel.connect_material_expressions(scalar("Opacity", 1.0, -250, 200), "", seen, "B")
mel.connect_material_property(seen, "", U.MaterialProperty.MP_OPACITY)

mel.recompile_material(mat)
eal.save_asset(path)
unreal.log("HOLOARROW DONE")
