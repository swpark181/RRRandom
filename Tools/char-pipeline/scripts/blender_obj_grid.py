"""Plot an OBJ's vertices from above, from its left side and from behind over a 5-unit grid, to read off where a part
(a tail) runs (run inside Blender, -b; no rendering, the points are drawn straight into PNGs).

Usage: blender -b --factory-startup --python blender_obj_grid.py -- <obj> <out prefix> [<px per unit, default 6>]
The OBJ is blender_tripo_to_obj.py's frame as written: Y up, the character facing +Z, its left on +X, 100 tall.
Writes <prefix>_top.png (X right, +Z down: the front at the bottom), <prefix>_side.png (looking at its left side: +Z,
the front, to the left; Y up) and <prefix>_back.png (from behind: its left, +X, on the left; Y up). Grid every 5 units,
brighter every 25, red at 0. Points are shaded by depth (nearer = brighter).
"""
import sys

import bpy
import numpy as np

args = sys.argv[sys.argv.index("--") + 1:]
src, prefix = args[0], args[1]
ppu = float(args[2]) if len(args) > 2 else 6.0

pts = []
with open(src) as f:
    for line in f:
        if line.startswith("v "):
            pts.append([float(v) for v in line.split()[1:4]])
P = np.array(pts)
lo, hi = P.min(0) - 5, P.max(0) + 5
print(f"OBJGRID bounds x {lo[0]+5:.1f}..{hi[0]-5:.1f} y {lo[1]+5:.1f}..{hi[1]-5:.1f} z {lo[2]+5:.1f}..{hi[2]-5:.1f}")


def plot(name, u, v, depth, umin, umax, vmin, vmax, flip_u, flip_v):
    w = int((umax - umin) * ppu) + 1
    h = int((vmax - vmin) * ppu) + 1
    img = np.zeros((h, w, 4), np.float32)
    img[..., 3] = 1
    img[..., :3] = 0.08

    def to_px(a, amin, n, flip):
        p = ((a - amin) * ppu).astype(int)
        return (n - 1 - p) if flip else p

    for g in np.arange(np.ceil(umin / 5) * 5, umax, 5):
        x = to_px(np.array([g]), umin, w, flip_u)[0]
        img[:, x, :3] = (0.6, 0.1, 0.1) if g == 0 else ((0.35,) * 3 if g % 25 == 0 else (0.18,) * 3)
    for g in np.arange(np.ceil(vmin / 5) * 5, vmax, 5):
        y = to_px(np.array([g]), vmin, h, flip_v)[0]
        img[y, :, :3] = (0.6, 0.1, 0.1) if g == 0 else ((0.35,) * 3 if g % 25 == 0 else (0.18,) * 3)
    order = np.argsort(depth)  # far first, near drawn last
    d = (depth - depth.min()) / max(np.ptp(depth), 1e-6)
    xs = to_px(u[order], umin, w, flip_u)
    ys = to_px(v[order], vmin, h, flip_v)
    shade = 0.35 + 0.65 * d[order]
    img[ys, xs, 0] = shade
    img[ys, xs, 1] = shade * 0.85
    img[ys, xs, 2] = shade * 0.4
    im = bpy.data.images.new(name, w, h)
    im.pixels[:] = img[::-1].ravel()  # Blender images are bottom-up
    im.filepath_raw = f"{prefix}_{name}.png"
    im.file_format = "PNG"
    im.save()
    print(f"OBJGRID wrote {prefix}_{name}.png {w}x{h}")


x, y, z = P[:, 0], P[:, 1], P[:, 2]
# Top: X to the right, +Z (front) down; nearer = higher Y
plot("top", x, z, y, lo[0], hi[0], lo[2], hi[2], False, False)
# Left side: looking from +X; front (+Z) on the left, Y up; nearer = larger X
plot("side", z, y, x, lo[2], hi[2], lo[1], hi[1], True, True)
# Back: looking from -Z; its left (+X) on the left, Y up; nearer = smaller Z
plot("back", x, y, -z, lo[0], hi[0], lo[1], hi[1], True, True)
