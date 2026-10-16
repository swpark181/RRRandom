"""Give the fox material a pastel switch (run inside Unreal; edits the material in place so references keep working).

Usage: UnrealEditor-Cmd <project> -run=pythonscript "-script=ue_fox_pastel.py /Game/Characters/Fox/M_Fox"

Whatever feeds Base Color now goes through: out = lerp(color, lerp(desaturate(color, "PastelDesaturate"), "PastelCream",
"PastelLift"), "Pastel"). "Pastel" is 0 by default (the fox as before); Dice Land, the third person map, sets it to 1 so
the fox sits in its softer palette. The mix happens on linear colors, where a little cream goes a long way (0.2 turned the
orange fur beige), hence the small amounts. Running it again replaces the earlier pastel nodes rather than stacking them
(the old ones stay in the graph unconnected).
"""
import sys

import unreal

path = " ".join(sys.argv[1:]).strip()
mel = unreal.MaterialEditingLibrary
U = unreal
BASE = U.MaterialProperty.MP_BASE_COLOR


def log(msg):
    unreal.log(f"FOXPASTEL {msg}")


mat = unreal.load_asset(path)
source = mel.get_material_property_input_node(mat, BASE)
source_out = mel.get_material_property_input_node_output_name(mat, BASE)
# A second run finds its own output lerp on Base Color: step back to what fed it
if isinstance(source, U.MaterialExpressionLinearInterpolate) and source.get_editor_property("desc") == "Pastel":
    source = mel.get_inputs_for_material_expression(mat, source)[0]
    source_out = ""
log(f"base color comes from {source.get_name()} '{source_out}'")


def node(cls, x, y, **props):
    n = mel.create_material_expression(mat, cls, x, y)
    for key, value in props.items():
        n.set_editor_property(key, value)
    return n


def link(a, a_out, b, b_in):
    if not mel.connect_material_expressions(a, a_out, b, b_in):
        raise RuntimeError(f"could not connect {a.get_name()} into {b.get_name()}.{b_in}")


amount = node(U.MaterialExpressionScalarParameter, -500, 600, parameter_name="Pastel", default_value=0.0)
fraction = node(U.MaterialExpressionScalarParameter, -900, 700, parameter_name="PastelDesaturate", default_value=0.12)
cream = node(U.MaterialExpressionVectorParameter, -900, 800, parameter_name="PastelCream", default_value=U.LinearColor(1.0, 0.92, 0.86, 1.0))
lift = node(U.MaterialExpressionScalarParameter, -900, 950, parameter_name="PastelLift", default_value=0.07)
desat = node(U.MaterialExpressionDesaturation, -700, 650)
if not mel.connect_material_expressions(source, source_out, desat, ""):
    link(source, source_out, desat, "Input")
link(fraction, "", desat, "Fraction")
soft = node(U.MaterialExpressionLinearInterpolate, -500, 750)
link(desat, "", soft, "A")
link(cream, "", soft, "B")
link(lift, "", soft, "Alpha")
out = node(U.MaterialExpressionLinearInterpolate, -300, 650, desc="Pastel")
link(source, source_out, out, "A")
link(soft, "", out, "B")
link(amount, "", out, "Alpha")
mel.connect_material_property(out, "", BASE)
mel.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset(path)
log("DONE")
