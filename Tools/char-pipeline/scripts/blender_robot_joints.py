"""Cut the star robot's legs and arms (from blender_robot_parts.py) at the joints, so it can walk and fire its forearms
(run inside Blender).

Usage: blender -b --factory-startup --python blender_robot_joints.py -- <out dir> <parts dir> <knee z> <elbow z>
           <pelvis half y> <pelvis z>

Reads <parts dir>/SM_StarRobotLegs.fbx, SM_StarRobotArmL.fbx and SM_StarRobotArmR.fbx and cuts them, face by face (by
each face's middle; heights from the feet and sideways from the middle, in Unreal units), into
  SM_StarRobotPelvis           the legs' middle block: within <pelvis half y> of the middle, above <pelvis z>
  SM_StarRobotThighL/R         the rest of the legs above <knee z>, the robot's left (Blender's +Y) or right
  SM_StarRobotShinL/R          the legs below <knee z>
  SM_StarRobotUpperArmL/R      the arms above <elbow z>
  SM_StarRobotForearmL/R       the arms below <elbow z>: the rocket gauntlet and its fist
each keeping the robot's own pivot (placed at the robot's origin they make it up again). Prints each part's extent and
middle, in Unreal units (Unreal's Y is Blender's -Y).
"""
import sys

import bmesh
import bpy
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
out_dir, parts_dir = args[0], args[1]
knee, elbow, pelvis_y, pelvis_z = (float(a) / 100.0 for a in args[2:6])

bpy.ops.wm.read_factory_settings(use_empty=True)


def load(name):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=f"{parts_dir}/{name}.fbx")
    obj = next(o for o in set(bpy.data.objects) - before if o.type == "MESH")
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    obj.select_set(False)
    return obj


def bounds(o):
    pts = [v.co for v in o.data.vertices]
    return (Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts))),
            Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts))))


def export(o, name):
    bpy.ops.object.select_all(action="DESELECT")
    o.select_set(True)
    bpy.context.view_layer.objects.active = o
    o.name = name
    bpy.ops.export_scene.fbx(filepath=f"{out_dir}/{name}.fbx", use_selection=True, object_types={"MESH"},
                             mesh_smooth_type="FACE", path_mode="STRIP", bake_anim=False)


def split(obj, label_of, names):
    labels = [label_of(p.center) for p in obj.data.polygons]
    for part in names:
        copy = obj.copy()
        copy.data = obj.data.copy()
        bpy.context.scene.collection.objects.link(copy)
        bm = bmesh.new()
        bm.from_mesh(copy.data)
        bm.faces.ensure_lookup_table()
        bmesh.ops.delete(bm, geom=[f for f, label in zip(bm.faces, labels) if label != part], context="FACES")
        bm.to_mesh(copy.data)
        bm.free()
        copy.data.update()
        if not copy.data.vertices:
            print(f"JOINTS part {part}: empty")
            continue
        lo, hi = bounds(copy)
        mid = (lo + hi) / 2
        print(f"JOINTS part {part}: {len(copy.data.polygons)} faces, x {lo.x * 100:.0f}..{hi.x * 100:.0f}, "
              f"y(unreal) {-hi.y * 100:.0f}..{-lo.y * 100:.0f}, z {lo.z * 100:.0f}..{hi.z * 100:.0f}, "
              f"middle ({mid.x * 100:.0f}, {-mid.y * 100:.0f}, {mid.z * 100:.0f})")
        export(copy, f"SM_StarRobot{part}")


def leg_label(c):
    if abs(c.y) < pelvis_y and c.z > pelvis_z:
        return "Pelvis"
    side = "L" if c.y > 0 else "R"
    return ("Thigh" if c.z >= knee else "Shin") + side


split(load("SM_StarRobotLegs"), leg_label, ["Pelvis", "ThighL", "ThighR", "ShinL", "ShinR"])
for side in ("L", "R"):
    split(load(f"SM_StarRobotArm{side}"), lambda c, s=side: ("UpperArm" if c.z >= elbow else "Forearm") + s,
          [f"UpperArm{side}", f"Forearm{side}"])
