"""Carve dice pips (1-6) into a blank Tripo cube and export it for Unreal (run inside Blender).

Usage: blender -b --factory-startup --python blender_dice_pips.py -- <tripo fbx> <out fbx>

- Opposite faces add up to 7: +Z 1 / -Z 6, -Y 2 / +Y 5, +X 3 / -X 4.
- Each pip is a sphere boolean-subtracted from the body. The pip's spot is found by casting a ray at the
  face, and the script stops if it lands on the rounded edge instead of the flat face.
- The carved faces get their own material slot "Pip"; the Tripo texture stays on slot "Dice".
- The pivot is moved to the center of the box (to spin it above a character's head), longest edge 1 m (100 in UE).
- Checks that the pip faces form exactly 21 separate dents and that the mesh stays closed.
"""
import sys

import bmesh
import bpy
from mathutils import Vector

src, out = sys.argv[sys.argv.index("--") + 1:][:2]

SPACING = 0.25  # pip offset from the face center, x the face's short edge
RADIUS = 0.085  # pip radius, x the face's short edge
SINK = 0.40     # sphere center sits r*SINK outside the surface -> dent depth r*(1-SINK)

FACES = [(2, +1, 1), (2, -1, 6), (1, -1, 2), (1, +1, 5), (0, +1, 3), (0, -1, 4)]  # (axis, sign, pips)
PIPS = {
    1: [(0, 0)],
    2: [(-1, 1), (1, -1)],
    3: [(-1, 1), (0, 0), (1, -1)],
    4: [(-1, -1), (-1, 1), (1, -1), (1, 1)],
    5: [(-1, -1), (-1, 1), (0, 0), (1, -1), (1, 1)],
    6: [(-1, -1), (-1, 0), (-1, 1), (1, -1), (1, 0), (1, 1)],
}


def log(msg):
    print(f"DICE {msg}")


bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
body = next(o for o in bpy.context.scene.objects if o.type == 'MESH')
bpy.context.view_layer.objects.active = body
body.select_set(True)
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

# Center on the origin, longest edge 1 m
vs = [v.co for v in body.data.vertices]
mn = Vector([min(v[i] for v in vs) for i in range(3)])
mx = Vector([max(v[i] for v in vs) for i in range(3)])
size = mx - mn
body.location = -(mn + mx) / 2
body.scale = [1 / max(size)] * 3
bpy.ops.object.transform_apply(location=True, rotation=False, scale=True)
half = size / max(size) / 2
body.data.materials[0].name = "Dice"

pip_mat = bpy.data.materials.new("Pip")
pip_mat.diffuse_color = (0.04, 0.045, 0.07, 1)

cutters = []
for ax, sign, n in FACES:
    u, v = [i for i in range(3) if i != ax]
    d = 2 * min(half[u], half[v])
    normal = Vector((0, 0, 0))
    normal[ax] = sign
    r = RADIUS * d
    for pu, pv in PIPS[n]:
        origin = Vector((0, 0, 0))
        origin[u] = pu * SPACING * d
        origin[v] = pv * SPACING * d
        origin[ax] = sign * (half[ax] + 1)
        hit, loc, hit_normal, _ = body.ray_cast(origin, -normal)
        if not hit or hit_normal.dot(normal) < 0.95:
            raise SystemExit(f"pip {n} at {(pu, pv)} is off the flat face (hit={hit}, normal={tuple(hit_normal)})")
        bpy.ops.mesh.primitive_uv_sphere_add(segments=48, ring_count=24, radius=r, location=loc + normal * r * SINK)
        s = bpy.context.active_object
        s.data.materials.append(pip_mat)
        bpy.ops.object.shade_smooth()
        cutters.append(s)
log(f"pips placed {len(cutters)}")

bpy.ops.object.select_all(action='DESELECT')
for s in cutters:
    s.select_set(True)
bpy.context.view_layer.objects.active = cutters[0]
bpy.ops.object.join()
cutter = bpy.context.active_object

mod = body.modifiers.new("Pips", 'BOOLEAN')
mod.operation = 'DIFFERENCE'
mod.solver = 'EXACT'
mod.object = cutter
mod.material_mode = 'TRANSFER'
bpy.context.view_layer.objects.active = body
bpy.ops.object.select_all(action='DESELECT')
body.select_set(True)
bpy.ops.object.modifier_apply(modifier="Pips")
bpy.data.objects.remove(cutter, do_unlink=True)
body.name = "SM_Dice"

bm = bmesh.new()
bm.from_mesh(body.data)
pip_index = [m.name for m in body.data.materials].index("Pip")
seen, dents = set(), 0
for f in bm.faces:
    if f.material_index != pip_index or f.index in seen:
        continue
    dents += 1
    stack = [f]
    while stack:
        g = stack.pop()
        if g.index in seen:
            continue
        seen.add(g.index)
        stack += [h for e in g.edges for h in e.link_faces if h.material_index == pip_index and h.index not in seen]
open_edges = sum(1 for e in bm.edges if not e.is_manifold)
log(f"dents {dents} open edges {open_edges} triangles {sum(len(f.verts) - 2 for f in bm.faces)}")
log(f"size {tuple(round(c * 100, 1) for c in half * 2)} cm, slots {[m.name for m in body.data.materials]}")
bm.free()
if dents != 21 or open_edges:
    raise SystemExit("carving failed")

bpy.ops.export_scene.fbx(filepath=out, use_selection=True, mesh_smooth_type='FACE', path_mode='STRIP',
                         bake_anim=False, add_leaf_bones=False)
log(f"exported {out}")
