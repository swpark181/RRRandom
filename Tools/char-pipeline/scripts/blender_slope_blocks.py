"""Diagonal blocks for Dice Town (run inside Blender): the block kit's slope and slant, like MapleStory 2's.

Usage: blender -b --factory-startup --python blender_slope_blocks.py -- <out dir>

Writes, in Unreal units (centimeters), each as <out dir>/<name>.fbx, in a 100 cell (-50..50 each way, pivot in the
middle, like SM_BTBlock) with the same softly bevelled edges and the same two material slots (Unreal tints each per
instance):
- SM_BTSlope: a 45 degree wedge rising toward +X (the bottom whole, the back at +X a full square, the top a slope from
  the bottom's -X edge to the back's top edge). "Accent" the slope and a band under it (vertically as deep as a block's
  grass lip), "Body" the rest: a grassy bank over earth.
- SM_BTSlopeCorner: a bank's end, a pyramid corner rising toward +X and +Y (the base whole, the top a point over the +X +Y
  corner, two slopes down from it): "Accent" the slopes and a band under them, as the slope's.
- SM_BTSlant: a block cut down its diagonal, the half toward +X+Y kept (in plan a right triangle, its long face facing
  -X-Y). "Accent" the top and the band round its top (as a block), "Body" the rest.
No collision in the meshes: Dice Town's collision is hidden boxes (ARRRandomDiceTown). Then renders a preview
(<out dir>/slope_blocks_preview.png): the two, a block, and a little hill corner made of them.
"""
import math
import sys

import bmesh
import bpy

out_dir = sys.argv[sys.argv.index("--") + 1]

BEVEL = 0.045
BEVEL_SEGMENTS = 2
# A block's grass lip runs from its top down to 0.32 (of -0.5..0.5): this deep
LIP = 0.18


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def solid(bm, points):
    """A convex solid through the points, flat faces merged, its edges rounded like a block's."""
    verts = [bm.verts.new(p) for p in points]
    bmesh.ops.convex_hull(bm, input=verts)
    bmesh.ops.dissolve_limit(bm, angle_limit=0.01, verts=bm.verts[:], edges=bm.edges[:])
    bmesh.ops.bevel(bm, geom=bm.edges[:] + bm.verts[:], offset=BEVEL, offset_type="OFFSET", segments=BEVEL_SEGMENTS,
                    profile=0.5, affect="EDGES", clamp_overlap=True)


def finish(bm, accent):
    """Flat big faces, smooth rounded edges; material 1 where accent(center) says."""
    for f in bm.faces:
        n = f.normal
        big = max(abs(n.x), abs(n.y), abs(n.z)) > 0.999 or abs(abs(n.x) - abs(n.z)) < 1e-3 and abs(n.y) < 1e-3 \
            or abs(abs(n.x) - abs(n.y)) < 1e-3 and abs(n.z) < 1e-3
        f.smooth = not big
        f.material_index = 1 if accent(f.calc_center_median()) else 0


def make_slope(bm):
    h = 0.5
    solid(bm, [(-h, -h, -h), (h, -h, -h), (h, h, -h), (-h, h, -h), (h, -h, h), (h, h, h)])
    # Cut a band under the slope (z = x) as deep as the lip, so its faces take the accent
    bmesh.ops.bisect_plane(bm, geom=bm.verts[:] + bm.edges[:] + bm.faces[:], dist=1e-5, plane_co=(0, 0, -LIP),
                           plane_no=(-1, 0, 1))
    finish(bm, lambda c: c.z - c.x > -LIP + 1e-4)


def make_corner(bm):
    h = 0.5
    solid(bm, [(-h, -h, -h), (h, -h, -h), (h, h, -h), (-h, h, -h), (h, h, h)])
    for plane_no in ((-1, 0, 1), (0, -1, 1)):
        bmesh.ops.bisect_plane(bm, geom=bm.verts[:] + bm.edges[:] + bm.faces[:], dist=1e-5, plane_co=(0, 0, -LIP),
                               plane_no=plane_no)
    finish(bm, lambda c: c.z - min(c.x, c.y) > -LIP + 1e-4)


def make_slant(bm):
    h = 0.5
    solid(bm, [(h, -h, -h), (h, h, -h), (-h, h, -h), (h, -h, h), (h, h, h), (-h, h, h)])
    bmesh.ops.bisect_plane(bm, geom=bm.verts[:] + bm.edges[:] + bm.faces[:], dist=1e-5, plane_co=(0, 0, h - LIP),
                           plane_no=(0, 0, 1))
    finish(bm, lambda c: c.z > h - LIP + 1e-4)


def export(name, build):
    reset()
    bm = bmesh.new()
    build(bm)
    bm.normal_update()
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    for slot in ("Body", "Accent"):
        mesh.materials.append(bpy.data.materials.new(slot))
    # (a Blender unit is a meter: 100 in Unreal, as SM_BTBlock)
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(filepath=f"{out_dir}/{name}.fbx", use_selection=True, object_types={"MESH"},
                             mesh_smooth_type="FACE", path_mode="STRIP", bake_anim=False)
    tris = sum(len(p.vertices) - 2 for p in mesh.polygons)
    print(f"SLOPEBLOCKS {name}: {tris} tris, dims {tuple(round(d, 1) for d in obj.dimensions)}")


export("SM_BTSlope", make_slope)
export("SM_BTSlant", make_slant)
export("SM_BTSlopeCorner", make_corner)

# Preview: slope, slant, block in grass colors, then a little terrace corner: blocks, a slanted corner, a bank on its edge
reset()
GRASS = ((0.45, 0.25, 0.12, 1), (0.25, 0.75, 0.15, 1))
PINK = ((1.0, 0.3, 0.5, 1), (1.0, 0.75, 0.85, 1))
mats = {}


def tint(obj, colors):
    for slot, color in zip(obj.material_slots, colors):
        key = (slot.name, color)
        if key not in mats:
            mat = bpy.data.materials.new(f"m{len(mats)}")
            mat.use_nodes = True
            bsdf = mat.node_tree.nodes["Principled BSDF"]
            bsdf.inputs["Base Color"].default_value = color
            bsdf.inputs["Roughness"].default_value = 0.6
            mats[key] = mat
        slot.material = mats[key]


def place(name, loc, yaw, colors):
    path = f"{out_dir}/{name}.fbx" if name != "SM_BTBlock" else f"{out_dir}/../block_town/SM_BTBlock.fbx"
    bpy.ops.import_scene.fbx(filepath=path)
    obj = bpy.context.selected_objects[0]
    obj.location = loc
    obj.rotation_euler = (0, 0, math.radians(yaw))
    tint(obj, colors)


place("SM_BTSlope", (-4.5, 0, 0), 0, GRASS)
place("SM_BTSlant", (-3, 0, 0), 0, GRASS)
place("SM_BTSlope", (-1.5, 0, 0), 90, PINK)
# A corner: 4 x 4 blocks two high with the corner slanted off, a bank along the front edge
for x in range(4):
    for y in range(4):
        s = x + y
        for z in range(2):
            if s < 1:
                continue
            if s == 1:
                place("SM_BTSlant", (1 + x, y, z), 0, GRASS)
            else:
                place("SM_BTBlock", (1 + x, y, z), 0, GRASS)
# A bank along the front edge: its ends tapered by corners (each rising inward and toward the bank's middle)
place("SM_BTSlopeCorner", (2, 0, 2), 0, GRASS)
place("SM_BTSlope", (3, 0, 2), 90, GRASS)
place("SM_BTSlopeCorner", (4, 0, 2), 90, GRASS)
place("SM_BTSlopeCorner", (1, 2, 2), 0, GRASS)
place("SM_BTSlopeCorner", (1, 3, 2), -90, GRASS)

scene = bpy.context.scene
cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
scene.collection.objects.link(cam)
cam.data.type = "ORTHO"
cam.data.ortho_scale = 10.5
cam.location = (-0.3, -7.0, 5.3)
cam.rotation_euler = (math.radians(55), 0.0, 0.0)
scene.camera = cam
sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
scene.collection.objects.link(sun)
sun.rotation_euler = (math.radians(40), math.radians(25), math.radians(-30))
sun.data.energy = 3.0
world = bpy.data.worlds.new("W")
world.use_nodes = True
world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.55, 0.7, 1.0, 1)
scene.world = world
scene.render.engine = "BLENDER_EEVEE"
scene.render.resolution_x = 1200
scene.render.resolution_y = 560
scene.render.filepath = f"{out_dir}/slope_blocks_preview.png"
bpy.ops.render.render(write_still=True)
