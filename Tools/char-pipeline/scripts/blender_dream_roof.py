"""A cartoon hip roof for Dice Town's houses (run inside Blender), in place of the stepped roof of blocks.

Usage: blender -b --factory-startup --python blender_dream_roof.py -- <out dir>

Writes <out dir>/SM_BTRoof.fbx in Unreal units (centimeters): a hip roof over a 100 x 100 footprint (X and Y -50..50,
the eaves' outer edge), 100 high (bottom at z 0, the point at 100), to be stretched over each house. Rows of soft rounded
shingle bands overlap like a toy's roof tiles; each band's lower lip juts out a little. Material slots like the blocks
(Unreal tints each per instance): "Body" the bands, "Accent" the eave board along the bottom and the little cap at the
top. Then renders a preview (<out dir>/roof_preview.png).
"""
import math
import sys

import bmesh
import bpy

out_dir = sys.argv[sys.argv.index("--") + 1]

ROWS = 4
LIP = 0.05
EAVE = 0.06


def hull_box(bm, rings, material):
    """A solid from square rings [(z, half), ...] (convex hull): flat faces, softly rounded edges."""
    verts = []
    for z, half in rings:
        for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
            verts.append(bm.verts.new((sx * half, sy * half, z)))
    bmesh.ops.convex_hull(bm, input=verts)
    # The hull comes in triangles: merge the flat ones back into faces before rounding the edges
    bmesh.ops.dissolve_limit(bm, angle_limit=0.01, verts=bm.verts[:], edges=bm.edges[:])
    for f in bm.faces:
        f.smooth = False
    result = bmesh.ops.bevel(bm, geom=bm.edges[:], offset=0.014, offset_type="OFFSET", segments=3, profile=0.5,
                             affect="EDGES", clamp_overlap=True)
    for f in result["faces"]:
        f.smooth = True
    return material


def make_roof():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bm_all = bmesh.new()
    pieces = []
    # Each piece as its own bmesh, then joined (convex hulls must not see each other)
    eave = bmesh.new()
    hull_box(eave, [(0.0, 0.5), (EAVE, 0.5), (EAVE, 0.5 - 0.02)], 1)
    pieces.append((eave, 1))
    for row in range(ROWS):
        t0 = row / ROWS
        t1 = (row + 1) / ROWS + 0.06
        z0 = EAVE * 0.6 + t0 * (0.92 - EAVE)
        z1 = EAVE * 0.6 + min(t1, 1.0) * (0.92 - EAVE)
        h0 = 0.5 * (1.0 - t0) + LIP * (1.0 - t0 * 0.5)
        h1 = max(0.5 * (1.0 - min(t1, 1.0)), 0.035)
        band = bmesh.new()
        # The lower lip: a short upright edge before the slope
        hull_box(band, [(z0, h0), (z0 + 0.05, h0), (z1, h1)], 0)
        pieces.append((band, 0))
    cap = bmesh.new()
    hull_box(cap, [(0.86, 0.075), (0.93, 0.075), (1.0, 0.02)], 1)
    pieces.append((cap, 1))

    mesh = bpy.data.meshes.new("SM_BTRoof")
    obj = bpy.data.objects.new("SM_BTRoof", mesh)
    bpy.context.scene.collection.objects.link(obj)
    for slot in ("Body", "Accent"):
        mesh.materials.append(bpy.data.materials.new(slot))
    for piece, material in pieces:
        for f in piece.faces:
            f.material_index = material


        temp = bpy.data.meshes.new("piece")
        piece.to_mesh(temp)
        piece.free()
        bm_all.from_mesh(temp)
        bpy.data.meshes.remove(temp)
    bm_all.to_mesh(mesh)
    bm_all.free()
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(filepath=f"{out_dir}/SM_BTRoof.fbx", use_selection=True, object_types={"MESH"},
                             mesh_smooth_type="FACE", path_mode="STRIP", bake_anim=False)
    print(f"DREAMROOF SM_BTRoof: {sum(len(p.vertices) - 2 for p in mesh.polygons)} tris, dims {obj.dimensions[:]}")
    return obj


roof = make_roof()
# Preview: stretched over a 10 x 12 house footprint, pink with a cream eave
roof.scale = (1.2, 1.4, 0.6)
for slot, color in zip(roof.material_slots, ((1.0, 0.25, 0.55, 1), (1.0, 0.92, 0.85, 1))):
    mat = slot.material
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes["Principled BSDF"]
    bsdf.inputs["Base Color"].default_value = color
    bsdf.inputs["Roughness"].default_value = 0.45
scene = bpy.context.scene
cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
scene.collection.objects.link(cam)
cam.data.type = "ORTHO"
cam.data.ortho_scale = 2.4
cam.location = (2.2, -2.6, 1.9)
direction = -cam.location
cam.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
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
scene.render.resolution_x = 800
scene.render.resolution_y = 600
scene.render.filepath = f"{out_dir}/roof_preview.png"
bpy.ops.render.render(write_still=True)
