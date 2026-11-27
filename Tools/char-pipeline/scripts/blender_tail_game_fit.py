"""Fit a tail's root to the size the game draws it at (2026-11-25, the user: "호랑이 꼬리 뿌리 부분이 또 전체적인 굵기하고 맞지
않는데 맞춰줘"). The game scales the first tail bone (FRRSpeciesLook::TailSize, the tiger's 0.7), so a vertex moves with
1 - (1 - S) * w of it (w its weight on the tail bones): the tail proper shrinks to S, the ring where the tail leaves the
body, half on the hips, hardly at all - a flare. Here every vertex near the root, from the first tail bone's head to
--len along the tail and within --reach of its line, is set at the radius that comes out at --final in the game (--root at
the head, easing to --final), i.e. model radius = wanted / (1 - (1 - S) * w). Past --len nothing moves. UVs and weights
are untouched; grow the shell fur again from the output.

Usage: blender -b -P blender_tail_game_fit.py -- <rig fbx> <out fbx> --size 0.7 [--final 0.034] [--root 0.037]
       [--len 0.12] [--back 0.004] [--reach 0.075]
(body units: the model 1 tall)
"""
import sys

import bpy

args = sys.argv[sys.argv.index("--") + 1:]
rig, out = args[:2]


def opt(name, default):
    return type(default)(args[args.index(name) + 1]) if name in args else default


S = opt("--size", 0.7)
FINAL = opt("--final", 0.034)
ROOT = opt("--root", 0.037)
LEN = opt("--len", 0.12)
BACK = opt("--back", 0.004)
REACH = opt("--reach", 0.075)


def smoothstep(e0, e1, x):
    t = max(0.0, min(1.0, (x - e0) / (e1 - e0)))
    return t * t * (3.0 - 2.0 * t)


bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=rig)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
arm.data.pose_position = "REST"
bones = sorted([b for b in arm.data.bones if b.name.split(":")[-1].startswith("Tail")],
               key=lambda b: int(b.name.split("Tail")[-1]))
line = [arm.matrix_world @ b.head_local for b in bones] + [arm.matrix_world @ bones[-1].tail_local]
seg = [0.0]
for a, b in zip(line, line[1:]):
    seg.append(seg[-1] + (b - a).length)


def nearest(p):
    """(distance along the tail from the first head, nearest point on the line); before the head, along the first bone."""
    a, b = line[0], line[1]
    d = (b - a).normalized()
    s = (p - a).dot(d)
    if s <= 0.0:
        return s, a + d * s
    best = None
    for i in range(len(line) - 1):
        a, b = line[i], line[i + 1]
        t = max(0.0, min(1.0, (p - a).dot(b - a) / (b - a).length_squared))
        c = a.lerp(b, t)
        dd = (p - c).length_squared
        if best is None or dd < best[0]:
            best = (dd, seg[i] + t * (seg[i + 1] - seg[i]), c)
    return best[1], best[2]


for m in [o for o in bpy.data.objects if o.type == "MESH"]:
    tail_ids = {g.index for g in m.vertex_groups if g.name.split(":")[-1].startswith("Tail")}
    if not tail_ids:
        continue
    mw = m.matrix_world
    inv = mw.inverted()
    rings = {}
    for v in m.data.vertices:
        p = mw @ v.co
        s, c = nearest(p)
        if not -BACK <= s <= LEN:
            continue
        r = (p - c).length
        if r < 1e-5 or r > REACH:
            continue
        w = sum(g.weight for g in v.groups if g.group in tail_ids)
        g = 1.0 - (1.0 - S) * w
        wanted = FINAL + (ROOT - FINAL) * (1.0 - smoothstep(0.0, LEN, max(0.0, s)))
        nr = wanted / g
        v.co = inv @ (c + (p - c) * (nr / r))
        k = round(s * 100 / 1.5) * 1.5
        before, after, n = rings.get(k, (0.0, 0.0, 0))
        rings[k] = (max(before, r * g), max(after, wanted), n + 1)
    m.data.update()
    for k in sorted(rings):
        b, a, n = rings[k]
        print(f"FIT {m.name} s={k:+5.1f}cm {n:3d} vertices: in the game {b * 100:.1f} -> {a * 100:.1f} cm")
for o in [o for o in bpy.data.objects if o.type == "MESH"]:
    if not any(md.type == "ARMATURE" for md in o.modifiers):
        md = o.modifiers.new("Armature", "ARMATURE")
        md.object = arm
bpy.ops.export_scene.fbx(
    filepath=out, use_selection=False, object_types={"ARMATURE", "MESH"}, add_leaf_bones=False, bake_anim=False,
    primary_bone_axis="Y", secondary_bone_axis="X", use_armature_deform_only=False, mesh_smooth_type="OFF",
    path_mode="STRIP")
print(f"FIT wrote {out}")
