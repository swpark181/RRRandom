"""Bend an animal friend's ear bones the way the game does and render the head, to check the skin before Unreal
(2026-11-25, with blender_ear_bones.py).

Usage: blender -b -P blender_ear_pose.py -- <rig fbx> <png prefix> [--poses "0,0;45,0;-20,0;0,35;30,30"] [--tex <basecolor>]
Each pose "back,out" in degrees, shared evenly over each ear's bones: back = the tip toward the back of the head
(negative: forward), out = the tip away from the head's middle (the 'airplane ear'; for a fin pointing sideways, down).
The axes come from each ear's own direction (first bone's head -> End bone), as in the game. Renders a row per pose
from the front, the side and behind: <png prefix>_<k>_<view>.png, and prints the paths.
"""
import math
import sys

import bpy
from mathutils import Matrix, Quaternion, Vector

args = sys.argv[sys.argv.index("--") + 1:]
rig, out = args[:2]


def opt(name, default):
    return type(default)(args[args.index(name) + 1]) if name in args else default


POSES = [tuple(float(c) for c in p.split(",")) for p in opt("--poses", "0,0;45,0;-20,0;0,35;30,30").split(";")]
TEX = opt("--tex", "")

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=rig)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
arm.animation_data_clear()
meshes = [o for o in bpy.data.objects if o.type == "MESH"]
for o in meshes:
    if TEX:
        mt = bpy.data.materials.new("tex")
        mt.use_nodes = True
        tn = mt.node_tree.nodes.new("ShaderNodeTexImage")
        tn.image = bpy.data.images.load(TEX)
        mt.node_tree.links.new(tn.outputs["Color"], mt.node_tree.nodes["Principled BSDF"].inputs["Base Color"])
        o.data.materials.clear()
        o.data.materials.append(mt)
head = next(b for b in arm.data.bones if b.name.endswith("Head"))
centre_x = (arm.matrix_world @ head.head_local).x
ears = {}
for b in arm.data.bones:
    n = b.name.split(":")[-1]
    if n.startswith("Ear") and not n.endswith("End"):
        ears.setdefault(n[3], []).append(b.name)
for side in ears:
    ears[side].sort()

scene = bpy.context.scene
scene.render.engine = "BLENDER_WORKBENCH"
scene.display.shading.color_type = "TEXTURE" if TEX else "MATERIAL"
scene.render.resolution_x = scene.render.resolution_y = 520
scene.world = bpy.data.worlds.new("w")
scene.world.color = (0.15, 0.15, 0.17)
cd = bpy.data.cameras.new("c")
cd.type = "ORTHO"
cam = bpy.data.objects.new("c", cd)
scene.collection.objects.link(cam)
scene.camera = cam
pts = [arm.matrix_world @ arm.data.bones[n].head_local for s in ears for n in ears[s]]
pts += [arm.matrix_world @ arm.data.bones[n[:-1] + "End"].head_local for s in ears for n in ears[s][:1]]
mid = sum(pts, Vector()) / len(pts)
mid.z -= 0.06
cd.ortho_scale = max(0.4, 2.6 * max((p - mid).length for p in pts))

bpy.context.view_layer.objects.active = arm
bpy.ops.object.mode_set(mode="POSE")
for k, (back, outw) in enumerate(POSES):
    for side, names in ears.items():
        root = arm.matrix_world @ arm.data.bones[names[0]].head_local
        end = arm.matrix_world @ arm.data.bones[names[0][:-1] + "End"].head_local
        e = (end - root).normalized()
        b_axis = e.cross(Vector((0, 1, 0)))           # the model faces -Y: +Y is the back of the head
        outward = Vector((1 if root.x > centre_x else -1, 0, -1)).normalized()
        outward = (outward - e * outward.dot(e)).normalized()
        o_axis = e.cross(outward)
        per = 1.0 / len(names)
        turn = Quaternion(o_axis, math.radians(outw * per)) @ Quaternion(b_axis, math.radians(back * per))
        for n in names:
            pb = arm.pose.bones[n]
            pb.rotation_mode = "QUATERNION"
            pb.rotation_quaternion = Quaternion()
        bpy.context.view_layer.update()
        for n in names:
            pb = arm.pose.bones[n]
            # world turn about the bone's own head, in the bone's local frame
            world = (arm.matrix_world @ pb.matrix).to_quaternion()
            pb.rotation_quaternion = pb.rotation_quaternion @ (world.inverted() @ turn @ world)
            bpy.context.view_layer.update()
    for view, off in (("front", Vector((0, -1, 0))), ("side", Vector((1, 0, 0))), ("back", Vector((0, 1, 0)))):
        cam.location = mid + off * 3.0
        cam.rotation_euler = (-off).to_track_quat("-Z", "Y").to_euler()
        scene.render.filepath = f"{out}_{k}_{view}.png"
        bpy.ops.render.render(write_still=True)
    print(f"EARPOSE {k}: back {back:g}, out {outw:g} -> {out}_{k}_*.png")
