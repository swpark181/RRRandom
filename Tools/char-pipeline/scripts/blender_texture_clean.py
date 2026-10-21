"""Tidy a Tripo base color for a toon-shaded animal friend (run inside Blender, for its bundled numpy).

Usage: blender -b --factory-startup --python blender_texture_clean.py -- <base color in> <png out> [size]
          [--fixed belt,...]

--fixed keeps those colors at their first guess below instead of letting k-means move them (fox girl v3: her darker
red fur pulled the belt to (186, 80, 46) and the cleaned belt turned red; --fixed belt kept it brown).

Tripo paints soft, blotchy color: a T-shirt fades into the fur over a few texels, the fur is patchy, everything looks a
little out of focus. This finds the texture's main colors (k-means from rough guesses below), then per texel:
- a color lying on the line between two flat colors where one of them is clothing (shirt, belt, shorts, buckle) is
  snapped to the nearer end, so clothing edges become a hard line (then softened over one texel, no jaggies);
- a color close to one main color is pulled toward it by that color's FLATTEN, which evens out patchy fur and cloth;
- anything else (eyes, nose, brows, the dice print, the forehead mark, blush, fur shading) is left as it is.
The output is resized to [size] (default 4096). The palette and its pull are printed.
"""
import sys

import bpy
import numpy as np

args = sys.argv[sys.argv.index("--") + 1:]
extra_fixed = set()
if "--fixed" in args:
    i = args.index("--fixed")
    extra_fixed = set(args[i + 1].split(","))
    del args[i:i + 2]
tex_in, tex_out = args[0], args[1]
size = int(args[2]) if len(args) > 2 else 4096

# name: (first guess in display sRGB 0..1, how much of a texel's own variation to drop when it is near this color)
PALETTE = {
    "fur": ((0.80, 0.25, 0.17), 0.75),
    "white": ((0.97, 0.90, 0.88), 0.45),
    "black": ((0.12, 0.08, 0.07), 0.40),
    "pink": ((0.95, 0.65, 0.70), 0.0),
    "shirt": ((0.55, 0.88, 0.72), 0.65),
    "shorts": ((0.95, 0.88, 0.68), 0.55),
    "belt": ((0.62, 0.38, 0.18), 0.45),
    "gold": ((0.90, 0.75, 0.40), 0.0),
}
CLOTH_EDGES = [("shirt", "fur"), ("shirt", "white"), ("shirt", "belt"), ("shirt", "black"), ("belt", "shorts"),
               ("shorts", "fur")]
# Blends inside the fur itself (shading, a white muzzle, black socks, pink ears) stay soft: when a texel lies closer
# to one of these lines than to any clothing line it is left alone
FUR_EDGES = [("fur", "white"), ("fur", "black"), ("fur", "pink"), ("white", "pink"), ("white", "black"),
             ("fur", "belt")]
FIXED = {"pink", "gold"} | extra_fixed  # small areas: k-means would drag them onto the fur's lighter shades
NEAR = 0.16      # within this distance of a main color, a texel counts as that color
NEAR_FUR = 0.24  # the fur's patches spread wider (darker and lighter reds on the back of the head)
SHARPEN = 0.6    # unsharp mask amount at the end (radius about 2 texels at 4K): Tripo's paint is a little soft
ON_LINE = 0.05   # a blend between two colors may stray this far from the line joining them

names = list(PALETTE)
centers = np.array([PALETTE[n][0] for n in names], dtype=np.float32)
flatten = np.array([PALETTE[n][1] for n in names], dtype=np.float32)

img = bpy.data.images.load(tex_in)
if img.size[0] != size:
    img.scale(size, size)
flat = np.empty(size * size * 4, dtype=np.float32)
img.pixels.foreach_get(flat)
rgb = flat.reshape(size, size, 4)[:, :, :3].copy()
del flat

# Refine the guesses on a sample of texels (k-means, starting from the guesses so each keeps its meaning)
rng = np.random.default_rng(1)
sample = rgb.reshape(-1, 3)[rng.integers(0, size * size, 300000)]
for _ in range(20):
    d = ((sample[:, None, :] - centers[None]) ** 2).sum(-1)
    nearest = d.argmin(1)
    for k in range(len(names)):
        members = sample[nearest == k]
        if len(members) > 200 and names[k] not in FIXED:
            centers[k] = np.median(members, axis=0)
for n, c in zip(names, centers):
    print(f"CLEAN {n:7s} {np.round(c * 255).astype(int)}")

idx = {n: i for i, n in enumerate(names)}
pairs = [(idx[a], idx[b], True) for a, b in CLOTH_EDGES] + [(idx[a], idx[b], False) for a, b in FUR_EDGES]
cloth = np.zeros(len(names), dtype=bool)
for n in ("shirt", "shorts", "belt", "gold"):
    cloth[idx[n]] = True

out = rgb.copy()
label = np.full((size, size), -1, dtype=np.int8)   # the flat color a texel was snapped to (cloth edges), else -1
snapped_total = 0
for y0 in range(0, size, 256):
    block = rgb[y0:y0 + 256].reshape(-1, 3)
    d = np.sqrt(((block[:, None, :] - centers[None]) ** 2).sum(-1))
    nearest = d.argmin(1)
    near_d = d[np.arange(len(block)), nearest]

    # Pull texels near one main color toward it
    res = block.copy()
    near = near_d < np.where(nearest == idx["fur"], NEAR_FUR, NEAR)
    k = nearest[near]
    res[near] = block[near] + (centers[k] - block[near]) * flatten[k][:, None]
    lab = np.full(len(block), -1, dtype=np.int8)
    lab[near & cloth[nearest]] = nearest[near & cloth[nearest]]

    # Snap blends that lie between clothing and its neighbor to the nearer end
    best_res = np.full(len(block), 1e9, dtype=np.float32)
    best_to = np.full(len(block), -1, dtype=np.int64)
    for a, b, is_cloth in pairs:
        ab = centers[b] - centers[a]
        t = ((block - centers[a]) @ ab) / float(ab @ ab)
        off = np.linalg.norm(block - (centers[a] + np.clip(t, 0, 1)[:, None] * ab), axis=1)
        ok = (t > 0.08) & (t < 0.92) & (off < ON_LINE) & (off < best_res)
        best_res[ok] = off[ok]
        best_to[ok] = np.where(t[ok] < 0.5, a, b) if is_cloth else -2
    blend = best_to >= 0
    res[blend] = centers[best_to[blend]]
    lab[blend] = best_to[blend]
    snapped_total += int(blend.sum())

    out[y0:y0 + 256] = res.reshape(-1, size, 3)
    label[y0:y0 + 256] = lab.reshape(-1, size)

# Soften the new hard edges over one texel: where a clothing label meets a different label, use the 3x3 average
pad = np.pad(label, 1, mode="edge")
edge = np.zeros_like(label, dtype=bool)
for dy, dx in ((0, 1), (2, 1), (1, 0), (1, 2)):
    edge |= pad[dy:dy + size, dx:dx + size] != label
edge &= label >= 0
padded = np.pad(out, ((1, 1), (1, 1), (0, 0)), mode="edge")
avg = sum(padded[dy:dy + size, dx:dx + size] for dy in range(3) for dx in range(3)) / 9.0
out[edge] = avg[edge]
print(f"CLEAN snapped {snapped_total} blend texels, softened {int(edge.sum())} edge texels of {size * size}")

# Sharpen everything a little (three box blurs make a soft blur; add back part of the difference)
def box(a, r):
    c = np.cumsum(np.pad(a, ((r + 1, r), (0, 0), (0, 0)), mode="edge"), axis=0)
    a = (c[2 * r + 1:] - c[:-2 * r - 1]) / (2 * r + 1)
    c = np.cumsum(np.pad(a, ((0, 0), (r + 1, r), (0, 0)), mode="edge"), axis=1)
    return (c[:, 2 * r + 1:] - c[:, :-2 * r - 1]) / (2 * r + 1)


soft = box(box(box(out, 1), 1), 1)
out = out + SHARPEN * (out - soft)
del soft

result = bpy.data.images.new("clean", size, size, alpha=True)
pixels = np.ones((size, size, 4), dtype=np.float32)
pixels[:, :, :3] = np.clip(out, 0, 1)
result.pixels.foreach_set(pixels.ravel())
result.filepath_raw = tex_out
result.file_format = "PNG"
result.save()
print(f"CLEAN wrote {tex_out}")
