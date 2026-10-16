"""Import the soil blocks made by blender_soil_blocks.py next to the grass blocks (run inside Unreal, legacy FBX importer).

Usage: UnrealEditor-Cmd <project> -run=pythonscript -dpcvars=Interchange.FeatureFlags.Import.FBX=0
           "-script=ue_import_soil_blocks.py /Game/Environment/Grass|SM_SoilBlock_A=<fbx>=M_GrassBlock_A|..."

Each becomes <dest>/SM_SoilBlock_X with no collision (the island's hidden boxes are what stand in the way), wearing the
matching grass block's material (same texture, same UVs).
"""
import sys

import unreal

args = " ".join(sys.argv[1:]).split("|")
dest, entries = args[0], [a.split("=") for a in args[1:]]
eal = unreal.EditorAssetLibrary

for name, fbx, material_name in entries:
    if eal.does_asset_exist(f"{dest}/{name}"):
        eal.delete_asset(f"{dest}/{name}")
    ui = unreal.FbxImportUI()
    ui.automated_import_should_detect_type = False
    ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    ui.import_mesh = True
    ui.import_materials = False
    ui.import_textures = False
    ui.static_mesh_import_data.set_editor_property("combine_meshes", True)
    ui.static_mesh_import_data.set_editor_property("auto_generate_collision", False)
    task = unreal.AssetImportTask()
    task.filename = fbx
    task.destination_path = dest
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    task.options = ui
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh = next(a for a in (unreal.load_asset(p) for p in task.imported_object_paths) if isinstance(a, unreal.StaticMesh))
    material = unreal.load_asset(f"{dest}/{material_name}")
    for i in range(len(mesh.get_editor_property("static_materials"))):
        mesh.set_material(i, material)
    eal.save_asset(mesh.get_path_name())
    box = mesh.get_bounding_box()
    unreal.log(f"SOILBLOCKS {name}: {material_name}, bounds z {box.min.z:.1f}..{box.max.z:.1f}, x {box.min.x:.1f}..{box.max.x:.1f}")
unreal.log("SOILBLOCKS DONE")
