"""One orthographic front picture resampled into another front picture's frame (run inside Blender), 2026-11-20.

The cinematic's expression candidates were drawn on blender_expression.py's head render (its camera JSON); the new
models' faces are carried over in blender_face_audit.py's frame (<prefix>_cam.json). Both look straight along -Y at the
same model, so a pixel maps by world x / z alone: this writes the candidate as it would look in the audit frame, over a
base picture of that frame where the candidate does not reach.

Usage: blender -b --factory-startup --python blender_image_reframe.py -- <src png> <src cam json> <dst cam json> <out png>
           [--base <png in the dst frame>]
"""
import json
import sys

import bpy
import numpy as np

argv = sys.argv[sys.argv.index("--") + 1:]
src_path, src_cam, dst_cam, out = argv[:4]
opts = dict(zip(argv[4::2], argv[5::2]))
sc = json.load(open(src_cam))
dc = json.load(open(dst_cam))


def load(path):
    im = bpy.data.images.load(path)
    w, h = im.size
    return np.array(im.pixels[:], dtype=np.float32).reshape(h, w, im.channels)[::-1, :, :3].copy()  # row 0 = top


src = load(src_path)
sh, sw = src.shape[:2]
res = dc["res"]
base = load(opts["--base"]) if "--base" in opts else np.zeros((res, res, 3), np.float32)
py, px = np.mgrid[0:res, 0:res].astype(np.float64) + 0.5
x = dc["center"][0] + (px / res - 0.5) * dc["ortho"]
z = dc["center"][2] + (0.5 - py / res) * dc["ortho"]
u = ((x - sc["center"][0]) / sc["ortho"] + 0.5) * sw - 0.5
v = (0.5 - (z - sc["center"][2]) / sc["ortho"]) * sh - 0.5
inside = (u >= 0) & (u <= sw - 1.001) & (v >= 0) & (v <= sh - 1.001)
u0 = np.clip(u.astype(int), 0, sw - 2)
v0 = np.clip(v.astype(int), 0, sh - 2)
fu = (u - u0)[..., None]
fv = (v - v0)[..., None]
samp = (src[v0, u0] * (1 - fu) * (1 - fv) + src[v0, u0 + 1] * fu * (1 - fv)
        + src[v0 + 1, u0] * (1 - fu) * fv + src[v0 + 1, u0 + 1] * fu * fv)
outp = np.where(inside[..., None], samp, base[:res, :res])
img = bpy.data.images.new("out", res, res)
img.pixels = np.concatenate([outp[::-1], np.ones((res, res, 1), np.float32)], 2).ravel().tolist()
img.filepath_raw = out
img.file_format = "PNG"
img.save()
print(f"REFRAME {out}: {inside.mean() * 100:.0f}% of the frame from the source, 1 source pixel = "
      f"{(sc['ortho'] / sw) / (dc['ortho'] / res):.2f} destination pixels")
