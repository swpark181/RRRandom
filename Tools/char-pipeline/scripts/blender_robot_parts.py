"""Tidy the Tripo combiner robot and cut it into the parts that fly together when it combines (run inside Blender).

Usage: blender -b --factory-startup --python blender_robot_parts.py -- <out dir> <tripo fbx> <height> <turn>
           [<hip z> <neck z> <arm y> [<low arm y>]]

The robot: all meshes joined, turned <turn> degrees about Z so its front is on +X (Unreal's forward), pivot in the
middle of its bottom, scaled to <height> Unreal units, decimated to about TARGET_TRIS triangles, written as
<out dir>/SM_StarRobot.fbx. With the cuts (in those units: heights from its feet, sideways from its middle) it is also
cut, face by face (by each face's middle), into
  SM_StarRobotArmL/R  further out than <arm y> above the hips, or <low arm y> below them (the fists hang by the legs)
  SM_StarRobotLegs    below <hip z>
  SM_StarRobotHead    above <neck z>
  SM_StarRobotTorso   the rest
each keeping the robot's own pivot, so placed at the robot's origin they make it up again. ArmL is on Blender's +Y:
facing +X, that is the robot's left (Blender's +Y is left of +X; the FBX export flips it to Unreal's -Y, which is
the left there too). Prints each part's extent.
"""
import math
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

args = sys.argv[sys.argv.index("--") + 1:]
out_dir, fbx, height, turn = args[0], args[1], float(args[2]), float(args[3])
cuts = [float(a) for a in args[4:]]
TARGET_TRIS = 16000

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=fbx)
meshes = [o for o in bpy.data.objects if o.type == "MESH"]
for o in list(bpy.data.objects):
    if o.type != "MESH":
        bpy.data.objects.remove(o, do_unlink=True)
bpy.ops.object.select_all(action="DESELECT")
for o in meshes:
    o.select_set(True)
bpy.context.view_layer.objects.active = meshes[0]
if len(meshes) > 1:
    bpy.ops.object.join()
obj = bpy.context.view_layer.objects.active
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
obj.data.transform(Matrix.Rotation(math.radians(turn), 4, "Z"))


def bounds(o):
    pts = [v.co for v in o.data.vertices]
    return (Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts))),
            Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts))))


lo, hi = bounds(obj)
offset = Vector(((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, lo.z))
scale = height / 100.0 / max(hi.z - lo.z, 1e-6)
for v in obj.data.vertices:
    v.co = (v.co - offset) * scale
obj.data.update()

tris = sum(len(p.vertices) - 2 for p in obj.data.polygons)
if tris > TARGET_TRIS:
    mod = obj.modifiers.new("Decimate", "DECIMATE")
    mod.ratio = TARGET_TRIS / tris
    bpy.ops.object.modifier_apply(modifier=mod.name)
lo, hi = bounds(obj)
print(f"ROBOT size {(hi - lo).x * 100:.0f} x {(hi - lo).y * 100:.0f} x {(hi - lo).z * 100:.0f}, "
      f"{sum(len(p.vertices) - 2 for p in obj.data.polygons)} tris")


def export(o, name):
    bpy.ops.object.select_all(action="DESELECT")
    o.select_set(True)
    bpy.context.view_layer.objects.active = o
    o.name = name
    bpy.ops.export_scene.fbx(filepath=f"{out_dir}/{name}.fbx", use_selection=True, object_types={"MESH"},
                             mesh_smooth_type="FACE", path_mode="STRIP", bake_anim=False)


export(obj, "SM_StarRobot")
if len(cuts) < 3:
    sys.exit(0)

hip, neck, arm = (c / 100.0 for c in cuts[:3])
# The fists hang down past the hips, out beside the legs
arm_low = cuts[3] / 100.0 if len(cuts) > 3 else arm * 2


def part_of(center):
    if center.z < hip:
        # The legs' flaps reach out to <low arm y> only near the feet; up by the thighs the rocket arms come closer in
        reach = arm_low if center.z < hip * 0.65 else arm * 1.35
        return "Legs" if abs(center.y) <= reach else ("ArmL" if center.y > 0 else "ArmR")
    if center.y > arm:
        return "ArmL"
    if center.y < -arm:
        return "ArmR"
    if center.z > neck:
        return "Head"
    return "Torso"


labels = [part_of(p.center) for p in obj.data.polygons]
for part in ("Legs", "Torso", "Head", "ArmL", "ArmR"):
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
    plo, phi = bounds(copy)
    print(f"ROBOT part {part}: {len(copy.data.polygons)} faces, x {plo.x * 100:.0f}..{phi.x * 100:.0f}, "
          f"y {plo.y * 100:.0f}..{phi.y * 100:.0f}, z {plo.z * 100:.0f}..{phi.z * 100:.0f}")
    export(copy, f"SM_StarRobot{part}")
