"""Import Dice Town's extra shapes (blender_dream_shapes.py) next to its blocks (run inside Unreal, legacy FBX importer).

Usage: UnrealEditor-Cmd <project> -run=pythonscript -dpcvars=Interchange.FeatureFlags.Import.FBX=0
           "-script=ue_import_dream_shapes.py <dir with the fbx files> [names, default the three below]"

Into /Game/Environment/DiceTown, using ue_import_block_town.py's materials (which must be there): SM_BTPuff, SM_BTStar,
SM_BTMoon (and SM_BTRoof from blender_dream_roof.py, by name), no collision, Nanite on, "Body" -> M_BTBlock, "Accent" ->
MI_BTAccent.
"""
import sys

import unreal

src = sys.argv[1].replace("\\", "/")
dest = "/Game/Environment/DiceTown"
eal = unreal.EditorAssetLibrary
U = unreal

block = U.load_asset(f"{dest}/M_BTBlock")
accent_mi = U.load_asset(f"{dest}/MI_BTAccent")
if not block or not accent_mi:
    raise RuntimeError("run ue_import_block_town.py first")

names = [n for n in " ".join(sys.argv[1:]).split()[1:]] or ["SM_BTPuff", "SM_BTStar", "SM_BTMoon"]
for name in names:
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
    U.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
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
        unreal.log(f"DREAMSHAPES {name}: nanite not set ({e})")
    eal.save_asset(mesh.get_path_name())
    box = mesh.get_bounding_box()
    unreal.log(f"DREAMSHAPES {name}: slots {names}, bounds {box.min} .. {box.max}")
unreal.log("DREAMSHAPES DONE")
