"""Make a rigged tail one even thickness along its length (2026-11-25, the user: "호랑이 꼬리는 점점 두꺼워지는게 아니라
일정한 두께로"). Works on the finished rig (blender_reskin.py's Tail1..N, after the face, whiskers, part split and ear
bones), so nothing else has to be done again; the shell fur is grown again from the output.

Usage: blender -b -P blender_tail_even.py -- <rig fbx> <out fbx> [--radius 0.048] [--root 0.12] [--check <png prefix>]

The tail = vertices of the mesh weighted over half to Tail bones; its middle line = the chain of Tail bones (heads, and
the last bone's tail). Along it, 14 slices: each slice's widest reach from the line, smoothed; every tail vertex moves
toward or away from the line so its slice is --radius wide (body units, the model 1 tall). Over the first --root share of
the tail it eases from the size it has to that (the join with the body stays as it was); past 88% the scale of 88% is
kept, so the rounded tip stays rounded. UVs and weights are untouched.
"""
import sys

import bpy
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
rig, out = args[:2]


def opt(name, default):
    return type(default)(args[args.index(name) + 1]) if name in args else default


RADIUS = opt("--radius", 0.048)
ROOT = opt("--root", 0.12)
# (2026-11-25, the user: "여전히 뿌리가 너무 얇아" - the ring at the root is skinned only partly to the tail and was left out,
# a 3.5 cm neck) every vertex with some tail weight is taken, moved by a share that grows with that weight (--from .. --full),
# and the root is --root-fat times thicker, easing to --radius over --root-len of the tail (a cat's tail is fullest at its base)
W_FROM = opt("--from", 0.05)
W_FULL = opt("--full", 0.35)
ROOT_FAT = opt("--root-fat", 1.0)
ROOT_LEN = opt("--root-len", 0.3)
CHECK = opt("--check", "")


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
L = seg[-1]


def along(p):
    best = (1e9, 0.0, line[0])
    for i in range(len(line) - 1):
        a, b = line[i], line[i + 1]
        t = max(0.0, min(1.0, (p - a).dot(b - a) / (b - a).length_squared))
        c = a.lerp(b, t)
        d = (p - c).length_squared
        if d < best[0]:
            best = (d, (seg[i] + t * (seg[i + 1] - seg[i])) / L, c)
    return best[1], best[2]


SL = 14
for m in [o for o in bpy.data.objects if o.type == "MESH"]:
    tail_ids = {g.index for g in m.vertex_groups if g.name.split(":")[-1].startswith("Tail")}
    if not tail_ids:
        continue
    mw = m.matrix_world
    inv = mw.inverted()
    picked = []
    for v in m.data.vertices:
        w = sum(g.weight for g in v.groups if g.group in tail_ids)
        if w > W_FROM:
            p = mw @ v.co
            t, c = along(p)
            picked.append((v, p, t, c, smoothstep(W_FROM, W_FULL, w)))
    reach = [0.0] * SL
    for v, p, t, c, b in picked:
        if b < 0.99:
            continue
        k = min(SL - 1, int(t * SL))
        reach[k] = max(reach[k], (p - c).length)
    # (slices with no wholly-tail vertex - the root ring - take the nearest measured one)
    have = [k for k in range(SL) if reach[k] > 0.0]
    reach = [reach[k] if reach[k] > 0.0 else reach[min(have, key=lambda j: abs(j - k))] for k in range(SL)]
    sm = [max(1e-4, (reach[max(0, k - 1)] + 2 * reach[k] + reach[min(SL - 1, k + 1)]) / 4) for k in range(SL)]
    last = int(0.88 * SL) - 1

    def scale(t):
        f = min(t, (last + 0.5) / SL) * SL - 0.5
        i = max(0, min(SL - 2, int(f)))
        r = sm[i] + (sm[i + 1] - sm[i]) * max(0.0, min(1.0, f - i))
        k = RADIUS * (1.0 + (ROOT_FAT - 1.0) * (1.0 - smoothstep(0.0, ROOT_LEN, t))) / r
        return 1.0 + (k - 1.0) * smoothstep(0.0, ROOT, t)

    for v, p, t, c, b in picked:
        v.co = inv @ (c + (p - c) * (1.0 + (scale(t) - 1.0) * b))
    # --base <back,front,reach> (the same day: the ring where the tail leaves the shorts is the body's, so it stayed 3.5 cm
    # and the tail rose out of it like a bottle neck): body vertices round the tail's line, from <back> before the first
    # tail bone's head to <front> past it (metres along the tail) and within <reach> x the root radius of the line, are
    # pushed out from the line toward the root radius, fading out at both ends and outward - the tail flares into the body
    if "--base" in args:
        bb, bf, breach = (float(x) for x in opt("--base", "0.03,0.06,1.8").split(","))
        d0 = (line[1] - line[0]).normalized()
        r_root = RADIUS * ROOT_FAT
        done = {v.index for v, *_ in picked}
        moved = 0
        for v in m.data.vertices:
            if v.index in done:
                continue
            p = mw @ v.co
            s = (p - line[0]).dot(d0)
            if not -bb < s < bf:
                continue
            c = line[0] + d0 * s
            r = (p - c).length
            if r < 1e-5 or r > breach * r_root:
                continue
            f = smoothstep(-bb, 0.0, s) * (1.0 - smoothstep(r_root, breach * r_root, r))
            if r >= r_root or f <= 0.0:
                continue
            nr = r + (r_root - r) * f
            v.co = inv @ (c + (p - c) * (nr / r))
            moved += 1
        print(f"EVEN base: {moved} body vertices round the root pushed out toward {r_root * 100:.1f} cm")
    m.data.update()
    print(f"EVEN {m.name}: {len(picked)} tail vertices, {L * 100:.0f} cm; reach by slice (cm) before "
          f"{[round(r * 100, 1) for r in reach]} -> {RADIUS * 100:.1f}")
for o in [o for o in bpy.data.objects if o.type == "MESH"]:
    if not any(md.type == "ARMATURE" for md in o.modifiers):
        md = o.modifiers.new("Armature", "ARMATURE")
        md.object = arm
bpy.ops.export_scene.fbx(
    filepath=out, use_selection=False, object_types={"ARMATURE", "MESH"}, add_leaf_bones=False, bake_anim=False,
    primary_bone_axis="Y", secondary_bone_axis="X", use_armature_deform_only=False, mesh_smooth_type="OFF",
    path_mode="STRIP")
print(f"EVEN wrote {out}")
