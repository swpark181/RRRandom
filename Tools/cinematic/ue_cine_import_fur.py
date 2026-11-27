# Fox vs tiger cinematic (2026-11-24, the user: the fur's edges in the close-ups): brings in a denser shell fur for
# the cinematic only. The game's fur has 12 layers over 3.6 cm - seen edge on from a face's distance the layers show
# as rows of dashes and saw teeth. The same fur with 36 layers (char-pipeline/scripts/blender_fur_shells_skel.py with
# the animal friend's own options and --layers 36) goes on ARRRandomCineAnimal.FurMesh; the game's SK_<Name>Fur, its
# material (M_<Name>Fur, and the expressions' fur copies) and its strand texture are used as they are.
#   ue_py_import.sh cinefur "CINEFUR DONE" "<this> Fox|<fbx>;Tiger|<fbx>"     (legacy FBX importer)
# Makes /Game/Cinematics/FoxVsTiger/Fur/SK_<Name>FurCine on SK_<Name>_Skeleton.
import sys

import unreal

U = unreal
DEST = "/Game/Cinematics/FoxVsTiger/Fur"
eal = U.EditorAssetLibrary
tools = U.AssetToolsHelpers.get_asset_tools()
for spec in " ".join(sys.argv[1:]).split(";"):
    name, fbx = spec.split("|")
    skeleton = U.load_asset(f"/Game/Characters/{name}/SK_{name}_Skeleton")
    game_fur = U.load_asset(f"/Game/Characters/{name}/SK_{name}Fur")
    ui = U.FbxImportUI()
    ui.automated_import_should_detect_type = False
    ui.import_as_skeletal = True
    ui.import_mesh = True
    ui.import_animations = False
    ui.import_materials = False
    ui.import_textures = False
    ui.create_physics_asset = False
    ui.mesh_type_to_import = U.FBXImportType.FBXIT_SKELETAL_MESH
    ui.skeleton = skeleton
    ui.skeletal_mesh_import_data.set_editor_property("vertex_color_import_option", U.VertexColorImportOption.REPLACE)
    ui.skeletal_mesh_import_data.set_editor_property("normal_import_method", U.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    task = U.AssetImportTask()
    task.filename = fbx
    task.destination_path = DEST
    task.destination_name = f"SK_{name}FurCine"
    task.automated = True
    task.replace_existing = True
    task.save = True
    task.options = ui
    tools.import_asset_tasks([task])
    mesh = U.load_asset(f"{DEST}/SK_{name}FurCine")
    if not mesh or mesh.get_editor_property("skeleton") != skeleton:
        raise RuntimeError(f"{name}: the fur mesh didn't land on the skeleton")
    # the game fur's materials, slot by slot (the tiger's fur has a "Muzzle" slot besides its body's)
    by_slot = {str(m.get_editor_property("material_slot_name")): m.get_editor_property("material_interface")
               for m in game_fur.get_editor_property("materials")}
    first = next(iter(by_slot.values()))
    mats = [U.SkeletalMaterial(material_interface=by_slot.get(str(s.get_editor_property("material_slot_name")), first),
                               material_slot_name=s.get_editor_property("material_slot_name"))
            for s in mesh.get_editor_property("materials")]
    mesh.set_editor_property("materials", mats)
    eal.save_asset(mesh.get_path_name())
    U.log_warning(f"CINEFUR {name}: {mesh.get_path_name()}, slots {[str(m.material_slot_name) for m in mats]} "
                  f"(the game's: {list(by_slot)}), bounds {mesh.get_bounds().box_extent}")
U.log_warning("CINEFUR DONE")
