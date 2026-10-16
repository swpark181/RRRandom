"""Turn Tripo grass blocks into unit blocks for building cover out of (run inside Blender).

Usage: blender -b --factory-startup --python blender_grass_blocks.py -- <out dir> <name>=<tripo fbx> [<name>=<tripo fbx> ...]
  e.g. ... -- C:/work/char-pipeline/output/grass SM_GrassBlock_A=.../a.fbx

The whole block (soil sides, grass top) this time, unlike blender_grass_tiles.py's floor tiles. For each block:
- normalizes it to a 100 cube, pivot in the middle of the bottom: the soil body (between its walls, found from the big faces facing each way, not the grass
  hanging over the top edges) fills the cube's sides exactly, so the overhang reaches a little past them and the game
  can stretch the block to any cover cell and line it up with the cover's collision box,
- cuts off anything further out than the grass overhang (KEEP),
- gives every face a normal straight along its main axis (custom split normals; the whole grassy top straight up):
  the top reads as one lit plane and
  each side as one shaded plane, and the bumpy grass no longer speckles with the toon post process's crease ink,
- decimates to about TARGET_TRIS triangles.
Then renders a strip of three cells (each a random block, stretched to 130 x 130 x 160 like a cover cell) from the
game's camera angle (<out dir>/blocks_preview.png).
"""
import math
import random
import sys

import bmesh
import bpy
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
out_dir = args[0]
blocks = [a.split("=", 1) for a in args[1:]]

CUBE = 1.0           # Blender meters (100 in Unreal)
TARGET_TRIS = 8000    # cover takes only a few dozen; cutting harder punched holes in the thin grass on top
KEEP = 0.56          # half-width kept around the middle, as a share of the cube: the overhang, not the stray bottom plate
TOP_FROM = 0.75      # faces above this share of the height are the grassy top
PLATE_BELOW = 0.3    # below this share of the height nothing reaches past the soil walls but the plate


def log(msg):
    print(f"BLOCK {msg}")


def snap(n):
    """The axis direction closest to n."""
    axis = max(range(3), key=lambda i: abs(n[i]))
    v = [0.0, 0.0, 0.0]
    v[axis] = 1.0 if n[axis] > 0 else -1.0
    return v


made = []
for name, src in blocks:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=src)
    obj = next(o for o in bpy.context.scene.objects if o.type == 'MESH')
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

    tris = sum(len(p.vertices) - 2 for p in obj.data.polygons)
    if tris > TARGET_TRIS:
        dec = obj.modifiers.new("Decimate", 'DECIMATE')
        dec.ratio = TARGET_TRIS / tris
        bpy.ops.object.modifier_apply(modifier="Decimate")

    vs = [v.co.copy() for v in obj.data.vertices]
    z_lo, z_hi = min(v.z for v in vs), max(v.z for v in vs)

    def wall(axis, sign):
        """Where the soil wall facing that way stands: the area-weighted median position of the faces facing that way
        in the lower part of the block. The walls are big flat faces, the grass bits small, so stray grass hanging
        down the sides (or a skirt at the bottom) can't drag it out the way the outermost vertex does."""
        faces = sorted((p.center[axis], p.area) for p in obj.data.polygons
                       if p.normal[axis] * sign > 0.9 and p.center.z < z_lo + (z_hi - z_lo) * 0.6)
        half = sum(a for _, a in faces) / 2
        for position, area in faces:
            half -= area
            if half <= 0:
                return position
        return faces[-1][0]

    # The soil body: between its walls, from the bottom to the top of the grass
    lo = Vector((wall(0, -1), wall(1, -1), z_lo))
    hi = Vector((wall(0, 1), wall(1, 1), z_hi))
    size = hi - lo
    center = Vector(((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, lo.z))
    for v in obj.data.vertices:
        p = v.co - center
        v.co = Vector((p.x / size.x * CUBE, p.y / size.y * CUBE, p.z / size.z * CUBE))
    obj.data.update()

    # Nothing past the grass overhang: some blocks have a plate sticking out of one side at the bottom, which showed
    # as a grey ledge at the ends of the cover
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    for axis in range(2):
        for sign in (1, -1):
            co, no = [0.0, 0.0, 0.0], [0.0, 0.0, 0.0]
            co[axis], no[axis] = sign * CUBE * KEEP, sign
            geom = bm.verts[:] + bm.edges[:] + bm.faces[:]
            bmesh.ops.bisect_plane(bm, geom=geom, plane_co=co, plane_no=no, clear_outer=True)
    # Low down there is no overhang, only the plate: nothing past the soil walls there
    low = [f for f in bm.faces if f.calc_center_median().z < CUBE * PLATE_BELOW
           and max(abs(f.calc_center_median().x), abs(f.calc_center_median().y)) > CUBE * 0.505]
    bmesh.ops.delete(bm, geom=low, context='FACES')
    bm.to_mesh(obj.data)
    bm.free()
    obj.data.update()

    # Every corner of a face takes the face's main axis as its normal, except on the grassy top, where all of it
    # faces up: upright blades snapped sideways would ink a speckle at every blade there
    normals = [None] * len(obj.data.loops)
    for poly in obj.data.polygons:
        n = [0.0, 0.0, 1.0] if poly.center.z > CUBE * TOP_FROM else snap(poly.normal)
        for i in poly.loop_indices:
            normals[i] = n
    obj.data.normals_split_custom_set(normals)

    after = sum(len(p.vertices) - 2 for p in obj.data.polygons)
    vs = [v.co for v in obj.data.vertices]
    b_lo = Vector([min(v[i] for v in vs) for i in range(3)])
    b_hi = Vector([max(v[i] for v in vs) for i in range(3)])
    log(f"{name}: body {size.x:.3f} x {size.y:.3f} x {size.z:.3f} -> 100 cube; {tris} -> {after} tris; "
        f"bounds {tuple(round(c * 100, 1) for c in b_lo)} .. {tuple(round(c * 100, 1) for c in b_hi)}")

    obj.name = name
    path = f"{out_dir}/{name}.fbx"
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, mesh_smooth_type='OFF', path_mode='STRIP',
                             bake_anim=False, add_leaf_bones=False)
    made.append((name, path, src))
    log(f"exported {path}")

# Preview: three cover cells side by side, each a random block stretched to 130 x 130 x 160, from above and in front
bpy.ops.wm.read_factory_settings(use_empty=True)
materials = {}
for name, path, src in made:
    bpy.ops.import_scene.fbx(filepath=src)
    original = next(o for o in bpy.context.selected_objects if o.type == 'MESH')
    materials[name] = original.data.materials[0]
    bpy.data.objects.remove(original, do_unlink=True)
rng = random.Random(3)
for i in range(3):
    name, path, _ = rng.choice(made)
    bpy.ops.import_scene.fbx(filepath=path)
    block = next(o for o in bpy.context.selected_objects if o.type == 'MESH')
    block.data.materials[0] = materials[name]
    block.location = (0, (i - 1) * 1.3, 0)
    block.scale = (1.3 * 1.08, 1.3 * 1.08, 1.6)  # overlapped a little, as the game does, so the tops meet
    block.rotation_euler = (0, 0, rng.choice((0, math.pi)))
sc = bpy.context.scene
sc.render.engine = 'BLENDER_EEVEE'
sc.render.resolution_x, sc.render.resolution_y = 1000, 700
sc.view_settings.view_transform = "Standard"
world = bpy.data.worlds.new("w")
sc.world = world
world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.6, 0.65, 0.6, 1)
sun = bpy.data.objects.new("s", bpy.data.lights.new("s", 'SUN'))
sun.data.energy = 3
sun.rotation_euler = (math.radians(45), 0, math.radians(30))
sc.collection.objects.link(sun)
cam = bpy.data.objects.new("c", bpy.data.cameras.new("c"))
sc.collection.objects.link(cam)
sc.camera = cam
cam.data.type = 'ORTHO'
cam.data.ortho_scale = 5.5
# The game's camera: pitched 60 degrees down, looking along +X
direction = Vector((math.cos(math.radians(60)), 0, -math.sin(math.radians(60))))
cam.location = Vector((0, 0, 0.8)) - direction * 10
cam.rotation_euler = direction.to_track_quat('-Z', 'Y').to_euler()
sc.render.filepath = f"{out_dir}/blocks_preview.png"
bpy.ops.render.render(write_still=True)
log(f"preview {sc.render.filepath}")
