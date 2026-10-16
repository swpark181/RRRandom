"""The squirrel's acorns into Unreal (run inside Unreal, legacy FBX importer).

Usage: UnrealEditor-Cmd <project> -run=pythonscript -dpcvars=Interchange.FeatureFlags.Import.FBX=0
           "-script=ue_import_acorn.py <dir with blender_acorn.py's fbx files>"

/Game/Effects/Acorn/SM_AcornNut and SM_AcornCap, no collision, every slot M_DLCandy (Dice Land's candy material: base color
from the instance's first three custom floats), for ARRRandomAcornRain's instanced acorns.
"""
import sys

import unreal

U = unreal
src = " ".join(sys.argv[1:]).strip().replace("\\", "/")
dest = "/Game/Effects/Acorn"
eal = U.EditorAssetLibrary
candy = U.load_asset("/Game/Environment/DiceLand/M_DLCandy")
for name in ("SM_AcornNut", "SM_AcornCap"):
    if eal.does_asset_exist(f"{dest}/{name}"):
        eal.delete_asset(f"{dest}/{name}")
    ui = U.FbxImportUI()
    ui.automated_import_should_detect_type = False
    ui.mesh_type_to_import = U.FBXImportType.FBXIT_STATIC_MESH
    ui.import_mesh = True
    ui.import_materials = False
    ui.import_textures = False
    ui.import_as_skeletal = False
    ui.static_mesh_import_data.set_editor_property("combine_meshes", True)
    ui.static_mesh_import_data.set_editor_property("auto_generate_collision", False)
    task = U.AssetImportTask()
    task.filename = f"{src}/{name}.fbx"
    task.destination_path = dest
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    task.options = ui
    U.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh = U.load_asset(f"{dest}/{name}")
    for i in range(len(mesh.get_editor_property("static_materials"))):
        mesh.set_material(i, candy)
    eal.save_asset(mesh.get_path_name())
    unreal.log(f"ACORN {name} bounds {mesh.get_bounds().box_extent}")
unreal.log("ACORN DONE")
