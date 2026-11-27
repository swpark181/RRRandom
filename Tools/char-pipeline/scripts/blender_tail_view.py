"""Render a rigged animal friend's tail from behind, above, the right and three-quarters, textured, at rest or bent the way
the game bends it (run in Blender; 2026-11-24, to judge blender_tail_part.py's tails before they go into Unreal).

Usage: blender -b --factory-startup --python blender_tail_view.py -- <rig fbx> <basecolor png> <out prefix> [--swing 0] [--lift 0]
           [--share root|even|tip] [--centre x,y,z] [--span 0.9] [--px 700] [--wire]
  --swing / --lift: degrees in all, shared over the tail bones (root: the root most, like the game's chain; even; tip),
  each bone turned in the mesh's space about its own head: to the animal friend's right about the up axis, up about
  the left-right axis. Writes <out prefix>_back.png, _top.png (the head up the picture), _right.png, _quarter.png.
"""
import math
import sys

import bpy
from mathutils import Matrix, Vector

args = sys.argv[sys.argv.index("--") + 1:]
src, tex, out = args[0], args[1], args[2]


def opt(name, default):
    return type(default)(args[args.index(name) + 1]) if name in args else default


SWING, LIFT = opt("--swing", 0.0), opt("--lift", 0.0)
SHARE = opt("--share", "root")
CENTRE = Vector(tuple(float(c) for c in opt("--centre", "0.0,0.08,0.22").split(",")))
SPAN = opt("--span", 0.9)
PX = opt("--px", 700)

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
arm.animation_data_clear()
meshes = [o for o in bpy.data.objects if o.type == "MESH"]
prefix = next(b.name for b in arm.data.bones if b.name.endswith("Hips")).replace("Hips", "")

image = bpy.data.images.load(tex)
for mesh in meshes:
    for mat in mesh.data.materials:
        mat.use_nodes = True
        nodes = mat.node_tree.nodes
        node = nodes.new("ShaderNodeTexImage")
        node.image = image
        nodes.active = node
        bsdf = next((n for n in nodes if n.type == "BSDF_PRINCIPLED"), None)
        if bsdf:
            mat.node_tree.links.new(node.outputs["Color"], bsdf.inputs["Base Color"])

tails = sorted((n for n in arm.pose.bones.keys() if n.startswith(prefix + "Tail")), key=lambda n: int(n.replace(prefix + "Tail", "")))
count = len(tails)
if SHARE == "root":
    shares = [(count - i) / (count * (count + 1) / 2) for i in range(count)]
elif SHARE == "tip":
    shares = [(i + 1) / (count * (count + 1) / 2) for i in range(count)]
else:
    shares = [1.0 / count] * count
for name, share in zip(tails, shares):
    pb = arm.pose.bones[name]
    world = arm.matrix_world @ pb.matrix
    head = world.translation.copy()
    turn = Matrix.Rotation(math.radians(SWING * share), 4, "Z") @ Matrix.Rotation(math.radians(LIFT * share), 4, "X")
    pb.matrix = arm.matrix_world.inverted() @ (Matrix.Translation(head) @ turn @ Matrix.Translation(-head) @ world)
    bpy.context.view_layer.update()

scene = bpy.context.scene
scene.render.engine = "BLENDER_WORKBENCH"
shading = scene.display.shading
shading.light = "STUDIO"
shading.color_type = "TEXTURE"
shading.show_cavity = True
shading.show_object_outline = False
scene.render.resolution_x = scene.render.resolution_y = PX
scene.render.film_transparent = False
scene.world = bpy.data.worlds.new("w")
scene.world.color = (0.82, 0.82, 0.84)
if "--wire" in args:
    for mesh in meshes:
        mod = mesh.modifiers.new("wire", "WIREFRAME")
        mod.thickness = 0.0015
        mod.use_replace = False

cam_data = bpy.data.cameras.new("cam")
cam_data.type = "ORTHO"
cam_data.ortho_scale = SPAN
cam = bpy.data.objects.new("cam", cam_data)
scene.collection.objects.link(cam)
scene.camera = cam


def look(position, up):
    forward = (CENTRE - position).normalized()
    right = forward.cross(up).normalized()
    true_up = right.cross(forward)
    m = Matrix((right, true_up, -forward)).transposed().to_4x4()
    m.translation = position
    cam.matrix_world = m


VIEWS = {
    "back": (Vector((0.0, 4.0, 0.0)), Vector((0.0, 0.0, 1.0))),
    "top": (Vector((0.0, 0.0, 4.0)), Vector((0.0, -1.0, 0.0))),
    "right": (Vector((-4.0, 0.0, 0.0)), Vector((0.0, 0.0, 1.0))),
    "quarter": (Vector((-2.2, 2.6, 2.2)), Vector((0.0, 0.0, 1.0))),
}
for name, (offset, up) in VIEWS.items():
    look(CENTRE + offset, up)
    scene.render.filepath = f"{out}_{name}.png"
    bpy.ops.render.render(write_still=True)
print(f"TAILVIEW wrote {out}_*.png ({count} tail bones, swing {SWING}, lift {LIFT})")
