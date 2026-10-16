"""Log a material instance's overridden parameters (run inside Unreal).

Usage: UnrealEditor-Cmd <project> -run=pythonscript "-script=ue_print_mi.py <instance path>"
"""
import sys

import unreal

mi = unreal.load_asset(sys.argv[-1])
for p in mi.get_editor_property("scalar_parameter_values"):
    unreal.log(f"MIVALUE {p.get_editor_property('parameter_info').get_editor_property('name')} = {p.get_editor_property('parameter_value')}")
for p in mi.get_editor_property("vector_parameter_values"):
    unreal.log(f"MIVALUE {p.get_editor_property('parameter_info').get_editor_property('name')} = {p.get_editor_property('parameter_value')}")
