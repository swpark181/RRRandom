"""Import Dice Land's procedural meshes (blender_dice_land.py) and make their materials (run inside Unreal, legacy FBX importer).

Usage: UnrealEditor-Cmd <project> -run=pythonscript -dpcvars=Interchange.FeatureFlags.Import.FBX=0
           "-script=ue_import_dice_land.py <dir with the fbx files>"

Into /Game/Environment/DiceLand:
- M_DLCandy: base color from the instance's first three custom floats (a linear color; white without them), glossy like a
  balloon ("Roughness" 0.35, specular 0.5), lit from within by "Glow" times its color (0); MI_DLTile is it with "Roughness"
  0.6 for the cushion floor, MI_DLCloud with "Roughness" 0.85 and "Glow" 0.2 for soft bright clouds.
- M_DLRainbowDie: base color from the vertex colors (the rainbow die's faces), the same gloss.
- M_DLPip: the pips, shinier; their color from the instance's custom floats 3 to 5 (near white without them).
All four take "Saturation" and "Brightness" (1 = as made) for tuning the map's colors live.
- SM_DLDie, SM_DLDieBlank, SM_DLRainbowDie, SM_DLTile, SM_DLBall, SM_DLTrunk, SM_DLRing: no collision (Dice Land puts hidden boxes where things stop), Nanite on
  (hundreds of instances), slot "Body" -> candy (rainbow for the rainbow die, the tile instance for the tile), "Pips" -> pip.
"""
import sys

import unreal

src = " ".join(sys.argv[1:]).strip().replace("\\", "/")
dest = "/Game/Environment/DiceLand"
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
U = unreal


def log(msg):
    unreal.log(f"DICELAND {msg}")


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


def tone(mat, color_node, color_out):
    """The color pushed away from (or toward) its own gray by "Saturation" and scaled by "Brightness" (both 1: as made),
    so Dice Land can tune the whole map's colors live (ARRRandomDiceLand, rr.DiceLand.* console variables)."""
    gray = node(mat, U.MaterialExpressionDesaturation, -1000, 500)
    if not mel.connect_material_expressions(color_node, color_out, gray, ""):
        mel.connect_material_expressions(color_node, color_out, gray, "Input")
    mel.connect_material_expressions(node(mat, U.MaterialExpressionConstant, -1200, 600, r=1.0), "", gray, "Fraction")
    amount = node(mat, U.MaterialExpressionScalarParameter, -1000, 650, parameter_name="Saturation", default_value=1.0)
    mix = node(mat, U.MaterialExpressionLinearInterpolate, -800, 500)
    mel.connect_material_expressions(gray, "", mix, "A")
    mel.connect_material_expressions(color_node, color_out, mix, "B")
    mel.connect_material_expressions(amount, "", mix, "Alpha")
    bright = node(mat, U.MaterialExpressionScalarParameter, -800, 650, parameter_name="Brightness", default_value=1.0)
    scaled = node(mat, U.MaterialExpressionMultiply, -650, 500)
    mel.connect_material_expressions(mix, "", scaled, "A")
    mel.connect_material_expressions(bright, "", scaled, "B")
    floor = node(mat, U.MaterialExpressionMax, -550, 500)
    mel.connect_material_expressions(scaled, "", floor, "A")
    mel.connect_material_expressions(node(mat, U.MaterialExpressionConstant, -650, 650, r=0.0), "", floor, "B")
    return floor


def glossy(mat, color_node, color_out, roughness, specular, glow=False):
    color_node, color_out = tone(mat, color_node, color_out), ""
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    try:
        mat.set_editor_property("used_with_nanite", True)
    except Exception:
        pass
    mel.connect_material_property(color_node, color_out, U.MaterialProperty.MP_BASE_COLOR)
    rough = node(mat, U.MaterialExpressionScalarParameter, -400, 200, parameter_name="Roughness", default_value=roughness)
    mel.connect_material_property(rough, "", U.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(node(mat, U.MaterialExpressionConstant, -400, 320, r=specular), "", U.MaterialProperty.MP_SPECULAR)
    if glow:
        amount = node(mat, U.MaterialExpressionScalarParameter, -400, 420, parameter_name="Glow", default_value=0.0)
        lit = node(mat, U.MaterialExpressionMultiply, -200, 420)
        mel.connect_material_expressions(color_node, color_out, lit, "A")
        mel.connect_material_expressions(amount, "", lit, "B")
        mel.connect_material_property(lit, "", U.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(mat)
    eal.save_asset(mat.get_path_name())


candy = fresh("M_DLCandy", U.Material, U.MaterialFactoryNew())
data = node(candy, U.MaterialExpressionPerInstanceCustomData3Vector, -700, 0, data_index=0,
            const_default_value=U.LinearColor(1.0, 1.0, 1.0, 1.0))
tint = node(candy, U.MaterialExpressionVectorParameter, -700, -200, parameter_name="Tint", default_value=U.LinearColor(1, 1, 1, 1))
mult = node(candy, U.MaterialExpressionMultiply, -400, -50)
mel.connect_material_expressions(data, "", mult, "A")
mel.connect_material_expressions(tint, "", mult, "B")
glossy(candy, mult, "", 0.35, 0.5, glow=True)

tile = fresh("MI_DLTile", U.MaterialInstanceConstant, U.MaterialInstanceConstantFactoryNew())
mel.set_material_instance_parent(tile, candy)
mel.set_material_instance_scalar_parameter_value(tile, "Roughness", 0.6)
eal.save_asset(tile.get_path_name())

cloud = fresh("MI_DLCloud", U.MaterialInstanceConstant, U.MaterialInstanceConstantFactoryNew())
mel.set_material_instance_parent(cloud, candy)
mel.set_material_instance_scalar_parameter_value(cloud, "Roughness", 0.85)
mel.set_material_instance_scalar_parameter_value(cloud, "Glow", 0.2)
eal.save_asset(cloud.get_path_name())

rainbow = fresh("M_DLRainbowDie", U.Material, U.MaterialFactoryNew())
glossy(rainbow, node(rainbow, U.MaterialExpressionVertexColor, -700, 0), "", 0.3, 0.5)

pip = fresh("M_DLPip", U.Material, U.MaterialFactoryNew())
glossy(pip, node(pip, U.MaterialExpressionPerInstanceCustomData3Vector, -700, 0, data_index=3,
                 const_default_value=U.LinearColor(0.92, 0.92, 0.95, 1)), "", 0.25, 0.6)

slots = {"SM_DLDie": {"Body": candy, "Pips": pip}, "SM_DLDieBlank": {"Body": candy}, "SM_DLRainbowDie": {"Body": rainbow, "Pips": pip},
         "SM_DLTile": {"Body": tile}, "SM_DLBall": {"Body": candy}, "SM_DLTrunk": {"Body": candy}, "SM_DLRing": {"Body": candy}}
for name, by_slot in slots.items():
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
    data.set_editor_property("vertex_color_import_option", U.VertexColorImportOption.REPLACE)
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
    materials = mesh.get_editor_property("static_materials")
    names = []
    for i, slot in enumerate(materials):
        slot_name = str(slot.get_editor_property("material_slot_name"))
        names.append(slot_name)
        mat = next((m for key, m in by_slot.items() if key.lower() in slot_name.lower()), by_slot.get("Body"))
        mesh.set_material(i, mat)
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
