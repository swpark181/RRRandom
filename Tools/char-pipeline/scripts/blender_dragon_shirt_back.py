"""Make the back of the little dragon's T-shirt look like the concept's (run inside Blender).

Usage: blender -b --factory-startup --python blender_dragon_shirt_back.py -- <rig fbx> <base color in> <png out> [preview png]

Tripo made the model from the front picture only, so behind it guessed: a thin white band across the middle of the back,
red scales from there up to the neck and a red smear over the band. The concept's back (input/dragon/concept/
dragon_back.png) is a white tee from the neck down to just above the belt, short white sleeves over the shoulders and the
wings coming out through it. Starting again from Tripo's own base color, on the game's mesh (input/dragon/dragon_rig.fbx:
the Tripo UVs, 1 tall, its back toward +Y, its left +X): every texel of every face on the back (normal pointing
backward) that falls in the shirt is painted the shirt's own white (sampled from the front's sleeves), with soft edges,
except
- the wings, whole: anything with the rig's WingL/WingR weights, or standing out behind the back (the torso's back never
  comes past BACK_Y away from the spine; the wings, their roots too, do), so the shirt stops where they come through;
- below HEM (the band's own hem stays, with the red strip above the belt like the concept) and above TOP.
The spikes down the back are left under the shirt (white).
"""
import math
import sys

import bpy
import numpy as np
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
src, tex_in, tex_out = args[0], args[1], args[2]
preview = args[3] if len(args) > 3 else None

HEM = 0.356
TOP = 0.505
# Half widths from the spine: the torso below the shoulders, and the shoulders with their short sleeves
TORSO_HALF = 0.115
SHOULDER_Z = 0.42
SHOULDER_HALF = 0.165
SOFT = 0.008
# The wings: any wing weight, or (off the spine) further back than the torso ever is
WING_WEIGHT = 0.01
WING_SPINE_GAP = 0.035
BACK_Y = -0.058
BACK_SOFT = 0.006


def ramp(value, edge, soft=SOFT):
    """0 well below edge, 1 well above, soft across soft."""
    return min(1.0, max(0.0, (value - edge) / soft + 0.5))


bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
mesh = next(o for o in bpy.context.selected_objects if o.type == "MESH")
world = mesh.matrix_world
me = mesh.data
verts = [world @ v.co for v in me.vertices]
normal_of_vertex = [(world.to_3x3() @ v.normal).normalized() for v in me.vertices]
groups = {g.index: g.name.split(":")[-1] for g in mesh.vertex_groups}
wing_of_vertex = [sum(g.weight for g in v.groups if groups[g.group].startswith("Wing")) for v in me.vertices]
# The spine's x: the middle of the body (arms and all, not the wings or the tail) round the waist
waist = [p.x for p, w in zip(verts, wing_of_vertex) if w == 0.0 and 0.37 < p.z < 0.41]
spine = (min(waist) + max(waist)) / 2
print(f"SHIRT spine x {spine:+.3f}")


def shirt(point, normal, wing):
    """0..1, how much of the shirt's back is at this point."""
    # Facing backward (the normal smoothed across the face, so the edge round the sides is smooth too)
    amount = min(1.0, max(0.0, (normal.y + 0.05) / 0.2))
    if amount <= 0.0 or wing > WING_WEIGHT:
        return 0.0
    side = abs(point.x - spine)
    amount *= ramp(point.z, HEM) * ramp(TOP, point.z)
    amount *= ramp(SHOULDER_HALF if point.z >= SHOULDER_Z else TORSO_HALF, side)
    if side > WING_SPINE_GAP:
        amount *= ramp(BACK_Y, point.y, BACK_SOFT)
    return amount


img = bpy.data.images.load(tex_in)
w, h = img.size
pixels = np.array(img.pixels[:], dtype=np.float32).reshape(h, w, 4)
uv_layer = me.uv_layers.active.data
me.calc_loop_triangles()


def texels(tri):
    """Every texel of a face (with a texel and a half of slack, so the faces either side of a UV seam both reach it),
    with the point on the face it shows and the barycentric weights."""
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
            l3 = 1 - l1 - l2
            if min(l1, l2, l3) < -slack:
                continue
            yield x, y, corners[0] * l1 + corners[1] * l2 + corners[2] * l3, (l1, l2, l3)


# The shirt's white: the bright texels of the sleeves on the front
samples = []
for tri in me.loop_triangles:
    corners = [verts[i] for i in tri.vertices]
    normal = (world.to_3x3() @ tri.normal).normalized()
    if normal.y > -0.3 or not all(0.40 < p.z < 0.47 and 0.07 < abs(p.x - spine) < 0.17 for p in corners):
        continue
    for x, y, point, _ in texels(tri):
        color = pixels[y, x, :3]
        if color.min() > 0.8:
            samples.append(color.copy())
white = np.percentile(np.array(samples), 60, axis=0) if samples else np.array([0.95, 0.94, 0.92])
print(f"SHIRT white {white} from {len(samples)} texels")

# Each texel's share of shirt, decided once: by the face it lies in, or else (in the slack round a face, for the UV
# seams) the most any face gives it; then painted once. (Blending face by face compounded the white along every edge.)
amounts = np.full((h, w), -1.0, dtype=np.float32)
shades = np.ones((h, w), dtype=np.float32)
inside = np.zeros((h, w), dtype=bool)
for tri in me.loop_triangles:
    corners = [verts[i] for i in tri.vertices]
    if max(p.z for p in corners) < HEM - 0.01 or min(p.z for p in corners) > TOP + 0.01 or min(abs(p.x - spine) for p in corners) > SHOULDER_HALF + 0.01:
        continue
    if (world.to_3x3() @ tri.normal).y < -0.3:
        continue
    # Any wing corner makes the whole face wing (the shirt mustn't creep up a root)
    wing = max(wing_of_vertex[i] for i in tri.vertices)
    normals = [normal_of_vertex[i] for i in tri.vertices]
    for x, y, point, bary in texels(tri):
        own = min(bary) >= 0.0
        if inside[y, x] and not own:
            continue
        normal = (normals[0] * bary[0] + normals[1] * bary[1] + normals[2] * bary[2]).normalized()
        amount = shirt(point, normal, wing)
        if own and not inside[y, x]:
            inside[y, x] = True
            amounts[y, x] = -1.0
        if amount > amounts[y, x]:
            amounts[y, x] = amount
            # A touch darker toward the hem and the sides, as the band is
            shades[y, x] = 1.0 - 0.05 * (1.0 - min(1.0, (point.z - HEM) / 0.03)) - 0.05 * min(1.0, abs(point.x - spine) / SHOULDER_HALF)
share = np.clip(amounts, 0.0, 1.0)[..., None]
pixels[..., :3] = pixels[..., :3] * (1 - share) + white[None, None, :] * shades[..., None] * share
painted = int((share > 0).sum())
out = bpy.data.images.new("out", w, h)
out.pixels[:] = pixels.ravel()
out.filepath_raw = tex_out
out.file_format = "PNG"
out.save()
print(f"SHIRT painted {painted} texels -> {tex_out}")

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
    # (name, yaw, elevation looking down, ortho size, height looked at): straight on, and from behind and above as the
    # game's camera sees it
    views = (("back", 180, 0, 1.05, 0.5), ("back_close", 180, 0, 0.45, 0.44), ("back_above", 180, 25, 0.6, 0.45),
             ("back3q_above", 145, 25, 0.6, 0.45), ("side", 90, 0, 1.05, 0.5), ("front", 0, 0, 1.05, 0.5))
    for name, yaw, down, zoom, look in views:
        r = math.radians(yaw)
        e = math.radians(down)
        cam.data.ortho_scale = zoom
        cam.location = Vector((spine + math.sin(r) * 3 * math.cos(e), -math.cos(r) * 3 * math.cos(e), look + 3 * math.sin(e)))
        cam.rotation_euler = (math.radians(90) - e, 0, r)
        sun.rotation_euler = (math.radians(50), 0, r + math.radians(20))
        scene.render.filepath = f"{preview}_{name}.png"
        bpy.ops.render.render(write_still=True)
    print("SHIRT preview done")
