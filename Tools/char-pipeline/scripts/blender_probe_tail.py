"""Prints where a Mixamo fox's tail is (run in Blender): bone heads, and a side-view occupancy map of the mesh.

Usage: blender -b --factory-startup --python blender_probe_tail.py -- <fox.fbx>
"""
import sys
import bpy
from mathutils import Vector

fbx = sys.argv[sys.argv.index("--") + 1]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=fbx)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
meshes = [o for o in bpy.data.objects if o.type == "MESH"]
for b in arm.data.bones:
    if any(k in b.name for k in ("Hips", "Spine", "UpLeg", "Head", "ToeBase")) and "End" not in b.name:
        print("BONE", b.name, tuple(round(c, 3) for c in arm.matrix_world @ b.head_local))
pts = []
for m in meshes:
    print("MESH", m.name, len(m.data.vertices), "groups", len(m.vertex_groups))
    for v in m.data.vertices:
        pts.append(m.matrix_world @ v.co)
lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
print("BOUNDS", tuple(round(c, 3) for c in lo), tuple(round(c, 3) for c in hi))
# Side view: horizontal = Y, vertical = Z
W, H = 60, 40
grid = [[0] * W for _ in range(H)]
for p in pts:
    i = int((p.y - lo.y) / (hi.y - lo.y + 1e-9) * (W - 1))
    j = int((p.z - lo.z) / (hi.z - lo.z + 1e-9) * (H - 1))
    grid[H - 1 - j][i] += 1
print("SIDE (Y ->, Z up)")
for row in grid:
    print("".join(" " if c == 0 else ("." if c < 5 else ("o" if c < 30 else "#")) for c in row))
# Top view: horizontal = X, vertical = Y
grid = [[0] * W for _ in range(H)]
for p in pts:
    i = int((p.x - lo.x) / (hi.x - lo.x + 1e-9) * (W - 1))
    j = int((p.y - lo.y) / (hi.y - lo.y + 1e-9) * (H - 1))
    grid[H - 1 - j][i] += 1
print("TOP (X ->, Y up)")
for row in grid:
    print("".join(" " if c == 0 else ("." if c < 5 else ("o" if c < 30 else "#")) for c in row))
