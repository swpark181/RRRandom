"""Dirt road material for the island (run inside Unreal).

Usage: UnrealEditor-Cmd <project> -run=pythonscript "-script=ue_make_path_material.py <tile png>"

Imports the seamless dirt tile (make_path_tile.py, from a Tripo image) as /Game/Environment/Island/Textures/T_DirtPath
and builds /Game/Environment/Island/M_IslandPath: one big plane over the whole island (ARRRandomIsland) that shows the
dirt only where the roads are. The roads come from "RoadMask", a texture ARRRandomIsland makes from its map (one texel
per share of a cell, blurred so corners are round), laid over the map's area ("MapArea": its far X, its left Y, its
size along X and along Y). The mask's edge is roughened with world-space noise ("EdgeNoise"), so a road's edge wanders
a little into the grass instead of running along the cells. The dirt repeats every "TileSize" units in world space.
Masked (cut at 0.5), fully rough, a little specular. The mask is sampled as an sRGB color (the island writes it
sRGB-encoded so it reads back linear), like its default, the engine's white square.
"""
import sys

import unreal

U = unreal
eal = U.EditorAssetLibrary
mel = U.MaterialEditingLibrary
tile_png = " ".join(sys.argv[1:]).strip()
FOLDER = "/Game/Environment/Island"
TEX = f"{FOLDER}/Textures/T_DirtPath"
MAT = f"{FOLDER}/M_IslandPath"

task = U.AssetImportTask()
task.filename = tile_png
task.destination_path = f"{FOLDER}/Textures"
task.destination_name = "T_DirtPath"
task.automated = True
task.replace_existing = True
task.save = True
U.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
tex = U.load_asset(TEX)
tex.set_editor_property("srgb", True)
tex.set_editor_property("address_x", U.TextureAddress.TA_WRAP)
tex.set_editor_property("address_y", U.TextureAddress.TA_WRAP)
# Always full size (a 1 MB tile; streaming kept it at a low mip, smearing the pebbles), and the smaller mips sharpened,
# since the high camera sees it a few mips down
tex.set_editor_property("never_stream", True)
tex.set_editor_property("mip_gen_settings", U.TextureMipGenSettings.TMGS_SHARPEN4)
eal.save_asset(TEX)

# Its height map (make_path_height.py, next to the tile as <name>_H.png): linear greyscale, for the relief
HEIGHT = f"{FOLDER}/Textures/T_DirtPath_H"
task = U.AssetImportTask()
task.filename = tile_png[:-4] + "_H.png"
task.destination_path = f"{FOLDER}/Textures"
task.destination_name = "T_DirtPath_H"
task.automated = True
task.replace_existing = True
task.save = True
U.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
height_tex = U.load_asset(HEIGHT)
height_tex.set_editor_property("srgb", False)
height_tex.set_editor_property("compression_settings", U.TextureCompressionSettings.TC_GRAYSCALE)
height_tex.set_editor_property("address_x", U.TextureAddress.TA_WRAP)
height_tex.set_editor_property("address_y", U.TextureAddress.TA_WRAP)
height_tex.set_editor_property("never_stream", True)
eal.save_asset(HEIGHT)

if eal.does_asset_exist(MAT):
    eal.delete_asset(MAT)
mat = U.AssetToolsHelpers.get_asset_tools().create_asset("M_IslandPath", FOLDER, U.Material, U.MaterialFactoryNew())
mat.set_editor_property("blend_mode", U.BlendMode.BLEND_MASKED)
mat.set_editor_property("opacity_mask_clip_value", 0.5)
mat.set_editor_property("used_with_instanced_static_meshes", True)


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
    for name in names:
        ci = U.CustomInput()
        ci.set_editor_property("input_name", name)
        ins.append(ci)
    c.set_editor_property("inputs", ins)
    return c


# Value noise in world space (inlined in each Custom node below): Noise(P) in 0..1, smooth
NOISE = (
    "float2 NI = floor(NP);\n"
    "float2 NF = frac(NP);\n"
    "NF = NF * NF * (3.0 - 2.0 * NF);\n"
    "float NA = frac(sin(dot(NI, float2(127.1, 311.7))) * 43758.5453);\n"
    "float NB = frac(sin(dot(NI + float2(1, 0), float2(127.1, 311.7))) * 43758.5453);\n"
    "float NC = frac(sin(dot(NI + float2(0, 1), float2(127.1, 311.7))) * 43758.5453);\n"
    "float ND = frac(sin(dot(NI + float2(1, 1), float2(127.1, 311.7))) * 43758.5453);\n"
    "float NV = lerp(lerp(NA, NB, NF.x), lerp(NC, ND, NF.x), NF.y);\n")


def noise_at(var, expr):
    """HLSL that sets float <var> to the noise at <expr>."""
    return f"float {var};\n{{\nfloat2 NP = {expr};\n{NOISE}{var} = NV;\n}}\n"


def scalar(name, x, y, value):
    return node(U.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=value)


def color(name, x, y, rgb):
    return node(U.MaterialExpressionVectorParameter, x, y, parameter_name=name, default_value=U.LinearColor(*rgb, 1.0))


world = node(U.MaterialExpressionWorldPosition, -1600, 0)
area = node(U.MaterialExpressionVectorParameter, -1600, 200, parameter_name="MapArea",
            default_value=U.LinearColor(3240.0, -3240.0, 6480.0, 6480.0))
tile_size = scalar("TileSize", -1600, 400, 620.0)
noise_amount = scalar("EdgeNoise", -1600, 480, 0.3)
pebbles = scalar("PebbleStrength", -1600, 560, 1.0)
edge_dark = scalar("EdgeDarken", -1600, 640, 0.6)
# Colors are linear (the texture is decoded to linear too): the tile's mean (sRGB 0.86, 0.69, 0.5), and a tint that
# takes it to an earthy yellow-brown (about sRGB 0.76, 0.58, 0.38); the sand is about sRGB 0.93, 0.85, 0.66
dirt_mean = color("DirtMean", -1600, 720, (0.71, 0.43, 0.21))
dirt_tint = color("DirtTint", -1600, 820, (0.74, 0.66, 0.5))
sand = color("SandColor", -1600, 920, (0.85, 0.69, 0.39))

# The dirt, repeating in world space (X up the screen, Y to the right, as the texture's V and U)
dirt_uv = custom(-1200, -100, U.CustomMaterialOutputType.CMOT_FLOAT2, "Dirt UV",
                 "return float2(World.y, -World.x) / max(TileSize, 1.0);", ("World", "TileSize"))
link(world, "", dirt_uv, "World")
link(tile_size, "", dirt_uv, "TileSize")
dirt = node(U.MaterialExpressionTextureSample, -900, -100, texture=tex)
link(dirt_uv, "", dirt, "UVs")

# Where the roads (R) and the beach (G) are: the island's mask over its area
mask_uv = custom(-1200, 250, U.CustomMaterialOutputType.CMOT_FLOAT2, "Road mask UV",
                 "// MapArea: far X, left Y, size along X, size along Y; the mask's rows run down the screen\n"
                 "return float2((World.y - Area.y) / max(Area.w, 1.0), (Area.x - World.x) / max(Area.z, 1.0));",
                 ("World", "Area"))
link(world, "", mask_uv, "World")
link(area, "RGBA", mask_uv, "Area")
mask = node(U.MaterialExpressionTextureSampleParameter2D, -900, 250, parameter_name="RoadMask",
            texture=U.load_asset("/Engine/EngineResources/WhiteSquareTexture"),
            sampler_type=U.MaterialSamplerType.SAMPLERTYPE_COLOR)
link(mask_uv, "", mask, "UVs")

# Color: the tile as painted (its pebbles and tufts crisp), a little earthier, in big soft patches of lighter and
# darker soil; a darker rim right along the wandering edge (packed soil and the grass's shade) outlines the road like a
# painted path; the beach plain sand with a little grain
EDGE_COVER = (noise_at("Wander", "World.xy / 150.0") + noise_at("Ruffle", "World.xy / 45.0 + 17.0")
              + "float Cover = max(Mask.r, Mask.g) + (Wander - 0.5) * Amount + (Ruffle - 0.5) * Amount * 0.6;\n")
paint = custom(-500, -50, U.CustomMaterialOutputType.CMOT_FLOAT3, "Path color",
               noise_at("Big", "World.xy / 760.0") + noise_at("Fine", "World.xy / 38.0 + 7.0") + EDGE_COVER
               + "float Road = Mask.r;\n"
                 "float Beach = Mask.g;\n"
                 "float3 Dirt = (Mean + (Tex - Mean) * Pebbles) * Tint;\n"
                 "Dirt *= 0.9 + 0.18 * Big;\n"
                 "float Rim = 1.0 - smoothstep(0.5, 0.64, Cover);\n"
                 "Dirt *= lerp(1.0, EdgeDarken, Rim);\n"
                 "float3 Grain = Sand * (0.9 + 0.1 * Fine + 0.06 * Big) * lerp(1.0, 0.85, Rim);\n"
                 "return lerp(Dirt, Grain, saturate(Beach / max(Road + Beach, 0.001)));",
               ("Tex", "Mask", "World", "Mean", "Tint", "Sand", "Pebbles", "EdgeDarken", "Amount"))
link(dirt, "RGB", paint, "Tex")
link(mask, "RGB", paint, "Mask")
link(world, "", paint, "World")
link(dirt_mean, "", paint, "Mean")
link(dirt_tint, "", paint, "Tint")
link(sand, "", paint, "Sand")
link(pebbles, "", paint, "Pebbles")
link(edge_dark, "", paint, "EdgeDarken")
link(noise_amount, "", paint, "Amount")
mel.connect_material_property(paint, "", U.MaterialProperty.MP_BASE_COLOR)

# Coverage: the mask with the same wandering edge, dithered over a narrow band (the anti-aliasing smooths the dither) so
# the edge is crisp but not jagged
edge = custom(-500, 300, U.CustomMaterialOutputType.CMOT_FLOAT1, "Road edge",
              EDGE_COVER
              + "float Alpha = smoothstep(0.46, 0.54, Cover);\n"
                "// Dithered against the 0.5 cut: interleaved gradient noise on the pixel, shifted every frame for the anti-aliasing\n"
                "float2 Pixel = Parameters.SvPosition.xy + float(View.StateFrameIndexMod8) * float2(5.588238, 5.588238);\n"
                "float Threshold = frac(52.9829189 * frac(dot(Pixel, float2(0.06711056, 0.00583715))));\n"
                "return Alpha >= 0.999 ? 1.0 : (Alpha <= 0.001 ? 0.0 : Alpha - Threshold + 0.5);",
              ("Mask", "World", "Amount"))
link(mask, "RGB", edge, "Mask")
link(world, "", edge, "World")
link(noise_amount, "", edge, "Amount")
mel.connect_material_property(edge, "", U.MaterialProperty.MP_OPACITY_MASK)

# Relief, as a world-space normal (the plane's own tangents don't follow the world-space UVs): the height map's slopes
# (pebbles and tufts as domes; the texture's U runs along world +Y, its V along world -X), a fine grain of packed soil,
# and at the edge a slope down from the grass into the road (the road reads as worn into the lawn)
mat.set_editor_property("tangent_space_normal", False)
height_obj = node(U.MaterialExpressionTextureObject, -900, 600, texture=height_tex,
                  sampler_type=U.MaterialSamplerType.SAMPLERTYPE_GRAYSCALE)
bumps = scalar("BumpHeight", -1600, 1020, 9.0)
grain = scalar("GrainHeight", -1600, 1100, 1.6)
bevel = scalar("EdgeBevel", -1600, 1180, 45.0)
relief = custom(-500, 650, U.CustomMaterialOutputType.CMOT_FLOAT3, "Path relief",
                EDGE_COVER
                + "// Height slopes in world units per world unit, from the map (a texel apart either way)\n"
                  "float E = 1.0 / 1024.0;\n"
                  "float HU1 = HeightTex.Sample(HeightTexSampler, UV + float2(E, 0)).r;\n"
                  "float HU0 = HeightTex.Sample(HeightTexSampler, UV - float2(E, 0)).r;\n"
                  "float HV1 = HeightTex.Sample(HeightTexSampler, UV + float2(0, E)).r;\n"
                  "float HV0 = HeightTex.Sample(HeightTexSampler, UV - float2(0, E)).r;\n"
                  "float Step = 2.0 * E * TileSize;\n"
                  "float DY = (HU1 - HU0) / Step * Bumps;\n"
                  "float DX = -(HV1 - HV0) / Step * Bumps;\n"
                  "// Grain: value noise slopes, small\n"
                + noise_at("G0", "World.xy / 9.0") + noise_at("GX", "(World.xy + float2(2.0, 0.0)) / 9.0")
                + noise_at("GY", "(World.xy + float2(0.0, 2.0)) / 9.0")
                + "DX += (GX - G0) / 2.0 * Grain;\n"
                  "DY += (GY - G0) / 2.0 * Grain;\n"
                  "// Edge: the coverage's world gradient (from its screen derivatives) tilts the surface up toward the grass\n"
                  "float2 SX = ddx(World.xy);\n"
                  "float2 SY = ddy(World.xy);\n"
                  "float Det = SX.x * SY.y - SX.y * SY.x;\n"
                  "float2 CG = abs(Det) > 1e-6 ? float2(ddx(Cover) * SY.y - ddy(Cover) * SX.y, ddy(Cover) * SX.x - ddx(Cover) * SY.x) / Det : 0;\n"
                  "float Lip = 1.0 - smoothstep(0.5, 0.75, Cover);\n"
                  "DX -= CG.x * Bevel * Lip;\n"
                  "DY -= CG.y * Bevel * Lip;\n"
                  "return normalize(float3(-DX, -DY, 1.0));",
                ("HeightTex", "UV", "Mask", "World", "Amount", "TileSize", "Bumps", "Grain", "Bevel"))
link(height_obj, "", relief, "HeightTex")
link(dirt_uv, "", relief, "UV")
link(mask, "RGB", relief, "Mask")
link(world, "", relief, "World")
link(noise_amount, "", relief, "Amount")
link(tile_size, "", relief, "TileSize")
link(bumps, "", relief, "Bumps")
link(grain, "", relief, "Grain")
link(bevel, "", relief, "Bevel")
mel.connect_material_property(relief, "", U.MaterialProperty.MP_NORMAL)

# Pebbles a touch glossier than the dry soil
gloss = custom(-500, 950, U.CustomMaterialOutputType.CMOT_FLOAT1, "Path roughness",
               "float H = HeightTex.Sample(HeightTexSampler, UV).r;\n"
               "return lerp(0.95, 0.62, saturate((H - 0.3) * 2.0));",
               ("HeightTex", "UV"))
link(height_obj, "", gloss, "HeightTex")
link(dirt_uv, "", gloss, "UV")
mel.connect_material_property(gloss, "", U.MaterialProperty.MP_ROUGHNESS)
spec = node(U.MaterialExpressionConstant, -200, 1050, r=0.35)
mel.connect_material_property(spec, "", U.MaterialProperty.MP_SPECULAR)
mel.recompile_material(mat)
eal.save_asset(MAT)
unreal.log(f"PATH made {MAT} with {TEX}")
