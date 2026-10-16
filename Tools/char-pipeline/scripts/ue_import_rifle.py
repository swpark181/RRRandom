"""Import the rifle made by blender_rifle.py as a static mesh (run inside Unreal).

Usage: UnrealEditor-Cmd <project> -run=pythonscript -dpcvars=Interchange.FeatureFlags.Import.FBX=0
           "-script=ue_import_rifle.py <dest>|<rifle fbx>|<tripo base color png>"
  e.g. /Game/Props/Rifle|C:/work/char-pipeline/output/rifle/SM_Rifle.fbx|C:/.../futuristic_gun_3d_model_basecolor.PNG

Makes <dest>/SM_Rifle (origin in the pistol grip, muzzle +X, 100 long, no collision: it's carried),
Textures/T_Rifle_BaseColor and M_Rifle, flat like ue_make_toon.py makes M_Fox (base color texture only:
no normal map, no metal, full roughness, no specular) so the toon post process cuts it into the same cel bands.
"""
import sys

import unreal

dest, fbx, base_png = " ".join(sys.argv[1:]).split("|")
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(msg):
    unreal.log(f"RIFLE {msg}")


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
mesh_task = unreal.AssetImportTask()
mesh_task.filename = fbx
mesh_task.destination_path = dest
mesh_task.destination_name = "SM_Rifle"
mesh_task.automated = True
mesh_task.replace_existing = True
mesh_task.save = False
mesh_task.options = ui

tex_task = unreal.AssetImportTask()
tex_task.filename = base_png
tex_task.destination_path = dest + "/Textures"
tex_task.destination_name = "T_Rifle_BaseColor"
tex_task.automated = True
tex_task.replace_existing = True
tex_task.save = False
asset_tools.import_asset_tasks([mesh_task, tex_task])

mesh = unreal.load_asset(f"{dest}/SM_Rifle")
tex = unreal.load_asset(f"{dest}/Textures/T_Rifle_BaseColor")
if mesh is None or tex is None:
    raise RuntimeError(f"import failed: mesh={mesh} texture={tex}")

mat = asset_tools.create_asset("M_Rifle", dest, unreal.Material, unreal.MaterialFactoryNew())
sample = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -500, -100)
sample.set_editor_property("texture", tex)
mel.connect_material_property(sample, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
for i, (prop, value) in enumerate(((unreal.MaterialProperty.MP_ROUGHNESS, 1.0),
                                   (unreal.MaterialProperty.MP_SPECULAR, 0.0),
                                   (unreal.MaterialProperty.MP_METALLIC, 0.0))):
    c = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -250, 100 + i * 100)
    c.set_editor_property("r", value)
    mel.connect_material_property(c, "", prop)
mel.recompile_material(mat)
mesh.set_material(0, mat)
eal.save_directory(dest)

b = mesh.get_bounds()
log(f"triangles {mesh.get_num_triangles(0)}, slot -> {mesh.get_material(0).get_name()}, "
    f"origin ({b.origin.x:.1f}, {b.origin.y:.1f}, {b.origin.z:.1f}) extent ({b.box_extent.x:.1f}, {b.box_extent.y:.1f}, {b.box_extent.z:.1f})")
log("DONE")
