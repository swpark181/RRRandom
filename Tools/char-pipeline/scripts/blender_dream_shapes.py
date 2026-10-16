"""Extra shapes for Dice Town's dreamier pass (run inside Blender), next to blender_block_town.py's blocks.

Usage: blender -b --factory-startup --python blender_dream_shapes.py -- <out dir>

Writes, in Unreal units (centimeters), each as <out dir>/<name>.fbx, pivot in the middle, material slots "Body" and
"Accent" like the blocks (Unreal tints each per instance):
- SM_BTPuff: a block 100 across with its edges rounded right off, a marshmallow (clouds, cotton candy crowns). Slot 1 on
  the top face only, the rest slot 0.
- SM_BTStar: a chunky five pointed star 100 across (point up, facing +X, 34 thick), bevelled. Front and back slot 1,
  the sides slot 0.
- SM_BTMoon: a crescent 100 high (facing +X, 30 thick), bevelled, the same slots as the star.
Then renders them side by side (<out dir>/dream_shapes_preview.png).
"""
import math
import sys

import bmesh
import bpy

out_dir = sys.argv[sys.argv.index("--") + 1]


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def export(name, bm):
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
    print(f"DREAMSHAPES {name}: {sum(len(p.vertices) - 2 for p in mesh.polygons)} tris")


def puff():
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.bevel(bm, geom=bm.edges[:] + bm.verts[:], offset=0.22, offset_type="OFFSET", segments=5, profile=0.5,
                    affect="EDGES", clamp_overlap=True)
    for f in bm.faces:
        n = f.normal
        f.smooth = max(abs(n.x), abs(n.y), abs(n.z)) < 0.999
        f.material_index = 1 if n.z > 0.999 else 0
    return bm


def slab(outline, thickness):
    """An outline in the YZ plane (facing +X) extruded through X, its edges bevelled; front and back slot 1."""
    bm = bmesh.new()
    verts = [bm.verts.new((-thickness * 0.5, y, z)) for y, z in outline]
    face = bm.faces.new(verts)
    bmesh.ops.recalc_face_normals(bm, faces=[face])
    ext = bmesh.ops.extrude_face_region(bm, geom=[face])
    moved = [e for e in ext["geom"] if isinstance(e, bmesh.types.BMVert)]
    bmesh.ops.translate(bm, verts=moved, vec=(thickness, 0, 0))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bmesh.ops.bevel(bm, geom=bm.edges[:], offset=0.05, offset_type="OFFSET", segments=2, profile=0.5,
                    affect="EDGES", clamp_overlap=True)
    for f in bm.faces:
        f.smooth = abs(f.normal.x) < 0.999
        f.material_index = 1 if abs(f.normal.x) > 0.999 else 0
    return bm


def star():
    points = []
    for i in range(10):
        angle = math.pi / 2 + i * math.pi / 5
        radius = 0.5 if i % 2 == 0 else 0.23
        points.append((math.cos(angle) * radius, math.sin(angle) * radius - 0.03))
    return slab(points, 0.34)


def moon():
    # The outer circle (radius 0.5) minus a circle shifted right: walk the outer arc, then back along the inner one
    outer_r, inner_r, shift = 0.5, 0.42, 0.24
    # Where the two circles cross
    x = (outer_r ** 2 - inner_r ** 2 + shift ** 2) / (2 * shift)
    z = math.sqrt(max(outer_r ** 2 - x ** 2, 0.0))
    a0 = math.atan2(z, x)
    b0 = math.atan2(z, x - shift)
    points = []
    steps = 22
    for i in range(steps + 1):
        a = a0 + (2 * math.pi - 2 * a0) * i / steps
        points.append((math.cos(a) * outer_r, math.sin(a) * outer_r))
    # Back from the lower crossing round the inner circle's left side to the upper one
    for i in range(1, steps):
        b = (2 * math.pi - b0) - (2 * math.pi - 2 * b0) * i / steps
        points.append((shift + math.cos(b) * inner_r, math.sin(b) * inner_r))
    # Centre it across
    mid = (min(p[0] for p in points) + max(p[0] for p in points)) * 0.5
    return slab([(p[0] - mid, p[1]) for p in points], 0.30)


for name, make in (("SM_BTPuff", puff), ("SM_BTStar", star), ("SM_BTMoon", moon)):
    reset()
    export(name, make())

# Preview: a puff, a star, a moon, lit from a little above
reset()
for i, (name, body, accent) in enumerate([("SM_BTPuff", (1.0, 0.6, 0.8, 1), (1.0, 0.75, 0.9, 1)),
                                          ("SM_BTStar", (1.0, 0.7, 0.1, 1), (1.0, 0.85, 0.2, 1)),
                                          ("SM_BTMoon", (1.0, 0.75, 0.2, 1), (1.0, 0.9, 0.4, 1))]):
    bpy.ops.import_scene.fbx(filepath=f"{out_dir}/{name}.fbx")
    obj = bpy.context.selected_objects[0]
    obj.location = (i * 1.4, 0, 0)
    obj.rotation_euler = (0, 0, math.radians(-70))
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
scene.render.filepath = f"{out_dir}/dream_shapes_preview.png"
bpy.ops.render.render(write_still=True)
