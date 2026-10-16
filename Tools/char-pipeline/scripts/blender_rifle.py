"""Prepare the Tripo rifle for Unreal: grip at the origin, muzzle along +X, 100 long (run inside Blender).

Usage: blender -b --factory-startup --python blender_rifle.py -- <tripo fbx> <out fbx> [<preview png>]

- Tripo exports the gun 1 m long with the muzzle toward Blender +X and the top up (+Z); that is kept
  (Blender +X / +Z come into Unreal as +X / +Z), only scaled so the length is exactly 100 units.
- The origin moves to the middle of the pistol grip, where the right hand closes (GRIP, measured on the side view
  as a share of the length from the gun's center), so the game can put the origin in the palm.
- With a preview path it renders the side view with a small red ball at the new origin, to check the grip spot.
"""
import math
import sys

import bpy
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
src, out = args[0], args[1]
preview = args[2] if len(args) > 2 else None

# Middle of the pistol grip, (x, z) with x from the gun's center along its length and z from its bottom,
# both as a share of the length (the side view: grip below the back of the receiver)
GRIP = (-0.19, 0.255)


def log(msg):
    print(f"RIFLE {msg}")


bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
gun = next(o for o in bpy.context.scene.objects if o.type == 'MESH')
bpy.context.view_layer.objects.active = gun
gun.select_set(True)
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

vs = [v.co for v in gun.data.vertices]
mn = Vector([min(v[i] for v in vs) for i in range(3)])
mx = Vector([max(v[i] for v in vs) for i in range(3)])
length = mx.x - mn.x
grip = Vector(((mn.x + mx.x) / 2 + GRIP[0] * length, (mn.y + mx.y) / 2, mn.z + GRIP[1] * length))
gun.location = -grip
bpy.ops.object.transform_apply(location=True, rotation=False, scale=False)
gun.scale = [1.0 / length] * 3
bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
gun.name = "SM_Rifle"
gun.data.materials[0].name = "Rifle"
vs = [v.co for v in gun.data.vertices]
mn = Vector([min(v[i] for v in vs) for i in range(3)])
mx = Vector([max(v[i] for v in vs) for i in range(3)])
log(f"bounds after: {tuple(round(c * 100, 1) for c in mn)} .. {tuple(round(c * 100, 1) for c in mx)} (UE units)")
log(f"triangles {sum(len(p.vertices) - 2 for p in gun.data.polygons)}")

bpy.ops.export_scene.fbx(filepath=out, use_selection=True, mesh_smooth_type='FACE', path_mode='STRIP',
                         bake_anim=False, add_leaf_bones=False)
log(f"exported {out}")

if preview:
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.02, location=(0, 0, 0))
    marker = bpy.context.active_object
    red = bpy.data.materials.new("marker")
    red.use_nodes = True
    red.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (1, 0, 0, 1)
    red.node_tree.nodes["Principled BSDF"].inputs["Emission Color"].default_value = (1, 0, 0, 1)
    red.node_tree.nodes["Principled BSDF"].inputs["Emission Strength"].default_value = 5
    marker.data.materials.append(red)
    marker.show_in_front = True
    sc = bpy.context.scene
    sc.render.engine = 'BLENDER_EEVEE'
    sc.render.resolution_x, sc.render.resolution_y = 1000, 600
    sc.view_settings.view_transform = "Standard"
    world = bpy.data.worlds.new("w")
    sc.world = world
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (0.6, 0.6, 0.63, 1)
    cam = bpy.data.objects.new("c", bpy.data.cameras.new("c"))
    sc.collection.objects.link(cam)
    sc.camera = cam
    cam.data.type = 'ORTHO'
    cam.data.ortho_scale = 1.15
    center = (mn + mx) / 2
    # From the gun's left side (-Y), so the muzzle points right; the marker sits on the grip's left face
    cam.location = Vector((center.x, -3.0, center.z))
    cam.rotation_euler = (math.radians(90), 0, 0)
    marker.location = (0, mn.y - 0.01, 0)
    sc.render.filepath = preview
    bpy.ops.render.render(write_still=True)
    log(f"preview {preview}")
