"""Thin soft lines drawn on a picture along given points (run in Blender; 2026-11-20: the gaps near the corners of
the fox's mouth line, where the front projection met the muzzle's side at a shallow angle and the line came out too faint).

For joining pieces of a line that is already there (a mouth line), not for painting fur or colour areas: draw on the
render of the view that looks square onto that surface, then bake with that view's camera (blender_expression.py bake).

  blender -b --factory-startup --python blender_stroke.py -- <in.png> <out.png> "<x,y;x,y;...|x,y;...>"
        [--width 6] [--taper 0.4] [--color 0.13,0.07,0.06] [--opacity 0.9] [--soft 1.5]
  points in pixels (y from the top), each '|' part one line, drawn as a smooth curve through its points
  --width  the line's width in pixels at its first point; --taper how much thinner it is at its last point (0..1)
  --soft   the edge's softness in pixels
"""
import sys

import bpy
import numpy as np

argv = sys.argv[sys.argv.index("--") + 1:]
opt = {"width": "6", "taper": "0.4", "color": "0.13,0.07,0.06", "opacity": "0.9", "soft": "1.5"}
rest = []
i = 0
while i < len(argv):
    if argv[i].startswith("--"):
        opt[argv[i][2:]] = argv[i + 1]
        i += 2
    else:
        rest.append(argv[i])
        i += 1
in_path, out_path, lines = rest[:3]
width, taper, opacity, soft = float(opt["width"]), float(opt["taper"]), float(opt["opacity"]), float(opt["soft"])
color = np.array([float(v) for v in opt["color"].split(",")], dtype=np.float32)

im = bpy.data.images.load(in_path)
W, H = im.size
a = np.empty(W * H * im.channels, dtype=np.float32)
im.pixels.foreach_get(a)
pic = a.reshape(H, W, im.channels)[::-1, :, :3].copy()


def curve(pts, n=24):
    """Catmull-Rom through the points, n samples a span; returns the samples and how far along (0..1) each is."""
    p = np.array(pts, dtype=np.float64)
    p = np.vstack([p[0] * 2 - p[1], p, p[-1] * 2 - p[-2]])
    out = []
    for k in range(1, len(p) - 2):
        for t in np.linspace(0, 1, n, endpoint=False):
            out.append(0.5 * ((2 * p[k]) + (-p[k - 1] + p[k + 1]) * t + (2 * p[k - 1] - 5 * p[k] + 4 * p[k + 1] - p[k + 2]) * t * t
                              + (-p[k - 1] + 3 * p[k] - 3 * p[k + 1] + p[k + 2]) * t ** 3))
    out.append(p[-2])
    out = np.array(out)
    seg = np.r_[0.0, np.cumsum(np.linalg.norm(np.diff(out, axis=0), axis=1))]
    return out, seg / max(seg[-1], 1e-9)


drawn = 0
for part in lines.split("|"):
    pts = [[float(v) for v in q.split(",")] for q in part.split(";")]
    c, s = curve(pts)
    x0, x1 = int(max(0, c[:, 0].min() - width - 4)), int(min(W, c[:, 0].max() + width + 5))
    y0, y1 = int(max(0, c[:, 1].min() - width - 4)), int(min(H, c[:, 1].max() + width + 5))
    yy, xx = np.mgrid[y0:y1, x0:x1]
    d = np.sqrt((xx[..., None] + 0.5 - c[:, 0]) ** 2 + (yy[..., None] + 0.5 - c[:, 1]) ** 2)
    half = width * (1.0 - taper * s) / 2.0
    cover = np.clip((half - d) / max(soft, 1e-6) + 0.5, 0.0, 1.0).max(axis=2) * opacity
    box = pic[y0:y1, x0:x1]
    pic[y0:y1, x0:x1] = box * (1 - cover[..., None]) + np.minimum(box, color) * cover[..., None]   # only ever darkens
    drawn += int((cover > 0.5).sum())

out = bpy.data.images.new("out", W, H)
rgba = np.concatenate([pic[::-1], np.ones((H, W, 1), dtype=np.float32)], axis=2)
out.pixels.foreach_set(rgba.astype(np.float32).ravel())
out.filepath_raw = out_path
out.file_format = "PNG"
bpy.context.scene.view_settings.view_transform = "Standard"
out.save()
print(f"STROKE {out_path}: {len(lines.split('|'))} lines, {drawn} px covered")
