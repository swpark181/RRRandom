"""Clear every overridden parameter on a material instance, back to its parent's defaults (run inside Unreal).

Usage: UnrealEditor-Cmd <project> -run=pythonscript "-script=ue_reset_mi.py <instance path>"
"""
import sys

import unreal

path = sys.argv[-1]
mi = unreal.load_asset(path)
unreal.MaterialEditingLibrary.clear_all_material_instance_parameters(mi)
unreal.MaterialEditingLibrary.update_material_instance(mi)
unreal.EditorAssetLibrary.save_asset(path)
unreal.log(f"MIRESET {path}: {len(mi.get_editor_property('scalar_parameter_values'))} scalar overrides left")
