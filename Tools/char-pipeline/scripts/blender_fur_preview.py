"""Preview fur on a Tripo character in Blender (EEVEE): none, shells, or strands, rendered from a few views (round 95).

What the RRRandom fur briefing proposes for Unreal, tried on our own model so the page can show it:
- shells: the fur faces copied outward in N layers (vertex normal x length x h, drooping a little), each layer keeping
  only thin round strand cross-sections (3D Voronoi cells on the ROOT position, so strands stay put through the layers)
  that narrow towards the tip, root darker. Masked, like it would be in Unreal.
- strands: real hair curves (3 points each) grown from random points on the fur faces, coloured from the texture at the
  root, as an Unreal groom would be.
Fur faces = base colour at the face centre is orange/cream fur: R - B > 0.2 and G / R > 0.38 (keeps the red shorts and
the white shirt out); faces within `eye_r` of the eye whites (very bright, colourless texels above 60% of the height)
stay bare.

Usage: blender -b --factory-startup --python blender_fur_preview.py -- <fbx> <base color> <out prefix> <mode>
       [--layers 12] [--length 0.012] [--strands 120000] [--views body,head,head_back] [--px 768]
  Writes <out prefix>_<mode>_<view>.png.
"""
import math
import random
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:]
fbx, tex_path, prefix, mode = argv[:4]


def opt(name, default, cast=float):
    return cast(argv[argv.index(name) + 1]) if name in argv else default


layers = opt("--layers", 12, int)
length = opt("--length", 0.012)
n_strands = opt("--strands", 120000, int)
views = opt("--views", "body,head,head_back", str).split(",")
px = opt("--px", 768, int)

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
engines = {e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items}
scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in engines else "BLENDER_EEVEE"
scene.render.resolution_x = scene.render.resolution_y = px
scene.view_settings.view_transform = "Standard"
world = bpy.data.worlds.new("World")
world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.25, 0.25, 0.27, 1.0)
world.node_tree.nodes["Background"].inputs[1].default_value = 1.6
scene.world = world
sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
sun.data.energy = 2.0
sun.rotation_euler = (math.radians(50), 0, math.radians(-30))
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


mat, nt, t, bsdf = tex_material("Body")
nt.links.new(t.outputs["Color"], bsdf.inputs["Base Color"])
body.data.materials.clear()
body.data.materials.append(mat)

# Which faces are fur
me = body.data
bm = bmesh.new()
bm.from_mesh(me)
bm.faces.ensure_lookup_table()
bm.verts.ensure_lookup_table()
uv_layer = bm.loops.layers.uv.active
zs = [v.co.z for v in bm.verts]
zlo, zhi = min(zs), max(zs)
height = zhi - zlo
face_col = []
for f in bm.faces:
    uv = sum((l[uv_layer].uv for l in f.loops), Vector((0, 0))) / len(f.loops)
    face_col.append(sample(uv))
face_col = np.array(face_col)
r, g, b = face_col[:, 0], face_col[:, 1], face_col[:, 2]
fur = (r - b > 0.2) & (g / np.maximum(r, 1e-3) > 0.38)
centres = np.array([tuple(f.calc_center_median()) for f in bm.faces])
eye_white = (face_col.min(1) > 0.85) & (face_col.max(1) - face_col.min(1) < 0.08) & (centres[:, 2] > zlo + 0.6 * height)
eye_r = 0.035 * height
if eye_white.any():
    eyes = centres[eye_white]
    near_eye = np.zeros(len(centres), bool)
    for e in eyes[:: max(1, len(eyes) // 400)]:
        near_eye |= np.linalg.norm(centres - e, axis=1) < eye_r
else:
    near_eye = np.zeros(len(centres), bool)
# The leather belt and pouches are brown like fur: no fur in the waist band on the front half (the tail is behind)
cy_mid = (centres[:, 1].min() + centres[:, 1].max()) / 2
belt = (centres[:, 2] > zlo + 0.15 * height) & (centres[:, 2] < zlo + 0.40 * height) & (centres[:, 1] < cy_mid + 0.05 * height)
near_eye |= belt
# Fill stray holes: a face whose neighbours are mostly fur is fur too (not near the eyes), twice
nbrs = [[e.link_faces for e in f.edges] for f in bm.faces]
nbrs = [[g.index for fs in n for g in fs if g.index != i] for i, n in enumerate(nbrs)]
for _ in range(2):
    vote = np.array([fur[n].mean() if n else 0.0 for n in nbrs])
    fur = (fur | (vote >= 0.6)) & ~near_eye
print(f"FUR {int(fur.sum())} of {len(fur)} faces are fur ({int(eye_white.sum())} eye-white faces)")
# Per vertex: the share of its faces that are fur, so the fur fades out at its edges instead of stopping in steps
vw = np.array([np.mean([fur[f.index] for f in v.link_faces]) if v.link_faces else 0.0 for v in bm.verts])

L = length * height
down = Vector((0, 0, -1))

if mode != "none":
    # Under fur the skin is the roots' shade: darken the body there, or the gaps between strands look like bright dots
    fa = me.attributes.new("fur", "FLOAT", "POINT")
    fa.data.foreach_set("value", vw.astype(np.float32))
    af = nt.nodes.new("ShaderNodeAttribute"); af.attribute_name = "fur"
    dim = nt.nodes.new("ShaderNodeMapRange")
    dim.inputs["To Min"].default_value = 1.0
    dim.inputs["To Max"].default_value = 0.72
    nt.links.new(af.outputs["Fac"], dim.inputs["Value"])
    dm = nt.nodes.new("ShaderNodeMix"); dm.data_type = "RGBA"; dm.blend_type = "MULTIPLY"
    dm.inputs["Factor"].default_value = 1.0
    nt.links.new(t.outputs["Color"], dm.inputs["A"])
    nt.links.new(dim.outputs["Result"], dm.inputs["B"])
    nt.links.new(dm.outputs["Result"], bsdf.inputs["Base Color"])

if mode == "shells":
    fur_faces = [f for f, k in zip(bm.faces, fur) if k]
    vidx = sorted({v.index for f in fur_faces for v in f.verts})
    remap = {vi: i for i, vi in enumerate(vidx)}
    base_co = [bm.verts[vi].co.copy() for vi in vidx]
    base_no = [bm.verts[vi].normal.copy() for vi in vidx]
    base_w = [float(vw[vi]) for vi in vidx]
    verts, faces, uvs, hs, roots = [], [], [], [], []
    for k in range(1, layers + 1):
        h = k / layers
        off = len(verts)
        for co, no, w in zip(base_co, base_no, base_w):
            verts.append(co + (no * L * h + down * L * 0.35 * h * h) * w)
            hs.append(h)
            roots.append(co)
        for f in fur_faces:
            faces.append([off + remap[v.index] for v in f.verts])
            uvs.extend(tuple(l[uv_layer].uv) for l in f.loops)
    sm = bpy.data.meshes.new("Shells")
    sm.from_pydata([tuple(v) for v in verts], [], faces)
    uvl = sm.uv_layers.new(name="UVMap")
    uvl.data.foreach_set("uv", [c for uv in uvs for c in uv])
    a = sm.attributes.new("h", "FLOAT", "POINT")
    a.data.foreach_set("value", hs)
    a = sm.attributes.new("root", "FLOAT_VECTOR", "POINT")
    a.data.foreach_set("vector", [c for v in roots for c in v])
    shells = bpy.data.objects.new("Shells", sm)
    scene.collection.objects.link(shells)
    smat, snt, st, sbsdf = tex_material("Shell")
    smat.surface_render_method = "DITHERED"
    attr_h = snt.nodes.new("ShaderNodeAttribute"); attr_h.attribute_name = "h"
    attr_r = snt.nodes.new("ShaderNodeAttribute"); attr_r.attribute_name = "root"
    vor = snt.nodes.new("ShaderNodeTexVoronoi"); vor.voronoi_dimensions = "3D"
    vor.inputs["Scale"].default_value = 1.0 / (0.0022 * height)
    snt.links.new(attr_r.outputs["Vector"], vor.inputs["Vector"])
    # strand radius narrows to the tip; each strand gets its own length from the cell colour
    seed = snt.nodes.new("ShaderNodeSeparateColor")
    snt.links.new(vor.outputs["Color"], seed.inputs["Color"])
    lenf = snt.nodes.new("ShaderNodeMapRange")
    lenf.inputs["To Min"].default_value = 0.55
    snt.links.new(seed.outputs[0], lenf.inputs["Value"])
    rel = snt.nodes.new("ShaderNodeMath"); rel.operation = "DIVIDE"
    snt.links.new(attr_h.outputs["Fac"], rel.inputs[0]); snt.links.new(lenf.outputs["Result"], rel.inputs[1])
    rad = snt.nodes.new("ShaderNodeMath"); rad.operation = "SUBTRACT"; rad.inputs[0].default_value = 1.0
    snt.links.new(rel.outputs[0], rad.inputs[1])
    rad2 = snt.nodes.new("ShaderNodeMath"); rad2.operation = "MULTIPLY"; rad2.inputs[1].default_value = 0.48
    snt.links.new(rad.outputs[0], rad2.inputs[0])
    keep = snt.nodes.new("ShaderNodeMath"); keep.operation = "LESS_THAN"
    snt.links.new(vor.outputs["Distance"], keep.inputs[0]); snt.links.new(rad2.outputs[0], keep.inputs[1])
    snt.links.new(keep.outputs[0], sbsdf.inputs["Alpha"])
    # darker at the root (fur shadowing itself), a touch brighter at the tips
    shade = snt.nodes.new("ShaderNodeMapRange")
    shade.inputs["To Min"].default_value = 0.7
    shade.inputs["To Max"].default_value = 1.08
    snt.links.new(attr_h.outputs["Fac"], shade.inputs["Value"])
    mul = snt.nodes.new("ShaderNodeMix"); mul.data_type = "RGBA"; mul.blend_type = "MULTIPLY"
    mul.inputs["Factor"].default_value = 1.0
    snt.links.new(st.outputs["Color"], mul.inputs["A"])
    snt.links.new(shade.outputs["Result"], mul.inputs["B"])
    snt.links.new(mul.outputs["Result"], sbsdf.inputs["Base Color"])
    shells.data.materials.append(smat)
    print(f"FUR shells: {layers} layers, {len(faces)} faces")

elif mode == "strands":
    rng = random.Random(7)
    fur_faces = [f for f, k in zip(bm.faces, fur) if k]
    areas = np.array([f.calc_area() for f in fur_faces])
    counts = np.random.default_rng(7).multinomial(n_strands, areas / areas.sum())
    pts, cols = [], []
    for f, c in zip(fur_faces, counts):
        if not c:
            continue
        loops = f.loops[:3]
        p = [l.vert.co for l in loops]
        n = [l.vert.normal for l in loops]
        q = [l[uv_layer].uv for l in loops]
        for _ in range(int(c)):
            a, b2 = rng.random(), rng.random()
            if a + b2 > 1:
                a, b2 = 1 - a, 1 - b2
            w = (1 - a - b2, a, b2)
            root = p[0] * w[0] + p[1] * w[1] + p[2] * w[2]
            nrm = (n[0] * w[0] + n[1] * w[1] + n[2] * w[2]).normalized()
            uv = q[0] * w[0] + q[1] * w[1] + q[2] * w[2]
            ln = L * (0.5 + 0.4 * rng.random())
            mid = root + nrm * ln * 0.5 + down * ln * 0.2
            tip = root + nrm * ln * 0.7 + down * ln * 0.55
            pts.extend((root, mid, tip))
            cols.append(tuple(np.power(sample(uv), 2.2)) + (1.0,))  # the texel is sRGB; the attribute is linear
    count = len(cols)
    curves = bpy.data.hair_curves.new("Fur")
    curves.add_curves([3] * count)
    curves.position_data.foreach_set("vector", [c for v in pts for c in v])
    rads = []
    for _ in range(count):
        rads.extend((0.0008 * height, 0.0005 * height, 0.00015 * height))
    curves.attributes["radius"].data.foreach_set("value", rads) if "radius" in curves.attributes else \
        curves.attributes.new("radius", "FLOAT", "POINT").data.foreach_set("value", rads)
    ca = curves.attributes.new("fur_color", "FLOAT_COLOR", "CURVE")
    ca.data.foreach_set("color", [c for col in cols for c in col])
    hair = bpy.data.objects.new("Fur", curves)
    scene.collection.objects.link(hair)
    hmat = bpy.data.materials.new("FurHair")
    hmat.use_nodes = True
    hnt = hmat.node_tree
    hb = hnt.nodes["Principled BSDF"]
    hb.inputs["Roughness"].default_value = 0.6
    ha = hnt.nodes.new("ShaderNodeAttribute"); ha.attribute_name = "fur_color"
    hint = hnt.nodes.new("ShaderNodeHairInfo")
    shade = hnt.nodes.new("ShaderNodeMapRange")
    shade.inputs["To Min"].default_value = 0.6
    shade.inputs["To Max"].default_value = 0.98
    hnt.links.new(hint.outputs["Intercept"], shade.inputs["Value"])
    mul = hnt.nodes.new("ShaderNodeMix"); mul.data_type = "RGBA"; mul.blend_type = "MULTIPLY"
    mul.inputs["Factor"].default_value = 1.0
    hnt.links.new(ha.outputs["Color"], mul.inputs["A"])
    hnt.links.new(shade.outputs["Result"], mul.inputs["B"])
    hnt.links.new(mul.outputs["Result"], hb.inputs["Base Color"])
    curves.materials.append(hmat)
    print(f"FUR strands: {count}")

# Cameras
cx = (min(v.co.x for v in bm.verts) + max(v.co.x for v in bm.verts)) / 2
cy = (min(v.co.y for v in bm.verts) + max(v.co.y for v in bm.verts)) / 2
head_z = zlo + 0.78 * height
VIEWS = {
    "body": (Vector((-0.7, -1, 0.15)).normalized(), Vector((cx, cy, zlo + height * 0.5)), height * 1.08),
    "head": (Vector((0, -1, 0.1)).normalized(), Vector((cx, cy, head_z + height * 0.06)), height * 0.55),
    "head_back": (Vector((0, 1, 0.25)).normalized(), Vector((cx, cy, head_z)), height * 0.55),
    "cheek": (Vector((-1, -0.6, 0.05)).normalized(), Vector((cx - height * 0.12, cy - height * 0.05, head_z - height * 0.06)), height * 0.3),
    "tail": (Vector((0.6, 1, 0.2)).normalized(), Vector((cx, cy + height * 0.12, zlo + height * 0.16)), height * 0.42),
}
cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
cam.data.type = "ORTHO"
scene.collection.objects.link(cam)
scene.camera = cam
for v in views:
    d, target, scale = VIEWS[v]
    cam.location = target + d * height * 3
    cam.rotation_euler = (-d).to_track_quat("-Z", "Y").to_euler()
    cam.data.ortho_scale = scale
    scene.render.filepath = f"{prefix}_{mode}_{v}.png"
    bpy.ops.render.render(write_still=True)
    print(f"FUR wrote {scene.render.filepath}")
