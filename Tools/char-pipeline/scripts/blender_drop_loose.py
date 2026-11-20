"""Drop the small loose pieces of a mesh, and thin sliver faces (run inside Blender) - e.g. a Tripo tiger's whiskers,
which are loose thin triangles in front of the face, so its face can be rendered without them (2026-11-20).

Usage: blender -b --factory-startup --python blender_drop_loose.py -- <fbx in> <fbx out> [min verts 50] [sliver ratio 0]
  A piece with fewer than <min verts> vertices goes. With a sliver ratio > 0, faces whose shortest altitude is under
  ratio x their longest edge (needle-thin triangles) inside the head's front go too.
Exported with the armature and weights as they were (FBX, Blender's own axes like the other rig scripts).
"""
import sys

import bmesh
import bpy

args = sys.argv[sys.argv.index("--") + 1:]
src, out = args[0], args[1]
min_verts = int(args[2]) if len(args) > 2 else 50
sliver = float(args[3]) if len(args) > 3 else 0.0
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
dropped = 0
for o in [o for o in bpy.data.objects if o.type == "MESH"]:
    bm = bmesh.new()
    bm.from_mesh(o.data)
    bm.verts.ensure_lookup_table()
    seen, kill = set(), []
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
            kill += comp
    if sliver > 0:
        zs = [v.co.z for v in bm.verts]
        for f in bm.faces:
            if len(f.verts) != 3:
                continue
            a, b, c = (v.co for v in f.verts)
            longest = max((a - b).length, (b - c).length, (c - a).length)
            if longest > 0 and 2 * f.calc_area() / longest < sliver * longest:
                kill += [v for v in f.verts if len(v.link_faces) <= 3]
    kill = list({v.index: v for v in kill}.values())
    dropped += len(kill)
    bmesh.ops.delete(bm, geom=kill, context="VERTS")
    bm.to_mesh(o.data)
    bm.free()
print(f"DROPLOOSE dropped {dropped} vertices")
bpy.ops.export_scene.fbx(filepath=out, use_selection=False, add_leaf_bones=False, path_mode="COPY", embed_textures=False)
print("DROPLOOSE wrote", out)
