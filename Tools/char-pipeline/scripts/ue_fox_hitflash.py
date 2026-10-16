"""Let the fox material flash when hit (run inside Unreal; edits the material in place so references keep working).

Usage: UnrealEditor-Cmd <project> -run=pythonscript "-script=ue_fox_hitflash.py /Game/Characters/Fox/M_Fox"

Emissive becomes (whatever fed it before, if anything) + "HitFlashColor" x "HitFlash". "HitFlash" is 0 by default;
ARRRandomCharacter raises it for a moment on every machine when the brawler takes a hit and lets it fade.
Running it again replaces the earlier flash nodes rather than stacking them (the old ones stay in the graph unconnected).
"""
import sys

import unreal

path = " ".join(sys.argv[1:]).strip()
mel = unreal.MaterialEditingLibrary
U = unreal
EMISSIVE = U.MaterialProperty.MP_EMISSIVE_COLOR


def log(msg):
    unreal.log(f"FOXFLASH {msg}")


mat = unreal.load_asset(path)
source = mel.get_material_property_input_node(mat, EMISSIVE)
source_out = mel.get_material_property_input_node_output_name(mat, EMISSIVE) if source else ""
# A second run finds its own add on Emissive: step back to what fed it (nothing, the first time round)
if isinstance(source, U.MaterialExpressionAdd) and source.get_editor_property("desc") == "HitFlash":
    source = mel.get_inputs_for_material_expression(mat, source)[0]
    source_out = ""
log(f"emissive came from {source.get_name() if source else 'nothing'}")


def node(cls, x, y, **props):
    n = mel.create_material_expression(mat, cls, x, y)
    for key, value in props.items():
        n.set_editor_property(key, value)
    return n


flash = node(U.MaterialExpressionScalarParameter, -700, 1100, parameter_name="HitFlash", default_value=0.0)
tint = node(U.MaterialExpressionVectorParameter, -700, 1200, parameter_name="HitFlashColor", default_value=U.LinearColor(1.0, 0.95, 0.9, 1.0))
lit = node(U.MaterialExpressionMultiply, -500, 1150)
mel.connect_material_expressions(tint, "", lit, "A")
mel.connect_material_expressions(flash, "", lit, "B")
total = node(U.MaterialExpressionAdd, -300, 1100, desc="HitFlash")
if source:
    mel.connect_material_expressions(source, source_out, total, "A")
else:
    mel.connect_material_expressions(node(U.MaterialExpressionConstant3Vector, -500, 1050, constant=U.LinearColor(0, 0, 0, 1)), "", total, "A")
mel.connect_material_expressions(lit, "", total, "B")
mel.connect_material_property(total, "", EMISSIVE)
mel.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset(path)
log("DONE")
