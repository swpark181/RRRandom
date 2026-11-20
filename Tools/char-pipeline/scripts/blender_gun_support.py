"""Where the second (left) hand goes on each toy gun (2026-11-20, the user: "총기류에 손 위치를 어디에 둬야 적당한지 파악해서
체크해두고 ... 해당 위치에 손을 맞쳐줘").

Usage: blender -b --factory-startup --python blender_gun_support.py -- <out dir> <name>=<normalized fbx>[=<x>[,<z>][=<grip x>,<grip z>]] ...

The FBX files are the normalized ones Unreal imported (blender_guns_r90.py: grip at the origin, muzzle +X, 100 long;
1 m in Blender = 100 units). For each gun: the underside profile (a ray up from below every 2 units along X on the
middle line), the front of the trigger guard (where the underside jumps from the guard's low bottom up to the body),
and the support point = the body's underside a hand's width ahead of that (or at the given x), lifted a little into
the palm. With x,z given the support is drawn there (the fist's middle as GunLooks has it), and a given grip
marks the right fist's middle (GunLooks' Grip; default the origin). Prints SUPPORT <name> x y z (Unreal: y flips) for GunLooks, and renders <out>/<name>_support.png (side view,
muzzle right, 10-unit grid; red = grip, green = support).
"""
import math
import sys

import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree

args = sys.argv[sys.argv.index("--") + 1:]
out_dir = args[0]
jobs = [a.split("=") for a in args[1:]]


def load(path):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=path)
    meshes = [o for o in bpy.data.objects if o.type == "MESH"]
    deps = bpy.context.evaluated_depsgraph_get()
    verts, polys = [], []
    for o in meshes:
        m = o.evaluated_get(deps).to_mesh()
        base = len(verts)
        verts += [o.matrix_world @ v.co for v in m.vertices]
        polys += [[base + i for i in p.vertices] for p in m.polygons]
    return meshes, verts, polys


def mark(name, at, color, r):
    bpy.ops.mesh.primitive_uv_sphere_add(radius=r, location=at)
    s = bpy.context.object
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = color
    mat.use_nodes = True
    mat.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = color
    mat.node_tree.nodes["Principled BSDF"].inputs["Emission Color"].default_value = color
    mat.node_tree.nodes["Principled BSDF"].inputs["Emission Strength"].default_value = 1.0
    s.data.materials.append(mat)


def ring(name, at, color, r, thick):
    """A circle in the side view's plane (XZ), drawn in front of the gun: the size of a fist there."""
    curve = bpy.data.curves.new(name, "CURVE")
    curve.bevel_depth = thick
    sp = curve.splines.new("POLY")
    sp.points.add(47)
    for i in range(48):
        a = i / 48 * 2 * math.pi
        sp.points[i].co = (at.x + r * math.cos(a), at.y, at.z + r * math.sin(a), 1)
    sp.use_cyclic_u = True
    obj = bpy.data.objects.new(name, curve)
    bpy.context.scene.collection.objects.link(obj)
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = color
    curve.materials.append(mat)


for job in jobs:
    name, path = job[0], job[1]
    given = [float(v) for v in job[2].split(",")] if len(job) > 2 and job[2] else []
    given_x = given[0] if given else None
    grip = [float(v) for v in job[3].split(",")] if len(job) > 3 and job[3] else [0.0, 0.0]
    meshes, verts, polys = load(path)
    bvh = BVHTree.FromPolygons(verts, polys)
    xs = [v.x for v in verts]
    zs = [v.z for v in verts]
    length = max(xs) - min(xs)
    u = length / 100.0  # Blender metres per gun unit
    # Underside along the middle line: the lowest surface hit from below at each x (units)
    profile = []
    for i in range(0, 101, 2):
        x = min(xs) + i * u
        hit = bvh.ray_cast(Vector((x, 0.0, min(zs) - 0.1)), Vector((0, 0, 1)))
        if hit[0] is None:
            # off the middle line (a side-mounted part): try a little to each side
            for dy in (0.05, -0.05, 0.1, -0.1):
                hit = bvh.ray_cast(Vector((x, dy * length, min(zs) - 0.1)), Vector((0, 0, 1)))
                if hit[0] is not None:
                    break
        profile.append((x / u, None if hit[0] is None else hit[0].z / u))
    print("PROFILE", name, " ".join(f"{x:.0f}:{'-' if z is None else f'{z:.0f}'}" for x, z in profile))
    # The front of the trigger guard: going forward from the grip, the last x where the underside is still low (below the
    # middle between the grip's bottom and the body's underside further ahead)
    ahead = [(x, z) for x, z in profile if x > 0 and z is not None]
    body_bottom = sorted(z for x, z in ahead if x > 25)[len([1 for x, z in ahead if x > 25]) // 2] if any(x > 25 for x, z in ahead) else 0
    lowest = min(z for x, z in ahead) if ahead else 0
    guard_front = 0.0
    for x, z in ahead:
        if z < (lowest + body_bottom) * 0.5 and x < 70:
            guard_front = x
    sx = given_x if given_x is not None else guard_front + 16.0
    # The body's underside at sx (middle line), and its width there (for the palm's size)
    hit = bvh.ray_cast(Vector((sx * u, 0.0, min(zs) - 0.1)), Vector((0, 0, 1)))
    under = hit[0].z / u if hit[0] is not None else body_bottom
    support = Vector((sx, 0.0, given[1] if len(given) > 1 else under - 3.0))
    print(f"SUPPORT {name} x {support.x:.1f} y {-support.y:.1f} z {support.z:.1f} | guard front {guard_front:.0f} body underside {body_bottom:.0f} "
          f"length {length:.3f} m top {max(zs) / u:.0f} bottom {min(zs) / u:.0f}")

    # Side view with a 10-unit grid
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "TEXTURE"
    scene.render.resolution_x = scene.render.resolution_y = 800
    # Drawn in front of the gun (the camera is on -Y): the fists' middles, and a ring a fist big (3.5 cm across the
    # middle at the gun's length in the game: 350 / length units, GunLooks' Length)
    front = (min(v.y for v in verts) - 0.03 * length)
    fist_r = (350.0 / float(job[4]) if len(job) > 4 else 9.0) * u
    for nm, at, col in (("grip", Vector((grip[0] * u, front, grip[1] * u)), (1, 0.1, 0.1, 1)),
                        ("support", Vector((support.x * u, front, support.z * u)), (0.05, 0.75, 0.15, 1))):
        mark(nm, at, col, 1.6 * u)
        ring(nm + "_ring", at, col, fist_r, 0.5 * u)
    bpy.context.scene.display.shading.color_type = "MATERIAL"
    for o in meshes:
        for slot in o.material_slots:
            if slot.material:
                slot.material.diffuse_color = (0.82, 0.82, 0.86, 1)
    for k in range(-30, 131, 10):
        for a, b in (((k * u, 0.3, -60 * u), (k * u, 0.3, 90 * u)), ((-40 * u, 0.3, k * u), (130 * u, 0.3, k * u))):
            curve = bpy.data.curves.new(f"g{k}", "CURVE")
            curve.bevel_depth = 0.12 * u
            sp = curve.splines.new("POLY")
            sp.points.add(1)
            sp.points[0].co = (*a, 1)
            sp.points[1].co = (*b, 1)
            obj = bpy.data.objects.new(f"g{k}", curve)
            bpy.context.scene.collection.objects.link(obj)
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = 1.35 * length
    cam.location = (35 * u, -3 * length, 15 * u)
    cam.rotation_euler = (math.radians(90), 0, 0)
    scene.collection.objects.link(cam)
    scene.camera = cam
    scene.render.filepath = f"{out_dir}/{name}_support.png"
    bpy.ops.render.render(write_still=True)
print("GUNSUPPORT DONE")
