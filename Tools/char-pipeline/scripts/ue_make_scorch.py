"""Scorched ground with glowing embers where the dragon's breath went by (run inside Unreal).

Usage: UnrealEditor-Cmd <project> -run=pythonscript "-script=ue_make_scorch.py /Game/Effects/DiceFX/M_FXScorch"

A thin flat strip laid once over the whole line of fire, just above the floor (ARRRandomDragonFlight: the engine cube,
100 on a side about its middle, flattened and stretched along its X), its material working out from each pixel's place
on the strip (local position) where on the line it is, so one strip follows the fire along it. Unlit and masked with a
dithered coverage (temporal AA blends it): unlit pixels pass the toon post process untouched, where a translucent dark
overlay came out as the ground's own shadow band (and drawn after it, over the trees in front). (A deferred decal was
tried first: in this project decals come out black whatever their color, the reason skid marks, black anyway, never
showed it.)
- "Length" and "Width" (the strip's size in the world), "Pad" (how far it reaches back past the line's start),
  "Radius" (half the burnt strip's width), "Front" (how far along the breath has got): nothing past the front, a rounded
  end at the start;
- "PassTime" (seconds since the breath left the start), "FlySpeed" and "BurnSeconds": a spot's age is PassTime minus
  how long the breath took to reach it; it smoulders until BurnSeconds old and is gone with its embers;
- the char: dark soot ("CharColor") with blotchy, wobbling edges, covering the ground by that much ("Opacity" on all);
- the embers in it: thin glowing cracks and hot spots, flickering, hottest just after the breath, dying down with age
  ("EmberColor" x "Glow").
"""
import sys

import unreal

path = " ".join(sys.argv[1:]).strip()
folder, name = path.rsplit("/", 1)
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
U = unreal

if eal.does_asset_exist(path):
    eal.delete_asset(path)
mat = U.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, U.Material, U.MaterialFactoryNew())
mat.set_editor_property("blend_mode", U.BlendMode.BLEND_MASKED)
mat.set_editor_property("shading_model", U.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property("opacity_mask_clip_value", 0.5)


def node(cls, x, y, **props):
    n = mel.create_material_expression(mat, cls, x, y)
    for key, value in props.items():
        n.set_editor_property(key, value)
    return n


def link(a, a_out, b, b_in):
    if not mel.connect_material_expressions(a, a_out, b, b_in):
        raise RuntimeError(f"could not connect into {b_in}")


# Where on the line a ground pixel is: S along it from the start, A across it, its age, and how much of the strip it is in
COMMON = """struct FNoise
{
    float Hash(float2 P) { return frac(sin(dot(P, float2(127.1, 311.7))) * 43758.5453); }
    float Value(float2 P)
    {
        float2 I = floor(P);
        float2 F = frac(P);
        F = F * F * (3.0 - 2.0 * F);
        return lerp(lerp(Hash(I), Hash(I + float2(1, 0)), F.x), lerp(Hash(I + float2(0, 1)), Hash(I + float2(1, 1)), F.x), F.y);
    }
};
FNoise N;
float S = (Local.x / 100.0 + 0.5) * Length - Pad;
float A = Local.y / 100.0 * Width;
float2 P = float2(S, A);
float Edge = Radius * (0.62 + 0.38 * N.Value(float2(S / 140.0, 3.7)) + 0.18 * N.Value(P / 45.0));
float Off = S < 0.0 ? length(float2(S, A)) : abs(A);
float In = (1.0 - smoothstep(Edge * 0.55, Edge, Off)) * (1.0 - smoothstep(Front - 90.0, Front, S));
float Age = PassTime - max(S, 0.0) / max(FlySpeed, 1.0);
float Left = 1.0 - smoothstep(BurnSeconds - 0.8, BurnSeconds, Age);
"""
EMBER = """float Heat = saturate(1.0 - Age / max(BurnSeconds, 0.1));
float Fresh = saturate(1.0 - Age / 1.0);
float C = N.Value(P / 110.0) + 0.5 * N.Value(P / 47.0 + 9.1);
float Crack = 1.0 - saturate(abs(C - 0.75) * 14.0);
float Spot = smoothstep(0.72, 0.9, N.Value(P / 40.0 + 4.3));
float Flicker = 0.6 + 0.4 * sin(Time * 7.0 + N.Hash(floor(P / 40.0)) * 6.2832);
float E = (Crack * (0.5 + Fresh) + Spot) * Flicker * Heat * In * Left * Opacity;
float Soot = In * Left * Opacity * (0.85 + 0.15 * N.Value(P / 80.0));
"""
# Over the floor: the soot, and the embers glowing through it
OPACITY = COMMON + EMBER + "return saturate(Soot + E * 0.5);"
COLOR = COMMON + EMBER + "return lerp(CharColor.rgb, EmberColor.rgb * Glow, saturate(E));"

local = node(U.MaterialExpressionLocalPosition, -1100, 0)
time = node(U.MaterialExpressionTime, -1100, 100)
params = {
    "Length": node(U.MaterialExpressionScalarParameter, -1100, 200, parameter_name="Length", default_value=1000.0),
    "Pad": node(U.MaterialExpressionScalarParameter, -1100, 280, parameter_name="Pad", default_value=240.0),
    "Width": node(U.MaterialExpressionScalarParameter, -1100, 360, parameter_name="Width", default_value=480.0),
    "Radius": node(U.MaterialExpressionScalarParameter, -1100, 500, parameter_name="Radius", default_value=200.0),
    "Front": node(U.MaterialExpressionScalarParameter, -1100, 580, parameter_name="Front", default_value=0.0),
    "PassTime": node(U.MaterialExpressionScalarParameter, -1100, 660, parameter_name="PassTime", default_value=0.0),
    "FlySpeed": node(U.MaterialExpressionScalarParameter, -1100, 740, parameter_name="FlySpeed", default_value=2800.0),
    "BurnSeconds": node(U.MaterialExpressionScalarParameter, -1100, 820, parameter_name="BurnSeconds", default_value=3.0),
    "Opacity": node(U.MaterialExpressionScalarParameter, -1100, 900, parameter_name="Opacity", default_value=1.0),
    "Glow": node(U.MaterialExpressionScalarParameter, -1100, 980, parameter_name="Glow", default_value=1.8),
    "EmberColor": node(U.MaterialExpressionVectorParameter, -1100, 1060, parameter_name="EmberColor", default_value=U.LinearColor(1.0, 0.26, 0.03, 1)),
    "CharColor": node(U.MaterialExpressionVectorParameter, -1100, 1200, parameter_name="CharColor", default_value=U.LinearColor(0.045, 0.028, 0.02, 1)),
}
sources = {"Local": local, "Time": time, **params}


def custom(code, out_type, y, description):
    c = node(U.MaterialExpressionCustom, -500, y, output_type=out_type, description=description, code=code)
    names = list(sources)
    inputs = []
    for input_name in names:
        ci = U.CustomInput()
        ci.set_editor_property("input_name", input_name)
        inputs.append(ci)
    c.set_editor_property("inputs", inputs)
    for input_name in names:
        link(sources[input_name], "", c, input_name)
    return c


opacity = custom(OPACITY, U.CustomMaterialOutputType.CMOT_FLOAT1, 200, "Scorch opacity")
color = custom(COLOR, U.CustomMaterialOutputType.CMOT_FLOAT3, 500, "Scorch color")
mel.connect_material_property(color, "", U.MaterialProperty.MP_EMISSIVE_COLOR)
# Coverage dithered (temporal AA smooths it into a soft blend)
dither = node(U.MaterialExpressionCustom, -250, 200, output_type=U.CustomMaterialOutputType.CMOT_FLOAT1, description="Dither",
              code="float2 P = Parameters.SvPosition.xy + View.StateFrameIndexMod8 * 5.588238;\n"
                   "float Noise = frac(52.9829189 * frac(dot(P, float2(0.06711056, 0.00583715))));\n"
                   "return Alpha + 0.5 - Noise;")
alpha_in = U.CustomInput()
alpha_in.set_editor_property("input_name", "Alpha")
dither.set_editor_property("inputs", [alpha_in])
link(opacity, "", dither, "Alpha")
mel.connect_material_property(dither, "", U.MaterialProperty.MP_OPACITY_MASK)
mel.recompile_material(mat)
eal.save_asset(path)
unreal.log(f"SCORCH made {path}")
