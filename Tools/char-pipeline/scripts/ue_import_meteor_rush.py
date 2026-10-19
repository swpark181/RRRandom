"""Meteor Rush pickups and the meteor (run inside Unreal, legacy FBX importer).

Usage: UnrealEditor-Cmd <project> -run=pythonscript -dpcvars=Interchange.FeatureFlags.Import.FBX=0
           "-script=ue_import_meteor_rush.py <fbx dir> <tripo input dir>"
  <fbx dir>: blender_props.py's output (SM_MRShard/Ore/Big/Meteor.fbx, each 100 tall, pivot at the bottom middle)
  <tripo input dir>: the unzipped Tripo exports, one folder each (shard, ore, big, meteor) with its *_basecolor.* texture

Makes /Game/MeteorRush/:
- SM_MRShard, SM_MROre, SM_MRBig, SM_MRMeteor (no collision) and their base color textures.
- M_MRGlass: lit translucent candy glass. "Tint", "Opacity" (middle), "EdgeOpacity" (the rim, by Fresnel), "Glow".
  The shard's and the ore's shells wear it; the game puts real dice inside them.
- M_MRCandy: the Tripo texture x "Tint", glossy, emissive = texture x "Glow" (the big crystal).
- M_MRMeteor: the meteor's texture, masked: its pale, colorless texels (the lottery globe's glass) are cut away so the
  game's own glass ball and tumbling dice show through; its saturated magenta cracks glow.
- M_MRDie: a plain glossy die body, "Color" and "Glow" (SM_Dice's body slots inside the shells).
"""
import glob
import sys

import unreal

U = unreal
fbx_dir, tripo_dir = sys.argv[1].replace("\\", "/"), sys.argv[2].replace("\\", "/")
dest = "/Game/MeteorRush"
eal = U.EditorAssetLibrary
mel = U.MaterialEditingLibrary
asset_tools = U.AssetToolsHelpers.get_asset_tools()


def log(msg):
    U.log(f"METEORRUSH {msg}")


def fresh_material(name):
    path = f"{dest}/{name}"
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    return asset_tools.create_asset(name, dest, U.Material, U.MaterialFactoryNew())


def node(mat, cls, x, y, **props):
    n = mel.create_material_expression(mat, cls, x, y)
    for key, value in props.items():
        n.set_editor_property(key, value)
    return n


def const(mat, value, x=-300, y=400):
    return node(mat, U.MaterialExpressionConstant, x, y, r=value)


def op(mat, cls, a, b, x=-300, y=0, a_pin="", b_pin=""):
    n = node(mat, cls, x, y)
    mel.connect_material_expressions(a, a_pin, n, "A")
    mel.connect_material_expressions(b, b_pin, n, "B")
    return n


def import_task(path, folder, name, options=None):
    task = U.AssetImportTask()
    task.filename = path
    task.destination_path = folder
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    if options:
        task.options = options
    asset_tools.import_asset_tasks([task])
    return [U.load_asset(p) for p in task.imported_object_paths]


# The glass
glass = fresh_material("M_MRGlass")
glass.set_editor_property("blend_mode", U.BlendMode.BLEND_TRANSLUCENT)
glass.set_editor_property("translucency_lighting_mode", U.TranslucencyLightingMode.TLM_SURFACE)
tint = node(glass, U.MaterialExpressionVectorParameter, -900, 0, parameter_name="Tint", default_value=U.LinearColor(0.55, 0.95, 0.8, 1.0))
middle = node(glass, U.MaterialExpressionScalarParameter, -900, 300, parameter_name="Opacity", default_value=0.28)
edge = node(glass, U.MaterialExpressionScalarParameter, -900, 400, parameter_name="EdgeOpacity", default_value=0.85)
glow = node(glass, U.MaterialExpressionScalarParameter, -900, 150, parameter_name="Glow", default_value=0.35)
fresnel = node(glass, U.MaterialExpressionFresnel, -900, 550, exponent=2.2, base_reflect_fraction=0.0)
alpha = node(glass, U.MaterialExpressionLinearInterpolate, -500, 350)
mel.connect_material_expressions(middle, "", alpha, "A")
mel.connect_material_expressions(edge, "", alpha, "B")
mel.connect_material_expressions(fresnel, "", alpha, "Alpha")
mel.connect_material_property(alpha, "", U.MaterialProperty.MP_OPACITY)
mel.connect_material_property(tint, "", U.MaterialProperty.MP_BASE_COLOR)
rim_glow = op(glass, U.MaterialExpressionMultiply, op(glass, U.MaterialExpressionMultiply, tint, glow, -650, 100),
              op(glass, U.MaterialExpressionAdd, fresnel, const(glass, 0.35, -800, 650), -650, 550), -400, 150)
mel.connect_material_property(rim_glow, "", U.MaterialProperty.MP_EMISSIVE_COLOR)
mel.connect_material_property(const(glass, 0.08), "", U.MaterialProperty.MP_ROUGHNESS)
mel.connect_material_property(const(glass, 1.0, -300, 500), "", U.MaterialProperty.MP_SPECULAR)
mel.recompile_material(glass)
eal.save_asset(glass.get_path_name())
log("M_MRGlass")

# A die inside
die = fresh_material("M_MRDie")
color = node(die, U.MaterialExpressionVectorParameter, -700, 0, parameter_name="Color", default_value=U.LinearColor(0.5, 0.9, 0.75, 1.0))
die_glow = node(die, U.MaterialExpressionScalarParameter, -700, 200, parameter_name="Glow", default_value=0.15)
mel.connect_material_property(color, "", U.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(op(die, U.MaterialExpressionMultiply, color, die_glow, -400, 150), "", U.MaterialProperty.MP_EMISSIVE_COLOR)
mel.connect_material_property(const(die, 0.3), "", U.MaterialProperty.MP_ROUGHNESS)
mel.connect_material_property(const(die, 0.6, -300, 500), "", U.MaterialProperty.MP_SPECULAR)
mel.recompile_material(die)
eal.save_asset(die.get_path_name())
log("M_MRDie")


def texture_for(folder, name):
    found = glob.glob(f"{tripo_dir}/{folder}/**/*basecolor*", recursive=True)
    if not found:
        raise RuntimeError(f"no base color texture under {tripo_dir}/{folder}")
    tex_name = name.replace("SM_", "T_") + "_BaseColor"
    if eal.does_asset_exist(f"{dest}/Textures/{tex_name}"):
        eal.delete_asset(f"{dest}/Textures/{tex_name}")
    tex = next(a for a in import_task(found[0].replace("\\", "/"), f"{dest}/Textures", tex_name) if isinstance(a, U.Texture2D))
    tex.set_editor_property("srgb", True)
    return tex


# The big crystal's candy
candy = fresh_material("M_MRCandy")
candy_tex = texture_for("big", "SM_MRBig")
sample = node(candy, U.MaterialExpressionTextureSample, -900, 0, texture=candy_tex)
candy_tint = node(candy, U.MaterialExpressionVectorParameter, -900, -200, parameter_name="Tint", default_value=U.LinearColor(1, 1, 1, 1))
candy_glow = node(candy, U.MaterialExpressionScalarParameter, -900, 250, parameter_name="Glow", default_value=0.3)
base = op(candy, U.MaterialExpressionMultiply, sample, candy_tint, -600, -50, a_pin="RGB")
mel.connect_material_property(base, "", U.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(op(candy, U.MaterialExpressionMultiply, base, candy_glow, -400, 200), "", U.MaterialProperty.MP_EMISSIVE_COLOR)
mel.connect_material_property(const(candy, 0.25), "", U.MaterialProperty.MP_ROUGHNESS)
mel.connect_material_property(const(candy, 0.7, -300, 500), "", U.MaterialProperty.MP_SPECULAR)
mel.recompile_material(candy)
eal.save_asset(candy.get_path_name())
log("M_MRCandy")

# The meteor's crust: the globe's glass cut away, the cracks glowing
meteor = fresh_material("M_MRMeteor")
meteor.set_editor_property("blend_mode", U.BlendMode.BLEND_MASKED)
meteor.set_editor_property("two_sided", True)
meteor.set_editor_property("opacity_mask_clip_value", 0.5)
meteor_tex = texture_for("meteor", "SM_MRMeteor")
msample = node(meteor, U.MaterialExpressionTextureSample, -1200, 0, texture=meteor_tex)
# Saturation (max - min) and brightness of the texel
mx = op(meteor, U.MaterialExpressionMax, op(meteor, U.MaterialExpressionMax, msample, msample, -1000, -200, "R", "G"), msample, -850, -200, b_pin="B")
mn = op(meteor, U.MaterialExpressionMin, op(meteor, U.MaterialExpressionMin, msample, msample, -1000, 200, "R", "G"), msample, -850, 200, b_pin="B")
sat = op(meteor, U.MaterialExpressionSubtract, mx, mn, -700, 0)
# Glass: bright and colorless. Keep = saturated or dark.
keep_sat = node(meteor, U.MaterialExpressionSmoothStep, -550, -100)
mel.connect_material_expressions(const(meteor, 0.06, -700, -300), "", keep_sat, "Min")
mel.connect_material_expressions(const(meteor, 0.14, -700, -250), "", keep_sat, "Max")
mel.connect_material_expressions(sat, "", keep_sat, "Value")
keep_dark = node(meteor, U.MaterialExpressionSmoothStep, -550, 150)
mel.connect_material_expressions(const(meteor, 0.72, -700, 300), "", keep_dark, "Min")
mel.connect_material_expressions(const(meteor, 0.58, -700, 350), "", keep_dark, "Max")
mel.connect_material_expressions(mx, "", keep_dark, "Value")
keep = op(meteor, U.MaterialExpressionMax, keep_sat, keep_dark, -400, 50)
mel.connect_material_property(keep, "", U.MaterialProperty.MP_OPACITY_MASK)
mel.connect_material_property(msample, "RGB", U.MaterialProperty.MP_BASE_COLOR)
# Glow where the texel is strongly saturated (the magenta cracks), a little everywhere else
crack = node(meteor, U.MaterialExpressionSmoothStep, -550, 450)
mel.connect_material_expressions(const(meteor, 0.45, -700, 500), "", crack, "Min")
mel.connect_material_expressions(const(meteor, 0.75, -700, 550), "", crack, "Max")
mel.connect_material_expressions(sat, "", crack, "Value")
meteor_glow = node(meteor, U.MaterialExpressionScalarParameter, -700, 650, parameter_name="Glow", default_value=2.0)
emis = op(meteor, U.MaterialExpressionMultiply, op(meteor, U.MaterialExpressionMultiply, msample, crack, -400, 400, a_pin="RGB"), meteor_glow, -250, 450)
mel.connect_material_property(emis, "", U.MaterialProperty.MP_EMISSIVE_COLOR)
mel.connect_material_property(const(meteor, 0.55), "", U.MaterialProperty.MP_ROUGHNESS)
mel.recompile_material(meteor)
eal.save_asset(meteor.get_path_name())
log("M_MRMeteor")

ui = U.FbxImportUI()
ui.automated_import_should_detect_type = False
ui.mesh_type_to_import = U.FBXImportType.FBXIT_STATIC_MESH
ui.import_mesh = True
ui.import_materials = False
ui.import_textures = False
ui.import_as_skeletal = False
ui.static_mesh_import_data.set_editor_property("combine_meshes", True)
ui.static_mesh_import_data.set_editor_property("auto_generate_collision", False)
for name, material in (("SM_MRShard", glass), ("SM_MROre", glass), ("SM_MRBig", candy), ("SM_MRMeteor", meteor)):
    if eal.does_asset_exist(f"{dest}/{name}"):
        eal.delete_asset(f"{dest}/{name}")
    mesh = next(a for a in import_task(f"{fbx_dir}/{name}.fbx", dest, name, ui) if isinstance(a, U.StaticMesh))
    for i in range(len(mesh.get_editor_property("static_materials"))):
        mesh.set_material(i, material)
    eal.save_asset(mesh.get_path_name())
    box = mesh.get_bounding_box()
    log(f"{name} -> {material.get_name()}, bounds {box.min} .. {box.max}")

dice = U.load_asset("/Game/Props/Dice/SM_Dice")
log("SM_Dice slots " + ", ".join(str(s.get_editor_property("material_slot_name")) for s in dice.get_editor_property("static_materials")))
log("DONE")
