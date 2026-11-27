import sys, bpy
from mathutils import Vector
src = sys.argv[sys.argv.index("--") + 1]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
mesh = next(o for o in bpy.data.objects if o.type == "MESH")
names = {g.index: g.name for g in mesh.vertex_groups}
tails = sorted((b for b in arm.data.bones if "Tail" in b.name), key=lambda b: int(b.name.split("Tail")[-1]))
J = [arm.matrix_world @ tails[0].head_local] + [arm.matrix_world @ b.tail_local for b in tails]
acc = [0.0]
for i in range(1, len(J)): acc.append(acc[-1] + (J[i] - J[i - 1]).length)
def on(p):
    best = (1e9, 0.0)
    for i in range(len(J) - 1):
        a, b = J[i], J[i + 1]
        t = max(0.0, min(1.0, (p - a).dot(b - a) / (b - a).length_squared))
        d = (p - a.lerp(b, t)).length
        if d < best[0]: best = (d, acc[i] + t * (acc[i + 1] - acc[i]))
    return best
rows = {}
far = 0.0
for v in mesh.data.vertices:
    w = sum(g.weight for g in v.groups if "Tail" in names[g.group])
    if w < 0.9: continue
    p = mesh.matrix_world @ v.co
    d, s = on(p)
    rows.setdefault(int(s / acc[-1] * 10), []).append(d)
    far = max(far, (p - J[0]).length)
print(f"TAILSIZE {src.split('/')[-1]}: bones {len(tails)}, line {acc[-1] * 100:.0f} cm, farthest tail vertex from the root {far * 100:.0f} cm")
print("TAILSIZE   radius by tenth of the length (max, cm): " + " ".join(f"{max(rows[k]) * 100:.1f}" for k in sorted(rows)))
