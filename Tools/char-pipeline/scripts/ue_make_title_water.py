"""The title screen's sea, alive: a UI material the title HUD draws over the background (run inside Unreal).

Usage: UnrealEditor-Cmd <project> -run=pythonscript "-script=ue_make_title_water.py /Game/UI/M_TitleWater"

It reads /Game/UI/T_TitleWater (make_title_layers.py: the background's colors, the sea as alpha) over the art's own
0..1 coordinates and, only where the sea is:
- ripples: the picture under it wobbles a little, flowing slowly to the right ("FlowSpeed"), the waves bigger toward
  the bottom (nearer) and finer toward the horizon ("RippleAmount" art pixels at the bottom),
- sheen: long, thin, horizontal glints where the moving swell catches the light ("SheenAmount"), and small sparkles
  that twinkle on and off ("SparkleAmount"), both warm white, brighter toward the horizon where the sky reflects.
Translucent, its opacity the sea mask, so the beach, the dock and the hills show through untouched.
One Custom (HLSL) node does it all; the parameters feed it.
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
mat.set_editor_property("material_domain", U.MaterialDomain.MD_UI)
mat.set_editor_property("blend_mode", U.BlendMode.BLEND_TRANSLUCENT)


def node(cls, x, y, **props):
    n = mel.create_material_expression(mat, cls, x, y)
    for key, value in props.items():
        n.set_editor_property(key, value)
    return n


def link(a, a_out, b, b_in):
    if not mel.connect_material_expressions(a, a_out, b, b_in):
        raise RuntimeError(f"could not connect into {b_in}")


CODE = r"""
struct FWaterNoise
{
    float Hash(float2 P)
    {
        P = frac(P * float2(123.34, 456.21));
        P += dot(P, P + 45.32);
        return frac(P.x * P.y);
    }
    // Gradient noise, about 0..1
    float Perlin(float2 P)
    {
        float2 I = floor(P);
        float2 F = P - I;
        float2 S = F * F * F * (F * (F * 6.0 - 15.0) + 10.0);
        float A = Hash(I) * 6.2831853;
        float B = Hash(I + float2(1, 0)) * 6.2831853;
        float C = Hash(I + float2(0, 1)) * 6.2831853;
        float D = Hash(I + float2(1, 1)) * 6.2831853;
        float Low = lerp(dot(float2(cos(A), sin(A)), F), dot(float2(cos(B), sin(B)), F - float2(1, 0)), S.x);
        float High = lerp(dot(float2(cos(C), sin(C)), F - float2(0, 1)), dot(float2(cos(D), sin(D)), F - float2(1, 1)), S.x);
        return saturate(0.5 + 0.75 * lerp(Low, High, S.y));
    }
};
FWaterNoise Noise;

const float2 ArtSize = float2(1376.0, 768.0);
float4 Here = Texture2DSample(Tex, TexSampler, UV);
float Sea = Here.a;
if (Sea <= 0.001)
{
    return float4(pow(saturate(Here.rgb), 1.0 / 2.2), 0.0);
}

// How near the viewer: 0 at the horizon, 1 at the bottom; waves are bigger and slower-looking nearer
float Near = saturate((UV.y - HorizonV) / (1.0 - HorizonV));
float WaveScale = lerp(18.0, 70.0, Near);
// Wave space in art pixels, squashed vertically (the sea is seen at a low angle), drifting with the flow
float2 Pixel = UV * ArtSize;
float2 Wave = float2(Pixel.x, Pixel.y * 2.6) / WaveScale;
float2 Flow = float2(Time * FlowSpeed, Time * FlowSpeed * 0.15);

// Ripples: the picture under the sea nudged about
float2 Nudge = float2(Noise.Perlin(Wave - Flow) - 0.5, Noise.Perlin(Wave * 1.3 + 7.1 + Flow * 0.7) - 0.5);
float2 Offset = Nudge * 2.0 * RippleAmount * lerp(0.35, 1.0, Near) / ArtSize;
float4 There = Texture2DSample(Tex, TexSampler, UV + Offset);
// Never pull in the dock or the beach: only sea from the nudged spot
float3 Color = lerp(Here.rgb, There.rgb, There.a);

// Sheen: long thin glints along the swell, catching the light where it crests
float2 Streak = float2(Pixel.x / (WaveScale * 2.2), Pixel.y * 2.6 / (WaveScale * 0.35));
float Swell = Noise.Perlin(Streak - Flow * 1.4) * 0.65 + Noise.Perlin(Streak * 2.1 + 3.3 + Flow) * 0.35;
float Glint = smoothstep(0.62, 0.78, Swell);
// Sparkles: small bright points that come and go
float2 Grain = Pixel / lerp(6.0, 14.0, Near);
float Twinkle = Noise.Perlin(Grain + float2(Time * 0.9, 0)) * Noise.Perlin(Grain * 0.7 - float2(0, Time * 1.3) + 11.0);
float Sparkle = smoothstep(0.5, 0.58, Twinkle);
// Brighter toward the horizon, where the sky reflects
float Light = lerp(1.0, 0.55, Near);
Color += SheenColor * Light * (SheenAmount * Glint + SparkleAmount * Sparkle);
// The canvas takes this as screen (gamma) color, the texture gave linear
return float4(pow(saturate(Color), 1.0 / 2.2), Sea);
"""

uv = node(U.MaterialExpressionTextureCoordinate, -1000, 0)
tex = node(U.MaterialExpressionTextureObjectParameter, -1000, 150, parameter_name="WaterTex",
           texture=U.load_asset("/Game/UI/T_TitleWater"), sampler_type=U.MaterialSamplerType.SAMPLERTYPE_COLOR)
time = node(U.MaterialExpressionTime, -1000, 300)
sheen = node(U.MaterialExpressionVectorParameter, -1000, 400, parameter_name="SheenColor",
             default_value=U.LinearColor(1.0, 0.96, 0.85, 1))
sheen_rgb = node(U.MaterialExpressionComponentMask, -800, 400, r=True, g=True, b=True, a=False)
link(sheen, "", sheen_rgb, "")

sources = {"Tex": (tex, ""), "UV": (uv, ""), "Time": (time, ""), "SheenColor": (sheen_rgb, "")}
y = 550
for pname, value in [("HorizonV", 0.75), ("FlowSpeed", 0.35), ("RippleAmount", 2.5), ("SheenAmount", 0.22),
                     ("SparkleAmount", 0.45)]:
    sources[pname] = (node(U.MaterialExpressionScalarParameter, -1000, y, parameter_name=pname, default_value=value), "")
    y += 80

custom = node(U.MaterialExpressionCustom, -400, 200, code=CODE, output_type=U.CustomMaterialOutputType.CMOT_FLOAT4,
              description="Title sea")
inputs = []
for input_name in sources:
    custom_input = U.CustomInput()
    custom_input.set_editor_property("input_name", input_name)
    inputs.append(custom_input)
custom.set_editor_property("inputs", inputs)
for input_name, (source, out) in sources.items():
    link(source, out, custom, input_name)

rgb = node(U.MaterialExpressionComponentMask, -150, 150, r=True, g=True, b=True, a=False)
link(custom, "", rgb, "")
alpha = node(U.MaterialExpressionComponentMask, -150, 300, r=False, g=False, b=False, a=True)
link(custom, "", alpha, "")
mel.connect_material_property(rgb, "", U.MaterialProperty.MP_EMISSIVE_COLOR)
mel.connect_material_property(alpha, "", U.MaterialProperty.MP_OPACITY)
mel.recompile_material(mat)
eal.save_asset(path)
unreal.log(f"WATER made {path}")
