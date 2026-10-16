"""Procedural meshes for the TPS map "Dice Land" (run inside Blender).

Usage: blender -b --factory-startup --python blender_dice_land.py -- <out dir>

Writes, in Unreal units (centimeters), each as <out dir>/<name>.fbx:
- SM_DLDie: a puffy die (a superellipsoid, soft balloon-like corners), 100 across, pivot in the middle; material slot 0
  "Body" (tinted per instance in Unreal), slot 1 "Pips" (21 pips, opposite faces sum to 7).
- SM_DLDieBlank: the same die without pips, for the hundreds stacked under the island.
- SM_DLRainbowDie: the die with each face a different pastel color (vertex colors, slot 0) and white pips (slot 1).
- SM_DLTile: a cushion floor tile 200 x 200 x 50, pivot in the middle of its top, one slot (tinted per instance).
- SM_DLBall: a smooth ball 100 across, pivot in the middle (bushes, clouds, flowers).
- SM_DLTrunk: a tree trunk, a smooth tapered column 100 across at the foot and 100 high, pivot in the middle of the foot.
- SM_DLRing: a thin ring 100 across (the halos round the plaza's die).
Then renders them side by side (<out dir>/dice_land_preview.png).
"""
import math
import sys

import bmesh
import bpy
from mathutils import Vector

out_dir = sys.argv[sys.argv.index("--") + 1]

# Superellipsoid exponent: 2 is a sphere, higher is boxier; 6 reads as a die blown up like a balloon
DIE_POWER = 6.0
DIE_SEGMENTS = 18
TILE_POWER = 9.0
TILE_SEGMENTS = 12
PIP_RADIUS = 0.085
PIP_SPREAD = 0.24
PIP_DEPTH = 0.488
PIP_FLATTEN = 0.32
# The rainbow die's faces (sRGB), +X -X +Y -Y +Z -Z
RAINBOW = [(1.00, 0.56, 0.69), (0.57, 0.61, 1.00), (1.00, 0.87, 0.43), (0.45, 0.77, 1.00), (0.48, 0.87, 0.65), (1.00, 0.70, 0.50)]
FACES = [((1, 0, 0), (0, 1, 0), (0, 0, 1), 2), ((-1, 0, 0), (0, 1, 0), (0, 0, 1), 5),
         ((0, 1, 0), (1, 0, 0), (0, 0, 1), 3), ((0, -1, 0), (1, 0, 0), (0, 0, 1), 4),
         ((0, 0, 1), (1, 0, 0), (0, 1, 0), 1), ((0, 0, -1), (1, 0, 0), (0, 1, 0), 6)]
PATTERNS = {1: [(0, 0)], 2: [(-1, -1), (1, 1)], 3: [(-1, -1), (0, 0), (1, 1)], 4: [(-1, -1), (-1, 1), (1, -1), (1, 1)],
            5: [(-1, -1), (-1, 1), (1, -1), (1, 1), (0, 0)], 6: [(-1, -1), (-1, 0), (-1, 1), (1, -1), (1, 0), (1, 1)]}


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def superellipsoid(name, power, segments, size):
    """A cube subdivided into segments per side, each vertex pushed out along its direction onto the superellipsoid
    |x|^p + |y|^p + |z|^p = 0.5^p, then scaled to size (meters)."""
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.subdivide_edges(bm, edges=bm.edges[:], cuts=segments - 1, use_grid_fill=True)
    for v in bm.verts:
        d = v.co.normalized()
        s = (abs(d.x) ** power + abs(d.y) ** power + abs(d.z) ** power) ** (1.0 / power)
        v.co = Vector((d.x * 0.5 / s * size[0], d.y * 0.5 / s * size[1], d.z * 0.5 / s * size[2]))
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    for p in mesh.polygons:
        p.use_smooth = True
    return obj


def add_pips(die, size):
    """Flattened spheres sunk into each face in the usual layout, material slot 1."""
    bm = bmesh.new()
    bm.from_mesh(die.data)
    for normal, a, b, value in FACES:
        n, a, b = Vector(normal), Vector(a), Vector(b)
        z = Vector((0, 0, 1))
        rot = z.rotation_difference(n).to_matrix().to_4x4()
        for u, w in PATTERNS[value]:
            center = n * PIP_DEPTH + a * u * PIP_SPREAD + b * w * PIP_SPREAD
            pip = bmesh.ops.create_uvsphere(bm, u_segments=32, v_segments=12, radius=PIP_RADIUS)
            verts = pip["verts"]
            for v in verts:
                v.co.z *= PIP_FLATTEN
                v.co = rot @ v.co + center
            for f in {f for v in verts for f in v.link_faces}:
                f.material_index = 1
                f.smooth = True
    bm.to_mesh(die.data)
    bm.free()
    die.scale = (size, size, size)


def materials(obj, names):
    for n in names:
        obj.data.materials.append(bpy.data.materials.get(n) or bpy.data.materials.new(n))


def paint_rainbow(obj):
    """Vertex colors by the face each polygon's middle points most along, so the colors meet in a clean line at the
    rounded edges instead of blending (its pips stay on slot 1 anyway)."""
    mesh = obj.data
    layer = mesh.color_attributes.new(name="Col", type="BYTE_COLOR", domain="CORNER")
    for poly in mesh.polygons:
        co = poly.center
        axis = max(range(3), key=lambda i: abs(co[i]))
        for li in poly.loop_indices:
            face = axis * 2 + (0 if co[axis] > 0 else 1)
            r, g, b = RAINBOW[face] if poly.material_index == 0 else (1.0, 1.0, 1.0)
            layer.data[li].color_srgb = (r, g, b, 1.0)


def export(obj, name):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    obj.name = name
    bpy.ops.export_scene.fbx(filepath=f"{out_dir}/{name}.fbx", use_selection=True, object_types={"MESH"},
                             mesh_smooth_type="FACE", colors_type="SRGB", path_mode="STRIP", bake_anim=False)
    lo = Vector([min(v.co[i] for v in obj.data.vertices) for i in range(3)]) * 100
    hi = Vector([max(v.co[i] for v in obj.data.vertices) for i in range(3)]) * 100
    tris = sum(len(p.vertices) - 2 for p in obj.data.polygons)
    print(f"DICELAND {name}: {tris} tris, {lo.x:.0f}..{hi.x:.0f} x {lo.y:.0f}..{hi.y:.0f} x {lo.z:.0f}..{hi.z:.0f}")


def make_die(name, pips, rainbow):
    reset()
    die = superellipsoid(name, DIE_POWER, DIE_SEGMENTS, (1.0, 1.0, 1.0))
    materials(die, ["Body", "Pips"])
    if pips:
        add_pips(die, 1.0)
    if rainbow:
        paint_rainbow(die)
    die.scale = (1.0, 1.0, 1.0)
    export(die, name)


def make_tile(name):
    reset()
    tile = superellipsoid(name, TILE_POWER, TILE_SEGMENTS, (2.0, 2.0, 0.5))
    materials(tile, ["Body"])
    # Pivot in the middle of the top
    top = max(v.co.z for v in tile.data.vertices)
    for v in tile.data.vertices:
        v.co.z -= top
    export(tile, name)


def make_ball(name):
    reset()
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=48, v_segments=24, radius=0.5)
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    for p in mesh.polygons:
        p.use_smooth = True
    materials(obj, ["Body"])
    export(obj, name)


def make_trunk(name):
    """A column tapering from 100 across at the foot to 76 at the top, its top edge rounded; smooth sides, flat ends."""
    reset()
    bm = bmesh.new()
    rings = [(0.0, 0.5), (0.85, 0.40), (0.93, 0.385), (0.98, 0.36), (1.0, 0.33)]
    segments = 40
    verts = [[bm.verts.new((r * math.cos(2 * math.pi * k / segments), r * math.sin(2 * math.pi * k / segments), z))
              for k in range(segments)] for z, r in rings]
    for lower, upper in zip(verts, verts[1:]):
        for k in range(segments):
            bm.faces.new((lower[k], lower[(k + 1) % segments], upper[(k + 1) % segments], upper[k]))
    bm.faces.new(list(reversed(verts[0])))
    bm.faces.new(verts[-1])
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    for p in mesh.polygons:
        p.use_smooth = abs(p.normal.z) < 0.9
    materials(obj, ["Body"])
    export(obj, name)


def make_ring(name):
    reset()
    bpy.ops.mesh.primitive_torus_add(major_radius=0.5, minor_radius=0.035, major_segments=72, minor_segments=16)
    obj = bpy.context.active_object
    for p in obj.data.polygons:
        p.use_smooth = True
    materials(obj, ["Body"])
    export(obj, name)


make_die("SM_DLDie", pips=True, rainbow=False)
make_die("SM_DLDieBlank", pips=False, rainbow=False)
make_die("SM_DLRainbowDie", pips=True, rainbow=True)
make_tile("SM_DLTile")
make_ball("SM_DLBall")
make_trunk("SM_DLTrunk")
make_ring("SM_DLRing")

# Preview: all four in a row, lit, from a little above
reset()
x = 0.0
colors = {"SM_DLDie": (1.0, 0.45, 0.6, 1), "SM_DLDieBlank": (0.5, 0.75, 1.0, 1), "SM_DLTile": (0.45, 0.88, 0.6, 1)}
for name in ("SM_DLDie", "SM_DLDieBlank", "SM_DLRainbowDie", "SM_DLTile"):
    bpy.ops.import_scene.fbx(filepath=f"{out_dir}/{name}.fbx")
    obj = bpy.context.selected_objects[0]
    width = 2.0 if name == "SM_DLTile" else 1.0
    obj.location = (x + width / 2, 0, 0.5 if name == "SM_DLTile" else 0)
    x += width + 0.5
    for i, slot in enumerate(obj.material_slots):
        mat = bpy.data.materials.new(f"{name}_{i}")
        mat.use_nodes = True
        bsdf = mat.node_tree.nodes["Principled BSDF"]
        bsdf.inputs["Roughness"].default_value = 0.35
        if i == 1:
            bsdf.inputs["Base Color"].default_value = (1, 1, 1, 1)
        elif name == "SM_DLRainbowDie":
            attr = mat.node_tree.nodes.new("ShaderNodeVertexColor")
            mat.node_tree.links.new(attr.outputs["Color"], bsdf.inputs["Base Color"])
        else:
            bsdf.inputs["Base Color"].default_value = colors[name]
        slot.material = mat
scene = bpy.context.scene
cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
scene.collection.objects.link(cam)
cam.data.type = "ORTHO"
cam.data.ortho_scale = x * 1.05
cam.location = (x / 2 - 0.25, -6.0, 3.2)
cam.rotation_euler = (math.radians(65), 0.0, 0.0)
scene.camera = cam
sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
scene.collection.objects.link(sun)
sun.rotation_euler = (math.radians(40), math.radians(25), 0.0)
sun.data.energy = 3.0
world = bpy.data.worlds.new("W")
world.use_nodes = True
world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.55, 0.7, 1.0, 1)
scene.world = world
engines = {e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items}
scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in engines else "BLENDER_EEVEE"
scene.render.resolution_x, scene.render.resolution_y = 1400, 400
scene.render.filepath = f"{out_dir}/dice_land_preview.png"
bpy.ops.render.render(write_still=True)
print(f"DICELAND preview {scene.render.filepath}")
