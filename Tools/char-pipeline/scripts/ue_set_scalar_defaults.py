"""Set the default values of scalar parameters in a material (run inside Unreal; edits the material in place).

Usage: UnrealEditor-Cmd <project> -run=pythonscript "-script=ue_set_scalar_defaults.py <material>|<param>=<value>|..."
  e.g. /Game/Characters/RedFox/M_RedFox|FurRimStrength=0|FurSheen=0
Used to compare the fur rim (ue_fur_rim.py) on and off in the game, and to settle its values.
"""
import sys

import unreal

args = " ".join(sys.argv[1:]).split("|")
path = args[0].strip()
values = {k.strip(): float(v) for k, v in (kv.split("=") for kv in args[1:])}
mat = unreal.load_asset(path)
done = []
for expr in unreal.ObjectIterator(unreal.MaterialExpressionScalarParameter):
    if expr.get_outer() != mat:
        continue
    name = str(expr.get_editor_property("parameter_name"))
    if name in values:
        expr.set_editor_property("default_value", values[name])
        done.append(name)
unreal.MaterialEditingLibrary.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset(path)
unreal.log(f"SCALARS {path}: set {sorted(set(done))} to {values}")
