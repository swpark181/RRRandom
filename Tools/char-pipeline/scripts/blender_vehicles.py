"""Tidy Tripo vehicles (kickboard, motorbike, car) for riding in the game (run inside Blender).

Usage: blender -b --factory-startup --python blender_vehicles.py -- <out dir> <name>=<tripo fbx>=<length>[=<turn>] ...
  e.g. ... -- C:/work/char-pipeline/output/vehicles SM_Car=C:/.../car.fbx=280=180

For each vehicle: all its meshes joined, transforms applied, its long side turned onto X, then turned <turn> degrees
more about Z (0 or 180, whichever puts its front on +X: Unreal's forward), pivot in the middle of the bottom, scaled
so its length is <length> Unreal units, decimated to about TARGET_TRIS triangles, and written as <out dir>/<name>.fbx
(mesh and UVs only; the Tripo base color texture goes into Unreal on its own). Prints each one's final size.
Then renders all of them side by side from their right side, front to the right (<out dir>/vehicles_side.png), to
check which way they face, and from the game camera's angle (<out dir>/vehicles_preview.png).
"""
import math
import sys

import bpy
from mathutils import Matrix, Vector

args = sys.argv[sys.argv.index("--") + 1:]
out_dir = args[0]
vehicles = [a.split("=") for a in args[1:]]
TARGET_TRIS = 8000


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def bounds(objects):
    # From the vertices themselves: bound_box lags behind edits made straight to the mesh data
    pts = [o.matrix_world @ v.co for o in objects for v in o.data.vertices]
    lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    return lo, hi


results = []
for name, fbx, length, *rest in vehicles:
    turn = float(rest[0]) if rest else 0.0
    reset()
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
    obj = bpy.context.view_layer.objects.active
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

    # Long side along X, then the extra turn
    lo, hi = bounds([obj])
    extent = hi - lo
    yaw = (90.0 if extent.y > extent.x else 0.0) + turn
    obj.data.transform(Matrix.Rotation(math.radians(yaw), 4, "Z"))
    obj.data.update()

    # Pivot: middle of the bottom
    lo, hi = bounds([obj])
    offset = Vector(((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, lo.z))
    for v in obj.data.vertices:
        v.co -= offset
    lo, hi = bounds([obj])
    # Blender meters; Unreal units are centimeters
    scale = float(length) / 100.0 / max(hi.x - lo.x, 1e-6)
    for v in obj.data.vertices:
        v.co *= scale
    obj.data.update()

    tris = sum(len(p.vertices) - 2 for p in obj.data.polygons)
    if tris > TARGET_TRIS:
        mod = obj.modifiers.new("Decimate", "DECIMATE")
        mod.ratio = TARGET_TRIS / tris
        bpy.ops.object.modifier_apply(modifier=mod.name)
    after = sum(len(p.vertices) - 2 for p in obj.data.polygons)
    lo, hi = bounds([obj])
    size = (hi - lo) * 100.0
    obj.name = name
    bpy.ops.export_scene.fbx(filepath=f"{out_dir}/{name}.fbx", use_selection=True, object_types={"MESH"},
                             mesh_smooth_type="FACE", path_mode="STRIP", bake_anim=False)
    print(f"VEHICLE {name}: turned {yaw:.0f}, {tris} -> {after} tris, size {size.x:.0f} x {size.y:.0f} x {size.z:.0f}")
    results.append((name, f"{out_dir}/{name}.fbx", size))


def render(path, rotation, location, ortho):
    scene = bpy.context.scene
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    scene.collection.objects.link(cam)
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = ortho
    cam.location = location
    cam.rotation_euler = rotation
    scene.camera = cam
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(cam, do_unlink=True)
    print(f"VEHICLE preview {path}")


# Previews: every vehicle in a row along Y, each with its textures
reset()
objects = []
for name, path, size in results:
    bpy.ops.import_scene.fbx(filepath=path)
    objects.append((bpy.context.selected_objects[0], size))
scene = bpy.context.scene
sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
scene.collection.objects.link(sun)
sun.rotation_euler = (math.radians(40), math.radians(20), 0.0)
world = bpy.data.worlds.new("World")
world.color = (0.4, 0.4, 0.45)
scene.world = world
scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in {e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items} else "BLENDER_EEVEE"
scene.render.resolution_x, scene.render.resolution_y = 1600, 600
# From their right side (camera on -Y looking +Y), in a row along X so the front (+X) points right
x = 0.0
for obj, size in objects:
    obj.location = (x + size.x / 200.0, 0.0, 0.0)
    x += size.x / 100.0 + 0.5
render(f"{out_dir}/vehicles_side.png", (math.radians(90), 0.0, 0.0), (x / 2, -20.0, 0.6), x * 1.05)
# Straight down, same row, with a 20-unit grid of thin lines to measure seats and decks by (front to the right)
grid = bpy.data.materials.new("Grid")
grid.diffuse_color = (1.0, 1.0, 1.0, 1.0)
for i in range(int(x * 5) + 1):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=(i * 0.2, 0.0, 2.0))
    line = bpy.context.active_object
    line.scale = (0.004 if i % 5 else 0.01, 4.0, 0.001)
    line.data.materials.append(grid)
render(f"{out_dir}/vehicles_top.png", (0.0, 0.0, 0.0), (x / 2, 0.0, 30.0), x * 1.05)
for o in [o for o in bpy.data.objects if o.name.startswith("Cube")]:
    bpy.data.objects.remove(o, do_unlink=True)
# From the game camera: above and behind (-X), pitched 60 degrees down, front up the screen; in a row along Y
y = 0.0
for obj, size in objects:
    obj.location = (0.0, y + size.y / 200.0, 0.0)
    y += size.y / 100.0 + 1.0
longest = max(s.x for _, s in objects) / 100.0
render(f"{out_dir}/vehicles_preview.png", (math.radians(30), 0.0, math.radians(-90)), (-10.0, y / 2, 17.3), max(y, longest) * 1.1)
