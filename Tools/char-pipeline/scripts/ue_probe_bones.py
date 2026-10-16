# Lists the bones of a skeletal mesh with their reference-pose world positions (and which ones carry skin).
# Usage (UnrealEditor-Cmd -run=pythonscript): "-script=ue_probe_bones.py /Game/Characters/Fox/SK_Fox|<out.txt>"
import sys
import unreal

args = sys.argv[1:] if len(sys.argv) > 1 else []
spec = " ".join(args).split("|")
mesh_path, out_path = spec[0].strip(), spec[1].strip()
mesh = unreal.load_asset(mesh_path)
skeleton = mesh.get_editor_property("skeleton")
lines = []
pose = unreal.AnimPoseExtensions.get_reference_pose(skeleton)
for name in unreal.AnimPoseExtensions.get_bone_names(pose):
    world = unreal.AnimPoseExtensions.get_ref_bone_pose(pose, name, unreal.AnimPoseSpaces.WORLD)
    local = unreal.AnimPoseExtensions.get_ref_bone_pose(pose, name, unreal.AnimPoseSpaces.LOCAL)
    w, l = world.translation, local.translation
    lines.append(f"{name}  world ({w.x:.1f}, {w.y:.1f}, {w.z:.1f})  local ({l.x:.1f}, {l.y:.1f}, {l.z:.1f})  parent {skeleton.get_parent_bone(name) if hasattr(skeleton, 'get_parent_bone') else '?'}")
open(out_path, "w", encoding="utf-8").write("\n".join(lines))
