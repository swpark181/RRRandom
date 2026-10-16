"""Import an FBX into /Game/_Validate to confirm Unreal's FBX reader accepts it (run inside Unreal).

Usage: UnrealEditor-Cmd <project> -run=pythonscript -script="ue_validate_fbx.py <fbx path>"
"""
import sys

import unreal

src = sys.argv[-1]
task = unreal.AssetImportTask()
task.filename = src
task.destination_path = "/Game/_Validate"
task.automated = True
task.replace_existing = True
task.save = False
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

paths = list(task.imported_object_paths)
unreal.log(f"VALIDATE imported: {paths}")
for p in paths:
    a = unreal.load_asset(p)
    if isinstance(a, unreal.StaticMesh):
        unreal.log(f"VALIDATE StaticMesh tris={a.get_num_triangles(0)} bounds={a.get_bounding_box()}")
    else:
        unreal.log(f"VALIDATE {type(a).__name__}")
unreal.EditorAssetLibrary.delete_directory("/Game/_Validate")
