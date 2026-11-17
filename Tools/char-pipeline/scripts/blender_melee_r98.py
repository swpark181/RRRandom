"""Normalize round 98's Tripo melee items (hammer, dagger) and the shield for Unreal, keeping their textures (run in Blender).

Usage: blender -b --factory-startup --python blender_melee_r98.py -- <out dir> <name>=<tripo fbx>=<kind>[=<grip>][=flip] ...

kind "melee": the longest side goes along X, its bulkier end (the hammer's head, the dagger's fox hilt) chosen by
  "head" (bulky end to +X: a hammer's head forward) or "hilt" (bulky end to -X: a dagger's blade forward); the origin
  on the long axis at <grip> (share of the length from the -X end, default 0.15), the whole thing 100 long.
kind "melee" also takes "box" (align by the bounding box, not the principal axes: a hammer's head tilts those).
kind "shield": the thinnest side becomes X with the face (the side away from the handle) toward +X, Z up; the origin at
  the middle of the back, the round 100 across. "flip" turns the face round when Tripo put it the other way; "roll:<deg>" turns it about its normal.
Writes <out dir>/<name>.fbx (base color embedded) and prints on the 100-scale mesh: the size, the tip (the +X end's
middle) and the grip; renders <name>_side.png (from -Y, +X right; red ball grip, blue tip) and <name>_34.png.
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


def render(path, cam_loc, cam_rot, scale, center):
    sc = bpy.context.scene
    cam = sc.camera
    cam.location = center + Vector(cam_loc)
    cam.rotation_euler = cam_rot
    cam.data.ortho_scale = scale
    sc.render.filepath = path
    bpy.ops.render.render(write_still=True)


def spread(verts, axis, lo, hi):
    """Cross-section size (product of the other two extents) of the vertices with axis coordinate in [lo, hi]."""
    sel = [v for v in verts if lo <= v[axis] <= hi]
    if not sel:
        return 0.0
    others = [i for i in range(3) if i != axis]
    return math.prod(max(v[i] for v in sel) - min(v[i] for v in sel) for i in others)


for entry in args[1:]:
    parts = entry.split("=")
    name, fbx, kind = parts[0], parts[1], parts[2]
    flip = "flip" in parts[3:]
    box = "box" in parts[3:]
    roll = next((float(p[5:]) for p in parts[3:] if p.startswith("roll:")), 0.0)
    forced = next((int(p[5:]) for p in parts[3:] if p.startswith("axis:")), None)  # "axis:0|1|2": the handle's axis (a hammer whose head is wider than its handle is long)
    backflat = "backflat" in parts[3:]  # round 101, a shield: its back (smeared: no picture showed it) in one plain color
    rest = [p for p in parts[3:] if p not in ("flip", "box", "backflat") and not p.startswith("roll:") and not p.startswith("axis:")]

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
    obj = bpy.context.view_layer.objects.active
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    mn, mx = bounds(obj)
    size = mx - mn

    if kind == "melee":
        end = rest[0] if rest else "head"
        grip_share = float(rest[1]) if len(rest) > 1 else 0.15
        # Principal axes (a curved or diagonal blade isn't along a box side): the longest onto X, the flattest onto Y
        import numpy as np
        if box:  # the box's longest side instead (a hammer: its head would tilt the principal axis off the handle)
            axis = forced if forced is not None else max(range(3), key=lambda i: size[i])
            print("RAW SIZE", tuple(round(x, 3) for x in size), "axis", axis)
            if axis == 1:
                obj.data.transform(Matrix.Rotation(math.radians(-90), 4, "Z"))
            elif axis == 2:
                obj.data.transform(Matrix.Rotation(math.radians(90), 4, "Y"))
        pts = np.array([tuple(v.co) for v in obj.data.vertices])
        c = pts.mean(axis=0)
        w, vecs = np.linalg.eigh(np.cov((pts - c).T))
        ax, az, ay = vecs[:, 2], vecs[:, 1], vecs[:, 0]
        if np.dot(np.cross(ax, ay), az) < 0:
            ay = -ay
        rot = Matrix([list(ax), list(ay), list(az)])  # rows: new X, Y, Z in old coordinates
        if not box:
            obj.data.transform(rot.to_4x4() @ Matrix.Translation(-Vector(c)))
        mn, mx = bounds(obj)
        verts = [v.co.copy() for v in obj.data.vertices]
        q = (mx.x - mn.x) * 0.25
        bulky_front = spread(verts, 0, mx.x - q, mx.x) > spread(verts, 0, mn.x, mn.x + q)
        if bulky_front != (end == "head"):
            obj.data.transform(Matrix.Rotation(math.pi, 4, "Z"))
        if flip:  # roll half a turn about the long axis (which side is up)
            obj.data.transform(Matrix.Rotation(math.pi, 4, "X"))
        mn, mx = bounds(obj)
        length = mx.x - mn.x
        gx = mn.x + grip_share * length
        sl = [v.co for v in obj.data.vertices if abs(v.co.x - gx) < length * 0.03]
        if len(sl) < 6:  # a smooth handle has few vertices along it: the handle's nearest rings either side instead
            sl = sorted(obj.data.vertices, key=lambda v: abs(v.co.x - gx))[:24]
            sl = [v.co for v in sl]
        origin = Vector((gx, sum(p.y for p in sl) / len(sl), sum(p.z for p in sl) / len(sl))) if sl else Vector((gx, 0, 0))
        scale = length
    else:  # shield
        axis = min(range(3), key=lambda i: size[i])
        # Tripo builds the front (the picture) toward -Y; that side becomes +X
        if axis == 1:
            obj.data.transform(Matrix.Rotation(math.radians(-90), 4, "Z"))
        elif axis == 2:
            obj.data.transform(Matrix.Rotation(math.radians(90), 4, "Y"))
        if flip:
            obj.data.transform(Matrix.Rotation(math.pi, 4, "Z"))
        if roll:  # turn the face's picture upright (about the face's normal)
            obj.data.transform(Matrix.Rotation(math.radians(roll), 4, "X"))
        mn, mx = bounds(obj)
        origin = Vector((mn.x, (mn.y + mx.y) / 2, (mn.z + mx.z) / 2))
        scale = max(mx.y - mn.y, mx.z - mn.z)

    for v in obj.data.vertices:
        v.co = (v.co - origin) / scale
    obj.data.update()
    mn, mx = bounds(obj)
    if kind == "shield" and backflat:
        # The back's faces take the UV of a spot on the front a third of the way out from the middle (the face's main
        # color, between the boss and the rim)
        uv = obj.data.uv_layers.active.data
        fronts = [p for p in obj.data.polygons if p.normal.x > 0.7]
        ref = min(fronts, key=lambda p: abs(math.hypot(p.center.y, p.center.z) - 0.33))
        ref_uv = sum((uv[i].uv for i in ref.loop_indices), Vector((0.0, 0.0))) / len(ref.loop_indices)
        changed = 0
        for p in obj.data.polygons:
            if p.normal.x < -0.35 and p.center.x < mn.x + (mx.x - mn.x) * 0.3 and math.hypot(p.center.y, p.center.z) < 0.4:
                for i in p.loop_indices:
                    uv[i].uv = ref_uv
                changed += 1
        print(f"MELEE {name}: back made plain, {changed} faces")
    front = [v.co for v in obj.data.vertices if v.co.x > mx.x - 0.03]
    tip = sum(front, Vector()) / max(len(front), 1)
    tris = sum(len(p.vertices) - 2 for p in obj.data.polygons)

    obj.name = name
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.ops.export_scene.fbx(filepath=f"{out_dir}/{name}.fbx", use_selection=True, object_types={"MESH"},
                             mesh_smooth_type="FACE", path_mode="COPY", embed_textures=True, bake_anim=False,
                             add_leaf_bones=False)
    sz = (mx - mn) * 100
    # Unreal's Y is Blender's -Y through FBX
    print(f"MELEE {name} ({kind}): {tris} tris, size {sz.x:.0f} x {sz.y:.0f} x {sz.z:.0f}, "
          f"x {mn.x * 100:.0f}..{mx.x * 100:.0f}, z {mn.z * 100:.0f}..{mx.z * 100:.0f}, tip ({tip.x * 100:.0f}, {-tip.y * 100:.0f}, {tip.z * 100:.0f})")

    sc = bpy.context.scene
    engines = {e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items}
    sc.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in engines else "BLENDER_EEVEE"
    sc.render.resolution_x, sc.render.resolution_y = 800, 800
    sc.view_settings.view_transform = "Standard"
    world = bpy.data.worlds.new("w")
    sc.world = world
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.75, 0.75, 0.78, 1)
    sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
    sun.data.energy = 2.5
    sc.collection.objects.link(sun)
    sun.rotation_euler = (math.radians(50), 0, math.radians(-30))
    cam = bpy.data.objects.new("c", bpy.data.cameras.new("c"))
    sc.collection.objects.link(cam)
    sc.camera = cam
    cam.data.type = "ORTHO"
    center = (mn + mx) / 2
    view = max(mx.x - mn.x, mx.z - mn.z) * 1.25
    render(f"{out_dir}/{name}_34.png", (1.6, -2.4, 1.0), (math.radians(72), 0, math.radians(34)), view, center)
    ball((0, mn.y - 0.01, 0), (1, 0, 0, 1))
    ball((tip.x, mn.y - 0.01, tip.z), (0, 0.3, 1, 1))
    render(f"{out_dir}/{name}_side.png", (0, -3.0, 0), (math.radians(90), 0, 0), view, center)
    if kind == "shield":
        # Straight at the face (from +X) too, so the picture on it shows
        render(f"{out_dir}/{name}_front.png", (3.0, 0, 0), (math.radians(90), 0, math.radians(90)), view, center)
