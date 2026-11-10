"""Round 93: lets M_FXCloak draw on instanced meshes too (the battle royale's cotton candy cloud bank round the ring).

Usage: UnrealEditor-Cmd <project> -run=pythonscript "-script=C:/work/char-pipeline/scripts/ue_cloak_instanced.py"

Without the usage flag the game draws the engine's default grey material instead ("missing usage flag
InstancedStaticMeshes! Default Material will be used in game") - the round 92 ring wall had that too.
"""
import unreal

path = "/Game/Effects/DiceFX/M_FXCloak"
mat = unreal.EditorAssetLibrary.load_asset(path)
before = mat.get_editor_property("used_with_instanced_static_meshes")
mat.set_editor_property("used_with_instanced_static_meshes", True)
unreal.MaterialEditingLibrary.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
unreal.log(f"CLOAK instanced usage: was {before}, now {mat.get_editor_property('used_with_instanced_static_meshes')}, saved")
