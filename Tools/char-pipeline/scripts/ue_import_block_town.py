"""Import Dice Town's block meshes (blender_block_town.py) and make their materials (run inside Unreal, legacy FBX importer).

Usage: UnrealEditor-Cmd <project> -run=pythonscript -dpcvars=Interchange.FeatureFlags.Import.FBX=0
           "-script=ue_import_block_town.py <dir with the fbx files>"

Into /Game/Environment/DiceTown:
- M_BTBlock: base color from the instance's custom floats 0 to 2 (a linear color) on slot "Body", or 3 to 5 with
  "Accent" 1 (the grass top, the pips); matte ("Roughness" 0.6). Every face shows a faint 8 x 8 grid of lighter and
  darker "pixels" ("PixelSize" 12.5 units, "PixelStrength" 0.1), the look of MapleStory 2's block textures. "Glow" lights
  it from within by its own color (0). "Saturation" and "Brightness" (1 = as made) tune the map's colors live.
- MI_BTAccent: M_BTBlock reading the accent floats. MI_BTCloud: soft, glowing (clouds). MI_BTWater: glossy, a little glow.
- SM_BTBlock, SM_BTDieBlock: no collision (Dice Town puts hidden boxes where things stop), Nanite on, "Body" -> M_BTBlock,
  "Accent" -> MI_BTAccent.
"""
import sys

import unreal

src = " ".join(sys.argv[1:]).strip().replace("\\", "/")
dest = "/Game/Environment/DiceTown"
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
U = unreal

PIXEL_CODE = """float3 Cell = floor((Pos - Nrm * 2.0) / max(Size, 1.0));
float Hash = frac(sin(dot(Cell, float3(12.9898, 78.233, 37.719))) * 43758.5453);
return Color * (1.0 + (Hash - 0.5) * Strength);"""


def log(msg):
    unreal.log(f"BLOCKTOWN {msg}")


def fresh(name, cls, factory):
    path = f"{dest}/{name}"
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    return asset_tools.create_asset(name, dest, cls, factory)


def node(mat, cls, x, y, **props):
    n = mel.create_material_expression(mat, cls, x, y)
    for key, value in props.items():
        n.set_editor_property(key, value)
    return n


def scalar(mat, name, value, x, y):
    return node(mat, U.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=value)


block = fresh("M_BTBlock", U.Material, U.MaterialFactoryNew())
block.set_editor_property("used_with_instanced_static_meshes", True)
try:
    block.set_editor_property("used_with_nanite", True)
except Exception:
    pass
# The body's color or the accent's, by "Accent"
body = node(block, U.MaterialExpressionPerInstanceCustomData3Vector, -1800, 0, data_index=0,
            const_default_value=U.LinearColor(1.0, 1.0, 1.0, 1.0))
accent = node(block, U.MaterialExpressionPerInstanceCustomData3Vector, -1800, 200, data_index=3,
              const_default_value=U.LinearColor(1.0, 1.0, 1.0, 1.0))
pick = node(block, U.MaterialExpressionLinearInterpolate, -1500, 100)
mel.connect_material_expressions(body, "", pick, "A")
mel.connect_material_expressions(accent, "", pick, "B")
mel.connect_material_expressions(scalar(block, "Accent", 0.0, -1800, 400), "", pick, "Alpha")
# Saturation round its own gray, then brightness, never below black
gray = node(block, U.MaterialExpressionDesaturation, -1300, 300)
mel.connect_material_expressions(pick, "", gray, "")
mel.connect_material_expressions(node(block, U.MaterialExpressionConstant, -1500, 400, r=1.0), "", gray, "Fraction")
sat = node(block, U.MaterialExpressionLinearInterpolate, -1100, 100)
mel.connect_material_expressions(gray, "", sat, "A")
mel.connect_material_expressions(pick, "", sat, "B")
mel.connect_material_expressions(scalar(block, "Saturation", 1.0, -1300, 500), "", sat, "Alpha")
bright = node(block, U.MaterialExpressionMultiply, -950, 100)
mel.connect_material_expressions(sat, "", bright, "A")
mel.connect_material_expressions(scalar(block, "Brightness", 1.0, -1100, 500), "", bright, "B")
floor = node(block, U.MaterialExpressionMax, -800, 100)
mel.connect_material_expressions(bright, "", floor, "A")
mel.connect_material_expressions(node(block, U.MaterialExpressionConstant, -950, 300, r=0.0), "", floor, "B")
# The pixel grid on every face
custom = node(block, U.MaterialExpressionCustom, -550, 100, code=PIXEL_CODE,
              output_type=U.CustomMaterialOutputType.CMOT_FLOAT3, description="BlockPixels")
inputs = []
for name in ("Color", "Pos", "Nrm", "Size", "Strength"):
    entry = U.CustomInput()
    entry.set_editor_property("input_name", name)
    inputs.append(entry)
custom.set_editor_property("inputs", inputs)
mel.connect_material_expressions(floor, "", custom, "Color")
mel.connect_material_expressions(node(block, U.MaterialExpressionWorldPosition, -800, 300), "", custom, "Pos")
mel.connect_material_expressions(node(block, U.MaterialExpressionVertexNormalWS, -800, 400), "", custom, "Nrm")
mel.connect_material_expressions(scalar(block, "PixelSize", 12.5, -800, 500), "", custom, "Size")
mel.connect_material_expressions(scalar(block, "PixelStrength", 0.1, -800, 600), "", custom, "Strength")
mel.connect_material_property(custom, "", U.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(scalar(block, "Roughness", 0.6, -300, 300), "", U.MaterialProperty.MP_ROUGHNESS)
mel.connect_material_property(node(block, U.MaterialExpressionConstant, -300, 400, r=0.45), "", U.MaterialProperty.MP_SPECULAR)
lit = node(block, U.MaterialExpressionMultiply, -300, 500)
mel.connect_material_expressions(custom, "", lit, "A")
mel.connect_material_expressions(scalar(block, "Glow", 0.0, -550, 650), "", lit, "B")
mel.connect_material_property(lit, "", U.MaterialProperty.MP_EMISSIVE_COLOR)
mel.recompile_material(block)
eal.save_asset(block.get_path_name())


def instance(name, **scalars):
    mi = fresh(name, U.MaterialInstanceConstant, U.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, block)
    for key, value in scalars.items():
        mel.set_material_instance_scalar_parameter_value(mi, key, value)
    eal.save_asset(mi.get_path_name())
    return mi


accent_mi = instance("MI_BTAccent", Accent=1.0)
instance("MI_BTCloud", Roughness=0.9, Glow=0.25, PixelStrength=0.04)
instance("MI_BTWater", Roughness=0.08, Glow=0.12, PixelStrength=0.06)

for name in ("SM_BTBlock", "SM_BTDieBlock"):
    if eal.does_asset_exist(f"{dest}/{name}"):
        eal.delete_asset(f"{dest}/{name}")
    ui = U.FbxImportUI()
    ui.automated_import_should_detect_type = False
    ui.mesh_type_to_import = U.FBXImportType.FBXIT_STATIC_MESH
    ui.import_mesh = True
    ui.import_materials = False
    ui.import_textures = False
    ui.import_as_skeletal = False
    data = ui.static_mesh_import_data
    data.set_editor_property("combine_meshes", True)
    data.set_editor_property("auto_generate_collision", False)
    try:
        data.set_editor_property("build_nanite", True)
    except Exception:
        pass
    task = U.AssetImportTask()
    task.filename = f"{src}/{name}.fbx"
    task.destination_path = dest
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    task.options = ui
    asset_tools.import_asset_tasks([task])
    mesh = next(a for a in (U.load_asset(p) for p in task.imported_object_paths) if isinstance(a, U.StaticMesh))
    names = []
    for i, slot in enumerate(mesh.get_editor_property("static_materials")):
        slot_name = str(slot.get_editor_property("material_slot_name"))
        names.append(slot_name)
        mesh.set_material(i, accent_mi if "accent" in slot_name.lower() else block)
    try:
        settings = mesh.get_editor_property("nanite_settings")
        settings.set_editor_property("enabled", True)
        mesh.set_editor_property("nanite_settings", settings)
    except Exception as e:
        log(f"{name}: nanite not set ({e})")
    eal.save_asset(mesh.get_path_name())
    box = mesh.get_bounding_box()
    log(f"{name}: slots {names}, bounds {box.min} .. {box.max}")
log("DONE")
