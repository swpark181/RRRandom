"""Put a new Tripo take of a character on the rig of an earlier take (run in Blender): same skeleton, the earlier mesh's
skin weights carried over to the new mesh by where it lies on the old one, so the same Mixamo animations and game
tuning work as they were. For when the new mesh is very nearly the old one's shape (the shirtless dragon, made from the
same front picture as the T-shirt one: half its vertices within 1.5 % of the height of the old surface), and so when
Mixamo's auto-rigger can't be had (it failed on every mesh, the old dragon's too, on 2026-10-15).

Usage: blender -b --factory-startup --python blender_transfer_rig.py -- <old rig fbx> <old obj> <new obj> <out fbx>
       [preview png]
  <old rig fbx>: the old take's final rig (input/dragon/dragon_rig.fbx: Mixamo body + blender_animal_tail.py's tail and
  wing bones), its mesh the whole of <old obj> (blender_tripo_to_obj.py's). <new obj>: the new take's
  blender_tripo_to_obj.py OBJ.

The new OBJ goes into the rig's frame the way the old OBJ maps onto the old rig mesh (by the bounds), then is slid
(translation only) to sit best on the old surface. Each new vertex takes the weights of the nearest point of the old
surface (interpolated across that face), among the nearby faces facing its way, so the inside of an arm doesn't pick up
the body's side; four biggest kept, normalized. Written like blender_merge_parts.py writes. The preview renders the rest
pose and a few test poses (arms up, a step, the tail and wings swung) from the front and behind.
"""
import math
import sys

import bpy
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree
from mathutils.geometry import barycentric_transform

args = sys.argv[sys.argv.index("--") + 1:]
old_fbx, old_obj, new_obj, out = args[:4]
preview = args[4] if len(args) > 4 else None
MAX_INFLUENCES = 4

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=old_fbx)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
arm.animation_data_clear()
for action in list(bpy.data.actions):
    bpy.data.actions.remove(action)
arm.data.pose_position = "REST"
old = next(o for o in bpy.data.objects if o.type == "MESH")
old_pts = [old.matrix_world @ v.co for v in old.data.vertices]


def bounds(ps):
    return (Vector((min(p.x for p in ps), min(p.y for p in ps), min(p.z for p in ps))),
            Vector((max(p.x for p in ps), max(p.y for p in ps), max(p.z for p in ps))))


def load_obj(path):
    before = set(bpy.data.objects)
    bpy.ops.wm.obj_import(filepath=path, forward_axis="NEGATIVE_Z", up_axis="Y")
    return next(o for o in bpy.data.objects if o not in before)


ref = load_obj(old_obj)
lo_r, hi_r = bounds(old_pts)
lo_o, hi_o = bounds([ref.matrix_world @ v.co for v in ref.data.vertices])
scale = (hi_r.z - lo_r.z) / (hi_o.z - lo_o.z)
to_rig = Matrix.Translation(lo_r) @ Matrix.Scale(scale, 4) @ Matrix.Translation(-lo_o)
bpy.data.objects.remove(ref, do_unlink=True)
height = hi_r.z - lo_r.z

tris = [tuple(p.vertices) for p in old.data.polygons]
tree = BVHTree.FromPolygons(old_pts, tris)
new = load_obj(new_obj)
new.data.transform(to_rig @ new.matrix_world)
new.matrix_world = Matrix.Identity(4)
# Slide it onto the old surface: the mean offset to the nearest old point, over the vertices already close
for step in range(8):
    offs = []
    for v in new.data.vertices:
        hit = tree.find_nearest(v.co, 0.04 * height)
        if hit[0] is not None:
            offs.append(hit[0] - v.co)
    shift = sum(offs, Vector()) / max(1, len(offs))
    new.data.transform(Matrix.Translation(shift))
    if shift.length < 1e-4 * height:
        break
dists = sorted(tree.find_nearest(v.co)[3] / height for v in new.data.vertices)
print(f"TRANSFER x{scale:.5f}, slid {step + 1} times; off the old surface (of the height): median {dists[len(dists) // 2]:.4f}"
      f" p90 {dists[int(len(dists) * 0.9)]:.4f} max {dists[-1]:.4f}")

# Weights: from the nearest old face that faces the same way (within a little more than the nearest distance)
new.data.update()
old_groups = {g.index: g.name for g in old.vertex_groups}
weights_of = [{old_groups[g.group]: g.weight for g in v.groups} for v in old.data.vertices]
groups = {name: new.vertex_groups.new(name=name) for name in old_groups.values()}
turned = 0
for v in new.data.vertices:
    loc, normal, face, dist = tree.find_nearest(v.co)
    if normal.dot(v.normal) < 0.2:
        best = None
        for l2, n2, f2, d2 in tree.find_nearest_range(v.co, dist * 1.6 + 0.01 * height):
            if n2.dot(v.normal) >= 0.2 and (best is None or d2 < best[3]):
                best = (l2, n2, f2, d2)
        if best:
            loc, normal, face, dist = best
            turned += 1
    corners = tris[face][:3]
    a, b, c = (old_pts[i] for i in corners)
    # Barycentric weights of loc in the triangle
    bary = barycentric_transform(loc, a, b, c, Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1)))
    mix = {}
    for i, share in zip(corners, bary):
        share = max(0.0, share)
        for name, w in weights_of[i].items():
            mix[name] = mix.get(name, 0.0) + w * share
    top = sorted(mix.items(), key=lambda kv: -kv[1])[:MAX_INFLUENCES]
    total = sum(w for _, w in top)
    for name, w in top:
        if w / total > 0.005:
            groups[name].add([v.index], w / total, "REPLACE")
print(f"TRANSFER {len(new.data.vertices)} vertices weighted ({turned} from a face behind the nearest one)")

# The new mesh takes the old one's place on the armature
old_name = old.name
bpy.data.objects.remove(old, do_unlink=True)
new.name = old_name
new.parent = arm
new.matrix_parent_inverse = arm.matrix_world.inverted()
mod = new.modifiers.new("Armature", "ARMATURE")
mod.object = arm
bpy.ops.export_scene.fbx(
    filepath=out, use_selection=False, object_types={"ARMATURE", "MESH"}, add_leaf_bones=False, bake_anim=False,
    primary_bone_axis="Y", secondary_bone_axis="X", use_armature_deform_only=False, mesh_smooth_type="OFF",
    path_mode="STRIP")
print(f"TRANSFER wrote {out}")

if preview:
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = scene.render.resolution_y = 500
    sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
    sun.data.energy = 3.0
    scene.collection.objects.link(sun)
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    scene.collection.objects.link(cam)
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = height * 1.5
    scene.camera = cam
    mid = (lo_r + hi_r) / 2
    arm.data.pose_position = "POSE"
    pb = arm.pose.bones
    prefix = next(b.name for b in pb if b.name.endswith("Hips")).replace("Hips", "")

    def turn(bone, axis, deg):
        b = pb.get(prefix + bone)
        if b:
            b.rotation_mode = "XYZ"
            setattr(b.rotation_euler, axis, math.radians(deg))

    poses = {
        "rest": [],
        "pose": [("LeftArm", "x", -60), ("RightArm", "x", -60), ("LeftForeArm", "z", 40), ("RightForeArm", "z", -40),
                 ("LeftUpLeg", "x", -45), ("LeftLeg", "x", 60), ("RightUpLeg", "x", 20), ("Spine1", "y", 15),
                 ("Tail1", "z", 25), ("Tail2", "z", 25), ("Tail3", "z", 25), ("WingL1", "y", 30), ("WingR1", "y", -30),
                 ("Head", "x", 15)],
    }
    for pose, turns in poses.items():
        for b in pb:
            b.rotation_mode = "XYZ"
            b.rotation_euler = (0, 0, 0)
        for t in turns:
            turn(*t)
        bpy.context.view_layer.update()
        for name, yaw in (("front", 0), ("back", 180), ("side", 90)):
            r = math.radians(yaw)
            cam.location = Vector((mid.x + math.sin(r) * 5 * height, mid.y - math.cos(r) * 5 * height, mid.z))
            cam.rotation_euler = (math.radians(90), 0, r)
            cam.data.clip_end = 20 * height
            sun.rotation_euler = (math.radians(50), 0, r + math.radians(20))
            scene.render.filepath = f"{preview}_{pose}_{name}.png"
            bpy.ops.render.render(write_still=True)
    print("TRANSFER preview done")
