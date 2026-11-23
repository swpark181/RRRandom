"""The fur mask of a face texture: where the shell fur must NOT grow (run inside Blender; 2026-11-23).

The user: "캐릭터 입이나 눈 위에 털이 생기지 않도록 별도의 uv맵 같은거 만들어서 얼굴 텍스쳐 갈아낄때 마다 같이 적용해줘."
Every face texture (the game's T_<Name>_BaseColor, each expression's T_<Name>_Expr_<X>) gets a twin, its fur mask, in the
body's own UVs: 1 = fur at its full length, 0 = bare, in between = the fur that much shorter (it tapers toward the eyes
and the mouth, so no strand leans over them). M_<Name>Fur reads it ("FurMaskTex", ue_import_fur_shells.py) and whatever
swaps the face texture swaps the mask with it (ue_fur_mask_lib.py: ue_import_basecolor.py, ue_cine_expressions.py).

How: the painted features are found by colour on the front of the head (dark = lines, pupils, brows, nose, an open mouth;
pure white = eye whites, teeth; with --ref also whatever differs from a blank-face texture), per texel. Tripo's atlas cuts
the face into many islands (an eye lies across two), so nothing is grown or blurred in UV space: every texel knows its
place on the rest-pose surface and all distances are measured there, in 3D. Holes inside a feature (an iris the fur's own
colour, a tongue) are closed (--close), then the mask rises from 0 at --margin from a feature to 1 at --margin + --feather.

Usage: blender -b --factory-startup --python blender_fur_mask.py -- <rig fbx> <face texture> <out mask png>
         [--size 1024]                 the mask's size (square)
         [--face zlo,zhi,rx[,front]]   the face: heights (0 feet .. 1 top) and the half width round the head's middle
                                       line, all x the body height; only the front of the head counts: <front> (0.5) of
                                       the way from its front-most point to its back (the tiger 0.75: its muzzle sticks
                                       out so far that half way cut its eyes' outer halves off)
         [--dark 0.30]                 luma below this = a painted line (0 = rule off)
         [--white 0.80,0.14]           darkest channel above the first and saturation below the second = eye white (0,0 = off)
         [--ref <blank texture> [--ref-diff 0.20]]   also a feature: differs this much from the same texel of <blank>
         [--close 0.012] [--margin 0.003] [--feather 0.020]   x the body height
         [--speck 0.015]               painted groups smaller than this across are dropped (0 = keep all)
         [--keep <x,z,rx,rz>;...]      ellipses seen from the front (x from the head's middle, z a height) that stay furred
         [--only <x,z,rx,rz[,rules]>;...]   features are looked for only inside these ellipses (the tiger: each eye
                                       and the muzzle; its stripes are as dark as its eye lines, its fur as white as its
                                       teeth); <rules> = the letters of the rules that count in that ellipse (d dark,
                                       w white, c cream, p pink, r --ref; none = all given): "dc" for an eye, "dp" a muzzle
         [--pink]                      also a feature: pink (a nose, a tongue), by the shells' own rule
         [--cream <luma>,<sat lo>,<sat hi>,<warm>]   also a feature: the warm white of an eye where the fur beside it
                                       is a cold white (the tiger: 0.72,0.11,0.30,0.10 = at least that bright, saturation
                                       between the two, red at least <warm> over blue; its eye outline is only mid grey,
                                       so the dark rule misses it, and its white fur has saturation under 0.1)
         [--fill-inside <link>]        with --only: whatever painted lines close off inside an ellipse is a feature too
                                       (the white of an eye inside its outline, however big; teeth inside a mouth's line);
                                       <link> = how far apart two texels still count as neighbours, about 2.5 mask texels
         [--show <png>]                the face texture with the bare part painted pink, to look at (blender_lit_views.py)
Sizes are fractions of the body's height, as in blender_fur_shells_skel.py. Prints "FURMASK ..." lines; the last is
"FURMASK wrote <png>".
"""
import math
import sys

import bpy
import numpy as np
from mathutils import kdtree

argv = sys.argv[sys.argv.index("--") + 1:]
rig_fbx, tex_path, out_png = argv[:3]


def opt(name, default, cast=float):
    return cast(argv[argv.index(name) + 1]) if name in argv else default


S = opt("--size", 1024, int)
dark_at = opt("--dark", 0.30)
white_min, white_sat = (float(c) for c in opt("--white", "0.80,0.14", str).split(","))
ref_path = opt("--ref", "", str)
ref_diff = opt("--ref-diff", 0.20)
close_r = opt("--close", 0.012)
speck = opt("--speck", 0.015)
margin = opt("--margin", 0.003)
feather = opt("--feather", 0.020)
show_png = opt("--show", "", str)
keep = [[float(c) for c in e.split(",")] for e in opt("--keep", "", str).split(";") if e]
only_raw = [e.split(",") for e in opt("--only", "", str).split(";") if e]
only = [[float(c) for c in e[:4]] for e in only_raw]
only_rules = [e[4] if len(e) > 4 else "" for e in only_raw]  # which rules count in each ellipse ("" = every rule given)
fill_link = opt("--fill-inside", 0.0)

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=rig_fbx)
for a in [o for o in bpy.data.objects if o.type == "ARMATURE"]:
    a.animation_data_clear()
    a.data.pose_position = "REST"
meshes = [o for o in bpy.data.objects if o.type == "MESH"]

# ---- every triangle of every mesh: corners in the world, UVs ----
P_list, UV_list = [], []
for o in meshes:
    me = o.data
    me.calc_loop_triangles()
    mw = np.array(o.matrix_world)
    co = np.empty(len(me.vertices) * 3, dtype=np.float64)
    me.vertices.foreach_get("co", co)
    co = (np.c_[co.reshape(-1, 3), np.ones(len(me.vertices))] @ mw.T)[:, :3]
    n = len(me.loop_triangles)
    tv = np.empty(n * 3, dtype=np.int32)
    tl = np.empty(n * 3, dtype=np.int32)
    me.loop_triangles.foreach_get("vertices", tv)
    me.loop_triangles.foreach_get("loops", tl)
    uv = np.empty(len(me.loops) * 2, dtype=np.float64)
    me.uv_layers[0].data.foreach_get("uv", uv)
    P_list.append(co[tv].reshape(n, 3, 3))
    UV_list.append(uv.reshape(-1, 2)[tl].reshape(n, 3, 2))
P = np.concatenate(P_list)
UV = np.concatenate(UV_list)
allv = P.reshape(-1, 3)
zlo, zhi = allv[:, 2].min(), allv[:, 2].max()
H = zhi - zlo

# ---- the face: the front half of the head, between two heights, near its middle line (front = -Y) ----
head = allv[allv[:, 2] > zlo + 0.55 * H]
hx = (np.percentile(head[:, 0], 2) + np.percentile(head[:, 0], 98)) / 2
arm = next((o for o in bpy.data.objects if o.type == "ARMATURE"), None)
if arm:
    for b in arm.data.bones:
        if b.name.split(":")[-1] == "Head":
            hx = (arm.matrix_world @ b.head_local).x
face_opt = opt("--face", "", str)
f_nums = [float(c) for c in face_opt.split(",")] if face_opt else [0.50, 0.80, 0.20]
f_zlo, f_zhi, f_rx = f_nums[:3]
f_front = f_nums[3] if len(f_nums) > 3 else 0.5
band = allv[(allv[:, 2] > zlo + f_zlo * H) & (allv[:, 2] < zlo + f_zhi * H) & (np.abs(allv[:, 0] - hx) < f_rx * H)]
hy_mid = band[:, 1].min() + (band[:, 1].max() - band[:, 1].min()) * f_front
pad = (close_r + margin + feather) * H * 1.5
print(f"FURMASK height {H:.3f}, head middle x {hx:.3f}, face z {f_zlo}..{f_zhi}, half width {f_rx}, front of y {hy_mid:.3f}")


def in_face(q, grow=0.0):
    return ((q[..., 2] > zlo + f_zlo * H - grow) & (q[..., 2] < zlo + f_zhi * H + grow)
            & (np.abs(q[..., 0] - hx) < f_rx * H + grow) & (q[..., 1] < hy_mid + grow))


# ---- the textures: which texels are painted features, at full size, then the strongest of each mask texel's block ----
def load(path):
    im = bpy.data.images.load(path)
    w, h = im.size
    px = np.empty(w * h * im.channels, dtype=np.float32)
    im.pixels.foreach_get(px)
    return px.reshape(h, w, im.channels)[:, :, :3], w, h  # row 0 = bottom, as UV v


tex, tw, th = load(tex_path)
luma = tex @ np.array([0.299, 0.587, 0.114], dtype=np.float32)
mx, mn = tex.max(2), tex.min(2)
ky, kx = max(1, th // S), max(1, tw // S)


def pooled(a):  # a rule's texels at the texture's size -> the mask's: a mask texel counts when any of its block does
    return a[:S * ky, :S * kx].reshape(S, ky, S, kx).any(axis=(1, 3))


# the rules, each by its letter: d dark, w white, c cream, p pink, r differs from --ref
rules = {}
if dark_at > 0:
    rules["d"] = pooled(luma < dark_at)
if white_min > 0:
    rules["w"] = pooled((mn > white_min) & ((mx - mn) / np.maximum(mx, 1e-3) < white_sat))
cream = opt("--cream", "", str)
if cream:
    c_luma, c_slo, c_shi, c_warm = (float(c) for c in cream.split(","))
    sat_ = (mx - mn) / np.maximum(mx, 1e-3)
    rules["c"] = pooled((luma > c_luma) & (sat_ > c_slo) & (sat_ < c_shi) & (tex[:, :, 0] - tex[:, :, 2] > c_warm))
if "--pink" in argv:
    r_, g_, b_ = tex[:, :, 0], tex[:, :, 1], tex[:, :, 2]
    rules["p"] = pooled((r_ > 0.45) & (r_ - g_ > 0.15) & (b_ > 0.3) & (b_ < r_) & (b_ > 0.9 * g_))
if ref_path:
    ref, rw, rh = load(ref_path)
    if (rw, rh) != (tw, th):
        ref = ref[(np.arange(th) * rh / th).astype(int)][:, (np.arange(tw) * rw / tw).astype(int)]
    rules["r"] = pooled(np.abs(tex - ref).max(2) > ref_diff)
feat_s = np.zeros((S, S), bool)
for m in rules.values():
    feat_s |= m

# ---- rasterise: which mask texels a triangle covers (all of them), and where on the surface (the face's) ----
covered = np.zeros((S, S), bool)
pos = np.zeros((S, S, 3), dtype=np.float32)
on_face = np.zeros((S, S), bool)
near_face = in_face(P, pad).any(1)
for t in range(len(P)):
    u = UV[t, :, 0] * S
    v = UV[t, :, 1] * S
    u0, u1 = int(max(0, math.floor(u.min()))), int(min(S - 1, math.ceil(u.max())))
    v0, v1 = int(max(0, math.floor(v.min()))), int(min(S - 1, math.ceil(v.max())))
    if u1 < u0 or v1 < v0:
        continue
    d = (v[1] - v[2]) * (u[0] - u[2]) + (u[2] - u[1]) * (v[0] - v[2])
    if abs(d) < 1e-12:
        continue
    gu, gv = np.meshgrid(np.arange(u0, u1 + 1) + 0.5, np.arange(v0, v1 + 1) + 0.5)
    a = ((v[1] - v[2]) * (gu - u[2]) + (u[2] - u[1]) * (gv - v[2])) / d
    b = ((v[2] - v[0]) * (gu - u[2]) + (u[0] - u[2]) * (gv - v[2])) / d
    c = 1 - a - b
    # (a texel counts when its middle is in the triangle, or nearly: the edge texels a bilinear read touches)
    inside = (a >= -0.35) & (b >= -0.35) & (c >= -0.35)
    if not inside.any():
        continue
    sub = covered[v0:v1 + 1, u0:u1 + 1]
    strict = (a >= 0) & (b >= 0) & (c >= 0)
    new = inside & (~sub | strict)
    sub |= inside
    if near_face[t]:
        q = a[..., None] * P[t, 0] + b[..., None] * P[t, 1] + c[..., None] * P[t, 2]
        pos[v0:v1 + 1, u0:u1 + 1][new] = q[new]
        on_face[v0:v1 + 1, u0:u1 + 1][new] = True
work = on_face & in_face(pos, pad)          # the texels this is worked out on (the face and a rim round it)
face = work & in_face(pos)
F = face & feat_s


def in_ellipses(q, ells, grow=0.0):
    hit = np.zeros(q.shape[:-1], bool)
    for ex, ez, erx, erz in ells:
        hit |= ((q[..., 0] - hx - ex * H) / ((erx + grow) * H)) ** 2 + ((q[..., 2] - (zlo + ez * H)) / ((erz + grow) * H)) ** 2 < 1.0
    return hit


if only:
    F = np.zeros((S, S), bool)
    for ell, letters in zip(only, only_rules):
        m = feat_s
        if letters:
            m = np.zeros((S, S), bool)
            for letter in letters:
                m |= rules.get(letter, np.zeros((S, S), bool))
        F |= face & m & in_ellipses(pos, [ell])
wy, wx = np.nonzero(work)
wp = pos[wy, wx].astype(np.float64)
print(f"FURMASK {int(face.sum())} face texels of {S}x{S}, {int(F.sum())} painted")
for e in keep:
    ex, ez, erx, erz = e
    q = pos[F]
    out = ((q[:, 0] - hx - ex * H) / (erx * H)) ** 2 + ((q[:, 2] - (zlo + ez * H)) / (erz * H)) ** 2 < 1.0
    fy, fx = np.nonzero(F)
    F[fy[out], fx[out]] = False


def tree_of(points):
    kd = kdtree.KDTree(len(points))
    for i, p in enumerate(points):
        kd.insert(p, i)
    kd.balance()
    return kd


def dist_to(points, kd):
    return np.array([kd.find(p)[2] for p in points]) if kd else np.full(len(points), 1e9)


mask = np.ones((S, S), dtype=np.float32)
if F.any() and speck > 0:
    # stray specks: painted texels join up within a few texels' reach; a group smaller than --speck across is a dark
    # dot in the fur's shading (one sat on the fox's cheek), not a feature
    fy, fx = np.nonzero(F)
    fp = pos[fy, fx].astype(np.float64)
    kd = tree_of(fp)
    link = 0.004 * H
    label = np.full(len(fp), -1, dtype=np.int32)
    dropped = 0
    for start in range(len(fp)):
        if label[start] >= 0:
            continue
        comp, stack = [], [start]
        label[start] = start
        while stack:
            i = stack.pop()
            comp.append(i)
            for _, j, _ in kd.find_range(fp[i], link):
                if label[j] < 0:
                    label[j] = start
                    stack.append(j)
        pts = fp[comp]
        if np.linalg.norm(pts.max(0) - pts.min(0)) < speck * H:
            F[fy[comp], fx[comp]] = False
            dropped += 1
    print(f"FURMASK dropped {dropped} speck(s), {int(F.sum())} painted texels left")
if F.any() and only and fill_link > 0:
    # what the painted lines close off: from the texels just outside each ellipse, walk over unpainted texels (neighbour
    # to neighbour on the surface, in 3D) — an unpainted texel inside an ellipse that the walk never reaches lies behind
    # a closed line (an eye's white and iris inside its outline, teeth inside the mouth's line) and is a feature too
    # (a painted line may be one texel thin, thinner than the step of the walk: the texels within one step of a line
    # are closed to the walk as well, so a line is a wall at least two steps thick)
    inside = in_ellipses(wp, only)
    near = in_ellipses(wp, only, 0.012)
    wall = dist_to(wp[near], tree_of(wp[F[wy, wx]])) <= fill_link * H
    dom = np.nonzero(near)[0][~wall]
    dp = wp[dom]
    kd = tree_of(dp)
    reached = ~inside[dom]
    stack = list(np.nonzero(reached)[0])
    while stack:
        i = stack.pop()
        for _, j, _ in kd.find_range(dp[i], fill_link * H):
            if not reached[j]:
                reached[j] = True
                stack.append(j)
    shut = dom[~reached]
    F[wy[shut], wx[shut]] = True
    print(f"FURMASK closed off inside painted lines: {len(shut)} texels of {int(inside.sum())} in the ellipses")
if F.any():
    Fw = F[wy, wx]
    d_feat = dist_to(wp, tree_of(wp[Fw]))
    # closing: grown by close_r, then shrunk by it again — what is left is the features with their holes filled
    grown = d_feat <= close_r * H
    d_out = dist_to(wp, tree_of(wp[~grown])) if (~grown).any() else np.full(len(wp), 1e9)
    core = Fw | (grown & (d_out > close_r * H))
    d_core = dist_to(wp, tree_of(wp[core]))
    m = np.clip((d_core - margin * H) / max(feather * H, 1e-6), 0.0, 1.0)
    m = m * m * (3 - 2 * m)
    mask[wy, wx] = m
    print(f"FURMASK bare (mask < 0.05): {int((m < 0.05).sum())} texels, shortened: {int(((m >= 0.05) & (m < 0.95)).sum())}")
else:
    print("FURMASK no painted feature found on the face: the mask is all fur")

# ---- the gutters between islands take their neighbours' values (a bilinear read at an island's edge) ----
have = covered.copy()
for _ in range(6):
    acc = np.zeros_like(mask)
    cnt = np.zeros_like(mask)
    for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        acc += np.roll(np.where(have, mask, 0.0), (dy, dx), (0, 1))
        cnt += np.roll(have, (dy, dx), (0, 1))
    fill = ~have & (cnt > 0)
    mask[fill] = acc[fill] / cnt[fill]
    have |= fill


def save(arr, path):
    hh, ww = arr.shape[:2]
    im = bpy.data.images.new("out", ww, hh, alpha=False)
    im.colorspace_settings.name = "Non-Color" if arr.ndim == 2 else "sRGB"
    rgb = np.repeat(arr[..., None], 3, axis=2) if arr.ndim == 2 else arr
    im.pixels.foreach_set(np.concatenate([rgb, np.ones((hh, ww, 1), dtype=np.float32)], axis=2).astype(np.float32).ravel())
    im.filepath_raw = path
    im.file_format = "PNG"
    im.save()


save(mask, out_png)
if show_png:
    n = 2048
    small = tex[(np.arange(n) * th / n).astype(int)][:, (np.arange(n) * tw / n).astype(int)]
    mm = mask[(np.arange(n) * S / n).astype(int)][:, (np.arange(n) * S / n).astype(int)][..., None]
    pink = np.array([1.0, 0.0, 0.6], dtype=np.float32)
    save(small * (0.45 + 0.55 * mm) + pink * 0.55 * (1 - mm), show_png)
    print(f"FURMASK show {show_png}")
print(f"FURMASK wrote {out_png}")
