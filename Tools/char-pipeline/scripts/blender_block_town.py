"""Block meshes for the TPS map "Dice Town" (run inside Blender), a world of stacked square blocks like MapleStory 2.

Usage: blender -b --factory-startup --python blender_block_town.py -- <out dir>

Writes, in Unreal units (centimeters), each as <out dir>/<name>.fbx:
- SM_BTBlock: a block 100 across with softly bevelled edges, pivot in the middle. Material slot 0 "Body" (the sides and
  bottom), slot 1 "Accent" (the top face and a band round the top of the sides: a grass block's lip). Unreal tints each
  slot per instance, so one mesh is both a plain block (same color twice) and a grass block (green over brown).
- SM_BTDieBlock: the same block with pips sunk into every face (opposite faces sum to 7): slot 0 "Body", slot 1 "Accent"
  (the pips).
Then renders them side by side (<out dir>/block_town_preview.png).
"""
import math
import sys

import bmesh
import bpy
from mathutils import Vector

out_dir = sys.argv[sys.argv.index("--") + 1]

BEVEL = 0.045
BEVEL_SEGMENTS = 2
# The grass lip: the top face and the sides above this height (the block runs -0.5..0.5)
LIP_Z = 0.32
PIP_RADIUS = 0.085
PIP_SPREAD = 0.25
PIP_FLATTEN = 0.35
FACES = [((1, 0, 0), (0, 1, 0), (0, 0, 1), 2), ((-1, 0, 0), (0, 1, 0), (0, 0, 1), 5),
         ((0, 1, 0), (1, 0, 0), (0, 0, 1), 3), ((0, -1, 0), (1, 0, 0), (0, 0, 1), 4),
         ((0, 0, 1), (1, 0, 0), (0, 1, 0), 1), ((0, 0, -1), (1, 0, 0), (0, 1, 0), 6)]
PATTERNS = {1: [(0, 0)], 2: [(-1, -1), (1, 1)], 3: [(-1, -1), (0, 0), (1, 1)], 4: [(-1, -1), (-1, 1), (1, -1), (1, 1)],
            5: [(-1, -1), (-1, 1), (1, -1), (1, 1), (0, 0)], 6: [(-1, -1), (-1, 0), (-1, 1), (1, -1), (1, 0), (1, 1)]}


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def bevelled_cube(bm, lip):
    """A unit cube with rounded edges; with lip, cut round at LIP_Z so the band above can take the top's color."""
    bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.bevel(bm, geom=bm.edges[:] + bm.verts[:], offset=BEVEL, offset_type="OFFSET", segments=BEVEL_SEGMENTS,
                    profile=0.5, affect="EDGES", clamp_overlap=True)
    if lip:
        bmesh.ops.bisect_plane(bm, geom=bm.verts[:] + bm.edges[:] + bm.faces[:], dist=1e-5, plane_co=(0, 0, LIP_Z),
                               plane_no=(0, 0, 1))
    for f in bm.faces:
        n = f.normal
        # Flat big faces, smooth rounded edges
        f.smooth = max(abs(n.x), abs(n.y), abs(n.z)) < 0.999
        f.material_index = 1 if lip and f.calc_center_median().z > LIP_Z else 0


def add_pips(bm):
    """Flattened spheres half sunk into each face in the usual layout, material slot 1."""
    for normal, a, b, value in FACES:
        n, a, b = Vector(normal), Vector(a), Vector(b)
        rot = Vector((0, 0, 1)).rotation_difference(n).to_matrix().to_4x4()
        for u, w in PATTERNS[value]:
            center = n * 0.5 + a * u * PIP_SPREAD + b * w * PIP_SPREAD
            pip = bmesh.ops.create_uvsphere(bm, u_segments=20, v_segments=10, radius=PIP_RADIUS)
            verts = pip["verts"]
            for v in verts:
                v.co.z *= PIP_FLATTEN
                v.co = rot @ v.co + center
            for f in {f for v in verts for f in v.link_faces}:
                f.material_index = 1
                f.smooth = True


def make(name, lip, pips):
    reset()
    bm = bmesh.new()
    bevelled_cube(bm, lip)
    if pips:
        add_pips(bm)
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    for slot in ("Body", "Accent"):
        mesh.materials.append(bpy.data.materials.new(slot))
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(filepath=f"{out_dir}/{name}.fbx", use_selection=True, object_types={"MESH"},
                             mesh_smooth_type="FACE", path_mode="STRIP", bake_anim=False)
    tris = sum(len(p.vertices) - 2 for p in mesh.polygons)
    print(f"BLOCKTOWN {name}: {tris} tris")


make("SM_BTBlock", lip=True, pips=False)
make("SM_BTDieBlock", lip=False, pips=True)

# Preview: a grass block, a pink block, a die block, lit from a little above
reset()
for i, (name, body, accent) in enumerate([("SM_BTBlock", (0.45, 0.25, 0.12, 1), (0.25, 0.75, 0.15, 1)),
                                          ("SM_BTBlock", (1.0, 0.3, 0.5, 1), (1.0, 0.3, 0.5, 1)),
                                          ("SM_BTDieBlock", (1.0, 1.0, 1.0, 1), (0.05, 0.05, 0.08, 1))]):
    bpy.ops.import_scene.fbx(filepath=f"{out_dir}/{name}.fbx")
    obj = bpy.context.selected_objects[0]
    obj.location = (i * 1.4, 0, 0)
    obj.rotation_euler = (0, 0, math.radians(25))
    for slot, color in zip(obj.material_slots, (body, accent)):
        mat = bpy.data.materials.new(f"{name}_{i}_{slot.name}")
        mat.use_nodes = True
        bsdf = mat.node_tree.nodes["Principled BSDF"]
        bsdf.inputs["Base Color"].default_value = color
        bsdf.inputs["Roughness"].default_value = 0.6
        slot.material = mat
scene = bpy.context.scene
cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
scene.collection.objects.link(cam)
cam.data.type = "ORTHO"
cam.data.ortho_scale = 4.4
cam.location = (1.4, -6.0, 3.6)
cam.rotation_euler = (math.radians(62), 0.0, 0.0)
scene.camera = cam
sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
scene.collection.objects.link(sun)
sun.rotation_euler = (math.radians(40), math.radians(25), 0.0)
sun.data.energy = 3.0
world = bpy.data.worlds.new("W")
world.use_nodes = True
world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.55, 0.7, 1.0, 1)
scene.world = world
scene.render.engine = "BLENDER_EEVEE"
scene.render.resolution_x = 1100
scene.render.resolution_y = 420
scene.render.filepath = f"{out_dir}/block_town_preview.png"
bpy.ops.render.render(write_still=True)
