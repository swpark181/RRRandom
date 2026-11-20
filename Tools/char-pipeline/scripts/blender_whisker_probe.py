"""List the whisker pieces of a Tripo animal friend (run inside Blender): loose small pieces and needle-thin faces on the
front of the head, in the face frame of blender_face_audit.py (centre of the head = 0, body height = 1), with their
average texture colour (2026-11-20, to copy the old tiger's whiskers onto the new one).

Usage: blender -b --factory-startup --python blender_whisker_probe.py -- <fbx> <basecolor> <out json> [min verts 50] [sliver 0.2]
"""
import json
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:]
fbx, tex, out = argv[:3]
min_verts = int(argv[3]) if len(argv) > 3 else 50
sliver = float(argv[4]) if len(argv) > 4 else 0.2
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=fbx)
mesh = max([o for o in bpy.data.objects if o.type == "MESH"], key=lambda o: len(o.data.polygons))
mw = mesh.matrix_world
bm = bmesh.new()
bm.from_mesh(mesh.data)
bm.transform(mw)
bm.verts.ensure_lookup_table()
bm.faces.ensure_lookup_table()
uvl = bm.loops.layers.uv.active
zs = [v.co.z for v in bm.verts]
lo, hi = min(zs), max(zs)
height = hi - lo
head_v = [v.co for v in bm.verts if v.co.z > lo + height * 0.55]
cx = (min(v.x for v in head_v) + max(v.x for v in head_v)) / 2
face_z = lo + height * 0.66
face_v = [v.co for v in bm.verts if abs(v.co.z - face_z) < height * 0.12]
cy = (min(v.y for v in face_v) + max(v.y for v in face_v)) / 2
centre = Vector((cx, cy, face_z))

img = bpy.data.images.load(tex)
W, H = img.size
pix = np.array(img.pixels[:], dtype=np.float32).reshape(H, W, 4)


def colour(faces):
    cs = []
    for f in faces:
        for l in f.loops:
            u, v = l[uvl].uv
            cs.append(pix[int(min(0.9999, max(0, v)) * H), int(min(0.9999, max(0, u)) * W), :3])
    return [round(float(c), 3) for c in np.median(np.array(cs), axis=0)] if cs else None


def norm(p):
    return [round(c, 4) for c in ((p - centre) / height)]


# loose pieces
seen, pieces = set(), []
for v in bm.verts:
    if v.index in seen:
        continue
    comp, stack = [], [v]
    seen.add(v.index)
    while stack:
        a = stack.pop()
        comp.append(a)
        for e in a.link_edges:
            b = e.other_vert(a)
            if b.index not in seen:
                seen.add(b.index)
                stack.append(b)
    if len(comp) < min_verts:
        faces = list({f for a in comp for f in a.link_faces})
        pieces.append({"kind": "loose", "verts": [norm(a.co) for a in comp],
                       "faces": [[comp.index(a) for a in f.verts] for f in faces], "colour": colour(faces)})
# needle faces on the head
needles = []
for f in bm.faces:
    if len(f.verts) != 3 or f.calc_center_median().z < lo + height * 0.5:
        continue
    a, b, c = (v.co for v in f.verts)
    longest = max((a - b).length, (b - c).length, (c - a).length)
    if longest > 0 and 2 * f.calc_area() / longest < sliver * longest:
        needles.append({"verts": [norm(v.co) for v in f.verts], "colour": colour([f]),
                        "attached": [len(v.link_faces) for v in f.verts]})
json.dump({"height": height, "centre": list(centre), "pieces": pieces, "needles": needles}, open(out, "w"))
print(f"PROBE {len(pieces)} loose pieces ({sum(len(p['verts']) for p in pieces)} verts), {len(needles)} needle faces -> {out}")
for p in pieces[:40]:
    vs = np.array(p["verts"])
    print("PROBE loose", len(vs), "min", vs.min(0).round(3).tolist(), "max", vs.max(0).round(3).tolist(), "col", p["colour"])
for n in needles[:60]:
    vs = np.array(n["verts"])
    print("PROBE needle min", vs.min(0).round(3).tolist(), "max", vs.max(0).round(3).tolist(), "col", n["colour"], n["attached"])
