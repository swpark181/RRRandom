"""Take the little dragon's T-shirt off (run inside Blender): its torso and upper arms all red scales, the cream belly kept.

Usage: blender -b --factory-startup --python blender_dragon_no_shirt.py -- <rig fbx> <base color in> <png out> [preview png]

Starts from Tripo's own base color, on the game's mesh (input/dragon/dragon_rig.fbx: the Tripo UVs, 1 tall, its back
toward +Y). The shirt is only painted on (the sleeves are a slight swell in the mesh), so on every face between
BOTTOM and TOP (the torso and the tops of the arms; the belt is below, the face above) each texel is split into the
skin's red and the shirt's white along the line between them (how much shirt it has, and what is left over: shading,
the pink where the two blend), and the shirt's part is taken out: what was white turns red, a fold's shadow stays a
shadow, a pink edge turns red, and colors off that line (the cream belly) keep almost all of themselves. The red is
sampled from the upper back's scales, the white from the front's sleeves.
"""
import math
import sys

import bpy
import numpy as np
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
src, tex_in, tex_out = args[0], args[1], args[2]
preview = args[3] if len(args) > 3 else None

BOTTOM = 0.33
TOP = 0.53

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
mesh = next(o for o in bpy.context.selected_objects if o.type == "MESH")
world = mesh.matrix_world
me = mesh.data
verts = [world @ v.co for v in me.vertices]
groups = {g.index: g.name.split(":")[-1] for g in mesh.vertex_groups}
wing_of_vertex = [sum(g.weight for g in v.groups if groups[g.group].startswith("Wing")) for v in me.vertices]
waist = [p.x for p, w in zip(verts, wing_of_vertex) if w == 0.0 and 0.37 < p.z < 0.41]
spine = (min(waist) + max(waist)) / 2

img = bpy.data.images.load(tex_in)
w, h = img.size
pixels = np.array(img.pixels[:], dtype=np.float32).reshape(h, w, 4)
uv_layer = me.uv_layers.active.data
me.calc_loop_triangles()


def texels(tri):
    """Every texel of a face, with a texel and a half of slack so the faces either side of a UV seam both reach it."""
    corners = [verts[i] for i in tri.vertices]
    a, b, c = [Vector((uv_layer[i].uv[0] * w, uv_layer[i].uv[1] * h, 0.0)) for i in tri.loops]
    den = (b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y)
    if abs(den) < 1e-9:
        return
    slack = 1.5 / max(1.0, min((a - b).length, (b - c).length, (c - a).length))
    for y in range(max(0, int(min(a.y, b.y, c.y)) - 1), min(h, int(math.ceil(max(a.y, b.y, c.y))) + 2)):
        for x in range(max(0, int(min(a.x, b.x, c.x)) - 1), min(w, int(math.ceil(max(a.x, b.x, c.x))) + 2)):
            px, py = x + 0.5, y + 0.5
            l1 = ((b.y - c.y) * (px - c.x) + (c.x - b.x) * (py - c.y)) / den
            l2 = ((c.y - a.y) * (px - c.x) + (a.x - c.x) * (py - c.y)) / den
            if min(l1, l2, 1 - l1 - l2) < -slack:
                continue
            yield x, y, corners[0] * l1 + corners[1] * l2 + corners[2] * (1 - l1 - l2)


# The two colors: red scales on the upper back (between the wings, off the spine), white on the front's sleeves
reds = []
whites = []
for tri in me.loop_triangles:
    corners = [verts[i] for i in tri.vertices]
    normal = (world.to_3x3() @ tri.normal).normalized()
    side = [abs(p.x - spine) for p in corners]
    if normal.y > 0.5 and all(0.50 < p.z < 0.53 for p in corners) and all(0.03 < s < 0.08 for s in side):
        reds += [pixels[y, x, :3].copy() for x, y, _ in texels(tri)]
    if normal.y < -0.3 and all(0.40 < p.z < 0.47 for p in corners) and all(0.07 < s < 0.17 for s in side):
        whites += [c for c in (pixels[y, x, :3].copy() for x, y, _ in texels(tri)) if c.min() > 0.8]
red = np.median(np.array([c for c in reds if c[0] > 0.5 and c[1] < 0.35]), axis=0)
white = np.percentile(np.array(whites), 60, axis=0)
print(f"NOSHIRT spine {spine:+.3f}, red {red} from {len(reds)}, white {white} from {len(whites)}")

# The texels to clean: every face in the band, each texel once
mask = np.zeros((h, w), dtype=bool)
for tri in me.loop_triangles:
    zs = [verts[i].z for i in tri.vertices]
    if max(zs) < BOTTOM or min(zs) > TOP:
        continue
    for x, y, point in texels(tri):
        if BOTTOM <= point.z <= TOP:
            mask[y, x] = True
ys, xs = np.nonzero(mask)
colors = pixels[ys, xs, :3]
# Along the red-to-white line: t = how much shirt. Only colors near that line are shirt or its blend into the scales
# (white, a fold's grey, a pink edge); the cream belly and the orange wings lie well off it and stay
axis = white - red
t = np.clip(((colors - red) @ axis) / (axis @ axis), 0.0, 1.0)
on_line = red[None, :] + t[:, None] * axis[None, :]
off = np.linalg.norm(colors - on_line, axis=1)
shirtlike = 1.0 - np.clip((off - 0.10) / 0.10, 0.0, 1.0)
# The scales it becomes keep the shirt's shading (a fold a little darker red)
luma = np.array([0.30, 0.59, 0.11])
shade = np.clip((colors @ luma) / np.maximum(on_line @ luma, 1e-3), 0.75, 1.1)
share = (shirtlike * np.clip(t * 2.0, 0.0, 1.0))[:, None]
pixels[ys, xs, :3] = colors * (1 - share) + red[None, :] * shade[:, None] * share
print(f"NOSHIRT looked at {len(ys)} texels, {int((share[:, 0] > 0.5).sum())} of them mostly shirt")

out = bpy.data.images.new("out", w, h)
out.pixels[:] = pixels.ravel()
out.filepath_raw = tex_out
out.file_format = "PNG"
out.save()

if preview:
    mat = bpy.data.materials.new("m")
    mat.use_nodes = True
    node = mat.node_tree.nodes.new("ShaderNodeTexImage")
    node.image = out
    bsdf = mat.node_tree.nodes["Principled BSDF"]
    mat.node_tree.links.new(node.outputs["Color"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.7
    me.materials.clear()
    me.materials.append(mat)
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = scene.render.resolution_y = 800
    scene.view_settings.view_transform = "Standard"
    wd = bpy.data.worlds.new("W")
    scene.world = wd
    wd.use_nodes = True
    wd.node_tree.nodes["Background"].inputs[0].default_value = (0.8, 0.8, 0.8, 1)
    sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
    sun.data.energy = 2.5
    scene.collection.objects.link(sun)
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    scene.collection.objects.link(cam)
    cam.data.type = "ORTHO"
    scene.camera = cam
    views = (("front", 0, 0, 1.05, 0.5), ("front_close", 0, 0, 0.45, 0.44), ("side", 90, 0, 1.05, 0.5),
             ("back", 180, 0, 1.05, 0.5), ("back_above", 180, 25, 0.6, 0.45))
    for name, yaw, down, zoom, look in views:
        r = math.radians(yaw)
        e = math.radians(down)
        cam.data.ortho_scale = zoom
        cam.location = Vector((spine + math.sin(r) * 3 * math.cos(e), -math.cos(r) * 3 * math.cos(e), look + 3 * math.sin(e)))
        cam.rotation_euler = (math.radians(90) - e, 0, r)
        sun.rotation_euler = (math.radians(50), 0, r + math.radians(20))
        scene.render.filepath = f"{preview}_{name}.png"
        bpy.ops.render.render(write_still=True)
    print("NOSHIRT preview done")
