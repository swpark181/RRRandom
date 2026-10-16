"""Fog of war post process material (run inside Unreal).

Usage: UnrealEditor-Cmd <project> -run=pythonscript "-script=ue_make_fog.py /Game/PostProcess/M_PP_Fog"

The game (ARRRandomFogOfWar) paints a small texture each frame over the arena floor and hands it to a dynamic
instance of this material: red = seen right now (soft edged circles around the team), green = seen at some point.
The material looks it up where each pixel's view ray meets a flat level near its surface ("FogOrigin" = the floor
corner, its blue = the floor's height, "LevelHeight" = the island's terrace step; the Tripo blocks' bumpy tops would
make it jump in lines), "FogSize" = the floor's width, "FogResolution" = cells per side:
- seen now: the scene as is,
- explored: darker ("ExploredLevel") and greyer ("ExploredDesaturate") under a thin haze,
- never seen: almost black ("UnexploredLevel", tinted "FogColor") under slowly drifting cloud ("CloudBrightness").
No stair steps along the grid: the lookup is a cubic B-spline through the 4x4 nearest cells (read one by one at their
middles, so it doesn't matter how the texture is filtered), wobbled by a noise warp ("WarpAmount" world units). The
edges of the seen and explored areas aren't a plain fade either: a drifting cloud of gradient noise (value noise has
square, grid-aligned features that showed as steps; "CloudScale" world units, "CloudSpeed" per second) moves the threshold about ("EdgeNoise", over "EdgeWidth"
of the fade), so the fog parts like billowing cloud, its thinning rim a little lit ("RimBrightness").
All of that is one Custom (HLSL) node; the parameters feed it.
After tonemapping, so it darkens the final colors (the toon post process runs before it); the HUD draws on top.
The texture parameter is a plain color sampler: the game's texture is sRGB like the engine's default black, so the
node turns the samples back into the game's 0..1 values.
"""
import sys

import unreal

path = " ".join(sys.argv[1:]).strip()
folder, name = path.rsplit("/", 1)
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
U = unreal


def log(msg):
    unreal.log(f"FOG {msg}")


if eal.does_asset_exist(path):
    eal.delete_asset(path)
mat = U.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, U.Material, U.MaterialFactoryNew())
mat.set_editor_property("material_domain", U.MaterialDomain.MD_POST_PROCESS)
mat.set_editor_property("blendable_location", U.BlendableLocation.BL_SCENE_COLOR_AFTER_TONEMAPPING)


def node(cls, x, y, **props):
    n = mel.create_material_expression(mat, cls, x, y)
    for key, value in props.items():
        n.set_editor_property(key, value)
    return n


def link(a, a_out, b, b_in):
    if not mel.connect_material_expressions(a, a_out, b, b_in):
        raise RuntimeError(f"could not connect into {b_in}")


def scalar(pname, value, x, y):
    return node(U.MaterialExpressionScalarParameter, x, y, parameter_name=pname, default_value=value)


def mask(source, out, x, y, r=False, g=False, b=False, a=False):
    n = node(U.MaterialExpressionComponentMask, x, y, r=r, g=g, b=b, a=a)
    link(source, out, n, "")
    return n


CODE = r"""
struct FFogNoise
{
    float Hash(float2 P)
    {
        P = frac(P * float2(123.34, 456.21));
        P += dot(P, P + 45.32);
        return frac(P.x * P.y);
    }
    // Gradient (Perlin) noise, about 0..1: unlike value noise it has no square, grid-aligned features, which the
    // sharpened edge would show as stair steps
    float Gradient(float2 I, float2 F)
    {
        float Angle = Hash(I) * 6.2831853;
        return dot(float2(cos(Angle), sin(Angle)), F);
    }
    float Perlin(float2 P)
    {
        float2 I = floor(P);
        float2 F = P - I;
        float2 S = F * F * F * (F * (F * 6.0 - 15.0) + 10.0);
        float Low = lerp(Gradient(I, F), Gradient(I + float2(1, 0), F - float2(1, 0)), S.x);
        float High = lerp(Gradient(I + float2(0, 1), F - float2(0, 1)), Gradient(I + float2(1, 1), F - float2(1, 1)), S.x);
        return saturate(0.5 + 0.75 * lerp(Low, High, S.y));
    }
    // Four octaves, 0..1, each turned a little against the last so no direction lines up
    float Fbm(float2 P)
    {
        float Sum = 0;
        float Amp = 0.5;
        for (int Octave = 0; Octave < 4; Octave++)
        {
            Sum += Amp * Perlin(P);
            P = float2(0.8 * P.x - 0.6 * P.y, 0.6 * P.x + 0.8 * P.y) * 2.03 + 17.1;
            Amp *= 0.5;
        }
        return Sum / 0.9375;
    }
};
FFogNoise Noise;

// Where the fog and its cloud are measured: not on the surface itself, whose Tripo blocks have bumpy tops (a little
// higher or lower block to block, creased between faces), which would make the fog jump in straight lines and steps
// across smooth sand. Instead the view ray meets a flat level: the surface's height eased onto the island's terrace
// steps (LevelHeight apart, from GroundZ), flat within 45% of a step either side so bumps vanish (the sand and grass
// blocks bulge up to ~35 units), turning over only on the vertical faces between steps, which hardly show (cover tops
// and hills keep their own level).
float3 Ray = WorldPos - CameraPos;
float Step = (WorldPos.z - GroundZ) / max(LevelHeight, 1.0);
float Level = GroundZ + max(LevelHeight, 1.0) * (floor(Step + 0.5) + smoothstep(0.45, 0.5, abs(frac(Step + 0.5) - 0.5)) * sign(frac(Step + 0.5) - 0.5) * 0.5);
float2 FlatXY = Ray.z < -1.0 ? CameraPos.xy + Ray.xy * ((Level - CameraPos.z) / Ray.z) : WorldPos.xy;
float2 UV = (FlatXY - FogOrigin2) / max(FogSize, 1.0);
// Cloud space: world units over CloudScale, drifting
float2 P = FlatXY / max(CloudScale, 1.0);
float2 Drift = float2(1.0, 0.6) * Time * CloudSpeed;
float Billow = Noise.Fbm(P + Drift);

// The lookup, wobbled by the cloud: a cubic B-spline through the 4x4 nearest cells, read at their middles one by one so
// it is smooth whatever filtering the texture gets (no grid steps)
float2 Warp = (float2(Noise.Fbm(P * 1.7 + 5.2 - Drift), Noise.Fbm(P * 1.7 + 9.1 + Drift)) - 0.5) * 2.0 * WarpAmount / max(FogSize, 1.0);
float Res = max(FogResolution, 1.0);
float2 Grid = (UV + Warp) * Res - 0.5;
float2 Cell = floor(Grid);
float2 F = Grid - Cell;
float2 F2 = F * F;
float2 F3 = F2 * F;
float2 W[4] = { (1.0 - F) * (1.0 - F) * (1.0 - F) / 6.0, (4.0 - 6.0 * F2 + 3.0 * F3) / 6.0, (1.0 + 3.0 * F + 3.0 * F2 - 3.0 * F3) / 6.0, F3 / 6.0 };
float2 Fog = 0;
for (int Row = 0; Row < 4; Row++)
{
    for (int Col = 0; Col < 4; Col++)
    {
        // Back to the game's values (the texture is sampled as sRGB)
        float2 Sample = Texture2DSampleLevel(Tex, TexSampler, (Cell + float2(Col - 1, Row - 1) + 0.5) / Res, 0).rg;
        Fog += W[Col].x * W[Row].y * pow(saturate(Sample), 1.0 / 2.2);
    }
}

// The cloud moves where each edge sits, so they part in billows instead of a plain circle
float Shift = (Billow - 0.5) * EdgeNoise;
float Now = smoothstep(0.0, 1.0, saturate((Fog.r - 0.5 - Shift) / max(EdgeWidth, 0.01) + 0.5));
float Seen = smoothstep(0.0, 1.0, saturate((Fog.g - 0.5 - Shift) / max(EdgeWidth, 0.01) + 0.5));

// Big soft puffs over the dark
float Cloud = Noise.Fbm(P * 0.6 + Drift * 0.5 + 3.7);
Cloud *= Cloud;
float Grey = dot(Scene, float3(0.3, 0.59, 0.11));
float3 Unexplored = Scene * UnexploredLevel * FogColor + FogColor * CloudBrightness * Cloud;
float3 Explored = lerp(Scene, Grey.xxx, ExploredDesaturate) * ExploredLevel + FogColor * CloudBrightness * 0.35 * Cloud;
float3 Out = lerp(lerp(Unexplored, Explored, Seen), Scene, Now);
// The thinning rim of the cloud, a little lit
return Out + FogColor * RimBrightness * (Now * (1.0 - Now) * 4.0) * (0.5 + Billow);
"""

# Inputs of the node: name -> (source expression, its output)
world = node(U.MaterialExpressionWorldPosition, -1600, 300)
origin = node(U.MaterialExpressionVectorParameter, -1600, 450, parameter_name="FogOrigin",
              default_value=U.LinearColor(0, 0, 0, 0))
origin_xy = mask(origin, "", -1400, 450, r=True, g=True)
size = scalar("FogSize", 8000.0, -1400, 550)
black = U.load_asset("/Engine/EngineResources/Black")
tex = node(U.MaterialExpressionTextureObjectParameter, -1000, 550, parameter_name="FogTex", texture=black,
           sampler_type=U.MaterialSamplerType.SAMPLERTYPE_COLOR)
scene = node(U.MaterialExpressionSceneTexture, -1000, -200, scene_texture_id=U.SceneTextureId.PPI_POST_PROCESS_INPUT0)
scene_rgb = mask(scene, "Color", -800, -200, r=True, g=True, b=True)
fog_color = node(U.MaterialExpressionVectorParameter, -1000, 700, parameter_name="FogColor",
                 default_value=U.LinearColor(0.8, 0.85, 1.0, 1))
fog_rgb = mask(fog_color, "", -800, 700, r=True, g=True, b=True)
time = node(U.MaterialExpressionTime, -1000, 800)
camera = node(U.MaterialExpressionCameraPositionWS, -1600, 650)
ground_z = mask(origin, "", -1400, 650, b=True)

sources = {
    "Tex": (tex, ""),
    "FogOrigin2": (origin_xy, ""),
    "WorldPos": (world, ""),
    "CameraPos": (camera, ""),
    "GroundZ": (ground_z, ""),
    "Scene": (scene_rgb, ""),
    "FogColor": (fog_rgb, ""),
    "Time": (time, ""),
    "FogSize": (size, ""),
}
y = 900
for pname, value in [("FogResolution", 256.0), ("LevelHeight", 80.0),("ExploredDesaturate", 0.6), ("ExploredLevel", 0.45),
                     ("UnexploredLevel", 0.06), ("CloudScale", 700.0), ("CloudSpeed", 0.05), ("WarpAmount", 70.0),
                     ("EdgeNoise", 0.5), ("EdgeWidth", 0.65), ("CloudBrightness", 0.16), ("RimBrightness", 0.12)]:
    sources[pname] = (scalar(pname, value, -1000, y), "")
    y += 80

custom = node(U.MaterialExpressionCustom, -400, 300, code=CODE, output_type=U.CustomMaterialOutputType.CMOT_FLOAT3,
              description="Fog of war")
inputs = []
for input_name in sources:
    custom_input = U.CustomInput()
    custom_input.set_editor_property("input_name", input_name)
    inputs.append(custom_input)
custom.set_editor_property("inputs", inputs)
for input_name, (source, out) in sources.items():
    link(source, out, custom, input_name)

mel.connect_material_property(custom, "", U.MaterialProperty.MP_EMISSIVE_COLOR)
mel.recompile_material(mat)
eal.save_asset(path)
log(f"made {path}")
log("DONE")
