# Fox vs tiger cinematic (2026-11-24): what an animal friend's rig has to act with besides its 41 Mixamo bones. For
# each name in RR_BONES ("Fox,Tiger"): every bone of SK_<Name> with its parent, which of them the clips animate, how
# many vertices each extra bone carries (from the mesh's skin weights, where Python can read them), and the mesh's
# morph targets. Run: cine_run.sh py ue_cine_bones.py "CINEBUILD DONE"
import os

import unreal

AL = unreal.AnimationLibrary
ext = unreal.AnimPoseExtensions


def log(msg):
    unreal.log(f"CINEBUILD {msg}")


for name in (w for w in os.environ.get("RR_BONES", "Fox,Tiger").split(",") if w):
    mesh = unreal.load_asset(f"/Game/Characters/{name}/SK_{name}")
    skeleton = mesh.get_editor_property("skeleton")
    pose = ext.get_reference_pose(skeleton)
    bones = [str(b) for b in ext.get_bone_names(pose)]
    clip = unreal.load_asset(f"/Game/Characters/{name}/Anims/A_{name}_Idle")
    tracks = {str(t).lower() for t in AL.get_animation_track_names(clip)}
    extra = [b for b in bones if b.lower() not in tracks]
    log(f"{name}: {len(bones)} bones, {len(tracks)} animated by A_{name}_Idle; not animated: {extra}")
    for b in extra:
        parent = str(ext.get_bone_pose(pose, b, unreal.AnimPoseSpaces.LOCAL).translation)
        chain = [str(p) for p in AL.find_bone_path_to_root(clip, b)] if b in bones else []
        w = ext.get_bone_pose(pose, b, unreal.AnimPoseSpaces.WORLD).translation
        log(f"  {b}: parent {chain[1] if len(chain) > 1 else '-'}, at {w.x:.1f},{w.y:.1f},{w.z:.1f}")
    try:
        morphs = [str(m) for m in mesh.get_all_morph_target_names()]
    except Exception as e:  # noqa: BLE001
        morphs = f"? ({e})"
    log(f"{name}: morph targets {morphs}")
    post = mesh.get_editor_property("post_process_anim_blueprint")
    log(f"{name}: post-process anim blueprint {post.get_path_name() if post else 'none'}; physics asset "
        f"{mesh.get_editor_property('physics_asset').get_path_name() if mesh.get_editor_property('physics_asset') else 'none'}")
log("DONE")
