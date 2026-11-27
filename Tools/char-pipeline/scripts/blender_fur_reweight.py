"""Give an existing shell-fur FBX the bones a rig gained (ears) and the weights its skin has there, without making the
fur again (2026-11-25, ear bones; the fur's shape, colours and UVs stay exactly as they were).

Usage: blender -b -P blender_fur_reweight.py -- <rig fbx with the new bones> <fur fbx> <out fbx> [--bones Ear]

Every fur vertex takes the weights of the nearest vertex of the rig's mesh (the fur grows from that skin, at most a
fur's length off it) when that skin vertex is weighted to bones whose names contain --bones; the others keep theirs.
The fur's armature gets the rig's new bones (same place, roll and parent). Exported as blender_fur_shells_skel.py
exports (vertex colours and the second UV map kept).
"""
import sys

import bpy
from mathutils import kdtree

args = sys.argv[sys.argv.index("--") + 1:]
rig_fbx, fur_fbx, out_fbx = args[:3]
KEY = args[args.index("--bones") + 1] if "--bones" in args else "Ear"

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=rig_fbx)
rig_arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
rig_meshes = [o for o in bpy.data.objects if o.type == "MESH"]
rig_arm.data.pose_position = "REST"
new_bones = [b.name for b in rig_arm.data.bones if KEY in b.name]
bone_rest = {b.name: (b.head_local.copy(), b.tail_local.copy(), b.matrix_local.copy(), b.parent.name, b.use_connect, b.use_deform)
             for b in rig_arm.data.bones if b.name in new_bones}
skin = []   # (world position, {group name: weight}) for the rig's vertices weighted to the new bones
for m in rig_meshes:
    names = {g.index: g.name for g in m.vertex_groups}
    for v in m.data.vertices:
        ws = {names[g.group]: g.weight for g in v.groups if g.weight > 0.0}
        skin.append((m.matrix_world @ v.co, ws))
tree = kdtree.KDTree(len(skin))
for i, (p, _) in enumerate(skin):
    tree.insert(p, i)
tree.balance()
rig_world = rig_arm.matrix_world.copy()

before = set(bpy.data.objects)
bpy.ops.import_scene.fbx(filepath=fur_fbx, colors_type="LINEAR")   # (as written: the fur script exports linear)
fur_arm = next(o for o in bpy.data.objects if o not in before and o.type == "ARMATURE")
fur = next(o for o in bpy.data.objects if o not in before and o.type == "MESH")
for o in list(before):
    bpy.data.objects.remove(o)
# (the fur's armature came in as "Armature.001" beside the rig's: Unreal takes a root not named "Armature" for a bone
# of its own and the skeleton's reference pose moved 38 cm - 2026-11-25, restored from the backup)
fur_arm.name = "Armature"
fur_arm.data.name = "Armature"
fur_arm.animation_data_clear()
fur_arm.data.pose_position = "REST"
print(f"FURWEIGHT fur {fur.name}: {len(fur.data.vertices)} vertices, colours {[a.name for a in fur.data.color_attributes]}, "
      f"uv {[u.name for u in fur.data.uv_layers]}; new bones {new_bones}")

# the bones, in the fur armature's own frame
bpy.context.view_layer.objects.active = fur_arm
bpy.ops.object.mode_set(mode="EDIT")
to_fur = fur_arm.matrix_world.inverted() @ rig_world
eb = fur_arm.data.edit_bones
for n in new_bones:
    if n in eb:
        eb.remove(eb[n])
for n in new_bones:   # (parents come first in the rig's order)
    h, t, ml, parent, connect, deform = bone_rest[n]
    b = eb.new(n)
    b.head, b.tail = to_fur @ h, to_fur @ t
    z = (to_fur.to_3x3() @ ml.to_3x3().col[2]).normalized()
    b.align_roll(z)
    b.parent = eb[parent]
    b.use_connect = connect
    b.use_deform = deform
bpy.ops.object.mode_set(mode="OBJECT")

for n in new_bones:
    if n not in fur.vertex_groups:
        fur.vertex_groups.new(name=n)
gi = {g.name: g.index for g in fur.vertex_groups}
groups = list(fur.vertex_groups)
changed = 0
mw = fur.matrix_world
for v in fur.data.vertices:
    _, idx, d = tree.find(mw @ v.co)
    ws = skin[idx][1]
    if not any(KEY in n for n in ws):
        continue
    for g in [g.group for g in v.groups]:
        groups[g].remove([v.index])
    for n, w in ws.items():
        if n not in gi:
            fur.vertex_groups.new(name=n)
            gi = {g.name: g.index for g in fur.vertex_groups}
            groups = list(fur.vertex_groups)
        groups[gi[n]].add([v.index], w, "REPLACE")
    changed += 1
print(f"FURWEIGHT {changed} fur vertices took the ear skin's weights")
bpy.ops.export_scene.fbx(
    filepath=out_fbx, use_selection=False, object_types={"ARMATURE", "MESH"}, add_leaf_bones=False, bake_anim=False,
    primary_bone_axis="Y", secondary_bone_axis="X", use_armature_deform_only=False, mesh_smooth_type="OFF",
    path_mode="STRIP", colors_type="LINEAR")
print(f"FURWEIGHT wrote {out_fbx}: {len(fur_arm.data.bones)} bones")
