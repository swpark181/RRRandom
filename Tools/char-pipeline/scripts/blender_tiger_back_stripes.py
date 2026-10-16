"""Paint tiger stripes on the back of the tiger's head, which Tripo left plain orange (run inside Blender).

Usage: blender -b --factory-startup --python blender_tiger_back_stripes.py -- <tripo fbx> <base color in> <png out> [preview png]

Works on the Tripo FBX (its UVs are the ones the game's texture uses; the Mixamo mesh keeps them). Around the head's
middle (from the mesh: the head is everything above HEAD_BOTTOM of the height), a point's direction from the head
center gives an angle across the back (0 straight behind, + toward the tiger's left) and an elevation; STRIPES are
bands of those angles, tapering at both ends and bending a little, in the dark brown of the stripes on its face.
Every texel of every face on the back of the head (normal pointing backward) that falls in a stripe is darkened, with
a soft edge. The ears (their backs are already dark) and the white cheek fur at the sides are past MAX_SIDE.
"""
import math
import sys

import bpy
import numpy as np
from mathutils import Vector
from mathutils.geometry import barycentric_transform

args = sys.argv[sys.argv.index("--") + 1:]
src, tex_in, tex_out = args[0], args[1], args[2]
preview = args[3] if len(args) > 3 else None

HEAD_BOTTOM = 0.56
STRIPE_COLOR = np.array([0.10, 0.045, 0.02])
# Like the ones on its forehead: bold at the crown end, tapering to a point downward.
# (angle across the back in degrees at the bottom end, elevation of the bottom end, of the top end, widest half width,
# how far the top end leans toward the middle in degrees)
STRIPES = [
    (0.0, 22.0, 88.0, 7.5, 0.0),
    (-25.0, 0.0, 80.0, 8.5, 10.0),
    (25.0, 0.0, 80.0, 8.5, -10.0),
    (-50.0, -18.0, 52.0, 7.5, 12.0),
    (50.0, -18.0, 52.0, 7.5, -12.0),
]
SOFT = 1.6
# Past this far round to the side are the ears and the white cheek fur
MAX_SIDE = 64.0

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
mesh = next(o for o in bpy.data.objects if o.type == "MESH")
world = mesh.matrix_world
verts = [world @ v.co for v in mesh.data.vertices]
lo = min(p.z for p in verts)
hi = max(p.z for p in verts)
height = hi - lo
head = [p for p in verts if p.z > lo + height * HEAD_BOTTOM]
center = Vector((
    (min(p.x for p in head) + max(p.x for p in head)) / 2,
    (min(p.y for p in head) + max(p.y for p in head)) / 2,
    (min(p.z for p in head) + max(p.z for p in head)) / 2,
))
print(f"STRIPES head center {center}, height {height}")


def stripe(point):
    """0..1, how much of a stripe is at this point on the head."""
    d = point - center
    across = math.degrees(math.atan2(d.x, d.y))   # +Y is behind the tiger, +X its left
    flat = math.hypot(d.x, d.y)
    up = math.degrees(math.atan2(d.z, flat))
    if abs(across) > MAX_SIDE:
        return 0.0
    best = 0.0
    for middle, low, high, width, lean in STRIPES:
        if up <= low or up >= high:
            continue
        t = (up - low) / (high - low)
        # Round at the top end, a long point at the bottom
        half = width * min(1.0, (1.0 - t) / 0.12) ** 0.5 * t ** 0.8
        mid = middle + lean * t * t
        edge = half - abs(across - mid)
        best = max(best, min(1.0, max(0.0, edge / SOFT + 0.5)))
    return best


img = bpy.data.images.load(tex_in)
w, h = img.size
pixels = np.array(img.pixels[:], dtype=np.float32).reshape(h, w, 4)
uv_layer = mesh.data.uv_layers.active.data
mesh.data.calc_loop_triangles()
painted = 0
for tri in mesh.data.loop_triangles:
    normal = world.to_3x3() @ tri.normal
    corners = [verts[i] for i in tri.vertices]
    if min(p.z for p in corners) < lo + height * HEAD_BOTTOM or normal.normalized().y < 0.15:
        continue
    uvs = [Vector((uv_layer[i].uv[0] * w, uv_layer[i].uv[1] * h, 0.0)) for i in tri.loops]
    x0, x1 = int(min(u.x for u in uvs)), int(math.ceil(max(u.x for u in uvs)))
    y0, y1 = int(min(u.y for u in uvs)), int(math.ceil(max(u.y for u in uvs)))
    for y in range(max(0, y0 - 1), min(h, y1 + 2)):
        for x in range(max(0, x0 - 1), min(w, x1 + 2)):
            p = Vector((x + 0.5, y + 0.5, 0.0))
            # Inside the UV triangle (with a half texel of slack so neighboring faces meet)
            a, b, c = uvs
            den = (b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y)
            if abs(den) < 1e-9:
                continue
            l1 = ((b.y - c.y) * (p.x - c.x) + (c.x - b.x) * (p.y - c.y)) / den
            l2 = ((c.y - a.y) * (p.x - c.x) + (a.x - c.x) * (p.y - c.y)) / den
            l3 = 1 - l1 - l2
            # A texel and a half past the edge, so the faces on either side of a UV seam both reach it
            slack = 1.5 / max(1.0, min((a - b).length, (b - c).length, (c - a).length))
            if min(l1, l2, l3) < -slack:
                continue
            point = corners[0] * l1 + corners[1] * l2 + corners[2] * l3
            amount = stripe(point)
            if amount <= 0.0:
                continue
            pixels[y, x, :3] = pixels[y, x, :3] * (1 - amount) + STRIPE_COLOR * amount
            painted += 1
out = bpy.data.images.new("out", w, h)
out.pixels[:] = pixels.ravel()
out.filepath_raw = tex_out
out.file_format = "PNG"
out.save()
print(f"STRIPES painted {painted} texels -> {tex_out}")

if preview:
    mat = bpy.data.materials.new("m")
    mat.use_nodes = True
    node = mat.node_tree.nodes.new("ShaderNodeTexImage")
    node.image = out
    mat.node_tree.links.new(node.outputs["Color"], mat.node_tree.nodes["Principled BSDF"].inputs["Base Color"])
    mesh.data.materials.clear()
    mesh.data.materials.append(mat)
    scene = bpy.context.scene
    engines = {e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items}
    scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in engines else "BLENDER_EEVEE"
    scene.render.resolution_x = scene.render.resolution_y = 600
    scene.view_settings.view_transform = "Standard"
    wd = bpy.data.worlds.new("W")
    scene.world = wd
    wd.use_nodes = True
    wd.node_tree.nodes["Background"].inputs[0].default_value = (1, 1, 1, 1)
    sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
    sun.data.energy = 3
    sun.rotation_euler = (math.radians(50), 0, math.radians(160))
    scene.collection.objects.link(sun)
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    scene.collection.objects.link(cam)
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = height * 0.6
    scene.camera = cam
    for name, yaw in (("back", 180), ("back3q", 140), ("side", 90)):
        r = math.radians(yaw)
        cam.location = center + Vector((math.sin(r) * 3, -math.cos(r) * 3, 0))
        cam.rotation_euler = (math.radians(90), 0, r)
        scene.render.filepath = f"{preview}_{name}.png"
        bpy.ops.render.render(write_still=True)
    print("STRIPES preview done")
