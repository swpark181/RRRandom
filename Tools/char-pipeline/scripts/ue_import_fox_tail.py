"""Brings the tail-rigged fox (blender_fox_tail.py) into Unreal (run inside Unreal, legacy FBX importer).

Usage: UnrealEditor-Cmd <project> -run=pythonscript -dpcvars=Interchange.FeatureFlags.Import.FBX=0
           "-script=ue_import_fox_tail.py test|<fbx>"    imports to a scratch folder with its own skeleton and compares
                                                           every bone's reference pose with SK_Fox's (then deletes it)
           "-script=ue_import_fox_tail.py apply|<fbx>"   reimports over /Game/Characters/Fox/SK_Fox on its skeleton,
                                                           which gains the tail bones; puts M_Fox back on every slot

The animations only key the old bones, so they play as before as long as those bones' reference poses match.
"""
import sys

import unreal

mode, fbx = " ".join(sys.argv[1:]).split("|")
eal = unreal.EditorAssetLibrary
FOX = "/Game/Characters/Fox"
SCRATCH = "/Game/Scratch/FoxTailTest"
fox = unreal.load_asset(f"{FOX}/SK_Fox")
skeleton = fox.get_editor_property("skeleton")


def log(msg):
    unreal.log(f"FOXTAIL {msg}")


def import_mesh(folder, name, onto=None):
    ui = unreal.FbxImportUI()
    ui.automated_import_should_detect_type = False
    ui.import_as_skeletal = True
    ui.import_mesh = True
    ui.import_animations = False
    ui.import_materials = False
    ui.import_textures = False
    ui.create_physics_asset = False
    ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
    if onto:
        ui.skeleton = onto
    task = unreal.AssetImportTask()
    task.filename = fbx
    task.destination_path = folder
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    task.options = ui
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    paths = list(task.imported_object_paths)
    log(f"imported -> {paths}")
    return next(a for a in (unreal.load_asset(p) for p in paths) if isinstance(a, unreal.SkeletalMesh))


def ref_pose(skel):
    pose = unreal.AnimPoseExtensions.get_reference_pose(skel)
    return {str(n): unreal.AnimPoseExtensions.get_ref_bone_pose(pose, n, unreal.AnimPoseSpaces.LOCAL)
            for n in unreal.AnimPoseExtensions.get_bone_names(pose)}


def compare(old, new):
    worst_move, worst_turn = 0.0, 0.0
    for name, a in old.items():
        b = new.get(name)
        if b is None:
            log(f"MISSING {name}")
            continue
        move = (a.translation - b.translation).length()
        turn = abs(unreal.MathLibrary.quat_angular_distance(a.rotation, b.rotation)) * 57.2958
        worst_move, worst_turn = max(worst_move, move), max(worst_turn, turn)
        if move > 0.05 or turn > 0.5:
            log(f"DIFF {name}: moved {move:.2f} cm, turned {turn:.2f} deg")
    log(f"compare: worst move {worst_move:.3f} cm, worst turn {worst_turn:.3f} deg over {len(old)} bones")
    for name in sorted(set(new) - set(old)):
        t = new[name]
        log(f"NEW {name}: local {t.translation}")


old = ref_pose(skeleton)
if mode == "test":
    made = import_mesh(SCRATCH, "SK_FoxTailTest")
    compare(old, ref_pose(made.get_editor_property("skeleton")))
    log(f"test mesh bounds {made.get_bounds().box_extent} vs fox {fox.get_bounds().box_extent}")
    eal.delete_directory("/Game/Scratch")
elif mode == "apply":
    materials = [m.get_editor_property("material_interface") for m in fox.get_editor_property("materials")]
    physics = fox.get_editor_property("physics_asset")
    made = import_mesh(FOX, "SK_Fox", skeleton)
    if made.get_editor_property("skeleton") != skeleton:
        raise RuntimeError("SK_Fox ended up on another skeleton")
    slots = made.get_editor_property("materials")
    fox_material = unreal.load_asset(f"{FOX}/M_Fox")
    for slot in slots:
        slot.set_editor_property("material_interface", fox_material)
    made.set_editor_property("materials", slots)
    if physics and made.get_editor_property("physics_asset") != physics:
        made.set_editor_property("physics_asset", physics)
    eal.save_asset(made.get_path_name())
    eal.save_asset(skeleton.get_path_name())
    log(f"materials before {[m.get_name() if m else None for m in materials]} -> {len(slots)} slots of M_Fox; "
        f"physics {physics.get_name() if physics else None} -> {made.get_editor_property('physics_asset')}")
    compare(old, ref_pose(skeleton))
    for a in eal.list_assets(f"{FOX}/Anims", recursive=False):
        anim = unreal.load_asset(a)
        if anim.get_editor_property("skeleton") != skeleton:
            raise RuntimeError(f"{a} lost its skeleton")
    log("DONE")
