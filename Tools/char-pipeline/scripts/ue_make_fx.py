"""Glowing effect particles for ARRRandomEffects (giant, wings, hits) (run inside Unreal, legacy FBX importer).

Usage: UnrealEditor-Cmd <project> -run=pythonscript -dpcvars=Interchange.FeatureFlags.Import.FBX=0
           "-script=ue_make_fx.py <dir with blender_dice_land.py's fbx files>"

Into /Game/Effects/DiceFX:
- M_FXGlow: unlit and translucent, for instanced meshes. Emissive = the instance's custom floats 0-2 (a linear color) times
  float 3 (brightness); opacity = float 4. Every particle is a tinted instance of one of the meshes below.
- M_FXGhost: the dashing brawlers' afterimages (skeletal pose copies): unlit, see-through, glowing "Color" brighter at the
  rim (Fresnel), "Opacity" set per frame as each one fades.
- SM_FXBall, SM_FXRing, SM_FXDie: the Dice Land ball, ring and die again, without Nanite (Nanite can't draw translucent
  materials), M_FXGlow on every slot, no collision.
"""
import sys

import unreal

src = " ".join(sys.argv[1:]).strip().replace("\\", "/")
dest = "/Game/Effects/DiceFX"
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
U = unreal


def log(msg):
    unreal.log(f"DICEFX {msg}")


def node(mat, cls, x, y, **props):
    n = mel.create_material_expression(mat, cls, x, y)
    for key, value in props.items():
        n.set_editor_property(key, value)
    return n


if eal.does_asset_exist(f"{dest}/M_FXGlow"):
    eal.delete_asset(f"{dest}/M_FXGlow")
glow = asset_tools.create_asset("M_FXGlow", dest, U.Material, U.MaterialFactoryNew())
glow.set_editor_property("blend_mode", U.BlendMode.BLEND_TRANSLUCENT)
glow.set_editor_property("shading_model", U.MaterialShadingModel.MSM_UNLIT)
glow.set_editor_property("used_with_instanced_static_meshes", True)
glow.set_editor_property("two_sided", True)
color = node(glow, U.MaterialExpressionPerInstanceCustomData3Vector, -700, 0, data_index=0,
             const_default_value=U.LinearColor(1, 1, 1, 1))
bright = node(glow, U.MaterialExpressionPerInstanceCustomData, -700, 200, data_index=3, const_default_value=1.0)
alpha = node(glow, U.MaterialExpressionPerInstanceCustomData, -700, 350, data_index=4, const_default_value=1.0)
lit = node(glow, U.MaterialExpressionMultiply, -400, 50)
mel.connect_material_expressions(color, "", lit, "A")
mel.connect_material_expressions(bright, "", lit, "B")
mel.connect_material_property(lit, "", U.MaterialProperty.MP_EMISSIVE_COLOR)
mel.connect_material_property(alpha, "", U.MaterialProperty.MP_OPACITY)
mel.recompile_material(glow)
eal.save_asset(glow.get_path_name())

if eal.does_asset_exist(f"{dest}/M_FXGhost"):
    eal.delete_asset(f"{dest}/M_FXGhost")
ghost = asset_tools.create_asset("M_FXGhost", dest, U.Material, U.MaterialFactoryNew())
ghost.set_editor_property("blend_mode", U.BlendMode.BLEND_TRANSLUCENT)
ghost.set_editor_property("shading_model", U.MaterialShadingModel.MSM_UNLIT)
ghost.set_editor_property("used_with_skeletal_mesh", True)
tint = node(ghost, U.MaterialExpressionVectorParameter, -900, 0, parameter_name="Color", default_value=U.LinearColor(1.0, 0.8, 0.25, 1.0))
fade = node(ghost, U.MaterialExpressionScalarParameter, -900, 300, parameter_name="Opacity", default_value=0.5)
glow_amount = node(ghost, U.MaterialExpressionScalarParameter, -900, 150, parameter_name="Glow", default_value=2.5)
fresnel = node(ghost, U.MaterialExpressionFresnel, -900, 450, exponent=2.5, base_reflect_fraction=0.0)
rim = node(ghost, U.MaterialExpressionAdd, -650, 450)
mel.connect_material_expressions(fresnel, "", rim, "A")
mel.connect_material_expressions(node(ghost, U.MaterialExpressionConstant, -800, 550, r=0.35), "", rim, "B")
lit = node(ghost, U.MaterialExpressionMultiply, -650, 50)
mel.connect_material_expressions(tint, "", lit, "A")
mel.connect_material_expressions(glow_amount, "", lit, "B")
lit_rim = node(ghost, U.MaterialExpressionMultiply, -400, 50)
mel.connect_material_expressions(lit, "", lit_rim, "A")
mel.connect_material_expressions(rim, "", lit_rim, "B")
mel.connect_material_property(lit_rim, "", U.MaterialProperty.MP_EMISSIVE_COLOR)
see = node(ghost, U.MaterialExpressionMultiply, -400, 300)
mel.connect_material_expressions(fade, "", see, "A")
mel.connect_material_expressions(rim, "", see, "B")
mel.connect_material_property(see, "", U.MaterialProperty.MP_OPACITY)
mel.recompile_material(ghost)
eal.save_asset(ghost.get_path_name())
log("M_FXGhost")

for source, name in (("SM_DLBall", "SM_FXBall"), ("SM_DLRing", "SM_FXRing"), ("SM_DLDie", "SM_FXDie")):
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
    task.filename = f"{src}/{source}.fbx"
    task.destination_path = dest
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    task.options = ui
    asset_tools.import_asset_tasks([task])
    mesh = next(a for a in (U.load_asset(p) for p in task.imported_object_paths) if isinstance(a, U.StaticMesh))
    for i in range(len(mesh.get_editor_property("static_materials"))):
        mesh.set_material(i, glow)
    eal.save_asset(mesh.get_path_name())
    log(f"{name} from {source}")
log("DONE")
