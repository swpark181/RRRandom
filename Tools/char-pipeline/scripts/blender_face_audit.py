"""How well a head suits texture expressions (2026-11-19): pictures of the head and numbers for its face relief.

A face whose eyes, brows and mouth are textures wants a smooth surface under them: sculpted eyeballs, lid and brow ridges
and a mouth crease stay put when the picture on them changes. This renders the head of a Tripo FBX (stands on +Z, faces -Y)
from the front, three-quarter, side and from above, textured and as grey clay, and measures the face from the front:
depth of the front-most surface on a grid, a smooth surface (a quadric) fitted to the face zone without the nose, and what
stands out of it ("relief", + toward the viewer), as % of the head's width.

  blender -b --factory-startup --python blender_face_audit.py -- <fbx> <basecolor|none> <out prefix>
        [--cz 0.66] [--span 0.52] [--px 700] [--zone cx,cy,rx,ry] [--nose cx,cy,rx,ry] [--eyes "cx,cy,rx,ry;cx,cy,rx,ry"]
        [--mouth cx,cy,rx,ry]
  --cz    height of the middle of the face (0 = feet, 1 = top of the bounds), --span the square's side, both in body heights
  zones   ellipses in the front picture, 0..1 from its left / from its TOP (centre x, centre y, radius x, radius y)
Writes <prefix>_tex_<view>.png, <prefix>_clay_<view>.png (front, q34, side, top), <prefix>_relief.png (red = stands out,
blue = sunk in, the zones drawn in), <prefix>_stats.json.
"""
import json
import math
import sys

import bpy
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

argv = sys.argv[sys.argv.index("--") + 1:]
opt = {"cz": 0.66, "span": 0.52, "px": 700,
       "zone": "0.5,0.50,0.30,0.27", "nose": "0.5,0.60,0.07,0.06",
       "eyes": "0.37,0.44,0.11,0.10;0.63,0.44,0.11,0.10", "mouth": "0.5,0.70,0.16,0.07"}
rest = []
i = 0
while i < len(argv):
    if argv[i].startswith("--"):
        opt[argv[i][2:]] = argv[i + 1]
        i += 2
    else:
        rest.append(argv[i])
        i += 1
fbx, tex_path, prefix = rest[:3]
px = int(opt["px"])


def ell(s):
    return [float(v) for v in s.split(",")]


zone, nose, mouth = ell(opt["zone"]), ell(opt["nose"]), ell(opt["mouth"])
eyes = [ell(e) for e in opt["eyes"].split(";")]

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
bpy.ops.import_scene.fbx(filepath=fbx)
meshes = [o for o in bpy.data.objects if o.type == "MESH"]
mesh = max(meshes, key=lambda o: len(o.data.polygons))
for o in meshes:
    if o is not mesh:
        o.hide_render = True
corners = [mesh.matrix_world @ Vector(c) for c in mesh.bound_box]
lo = Vector([min(c[k] for c in corners) for k in range(3)])
hi = Vector([max(c[k] for c in corners) for k in range(3)])
height = hi.z - lo.z
me = mesh.data
me.calc_loop_triangles()
mw = mesh.matrix_world
verts = [mw @ v.co for v in me.vertices]
# the body's middle: the feet and head, not the tail that sweeps to one side
head_v = [v for v in verts if v.z > lo.z + height * 0.55]
cx = (min(v.x for v in head_v) + max(v.x for v in head_v)) / 2
head_w = max(v.x for v in head_v) - min(v.x for v in head_v)
face_z = lo.z + height * float(opt["cz"])
span = height * float(opt["span"])
# the head's own middle front to back (not the body's: a tail behind or T-pose arms would push the face out of the side views)
face_v = [v for v in verts if abs(v.z - face_z) < height * 0.12]
cy = (min(v.y for v in face_v) + max(v.y for v in face_v)) / 2
target = Vector((cx, cy, face_z))
print(f"AUDIT {fbx}: {len(me.polygons)} faces, height {height:.3f}, head width {head_w:.3f}, centre x {cx:.3f}")

if tex_path != "none":
    mat = bpy.data.materials.new("M")
    mat.use_nodes = True
    t = mat.node_tree.nodes.new("ShaderNodeTexImage")
    t.image = bpy.data.images.load(tex_path)
    mat.node_tree.links.new(t.outputs["Color"], mat.node_tree.nodes["Principled BSDF"].inputs["Base Color"])
    mat.node_tree.nodes.active = t
    me.materials.clear()
    me.materials.append(mat)

# the front picture's camera, in blender_expression.py's form: its "bake" can project an edited <prefix>_tex_front.png back
json.dump({"center": list(target), "forward": [0.0, -1.0, 0.0], "up": [0.0, 0.0, 1.0], "size": height * 0.3,
           "ortho": span, "cam": list(target + Vector((0, -1, 0)) * height * 4), "res": px},
          open(f"{prefix}_cam.json", "w"), indent=1)

cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
scene.collection.objects.link(cam)
scene.camera = cam
cam.data.type = "ORTHO"
cam.data.ortho_scale = span
scene.render.engine = "BLENDER_WORKBENCH"
scene.render.resolution_x = scene.render.resolution_y = px
scene.render.film_transparent = False
scene.view_settings.view_transform = "Standard"
world = bpy.data.worlds.new("W")
world.color = (0.78, 0.79, 0.81)
scene.world = world
sh = scene.display.shading
VIEWS = {"front": Vector((0, -1, 0)), "q34": Vector((1, -1, 0.05)).normalized(), "side": Vector((1, 0, 0)),
         "top": Vector((0, -0.75, 0.66)).normalized()}
for mode in ("tex", "clay"):
    if mode == "tex":
        sh.light = "FLAT"
        sh.color_type = "TEXTURE"
        sh.show_cavity = False
    else:
        sh.light = "STUDIO"
        sh.color_type = "SINGLE"
        sh.single_color = (0.80, 0.80, 0.80)
        sh.show_cavity = True
        sh.cavity_type = "BOTH"
        sh.cavity_ridge_factor = 1.0
        sh.cavity_valley_factor = 1.6
        sh.curvature_ridge_factor = 1.2
        sh.curvature_valley_factor = 1.6
        sh.show_shadows = False
        sh.show_specular_highlight = True
    for name, d in VIEWS.items():
        cam.location = target + d * height * 4
        cam.rotation_euler = (-d).to_track_quat("-Z", "Y").to_euler()
        scene.render.filepath = f"{prefix}_{mode}_{name}.png"
        bpy.ops.render.render(write_still=True)

# ---- relief from the front
G = 320
tri_v = [[verts[k] for k in t.vertices] for t in me.loop_triangles]
bvh = BVHTree.FromPolygons([tuple(v) for v in verts], [tuple(t.vertices) for t in me.loop_triangles])
depth = np.full((G, G), np.nan)
facing = np.full((G, G), np.nan)
y0 = lo.y - height
for r in range(G):
    z = face_z + span / 2 - (r + 0.5) / G * span
    for c in range(G):
        x = cx - span / 2 + (c + 0.5) / G * span
        hit, nrm, _, dist = bvh.ray_cast(Vector((x, y0, z)), Vector((0, 1, 0)))
        if hit is not None:
            depth[r, c] = -(hit.y)          # bigger = nearer the viewer
            facing[r, c] = abs(nrm.y)
yy, xx = np.mgrid[0:G, 0:G]
u = (xx + 0.5) / G
v = (yy + 0.5) / G


def inside(e, grow=1.0):
    return ((u - e[0]) / (e[2] * grow)) ** 2 + ((v - e[1]) / (e[3] * grow)) ** 2 <= 1.0


valid = ~np.isnan(depth)
zmask = inside(zone) & valid
fit_mask = zmask & ~inside(nose, 1.25)
A = np.c_[np.ones(fit_mask.sum()), u[fit_mask], v[fit_mask], u[fit_mask] ** 2, u[fit_mask] * v[fit_mask], v[fit_mask] ** 2]
w = np.ones(len(A))
for _ in range(6):  # robust: bumps and pits weigh less, so the fit follows the head's broad shape
    coef, *_ = np.linalg.lstsq(A * w[:, None], depth[fit_mask] * w, rcond=None)
    res = depth[fit_mask] - A @ coef
    s = np.median(np.abs(res)) * 1.5 + 1e-9
    w = 1.0 / (1.0 + (res / (3 * s)) ** 2)
fit = coef[0] + coef[1] * u + coef[2] * v + coef[3] * u ** 2 + coef[4] * u * v + coef[5] * v ** 2
relief = (depth - fit) / head_w * 100.0     # % of head width


def stats(mask):
    r = relief[mask & valid]
    f = facing[mask & valid]
    if len(r) == 0:
        return None
    return {"rms": round(float(np.sqrt((r ** 2).mean())), 2), "p95": round(float(np.percentile(np.abs(r), 95)), 2),
            "max_out": round(float(r.max()), 2), "max_in": round(float(r.min()), 2),
            "facing_mean": round(float(f.mean()), 3), "facing_ok_share": round(float((f >= 0.8).mean()), 3)}


out = {"fbx": fbx, "faces": len(me.polygons), "head_width": head_w, "height": height,
       "zone": stats(fit_mask), "eye_l": stats(inside(eyes[0])), "eye_r": stats(inside(eyes[1])),
       "mouth": stats(inside(mouth) & ~inside(nose, 1.25))}
# local bumpiness: the relief minus its own blur (ridges and creases a few % of the head wide)
rl = np.where(valid, relief, 0.0)
k = 9
ker = np.ones(2 * k + 1) / (2 * k + 1)
bl = np.apply_along_axis(lambda a: np.convolve(a, ker, mode="same"), 0, rl)
bl = np.apply_along_axis(lambda a: np.convolve(a, ker, mode="same"), 1, bl)
fine = rl - bl
for key, m in (("zone", fit_mask), ("eye_l", inside(eyes[0])), ("eye_r", inside(eyes[1])),
               ("mouth", inside(mouth) & ~inside(nose, 1.25))):
    if out[key]:
        out[key]["fine_rms"] = round(float(np.sqrt((fine[m & valid] ** 2).mean())), 3)
json.dump(out, open(f"{prefix}_stats.json", "w"), indent=1)
print("AUDIT stats " + json.dumps(out))

# heat map: red out, blue in, +-4 % of the head width full colour; zones outlined
S = 2
img = np.zeros((G, G, 4), dtype=np.float32)
t = np.clip(np.nan_to_num(relief) / 4.0, -1, 1)
base = np.ones((G, G, 3)) * 0.93
pos = np.clip(t, 0, 1)[..., None]
neg = np.clip(-t, 0, 1)[..., None]
col = base * (1 - pos) + np.array([0.84, 0.15, 0.16]) * pos
col = col * (1 - neg) + np.array([0.13, 0.35, 0.80]) * neg
col[~valid] = (0.78, 0.79, 0.81)
col[valid & ~zmask] = col[valid & ~zmask] * 0.45 + 0.5 * 0.55


def outline(e, c):
    d = ((u - e[0]) / e[2]) ** 2 + ((v - e[1]) / e[3]) ** 2
    edge = np.abs(d - 1.0) < 0.06
    col[edge] = c


outline(zone, (0.1, 0.1, 0.1))
outline(nose, (0.4, 0.4, 0.4))
for e in eyes:
    outline(e, (0.0, 0.45, 0.2))
outline(mouth, (0.55, 0.3, 0.0))
img[..., :3] = col
img[..., 3] = 1.0
big = np.repeat(np.repeat(img[::-1], S, 0), S, 1)
im = bpy.data.images.new("relief", G * S, G * S)
im.pixels.foreach_set(big.ravel())
im.filepath_raw = f"{prefix}_relief.png"
im.file_format = "PNG"
im.save()
print(f"AUDIT wrote {prefix}_*.png")
