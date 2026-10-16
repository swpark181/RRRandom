"""Import a Mixamo character (one FBX with skin) plus its animation FBXs into a UE project (run inside Unreal).

Usage (args after the script path, '|'-separated to survive spaces):
  UnrealEditor-Cmd <project> -run=pythonscript -dpcvars=Interchange.FeatureFlags.Import.FBX=0
      "-script=ue_import_mixamo.py <dest>|<mesh fbx>|<anim fbx>|<anim fbx>..."

Uses the legacy FBX importer (Interchange FBX off via the cvar above), because Interchange ignored the
AssetImportTask options here: every animation came in with its own skeleton and mesh.
The first FBX makes the skeletal mesh and skeleton; the rest import as animations on that skeleton.
Afterwards it logs asset paths, mesh bounds, bone names, animation lengths and how far the hips travel.
"""
import sys

import unreal

args = " ".join(sys.argv[1:]).split("|")
dest, mesh_fbx, anim_fbxs = args[0], args[1], args[2:]
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(msg):
    unreal.log(f"MIXAMO {msg}")


def import_fbx(path, folder, skeleton=None):
    ui = unreal.FbxImportUI()
    ui.automated_import_should_detect_type = False
    ui.import_as_skeletal = True
    ui.import_animations = True
    ui.import_materials = False
    ui.import_textures = False
    ui.create_physics_asset = False
    if skeleton:
        ui.import_mesh = False
        ui.skeleton = skeleton
        ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_ANIMATION
    else:
        ui.import_mesh = True
        ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
    ui.anim_sequence_import_data.set_editor_property(
        "animation_length", unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)

    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = folder
    task.automated = True
    task.replace_existing = True
    task.save = True
    task.options = ui
    asset_tools.import_asset_tasks([task])
    paths = list(task.imported_object_paths)
    log(f"imported {path.split('/')[-1]} -> {paths}")
    return [unreal.load_asset(p) for p in paths]


# Start clean so a rerun doesn't leave assets from an earlier attempt behind
if unreal.EditorAssetLibrary.does_directory_exist(dest):
    unreal.EditorAssetLibrary.delete_directory(dest)

made = import_fbx(mesh_fbx, dest)
mesh = next((a for a in made if isinstance(a, unreal.SkeletalMesh)), None)
if mesh is None:
    raise RuntimeError("no skeletal mesh imported")
skeleton = mesh.get_editor_property("skeleton")
log(f"mesh {mesh.get_path_name()} skeleton {skeleton.get_path_name()}")

anims = [a for a in unreal.EditorAssetLibrary.list_assets(dest, recursive=False)
         if isinstance(unreal.load_asset(a), unreal.AnimSequence)]
anims = [unreal.load_asset(a) for a in anims]
for fbx in anim_fbxs:
    anims += [a for a in import_fbx(fbx, dest + "/Anims", skeleton) if isinstance(a, unreal.AnimSequence)]

bounds = mesh.get_bounds()
log(f"bounds origin {bounds.origin} extent {bounds.box_extent}")
tracks = [str(n) for n in unreal.AnimationLibrary.get_animation_track_names(anims[0])] if anims else []
log(f"bones ({len(tracks)}): {tracks}")
hips = next((t for t in tracks if t.lower().endswith("hips")), None)

for a in anims:
    frames = unreal.AnimationLibrary.get_num_frames(a)
    line = f"anim {a.get_path_name()} len={a.get_play_length():.2f}s frames={frames} skel={a.get_editor_property('skeleton').get_name()}"
    if hips:
        poses = [unreal.AnimationLibrary.get_bone_pose_for_frame(a, hips, f, False).translation
                 for f in (0, frames // 2, max(0, frames - 1))]
        line += " hips " + " -> ".join(f"({p.x:.1f},{p.y:.1f},{p.z:.1f})" for p in poses)
    log(line)

unreal.EditorAssetLibrary.save_directory(dest)
log("DONE")
