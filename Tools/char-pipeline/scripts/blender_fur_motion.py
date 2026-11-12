"""Animated fur preview with forces, after Sky: Children of the Light's GDC 2025 talk (run inside Blender, EEVEE; round 95).

What the talk is known to do (speaker clip "Applying Forces to Fur" + a third-party summary of "Glitter, Fur and
Shadows"): wind from a scrolling Perlin-like noise ("random bumps that blow the fur around"), air resistance against the
character's motion (fur curls up while falling), and a one-mass spring per strand (it overshoots and jiggles when the
character stops or lands). Here each fur root has a small spring whose target is
    gravity sag + drag (-velocity) + inertia (-acceleration) + wind noise at the root, moving with time,
integrated every frame (semi-implicit Euler, 4 substeps); a strand/shell point at height h along the fur is pushed by
that offset x h^2, so roots stay put and tips swing most. The fox runs a fixed path: stand in the wind, run, stop
dead, jump and land.

The fur faces, eye/belt exclusions and edge fade are the same as blender_fur_preview.py (copied, not imported, since
that one runs as a script).

Usage: blender -b --factory-startup --python blender_fur_motion.py -- <fbx> <base color> <out dir> shells|strands
       [--frames 210] [--px 540] [--layers 12] [--length 0.012] [--strands 120000] [--view full|close]
  Writes <out dir>/<mode>_<view>_0001.png ... at 30 fps.
"""
import math
import os
import random
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:]
fbx, tex_path, out_dir, mode = argv[:4]


def opt(name, default, cast=float):
    return cast(argv[argv.index(name) + 1]) if name in argv else default


frames = opt("--frames", 210, int)
px = opt("--px", 540, int)
layers = opt("--layers", 12, int)
length = opt("--length", 0.012)
n_strands = opt("--strands", 120000, int)
view = opt("--view", "full", str)  # full: the whole fox from the side; close: head and shoulders
os.makedirs(out_dir, exist_ok=True)
FPS = 30

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
engines = {e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items}
scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in engines else "BLENDER_EEVEE"
scene.render.resolution_x = scene.render.resolution_y = px
scene.view_settings.view_transform = "Standard"
world = bpy.data.worlds.new("World")
world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.62, 0.70, 0.80, 1.0)
world.node_tree.nodes["Background"].inputs[1].default_value = 0.9
scene.world = world
sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
sun.data.energy = 2.6
sun.rotation_euler = (math.radians(45), 0, math.radians(-50))
scene.collection.objects.link(sun)

bpy.ops.import_scene.fbx(filepath=fbx)
body = next(o for o in bpy.data.objects if o.type == "MESH")
bpy.context.view_layer.objects.active = body
body.select_set(True)
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

img = bpy.data.images.load(tex_path)
W, H = img.size
pix = np.empty(W * H * 4, dtype=np.float32)
img.pixels.foreach_get(pix)
pix = pix.reshape(H, W, 4)


def sample(uv):
    x = min(W - 1, max(0, int(uv[0] % 1.0 * W)))
    y = min(H - 1, max(0, int(uv[1] % 1.0 * H)))
    return pix[y, x, :3]


def tex_material(name):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nt = mat.node_tree
    t = nt.nodes.new("ShaderNodeTexImage")
    t.image = img
    bsdf = nt.nodes["Principled BSDF"]
    bsdf.inputs["Roughness"].default_value = 0.8
    return mat, nt, t, bsdf


def multiply(nt, a, b):
    m = nt.nodes.new("ShaderNodeMix"); m.data_type = "RGBA"; m.blend_type = "MULTIPLY"
    m.inputs["Factor"].default_value = 1.0
    nt.links.new(a, m.inputs["A"]); nt.links.new(b, m.inputs["B"])
    return m.outputs["Result"]


def map_range(nt, src, lo, hi):
    m = nt.nodes.new("ShaderNodeMapRange")
    m.inputs["To Min"].default_value = lo
    m.inputs["To Max"].default_value = hi
    nt.links.new(src, m.inputs["Value"])
    return m.outputs["Result"]


# ---- fur faces (as blender_fur_preview.py) ----
me = body.data
bm = bmesh.new()
bm.from_mesh(me)
bm.faces.ensure_lookup_table()
bm.verts.ensure_lookup_table()
uv_layer = bm.loops.layers.uv.active
zs = [v.co.z for v in bm.verts]
zlo, zhi = min(zs), max(zs)
height = zhi - zlo
face_col = np.array([sample(sum((l[uv_layer].uv for l in f.loops), Vector((0, 0))) / len(f.loops)) for f in bm.faces])
r, g, b = face_col[:, 0], face_col[:, 1], face_col[:, 2]
fur = (r - b > 0.2) & (g / np.maximum(r, 1e-3) > 0.38)
centres = np.array([tuple(f.calc_center_median()) for f in bm.faces])
eye_white = (face_col.min(1) > 0.85) & (face_col.max(1) - face_col.min(1) < 0.08) & (centres[:, 2] > zlo + 0.6 * height)
bare = np.zeros(len(centres), bool)
for e in centres[eye_white][:: max(1, int(eye_white.sum()) // 400)]:
    bare |= np.linalg.norm(centres - e, axis=1) < 0.035 * height
cy_mid = (centres[:, 1].min() + centres[:, 1].max()) / 2
bare |= (centres[:, 2] > zlo + 0.15 * height) & (centres[:, 2] < zlo + 0.40 * height) & (centres[:, 1] < cy_mid + 0.05 * height)
fur &= ~bare
nbrs = [[g2.index for e in f.edges for g2 in e.link_faces if g2.index != f.index] for f in bm.faces]
for _ in range(2):
    vote = np.array([fur[n].mean() if n else 0.0 for n in nbrs])
    fur = (fur | (vote >= 0.6)) & ~bare
vw = np.array([np.mean([fur[f.index] for f in v.link_faces]) if v.link_faces else 0.0 for v in bm.verts])
print(f"MOTION {int(fur.sum())} fur faces of {len(fur)}")

mat, nt, t, bsdf = tex_material("Body")
fa = me.attributes.new("fur", "FLOAT", "POINT")
fa.data.foreach_set("value", vw.astype(np.float32))
af = nt.nodes.new("ShaderNodeAttribute"); af.attribute_name = "fur"
nt.links.new(multiply(nt, t.outputs["Color"], map_range(nt, af.outputs["Fac"], 1.0, 0.72)), bsdf.inputs["Base Color"])
body.data.materials.clear()
body.data.materials.append(mat)

L = length * height
fur_faces = [f for f, k in zip(bm.faces, fur) if k]

# ---- the fur geometry; roots (R x 3), normals, edge weights; positions rebuilt every frame from the spring offsets ----
if mode == "shells":
    vidx = sorted({v.index for f in fur_faces for v in f.verts})
    remap = {vi: i for i, vi in enumerate(vidx)}
    roots = np.array([tuple(bm.verts[i].co) for i in vidx])
    norms = np.array([tuple(bm.verts[i].normal) for i in vidx])
    wts = vw[vidx][:, None]
    R = len(vidx)
    faces, uvs = [], []
    for k in range(layers):
        for f in fur_faces:
            faces.append([k * R + remap[v.index] for v in f.verts])
            uvs.extend(tuple(l[uv_layer].uv) for l in f.loops)
    hs = np.repeat((np.arange(1, layers + 1) / layers), R)
    sm = bpy.data.meshes.new("Shells")
    sm.from_pydata([tuple(p) for p in np.tile(roots, (layers, 1))], [], faces)
    sm.uv_layers.new(name="UVMap").data.foreach_set("uv", [c for uv in uvs for c in uv])
    sm.attributes.new("h", "FLOAT", "POINT").data.foreach_set("value", hs.astype(np.float32))
    sm.attributes.new("root", "FLOAT_VECTOR", "POINT").data.foreach_set("vector", np.tile(roots, (layers, 1)).ravel())
    fur_obj = bpy.data.objects.new("Shells", sm)
    smat, snt, st, sbsdf = tex_material("Shell")
    smat.surface_render_method = "DITHERED"
    ah = snt.nodes.new("ShaderNodeAttribute"); ah.attribute_name = "h"
    ar = snt.nodes.new("ShaderNodeAttribute"); ar.attribute_name = "root"
    vor = snt.nodes.new("ShaderNodeTexVoronoi"); vor.voronoi_dimensions = "3D"
    vor.inputs["Scale"].default_value = 1.0 / (0.0022 * height)
    snt.links.new(ar.outputs["Vector"], vor.inputs["Vector"])
    sep = snt.nodes.new("ShaderNodeSeparateColor"); snt.links.new(vor.outputs["Color"], sep.inputs["Color"])
    lenf = map_range(snt, sep.outputs[0], 0.55, 1.0)
    rel = snt.nodes.new("ShaderNodeMath"); rel.operation = "DIVIDE"
    snt.links.new(ah.outputs["Fac"], rel.inputs[0]); snt.links.new(lenf, rel.inputs[1])
    rad = snt.nodes.new("ShaderNodeMath"); rad.operation = "SUBTRACT"; rad.inputs[0].default_value = 1.0
    snt.links.new(rel.outputs[0], rad.inputs[1])
    rad2 = snt.nodes.new("ShaderNodeMath"); rad2.operation = "MULTIPLY"; rad2.inputs[1].default_value = 0.48
    snt.links.new(rad.outputs[0], rad2.inputs[0])
    keep = snt.nodes.new("ShaderNodeMath"); keep.operation = "LESS_THAN"
    snt.links.new(vor.outputs["Distance"], keep.inputs[0]); snt.links.new(rad2.outputs[0], keep.inputs[1])
    snt.links.new(keep.outputs[0], sbsdf.inputs["Alpha"])
    snt.links.new(multiply(snt, st.outputs["Color"], map_range(snt, ah.outputs["Fac"], 0.7, 1.08)), sbsdf.inputs["Base Color"])
    sm.materials.append(smat)
else:
    rng = random.Random(7)
    areas = np.array([f.calc_area() for f in fur_faces])
    counts = np.random.default_rng(7).multinomial(n_strands, areas / areas.sum())
    rl, nl, ll, cl = [], [], [], []
    for f, c in zip(fur_faces, counts):
        loops = f.loops[:3]
        p = [l.vert.co for l in loops]
        n = [l.vert.normal for l in loops]
        q = [l[uv_layer].uv for l in loops]
        for _ in range(int(c)):
            a, b2 = rng.random(), rng.random()
            if a + b2 > 1:
                a, b2 = 1 - a, 1 - b2
            w = (1 - a - b2, a, b2)
            rl.append(tuple(p[0] * w[0] + p[1] * w[1] + p[2] * w[2]))
            nl.append(tuple((n[0] * w[0] + n[1] * w[1] + n[2] * w[2]).normalized()))
            ll.append(L * (0.5 + 0.4 * rng.random()))
            cl.append(tuple(np.power(sample(q[0] * w[0] + q[1] * w[1] + q[2] * w[2]), 2.2)) + (1.0,))
    roots, norms, lens = np.array(rl), np.array(nl), np.array(ll)[:, None]
    R = len(roots)
    curves = bpy.data.hair_curves.new("Fur")
    curves.add_curves([3] * R)
    rads = np.tile(np.array([0.0008, 0.0005, 0.00015]) * height, R)
    rattr = curves.attributes["radius"] if "radius" in curves.attributes else curves.attributes.new("radius", "FLOAT", "POINT")
    rattr.data.foreach_set("value", rads.astype(np.float32))
    curves.attributes.new("fur_color", "FLOAT_COLOR", "CURVE").data.foreach_set("color", [c for col in cl for c in col])
    fur_obj = bpy.data.objects.new("Fur", curves)
    hmat = bpy.data.materials.new("FurHair")
    hmat.use_nodes = True
    hnt = hmat.node_tree
    hb = hnt.nodes["Principled BSDF"]
    hb.inputs["Roughness"].default_value = 0.6
    ha = hnt.nodes.new("ShaderNodeAttribute"); ha.attribute_name = "fur_color"
    hi = hnt.nodes.new("ShaderNodeHairInfo")
    hnt.links.new(multiply(hnt, ha.outputs["Color"], map_range(hnt, hi.outputs["Intercept"], 0.6, 0.98)), hb.inputs["Base Color"])
    curves.materials.append(hmat)
scene.collection.objects.link(fur_obj)
print(f"MOTION {mode}: {R} roots")

# ---- the ground (a pastel checker so the run reads) ----
bpy.ops.mesh.primitive_plane_add(size=60 * height, location=(0, 0, zlo))
ground = bpy.context.active_object
gm = bpy.data.materials.new("Ground")
gm.use_nodes = True
gnt = gm.node_tree
chk = gnt.nodes.new("ShaderNodeTexChecker")
chk.inputs["Scale"].default_value = 60.0
chk.inputs["Color1"].default_value = (0.78, 0.86, 0.72, 1)
chk.inputs["Color2"].default_value = (0.66, 0.78, 0.62, 1)
gnt.links.new(chk.outputs["Color"], gnt.nodes["Principled BSDF"].inputs["Base Color"])
ground.data.materials.append(gm)


# ---- the path: facing -Y, so running is along -Y; heights in model units (the model is 1 tall = 1.8 m in game) ----
def path(t):
    run_v, acc = 1.6 * height, 1.6 * height  # about 2.9 m/s in game, reached in 1 s
    y, z = 0.0, 0.0
    if t > 2.0:
        ta = min(t - 2.0, 1.0)
        y -= 0.5 * acc * ta * ta
        if t > 3.0:
            y -= run_v * min(t - 3.0, 1.0)
        if t > 4.0:  # stop dead in 0.2 s
            td = min(t - 4.0, 0.2)
            y -= run_v * td - 0.5 * (run_v / 0.2) * td * td
    if 5.0 < t < 5.74:
        g = 5.4 * height
        tj = t - 5.0
        z = 2.0 * height * tj - 0.5 * g * tj * tj
    return np.array([0.0, y, max(z, 0.0)])


dt = 1.0 / FPS
SUB = 4
rngn = np.random.default_rng(3)
K = (2 * math.pi * 2.6) ** 2 * rngn.uniform(0.7, 1.3, (R, 1))  # each strand its own spring
C = 2 * 0.22 * np.sqrt(K)  # underdamped: overshoots and jiggles
s = np.zeros((R, 3))
sv = np.zeros((R, 3))
GRAV = np.array([0.0, 0.0, -0.35])
DRAG = 0.55 / height  # offset (in fur lengths) per unit of speed
INERTIA = 0.06 / height
k1, k2 = np.array([7.0, 3.0, 5.0]) / height, np.array([-4.0, 6.0, 2.0]) / height

cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
cam.data.lens = 50  # a side view from a little above, following the run (not the jump)
scene.collection.objects.link(cam)
scene.camera = cam
cx = (roots[:, 0].min() + roots[:, 0].max()) / 2


def wind(p, t):
    """Scrolling noise, blowing mostly from the front (+Y wind on a fox facing -Y), in gusts."""
    gust = 0.55 + 0.45 * math.sin(t * 1.3) * math.sin(t * 0.7 + 1.0)
    ph1 = p @ k1 - t * 6.0
    ph2 = p @ k2 - t * 4.3
    n = np.sin(ph1) + 0.6 * np.sin(ph2 * 1.7 + 0.5)
    m = np.cos(ph1 * 1.3 + 2.0) + 0.5 * np.sin(ph2)
    return gust * np.stack([0.18 * m, 0.42 + 0.22 * n, 0.12 * n], axis=1)


prev_v = np.zeros(3)
for f in range(frames):
    t = f / FPS
    for k in range(SUB):
        tt = t + k * dt / SUB
        h_ = dt / SUB
        v = (path(tt + h_) - path(tt - h_)) / (2 * h_)
        a = (path(tt + h_) - 2 * path(tt) + path(tt - h_)) / (h_ * h_)
        world_roots = roots + path(tt)
        target = GRAV + (-v) * DRAG + (-a) * INERTIA + wind(world_roots, tt)
        sv += (K * (target - s) - C * sv) * h_
        s += sv * h_
    mag = np.linalg.norm(s, axis=1, keepdims=True)
    d = s * np.minimum(1.0, 1.3 / np.maximum(mag, 1e-6))
    bend = np.minimum(np.linalg.norm(d, axis=1, keepdims=True), 1.0)
    if mode == "shells":
        pos = []
        for kk in range(1, layers + 1):
            h = kk / layers
            pos.append(roots + (norms * L * h * (1 - 0.3 * bend) + d * L * h * h) * wts)
        sm.vertices.foreach_set("co", np.concatenate(pos).ravel().astype(np.float32))
        sm.update()
    else:
        mid = roots + norms * lens * 0.45 * (1 - 0.2 * bend) + d * lens * 0.25
        tip = roots + norms * lens * 0.75 * (1 - 0.3 * bend) + d * lens * 0.9
        curves.position_data.foreach_set("vector", np.stack([roots, mid, tip], axis=1).ravel().astype(np.float32))
        curves.update_tag()
    off = path(t)
    body.location = fur_obj.location = Vector(off)
    if view == "close":  # head and shoulders from the front-side, following the jump too
        look = Vector((cx, off[1] - height * 0.02, zlo + off[2] + height * 0.7))
        cam.location = look + Vector((height * 1.0, -height * 0.55, height * 0.12))
    else:  # the whole fox from the side, following the run only
        look = Vector((cx, off[1], zlo + height * 0.55))
        cam.location = look + Vector((height * 2.3, 0, height * 0.5))
    cam.rotation_euler = (look - cam.location).to_track_quat("-Z", "Y").to_euler()
    scene.render.filepath = os.path.join(out_dir, f"{mode}_{view}_{f + 1:04d}.png")
    bpy.ops.render.render(write_still=True)
    if f % 30 == 0:
        print(f"MOTION frame {f + 1}/{frames}  speed {np.linalg.norm(v):.2f}  mean bend {float(bend.mean()):.2f}")
print("MOTION done")
