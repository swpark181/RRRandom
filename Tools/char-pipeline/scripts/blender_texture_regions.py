"""Make each part of a Tripo character one clean color region, using the mesh (run inside Blender).

Usage: blender -b --factory-startup --python blender_texture_regions.py -- <fbx> <base color in> <png out>

Tripo projects its pictures onto the mesh, so where a part was not seen (under a pouch, the rim of a rolled cuff)
the colors of two parts end up speckled together with torn edges, and per-texel color tests
(blender_texture_clean.py) cannot tell which part a speck belongs to. The mesh can:
1. every texel gets its nearest palette color, which says its part: fur (fur, white, black), shirt, belt, shorts;
   pink and gold are details that belong to no part;
2. every triangle measures how much of each part its texels show; every vertex averages its triangles, and that is
   smoothed over the mesh a couple of times: a smooth "how much shirt / belt / shorts / fur is here" field;
3. every texel reads the field from its triangle's corners (interpolated, so part edges are smooth curves across the
   triangles, not the triangles' zigzag) and takes the strongest part;
4. a texel whose color shows another part becomes the local color of its field's part (the average of that part's
   texels nearby, so shading carries over); texels near pink (the dice print, blush, inner ears) are left alone;
5. where two parts meet, the texels on the line are softened over one texel.
"""
import math
import sys

import bpy
import numpy as np

_args = sys.argv[sys.argv.index("--") + 1:]
NO_TAIL = "--no-tail" in _args  # fox girl v3: the red/white tail pass broke a well drawn tail tip into white blobs
# --repaint-top <fraction>: repaint parts only below this height (default BODY_TOP). Fox girl v3's neck has big
# triangles, so the part field stepped along them and tore the collar: 0.44 keeps the collar as the cleanup left it
REPAINT_TOP = float(_args[_args.index("--repaint-top") + 1]) if "--repaint-top" in _args else None
if REPAINT_TOP is not None:
    del _args[_args.index("--repaint-top"):_args.index("--repaint-top") + 2]
# --tail-raw <Tripo base color>: the tail's fur straight from Tripo's own texture (fox girl v3: the cleanup pulled the
# pinkish white tip toward the shorts' cream and every repaint after that left blotches; Tripo's was the cleanest)
TAIL_RAW = _args[_args.index("--tail-raw") + 1] if "--tail-raw" in _args else None
if TAIL_RAW is not None:
    del _args[_args.index("--tail-raw"):_args.index("--tail-raw") + 2]
fbx, tex_in, tex_out = [a for a in _args if not a.startswith("--")][:3]

PALETTE = {  # first guesses in display sRGB 0..1 (k-means refines all but the small ones)
    "fur": (0.75, 0.22, 0.14), "white": (0.93, 0.81, 0.76), "black": (0.12, 0.09, 0.06), "pink": (0.95, 0.65, 0.70),
    "shirt": (0.49, 0.84, 0.63), "shorts": (0.91, 0.81, 0.61), "belt": (0.60, 0.33, 0.11), "gold": (0.90, 0.75, 0.40),
}
FIXED = {"pink", "gold"}
PARTS = ["fur", "shirt", "belt", "shorts"]
PART_OF = {"fur": 0, "white": 0, "black": 0, "shirt": 1, "belt": 2, "shorts": 3, "pink": -1, "gold": -1}
SMOOTH = 2       # rounds of averaging the field with each vertex's neighbors
LOCAL = 14       # texels: radius of "nearby" for a part's local color (two box blurs)
PROTECT = 10     # texels around pink left alone
DOWN = -0.15       # a triangle whose normal points this far down is an underside (a pouch's bevel too)
UNDER_BAND = (0.15, 0.5)  # and undersides are only re-parted between these fractions of the height (the clothes)
BODY_TOP = 0.52           # parts are only repainted below this fraction of the height (the neck)
SOLID_GOLD = 0.5         # gold texels with less gold than this around them (5x5) are blend lines, not metal
PAD = 16                 # texels of padding grown into the gaps between UV islands
UNDER_SURE = 0.55        # ... when at least this share of the patch's rim borders one part
TAIL_BEHIND = 0.02  # the tail: vertices behind this (+Y, metres; the mesh faces -Y, its back is at about -0.02)
TAIL_BELOW = 0.3    # and below this fraction of the height (the ponytail is above)
TAIL_FAR = 0.07     # metres: the shorts' seat ends about here (fox girl v3), anything further back is tail
MARK_R = 12          # texels: the neighborhood a stray mark is judged in (two box blurs)
MARK_RED = 0.7       # ... at least this share of it red fur
MARK_DARK = 0.25     # ... and at most this share dark
WHITE_NEAR = 0.3     # cream on fur with at least this share of white fur around (MARK_R) is white fur, not red
TAIL_SMOOTH = 5    # the tail's red/white field is noisier: smooth it more
PAW_R = 6            # texels: the neighborhood a speck on a black paw is judged in (two box blurs)
PAW_BLACK = 0.75     # ... at least this share of it black (a glove line is about half)
PAW_SMOOTH = 2       # rounds of averaging the paws' per-vertex black share with each vertex's neighbors
PAW_TOL = 0.12       # ... and on a paw, a texel this far from the local black is a speck too

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=fbx)
mesh = next(o for o in bpy.context.selected_objects if o.type == "MESH")
me = mesh.data
me.calc_loop_triangles()
uv = me.uv_layers.active.data

img = bpy.data.images.load(tex_in)
w, h = img.size
flat = np.empty(w * h * 4, dtype=np.float32)
img.pixels.foreach_get(flat)
rgb = flat.reshape(h, w, 4)[:, :, :3].copy()
del flat

names = list(PALETTE)
idx = {n: i for i, n in enumerate(names)}
centers = np.array([PALETTE[n] for n in names], dtype=np.float32)
rng = np.random.default_rng(1)
sample = rgb.reshape(-1, 3)[rng.integers(0, w * h, 300000)]
for _ in range(20):
    nearest = ((sample[:, None] - centers[None]) ** 2).sum(-1).argmin(1)
    for k, n in enumerate(names):
        members = sample[nearest == k]
        if n not in FIXED and len(members) > 200:
            centers[k] = np.median(members, axis=0)
cls = np.empty((h, w), dtype=np.int8)
for y0 in range(0, h, 256):
    block = rgb[y0:y0 + 256]
    cls[y0:y0 + 256] = ((block[:, :, None] - centers[None, None]) ** 2).sum(-1).argmin(-1)
part_of_cls = np.array([PART_OF[n] for n in names])
cls_part = part_of_cls[cls].ravel()
print("REGIONS palette " + ", ".join(f"{n} {np.round(c * 255).astype(int)}" for n, c in zip(names, centers)))

# Each triangle's texels and their barycentric weights (a texel and a half of slack reaches across UV seams)
tris = list(me.loop_triangles)
tex_idx, tex_bary = [], []
for t in tris:
    p = np.array([(uv[i].uv[0] * w, uv[i].uv[1] * h) for i in t.loops])
    x0, y0 = np.floor(p.min(0)).astype(int) - 1
    x1, y1 = np.ceil(p.max(0)).astype(int) + 2
    x0, y0, x1, y1 = max(x0, 0), max(y0, 0), min(x1, w), min(y1, h)
    (ax, ay), (bx, by), (cx, cy) = p
    den = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
    if x1 <= x0 or y1 <= y0 or abs(den) < 1e-9:
        tex_idx.append(np.empty(0, dtype=np.int64))
        tex_bary.append(np.empty((0, 3), dtype=np.float32))
        continue
    ys, xs = np.mgrid[y0:y1, x0:x1]
    px, py = xs.ravel() + 0.5, ys.ravel() + 0.5
    l1 = ((by - cy) * (px - cx) + (cx - bx) * (py - cy)) / den
    l2 = ((cy - ay) * (px - cx) + (ax - cx) * (py - cy)) / den
    l3 = 1 - l1 - l2
    edge = min(math.dist(p[0], p[1]), math.dist(p[1], p[2]), math.dist(p[2], p[0]))
    inside = np.minimum(np.minimum(l1, l2), l3) >= -1.5 / max(1.0, edge)
    tex_idx.append((ys.ravel()[inside] * w + xs.ravel()[inside]).astype(np.int64))
    bary = np.clip(np.stack([l1[inside], l2[inside], l3[inside]], 1), 0, 1)
    tex_bary.append((bary / np.maximum(bary.sum(1, keepdims=True), 1e-6)).astype(np.float32))

# Per-triangle share of each part, averaged onto vertices (merged by position so UV seams do not split them)
keys = {}
vert = np.array([keys.setdefault(tuple(np.round(np.array(v.co), 5)), len(keys)) for v in me.vertices])
nv = len(keys)
field = np.zeros((nv, len(PARTS)))
weight = np.zeros(nv)
tri_verts = np.array([[vert[v] for v in t.vertices] for t in tris])
for i, tx in enumerate(tex_idx):
    parts = cls_part[tx]
    parts = parts[parts >= 0]
    if not len(parts):
        continue
    share = np.bincount(parts, minlength=len(PARTS)) / len(parts)
    for v in tri_verts[i]:
        field[v] += share * len(parts)
        weight[v] += len(parts)
field /= np.maximum(weight, 1e-6)[:, None]
neighbors = [set() for _ in range(nv)]
for a, b, c in tri_verts:
    neighbors[a] |= {b, c}
    neighbors[b] |= {a, c}
    neighbors[c] |= {a, b}
neighbors = [np.array(sorted(n)) for n in neighbors]
for _ in range(SMOOTH):
    field = np.array([(field[v] + field[nb].mean(0)) / 2 if len(nb) else field[v] for v, nb in enumerate(neighbors)])

# Every texel's part from the field at its spot
field_part = np.full(w * h, -1, dtype=np.int8)
for i, tx in enumerate(tex_idx):
    if len(tx):
        f = tex_bary[i] @ field[tri_verts[i]]
        field_part[tx] = f.argmax(1)

# Undersides: no picture saw a downward face (a pouch's bottom, a cuff's rim), so Tripo painted it with whatever
# was below it. Each patch of downward faces takes the part that most of its rim (by length) borders on
co = np.zeros((nv, 3))
for v in me.vertices:
    co[vert[v.index]] = (mesh.matrix_world @ v.co)[:]
zmin_all = co[:, 2].min()
height_all = co[:, 2].max() - zmin_all
normal_z = np.array([(mesh.matrix_world.to_3x3() @ t.normal).normalized().z for t in tris])
down = normal_z < DOWN
tri_part = np.full(len(tris), -1)
for i, tx in enumerate(tex_idx):
    parts = cls_part[tx]
    parts = parts[parts >= 0]
    if len(parts):
        tri_part[i] = np.bincount(parts, minlength=len(PARTS)).argmax()
edge_tris = {}
for i, (a, b, c) in enumerate(tri_verts):
    for e in ((a, b), (b, c), (c, a)):
        edge_tris.setdefault(tuple(sorted(e)), []).append(i)
tri_nb = [[] for _ in tris]
for e, ts in edge_tris.items():
    for x in ts:
        tri_nb[x] += [(y, e) for y in ts if y != x]
patch_of = np.full(len(tris), -1)
patches = []
for start in np.nonzero(down)[0]:
    if patch_of[start] >= 0:
        continue
    stack, members = [start], []
    patch_of[start] = len(patches)
    while stack:
        x = stack.pop()
        members.append(x)
        for y, _ in tri_nb[x]:
            if down[y] and patch_of[y] < 0:
                patch_of[y] = len(patches)
                stack.append(y)
    patches.append(members)
forced = np.full(len(tris), -1)
changed_patches = 0
for p, members in enumerate(patches):
    votes = np.zeros(len(PARTS))
    for x in members:
        for y, (a, b) in tri_nb[x]:
            if not down[y] and tri_part[y] >= 0:
                votes[tri_part[y]] += np.linalg.norm(co[a] - co[b])
    if votes.sum() == 0:
        continue
    winner = int(votes.argmax())
    own = np.bincount(tri_part[members][tri_part[members] >= 0], minlength=len(PARTS))
    if not own.sum():
        continue
    mid_z = co[tri_verts[members].ravel()][:, 2].mean()
    if PARTS[winner] not in ("shirt", "belt", "shorts") or PARTS[own.argmax()] not in ("shirt", "belt", "shorts") \
            or not (zmin_all + UNDER_BAND[0] * height_all < mid_z < zmin_all + UNDER_BAND[1] * height_all):
        continue  # only cloth hiding under cloth on the body (a chin, a nose, an arm's underside are drawn right)
    if votes.max() < UNDER_SURE * votes.sum():
        continue  # a rim split between two parts (where a pouch meets the shorts at the back): leave it to the field
    if own.argmax() == winner and own[winner] == own.sum():
        continue  # already all one part
    changed_patches += 1
    forced[members] = winner
    mid = co[tri_verts[members].ravel()].mean(0)
    print(f"REGIONS underside patch of {len(members)} triangles at {np.round(mid, 3)}: "
          f"{PARTS[own.argmax()]} -> {PARTS[winner]} (rim {np.round(votes, 3)})")
for i, tx in enumerate(tex_idx):
    if len(tx) and forced[i] >= 0:
        field_part[tx] = forced[i]
print(f"REGIONS undersides: {int(down.sum())} downward triangles in {len(patches)} patches, "
      f"{changed_patches} patches change part")


def box(a, r):
    c = np.cumsum(np.pad(a, ((r + 1, r), (0, 0)) + ((0, 0),) * (a.ndim - 2), mode="edge"), axis=0)
    a = (c[2 * r + 1:] - c[:-2 * r - 1]) / (2 * r + 1)
    c = np.cumsum(np.pad(a, ((0, 0), (r + 1, r)) + ((0, 0),) * (a.ndim - 2), mode="edge"), axis=1)
    return (c[:, 2 * r + 1:] - c[:, :-2 * r - 1]) / (2 * r + 1)


def local_color(mask, fallback):
    m = mask.astype(np.float32)
    s = box(box(rgb * m[:, :, None], LOCAL), LOCAL)
    n = box(box(m, LOCAL), LOCAL)
    return np.where(n[:, :, None] > 0.02, s / np.maximum(n, 1e-6)[:, :, None], fallback).reshape(-1, 3)


local = [local_color(cls == idx["fur"], centers[idx["fur"]]),
         local_color(cls == idx["shirt"], centers[idx["shirt"]]),
         local_color(cls == idx["belt"], centers[idx["belt"]]),
         local_color(cls == idx["shorts"], centers[idx["shorts"]])]
protected = (box((cls == idx["pink"]).astype(np.float32), PROTECT) > 0).ravel()

out = rgb.reshape(-1, 3).copy()
wrong = (field_part >= 0) & (cls_part >= 0) & (cls_part != field_part) & ~protected
# A blend of brown belt and cream shorts sits closest to gold: thin gold lines on clothing are such blends (the
# buckle is solid gold, the bell sits on fur), so they take the clothing's color too
gold = cls == idx["gold"]
thin_gold = (gold & (box(gold.astype(np.float32), 2) < SOLID_GOLD)).ravel()
wrong |= thin_gold & (field_part >= 1)
# Only the body: the head has no clothes, and its green eyes would read as the mint shirt
body = np.zeros(w * h, dtype=bool)
neck_z = zmin_all + BODY_TOP * height_all
for i, tx in enumerate(tex_idx):
    if len(tx) and co[tri_verts[i]][:, 2].max() < neck_z:
        body[tx] = True
wrong &= body
if REPAINT_TOP is not None:
    below = np.zeros(w * h, dtype=bool)
    top_z = zmin_all + REPAINT_TOP * height_all
    for i, tx in enumerate(tex_idx):
        if len(tx) and co[tri_verts[i]][:, 2].max() < top_z:
            below[tx] = True
    wrong &= below
for k in range(len(PARTS)):
    sel = wrong & (field_part == k)
    out[sel] = local[k][sel]
# ... but the fur is red and white: a pinkish white tail tip in shadow reads as the shorts' cream, and repainting it
# red blotched the tail (fox girl v3). Cream on fur with plenty of white fur around goes white instead
white = cls == idx["white"]
white_around = box(box(white.astype(np.float32), MARK_R), MARK_R).ravel()
to_white = wrong & (field_part == 0) & (cls == idx["shorts"]).ravel() & (white_around > WHITE_NEAR)
out[to_white] = local_color(white, centers[idx["white"]])[to_white]
print(f"REGIONS cream on fur by white fur: {int(to_white.sum())} texels went white, not red")
print(f"REGIONS repainted {int(wrong.sum())} texels: " +
      ", ".join(f"to {p} {int((wrong & (field_part == k)).sum())}" for k, p in enumerate(PARTS)))
# The head has no cream shorts: white fur in shadow (under the chin, inside the ears) sits next to the shorts' cream
# and the cleanup pulled it there (fox girl v3), so on the head that cream goes back to the nearby white fur
head_cream = ~body & (cls == idx["shorts"]).ravel() & (field_part >= 0)
out[head_cream] = local_color(cls == idx["white"], centers[idx["white"]])[head_cream]
print(f"REGIONS cream on the head back to white fur: {int(head_cream.sum())} texels")

# The tail: the same again inside the fur, red against its white tip (only there: the face's own white is drawn well)
zmin, zmax = co[:, 2].min(), co[:, 2].max()
tail_v = (co[:, 1] > TAIL_BEHIND) & (co[:, 2] < zmin + (zmax - zmin) * TAIL_BELOW) & (not NO_TAIL)
fur2 = np.zeros((nv, 2))
w2 = np.zeros(nv)
tip = (cls == idx["white"]) | (cls == idx["pink"])  # the tail tip's white is a little pink
red_white = np.where(cls.ravel() == idx["fur"], 0, np.where(tip.ravel(), 1, -1))
tail_any = (cls.ravel() != idx["black"])  # in the tail every texel but black goes red or white (mid-tones too)
for i, tx in enumerate(tex_idx):
    if not len(tx) or not tail_v[tri_verts[i]].all():
        continue
    c = red_white[tx]
    c = c[c >= 0]
    if len(c):
        for v in tri_verts[i]:
            fur2[v] += np.bincount(c, minlength=2)
            w2[v] += len(c)
fur2 /= np.maximum(w2, 1e-6)[:, None]
for _ in range(TAIL_SMOOTH):
    fur2 =np.array([(fur2[v] + fur2[nb].mean(0)) / 2 if len(nb) else fur2[v] for v, nb in enumerate(neighbors)])
local_white = local_color(tip, centers[idx["white"]])
tail_fixed = 0
for i, tx in enumerate(tex_idx):
    if not len(tx) or not tail_v[tri_verts[i]].all():
        continue
    side = (tex_bary[i] @ fur2[tri_verts[i]]).argmax(1)
    own = red_white[tx]
    bad = tail_any[tx] & (own != side)
    out[tx[bad & (side == 0)]] = local[0][tx[bad & (side == 0)]]
    out[tx[bad & (side == 1)]] = local_white[tx[bad & (side == 1)]]
    field_part[tx] = np.where(side == 1, 4, field_part[tx])  # so the red/white line gets softened too
    tail_fixed += int(bad.sum())
print(f"REGIONS tail: {int(tail_v.sum())} vertices behind and low, repainted {tail_fixed} texels")
if TAIL_RAW is not None:
    raw = bpy.data.images.load(TAIL_RAW)
    if tuple(raw.size) != (w, h):
        raw.scale(w, h)
    raw_flat = np.empty(w * h * 4, dtype=np.float32)
    raw.pixels.foreach_get(raw_flat)
    raw_rgb = raw_flat.reshape(-1, 4)[:, :3]
    behind_low = (co[:, 1] > TAIL_BEHIND) & (co[:, 2] < zmin + (zmax - zmin) * TAIL_BELOW)
    kept = 0
    for i, tx in enumerate(tex_idx):
        if len(tx) and behind_low[tri_verts[i]].all():
            # well behind the shorts it is all tail, whatever part its colors read as (the white tip reads as cream)
            far = (co[tri_verts[i]][:, 1] > TAIL_FAR).all()
            fur_tx = tx if far else tx[field_part[tx] == 0]
            out[fur_tx] = raw_rgb[fur_tx]
            kept += len(fur_tx)
    print(f"REGIONS tail fur from Tripo's texture: {kept} texels")

# Stray dark marks on the body's red fur (Tripo drew a scribble like "97" on the right forearm): a dark texel
# whose surroundings are nearly all red and hardly dark at all. A glove line is half dark around, so it stays as
# drawn (redrawing the glove line from the mesh gave a stepped edge)
dark = np.isin(cls, [idx["black"], idx["belt"]])  # a scribble on red fur reads as black or dark brown
red_around = box(box((cls == idx["fur"]).astype(np.float32), MARK_R), MARK_R).ravel()
dark_around = box(box(dark.astype(np.float32), MARK_R), MARK_R).ravel()
mark = dark.ravel() & (red_around > MARK_RED) & (dark_around < MARK_DARK) & body & (field_part == 0)
grown = (box(mark.reshape(h, w).astype(np.float32), 3) > 0).ravel() & ~np.isin(cls.ravel(), [idx["fur"]]) & body
out[grown] = local[0][grown]
print(f"REGIONS stray dark marks on fur: {int(mark.sum())} texels, repainted {int(grown.sum())}")

# Specks on the black paws and feet (fox girl v3: orange-gold bits on the right paw, which the part pass then turned
# red): any texel that is not black but sits in nearly all black takes the paw's own black. The share counts painted
# texels only: specks at a fingertip sit by a UV island's edge, where the empty gap would dilute it
blk = cls == idx["black"]
painted = (field_part >= 0).reshape(h, w).astype(np.float32)
black_around = (box(box(blk * painted, PAW_R), PAW_R) / np.maximum(box(box(painted, PAW_R), PAW_R), 1e-6)).ravel()
speck = ~blk.ravel() & (black_around > PAW_BLACK) & body & ~protected & (painted.ravel() > 0)
# ... and on the mesh: the gold bit at a fingertip is a little spike with its own tiny UV island, so no texel
# neighborhood sees the black around it. Each vertex gets the black share of its triangles' texels (smoothed over
# its neighbors); a triangle whose corners are mostly black on average is a paw's (the spike's own tip vertex is not),
# and its texels that are not black, or are black but visibly warmer or lighter than the paw (a dim orange speck,
# 0.34/0.15/0.07, is nearest to black), go black
blk_flat = blk.ravel()
local_black = local_color(blk, centers[idx["black"]])
off_black = np.linalg.norm(out - local_black, axis=1) > PAW_TOL
vb = np.zeros(nv)
vw = np.zeros(nv)
for i, tx in enumerate(tex_idx):
    if len(tx):
        vb[tri_verts[i]] += blk_flat[tx].sum()
        vw[tri_verts[i]] += len(tx)
vb /= np.maximum(vw, 1e-6)
for _ in range(PAW_SMOOTH):
    vb = np.array([(vb[v] + vb[nb].mean()) / 2 if len(nb) else vb[v] for v, nb in enumerate(neighbors)])
for i, tx in enumerate(tex_idx):
    if len(tx) and vb[tri_verts[i]].mean() > PAW_BLACK:
        speck[tx] |= (~blk_flat[tx] | off_black[tx]) & body[tx]
out[speck] = local_black[speck]
print(f"REGIONS specks on black paws: repainted {int(speck.sum())} texels")

# Fill the gaps between UV islands from the islands' own edges (no triangle covers them, so they still hold
# Tripo's old colors, and filtering and mipmaps pull those in along every seam: the light line under a pouch)
img3 = out.reshape(h, w, 3)
covered = (field_part >= 0).reshape(h, w)
filled = 0
for _ in range(PAD):
    if covered.all():
        break
    acc = np.zeros_like(img3)
    cnt = np.zeros((h, w), dtype=np.float32)
    for dy, dx in ((-1, 0), (1, 0), (0, -1), (0, 1)):
        nb_ok = np.roll(covered, (dy, dx), axis=(0, 1))
        acc += np.roll(img3, (dy, dx), axis=(0, 1)) * nb_ok[:, :, None]
        cnt += nb_ok
    grow = ~covered & (cnt > 0)
    img3[grow] = acc[grow] / cnt[grow][:, None]
    covered |= grow
    filled += int(grow.sum())
print(f"REGIONS padded {filled} gap texels")

# Soften the part edges over one texel
fp = field_part.reshape(h, w)
pad = np.pad(fp, 1, mode="edge")
edge = np.zeros((h, w), dtype=bool)
for dy, dx in ((0, 1), (2, 1), (1, 0), (1, 2)):
    nb = pad[dy:dy + h, dx:dx + w]
    edge |= (nb != fp) & (nb >= 0)
edge &= (fp >= 0) & ~protected.reshape(h, w)
padded = np.pad(img3, ((1, 1), (1, 1), (0, 0)), mode="edge")
avg = sum(padded[dy:dy + h, dx:dx + w] for dy in range(3) for dx in range(3)) / 9.0
img3[edge] = avg[edge]
print(f"REGIONS softened {int(edge.sum())} edge texels")

result = bpy.data.images.new("regions", w, h, alpha=True)
pixels = np.ones((h, w, 4), dtype=np.float32)
pixels[:, :, :3] = np.clip(img3, 0, 1)
result.pixels.foreach_set(pixels.ravel())
result.filepath_raw = tex_out
result.file_format = "PNG"
result.save()
print(f"REGIONS wrote {tex_out}")
