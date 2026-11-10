"""Round 94: lets the materials of the given static meshes draw on instanced meshes (Dice Town's parked cars are
instances of the drivable car's SM_Car; without the usage flag the game draws the engine's default grey material).

Usage: UnrealEditor-Cmd <project> -run=pythonscript "-script=C:/work/char-pipeline/scripts/ue_instanced_usage.py /Game/Vehicles/SM_Car ..."

For each mesh: every slot's material, followed up to its base material, gets "used with instanced static meshes",
is recompiled and saved.
"""
import sys

import unreal

eal = unreal.EditorAssetLibrary

for mesh_path in sys.argv[1:]:
    mesh = eal.load_asset(mesh_path)
    for slot in mesh.get_editor_property("static_materials"):
        mat = slot.get_editor_property("material_interface")
        if not mat:
            continue
        base = mat.get_base_material()
        before = base.get_editor_property("used_with_instanced_static_meshes")
        if not before:
            base.set_editor_property("used_with_instanced_static_meshes", True)
            unreal.MaterialEditingLibrary.recompile_material(base)
            eal.save_asset(base.get_path_name().split(".")[0], only_if_is_dirty=False)
        unreal.log_warning(f"INSTANCED {mesh_path}: {mat.get_name()} -> {base.get_name()} was {before}, now True")
