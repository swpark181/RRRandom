"""Pull thin spikes back into the surface in one zone of a Tripo mesh (run inside Blender).

Round 95: the Smart Mesh fox had a long thin shard behind the outer edge of each ear. A vertex is "spiky" when it sits far
from the average of its neighbours compared with its edge length (a tip with few faces round it). In the zone, every
pass moves each spiky vertex halfway to its neighbours' average; as a tip retracts, the next vertex down becomes the tip,
so a whole spike shrinks back while flat or gently curved surface around it doesn't move. UVs stay; positions only.

Usage: blender -b --factory-startup --python blender_shave_spikes.py -- <in fbx> <out fbx>
       [--zmin 0.7] [--side 0.2] [--passes 80] [--spiky 0.45] [--cx auto]
  In the FBX frame (Z up, 1 m tall, facing -Y): the zone is z > zmin and |x - cx| > side (the outer sides of the head,
  away from the middle line's hair tuft); cx is the middle of the mesh's x range at the zone's height unless given.
"""
import sys

import bmesh
import bpy

argv = sys.argv[sys.argv.index("--") + 1:]
src, dst = argv[0], argv[1]


def opt(name, default):
    return float(argv[argv.index(name) + 1]) if name in argv else default


zmin, side, passes, spiky = opt("--zmin", 0.7), opt("--side", 0.2), int(opt("--passes", 80)), opt("--spiky", 0.45)

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
obj = next(o for o in bpy.data.objects if o.type == "MESH")
bm = bmesh.new()
bm.from_mesh(obj.data)
bm.transform(obj.matrix_world)
bm.verts.ensure_lookup_table()
high = [v for v in bm.verts if v.co.z > zmin]
cx = opt("--cx", (min(v.co.x for v in high) + max(v.co.x for v in high)) / 2)
zone = [v for v in high if abs(v.co.x - cx) > side and v.link_edges]
print(f"SPIKES zone {len(zone)} verts (cx {cx:.3f})")
start = {v.index: v.co.copy() for v in zone}
for p in range(passes):
    moves = []
    for v in zone:
        nb = [e.other_vert(v).co for e in v.link_edges]
        mean = sum(nb, v.co * 0) / len(nb)
        edge = sum((n - v.co).length for n in nb) / len(nb)
        if edge > 0 and (v.co - mean).length / edge > spiky:
            moves.append((v, v.co.lerp(mean, 0.5)))
    for v, to in moves:
        v.co = to
    if p % 20 == 0 or not moves:
        print(f"SPIKES pass {p}: {len(moves)} moved")
    if not moves:
        break
moved = sorted(((v.co - start[v.index]).length, v.index) for v in zone)
print(f"SPIKES moved > 1 cm: {sum(1 for d, _ in moved if d > 0.01)}, largest {moved[-1][0]:.3f} m")
bm.transform(obj.matrix_world.inverted())
bm.to_mesh(obj.data)
obj.data.update()
bpy.ops.object.select_all(action="DESELECT")
obj.select_set(True)
bpy.ops.export_scene.fbx(filepath=dst, use_selection=True, object_types={"MESH"}, mesh_smooth_type="FACE",
                         path_mode="STRIP", bake_anim=False)
print(f"SPIKES wrote {dst}")
