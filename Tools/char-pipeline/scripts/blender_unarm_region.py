"""Take the arm bones' skin weights off the waist (pouches, belt, shorts) of a rig (run in Blender).

Round 77, the film-style tiger: Mixamo's auto-rigger refused its body, so blender_fit_rig.py fitted the fluffy tiger's
rig, and the limb reskin gave the belt pouches (which hang right beside the paws) to the hands: raising an arm pulled a
strip of pouch and shorts after it. Here every vertex inside a box around the middle (not out where the arms hang) loses
its arm-bone weights; what it had on other bones is renormalised, and a vertex left with nothing goes to <fallback>.

Usage: blender -b --factory-startup --python blender_unarm_region.py -- <rig fbx> <out fbx> <cx> <half width> <z max>
       [fallback Hips]
  In the rig's Blender frame (m; faces -Y, +X its left, Z up): vertices with |x - cx| < half width and z < z max.
  Below z max, of the arm-weighted pieces only the two biggest (the arms) keep their arm weights.
  Arm bones = names containing Shoulder, Arm, ForeArm, Hand (fingers included). Written like blender_animal_tail.py writes.
"""
import sys

import bpy

args = sys.argv[sys.argv.index("--") + 1:]
src, out, cx, half, zmax = args[0], args[1], float(args[2]), float(args[3]), float(args[4])
fallback = args[5] if len(args) > 5 else "Hips"

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
mesh = next(o for o in bpy.data.objects if o.type == "MESH")
names = {g.index: g.name for g in mesh.vertex_groups}
is_arm = {i: any(k in n for k in ("Shoulder", "Arm", "Hand")) for i, n in names.items()}
fb = next(g for g in mesh.vertex_groups if g.name.endswith(fallback))
mw = mesh.matrix_world
# Below z max, the arm-weighted vertices split into pieces joined by mesh edges: the two biggest are the arms and paws,
# smaller ones (a pouch's outer face, a bit of belt) are what the reskin wrongly gave the hands. (A capsule round the
# forearm and hand bones was tried first: the paws are thicker than any radius that leaves the pouches out.)
import bmesh
P = [mw @ v.co for v in mesh.data.vertices]
armed = {v.index for v in mesh.data.vertices
         if P[v.index].z < zmax and any(is_arm.get(g.group) and g.weight > 0 for g in v.groups)}
bm = bmesh.new()
bm.from_mesh(mesh.data)
bm.verts.ensure_lookup_table()
seen, pieces = set(), []
for s in armed:
    if s in seen:
        continue
    stack, piece = [s], []
    seen.add(s)
    while stack:
        a = stack.pop()
        piece.append(a)
        for e in bm.verts[a].link_edges:
            b = e.other_vert(bm.verts[a]).index
            if b in armed and b not in seen:
                seen.add(b)
                stack.append(b)
    pieces.append(piece)
pieces.sort(key=len, reverse=True)
keep = {i for p in pieces[:2] for i in p}
print(f"UNARM arm pieces below z {zmax}: {[len(p) for p in pieces[:6]]} (the 2 biggest kept)")

changed = 0
for v in mesh.data.vertices:
    p = P[v.index]
    if p.z >= zmax or v.index in keep:
        continue
    if abs(p.x - cx) >= half and v.index not in armed:
        continue
    arm_w = [(g.group, g.weight) for g in v.groups if is_arm.get(g.group) and g.weight > 0]
    if not arm_w:
        continue
    rest = [(g.group, g.weight) for g in v.groups if not is_arm.get(g.group) and g.weight > 0]
    for gi, _ in arm_w:
        mesh.vertex_groups[gi].remove([v.index])
    total = sum(w for _, w in rest)
    if total > 1e-6:
        for gi, w in rest:
            mesh.vertex_groups[gi].add([v.index], w / total, "REPLACE")
    else:
        fb.add([v.index], 1.0, "REPLACE")
    changed += 1
print(f"UNARM {changed} vertices lost their arm weights (|x-{cx}|<{half}, z<{zmax})")
for o in bpy.data.objects:
    o.select_set(True)
bpy.ops.export_scene.fbx(
    filepath=out, use_selection=False, object_types={"ARMATURE", "MESH"}, add_leaf_bones=False, bake_anim=False,
    primary_bone_axis="Y", secondary_bone_axis="X", use_armature_deform_only=False, mesh_smooth_type="OFF",
    path_mode="STRIP")
print(f"UNARM wrote {out}")
