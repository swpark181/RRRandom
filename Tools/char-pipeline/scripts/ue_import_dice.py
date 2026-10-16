"""Import the dice made by blender_dice_pips.py as a static mesh (run inside Unreal).

Usage: UnrealEditor-Cmd <project> -run=pythonscript -dpcvars=Interchange.FeatureFlags.Import.FBX=0
           "-script=ue_import_dice.py <dest>|<dice fbx>|<tripo base color png>"
  e.g. /Game/Props/Dice|C:/work/char-pipeline/output/dice/SM_Dice.fbx|C:/.../colorful_cube_3d_model_basecolor.PNG

Makes <dest>/SM_Dice (pivot at the center, no collision: it's meant to float over a character's head),
Textures/T_Dice_BaseColor, and two materials in the flat style ue_make_toon.py gives M_Fox (no normal map,
no metal, full roughness, no specular) so the toon post process cuts them into the same cel bands:
- M_Dice: the Tripo base color texture.
- M_DicePip: "PipColor" parameter, dark navy. Kept above 0.01 luminance: the toon post process passes darker
  pixels through as unlit.
"""
import sys

import unreal

dest, fbx, base_png = " ".join(sys.argv[1:]).split("|")
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(msg):
    unreal.log(f"DICE {msg}")


# Start clean so a rerun doesn't leave assets from an earlier attempt behind
if eal.does_directory_exist(dest):
    eal.delete_directory(dest)

ui = unreal.FbxImportUI()
ui.automated_import_should_detect_type = False
ui.import_mesh = True
ui.import_as_skeletal = False
ui.import_animations = False
ui.import_materials = False
ui.import_textures = False
ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
ui.static_mesh_import_data.set_editor_property("combine_meshes", True)
ui.static_mesh_import_data.set_editor_property("auto_generate_collision", False)
task = unreal.AssetImportTask()
task.filename = fbx
task.destination_path = dest
task.destination_name = "SM_Dice"
task.automated = True
task.replace_existing = True
task.save = False
task.options = ui

tex_task = unreal.AssetImportTask()
tex_task.filename = base_png
tex_task.destination_path = dest + "/Textures"
tex_task.destination_name = "T_Dice_BaseColor"
tex_task.automated = True
tex_task.replace_existing = True
tex_task.save = False
asset_tools.import_asset_tasks([task, tex_task])

mesh = unreal.load_asset(f"{dest}/SM_Dice")
tex = unreal.load_asset(f"{dest}/Textures/T_Dice_BaseColor")
if mesh is None or tex is None:
    raise RuntimeError(f"import failed: mesh={mesh} texture={tex}")


def flat_material(name, color_node):
    """New lit material with base color from color_node(mat) and the flat toon inputs."""
    mat = asset_tools.create_asset(name, dest, unreal.Material, unreal.MaterialFactoryNew())
    base = color_node(mat)
    mel.connect_material_property(base, "RGB" if isinstance(base, unreal.MaterialExpressionTextureSample) else "",
                                  unreal.MaterialProperty.MP_BASE_COLOR)
    for i, (prop, value) in enumerate(((unreal.MaterialProperty.MP_ROUGHNESS, 1.0),
                                       (unreal.MaterialProperty.MP_SPECULAR, 0.0),
                                       (unreal.MaterialProperty.MP_METALLIC, 0.0))):
        c = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -250, 100 + i * 100)
        c.set_editor_property("r", value)
        mel.connect_material_property(c, "", prop)
    mel.recompile_material(mat)
    return mat


def texture_node(mat):
    s = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -500, -100)
    s.set_editor_property("texture", tex)
    return s


def pip_color_node(mat):
    p = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -500, -100)
    p.set_editor_property("parameter_name", "PipColor")
    p.set_editor_property("default_value", unreal.LinearColor(0.04, 0.045, 0.07, 1))
    return p


materials = {"Dice": flat_material("M_Dice", texture_node), "Pip": flat_material("M_DicePip", pip_color_node)}

# The slot structs come back as copies, so set each slot by index
for i, slot in enumerate(mesh.get_editor_property("static_materials")):
    name = str(slot.get_editor_property("material_slot_name"))
    if name not in materials:
        raise RuntimeError(f"unexpected material slot {name}")
    mesh.set_material(i, materials[name])
slots = mesh.get_editor_property("static_materials")
if any(s.get_editor_property("material_interface") != materials[str(s.get_editor_property("material_slot_name"))]
       for s in slots):
    raise RuntimeError("material slots not set")
log("slots " + ", ".join(f"{s.get_editor_property('material_slot_name')} -> "
                         f"{s.get_editor_property('material_interface').get_name()}" for s in slots))

eal.save_directory(dest)

bounds = mesh.get_bounds()
log(f"bounds origin {bounds.origin} extent {bounds.box_extent}")
log(f"triangles {mesh.get_num_triangles(0)} lods {mesh.get_num_lods()}")
log("assets: " + ", ".join(sorted(a.split(".")[0].replace(dest + "/", "") for a in eal.list_assets(dest, recursive=True))))
log("DONE")
