"""Print the size and long axis of a Tripo projectile FBX and render it from three views (run inside Blender).

Usage: blender -b --factory-startup --python blender_projectile_info.py -- <fbx> <out prefix> [--px 600]

Writes <out prefix>_q.png (three-quarter from front-right-above), _side.png (camera on -Y looking +Y, so +X is image
right) and _top.png (camera above, +X image right, +Y image up). Prints "PROJ size x y z", "PROJ long +X/..." and the
mean radius of the 15% at each end of the long axis (the thinner end is usually the nose).
"""
import math
import sys

import bpy
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:]
px = 600
if "--px" in argv:
    px = int(argv[argv.index("--px") + 1])
fbx, prefix = argv[0], argv[1]

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
engines = {e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items}
scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in engines else "BLENDER_EEVEE"
scene.render.resolution_x = px
scene.render.resolution_y = px
scene.render.film_transparent = False
scene.view_settings.view_transform = "Standard"
world = bpy.data.worlds.new("World")
world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.86, 0.86, 0.9, 1.0)
world.node_tree.nodes["Background"].inputs[1].default_value = 1.0
scene.world = world
sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
sun.data.energy = 2.5
sun.rotation_euler = (math.radians(40), 0, math.radians(30))
scene.collection.objects.link(sun)

bpy.ops.import_scene.fbx(filepath=fbx)
meshes = [o for o in bpy.context.selected_objects if o.type == "MESH"]
pts = [m.matrix_world @ v.co for m in meshes for v in m.data.vertices]
lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
size = hi - lo
center = (lo + hi) / 2
print(f"PROJ size {size.x:.3f} {size.y:.3f} {size.z:.3f} center {center.x:.3f} {center.y:.3f} {center.z:.3f}")

axis = max(range(3), key=lambda i: size[i])
others = [i for i in range(3) if i != axis]
band = size[axis] * 0.15


def end_radius(sign):
    edge = hi[axis] if sign > 0 else lo[axis]
    sel = [p for p in pts if abs(p[axis] - edge) < band]
    return sum(math.hypot(p[others[0]] - center[others[0]], p[others[1]] - center[others[1]]) for p in sel) / len(sel)


r_pos, r_neg = end_radius(1), end_radius(-1)
name = "XYZ"[axis]
print(f"PROJ long {name} ({size[axis]:.3f}); mean radius of +{name} end {r_pos:.3f}, -{name} end {r_neg:.3f}; "
      f"thinner end {'+' if r_pos < r_neg else '-'}{name}")

cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
scene.collection.objects.link(cam)
scene.camera = cam
cam.data.type = "ORTHO"
span = max(size) * 1.25
VIEWS = {
    "q": Vector((0.8, -1, 0.6)).normalized(),
    "side": Vector((0, -1, 0)),
    "top": Vector((0, -0.0001, 1)).normalized(),
}
for view, d in VIEWS.items():
    cam.location = center + d * span * 4
    cam.rotation_euler = (-d).to_track_quat("-Z", "Y").to_euler()
    cam.data.ortho_scale = span
    cam.data.clip_end = span * 10
    scene.render.filepath = f"{prefix}_{view}.png"
    bpy.ops.render.render(write_still=True)
print("PROJ done")
