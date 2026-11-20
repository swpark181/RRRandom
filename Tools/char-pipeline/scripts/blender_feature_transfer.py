"""Face features (eyes, brows, mouth) of one render carried over into another render of a different model, same view
(2026-11-20: the blank-face fox got its game face redrawn by an image model and lost the old fox's look; the old
fox's own eye, brow and mouth pixels moved over keep it).

Inside the given ellipses the picture becomes the SOURCE picture, shifted in colour so that it meets the destination at
the ellipse's edge (the difference between the two pictures at the edge is spread smoothly through the inside), so the
old fur around an eye does not show as a patch on fur of a slightly different orange.

  blender -b --factory-startup --python blender_feature_transfer.py -- <dst.png> <src.png> <out.png> "<regions>"
        --src x,y --dst x,y [--scale 1.0] [--feather 12] [--blur 8] [--gain 1.0]
  regions  "cx,cy,rx,ry[,deg]" separated by ';', ellipses in the SOURCE picture's pixels (y from the top, deg = turn
           clockwise on screen)
  --src / --dst  one point in the source and where it lands in the destination; --scale source pixel -> destination pixel
  --feather pixels over which the edge fades into the destination, --blur how far the edge colour difference is averaged
  --gain    how much stronger everything darker than the destination becomes (a faint mouth line: 1.8)
  --shift   share of the edge colour step spread inside (default 1; 0 = plain feathered paste)
"""
import sys

import bpy
import numpy as np

argv = sys.argv[sys.argv.index("--") + 1:]
opt = {"scale": "1.0", "feather": "12", "blur": "8", "gain": "1.0", "shift": "1.0"}
rest = []
i = 0
while i < len(argv):
    if argv[i].startswith("--"):
        opt[argv[i][2:]] = argv[i + 1]
        i += 2
    else:
        rest.append(argv[i])
        i += 1
dst_path, src_path, out_path, regions = rest[:4]
sx0, sy0 = [float(v) for v in opt["src"].split(",")]
dx0, dy0 = [float(v) for v in opt["dst"].split(",")]
scale, feather, blur_r = float(opt["scale"]), float(opt["feather"]), int(float(opt["blur"]))


def load(path):
    im = bpy.data.images.load(path)
    w, h = im.size
    a = np.empty(w * h * im.channels, dtype=np.float32)
    im.pixels.foreach_get(a)
    return a.reshape(h, w, im.channels)[::-1, :, :3].copy()   # row 0 = top


dst = load(dst_path)
src = load(src_path)
H, W = dst.shape[:2]
SH, SW = src.shape[:2]
yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
u = (xx - dx0) / scale + sx0          # where each destination pixel looks in the source
v = (yy - dy0) / scale + sy0

# the mask: inside any ellipse; d = rough distance to the edge in destination pixels (negative inside)
d = np.full((H, W), 1e9, dtype=np.float32)
for reg in regions.split(";"):
    p = [float(t) for t in reg.split(",")]
    cx, cy, rx, ry = p[:4]
    a = np.radians(p[4]) if len(p) > 4 else 0.0
    lx = (u - cx) * np.cos(a) + (v - cy) * np.sin(a)
    ly = -(u - cx) * np.sin(a) + (v - cy) * np.cos(a)
    d = np.minimum(d, (np.sqrt((lx / rx) ** 2 + (ly / ry) ** 2) - 1.0) * min(rx, ry) * scale)
inside = d < 0
ys, xs = np.where(inside)
pad = blur_r * 3 + 8
y0, y1 = max(0, ys.min() - pad), min(H, ys.max() + pad + 1)
x0, x1 = max(0, xs.min() - pad), min(W, xs.max() + pad + 1)
sl = (slice(y0, y1), slice(x0, x1))

# the source resampled onto the destination's pixels (bilinear), only the box that matters
uu, vv = np.clip(u[sl], 0, SW - 1.001), np.clip(v[sl], 0, SH - 1.001)
iu, iv = uu.astype(int), vv.astype(int)
fu, fv = (uu - iu)[..., None], (vv - iv)[..., None]
g = (src[iv, iu] * (1 - fu) * (1 - fv) + src[iv, iu + 1] * fu * (1 - fv)
     + src[iv + 1, iu] * (1 - fu) * fv + src[iv + 1, iu + 1] * fu * fv)
box_dst = dst[sl]
unknown = inside[sl]


def box_blur(a, r):
    if r <= 0:
        return a
    for axis in (0, 1):
        c = np.cumsum(np.pad(a, [(r + 1, r) if k == axis else (0, 0) for k in range(a.ndim)], mode="edge"), axis=axis)
        n = a.shape[axis]
        a = (np.take(c, np.arange(2 * r + 1, 2 * r + 1 + n), axis=axis) - np.take(c, np.arange(0, n), axis=axis)) / (2 * r + 1)
    return a


diff = box_blur(box_blur(box_dst - g, blur_r), blur_r)       # the colour step between the two pictures, texture averaged out


def jacobi(val, unk, n):
    for _ in range(n):
        p = np.pad(val, ((1, 1), (1, 1), (0, 0)), mode="edge")
        avg = (p[:-2, 1:-1] + p[2:, 1:-1] + p[1:-1, :-2] + p[1:-1, 2:]) * 0.25
        val = np.where(unk[..., None], avg, val)
    return val


def membrane(val, unk):
    """val known where not unk; the unknown part filled so it is smooth (Laplace) and meets the known part."""
    h, w = unk.shape
    if min(h, w) <= 48 or not unk.any():
        return jacobi(np.where(unk[..., None], 0.0, val), unk, 600)
    ph, pw = h % 2, w % 2
    vp = np.pad(val, ((0, ph), (0, pw), (0, 0)), mode="edge")
    up = np.pad(unk, ((0, ph), (0, pw)), mode="edge")
    known = (~up).astype(np.float32)[..., None]
    k4 = known[0::2, 0::2] + known[1::2, 0::2] + known[0::2, 1::2] + known[1::2, 1::2]
    v4 = (vp * known)[0::2, 0::2] + (vp * known)[1::2, 0::2] + (vp * known)[0::2, 1::2] + (vp * known)[1::2, 1::2]
    coarse = membrane(v4 / np.maximum(k4, 1.0), k4[..., 0] == 0)
    fine = np.repeat(np.repeat(coarse, 2, axis=0), 2, axis=1)[:h, :w]
    return jacobi(np.where(unk[..., None], fine, val), unk, 60)


hfield = membrane(diff, unknown)
t = np.clip(-d[sl] / max(feather, 1e-6), 0.0, 1.0)
t = (t * t * (3 - 2 * t))[..., None]
out = dst.copy()
# --shift: how much of that colour step is spread inside (0: the source pasted as it is, only the feathered edge
# blends; the tiger 2026-11-20, whose white eye patches sit on orange in the new face and turned orange at 1)
new = g + hfield * float(opt["shift"])
gain = float(opt["gain"])
if gain != 1.0:                       # faint dark lines (a mouth) made stronger: what is darker than the destination, times gain
    new = box_dst + np.maximum(new - box_dst, 0.0) - np.maximum(box_dst - new, 0.0) * gain
out[sl] = np.clip(box_dst * (1 - t) + new * t, 0.0, 1.0)

img = bpy.data.images.new("out", W, H)
rgba = np.concatenate([out[::-1], np.ones((H, W, 1), dtype=np.float32)], axis=2)
img.pixels.foreach_set(rgba.astype(np.float32).ravel())
img.filepath_raw = out_path
img.file_format = "PNG"
bpy.context.scene.view_settings.view_transform = "Standard"
img.save()
step = np.abs(diff[unknown]).mean(axis=0) if unknown.any() else np.zeros(3)
print(f"TRANSFER {out_path}: {int(inside.sum())} px in {len(regions.split(';'))} regions, box x {x0}-{x1} y {y0}-{y1}, "
      f"mean colour step at the source (r,g,b) {tuple(round(float(c), 3) for c in step)}")
