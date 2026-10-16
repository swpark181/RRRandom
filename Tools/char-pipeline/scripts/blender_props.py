"""Tidy Tripo props (trees, bushes, rocks, pond plants) for the island (run inside Blender).

Usage: blender -b --factory-startup --python blender_props.py -- <out dir> <name>=<tripo fbx>=<size>[h|w][=strip] ...
  e.g. ... -- C:/work/char-pipeline/output/props SM_PineTree=C:/.../pine.fbx=520h SM_LilyPads=C:/.../lily.fbx=200w

For each prop: all its meshes joined, transforms applied, pivot in the middle of the bottom, scaled so its height (h) or
its widest side (w) is <size> Unreal units, decimated to about TARGET_TRIS triangles, and written as <out dir>/<name>.fbx
(mesh and UVs only; the Tripo base color texture goes into Unreal on its own). Prints each prop's final size.
"strip" also deletes a grey ground plate under the model (low faces whose texture is grey there; see strip_grey_base).
Then renders all of them side by side from the game's camera angle (<out dir>/props_preview.png).
"""
import math
import sys

import bpy
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
out_dir = args[0]
props = [a.split("=") for a in args[1:]]
# PROPS_TARGET_TRIS in the environment overrides it (Dice Town keeps Tripo's 20,000: decimating spoiled the shading)
import os
TARGET_TRIS = int(os.environ.get("PROPS_TARGET_TRIS", "6000"))


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def bounds(objects):
    pts = [o.matrix_world @ Vector(c) for o in objects for c in o.bound_box]
    lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    return lo, hi


def strip_grey_base(obj):
    """Deletes the grey ground plate Tripo sometimes puts under a model: faces low down (under BASE_HEIGHT of the
    height) whose base color texture is grey there."""
    import bmesh
    image = next((n.image for m in obj.data.materials if m and m.use_nodes for n in m.node_tree.nodes
                  if n.type == "TEX_IMAGE" and n.image), None)
    if image is None:
        print(f"PROP {obj.name}: no texture, base kept")
        return
    width, height = image.size
    pixels = image.pixels[:]
    top = max(v.co.z for v in obj.data.vertices)
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    uv = bm.loops.layers.uv.active
    doomed = []
    for face in bm.faces:
        if face.calc_center_median().z > top * BASE_HEIGHT:
            continue
        u = sum(l[uv].uv.x for l in face.loops) / len(face.loops)
        v = sum(l[uv].uv.y for l in face.loops) / len(face.loops)
        x = min(width - 1, max(0, int((u % 1.0) * width)))
        y = min(height - 1, max(0, int((v % 1.0) * height)))
        r, g, b = pixels[(y * width + x) * 4:(y * width + x) * 4 + 3]
        if max(r, g, b) - min(r, g, b) < GREY_SPREAD:
            doomed.append(face)
    bmesh.ops.delete(bm, geom=doomed, context="FACES")
    bm.to_mesh(obj.data)
    bm.free()
    print(f"PROP {obj.name}: removed {len(doomed)} grey faces of the base")


# The plate: faces this low (share of the height) and this colorless (max - min channel) go
BASE_HEIGHT = 0.06
GREY_SPREAD = 0.12

results = []
for name, fbx, size, *flags in props:
    strip = "strip" in flags
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

    # Pivot: middle of the bottom
    lo, hi = bounds([obj])
    offset = Vector(((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, lo.z))
    for v in obj.data.vertices:
        v.co -= offset
    lo, hi = bounds([obj])
    extent = hi - lo
    # Blender meters; Unreal units are centimeters
    target = float(size[:-1]) / 100.0
    measured = extent.z if size.endswith("h") else max(extent.x, extent.y)
    scale = target / max(measured, 1e-6)
    for v in obj.data.vertices:
        v.co *= scale
    obj.data.update()

    if strip:
        strip_grey_base(obj)
        obj.data.update()
    tris = sum(len(p.vertices) - 2 for p in obj.data.polygons)
    if tris > TARGET_TRIS:
        mod = obj.modifiers.new("Decimate", "DECIMATE")
        mod.ratio = TARGET_TRIS / tris
        bpy.ops.object.modifier_apply(modifier=mod.name)
    after = sum(len(p.vertices) - 2 for p in obj.data.polygons)
    lo, hi = bounds([obj])
    extent = (hi - lo) * 100.0
    obj.name = name
    bpy.ops.export_scene.fbx(filepath=f"{out_dir}/{name}.fbx", use_selection=True, object_types={"MESH"},
                             mesh_smooth_type="FACE", path_mode="STRIP", bake_anim=False)
    print(f"PROP {name}: {tris} -> {after} tris, size {extent.x:.0f} x {extent.y:.0f} x {extent.z:.0f}")
    results.append((name, f"{out_dir}/{name}.fbx", extent))

# Preview: every prop in a row, from above at the game camera's 60 degree pitch
reset()
x = 0.0
for name, path, extent in results:
    bpy.ops.import_scene.fbx(filepath=path)
    obj = bpy.context.selected_objects[0]
    obj.location.x = x + extent.x / 200.0
    x += extent.x / 100.0 + 1.0
scene = bpy.context.scene
cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
scene.collection.objects.link(cam)
cam.data.type = "ORTHO"
cam.data.ortho_scale = max(x, 4.0) * 1.1
cam.location = (x / 2 - 0.5, -12.0, 12.0 * math.tan(math.radians(60)) / 1.0 / 1.7)
cam.rotation_euler = (math.radians(60), 0.0, 0.0)
scene.camera = cam
sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
scene.collection.objects.link(sun)
sun.rotation_euler = (math.radians(40), math.radians(20), 0.0)
scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in {e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items} else "BLENDER_EEVEE"
scene.render.resolution_x, scene.render.resolution_y = 1600, 600
scene.render.filepath = f"{out_dir}/props_preview.png"
bpy.ops.render.render(write_still=True)
print(f"PROP preview {scene.render.filepath}")
