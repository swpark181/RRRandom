"""Title art split into layers for the title screen's fox to move (run with Blender's Python, it has numpy).

Usage: blender -b --python make_title_layers.py -- <fox on green png> <background png> <out dir>

Inputs are two edits of the title art made in Tripo (Nano Banana Pro, same 1376 x 768 framing as title_art.jpeg):
the fox alone on a flat #00FF00 background, and the scene with the fox (and its rifle and smoke) painted out.
Writes, at the same size so the HUD keeps using the art's pixel coordinates:
- T_TitleBack.png: the background as is (opaque).
- T_TitleFoxBody.png: the fox keyed off the green (straight alpha, the edge choked a little and colored from the
  solid fox just inside it, so no green ring), without the tail.
- T_TitleFoxTail.png: the tail only, its colors carried on under the body's edge (TailBleed pixels), so turning it
  about its root never opens a gap where it goes behind the body (the body is drawn over it).
- T_TitleWater.png: the background's colors with the sea as alpha, for the title's water material (M_TitleWater) to
  ripple and shine.
The tail is cut along the body's outline in front of it (arm, rifle stock, shorts): TailPolygon, in art pixels.
"""
import struct
import sys
import zlib

import bpy
import numpy as np

fox_path, back_path, out_dir = sys.argv[sys.argv.index("--") + 1:][:3]

# Green to transparent: how much greener than red and blue a pixel is, from opaque to gone
KEY_LOW = 0.10
KEY_HIGH = 0.40
# The edge is pulled in by this much alpha before anything else, and its colors come from this many pixels inside
CHOKE = 0.3
ERODE = 2
EDGE_FILL = 4
# Transparent pixels this close to the fox take its colors (for filtering)
EDGE_SPREAD = 6
# The sea in the background: how many times redder its blue and green both are (from not water to water; light and
# dark faces of the voxel sea alike, not the grass, sand, wood or stone), the horizon (nothing above it is sea; the
# sky is blue too) and how many 3x3 softening passes its edge gets
WATER_LOW = 1.06
WATER_HIGH = 1.18
HORIZON_Y = 570
WATER_SOFTEN = 2
# Only the fox's corner of the picture holds the fox (anything keyed elsewhere is noise)
FOX_BOX = (860, 140, 1376, 720)  # x0, y0, x1, y1
# Around the visible tail, right of the arm, the rifle's stock and the shorts (clockwise, art pixels, y down)
TAIL_POLYGON = [(1240, 435), (1352, 435), (1352, 605), (1229, 605), (1224, 546), (1213, 540), (1245, 488), (1240, 470)]
TAIL_BLEED = 16


def load(path):
    image = bpy.data.images.load(path)
    w, h = image.size
    pixels = np.array(image.pixels[:], dtype=np.float32).reshape(h, w, 4)
    return pixels[::-1].copy()  # top row first


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


def inside_polygon(w, h, polygon):
    """Even-odd test at every pixel's middle."""
    ys, xs = np.mgrid[0:h, 0:w].astype(np.float32) + 0.5
    inside = np.zeros((h, w), dtype=bool)
    n = len(polygon)
    for i in range(n):
        x0, y0 = polygon[i]
        x1, y1 = polygon[(i + 1) % n]
        crosses = (y0 > ys) != (y1 > ys)
        at_x = x0 + (ys - y0) * (x1 - x0) / ((y1 - y0) if y1 != y0 else 1e-6)
        inside ^= crosses & (xs < at_x)
    return inside


def neighbors_mean(values, weights):
    """Average of the 8 neighbors' values, weighted (edges padded with nothing)."""
    pv = np.pad(values * weights[..., None], ((1, 1), (1, 1), (0, 0)))
    pw = np.pad(weights, ((1, 1), (1, 1)))
    total = np.zeros_like(values)
    count = np.zeros(weights.shape, dtype=np.float32)
    h, w = weights.shape
    for dy in (0, 1, 2):
        for dx in (0, 1, 2):
            if dy == 1 and dx == 1:
                continue
            total += pv[dy:dy + h, dx:dx + w]
            count += pw[dy:dy + h, dx:dx + w]
    return total, count


fox = load(fox_path)
back = load(back_path)
h, w, _ = fox.shape
rgb = fox[..., :3]

# Key: alpha from how green each pixel is, only inside the fox's box
green = rgb[..., 1] - np.maximum(rgb[..., 0], rgb[..., 2])
alpha = 1.0 - smoothstep(KEY_LOW, KEY_HIGH, green)
box = np.zeros((h, w), dtype=bool)
box[FOX_BOX[1]:FOX_BOX[3], FOX_BOX[0]:FOX_BOX[2]] = True
alpha[~box] = 0.0
# Choke the edge, so no ring of the green is left around the fox: the outermost pixels mix the fur with the green
# into yellows that hardly read as green at all, so the edge is eaten in by ERODE pixels (and softened by one)
alpha = np.clip((alpha - CHOKE) / (1.0 - CHOKE), 0.0, 1.0)
for _ in range(ERODE):
    padded = np.pad(alpha, 1, mode="edge")
    alpha = np.min([padded[dy:dy + h, dx:dx + w] for dy in range(3) for dx in range(3)], axis=0)
padded = np.pad(alpha, 1, mode="edge")
alpha = np.mean([padded[dy:dy + h, dx:dx + w] for dy in range(3) for dx in range(3)], axis=0)
alpha[alpha < 0.02] = 0.0
alpha[alpha > 0.98] = 1.0
# Edge pixels are part green (and the picture's edge isn't the pure key green, so it can't be subtracted cleanly):
# they take their color from the solid fox next to them instead, a pixel at a time; the alpha keeps the soft edge.
# Then no pixel greener than its red and blue allow.
color = rgb.copy()
solid = (alpha >= 0.98).astype(np.float32)
for _ in range(EDGE_FILL):
    total, count = neighbors_mean(color, solid)
    grow = (solid == 0) & (count > 0) & (alpha > 0.0)
    color[grow] = total[grow] / count[grow][..., None]
    solid[grow] = 1.0
color[..., 1] = np.minimum(color[..., 1], np.maximum(color[..., 0], color[..., 2]))

tail_zone = inside_polygon(w, h, TAIL_POLYGON)
tail_alpha = alpha * tail_zone
body_alpha = alpha * ~tail_zone

# The tail's colors carried on outward, a pixel at a time, into the body next to it (hidden under the body until the
# tail turns), so the root never shows a gap
tail_color = color * tail_zone[..., None]
filled = (tail_alpha > 0.0).astype(np.float32)
bleed = np.zeros((h, w), dtype=np.float32)
for _ in range(TAIL_BLEED):
    total, count = neighbors_mean(tail_color, filled)
    grow = (filled == 0) & (count > 0) & (alpha > 0.5) & ~tail_zone
    tail_color[grow] = total[grow] / count[grow][..., None]
    filled[grow] = 1.0
    bleed[grow] = 1.0
tail_alpha = np.maximum(tail_alpha, bleed)


def spread_color(values, coverage, steps):
    """Carries the covered pixels' colors out into the transparent ones around them (alpha untouched), so filtering
    at the edge (the canvas scales and turns these) blends with the fox's own colors, not with green or black."""
    values = values.copy()
    known = (coverage > 0.0).astype(np.float32)
    for _ in range(steps):
        total, count = neighbors_mean(values, known)
        grow = (known == 0) & (count > 0)
        values[grow] = total[grow] / count[grow][..., None]
        known[grow] = 1.0
    return values


body_color = spread_color(color, body_alpha, EDGE_SPREAD)
tail_color = spread_color(tail_color, tail_alpha, EDGE_SPREAD)
opaque = np.ones((h, w, 1), dtype=np.float32)
save_png(f"{out_dir}/T_TitleBack.png", np.concatenate([back[..., :3], opaque], axis=2))

# The sea: turquoise (blue and green well above red) below the horizon, softened a little at its edges so the moving
# water blends into the dock, the beach and the hills
back_rgb = back[..., :3]
turquoise = np.minimum(back_rgb[..., 2], back_rgb[..., 1]) / np.maximum(back_rgb[..., 0], 0.02)
water = smoothstep(WATER_LOW, WATER_HIGH, turquoise)
water[:HORIZON_Y] = 0.0
for _ in range(WATER_SOFTEN):
    padded = np.pad(water, 1, mode="edge")
    water = np.mean([padded[dy:dy + h, dx:dx + w] for dy in range(3) for dx in range(3)], axis=0)
water = smoothstep(0.35, 1.0, water)
save_png(f"{out_dir}/T_TitleWater.png", np.concatenate([back_rgb, water[..., None]], axis=2))
save_png(f"{out_dir}/T_TitleFoxBody.png", np.concatenate([body_color, body_alpha[..., None]], axis=2))
save_png(f"{out_dir}/T_TitleFoxTail.png", np.concatenate([tail_color, tail_alpha[..., None]], axis=2))
print(f"LAYERS fox pixels {int((alpha > 0.5).sum())}, tail {int((tail_alpha > 0.5).sum())} (bleed {int(bleed.sum())}), size {w}x{h}")
