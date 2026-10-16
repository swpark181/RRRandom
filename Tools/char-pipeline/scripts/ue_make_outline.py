"""Build an inverted-hull outline material for skeletal meshes (run inside Unreal).

Usage: UnrealEditor-Cmd <project> -run=pythonscript "-script=ue_make_outline.py <asset path>"
  e.g. /Game/Characters/M_CharacterOutline

Meant as a mesh's overlay material: the mesh is drawn again pushed out along its normals by "OutlineWidth"
(world units) with only the back faces kept, so a rim in "OutlineColor" shows around the silhouette.
Unlit so the rim keeps its exact color under any light.
"""
import sys

import unreal

asset_path = sys.argv[-1]
folder, name = asset_path.rsplit("/", 1)
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary

if eal.does_asset_exist(asset_path):
    eal.delete_asset(asset_path)
mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property("two_sided", True)
mat.set_editor_property("used_with_skeletal_mesh", True)


def node(cls, x, y):
    return mel.create_material_expression(mat, cls, x, y)


color = node(unreal.MaterialExpressionVectorParameter, -500, -200)
color.set_editor_property("parameter_name", "OutlineColor")
color.set_editor_property("default_value", unreal.LinearColor(0.02, 0.02, 0.02, 1))
mel.connect_material_property(color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

# Front faces get 0 and are clipped; back faces get 1 and stay
sign = node(unreal.MaterialExpressionTwoSidedSign, -700, 50)
flip = node(unreal.MaterialExpressionOneMinus, -500, 50)
half = node(unreal.MaterialExpressionMultiply, -300, 50)
half.set_editor_property("const_b", 0.5)
mel.connect_material_expressions(sign, "", flip, "")
mel.connect_material_expressions(flip, "", half, "A")
mel.connect_material_property(half, "", unreal.MaterialProperty.MP_OPACITY_MASK)

normal = node(unreal.MaterialExpressionVertexNormalWS, -700, 250)
width = node(unreal.MaterialExpressionScalarParameter, -700, 350)
width.set_editor_property("parameter_name", "OutlineWidth")
width.set_editor_property("default_value", 4.0)
push = node(unreal.MaterialExpressionMultiply, -400, 300)
mel.connect_material_expressions(normal, "", push, "A")
mel.connect_material_expressions(width, "", push, "B")
mel.connect_material_property(push, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)

mel.recompile_material(mat)
eal.save_asset(asset_path)
unreal.log(f"OUTLINE made {asset_path}")
