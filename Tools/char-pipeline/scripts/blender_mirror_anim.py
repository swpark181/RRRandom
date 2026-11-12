"""Mirror a Mixamo motion left to right (run inside Blender): every bone takes its partner's pose (Left <-> Right), mirrored
across the character's middle (armature X). Round 97: the "Sword And Shield Block Idle" raises the shield arm (left);
mirrored, the sword arm (right) holds the guard in front.

Usage: blender -b --factory-startup --python blender_mirror_anim.py -- <in fbx> <out fbx>
"""
import sys

import bpy
from mathutils import Matrix

src, dst = sys.argv[sys.argv.index("--") + 1:][:2]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
scene = bpy.context.scene
action = arm.animation_data.action
f0, f1 = int(action.frame_range[0]), int(action.frame_range[1])

F = Matrix.Diagonal((-1.0, 1.0, 1.0, 1.0))


def partner(name):
    if "Left" in name:
        return name.replace("Left", "Right")
    if "Right" in name:
        return name.replace("Right", "Left")
    return name


bones = arm.pose.bones
# Parents first
order = []
def visit(b):
    order.append(b.name)
    for c in b.children:
        visit(c)
for b in bones:
    if b.parent is None:
        visit(b)

# Per bone, the constant turn between the mirrored partner's rest frame and its own
fix = {}
for name in order:
    p = partner(name) if partner(name) in bones else name
    rest_b = arm.data.bones[name].matrix_local
    rest_p = arm.data.bones[p].matrix_local
    fix[name] = (F @ rest_p @ F).inverted() @ rest_b

scene.frame_start, scene.frame_end = f0, f1
frames = {}
for f in range(f0, f1 + 1):
    scene.frame_set(f)
    frames[f] = {b.name: b.matrix.copy() for b in bones}

# A new action written frame by frame
arm.animation_data.action = None
for b in bones:
    b.rotation_mode = "QUATERNION"
for f in range(f0, f1 + 1):
    scene.frame_set(f)
    for name in order:
        p = partner(name) if partner(name) in bones else name
        m = F @ frames[f][p] @ F @ fix[name]
        loc = m.to_translation()
        rot = m.to_3x3().normalized().to_4x4()
        rot.translation = loc
        bones[name].matrix = rot
        bpy.context.view_layer.update()
    for name in order:
        bones[name].keyframe_insert("rotation_quaternion", frame=f)
        bones[name].keyframe_insert("location", frame=f)
    print(f"MIRROR frame {f}")

bpy.ops.object.select_all(action="DESELECT")
arm.select_set(True)
bpy.context.view_layer.objects.active = arm
bpy.ops.export_scene.fbx(filepath=dst, use_selection=True, object_types={"ARMATURE"}, add_leaf_bones=False, bake_anim=True,
                         bake_anim_use_all_actions=False, bake_anim_use_nla_strips=False, bake_anim_force_startend_keying=True,
                         bake_anim_simplify_factor=0.0)
print("MIRROR wrote", dst)
