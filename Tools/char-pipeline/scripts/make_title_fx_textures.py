"""Small effect sprites for the title screen, made procedurally (run with any Python that has numpy, e.g. Blender's).

Usage: python make_title_fx_textures.py <out dir>
Writes sprites the HUD tints (glow and sparkle premultiplied, color = alpha on black, for additive blending; the puff
white on transparent, for alpha blending):
- T_TitleGlow.png (128): a soft round glow, bright in the middle and fading to nothing at the edge.
- T_TitleSparkle.png (128): a four-pointed star with a small halo, like the sparkles in the title art.
- T_TitlePuff.png (128): a soft, slightly lumpy smoke puff.
PNG is written by hand (zlib + struct) so nothing else is needed.
"""
import struct
import sys
import zlib

import numpy as np

out_dir = sys.argv[1]
N = 128


def save_png(path, rgba):
    h, w, _ = rgba.shape
    data = (np.clip(rgba, 0.0, 1.0) * 255.0 + 0.5).astype(np.uint8)
    raw = b"".join(b"\x00" + data[y].tobytes() for y in range(h))

    def chunk(tag, body):
        return struct.pack(">I", len(body)) + tag + body + struct.pack(">I", zlib.crc32(tag + body) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    open(path, "wb").write(png)


y, x = np.mgrid[0:N, 0:N].astype(np.float32)
u = (x + 0.5) / N * 2.0 - 1.0
v = (y + 0.5) / N * 2.0 - 1.0
r = np.sqrt(u * u + v * v)


def white(alpha):
    """White, transparent where alpha is 0 (for alpha blending)."""
    img = np.ones((N, N, 4), dtype=np.float32)
    img[..., 3] = np.clip(alpha, 0.0, 1.0)
    return img


def light(alpha):
    """Premultiplied: color = alpha on black, for additive blending (which ignores alpha)."""
    img = np.zeros((N, N, 4), dtype=np.float32)
    a = np.clip(alpha, 0.0, 1.0)
    img[..., 0] = img[..., 1] = img[..., 2] = img[..., 3] = a
    return img


# Glow: a smooth falloff, gone at the edge
glow = np.clip(1.0 - r, 0.0, 1.0) ** 2.2
save_png(f"{out_dir}/T_TitleGlow.png", light(glow))

# Sparkle: two thin crossing spikes plus a small round core
spikes = np.maximum(np.clip(1.0 - np.abs(u) * 14.0, 0, 1) * np.clip(1.0 - np.abs(v), 0, 1) ** 1.5,
                    np.clip(1.0 - np.abs(v) * 14.0, 0, 1) * np.clip(1.0 - np.abs(u), 0, 1) ** 1.5)
core = np.clip(1.0 - r * 3.5, 0.0, 1.0) ** 1.5
halo = np.clip(1.0 - r * 1.6, 0.0, 1.0) ** 3 * 0.35
save_png(f"{out_dir}/T_TitleSparkle.png", light(np.maximum(np.maximum(spikes, core), halo)))

# Puff: a soft blob with a few lumps around its edge
angle = np.arctan2(v, u)
edge = 0.78 + 0.08 * np.sin(angle * 3.0 + 0.7) + 0.05 * np.sin(angle * 5.0 + 2.1)
puff = np.clip(1.0 - r / edge, 0.0, 1.0) ** 1.6
save_png(f"{out_dir}/T_TitlePuff.png", white(puff))
print(f"FX wrote glow, sparkle and puff ({N}px) to {out_dir}")
