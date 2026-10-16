"""Import island props made by blender_props.py (run inside Unreal, legacy FBX importer).

Usage: UnrealEditor-Cmd <project> -run=pythonscript -dpcvars=Interchange.FeatureFlags.Import.FBX=0
           "-script=ue_import_props.py <dest>|<name>=<fbx>=<tripo base color png>[=gloss]|..."
  e.g. /Game/Environment/Props|SM_PineTree=C:/.../SM_PineTree.fbx=C:/.../pine_basecolor.PNG
"gloss" makes it shiny like a balloon instead (Dice Land's candy props): "Tint" 1, roughness 0.4, specular 0.5;
"nanite" turns Nanite on (Dice Town's trees keep their full 20,000 triangles).

For each prop: <dest>/SM_X (no collision: props are scenery, they don't stop brawlers, shots or sight),
<dest>/Textures/T_X_BaseColor and <dest>/M_X: lit but flat like the other props for the toon post process (the
texture times "Tint", full roughness, no specular or metal), set up for instanced static meshes.
"""
import sys

import unreal

args = " ".join(sys.argv[1:]).split("|")
dest, entries = args[0], [a.split("=") for a in args[1:]]
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(msg):
    unreal.log(f"PROPS {msg}")


def import_task(path, folder, name, options=None):
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = folder
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    if options:
        task.options = options
    asset_tools.import_asset_tasks([task])
    return [unreal.load_asset(p) for p in task.imported_object_paths]


def node(mat, cls, x, y, **props):
    n = mel.create_material_expression(mat, cls, x, y)
    for key, value in props.items():
        n.set_editor_property(key, value)
    return n


for name, fbx, png, *flags in entries:
    gloss = "gloss" in flags
    tex_name = name.replace("SM_", "T_") + "_BaseColor"
    mat_name = name.replace("SM_", "M_")
    for asset in (f"{dest}/{name}", f"{dest}/Textures/{tex_name}", f"{dest}/{mat_name}"):
        if eal.does_asset_exist(asset):
            eal.delete_asset(asset)

    tex = next(a for a in import_task(png, f"{dest}/Textures", tex_name) if isinstance(a, unreal.Texture2D))
    tex.set_editor_property("srgb", True)

    mat = asset_tools.create_asset(mat_name, dest, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    try:
        mat.set_editor_property("used_with_nanite", True)
    except Exception:
        pass
    sample = node(mat, unreal.MaterialExpressionTextureSample, -600, 0, texture=tex)
    tint = node(mat, unreal.MaterialExpressionVectorParameter, -600, -200, parameter_name="Tint",
                default_value=unreal.LinearColor(*((1.0, 1.0, 1.0) if gloss else (0.8, 0.8, 0.8)), 1.0))
    mult = node(mat, unreal.MaterialExpressionMultiply, -300, -50)
    mel.connect_material_expressions(sample, "RGB", mult, "A")
    mel.connect_material_expressions(tint, "", mult, "B")
    mel.connect_material_property(mult, "", unreal.MaterialProperty.MP_BASE_COLOR)
    for prop, value in ((unreal.MaterialProperty.MP_ROUGHNESS, 0.4 if gloss else 1.0), (unreal.MaterialProperty.MP_SPECULAR, 0.5 if gloss else 0.0),
                        (unreal.MaterialProperty.MP_METALLIC, 0.0)):
        mel.connect_material_property(node(mat, unreal.MaterialExpressionConstant, -300, 200, r=value), "", prop)
    mel.recompile_material(mat)
    eal.save_asset(mat.get_path_name())

    ui = unreal.FbxImportUI()
    ui.automated_import_should_detect_type = False
    ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    ui.import_mesh = True
    ui.import_materials = False
    ui.import_textures = False
    ui.import_as_skeletal = False
    ui.static_mesh_import_data.set_editor_property("combine_meshes", True)
    ui.static_mesh_import_data.set_editor_property("auto_generate_collision", False)
    mesh = next(a for a in import_task(fbx, dest, name, ui) if isinstance(a, unreal.StaticMesh))
    materials = mesh.get_editor_property("static_materials")
    for i in range(len(materials)):
        mesh.set_material(i, mat)
    body = mesh.get_editor_property("body_setup")
    if body:
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
        body.remove_simple_collision() if hasattr(body, "remove_simple_collision") else None
    if "nanite" in flags:
        try:
            settings = mesh.get_editor_property("nanite_settings")
            settings.set_editor_property("enabled", True)
            mesh.set_editor_property("nanite_settings", settings)
        except Exception as e:
            log(f"{name}: nanite not set ({e})")
    eal.save_asset(mesh.get_path_name())
    box = mesh.get_bounding_box()
    log(f"{name}: {len(materials)} slot(s) -> {mat_name}, bounds {box.min} .. {box.max}")
log("DONE")
