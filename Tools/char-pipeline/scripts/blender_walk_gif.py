# Blender -b --python make_gif.py -- <shots dir> <prefix> <out dir> <name>
# Shots <prefix>_0..65.png = side, front, back (22 each, 0.08 s of game time apart). Crops around the character,
# trims each view to one walk cycle (the frame most like the first), writes <out>/<name>_<view>.gif with a shared palette.
import os
import sys

import bpy
import numpy as np

args = sys.argv[sys.argv.index("--") + 1:]
shots, prefix, out, name = args[:4]
# Optional 5th argument: the views' names in shot order (round 85 flight check: rise_side,glide_side,rise_front,glide_back)
VIEWS = args[4].split(",") if len(args) > 4 else ["side", "front", "back"]
PER = 22
# RR_GIF_CROP "r0,r1,c0,c1" moves the crop (a flying character sits higher in the shot); RR_GIF_ALL=1 keeps every frame
# (no one-cycle trim: a glide doesn't repeat); RR_GIF_DELAY sets centiseconds per frame
CROP = tuple(int(v) for v in os.environ.get("RR_GIF_CROP", "100,620,380,900").split(","))  # rows, cols in the 1280x720 shot (top down)
SIZE = 360
DELAY = int(os.environ.get("RR_GIF_DELAY", "8"))  # centiseconds: 0.08 s of game time per frame = real speed


def load(path):
    img = bpy.data.images.load(path)
    w, h = img.size
    px = np.empty(w * h * 4, np.float32)
    img.pixels.foreach_get(px)
    bpy.data.images.remove(img)
    a = (px.reshape(h, w, 4)[::-1, :, :3] * 255.0 + 0.5).astype(np.float32)
    r0, r1, c0, c1 = CROP
    return a[r0:r1, c0:c1]


def resize(a, n):
    h, w = a.shape[:2]
    ys = (np.arange(n) + 0.5) * h / n - 0.5
    xs = (np.arange(n) + 0.5) * w / n - 0.5
    y0 = np.clip(np.floor(ys).astype(int), 0, h - 2); fy = (ys - y0)[:, None, None]
    x0 = np.clip(np.floor(xs).astype(int), 0, w - 2); fx = (xs - x0)[None, :, None]
    t = a[y0][:, x0] * (1 - fx) + a[y0][:, x0 + 1] * fx
    b = a[y0 + 1][:, x0] * (1 - fx) + a[y0 + 1][:, x0 + 1] * fx
    return t * (1 - fy) + b * fy


def palette(frames):
    rng = np.random.default_rng(1)
    pts = np.concatenate([f.reshape(-1, 3) for f in frames])
    pts = pts[rng.choice(len(pts), min(60000, len(pts)), replace=False)]
    cen = pts[rng.choice(len(pts), 256, replace=False)].copy()
    for _ in range(12):
        lab = nearest(pts, cen)
        for k in range(256):
            m = pts[lab == k]
            if len(m):
                cen[k] = m.mean(0)
    return cen


def nearest(pts, cen):
    out = np.empty(len(pts), np.int64)
    c2 = (cen ** 2).sum(1)
    for i in range(0, len(pts), 20000):
        p = pts[i:i + 20000]
        out[i:i + 20000] = np.argmin(c2[None, :] - 2.0 * p @ cen.T, axis=1)
    return out


def lzw(idx):
    # GIF LZW, 8-bit minimum code size, variable width codes up to 12 bits
    clear, eoi = 256, 257
    table = {(i,): i for i in range(256)}
    nxt, width = 258, 9
    bits, nbits, outb = 0, 0, bytearray()

    def emit(code):
        nonlocal bits, nbits
        bits |= code << nbits
        nbits += width
        while nbits >= 8:
            outb.append(bits & 0xFF)
            bits >>= 8
            nbits -= 8

    emit(clear)
    d = {}
    cur = -1
    for v in idx.tolist():
        if cur < 0:
            cur = v
            continue
        key = (cur, v)
        c = d.get(key)
        if c is not None:
            cur = c
            continue
        emit(cur)
        if nxt < 4096:
            d[key] = nxt
            nxt += 1
            if nxt > (1 << width) and width < 12:
                width += 1
        else:
            emit(clear)
            d = {}
            nxt, width = 258, 9
        cur = v
    emit(cur)
    emit(eoi)
    if nbits:
        outb.append(bits & 0xFF)
    return bytes(outb)


def write_gif(path, frames, pal):
    n = frames[0].shape[0]
    b = bytearray(b"GIF89a")
    b += n.to_bytes(2, "little") * 2 + bytes([0xF7, 0, 0])
    b += np.clip(pal, 0, 255).astype(np.uint8).tobytes()
    b += b"\x21\xFF\x0BNETSCAPE2.0\x03\x01\x00\x00\x00"
    for f in frames:
        idx = nearest(f.reshape(-1, 3), pal)
        b += b"\x21\xF9\x04\x04" + DELAY.to_bytes(2, "little") + b"\x00\x00"
        b += b"\x2C\x00\x00\x00\x00" + n.to_bytes(2, "little") * 2 + b"\x00"
        data = lzw(idx)
        b += b"\x08"
        for i in range(0, len(data), 255):
            chunk = data[i:i + 255]
            b += bytes([len(chunk)]) + chunk
        b += b"\x00"
    b += b"\x3B"
    open(path, "wb").write(bytes(b))


os.makedirs(out, exist_ok=True)
views = [[resize(load(os.path.join(shots, f"{prefix}_{vi * PER + i}.png")), SIZE) for i in range(PER)] for vi in range(len(VIEWS))]
# one walk cycle, the same for every view: the view where some frame from 10 on looks most like the first sets it
best = None
for frames in views:
    diffs = [np.abs(frames[k] - frames[0]).mean() for k in range(10, PER)]
    k = 10 + int(np.argmin(diffs))
    ratio = diffs[k - 10] / np.mean(diffs)
    if best is None or ratio < best[1]:
        best = (k, ratio)
k = best[0] if best[1] < 0.6 and os.environ.get("RR_GIF_ALL") != "1" else PER
for view, frames in zip(VIEWS, views):
    keep = frames[:k]
    path = os.path.join(out, f"{name}_{view}.gif")
    write_gif(path, keep, palette(keep))
    print(f"GIF {path} {len(keep)} frames (cycle ratio {best[1]:.2f}), {os.path.getsize(path) // 1024} KB")
