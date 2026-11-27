"""Reference pictures of a rigged animal friend's tail alone, for making a new tail from them in Tripo (multiview image to
3D), and the tail itself as an OBJ to fit the new one to (run in Blender; 2026-11-24, the fox).

Usage: blender -b --factory-startup --python blender_tail_refs.py -- <rig fbx> <basecolor png> <out folder> [--share 0.5] [--px 1024]
  <rig fbx>: a rig whose tail is laid out straight (blender_tail_straight.py). The tail = the faces whose corners are all
  more than --share the tail bones'. It is turned a quarter turn so that it lies across the picture (root on the left,
  tip on the right, seen from the animal friend's right side): Tripo's front view. Writes tail_front|left|back|right.png
  (Tripo's names: left = seen from the object's own left, the tip's end; right = the root's end), tail_ref.obj (the tail
  where it is on the animal friend, meters) and tail_ref.json (the turn used, the root ring's middle).
"""
import json
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

args = sys.argv[sys.argv.index("--") + 1:]
src, tex, out = args[0], args[1], args[2]


def opt(name, default):
    return type(default)(args[args.index(name) + 1]) if name in args else default


SHARE = opt("--share", 0.5)
PX = opt("--px", 1024)
os.makedirs(out, exist_ok=True)

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
mesh = next(o for o in bpy.data.objects if o.type == "MESH")
for o in list(bpy.data.objects):
    if o.type == "MESH" and o is not mesh:
        bpy.data.objects.remove(o)
tail_groups = {g.index for g in mesh.vertex_groups if "Tail" in g.name}
world = mesh.matrix_world.copy()
bm = bmesh.new()
bm.from_mesh(mesh.data)
deform = bm.verts.layers.deform.verify()
uv = bm.loops.layers.uv.verify()


def tail_share(v):
    ws = v[deform]
    total = sum(ws.values())
    return sum(w for g, w in ws.items() if g in tail_groups) / total if total > 0.0 else 0.0


keep = [f for f in bm.faces if all(tail_share(v) > SHARE for v in f.verts)]
bmesh.ops.delete(bm, geom=[f for f in bm.faces if f not in set(keep)], context="FACES")
bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")
# close the root with a fan in the ring's own colour
ring = [e for e in bm.edges if e.is_boundary]
ring_verts = {v for e in ring for v in e.verts}
root = sum((world @ v.co for v in ring_verts), Vector()) / len(ring_verts)
centre = bm.verts.new(world.inverted() @ root)
for e in ring:
    loop = e.link_loops[0]
    a, b = loop.vert, loop.link_loop_next.vert
    face = bm.faces.new((b, a, centre))
    face.smooth = True
    for l in face.loops:
        l[uv].uv = (loop.link_loop_next[uv].uv if l.vert is b else loop[uv].uv).copy()
for v in bm.verts:
    v.co = world @ v.co
bm.normal_update()
tail_me = bpy.data.meshes.new("TailRef")
bm.to_mesh(tail_me)
bm.free()
for m in mesh.data.materials:
    tail_me.materials.append(m)
tail = bpy.data.objects.new("TailRef", tail_me)
bpy.context.scene.collection.objects.link(tail)
bpy.data.objects.remove(mesh)
bpy.data.objects.remove(arm)

co = [v.co.copy() for v in tail_me.vertices]
lo = Vector((min(p.x for p in co), min(p.y for p in co), min(p.z for p in co)))
hi = Vector((max(p.x for p in co), max(p.y for p in co), max(p.z for p in co)))
print(f"TAILREF tail {len(co)} vertices, root ring {len(ring)} edges at {tuple(round(c, 3) for c in root)}, "
      f"bounds {tuple(round(c, 3) for c in lo)} .. {tuple(round(c, 3) for c in hi)}")
bpy.ops.object.select_all(action="DESELECT")
tail.select_set(True)
bpy.context.view_layer.objects.active = tail
bpy.ops.wm.obj_export(filepath=os.path.join(out, "tail_ref.obj"), export_selected_objects=True, export_materials=False,
                      forward_axis="Y", up_axis="Z")

# (for the pictures only: rounded, so the new tail is not made as angular as the old one's few faces)
smooth = tail.modifiers.new("round", "SUBSURF")
smooth.levels = smooth.render_levels = 2
for poly in tail_me.polygons:
    poly.use_smooth = True

# The pictures: the tail turned so that its length (the animal friend's back, +Y) lies along +X
turn = Matrix.Rotation(math.radians(-90.0), 4, "Z")
tail.matrix_world = turn
mid = turn @ ((lo + hi) * 0.5)
span = max(hi.y - lo.y, hi.z - lo.z, hi.x - lo.x) * 1.12
image = bpy.data.images.load(tex)
for mat in tail_me.materials:
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    node = nodes.new("ShaderNodeTexImage")
    node.image = image
    nodes.active = node
scene = bpy.context.scene
scene.render.engine = "BLENDER_WORKBENCH"
shading = scene.display.shading
shading.light = "STUDIO"
shading.color_type = "TEXTURE"
shading.show_cavity = False
shading.show_object_outline = False
shading.show_shadows = False
shading.show_specular_highlight = False
scene.render.resolution_x = scene.render.resolution_y = PX
scene.world = bpy.data.worlds.new("w")
scene.world.color = (1.0, 1.0, 1.0)
scene.view_settings.view_transform = "Standard"
cam_data = bpy.data.cameras.new("cam")
cam_data.type = "ORTHO"
cam_data.ortho_scale = span
cam = bpy.data.objects.new("cam", cam_data)
scene.collection.objects.link(cam)
scene.camera = cam
VIEWS = {"front": Vector((0.0, -4.0, 0.0)), "back": Vector((0.0, 4.0, 0.0)), "left": Vector((4.0, 0.0, 0.0)), "right": Vector((-4.0, 0.0, 0.0))}
for name, offset in VIEWS.items():
    forward = (-offset).normalized()
    right = forward.cross(Vector((0.0, 0.0, 1.0))).normalized()
    m = Matrix((right, right.cross(forward), -forward)).transposed().to_4x4()
    m.translation = mid + offset
    cam.matrix_world = m
    scene.render.filepath = os.path.join(out, f"tail_{name}.png")
    bpy.ops.render.render(write_still=True)
json.dump({"turn_z_deg": -90.0, "root": list(root), "lo": list(lo), "hi": list(hi), "span": span, "rig": src},
          open(os.path.join(out, "tail_ref.json"), "w"), indent=1)
print(f"TAILREF wrote {out}")
