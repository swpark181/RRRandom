# Cinematic (2026-11-19, fifth pass): the face masks (char-pipeline/scripts/blender_face_flatten.py) into Unreal.
# In /Game/Cinematics/FoxVsTiger/Faces, for the fox and the tiger:
# - SK_<Species>_Cine: the game body with the smooth face mask joined to it (material slot "Face"; what stood out of the
#   face pushed behind it), on the game's skeleton; SK_<Species>Fur_Cine: the game's fur thinned away over the mask.
# - T_<Species>Face_<Expr> (the whole face as a picture, the mask's UVs = the front view) and M_<Species>Face_<Expr>: a copy
#   of the species' body material with that picture instead of its base colour, masked, the mask's edge (vertex colour R)
#   dithered so it melts into the body.
# Run with UnrealEditor-Cmd -run=pythonscript -dpcvars=Interchange.FeatureFlags.Import.FBX=0 (the legacy FBX importer).
import os

import unreal

U = unreal
SRC = "C:/work/char-pipeline/input/expressions/faces"
DEST = "/Game/Cinematics/FoxVsTiger/Faces"
EXPRS = {"Fox": ["Scared", "Worried", "Determined", "Grin", "WorriedBlink", "GrinBlink"],
         "Tiger": ["Smug", "Startled", "Nervous"]}
eal = U.EditorAssetLibrary
mel = U.MaterialEditingLibrary
tools = U.AssetToolsHelpers.get_asset_tools()


def log(msg):
    U.log(f"CINEFACE {msg}")


def import_mesh(fbx, name, skeleton):
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
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    task.options = ui
    tools.import_asset_tasks([task])
    mesh = U.load_asset(f"{DEST}/{name}")
    if not mesh or mesh.get_editor_property("skeleton") != skeleton:
        raise RuntimeError(f"{name} didn't land on {skeleton.get_path_name()}")
    return mesh


def set_slots(mesh, pick):
    mats = []
    for s in mesh.get_editor_property("materials"):
        slot = str(s.get_editor_property("material_slot_name"))
        s.set_editor_property("material_interface", pick(slot))
        mats.append(s)
    mesh.set_editor_property("materials", mats)
    eal.save_asset(mesh.get_path_name())
    return [str(s.get_editor_property("material_slot_name")) for s in mats]


def connect_first(src, out, dst, names):
    for n in names:
        if mel.connect_material_expressions(src, out, dst, n):
            return n
    return None


for species, exprs in EXPRS.items():
    game = f"/Game/Characters/{species}"
    skeleton = U.load_asset(f"{game}/SK_{species}_Skeleton")
    body_mat = U.load_asset(f"{game}/M_{species}")
    fur_game = U.load_asset(f"{game}/SK_{species}Fur")
    fur_mat = fur_game.get_editor_property("materials")[0].get_editor_property("material_interface")

    body = import_mesh(f"{SRC}/SK_{species}_Cine.fbx", f"SK_{species}_Cine", skeleton)
    slots = set_slots(body, lambda slot: body_mat)
    fur = import_mesh(f"{SRC}/SK_{species}Fur_Cine.fbx", f"SK_{species}Fur_Cine", skeleton)
    set_slots(fur, lambda slot: fur_mat)
    log(f"{species}: body slots {slots}, bounds {body.get_bounds().box_extent}; fur material {fur_mat.get_name()}")

    base = U.load_asset(f"{game}/Textures/T_{species}_BaseColor")
    for e in exprs:
        tname = f"T_{species}Face_{e}"
        task = U.AssetImportTask()
        task.filename = f"{SRC}/{tname}.png"
        task.destination_path = DEST
        task.destination_name = tname
        task.automated = True
        task.replace_existing = True
        task.save = True
        tools.import_asset_tasks([task])
        tex = U.load_asset(f"{DEST}/{tname}")
        tex.set_editor_property("mip_gen_settings", U.TextureMipGenSettings.TMGS_FROM_TEXTURE_GROUP)
        mname = f"M_{species}Face_{e}"
        if eal.does_asset_exist(f"{DEST}/{mname}"):
            eal.delete_asset(f"{DEST}/{mname}")
        eal.duplicate_asset(body_mat.get_path_name(), f"{DEST}/{mname}")
        mat = U.load_asset(f"{DEST}/{mname}")
        n = U.RRRandomCineLibrary.replace_material_texture(mat, base, tex)
        mat.set_editor_property("blend_mode", U.BlendMode.BLEND_MASKED)
        mat.set_editor_property("opacity_mask_clip_value", 0.5)
        vc = mel.create_material_expression(mat, U.MaterialExpressionVertexColor, -900, 900)
        # (DitherTemporalAA is an engine material function, not a node of its own)
        dither = mel.create_material_expression(mat, U.MaterialExpressionMaterialFunctionCall, -600, 900)
        dither.set_editor_property("material_function", U.load_asset(
            "/Engine/Functions/Engine_MaterialFunctions02/Utility/DitherTemporalAA"))
        pin = connect_first(vc, "R", dither, ["Alpha Threshold", "AlphaThreshold", ""])
        ok = mel.connect_material_property(dither, "", U.MaterialProperty.MP_OPACITY_MASK)
        if not pin or not ok:
            # (no dither: the edge as a plain mask)
            mel.connect_material_property(vc, "R", U.MaterialProperty.MP_OPACITY_MASK)
        mel.recompile_material(mat)
        eal.save_asset(f"{DEST}/{mname}")
        log(f"{mname}: {n} texture sample(s) -> {tname}, dither pin {pin!r}, opacity {ok}")
log("DONE")
