"""Paint over the light spots on the backs of a Tripo character's ears with the ears' own dark color (run inside Blender, -b).

Usage: blender -b --factory-startup --python blender_ear_back_fill.py -- <fbx> <basecolor in> <png out> [--top 0.2] [--side 0.12]

Tripo FBX for Blender: +Z up, front -Y. Ear-back triangles: in the top <top> share of the height, at least <side> x height
away from the middle line (so the head tuft is left alone) and facing backward (normal Y > 0.25). Their texels are found
in UV; the light and grey ones (value > 0.13, low saturation: the white spot and its soft grey edge) are repainted
with the median of the dark ones, with a 3-texel feather. Prints how many triangles and texels it touched. Round 72 (fluffy tiger: no white spots behind the ears).
"""
import sys

import bpy
import numpy as np

args = sys.argv[sys.argv.index("--") + 1:]
fbx, tex_in, tex_out = args[0], args[1], args[2]
top = float(args[args.index("--top") + 1]) if "--top" in args else 0.2
side = float(args[args.index("--side") + 1]) if "--side" in args else 0.12

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=fbx)
obj = [o for o in bpy.data.objects if o.type == "MESH"][0]
mesh = obj.data
mw = obj.matrix_world
verts = np.array([tuple(mw @ v.co) for v in mesh.vertices])
lo, hi = verts.min(0), verts.max(0)
height = hi[2] - lo[2]
cx = (lo[0] + hi[0]) / 2
uv_layer = mesh.uv_layers.active.data

img = bpy.data.images.load(tex_in)
w, h = img.size
px = np.array(img.pixels[:], dtype=np.float32).reshape(h, w, 4)

mask = np.zeros((h, w), bool)
tris = 0
mesh.calc_loop_triangles()
normal_m = mw.to_3x3()
for t in mesh.loop_triangles:
    p = verts[list(t.vertices)]
    c = p.mean(0)
    n = (normal_m @ t.normal).normalized()
    if c[2] < hi[2] - top * height or abs(c[0] - cx) < side * height or n.y <= 0.25:
        continue
    tris += 1
    uv = np.array([tuple(uv_layer[l].uv) for l in t.loops]) * [w, h]
    x0, y0 = np.floor(uv.min(0)).astype(int)
    x1, y1 = np.ceil(uv.max(0)).astype(int)
    xs, ys = np.meshgrid(np.arange(max(x0, 0), min(x1 + 1, w)), np.arange(max(y0, 0), min(y1 + 1, h)))
    q = np.stack([xs + 0.5, ys + 0.5], -1)
    a, b, cc = uv
    v0, v1, v2 = b - a, cc - a, q - a
    d = v0[0] * v1[1] - v1[0] * v0[1]
    if abs(d) < 1e-9:
        continue
    l1 = (v2[..., 0] * v1[1] - v1[0] * v2[..., 1]) / d
    l2 = (v0[0] * v2[..., 1] - v2[..., 0] * v0[1]) / d
    inside = (l1 >= -0.02) & (l2 >= -0.02) & (l1 + l2 <= 1.04)
    mask[ys[inside], xs[inside]] = True

rgb = px[..., :3]
mx, mn = rgb.max(-1), rgb.min(-1)
sat = np.where(mx > 0, (mx - mn) / np.maximum(mx, 1e-6), 0)
light = mask & (mx > 0.13) & (sat < 0.4)
dark = mask & (mx < 0.2)
fill = np.median(rgb[dark], 0) if dark.any() else np.array([0.04, 0.04, 0.04])
print(f"EARFILL tris {tris} mask {mask.sum()} light {light.sum()} dark {dark.sum()} fill {np.round(fill * 255)}")

# Feather: grow the light area by 3 texels inside the ear mask, blend the ring halfway
grown = light.copy()
for _ in range(3):
    g = grown.copy()
    g[1:] |= grown[:-1]; g[:-1] |= grown[1:]; g[:, 1:] |= grown[:, :-1]; g[:, :-1] |= grown[:, 1:]
    grown = g & mask
ring = grown & ~light
rgb[light] = fill
rgb[ring] = rgb[ring] * 0.5 + fill * 0.5
px[..., :3] = rgb
img.pixels[:] = px.ravel()
img.filepath_raw = tex_out
img.file_format = "PNG"
img.save()
print(f"EARFILL wrote {tex_out}")
