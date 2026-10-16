"""Start a rig for a creature Mixamo can't rig (four legs, wings): the Tripo mesh on an armature with one bone (run in Blender).

Usage: blender -b --factory-startup --python blender_creature_base.py -- <tripo fbx> <out fbx> <grid png prefix> [height m]

The mesh is joined, its transforms applied, scaled to the height (default: as it is), feet on the floor (z 0) and centered
over the origin; it faces -Y (Tripo's Blender preset). The armature has one bone, "Hips", up through the middle of the
body, and every vertex is on it. blender_animal_tail.py then adds the chains (tail, wings, neck, legs) one per run.
Also renders the mesh over a 10 cm grid from the side (+X, front to the left), the back (+Y) and the top (+Z), to read
the chains' joints off: the red lines are 0, the dark ones every 50 cm, in the Blender units the chains are given in.
"""
import math
import sys

import bpy
from mathutils import Matrix, Vector

args = sys.argv[sys.argv.index("--") + 1:]
src, out, grid = args[0], args[1], args[2]
height = float(args[3]) if len(args) > 3 else 0.0
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
meshes = [o for o in bpy.data.objects if o.type == "MESH"]
for o in bpy.data.objects:
    o.select_set(o in meshes)
bpy.context.view_layer.objects.active = meshes[0]
bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
if len(meshes) > 1:
    bpy.ops.object.join()
mesh = bpy.context.view_layer.objects.active
for o in list(bpy.data.objects):
    if o != mesh:
        bpy.data.objects.remove(o, do_unlink=True)
pts = [v.co for v in mesh.data.vertices]
lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
scale = height / (hi.z - lo.z) if height > 0 else 1.0
mesh.data.transform(Matrix.Scale(scale, 4) @ Matrix.Translation(Vector((-(lo.x + hi.x) / 2, -(lo.y + hi.y) / 2, -lo.z))))
pts = [v.co for v in mesh.data.vertices]
lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
print(f"BASE {len(pts)} vertices, x {lo.x:+.3f}..{hi.x:+.3f} y {lo.y:+.3f}..{hi.y:+.3f} z {lo.z:+.3f}..{hi.z:+.3f}")

# The body's middle: the mean of the vertices in the middle third across (no wings) and below the wings' reach
body = [p for p in pts if abs(p.x) < (hi.x - lo.x) / 6]
mid = sum(body, Vector()) / len(body)
bpy.ops.object.armature_add(location=(0, 0, 0))
arm = bpy.context.active_object
arm.name = "Armature"
bpy.ops.object.mode_set(mode="EDIT")
bone = arm.data.edit_bones[0]
bone.name = "Hips"
bone.head = Vector((0, mid.y, mid.z - 0.05))
bone.tail = Vector((0, mid.y, mid.z + 0.05))
bpy.ops.object.mode_set(mode="OBJECT")
print(f"BASE Hips at (0, {mid.y:+.3f}, {mid.z:+.3f})")
group = mesh.vertex_groups.new(name="Hips")
group.add(list(range(len(mesh.data.vertices))), 1.0, "REPLACE")
mesh.parent = arm
mod = mesh.modifiers.new("Armature", "ARMATURE")
mod.object = arm
bpy.ops.export_scene.fbx(
    filepath=out, use_selection=False, object_types={"ARMATURE", "MESH"}, add_leaf_bones=False, bake_anim=False,
    primary_bone_axis="Y", secondary_bone_axis="X", use_armature_deform_only=False, mesh_smooth_type="OFF",
    path_mode="COPY", embed_textures=False)
print(f"BASE wrote {out}")

# The grid pictures
scene = bpy.context.scene
engines = {e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items}
scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in engines else "BLENDER_EEVEE"
scene.render.resolution_x = scene.render.resolution_y = 1000
scene.view_settings.view_transform = "Standard"
world = bpy.data.worlds.new("W")
scene.world = world
world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (1, 1, 1, 1)
sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
sun.data.energy = 3
sun.rotation_euler = (math.radians(50), 0, math.radians(40))
scene.collection.objects.link(sun)
cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
scene.collection.objects.link(cam)
cam.data.type = "ORTHO"
span = max(hi - lo) * 1.15
cam.data.ortho_scale = span
scene.camera = cam


def material(name, rgb):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    bsdf = m.node_tree.nodes["Principled BSDF"]
    bsdf.inputs["Base Color"].default_value = (*rgb, 1)
    bsdf.inputs["Emission Color"].default_value = (*rgb, 1)
    bsdf.inputs["Emission Strength"].default_value = 1.0
    return m


grey, dark, red = material("g", (0.4, 0.75, 1.0)), material("d", (0.0, 0.1, 0.6)), material("r", (1, 0, 0))


def bar(loc, size, mat):
    bpy.ops.mesh.primitive_cube_add(location=loc)
    o = bpy.context.active_object
    o.scale = Vector(size) / 2
    o.data.materials.append(mat)
    return o


steps = range(int(math.floor(-span / 0.1)) - 1, int(math.ceil(span / 0.1)) + 2)
center = (lo + hi) / 2
for view in ("side", "back", "top"):
    made = []
    for i in steps:
        v = i * 0.1
        mat = red if i == 0 else (dark if i % 5 == 0 else grey)
        w = 0.003 if i % 5 else 0.006
        if view == "side":  # plane behind the model at x = lo.x - 0.1, lines of y and z
            made.append(bar((lo.x - 0.1, center.y, v), (0.001, span * 2, w), mat))
            made.append(bar((lo.x - 0.1, v, center.z), (0.001, w, span * 2), mat))
        elif view == "back":  # plane in front at y = lo.y - 0.1, lines of x and z
            made.append(bar((center.x, lo.y - 0.1, v), (span * 2, 0.001, w), mat))
            made.append(bar((v, lo.y - 0.1, center.z), (w, 0.001, span * 2), mat))
        else:  # floor plane under it, lines of x and y
            made.append(bar((center.x, v, -0.05), (span * 2, w, 0.001), mat))
            made.append(bar((v, center.y, -0.05), (w, span * 2, 0.001), mat))
    if view == "side":
        cam.location = (3, center.y, center.z)
        cam.rotation_euler = (math.radians(90), 0, math.radians(90))
    elif view == "back":
        cam.location = (center.x, 3, center.z)
        cam.rotation_euler = (math.radians(90), 0, math.radians(180))
    else:
        cam.location = (center.x, center.y, 3)
        cam.rotation_euler = (0, 0, 0)
    scene.render.filepath = f"{grid}_{view}.png"
    bpy.ops.render.render(write_still=True)
    for o in made:
        bpy.data.objects.remove(o)
print(f"BASE grid {grid}_side|back|top.png, {span / 1000 * 100:.3f} m per 100 px; picture middle = {tuple(round(c, 3) for c in center)}")
