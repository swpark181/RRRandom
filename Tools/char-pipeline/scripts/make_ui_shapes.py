"""Shape sprites for the pastel HUD (run with any Python that has numpy, e.g. Blender's).

Usage: python make_ui_shapes.py <out dir>
Writes white sprites on transparent (straight alpha, white everywhere so filtering leaves no dark fringe) that
RRRandomUIStyle cuts into quarters for nine-slice drawing:
- T_UICircle.png (256): a crisp anti-aliased disc filling the square; its quarters are rounded corners.
- T_UISoft.png (128): a soft round falloff, solid in the middle; its quarters are soft shadow and glow corners, and the
  whole sprite is a soft wash behind HUD groups.
PNG is written by hand (zlib + struct) so nothing else is needed.
"""
import struct
import sys
import zlib

import numpy as np

out_dir = sys.argv[1]


def save_png(path, rgba):
    h, w, _ = rgba.shape
    data = (np.clip(rgba, 0.0, 1.0) * 255.0 + 0.5).astype(np.uint8)
    raw = b"".join(b"\x00" + data[y].tobytes() for y in range(h))

    def chunk(tag, body):
        return struct.pack(">I", len(body)) + tag + body + struct.pack(">I", zlib.crc32(tag + body) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    open(path, "wb").write(png)


def white(alpha):
    n = alpha.shape[0]
    img = np.ones((n, n, 4), dtype=np.float32)
    img[..., 3] = np.clip(alpha, 0.0, 1.0)
    return img


def radius(n):
    """Distance from the middle in pixels, per pixel center."""
    y, x = np.mgrid[0:n, 0:n].astype(np.float32)
    c = n * 0.5
    return np.sqrt((x + 0.5 - c) ** 2 + (y + 0.5 - c) ** 2)


# Disc: one pixel of anti-aliasing at the rim, touching the square's edges
N = 256
r = radius(N)
save_png(f"{out_dir}/T_UICircle.png", white(N * 0.5 - r + 0.5))

# Soft: solid out to a fifth of the radius, then a smooth fall to nothing at the edge
N = 128
t = np.clip((radius(N) / (N * 0.5) - 0.2) / 0.8, 0.0, 1.0)
save_png(f"{out_dir}/T_UISoft.png", white(1.0 - t * t * (3.0 - 2.0 * t)))
print(f"UI shapes written to {out_dir}")
