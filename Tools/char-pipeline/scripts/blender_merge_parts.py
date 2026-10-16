"""Put a character's whole mesh back on the rig Mixamo made for its body alone (run in Blender).

Usage: blender -b --factory-startup --python blender_merge_parts.py -- <body SK fbx> <body obj> <whole obj> <out fbx>
       [ROOTS]
  <body SK fbx>: the Mixamo download (with skin) of <body obj> (blender_strip_parts.py's cut of <whole obj>).

The whole mesh is brought into the rig's frame the way the body OBJ maps onto the body in the FBX (by the bounds, the
two are the same vertices). The body's own vertices get exactly Mixamo's weights. The cut parts (tail, wings) wait for
blender_animal_tail.py to give them bones of their own; until then, and for the bits of them it leaves out, they follow
the bone they grow from, by height: ROOTS "Hips,0.35,Spine2" puts what was cut below 35 cm on Hips and the rest on
Spine2 (without ROOTS they take the nearest body vertex's weights, which for the dragon's tail lying by its feet were
the legs', and the tail's underside stretched with every step). Written like blender_animal_tail.py writes (no animation, no leaf bones).
"""
import sys

import bpy
from mathutils import Matrix, Vector
from mathutils.kdtree import KDTree

args = sys.argv[sys.argv.index("--") + 1:]
body_fbx, body_obj, whole_obj, out = args[:4]
ROOTS = args[4].split(",") if len(args) > 4 else []
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=body_fbx)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
arm.animation_data_clear()
for action in list(bpy.data.actions):
    bpy.data.actions.remove(action)
arm.data.pose_position = "REST"
body = next(o for o in bpy.data.objects if o.type == "MESH")
body_pts = [body.matrix_world @ v.co for v in body.data.vertices]


def bounds(ps):
    return (Vector((min(p.x for p in ps), min(p.y for p in ps), min(p.z for p in ps))),
            Vector((max(p.x for p in ps), max(p.y for p in ps), max(p.z for p in ps))))


def load_obj(path):
    before = set(bpy.data.objects)
    bpy.ops.wm.obj_import(filepath=path, forward_axis="NEGATIVE_Z", up_axis="Y")
    return next(o for o in bpy.data.objects if o not in before)


cut_body = load_obj(body_obj)
lo_b, hi_b = bounds(body_pts)
lo_o, hi_o = bounds([cut_body.matrix_world @ v.co for v in cut_body.data.vertices])
scale = (hi_b.z - lo_b.z) / (hi_o.z - lo_o.z)
bpy.data.objects.remove(cut_body, do_unlink=True)
whole = load_obj(whole_obj)
# Into the rig's frame, baked into the vertices
to_rig = Matrix.Translation(lo_b) @ Matrix.Scale(scale, 4) @ Matrix.Translation(-lo_o) @ whole.matrix_world
whole.data.transform(to_rig)
whole.matrix_world = Matrix.Identity(4)

tree = KDTree(len(body_pts))
for i, p in enumerate(body_pts):
    tree.insert(p, i)
tree.balance()
names = {g.index: g.name for g in body.vertex_groups}
groups = {name: whole.vertex_groups.new(name=name) for name in names.values()}
prefix = next(n for n in names.values() if n.endswith("Hips")).replace("Hips", "")
exact = 0
for v in whole.data.vertices:
    co, j, dist = tree.find(v.co)
    if dist < 1e-4:
        exact += 1
    elif ROOTS:
        root = ROOTS[-1]
        for i in range(0, len(ROOTS) - 1, 2):
            if v.co.z < float(ROOTS[i + 1]):
                root = ROOTS[i]
                break
        groups[prefix + root].add([v.index], 1.0, "REPLACE")
        continue
    for g in body.data.vertices[j].groups:
        groups[names[g.group]].add([v.index], g.weight, "REPLACE")
print(f"MERGE {len(whole.data.vertices)} vertices, {exact} on a body vertex (body has {len(body_pts)}), x{scale:.4f}")

# The body mesh goes, the whole one takes its place on the armature
bpy.data.objects.remove(body, do_unlink=True)
whole.parent = arm
whole.matrix_parent_inverse = arm.matrix_world.inverted()
mod = whole.modifiers.new("Armature", "ARMATURE")
mod.object = arm
lo, hi = bounds([whole.matrix_world @ v.co for v in whole.data.vertices])
print(f"MERGE bounds x {lo.x:+.3f}..{hi.x:+.3f} y {lo.y:+.3f}..{hi.y:+.3f} z {lo.z:+.3f}..{hi.z:+.3f}")
bpy.ops.export_scene.fbx(
    filepath=out, use_selection=False, object_types={"ARMATURE", "MESH"}, add_leaf_bones=False, bake_anim=False,
    primary_bone_axis="Y", secondary_bone_axis="X", use_armature_deform_only=False, mesh_smooth_type="OFF",
    path_mode="STRIP")
print(f"MERGE wrote {out}")
