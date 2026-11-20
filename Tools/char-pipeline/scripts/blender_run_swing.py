"""Run + swing in one Mixamo clip (cinematic, 2026-11-19): the upper body (Spine and everything above it, arms, hands)
from a swing, the hips and legs from a run looped under it, the way the game lays a melee blow over the run at runtime
(URRRandomBodyAnimInstance) but baked, so a Level Sequence can play it on the whole body. Both clips must come from the
same Mixamo rig (same rest pose), like the sword-and-shield set taken from the rabbit rig.

Usage: blender -b --factory-startup --python blender_run_swing.py -- <run.fbx> <swing.fbx> <out.fbx> [run rate]
The clip is as long as the swing; the run is looped (and sped up by run rate) under it.
"""
import os
import sys

import bpy

run_path, swing_path, out_path = sys.argv[sys.argv.index("--") + 1:][:3]
rest = sys.argv[sys.argv.index("--") + 4:]
run_rate = float(rest[0]) if rest else 1.0
UPPER = ("Spine", "Neck", "Head", "Shoulder", "Arm", "Hand")

bpy.ops.wm.read_factory_settings(use_empty=True)


def load(path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=path, automatic_bone_orientation=False)
    return next(o for o in bpy.data.objects if o not in before and o.type == "ARMATURE")


def curves(obj):
    """The F-curves of the object's action, layered actions (Blender 4.4+) or the old flat list."""
    ad = obj.animation_data
    act = ad.action
    if hasattr(act, "fcurves") and len(getattr(act, "layers", [])) == 0:
        return act.fcurves
    from bpy_extras import anim_utils
    return anim_utils.action_get_channelbag_for_slot(act, ad.action_slot).fcurves


def bone_of(path):
    return path.split('"')[1] if '"' in path else ""


def is_upper(name):
    short = name.split(":")[-1]
    return any(short.startswith(k) or k in short for k in UPPER) and not short.startswith("Hips")


run = load(run_path)
swing = load(swing_path)
run_curves = {(c.data_path, c.array_index): c for c in curves(run)}
r0, r1 = run.animation_data.action.frame_range
s0, s1 = swing.animation_data.action.frame_range
run_len = r1 - r0
swapped = 0
for c in list(curves(swing)):
    name = bone_of(c.data_path)
    if not name or is_upper(name):
        continue
    src = run_curves.get((c.data_path, c.array_index))
    if src is None:
        continue
    values = []
    f = s0
    while f <= s1 + 1e-6:
        t = r0 + ((f - s0) * run_rate) % run_len
        values.append((f, src.evaluate(t)))
        f += 1.0
    c.keyframe_points.clear()
    c.keyframe_points.add(len(values))
    for k, (f, v) in zip(c.keyframe_points, values):
        k.co = (f, v)
        k.interpolation = "LINEAR"
    c.update()
    swapped += 1

bpy.data.objects.remove(run, do_unlink=True)
bpy.ops.object.select_all(action="DESELECT")
swing.select_set(True)
bpy.context.view_layer.objects.active = swing
bpy.context.scene.frame_start, bpy.context.scene.frame_end = int(s0), int(s1)
os.makedirs(os.path.dirname(out_path), exist_ok=True)
bpy.ops.export_scene.fbx(filepath=out_path, use_selection=True, add_leaf_bones=False,
                         bake_anim=True, bake_anim_use_all_bones=True, bake_anim_use_nla_strips=False,
                         bake_anim_use_all_actions=False, bake_anim_force_startend_keying=True, bake_anim_step=1.0,
                         bake_anim_simplify_factor=0.0, path_mode="COPY", embed_textures=False)
print(f"RUNSWING wrote {out_path}: {int(s1 - s0) + 1} frames, {swapped} lower-body curves from the run (run {int(run_len)} frames x{run_rate})")
