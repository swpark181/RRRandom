"""Prepare Tripo guns for Unreal like blender_rifle.py does the rifle (run inside Blender).

Usage: blender -b --factory-startup --python blender_guns.py -- <out dir> <name>=<tripo fbx>=<grip x>,<grip z>[=flip] ...
  e.g. ... -- C:/work/char-pipeline/output/guns SM_Pistol=C:/.../pistol.fbx=-0.2,0.2

For each gun: meshes joined, its long side along X (and turned round with "flip" when Tripo put the muzzle toward -X),
the origin moved to the middle of the pistol grip (grip x from the gun's center along its length, grip z from its
bottom, both as a share of the length: measured on the side view), scaled to 100 long, decimated to about TARGET_TRIS,
written as <out dir>/<name>.fbx (mesh and UVs; the base color goes in on its own). Prints the muzzle tip (the middle
of the front 3% of the vertices) in the 100-long mesh's units, for URRRandomGunComponent, and renders the side view
(<out dir>/<name>_side.png, muzzle to the right) with a red ball on the grip and a blue one on the muzzle tip.
"""
import math
import sys

import bpy
from mathutils import Matrix, Vector

args = sys.argv[sys.argv.index("--") + 1:]
out_dir = args[0]
TARGET_TRIS = 6000


def bounds(obj):
    pts = [v.co for v in obj.data.vertices]
    return (Vector([min(p[i] for p in pts) for i in range(3)]), Vector([max(p[i] for p in pts) for i in range(3)]))


def ball(location, color):
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.02, location=location)
    marker = bpy.context.active_object
    mat = bpy.data.materials.new("marker")
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes["Principled BSDF"]
    bsdf.inputs["Base Color"].default_value = color
    bsdf.inputs["Emission Color"].default_value = color
    bsdf.inputs["Emission Strength"].default_value = 5
    marker.data.materials.append(mat)
    marker.show_in_front = True


for entry in args[1:]:
    parts = entry.split("=")
    name, fbx, grip = parts[0], parts[1], [float(v) for v in parts[2].split(",")]
    flip = len(parts) > 3 and parts[3] == "flip"

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
    turn = (90.0 if (mx.y - mn.y) > (mx.x - mn.x) else 0.0) + (180.0 if flip else 0.0)
    if turn:
        gun.data.transform(Matrix.Rotation(math.radians(turn), 4, "Z"))
    mn, mx = bounds(gun)
    length = mx.x - mn.x
    origin = Vector(((mn.x + mx.x) / 2 + grip[0] * length, (mn.y + mx.y) / 2, mn.z + grip[1] * length))
    for v in gun.data.vertices:
        v.co = (v.co - origin) / length
    gun.data.update()

    tris = sum(len(p.vertices) - 2 for p in gun.data.polygons)
    if tris > TARGET_TRIS:
        mod = gun.modifiers.new("Decimate", "DECIMATE")
        mod.ratio = TARGET_TRIS / tris
        bpy.ops.object.modifier_apply(modifier=mod.name)
    mn, mx = bounds(gun)
    front = [v.co for v in gun.data.vertices if v.co.x > mx.x - 0.03]
    tip = sum(front, Vector()) / max(len(front), 1)
    gun.name = name
    bpy.ops.object.select_all(action="DESELECT")
    gun.select_set(True)
    bpy.ops.export_scene.fbx(filepath=f"{out_dir}/{name}.fbx", use_selection=True, object_types={"MESH"},
                             mesh_smooth_type="FACE", path_mode="STRIP", bake_anim=False, add_leaf_bones=False)
    size = (mx - mn) * 100
    # Unreal's Y is Blender's -Y through FBX
    print(f"GUN {name}: turned {turn:.0f}, size {size.x:.0f} x {size.y:.0f} x {size.z:.0f}, "
          f"muzzle tip ({tip.x * 100:.0f}, {-tip.y * 100:.0f}, {tip.z * 100:.0f})")

    ball((0, mn.y - 0.01, 0), (1, 0, 0, 1))
    ball((tip.x, mn.y - 0.01, tip.z), (0, 0.3, 1, 1))
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in {e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items} else "BLENDER_EEVEE"
    sc.render.resolution_x, sc.render.resolution_y = 800, 800
    sc.view_settings.view_transform = "Standard"
    world = bpy.data.worlds.new("w")
    sc.world = world
    world.color = (0.6, 0.6, 0.63)
    sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
    sc.collection.objects.link(sun)
    sun.rotation_euler = (math.radians(60), 0, math.radians(20))
    cam = bpy.data.objects.new("c", bpy.data.cameras.new("c"))
    sc.collection.objects.link(cam)
    sc.camera = cam
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = 1.2
    center = (mn + mx) / 2
    # From the gun's left side (-Y), so the muzzle (+X) points right
    cam.location = Vector((center.x, -3.0, center.z))
    cam.rotation_euler = (math.radians(90), 0, 0)
    sc.render.filepath = f"{out_dir}/{name}_side.png"
    bpy.ops.render.render(write_still=True)
