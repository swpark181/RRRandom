"""Gives the Mixamo fox a tail rig (run in Blender): a chain of tail bones off the hips, with the tail skinned to it.

Usage: blender -b --factory-startup --python blender_fox_tail.py -- <SK_Fox.fbx> <out.fbx>

The auto-rig had no tail bones and skinned the tail mostly to the left thigh, so it flapped with the left leg.
Tail vertices (behind the hips, low and narrow; see TAIL_*) now follow mixamorig:Tail1..TailN only, blending from the
hips over the first few centimeters. Every other bone and weight is left as it was, and no animation is exported:
in Unreal the mesh is reimported onto its existing skeleton, which just gains the tail bones.

Coordinates below are Unreal's mesh space in cm: X = the fox's left, -Y = backward (the tail), Z = up.
Blender world (meters) maps to it as (x, -y, z) * 100.
"""
import sys

import bpy
from mathutils import Vector

src, out = sys.argv[sys.argv.index("--") + 1:][:2]

# Tail centerline joints, root to tip (measured from the mesh: the tail sags a little and turns up at the tip)
JOINTS = [(0.0, -6.0, 18.5), (0.0, -12.5, 15.5), (0.0, -19.0, 13.0), (0.0, -25.5, 11.8), (0.0, -32.5, 13.6)]
# Which vertices are the tail: behind TAIL_START, below TAIL_TOP, within TAIL_HALF_WIDTH of the middle
TAIL_START = -4.5
TAIL_TOP = 30.0
TAIL_HALF_WIDTH = 8.0
# Hips to Tail1 blend, from TAIL_START back to here
ROOT_BLEND_END = -9.0
# Neighboring tail bones blend over this many cm around each joint
JOINT_BLEND = 3.0
PREFIX = "mixamorig:"


def ue_to_world(p):
    return Vector((p[0] / 100.0, -p[1] / 100.0, p[2] / 100.0))


def smoothstep(e0, e1, x):
    t = max(0.0, min(1.0, (x - e0) / (e1 - e0)))
    return t * t * (3.0 - 2.0 * t)


bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
mesh = next(o for o in bpy.data.objects if o.type == "MESH")
# Only the mesh and its skin matter; the bind pose comes from the armature's rest pose
arm.animation_data_clear()
for action in list(bpy.data.actions):
    bpy.data.actions.remove(action)

# Bones, in armature space
bpy.context.view_layer.objects.active = arm
bpy.ops.object.mode_set(mode="EDIT")
to_arm = arm.matrix_world.inverted()
edit_bones = arm.data.edit_bones
parent = edit_bones[PREFIX + "Hips"]
names = []
for i in range(len(JOINTS) - 1):
    bone = edit_bones.new(f"{PREFIX}Tail{i + 1}")
    bone.head = to_arm @ ue_to_world(JOINTS[i])
    bone.tail = to_arm @ ue_to_world(JOINTS[i + 1])
    # Roll so the bone's X axis points the same way as the hips', keeping side swing and lift on clean axes
    bone.align_roll(parent.z_axis)
    bone.parent = parent
    bone.use_connect = i > 0
    parent = bone
    names.append(bone.name)
bpy.ops.object.mode_set(mode="OBJECT")

# Weights
groups = {n: mesh.vertex_groups.get(n) or mesh.vertex_groups.new(name=n) for n in names + [PREFIX + "Hips"]}
hips = groups[PREFIX + "Hips"]
joint_y = [j[1] for j in JOINTS]
count = 0
for v in mesh.data.vertices:
    w = mesh.matrix_world @ v.co
    x, y, z = w.x * 100.0, -w.y * 100.0, w.z * 100.0
    if y >= TAIL_START or z >= TAIL_TOP or abs(x) >= TAIL_HALF_WIDTH:
        continue
    count += 1
    # Each tail bone owns its stretch of the tail, blending into the next around the joints
    chain = {}
    for i in range(len(names)):
        start, end = joint_y[i], joint_y[i + 1]
        lo = 1.0 if i == 0 else smoothstep(start + JOINT_BLEND, start - JOINT_BLEND, y)
        hi = 1.0 if i == len(names) - 1 else smoothstep(end + JOINT_BLEND, end - JOINT_BLEND, y)
        weight = lo * (1.0 - hi)
        if weight > 1e-4:
            chain[names[i]] = weight
    total = sum(chain.values())
    root = smoothstep(TAIL_START, ROOT_BLEND_END, y)
    for g in list(v.groups):
        mesh.vertex_groups[g.group].remove([v.index])
    if root < 1.0:
        hips.add([v.index], 1.0 - root, "REPLACE")
    for name, weight in chain.items():
        groups[name].add([v.index], root * weight / total, "REPLACE")
print(f"TAIL bones {names}, {count} tail vertices reskinned")

bpy.ops.export_scene.fbx(
    filepath=out, use_selection=False, object_types={"ARMATURE", "MESH"}, add_leaf_bones=False, bake_anim=False,
    primary_bone_axis="Y", secondary_bone_axis="X", use_armature_deform_only=False, mesh_smooth_type="OFF",
    path_mode="STRIP")
print(f"TAIL wrote {out}")
