"""A height map for the dirt road tile, so the flat painting gets relief under light (run with Blender's Python).

Usage: blender -b --factory-startup --python make_path_height.py -- <tile png> <out height png>

The tile (make_path_tile.py) is flat dirt with painted pebbles and grass tufts. Everything that stands out from the dirt
around it (color far from the dirt's local average: the pebbles, the tufts and their dark outlines) is a bump, rounded
into a dome by blurring; the dirt itself gets a gentle undulation from its own light and dark patches. Wraps at the
edges like the tile. Saved as greyscale 0 (low) .. 1 (high), the same size and orientation as the tile.
"""
import sys

import bpy
import numpy as np

args = sys.argv[sys.argv.index("--") + 1:]
src, dst = args[0], args[1]

image = bpy.data.images.load(src)
w, h = image.size
px = np.array(image.pixels[:], dtype=np.float32).reshape(h, w, 4)[:, :, :3]


def blur(a, radius, passes=2):
    """Box blur that wraps, a few passes (close to a Gaussian)."""
    for _ in range(passes):
        for axis in (0, 1):
            acc = np.zeros_like(a)
            for step in range(-radius, radius + 1):
                acc += np.roll(a, step, axis=axis)
            a = acc / (2 * radius + 1)
    return a


scale = w / 1024.0
# The dirt's local average color, and how far each pixel is from it
local = blur(px, max(2, int(24 * scale)))
dist = np.sqrt(((px - local) ** 2).sum(axis=2))
feature = np.clip((dist - 0.06) / 0.12, 0.0, 1.0)
# Fill the inside of each pebble (their middles are close to flat color too, but sit inside a ring of outline)
feature = np.clip(blur(feature, max(1, int(5 * scale))) * 2.2, 0.0, 1.0)
# Domes: blur the filled shapes and round them off
dome = blur(feature, max(1, int(6 * scale)))
dome = np.clip(dome / max(dome.max(), 1e-6), 0.0, 1.0) ** 0.8

# Dirt undulation: its lighter patches a little higher, very soft
grey = px.mean(axis=2)
undulate = blur(grey - grey.mean(), max(2, int(20 * scale)))
undulate = undulate / max(np.abs(undulate).max(), 1e-6)

height = np.clip(0.18 + 0.72 * dome + 0.1 * undulate, 0.0, 1.0)
out = bpy.data.images.new("height", w, h, alpha=False, float_buffer=False)
out.colorspace_settings.name = "Non-Color"
rgba = np.stack([height, height, height, np.ones_like(height)], axis=2)
out.pixels[:] = rgba.ravel()
out.filepath_raw = dst
out.file_format = "PNG"
out.save()
print(f"HEIGHT saved {dst} ({w} x {h}), features cover {float((dome > 0.3).mean()) * 100:.0f}%")
