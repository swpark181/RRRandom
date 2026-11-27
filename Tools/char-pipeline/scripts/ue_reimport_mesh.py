"""Replace only an animal friend's skinned mesh, on the skeleton it already has (run inside Unreal, legacy FBX importer;
2026-11-20, the fox's face reshaped a little between the eyes).

Usage: UnrealEditor-Cmd <project> -run=pythonscript -dpcvars=Interchange.FeatureFlags.Import.FBX=0
           "-script=ue_reimport_mesh.py <Name>|<rig fbx>"

<rig fbx>: the same rig as before with the mesh changed (vertices moved or cut finer; same bones or more, same UVs), e.g.
blender_face_relief_transfer.py's output. It goes in over /Game/Characters/<Name>/SK_<Name> on SK_<Name>_Skeleton; the
animations, the material, the texture and the fur are not touched (make the fur again if the surface moved under it).
Logs how far the skeleton's reference pose moved (should be 0) and puts M_<Name> back on the mesh.

"<Name>|<fur fbx>|Fur" (2026-11-25, ear bones): the same for SK_<Name>Fur, with the fur importer's vertex colour and normal
options, its own material put back as it was (M_<Name>Fur and its tuning untouched) - for a fur whose shape is the same
and only its weights or bones changed (blender_fur_reweight.py). ue_import_fur_shells.py would rebuild the material.
"""
import sys

import unreal

parts = [a.strip() for a in " ".join(sys.argv[1:]).split("|")]
name, fbx = parts[:2]
suffix = parts[2] if len(parts) > 2 else ""   # "Fur": SK_<Name>Fur
dest = f"/Game/Characters/{name}"
eal = unreal.EditorAssetLibrary
skeleton = unreal.load_asset(f"{dest}/SK_{name}_Skeleton")
if not skeleton or not eal.does_asset_exist(f"{dest}/SK_{name}{suffix}"):
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
if suffix == "Fur":
    ui.skeletal_mesh_import_data.set_editor_property("vertex_color_import_option", unreal.VertexColorImportOption.REPLACE)
    ui.skeletal_mesh_import_data.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
old_materials = [m.get_editor_property("material_interface") for m in unreal.load_asset(f"{dest}/SK_{name}{suffix}").get_editor_property("materials")]
task = unreal.AssetImportTask()
task.filename = fbx
task.destination_path = dest
task.destination_name = f"SK_{name}{suffix}"
task.automated = True
task.replace_existing = True
task.save = True
task.options = ui
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
mesh = unreal.load_asset(f"{dest}/SK_{name}{suffix}")
if mesh.get_editor_property("skeleton") != skeleton:
    raise RuntimeError("the mesh ended up on another skeleton")
after = ref_pose(skeleton)
gone = sorted(set(before) - set(after))
worst_move = max((before[n].translation - after[n].translation).length() for n in before if n in after)
worst_turn = max(abs(unreal.MathLibrary.quat_angular_distance(before[n].rotation, after[n].rotation)) * 57.2958
                 for n in before if n in after)
# The fur: its own material in every slot - the import adds a slot of its own, "MaterialSlot", left empty, and the fur
# component draws with the mesh's materials: an empty slot drew the fox's fur as solid grey lumps (2026-11-25). The body:
# M_<Name>, or what each slot had before when there is none (the game puts the body's material on at run time). New slot
# structs: setting material_interface on the ones read back did not stick.
wanted = unreal.load_asset(f"{dest}/M_{name}Fur") if suffix == "Fur" else unreal.load_asset(f"{dest}/M_{name}")
materials = [unreal.SkeletalMaterial(material_interface=wanted or old_materials[min(i, len(old_materials) - 1)], material_slot_name=m.get_editor_property("material_slot_name"))
             for i, m in enumerate(mesh.get_editor_property("materials"))]
mesh.set_editor_property("materials", materials)
eal.save_asset(mesh.get_path_name())
empty = [i for i, m in enumerate(mesh.get_editor_property("materials")) if not m.get_editor_property("material_interface")]
if suffix and empty:
    raise RuntimeError(f"empty material slots {empty} on {mesh.get_path_name()}")
# (a mesh with more bones - blender_tail_part.py's longer tail - adds them to the skeleton: keep that too, or the
# mesh comes back with bones its skeleton does not know)
if len(after) != len(before):
    eal.save_asset(skeleton.get_path_name())
unreal.log_warning(f"MESHONLY DONE {dest}/SK_{name}{suffix} <- {fbx}: bones {len(before)} -> {len(after)} (gone: {gone}), "
                   f"reference pose moved at most {worst_move:.3f} cm, turned {worst_turn:.3f} deg, "
                   f"{len(materials)} material slot(s) {[str(m.get_editor_property('material_slot_name')) for m in materials]}, bounds {mesh.get_bounds().box_extent}")
