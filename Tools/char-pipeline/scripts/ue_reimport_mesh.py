"""Replace only an animal friend's skinned mesh, on the skeleton it already has (run inside Unreal, legacy FBX importer;
2026-11-20, the fox's face reshaped a little between the eyes).

Usage: UnrealEditor-Cmd <project> -run=pythonscript -dpcvars=Interchange.FeatureFlags.Import.FBX=0
           "-script=ue_reimport_mesh.py <Name>|<rig fbx>"

<rig fbx>: the same rig as before with the mesh changed (vertices moved or cut finer; same bones, same UVs), e.g.
blender_face_relief_transfer.py's output. It goes in over /Game/Characters/<Name>/SK_<Name> on SK_<Name>_Skeleton; the
animations, the material, the texture and the fur are not touched (make the fur again if the surface moved under it).
Logs how far the skeleton's reference pose moved (should be 0) and puts M_<Name> back on the mesh.
"""
import sys

import unreal

name, fbx = [a.strip() for a in " ".join(sys.argv[1:]).split("|")[:2]]
dest = f"/Game/Characters/{name}"
eal = unreal.EditorAssetLibrary
skeleton = unreal.load_asset(f"{dest}/SK_{name}_Skeleton")
if not skeleton or not eal.does_asset_exist(f"{dest}/SK_{name}"):
    raise RuntimeError(f"no {dest}/SK_{name} or its skeleton: import the animal first (ue_import_animal.py)")


def ref_pose(skel):
    pose = unreal.AnimPoseExtensions.get_reference_pose(skel)
    return {str(n): unreal.AnimPoseExtensions.get_ref_bone_pose(pose, n, unreal.AnimPoseSpaces.LOCAL)
            for n in unreal.AnimPoseExtensions.get_bone_names(pose)}


before = ref_pose(skeleton)
ui = unreal.FbxImportUI()
ui.automated_import_should_detect_type = False
ui.import_as_skeletal = True
ui.import_mesh = True
ui.import_animations = False
ui.import_materials = False
ui.import_textures = False
ui.create_physics_asset = False
ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
ui.skeleton = skeleton
task = unreal.AssetImportTask()
task.filename = fbx
task.destination_path = dest
task.destination_name = f"SK_{name}"
task.automated = True
task.replace_existing = True
task.save = True
task.options = ui
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
mesh = unreal.load_asset(f"{dest}/SK_{name}")
if mesh.get_editor_property("skeleton") != skeleton:
    raise RuntimeError("the mesh ended up on another skeleton")
after = ref_pose(skeleton)
gone = sorted(set(before) - set(after))
worst_move = max((before[n].translation - after[n].translation).length() for n in before if n in after)
worst_turn = max(abs(unreal.MathLibrary.quat_angular_distance(before[n].rotation, after[n].rotation)) * 57.2958
                 for n in before if n in after)
materials = mesh.get_editor_property("materials")
for m in materials:
    m.set_editor_property("material_interface", unreal.load_asset(f"{dest}/M_{name}"))
mesh.set_editor_property("materials", materials)
eal.save_asset(mesh.get_path_name())
unreal.log_warning(f"MESHONLY DONE {dest}/SK_{name} <- {fbx}: bones {len(before)} -> {len(after)} (gone: {gone}), "
                   f"reference pose moved at most {worst_move:.3f} cm, turned {worst_turn:.3f} deg, "
                   f"{len(materials)} material slot(s) {[str(m.get_editor_property('material_slot_name')) for m in materials]}, bounds {mesh.get_bounds().box_extent}")
