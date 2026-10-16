"""Cuts a seamless tile out of an AI image that repeats its pattern (run with Blender's Python, which has numpy).

Usage: blender -b --factory-startup --python make_path_tile.py -- <in png> <out png> [size] [sharpen] [contrast]

Finds the repeat period along X and Y near half the image (the shift that best matches the image with itself), cuts one
period from the middle, cross-fades its edges with the opposite side (so any small mismatch at the seam disappears),
resizes it to <size> (default 1024), crisps it (an unsharp mask of <sharpen>, default 0, and <contrast> around its
mean color, default 1: AI paintings come out soft, and the mipmaps soften them more from the game camera) and saves it. Prints the periods and how well they matched.
"""
import sys

import bpy
import numpy as np

args = sys.argv[sys.argv.index("--") + 1:]
src, dst = args[0], args[1]
size = int(args[2]) if len(args) > 2 else 1024
sharpen = float(args[3]) if len(args) > 3 else 0.0
contrast = float(args[4]) if len(args) > 4 else 1.0

image = bpy.data.images.load(src)
w, h = image.size
px = np.array(image.pixels[:], dtype=np.float32).reshape(h, w, 4)[:, :, :3]
grey = px.mean(axis=2)


def period(axis):
    n = grey.shape[1 - axis] if axis == 0 else grey.shape[0]
    best, best_err = None, None
    for shift in range(int(n * 0.35), int(n * 0.65)):
        if axis == 0:
            a, b = grey[:, :n - shift], grey[:, shift:]
        else:
            a, b = grey[:n - shift, :], grey[shift:, :]
        err = float(np.mean(np.abs(a - b)))
        if best_err is None or err < best_err:
            best, best_err = shift, err
    return best, best_err


px_x, err_x = period(0)
px_y, err_y = period(1)
print(f"TILE period x {px_x} (err {err_x:.4f}), y {px_y} (err {err_y:.4f}) of {w} x {h}")

# One period from the middle of the image
x0 = (w - px_x) // 2
y0 = (h - px_y) // 2
tile = px[y0:y0 + px_y, x0:x0 + px_x].copy()

# Cross-fade a band at each edge with the pixels one period over, so the seam is invisible
band = max(4, px_x // 16)
for i in range(band):
    t = 0.5 * (1.0 - i / band)
    # Left edge with what lies just right of the tile's right edge, and the reverse
    tile[:, i] = tile[:, i] * (1 - t) + px[y0:y0 + px_y, min(w - 1, x0 + px_x + i)] * t
    tile[:, px_x - 1 - i] = tile[:, px_x - 1 - i] * (1 - t) + px[y0:y0 + px_y, max(0, x0 - 1 - i)] * t
band = max(4, px_y // 16)
for i in range(band):
    t = 0.5 * (1.0 - i / band)
    tile[i, :] = tile[i, :] * (1 - t) + px[min(h - 1, y0 + px_y + i), x0:x0 + px_x] * t
    tile[px_y - 1 - i, :] = tile[px_y - 1 - i, :] * (1 - t) + px[max(0, y0 - 1 - i), x0:x0 + px_x] * t

# Crisper: contrast around the mean color, then an unsharp mask (the tile wraps, so the blur wraps too)
mean = tile.reshape(-1, 3).mean(axis=0)
tile = mean + (tile - mean) * contrast
if sharpen > 0:
    blur = tile.copy()
    for axis in (0, 1):
        acc = np.zeros_like(blur)
        for step in range(-3, 4):
            acc += np.roll(blur, step, axis=axis)
        blur = acc / 7.0
    tile = tile + (tile - blur) * sharpen
tile = np.clip(tile, 0.0, 1.0)
print(f"TILE crisped: contrast {contrast}, sharpen {sharpen}")

out = bpy.data.images.new("tile", px_x, px_y, alpha=False)
rgba = np.concatenate([tile, np.ones((px_y, px_x, 1), dtype=np.float32)], axis=2)
out.pixels[:] = rgba.ravel()
out.scale(size, size)
out.filepath_raw = dst
out.file_format = "PNG"
out.save()
print(f"TILE saved {dst} ({size} x {size})")
