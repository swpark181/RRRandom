"""One region of a picture pasted into another of the same view (2026-11-19, mouth frames for texture "talking").

An image editor asked to change only the mouth still nudges the eyes a little, and eyes that shift between mouth frames
jitter when the frames are flipped. So each frame = the base picture everywhere, the edited picture only inside an
ellipse (soft edge), e.g. the mouth.

  blender -b --factory-startup --python blender_image_region.py -- <base.png> <edited.png> <out.png> <cx,cy,rx,ry> [fade 14]
  the ellipse in the base picture's pixels, x from the left, y from the TOP. The edited picture is resampled to the base's size.
"""
import sys

import bpy
import numpy as np

args = sys.argv[sys.argv.index("--") + 1:]
base_path, edit_path, out_path = args[:3]
cx, cy, rx, ry = [float(v) for v in args[3].split(",")]
fade = float(args[4]) if len(args) > 4 else 14.0


def load(path):
    im = bpy.data.images.load(path)
    w, h = im.size
    a = np.empty(w * h * im.channels, dtype=np.float32)
    im.pixels.foreach_get(a)
    return a.reshape(h, w, im.channels)[::-1, :, :3].copy(), w, h   # row 0 = top


base, w, h = load(base_path)
edit, ew, eh = load(edit_path)
if (ew, eh) != (w, h):
    yy = (np.arange(h) * eh / h).astype(int)
    xx = (np.arange(w) * ew / w).astype(int)
    edit = edit[yy][:, xx]
y, x = np.mgrid[0:h, 0:w]
d = (np.sqrt(((x + 0.5 - cx) / rx) ** 2 + ((y + 0.5 - cy) / ry) ** 2) - 1.0) * min(rx, ry)
t = np.clip(1.0 - d / max(fade, 1e-6), 0.0, 1.0)
t = np.where(d <= 0, 1.0, t)
t = (t * t * (3 - 2 * t))[..., None]
out = base * (1 - t) + edit * t
img = bpy.data.images.new("out", w, h)
rgba = np.concatenate([out[::-1], np.ones((h, w, 1), dtype=np.float32)], axis=2)
img.pixels.foreach_set(rgba.astype(np.float32).ravel())
img.filepath_raw = out_path
img.file_format = "PNG"
scene = bpy.context.scene
scene.view_settings.view_transform = "Standard"
img.save()
changed = float((np.abs(edit - base).max(axis=2) > 0.08)[t[..., 0] > 0.5].mean())
print(f"REGION {out_path}: ellipse ({cx:.0f},{cy:.0f}) r ({rx:.0f},{ry:.0f}), {changed * 100:.1f}% of it differs from the base")
