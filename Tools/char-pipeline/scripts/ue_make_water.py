"""The island's water, made to look like the title screen's sea (run inside Unreal).

Usage: UnrealEditor-Cmd <project> -run=pythonscript "-script=ue_make_water.py /Game/Environment/Island/M_IslandWater"

Replaces M_IslandWater (ARRRandomIsland's water surfaces and waterfalls). Like the title art's sea:
- clear turquoise, light over the shallows near the shore and deep teal further out: "ShoreMask" (a texture the island
  makes from its map, land white, blurred out over the water) laid over the map's area ("MapArea": far X, left Y,
  size along X and Y), the open sea beyond the map deep,
- a network of bright caustic lines drifting over the shallows (two layers of animated cellular noise, the gaps
  between their cells), fading out toward the deep,
- a white ruffle of foam along the shore, wobbling and lapping in and out,
- ripples in the normal (animated noise slopes) and a low roughness, so the sun glints off the swell, plus small
  sparkles that twinkle on and off,
- waterfall faces (steep normals): bright streaks falling down instead.
Seen from the side too (the third person camera), not only from above: a smooth surface ("Roughness" 0.08) so the
engine's reflections put the sky on it, plus a sheen of sky color that grows toward grazing angles (Fresnel), so the far
sea turns into the horizon instead of a flat band; the fine detail (caustics, sparkles, small ripples) fades with
distance ("DetailFade" units) when seen at a slant, where it would only shimmer (looking down from the top view's high
camera it all stays).
Opaque and lit, world-space normal; one Custom (HLSL) node for color, one for the normal, one for the glow.
"""
import sys

import unreal

U = unreal
eal = U.EditorAssetLibrary
mel = U.MaterialEditingLibrary
path = " ".join(sys.argv[1:]).strip()
folder, name = path.rsplit("/", 1)

if eal.does_asset_exist(path):
    eal.delete_asset(path)
mat = U.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, U.Material, U.MaterialFactoryNew())
mat.set_editor_property("used_with_instanced_static_meshes", True)
mat.set_editor_property("tangent_space_normal", False)


def node(cls, x, y, **props):
    n = mel.create_material_expression(mat, cls, x, y)
    for key, value in props.items():
        n.set_editor_property(key, value)
    return n


def link(a, a_out, b, b_in):
    if not mel.connect_material_expressions(a, a_out, b, b_in):
        raise RuntimeError(f"could not connect into {b_in}")


def custom(x, y, out_type, desc, code, names):
    c = node(U.MaterialExpressionCustom, x, y, output_type=out_type, description=desc, code=code)
    ins = []
    for n in names:
        ci = U.CustomInput()
        ci.set_editor_property("input_name", n)
        ins.append(ci)
    c.set_editor_property("inputs", ins)
    return c


def scalar(n, x, y, v):
    return node(U.MaterialExpressionScalarParameter, x, y, parameter_name=n, default_value=v)


def color(n, x, y, rgb):
    return node(U.MaterialExpressionVectorParameter, x, y, parameter_name=n, default_value=U.LinearColor(*rgb, 1.0))


# Shared HLSL helpers, pasted at the top of each Custom node
HELPERS = """
struct FWater
{
    static float Hash(float2 P) { return frac(sin(dot(P, float2(127.1, 311.7))) * 43758.5453); }
    static float2 Hash2(float2 P) { return frac(sin(float2(dot(P, float2(127.1, 311.7)), dot(P, float2(269.5, 183.3)))) * 43758.5453); }
    static float Noise(float2 P)
    {
        float2 I = floor(P); float2 F = frac(P); F = F * F * (3.0 - 2.0 * F);
        return lerp(lerp(Hash(I), Hash(I + float2(1, 0)), F.x), lerp(Hash(I + float2(0, 1)), Hash(I + float2(1, 1)), F.x), F.y);
    }
    // Distance to the border between cells (F2 - F1) of a cellular noise whose points wander with time
    static float CellEdge(float2 P, float T)
    {
        float2 I = floor(P); float2 F = frac(P);
        float F1 = 8.0; float F2 = 8.0;
        for (int Y = -1; Y <= 1; Y++)
        {
            for (int X = -1; X <= 1; X++)
            {
                float2 G = float2(X, Y);
                float2 O = Hash2(I + G);
                O = 0.5 + 0.45 * sin(T + 6.2831 * O);
                float D = length(G + O - F);
                if (D < F1) { F2 = F1; F1 = D; } else if (D < F2) { F2 = D; }
            }
        }
        return F2 - F1;
    }
};
"""

world = node(U.MaterialExpressionWorldPosition, -1600, 0)
time = node(U.MaterialExpressionTime, -1600, 120)
normal_ws = node(U.MaterialExpressionVertexNormalWS, -1600, 220)
depth = node(U.MaterialExpressionPixelDepth, -1600, 1500)
view = node(U.MaterialExpressionCameraVectorWS, -1600, 1580)
detail_fade = scalar("DetailFade", -1600, 1660, 3500.0)
area = color("MapArea", -1600, 320, (0.0, 0.0, 1.0))
area.set_editor_property("default_value", U.LinearColor(3240.0, -3240.0, 6480.0, 6480.0))
mask_uv = custom(-1250, 320, U.CustomMaterialOutputType.CMOT_FLOAT2, "Shore mask UV",
                 "return float2((World.y - Area.y) / max(Area.w, 1.0), (Area.x - World.x) / max(Area.z, 1.0));",
                 ("World", "Area"))
link(world, "", mask_uv, "World")
link(area, "RGBA", mask_uv, "Area")
shore = node(U.MaterialExpressionTextureSampleParameter2D, -950, 320, parameter_name="ShoreMask",
             texture=U.load_asset("/Engine/EngineResources/Black"),
             sampler_type=U.MaterialSamplerType.SAMPLERTYPE_COLOR)
link(mask_uv, "", shore, "UVs")

# Linear colors: shallow turquoise, mid teal, deep sea blue, caustic and foam whites
shallow = color("Shallow", -1600, 450, (0.16, 0.62, 0.55))
middle = color("Middle", -1600, 550, (0.02, 0.32, 0.38))
deep = color("Deep", -1600, 650, (0.005, 0.1, 0.2))
caustic = color("Caustic", -1600, 750, (0.75, 1.0, 0.92))
foam = color("Foam", -1600, 850, (0.95, 0.98, 1.0))
caustic_amount = scalar("CausticAmount", -1600, 950, 0.7)
caustic_scale = scalar("CausticScale", -1600, 1030, 120.0)
foam_amount = scalar("FoamAmount", -1600, 1110, 0.9)
sparkle_amount = scalar("SparkleAmount", -1600, 1190, 2.5)
flow = scalar("FlowSpeed", -1600, 1270, 12.0)

COLOR = HELPERS + """
float T = Time;
float2 P = World.xy;
// Waterfall faces: bright streaks running down, foaming at the bottom
if (abs(VN.z) < 0.5)
{
    float2 F = float2(dot(World.xy, float2(-VN.y, VN.x)) / 40.0, World.z / 90.0 + T * 2.2);
    float Streak = FWater::Noise(float2(F.x * 3.0, F.y)) * 0.6 + FWater::Noise(float2(F.x * 7.0, F.y * 2.3 + 5.0)) * 0.4;
    float3 Fall = lerp(Middle * 1.6, Foam, smoothstep(0.45, 0.85, Streak));
    return Fall;
}
// How near the shore: 1 on land, fading to 0 a cell or two out over the water (none past the map)
float Near = saturate(Shore);
float Shallowness = smoothstep(0.0, 0.42, Near);
// Big slow patches of darker and lighter water, so the deep isn't flat
float Patch = FWater::Noise(P / 900.0 + float2(T * 0.01, 0));
float3 Water = lerp(Deep, Middle, saturate(Shallowness * 1.6 + Patch * 0.35));
Water = lerp(Water, Shallow, smoothstep(0.15, 0.6, Shallowness));
// Caustics: the thin borders of two drifting cell layers, brightest in the shallows, a faint web in the deep
float2 Drift = float2(Flow, Flow * 0.6) * T;
// Warped by slow noise, so the light's web curves and wobbles instead of running in straight lines
float2 Warp = float2(FWater::Noise(P / 260.0 + T * 0.05), FWater::Noise(P / 260.0 + 9.3 - T * 0.04)) - 0.5;
float2 Q = P + Warp * 140.0;
float C1 = FWater::CellEdge((Q + Drift) / CausticScale, T * 0.9);
float C2 = FWater::CellEdge((Q - Drift * 0.7) / (CausticScale * 1.37) + 3.1, T * 0.7 + 1.7);
float Lines = (1.0 - smoothstep(0.0, 0.05, C1)) * 0.65 + (1.0 - smoothstep(0.0, 0.04, C2)) * 0.45;
// Brighter in some places than others, like light through a moving surface
float Bright = smoothstep(0.25, 0.85, FWater::Noise(P / 420.0 - T * 0.03));
// Only close up when seen at a slant: far off at a low angle the fine web only shimmers (from high above it stays)
float Slant = 1.0 - smoothstep(0.3, 0.65, abs(normalize(V).z));
float Close = 1.0 - smoothstep(Fade * 0.35, Fade, Depth) * Slant;
Water += Caustic * Lines * Bright * CausticAmount * lerp(0.12, 1.0, Shallowness) * Close;
// Foam along the shore: a ruffled band right at the edge of the water, lapping in and out
float Lap = 0.5 + 0.5 * sin(T * 1.3 + FWater::Noise(P / 120.0) * 6.0);
float Ruffle = FWater::Noise(P / 35.0 + float2(T * 0.3, 0)) * 0.5 + FWater::Noise(P / 14.0 - float2(0, T * 0.4)) * 0.5;
float Edge = Near - (0.44 + 0.05 * Lap);
float FoamBand = smoothstep(-0.03, 0.0, Edge) * (1.0 - smoothstep(0.02, 0.07, Edge));
FoamBand *= smoothstep(0.4, 0.65, Ruffle + 0.15);
Water = lerp(Water, Foam, saturate(FoamBand * FoamAmount));
return Water;
"""
color_node = custom(-500, 0, U.CustomMaterialOutputType.CMOT_FLOAT3, "Water color", COLOR,
                    ("World", "Time", "VN", "Shore", "Shallow", "Middle", "Deep", "Caustic", "Foam", "CausticAmount",
                     "CausticScale", "FoamAmount", "Flow", "Depth", "Fade", "V"))
for src, out, dst in ((world, "", "World"), (time, "", "Time"), (normal_ws, "", "VN"), (shore, "R", "Shore"),
                      (shallow, "", "Shallow"), (middle, "", "Middle"), (deep, "", "Deep"), (caustic, "", "Caustic"),
                      (foam, "", "Foam"), (caustic_amount, "", "CausticAmount"), (caustic_scale, "", "CausticScale"),
                      (foam_amount, "", "FoamAmount"), (flow, "", "Flow"), (depth, "", "Depth"), (detail_fade, "", "Fade"), (view, "", "V")):
    link(src, out, color_node, dst)
mel.connect_material_property(color_node, "", U.MaterialProperty.MP_BASE_COLOR)

# Sparkles: tiny glints that twinkle on and off over the swell (emissive, so they read in any light)
SPARKLE = HELPERS + """
if (abs(VN.z) < 0.5) { return 0; }
float2 Cell = floor(World.xy / 22.0);
float Seed = FWater::Hash(Cell);
float2 Spot = (Cell + 0.2 + 0.6 * FWater::Hash2(Cell + 7.0)) * 22.0;
float Dist = length(World.xy - Spot);
float Blink = pow(saturate(sin(Time * (1.5 + 2.5 * Seed) + Seed * 40.0)), 24.0);
float Dot = 1.0 - smoothstep(0.0, 3.2, Dist);
float Slant = 1.0 - smoothstep(0.3, 0.65, abs(normalize(V).z));
return Dot * Blink * step(0.72, Seed) * Amount * (1.0 - smoothstep(Fade * 0.4, Fade, Depth) * Slant);
"""
sparkle = custom(-500, 450, U.CustomMaterialOutputType.CMOT_FLOAT1, "Sparkles", SPARKLE, ("World", "Time", "VN", "Amount", "Depth", "Fade", "V"))
link(world, "", sparkle, "World")
link(time, "", sparkle, "Time")
link(normal_ws, "", sparkle, "VN")
link(sparkle_amount, "", sparkle, "Amount")
link(depth, "", sparkle, "Depth")
link(detail_fade, "", sparkle, "Fade")
link(view, "", sparkle, "V")


# Ripples: the slopes of two drifting noise layers tilt the normal, so the sun glints across the swell
NORMAL = HELPERS + """
if (abs(VN.z) < 0.5) { return VN; }
// A few sine swells crossing at different angles, lengths and speeds; their slopes are exact (noise slopes taken by
// finite differences striped the water)
float2 P = World.xy;
float T = Time;
float2 Slope = 0;
float Slant = 1.0 - smoothstep(0.3, 0.65, abs(normalize(V).z));
const float3 Waves[6] = { float3(0.94, 0.34, 260.0), float3(-0.42, 0.91, 190.0), float3(0.70, -0.71, 140.0),
                          float3(-0.97, -0.24, 95.0), float3(0.26, 0.97, 70.0), float3(-0.60, 0.80, 48.0) };
for (int I = 0; I < 6; I++)
{
    float2 D = Waves[I].xy;
    float L = Waves[I].z;
    float K = 6.2831 / L;
    float Speed = sqrt(L) * 0.18;
    float Amp = L * 0.012;
    // Short waves fade out with distance (they would shimmer), the long swells hold on further
    float Keep = 1.0 - smoothstep(Fade * L / 120.0, Fade * L / 40.0, Depth) * 0.85 * Slant;
    Slope += D * K * Amp * cos(K * dot(D, P) - T * Speed * K * 10.0 + I * 1.7) * Keep;
}
return normalize(float3(-Slope * Strength, 1.0));
"""
strength = scalar("RippleStrength", -1600, 1350, 0.6)
normal_node = custom(-500, 650, U.CustomMaterialOutputType.CMOT_FLOAT3, "Water ripples", NORMAL,
                     ("World", "Time", "VN", "Strength", "Depth", "Fade", "V"))
link(world, "", normal_node, "World")
link(time, "", normal_node, "Time")
link(normal_ws, "", normal_node, "VN")
link(strength, "", normal_node, "Strength")
link(depth, "", normal_node, "Depth")
link(detail_fade, "", normal_node, "Fade")
link(view, "", normal_node, "V")
mel.connect_material_property(normal_node, "", U.MaterialProperty.MP_NORMAL)

# Glow: the sparkles, and a sheen of sky color toward grazing angles
SHEEN = """
if (abs(VN.z) < 0.5) { return Sparkle * Foam; }
// Schlick's Fresnel on the rippled normal: next to nothing looking down, most of the sky's color at a slant
float Facing = saturate(dot(normalize(N), normalize(V)));
float Fresnel = pow(1.0 - Facing, 5.0);
return Sparkle * Foam + Sky * Fresnel * Amount;
"""
sky_tint = color("SkySheen", -1600, 1740, (0.42, 0.78, 0.95))
sheen_amount = scalar("SheenAmount", -1600, 1820, 0.3)
glow = custom(-200, 450, U.CustomMaterialOutputType.CMOT_FLOAT3, "Glow", SHEEN, ("Sparkle", "Foam", "VN", "N", "V", "Sky", "Amount"))
for src, dst in ((sparkle, "Sparkle"), (foam, "Foam"), (normal_ws, "VN"), (normal_node, "N"), (view, "V"), (sky_tint, "Sky"), (sheen_amount, "Amount")):
    link(src, "", glow, dst)
mel.connect_material_property(glow, "", U.MaterialProperty.MP_EMISSIVE_COLOR)

rough = scalar("Roughness", -200, 850, 0.08)
mel.connect_material_property(rough, "", U.MaterialProperty.MP_ROUGHNESS)
spec = node(U.MaterialExpressionConstant, -200, 950, r=0.7)
mel.connect_material_property(spec, "", U.MaterialProperty.MP_SPECULAR)
mel.recompile_material(mat)
eal.save_asset(path)
unreal.log(f"WATER made {path}")
