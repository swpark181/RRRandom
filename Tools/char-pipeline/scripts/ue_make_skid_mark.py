"""Tyre mark decal material for drifting vehicles (run inside Unreal).

Usage: UnrealEditor-Cmd <project> -run=pythonscript "-script=ue_make_skid_mark.py /Game/Effects/Skid/M_SkidMark"

A deferred decal (ARRRandomSkidMarks lays one per short strip and fades it with "Opacity"): near-black rubber, fully
rough, its edges across the strip softened so the strips read as worn tyre lines rather than hard boxes. It changes
the ground's base color before lighting, so the toon post process (which works from the base color) keeps it.
"""
import sys

import unreal

path = " ".join(sys.argv[1:]).strip()
folder, name = path.rsplit("/", 1)
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
U = unreal

if eal.does_asset_exist(path):
    eal.delete_asset(path)
mat = U.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, U.Material, U.MaterialFactoryNew())
mat.set_editor_property("material_domain", U.MaterialDomain.MD_DEFERRED_DECAL)
mat.set_editor_property("blend_mode", U.BlendMode.BLEND_TRANSLUCENT)


def node(cls, x, y, **props):
    n = mel.create_material_expression(mat, cls, x, y)
    for key, value in props.items():
        n.set_editor_property(key, value)
    return n


def link(a, a_out, b, b_in):
    if not mel.connect_material_expressions(a, a_out, b, b_in):
        raise RuntimeError(f"could not connect into {b_in}")


uv = node(U.MaterialExpressionTextureCoordinate, -800, 200)
opacity = node(U.MaterialExpressionScalarParameter, -800, 350, parameter_name="Opacity", default_value=1.0)
color = node(U.MaterialExpressionVectorParameter, -800, 0, parameter_name="MarkColor",
             default_value=U.LinearColor(0.025, 0.022, 0.02, 1.0))
custom = node(U.MaterialExpressionCustom, -450, 250, output_type=U.CustomMaterialOutputType.CMOT_FLOAT1,
              description="Skid opacity",
              code="// Soft across the strip (the decal's width runs along V), a touch of grain along it\n"
                   "float Across = 1.0 - smoothstep(0.25, 0.5, abs(UV.y - 0.5));\n"
                   "float Grain = 0.85 + 0.15 * frac(sin(dot(floor(UV * float2(3.0, 6.0)), float2(12.9898, 78.233))) * 43758.5453);\n"
                   "return saturate(Opacity * Across * Grain);")
inputs = []
for input_name in ("UV", "Opacity"):
    custom_input = U.CustomInput()
    custom_input.set_editor_property("input_name", input_name)
    inputs.append(custom_input)
custom.set_editor_property("inputs", inputs)
link(uv, "", custom, "UV")
link(opacity, "", custom, "Opacity")

rgb = node(U.MaterialExpressionComponentMask, -450, 0, r=True, g=True, b=True, a=False)
link(color, "", rgb, "")
mel.connect_material_property(rgb, "", U.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(custom, "", U.MaterialProperty.MP_OPACITY)
rough = node(U.MaterialExpressionConstant, -450, 400, r=1.0)
mel.connect_material_property(rough, "", U.MaterialProperty.MP_ROUGHNESS)
mel.recompile_material(mat)
eal.save_asset(path)
unreal.log(f"SKID made {path}")
