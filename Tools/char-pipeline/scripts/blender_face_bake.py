"""Carry an animal friend's painted face from its old model's texture onto a newly generated model's texture, surface to
surface (run in Blender, Cycles bake; 2026-11-24, the fox generated again with a straight tail).

Usage: blender -b --factory-startup --python blender_face_bake.py -- <old rig fbx> <new rig fbx> <new basecolor> <out folder>
           <name>=<old texture> [<name>=<old texture> ...] [--face 0.50,0.80,0.20[,0.5]] [--feather 0.03] [--size 4096]
           [--reach 0.05] [--no-shift]
  <new rig fbx>: blender_reskin.py's (the new mesh already lies on the old one: both in the same rest pose).
  Each <old texture> (the base face, every expression) is baked from the old mesh onto the new mesh's UVs, and pasted
  into the new base colour where the face is: the front of the head between the --face heights (shares of the height)
  within that half width of the middle line (the fur's --face-fur numbers), fading out over --feather. Outside it the
  new texture stays. The old colours are shifted by the mean difference between old and new fur round the face's edge
  (--no-shift: not), so the face does not sit on the head as a patch of another orange.
  Writes <out folder>/<name>.png and face_mask.png. Prints FACEBAKE lines.
"""
import os
import sys

import bmesh
import bpy
import numpy as np

args = sys.argv[sys.argv.index("--") + 1:]
old_fbx, new_fbx, new_tex, out_dir = args[:4]
pairs = [a.split("=", 1) for a in args[4:] if "=" in a and not a.startswith("--")]


def opt(name, default):
    return type(default)(args[args.index(name) + 1]) if name in args else default


FACE = [float(c) for c in opt("--face", "0.50,0.80,0.20").split(",")]
FRONT = FACE[3] if len(FACE) > 3 else 0.5
FEATHER = opt("--feather", 0.03)
SIZE = opt("--size", 4096)
REACH = opt("--reach", 0.05)
os.makedirs(out_dir, exist_ok=True)


def smoothstep(e0, e1, x):
    t = max(0.0, min(1.0, (x - e0) / (e1 - e0)))
    return t * t * (3.0 - 2.0 * t)


def load(path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=path)
    added = [o for o in bpy.data.objects if o not in before]
    arm = next(o for o in added if o.type == "ARMATURE")
    arm.animation_data_clear()
    arm.data.pose_position = "REST"
    return arm, next(o for o in added if o.type == "MESH"), added


bpy.ops.wm.read_factory_settings(use_empty=True)
old_arm, old, old_all = load(old_fbx)
new_arm, new, new_all = load(new_fbx)
for o in old_all + new_all:
    if o.type == "MESH" and o not in (old, new):
        bpy.data.objects.remove(o)
scene = bpy.context.scene
scene.render.engine = "CYCLES"
scene.cycles.device = "CPU"
scene.cycles.samples = 1
scene.render.bake.margin = 12
scene.render.bake.use_clear = True


def emit(mesh, node_maker):
    """One material on the mesh: an emission of whatever node_maker gives, plus an image node to bake into."""
    mat = bpy.data.materials.new(mesh.name + "_bake")
    mat.use_nodes = True
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    nodes.clear()
    out = nodes.new("ShaderNodeOutputMaterial")
    em = nodes.new("ShaderNodeEmission")
    links.new(em.outputs[0], out.inputs[0])
    src = node_maker(nodes)
    if src is not None:
        links.new(src.outputs[0], em.inputs[0])
    mesh.data.materials.clear()
    mesh.data.materials.append(mat)
    return mat


# ---- the face's place on the new mesh, as a vertex colour ----
world = new.matrix_world
co = [world @ v.co for v in new.data.vertices]
zlo, zhi = min(p.z for p in co), max(p.z for p in co)
height = zhi - zlo
head_bone = next(b for b in new_arm.data.bones if b.name.split(":")[-1] == "Head")
hx = (new_arm.matrix_world @ head_bone.head_local).x
band = [p for p in co if zlo + FACE[0] * height < p.z < zlo + FACE[1] * height and abs(p.x - hx) < FACE[2] * height]
y_front, y_back = min(p.y for p in band), max(p.y for p in band)
y_mid = y_front + (y_back - y_front) * FRONT
weights = []
for p in co:
    w = smoothstep(zlo + FACE[0] * height - FEATHER, zlo + FACE[0] * height + FEATHER, p.z)
    w *= 1.0 - smoothstep(zlo + FACE[1] * height - FEATHER, zlo + FACE[1] * height + FEATHER, p.z)
    w *= 1.0 - smoothstep(FACE[2] * height - FEATHER, FACE[2] * height + FEATHER, abs(p.x - hx))
    w *= 1.0 - smoothstep(y_mid - FEATHER, y_mid + FEATHER, p.y)
    weights.append(w)
attr = new.data.color_attributes.new("face", "FLOAT_COLOR", "POINT")
for i, w in enumerate(weights):
    attr.data[i].color = (w, w, w, 1.0)
print(f"FACEBAKE face: {sum(1 for w in weights if w > 0.5)} of {len(weights)} vertices, x {hx:.3f}, heights {zlo + FACE[0] * height:.3f}..{zlo + FACE[1] * height:.3f}, front of y {y_mid:.3f}")


def bake_image(name):
    img = bpy.data.images.new(name, SIZE, SIZE, alpha=True, float_buffer=False)
    return img


def pixels(img):
    buf = np.empty(SIZE * SIZE * 4, dtype=np.float32)
    img.pixels.foreach_get(buf)
    return buf.reshape(SIZE, SIZE, 4)


def select(active, others=()):
    bpy.ops.object.select_all(action="DESELECT")
    for o in others:
        o.select_set(True)
    active.select_set(True)
    bpy.context.view_layer.objects.active = active


def target(mesh_mat, img):
    node = mesh_mat.node_tree.nodes.new("ShaderNodeTexImage")
    node.image = img
    mesh_mat.node_tree.nodes.active = node
    return node


def attribute(nodes):
    n = nodes.new("ShaderNodeAttribute")
    n.attribute_name = "face"
    return n


new_mat = emit(new, attribute)
mask_img = bake_image("mask")
mask_node = target(new_mat, mask_img)
select(new)
bpy.ops.object.bake(type="EMIT")
mask = pixels(mask_img)[:, :, 0].copy()
print(f"FACEBAKE mask: {int((mask > 0.5).sum())} texels of {SIZE * SIZE}")

base_img = bpy.data.images.load(new_tex)
if base_img.size[0] != SIZE:
    base_img.scale(SIZE, SIZE)
base = pixels(base_img)
ring = (mask > 0.05) & (mask < 0.5)
m3 = mask[:, :, None]
for name, tex in pairs:
    old_img = bpy.data.images.load(tex)

    def texture(nodes, image=old_img):
        n = nodes.new("ShaderNodeTexImage")
        n.image = image
        return n

    emit(old, texture)
    got = bake_image("got_" + name)
    mask_node.image = got
    select(new, [old])
    bpy.ops.object.bake(type="EMIT", use_selected_to_active=True, cage_extrusion=REACH, max_ray_distance=REACH * 2)
    px = pixels(got)
    hit = px[:, :, 3] > 0.5
    shift = np.zeros(3, np.float32)
    if "--no-shift" not in args and (ring & hit).sum() > 100:
        shift = (base[:, :, :3][ring & hit] - px[:, :, :3][ring & hit]).mean(0)
    w = m3 * hit[:, :, None]
    out = base.copy()
    out[:, :, :3] = base[:, :, :3] * (1.0 - w) + np.clip(px[:, :, :3] + shift, 0.0, 1.0) * w
    out[:, :, 3] = 1.0
    res = bpy.data.images.new("out_" + name, SIZE, SIZE, alpha=True)
    res.pixels.foreach_set(out.reshape(-1))
    res.filepath_raw = os.path.join(out_dir, name + ".png")
    res.file_format = "PNG"
    res.save()
    print(f"FACEBAKE {name}: {int((hit & (mask > 0.5)).sum())} of {int((mask > 0.5).sum())} face texels found on the old model, colour shift {np.round(shift, 3)}")
    bpy.data.images.remove(got)
    bpy.data.images.remove(res)
mask_out = bpy.data.images.new("mask_out", SIZE, SIZE)
mo = np.zeros((SIZE, SIZE, 4), np.float32)
mo[:, :, :3] = m3
mo[:, :, 3] = 1.0
mask_out.pixels.foreach_set(mo.reshape(-1))
mask_out.filepath_raw = os.path.join(out_dir, "face_mask.png")
mask_out.file_format = "PNG"
mask_out.save()
print(f"FACEBAKE wrote {out_dir}")
