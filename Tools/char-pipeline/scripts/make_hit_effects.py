"""Hit effect stickers for the HUD, from a Tripo (Nano Banana Pro) sheet (run with Blender's Python, it has numpy).

Usage: blender -b --python make_hit_effects.py -- <sheet png> <out png>

The sheet is 8 cartoon stickers in a 4 x 2 grid on flat #00FF00, in ERRHitEffect order (pistol, SMG, minigun, rifle,
blaster, cannon, foam finger poke, skull). Each grid cell is keyed off the green (alpha from how much greener than
red and blue, the edge's green taken back out), cropped to its sticker, centered in a square with a little margin and
scaled to CELL pixels; the result is a 4 x 2 atlas (straight alpha) that ARRRandomHUD draws one cell at a time.
"""
import struct
import sys
import zlib

import bpy
import numpy as np

sheet_path, out_path = sys.argv[sys.argv.index("--") + 1:][:2]
COLUMNS, ROWS = 4, 2
CELL = 256
MARGIN = 0.06
KEY_LOW, KEY_HIGH = 0.12, 0.4


def load(path):
    image = bpy.data.images.load(path)
    w, h = image.size
    return np.array(image.pixels[:], dtype=np.float32).reshape(h, w, 4)[::-1].copy()


def save_png(path, rgba):
    h, w, _ = rgba.shape
    data = (np.clip(rgba, 0.0, 1.0) * 255.0 + 0.5).astype(np.uint8)
    raw = b"".join(b"\x00" + data[y].tobytes() for y in range(h))

    def chunk(tag, body):
        return struct.pack(">I", len(body)) + tag + body + struct.pack(">I", zlib.crc32(tag + body) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def resample(img, size):
    """Bilinear resize of an RGBA (premultiplied for the blend) square to size x size."""
    h, w, _ = img.shape
    ys = (np.arange(size) + 0.5) * h / size - 0.5
    xs = (np.arange(size) + 0.5) * w / size - 0.5
    y0 = np.clip(np.floor(ys).astype(int), 0, h - 1)
    x0 = np.clip(np.floor(xs).astype(int), 0, w - 1)
    y1 = np.clip(y0 + 1, 0, h - 1)
    x1 = np.clip(x0 + 1, 0, w - 1)
    fy = np.clip(ys - y0, 0, 1)[:, None, None]
    fx = np.clip(xs - x0, 0, 1)[None, :, None]
    top = img[y0][:, x0] * (1 - fx) + img[y0][:, x1] * fx
    bottom = img[y1][:, x0] * (1 - fx) + img[y1][:, x1] * fx
    return top * (1 - fy) + bottom * fy


sheet = load(sheet_path)
h, w, _ = sheet.shape
rgb = sheet[..., :3]
green = rgb[..., 1] - np.maximum(rgb[..., 0], rgb[..., 2])
alpha = 1.0 - smoothstep(KEY_LOW, KEY_HIGH, green)
# Spill: no pixel greener than its red and blue allow
color = rgb.copy()
color[..., 1] = np.minimum(color[..., 1], np.maximum(color[..., 0], color[..., 2]))

atlas = np.zeros((ROWS * CELL, COLUMNS * CELL, 4), dtype=np.float32)
for index in range(COLUMNS * ROWS):
    row, col = divmod(index, COLUMNS)
    y0, y1 = row * h // ROWS, (row + 1) * h // ROWS
    x0, x1 = col * w // COLUMNS, (col + 1) * w // COLUMNS
    a = alpha[y0:y1, x0:x1]
    ys, xs = np.nonzero(a > 0.05)
    if len(ys) == 0:
        print(f"HIT cell {index}: empty")
        continue
    top, bottom, left, right = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
    side = int(max(bottom - top, right - left) * (1 + 2 * MARGIN))
    square = np.zeros((side, side, 4), dtype=np.float32)
    oy = (side - (bottom - top)) // 2
    ox = (side - (right - left)) // 2
    crop_a = a[top:bottom, left:right, None]
    crop_c = color[y0:y1, x0:x1][top:bottom, left:right]
    square[oy:oy + bottom - top, ox:ox + right - left, :3] = crop_c * crop_a
    square[oy:oy + bottom - top, ox:ox + right - left, 3:] = crop_a
    small = resample(square, CELL)
    # Back to straight alpha
    out = small.copy()
    out[..., :3] = np.where(small[..., 3:] > 1e-4, small[..., :3] / np.maximum(small[..., 3:], 1e-4), 0.0)
    atlas[row * CELL:(row + 1) * CELL, col * CELL:(col + 1) * CELL] = out
    print(f"HIT cell {index}: sticker {right - left} x {bottom - top} px")

save_png(out_path, atlas)
print(f"HIT atlas {out_path} {atlas.shape[1]} x {atlas.shape[0]}")
