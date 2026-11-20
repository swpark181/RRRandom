"""Re-bake an FBX motion at 30 fps (a Mixamo clip whose keys came in at an odd rate: round 98's cheer imported as 5568
frames for 2.9 s).  blender -b -P blender_resample_anim.py -- <in.fbx> <out.fbx> [seconds]"""
import sys

import bpy

src, dst = sys.argv[sys.argv.index("--") + 1:][:2]
rest = sys.argv[sys.argv.index("--") + 3:]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src, automatic_bone_orientation=False)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
action = arm.animation_data.action
start, end = action.frame_range
scene = bpy.context.scene
fps_in = scene.render.fps
seconds = float(rest[0]) if rest else (end - start) / fps_in
print(f"RESAMPLE in: frames {start:.0f}..{end:.0f} at {fps_in} fps = {(end - start) / fps_in:.2f} s; wanted {seconds:.2f} s")
# Stretch the action so that it lasts `seconds` at 30 fps, then bake one key a frame
scene.render.fps = 30
frames = max(2, round(seconds * 30))
scale = frames / max(1.0, end - start)
for fc in (action.fcurves if hasattr(action, "fcurves") else [f for l in action.layers for s in l.strips for b in s.channelbags for f in b.fcurves]):
    for k in fc.keyframe_points:
        k.co.x = (k.co.x - start) * scale
        k.handle_left.x = (k.handle_left.x - start) * scale
        k.handle_right.x = (k.handle_right.x - start) * scale
scene.frame_start, scene.frame_end = 0, frames
bpy.context.view_layer.objects.active = arm
arm.select_set(True)
bpy.ops.object.mode_set(mode="POSE")
bpy.ops.pose.select_all(action="SELECT")
bpy.ops.nla.bake(frame_start=0, frame_end=frames, only_selected=False, visual_keying=True, clear_constraints=False, bake_types={"POSE"})
bpy.ops.object.mode_set(mode="OBJECT")
bpy.ops.export_scene.fbx(filepath=dst, use_selection=False, object_types={"ARMATURE"}, add_leaf_bones=False, bake_anim=True,
                         bake_anim_use_all_actions=False, bake_anim_use_nla_strips=False, bake_anim_simplify_factor=0.0)
print(f"RESAMPLE out: {frames} frames at 30 fps -> {dst}")
