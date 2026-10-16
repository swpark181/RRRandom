"""Cut Tripo grass blocks into square floor tiles that butt up against each other (run inside Blender).

Usage: blender -b --factory-startup --python blender_grass_tiles.py -- <out dir> <name>=<tripo fbx> [<name>=<tripo fbx> ...]
  e.g. ... -- C:/work/char-pipeline/output/grass SM_GrassTile_A=.../a.fbx SM_GrassTile_B=.../b.fbx

Tripo's "grass block" is a soil cube with grass on top: not square, with spiky grass hanging over the edges and the
top rounding off toward them, so laid side by side it leaves gaps and overlaps. For each block this:
- finds the flat top: its height (the median height over the middle) and how far it spreads at that height,
- keeps the square inside that spread (INSET), cut with straight planes, and only a thin layer under the top (DEPTH),
  so neighbors meet edge to edge at the same height and no soil shows,
- moves the middle of that square, at the top's height, to the origin and scales the square to TILE (180 = the fox),
- measures the top's average color from its texture (area weighted, in linear color) and works out a tint per tile
  that brings every tile to the set's shared average, so the tiles don't show as patches of different greens,
- decimates to about TARGET_TRIS triangles, since the arena takes hundreds of them.
Writes <out dir>/<name>.fbx per tile and <out dir>/tiles.json ({name: {"tint": [r, g, b], "average": [r, g, b]}}),
then renders a 3x3 patch of the tinted results, picked and turned at random, from above (<out dir>/tiles_preview.png).
"""
import json
import math
import random
import sys

import bmesh
import bpy
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
out_dir = args[0]
tiles = [a.split("=", 1) for a in args[1:]]

TILE = 1.8          # side in Blender meters (180 in Unreal)
INSET = 0.92        # share of the flat top's spread kept, clear of its ragged edge and where it starts rounding off
TOP_BAND = 0.03     # a vertex this close under the top (as a share of the block's width) counts as part of the flat top
DEPTH = 0.08        # how far under the top is kept, as a share of the kept side
TARGET_TRIS = 800    # the floor takes ~2,000 tiles; from the high camera the texture carries the blades


def log(msg):
    print(f"GRASS {msg}")


def median(values):
    values = sorted(values)
    return values[len(values) // 2]


def to_linear(c):
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def base_color_image(material):
    """The image feeding the material's base color (Tripo's import wires an image texture straight in)."""
    bsdf = next(n for n in material.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
    link = bsdf.inputs["Base Color"].links[0]
    return link.from_node.image, bsdf, link


def top_average(obj, image):
    """Area-weighted average linear color of the upward-facing faces, sampled at each face's middle UV."""
    width, height = image.size
    pixels = image.pixels[:]
    uv = obj.data.uv_layers.active.data
    total, weight = Vector((0.0, 0.0, 0.0)), 0.0
    for poly in obj.data.polygons:
        if poly.normal.z < 0.5:
            continue
        u = sum(uv[i].uv.x for i in poly.loop_indices) / poly.loop_total
        v = sum(uv[i].uv.y for i in poly.loop_indices) / poly.loop_total
        x = min(width - 1, max(0, int((u % 1.0) * width)))
        y = min(height - 1, max(0, int((v % 1.0) * height)))
        p = (y * width + x) * 4
        total += Vector([to_linear(pixels[p + c]) for c in range(3)]) * poly.area
        weight += poly.area
    return total / weight


made = []
for name, src in tiles:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=src)
    obj = next(o for o in bpy.context.scene.objects if o.type == 'MESH')
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

    # Upright: Tripo sometimes models a tile standing up, facing the front. The slab's thinnest side is its height;
    # its top is the bumpy side, where more of the vertices sit (the pebbles and tufts), so that side goes up
    vs = [v.co.copy() for v in obj.data.vertices]
    extent = [max(v[i] for v in vs) - min(v[i] for v in vs) for i in range(3)]
    thin = extent.index(min(extent))
    if thin != 2:
        mid = (max(v[thin] for v in vs) + min(v[thin] for v in vs)) / 2
        up = 1.0 if sum(v[thin] for v in vs) / len(vs) > mid else -1.0
        axis = Vector((0.0, 0.0, 0.0))
        axis[thin] = up
        obj.data.transform(axis.rotation_difference(Vector((0.0, 0.0, 1.0))).to_matrix().to_4x4())
        obj.data.update()
        log(f"{name}: stood upright ({'XYZ'[thin]} {up:+.0f} turned up)")

    vs = [v.co.copy() for v in obj.data.vertices]
    mn = Vector([min(v[i] for v in vs) for i in range(3)])
    mx = Vector([max(v[i] for v in vs) for i in range(3)])
    middle_xy = (mn + mx) / 2
    width = min(mx.x - mn.x, mx.y - mn.y)
    # The flat top: the median height of the upper vertices over the middle, then how far it spreads at that height
    middle = [v.z for v in vs if abs(v.x - middle_xy.x) < width * 0.3 and abs(v.y - middle_xy.y) < width * 0.3]
    top = median([z for z in middle if z > (mn.z + mx.z) / 2])
    flat = [v for v in vs if v.z > top - TOP_BAND * width]
    lo = Vector((min(v.x for v in flat), min(v.y for v in flat)))
    hi = Vector((max(v.x for v in flat), max(v.y for v in flat)))
    center = Vector(((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, top))
    half = min(hi.x - lo.x, hi.y - lo.y) / 2 * INSET
    floor = top - DEPTH * half * 2

    bm = bmesh.new()
    bm.from_mesh(obj.data)
    # Keep the square column above "floor": clear everything outside each cutting plane
    for co, no in (((center.x + half, 0, 0), (1, 0, 0)), ((center.x - half, 0, 0), (-1, 0, 0)),
                   ((0, center.y + half, 0), (0, 1, 0)), ((0, center.y - half, 0), (0, -1, 0)),
                   ((0, 0, floor), (0, 0, -1))):
        geom = bm.verts[:] + bm.edges[:] + bm.faces[:]
        bmesh.ops.bisect_plane(bm, geom=geom, plane_co=co, plane_no=no, clear_outer=True)
    # Origin in the middle of the square at the top's height, side TILE
    scale = TILE / (half * 2)
    for v in bm.verts:
        v.co = (v.co - center) * scale
    bm.to_mesh(obj.data)
    bm.free()
    obj.data.update()

    image, _, _ = base_color_image(obj.data.materials[0])
    average = top_average(obj, image)

    tris = sum(len(p.vertices) - 2 for p in obj.data.polygons)
    if tris > TARGET_TRIS:
        dec = obj.modifiers.new("Decimate", 'DECIMATE')
        dec.ratio = TARGET_TRIS / tris
        bpy.ops.object.modifier_apply(modifier="Decimate")
    after = sum(len(p.vertices) - 2 for p in obj.data.polygons)
    cut = [v.co for v in obj.data.vertices]
    b_lo = Vector([min(v[i] for v in cut) for i in range(3)])
    b_hi = Vector([max(v[i] for v in cut) for i in range(3)])
    log(f"{name}: top spread {hi.x - lo.x:.3f} x {hi.y - lo.y:.3f} of {mx.x - mn.x:.3f} x {mx.y - mn.y:.3f}, kept {half * 2:.3f}; "
        f"{tris} -> {after} tris; bounds {tuple(round(c * 100, 1) for c in b_lo)} .. {tuple(round(c * 100, 1) for c in b_hi)}; "
        f"top color {tuple(round(c, 3) for c in average)}")

    obj.name = name
    obj.data.materials[0].name = name.replace("SM_", "M_")
    path = f"{out_dir}/{name}.fbx"
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, mesh_smooth_type='FACE', path_mode='STRIP',
                             bake_anim=False, add_leaf_bones=False)
    made.append({"name": name, "path": path, "src": src, "average": average})
    log(f"exported {path}")

# One tint per tile that brings its top's average color to the set's shared average
shared = sum((t["average"] for t in made), Vector((0.0, 0.0, 0.0))) / len(made)
info = {}
for t in made:
    # Grass has almost no blue, so its ratio is mostly noise: blue follows green
    red, green = (shared[i] / max(t["average"][i], 1e-4) for i in range(2))
    t["tint"] = Vector((red, green, green))
    info[t["name"]] = {"tint": [round(c, 4) for c in t["tint"]], "average": [round(c, 4) for c in t["average"]]}
with open(f"{out_dir}/tiles.json", "w") as f:
    json.dump(info, f, indent=2)
log(f"tints {json.dumps(info)}")

# Preview: a 3x3 patch, each tile a random one of the set turned a random quarter, tinted, seen from above
bpy.ops.wm.read_factory_settings(use_empty=True)
materials = {}
for t in made:
    bpy.ops.import_scene.fbx(filepath=t["src"])  # the Tripo original brings its textured material along
    original = next(o for o in bpy.context.selected_objects if o.type == 'MESH')
    mat = original.data.materials[0]
    _, bsdf, link = base_color_image(mat)
    tint = mat.node_tree.nodes.new("ShaderNodeMix")
    tint.data_type = 'RGBA'
    tint.blend_type = 'MULTIPLY'
    tint.inputs[0].default_value = 1.0
    tint.inputs[7].default_value = (*t["tint"], 1.0)
    source = link.from_socket
    mat.node_tree.links.remove(link)
    mat.node_tree.links.new(source, tint.inputs[6])
    mat.node_tree.links.new(tint.outputs[2], bsdf.inputs["Base Color"])
    materials[t["name"]] = mat
    bpy.data.objects.remove(original, do_unlink=True)
rng = random.Random(7)
for gx in range(3):
    for gy in range(3):
        t = rng.choice(made)
        bpy.ops.import_scene.fbx(filepath=t["path"])
        tile = next(o for o in bpy.context.selected_objects if o.type == 'MESH')
        tile.data.materials[0] = materials[t["name"]]
        tile.location = ((gx - 1) * TILE, (gy - 1) * TILE, 0)
        tile.rotation_euler = (0, 0, rng.randrange(4) * math.pi / 2)
sc = bpy.context.scene
sc.render.engine = 'BLENDER_EEVEE'
sc.render.resolution_x = sc.render.resolution_y = 900
sc.view_settings.view_transform = "Standard"
world = bpy.data.worlds.new("w")
sc.world = world
world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.9, 0.9, 0.95, 1)
sun = bpy.data.objects.new("s", bpy.data.lights.new("s", 'SUN'))
sun.data.energy = 2.5
sun.rotation_euler = (math.radians(40), 0, math.radians(35))
sc.collection.objects.link(sun)
cam = bpy.data.objects.new("c", bpy.data.cameras.new("c"))
sc.collection.objects.link(cam)
sc.camera = cam
cam.data.type = 'ORTHO'
cam.data.ortho_scale = TILE * 3.2
cam.location = (0, 0, 10)
sc.render.filepath = f"{out_dir}/tiles_preview.png"
bpy.ops.render.render(write_still=True)
log(f"preview {sc.render.filepath}")
