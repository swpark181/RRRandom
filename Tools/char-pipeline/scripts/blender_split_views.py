# Split a multi-view concept sheet (views on a plain background) into separate square images.
# Usage: blender -b --factory-startup --python blender_split_views.py -- <sheet> <rows> <cols> <out_prefix> [size] [min gap px]
# Finds the rows/cols of content by scanning for background gaps, keeps the largest `rows` x `cols` blobs
# (so stray captions are dropped), and writes <out_prefix>_r<row>c<col>.png, padded square with the background color.
import sys, bpy, numpy as np

argv = sys.argv[sys.argv.index("--") + 1:]
sheet, rows, cols, prefix = argv[0], int(argv[1]), int(argv[2]), argv[3]
size = int(argv[4]) if len(argv) > 4 else 1536

img = bpy.data.images.load(sheet)
w, h = img.size
px = np.empty(w * h * 4, dtype=np.float32)
img.pixels.foreach_get(px)
px = px.reshape(h, w, 4)[::-1, :, :3]  # top row first

corners = np.concatenate([px[:20, :20].reshape(-1, 3), px[:20, -20:].reshape(-1, 3),
                          px[-20:, :20].reshape(-1, 3), px[-20:, -20:].reshape(-1, 3)])
bg = np.median(corners, axis=0)
mask = np.abs(px - bg).max(axis=2) > 0.06


def runs(profile, min_gap, min_len):
    on = profile > 0
    out, start, gap = [], None, 0
    for i, v in enumerate(on):
        if v:
            if start is None:
                start = i
            gap = 0
            end = i
        elif start is not None:
            gap += 1
            if gap >= min_gap:
                out.append((start, end)); start = None
    if start is not None:
        out.append((start, end))
    return [r for r in out if r[1] - r[0] >= min_len]


def biggest(segs, n, weight):
    segs = sorted(segs, key=lambda s: -weight(s))[:n]
    return sorted(segs)


gap = int(argv[5]) if len(argv) > 5 else max(12, w // 120)
row_profile = mask.sum(axis=1) > w * 0.002
row_segs = biggest(runs(row_profile, gap, h // 20), rows, lambda s: mask[s[0]:s[1] + 1].sum())
for ri, (y0, y1) in enumerate(row_segs):
    band = mask[y0:y1 + 1]
    col_profile = band.sum(axis=0) > 2
    col_segs = biggest(runs(col_profile, gap, w // 40), cols, lambda s: band[:, s[0]:s[1] + 1].sum())
    for ci, (x0, x1) in enumerate(col_segs):
        sub = mask[y0:y1 + 1, x0:x1 + 1]
        ys = np.where(sub.any(axis=1))[0]
        by0, by1 = y0 + ys[0], y0 + ys[-1]
        bw, bh = x1 - x0 + 1, by1 - by0 + 1
        side = int(max(bw, bh) * 1.12)
        canvas = np.empty((side, side, 3), dtype=np.float32); canvas[:] = bg
        ox, oy = (side - bw) // 2, (side - bh) // 2
        canvas[oy:oy + bh, ox:ox + bw] = px[by0:by1 + 1, x0:x1 + 1]
        out = bpy.data.images.new("v", side, side, alpha=False)
        rgba = np.ones((side, side, 4), dtype=np.float32); rgba[..., :3] = canvas
        out.pixels.foreach_set(rgba[::-1].ravel())
        if side != size:
            out.scale(size, size)
        path = f"{prefix}_r{ri}c{ci}.png"
        out.filepath_raw = path; out.file_format = "PNG"; out.save()
        print("VIEW", path, "box", x0, by0, x1, by1)
