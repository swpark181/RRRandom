"""Normalize the r90 creature-mouth toy guns from Tripo for Unreal, keeping their textures (run inside Blender).

Usage: blender -b --factory-startup --python blender_guns_r90.py -- <out dir> <name>=<tripo fbx>[=<grip x>,<grip z>][=flip] ...

Like blender_guns.py: meshes joined, the long side along X, the muzzle toward +X, the origin on the middle of the pistol
grip, scaled to 100 long (1 m in Blender = 100 cm in Unreal). Without a grip given, the grip is found as the rearmost
part hanging below the body (vertices 8-20% up, split into runs along X); "flip" turns the gun round first when
Tripo put the muzzle toward -X. A given grip is x from the gun's center along its length and z from its bottom, both as
a share of the length, measured on the side view the script renders.
Writes <out dir>/<name>.fbx with the base color embedded, and prints for GunLooks, on the 100-long mesh from the grip:
the muzzle tip (middle of the front 3% of the vertices) and the hold (the back of the body at the muzzle's height,
like the old toy guns are held). Renders <out dir>/<name>_side.png (muzzle right; red ball grip, blue tip, yellow hold)
and <name>_34.png (three-quarter, textured, no balls) for the comparison page.
"""
import math
import sys

import bpy
from mathutils import Matrix, Vector

args = sys.argv[sys.argv.index("--") + 1:]
out_dir = args[0]


def bounds(obj):
    pts = [v.co for v in obj.data.vertices]
    return (Vector([min(p[i] for p in pts) for i in range(3)]), Vector([max(p[i] for p in pts) for i in range(3)]))


def find_grip(gun, mn, mx):
    """Middle of the rearmost run of low vertices along X (rear = the -X end, before any flip): x, z in world units."""
    h = mx.z - mn.z
    low = [v.co for v in gun.data.vertices if mn.z + h * 0.08 < v.co.z < mn.z + h * 0.2]
    xs = sorted(p.x for p in low)
    gap = (mx.x - mn.x) * 0.04
    runs, start = [], xs[0]
    for a, b in zip(xs, xs[1:]):
        if b - a > gap:
            runs.append((start, a))
            start = b
    runs.append((start, xs[-1]))
    return runs


def ball(location, color):
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.02, location=location)
    marker = bpy.context.active_object
    mat = bpy.data.materials.new("marker")
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes["Principled BSDF"]
    bsdf.inputs["Base Color"].default_value = color
    bsdf.inputs["Emission Color"].default_value = color
    bsdf.inputs["Emission Strength"].default_value = 1.5
    marker.data.materials.append(mat)
    marker.show_in_front = True
    return marker


def render(path, cam_loc, cam_rot, scale, center):
    sc = bpy.context.scene
    cam = sc.camera
    cam.location = center + Vector(cam_loc)
    cam.rotation_euler = cam_rot
    cam.data.ortho_scale = scale
    sc.render.filepath = path
    bpy.ops.render.render(write_still=True)


for entry in args[1:]:
    parts = entry.split("=")
    name, fbx = parts[0], parts[1]
    flip = "flip" in parts[2:]
    given = [p for p in parts[2:] if p != "flip"]
    grip = [float(v) for v in given[0].split(",")] if given else None

    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=fbx)
    meshes = [o for o in bpy.data.objects if o.type == "MESH"]
    for o in list(bpy.data.objects):
        if o.type != "MESH":
            bpy.data.objects.remove(o, do_unlink=True)
    bpy.ops.object.select_all(action="DESELECT")
    for o in meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1:
        bpy.ops.object.join()
    gun = bpy.context.view_layer.objects.active
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

    mn, mx = bounds(gun)
    turn = 90.0 if (mx.y - mn.y) > (mx.x - mn.x) else 0.0
    if turn:
        gun.data.transform(Matrix.Rotation(math.radians(turn), 4, "Z"))
    mn, mx = bounds(gun)
    length = mx.x - mn.x
    if flip:
        gun.data.transform(Matrix.Rotation(math.pi, 4, "Z"))
        turn += 180.0
        mn, mx = bounds(gun)
    if grip is None:
        # The pistol grip is the rearmost run (an SMG's magazine hangs in front of it)
        run = min(find_grip(gun, mn, mx), key=lambda r: r[0])
        gx = (run[0] + run[1]) / 2
        origin = Vector((gx, (mn.y + mx.y) / 2, mn.z + (mx.z - mn.z) * 0.18))
        grip = ((gx - (mn.x + mx.x) / 2) / length, (origin.z - mn.z) / length)
    else:
        origin = Vector(((mn.x + mx.x) / 2 + grip[0] * length, (mn.y + mx.y) / 2, mn.z + grip[1] * length))
    for v in gun.data.vertices:
        v.co = (v.co - origin) / length
    gun.data.update()

    mn, mx = bounds(gun)
    front = [v.co for v in gun.data.vertices if v.co.x > mx.x - 0.03]
    tip = sum(front, Vector()) / max(len(front), 1)
    # Hold: the back of the body (8 units in from the rear) at the muzzle's height, kept inside that slice of the body
    hx = mn.x + 0.08
    slice_z = [v.co.z for v in gun.data.vertices if abs(v.co.x - hx) < 0.03 and v.co.z > 0.05]
    hz = min(max(tip.z, min(slice_z)), max(slice_z)) if slice_z else tip.z
    hold = Vector((hx, 0, hz))
    tris = sum(len(p.vertices) - 2 for p in gun.data.polygons)

    gun.name = name
    bpy.ops.object.select_all(action="DESELECT")
    gun.select_set(True)
    bpy.ops.export_scene.fbx(filepath=f"{out_dir}/{name}.fbx", use_selection=True, object_types={"MESH"},
                             mesh_smooth_type="FACE", path_mode="COPY", embed_textures=True, bake_anim=False,
                             add_leaf_bones=False)
    size = (mx - mn) * 100
    # Unreal's Y is Blender's -Y through FBX
    print(f"GUN {name}: turned {turn:.0f}, grip {grip[0]:.3f},{grip[1]:.3f}, {tris} tris, "
          f"size {size.x:.0f} x {size.y:.0f} x {size.z:.0f}, tip ({tip.x * 100:.0f}, {-tip.y * 100:.0f}, {tip.z * 100:.0f}), "
          f"hold ({hold.x * 100:.0f}, 0, {hold.z * 100:.0f})")

    sc = bpy.context.scene
    engines = {e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items}
    sc.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in engines else "BLENDER_EEVEE"
    sc.render.resolution_x, sc.render.resolution_y = 800, 800
    sc.view_settings.view_transform = "Standard"
    world = bpy.data.worlds.new("w")
    sc.world = world
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.75, 0.75, 0.78, 1)
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = 1.0
    sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
    sun.data.energy = 2.5
    sc.collection.objects.link(sun)
    sun.rotation_euler = (math.radians(50), 0, math.radians(-30))
    cam = bpy.data.objects.new("c", bpy.data.cameras.new("c"))
    sc.collection.objects.link(cam)
    sc.camera = cam
    cam.data.type = "ORTHO"
    center = (mn + mx) / 2
    render(f"{out_dir}/{name}_34.png", (1.6, -2.4, 1.0), (math.radians(72), 0, math.radians(34)), 1.25, center)
    ball((0, mn.y - 0.01, 0), (1, 0, 0, 1))
    ball((tip.x, mn.y - 0.01, tip.z), (0, 0.3, 1, 1))
    ball((hold.x, mn.y - 0.01, hold.z), (1, 0.75, 0, 1))
    # From the gun's left side (-Y), so the muzzle (+X) points right
    render(f"{out_dir}/{name}_side.png", (0, -3.0, 0), (math.radians(90), 0, 0), 1.2, center)
