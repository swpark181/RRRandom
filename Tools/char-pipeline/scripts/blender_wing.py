"""Build a stylized feathered wing for the flight power and export it for Unreal (run inside Blender).

Usage: blender -b --factory-startup --python blender_wing.py -- <wing fbx> [<sparkle fbx>]

One right wing, flat with a slight upward cup: the root at the origin, the span along Unreal's +Y and the feathers
trailing toward -X (Blender's Y is flipped on import, so it is built along Blender's -Y). Unreal mirrors it for the left.
- Seven pointed feathers fan out from the arm (inner ones point back, outer ones out and back, longer toward the tip),
  each stacked a hair above the last, with a band of covert feathers over their roots on top.
- Every piece has a thin inset rim with vertex color red = 1 (inside 0) so the material can draw a bright edge;
  green = 1 marks the coverts so the material can lighten them.
- UV: U runs root -> tip (0..1) and V front -> back (0..1), for the material's gradient and shimmer.
Sizes are in Unreal units / 100 (Blender meters): the span is about 142.
With a second path it also writes SM_WingSparkle, the little star the wings shed (see the end of the file).
"""
import math
import sys

import bmesh
import bpy
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
out = args[0]
sparkle_out = args[1] if len(args) > 1 else None

RIM = 0.014          # rim width
CUP = 0.08           # how far the tip curls up
LAYER = 0.004        # height between stacked pieces
WRIST = Vector((0.80, 0.06))   # end of the arm, as (span, chord); chord is negative toward the back
FEATHERS = 7
SPAN_EXTENT = 1.15   # for UVs
CHORD_FRONT, CHORD_BACK = 0.14, -0.72


def log(msg):
    print(f"WING {msg}")


def feather_outline(base, direction, length, width, points=14):
    """A pointed leaf: rounded at the base, sharp at the tip. Points as (span, chord), counterclockwise."""
    side = Vector((-direction.y, direction.x))
    left, right = [], []
    for i in range(points + 1):
        t = i / points
        # A pointed oval, a little wider toward the base
        half = width * 0.5 * math.sin(math.pi * t) ** 0.6 * (1.0 - 0.3 * t) if 0 < t < 1 else 0.0
        center = base + direction * (length * t)
        left.append(center + side * half)
        right.append(center - side * half)
    # base (t=0) and tip (t=1) collapse to one point each
    return [right[0]] + right[1:-1] + [right[-1]] + list(reversed(left[1:-1]))


def covert_outline():
    """A band along the arm, scalloped along its back edge, covering the feathers' roots."""
    front = [Vector((s, 0.02 + 0.10 * math.sin(math.pi * s / 0.95))) for s in (0.0, 0.2, 0.4, 0.6, 0.8, 0.95)]
    back = []
    scallops = 6
    for i in range(scallops * 2 + 1):
        s = 0.95 - 0.95 * i / (scallops * 2)
        depth = 0.20 if i % 2 else 0.12           # bumps (covert tips) and notches between them
        base_chord = 0.02 + 0.10 * math.sin(math.pi * s / 0.95)
        back.append(Vector((s, base_chord - depth * (0.6 + 0.4 * (1 - s)))))
    return front + back[1:-1]


bpy.ops.wm.read_factory_settings(use_empty=True)
mesh = bpy.data.meshes.new("SM_Wing")
obj = bpy.data.objects.new("SM_Wing", mesh)
bpy.context.scene.collection.objects.link(obj)
bm = bmesh.new()
uv = bm.loops.layers.uv.new("UVMap")
col = bm.loops.layers.color.new("Col")


def add_piece(outline, height, covert):
    before = set(bm.faces)
    verts = [bm.verts.new((c.y, -c.x, height)) for c in outline]
    face = bm.faces.new(verts)
    face.normal_update()  # the inset offsets along the face's plane, which needs its normal
    bmesh.ops.inset_individual(bm, faces=[face], thickness=RIM, use_even_offset=True)
    edge = set(verts)
    # Every new face belongs to this piece (bmesh ops clear element tags, so compare sets instead).
    # The inset left the original verts on the outside: the ring faces touch them, the shrunken middle doesn't.
    # Every corner gets a color; uncolored ones would default to white.
    for f in set(bm.faces) - before:
        rim = any(v in edge for v in f.verts)
        for loop in f.loops:
            loop[col] = (1.0 if rim else 0.0, 1.0 if covert else 0.0, 0.0, 1.0)


# Feathers from the inside out; each one's base sits on the arm (root -> wrist)
for i in range(FEATHERS):
    t = i / (FEATHERS - 1)
    base = WRIST * (0.12 + 0.88 * t)
    # Straight back on the inside, swinging out and back toward the tip
    angle = math.radians(5 + 55 * t ** 1.3)
    direction = Vector((math.sin(angle), -math.cos(angle)))
    length = 0.42 + 0.30 * t
    add_piece(feather_outline(base, direction, length, 0.22), LAYER * (FEATHERS - i), covert=False)
add_piece(covert_outline(), LAYER * (FEATHERS + 2), covert=True)

bmesh.ops.triangulate(bm, faces=bm.faces[:], quad_method='BEAUTY', ngon_method='EAR_CLIP')
for v in bm.verts:
    span = -v.co.y
    v.co.z += CUP * (max(span, 0.0) / SPAN_EXTENT) ** 2
for f in bm.faces:
    for loop in f.loops:
        span, chord = -loop.vert.co.y, loop.vert.co.x
        loop[uv].uv = (span / SPAN_EXTENT, (CHORD_FRONT - chord) / (CHORD_FRONT - CHORD_BACK))
bm.normal_update()

xs = [-v.co.y for v in bm.verts]
cs = [v.co.x for v in bm.verts]
log(f"triangles {len(bm.faces)}, span {min(xs):.2f}..{max(xs):.2f}, chord {min(cs):.2f}..{max(cs):.2f}")
bm.to_mesh(mesh)
bm.free()

mesh.materials.append(bpy.data.materials.new("Wing"))
obj.select_set(True)
bpy.context.view_layer.objects.active = obj
bpy.ops.export_scene.fbx(filepath=out, use_selection=True, mesh_smooth_type='FACE', path_mode='STRIP',
                         bake_anim=False, add_leaf_bones=False, colors_type='LINEAR')
log(f"exported {out}")

# ---------- Sparkle: a little four-pointed star the wings shed while flying ----------
# Faces Unreal's +X (the game turns each one toward the camera), points along Y and Z, radius 5.
# Vertex color red = 1 at the center fading to 0 at the points, so the material can make a white-hot core.
if sparkle_out:
    bpy.ops.object.select_all(action='DESELECT')
    star_mesh = bpy.data.meshes.new("SM_WingSparkle")
    star = bpy.data.objects.new("SM_WingSparkle", star_mesh)
    bpy.context.scene.collection.objects.link(star)
    bm = bmesh.new()
    col = bm.loops.layers.color.new("Col")
    uv = bm.loops.layers.uv.new("UVMap")
    center = bm.verts.new((0.0, 0.0, 0.0))
    rim = []
    for i in range(8):
        a = math.pi * 2 * i / 8
        r = 0.05 if i % 2 == 0 else 0.012   # points, then the pinch between them
        rim.append(bm.verts.new((0.0, math.cos(a) * r, math.sin(a) * r)))
    for i in range(8):
        f = bm.faces.new((center, rim[i], rim[(i + 1) % 8]))
        for loop in f.loops:
            core = 1.0 if loop.vert is center else 0.0
            loop[col] = (core, 0.0, 0.0, 1.0)
            loop[uv].uv = (0.5 + loop.vert.co.y * 10, 0.5 + loop.vert.co.z * 10)
    bm.normal_update()
    bm.to_mesh(star_mesh)
    bm.free()
    star_mesh.materials.append(bpy.data.materials.new("Sparkle"))
    star.select_set(True)
    bpy.context.view_layer.objects.active = star
    bpy.ops.export_scene.fbx(filepath=sparkle_out, use_selection=True, mesh_smooth_type='FACE', path_mode='STRIP',
                             bake_anim=False, add_leaf_bones=False, colors_type='LINEAR')
    log(f"exported {sparkle_out}")
