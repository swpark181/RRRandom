"""Cut a part (a tail) out of an OBJ by growing over the mesh from its tip until it reaches the body (any Python 3 with
numpy, e.g. Blender's: python.exe -I obj_grow_cut.py <obj> <out obj> <seed x,y,z> <seed radius> "<body test>").

blender_strip_polyline.py cuts a tube around a centerline, which misses parts of a thick tail and bites into what lies
beside it (round 85, the A-pose dragon: its tail runs from under the belt down between the legs, touches the back of the
right leg and lies on the floor; the tube left a hollow shell of tail skin and took the backs of the shorts and a leg).
Here the part is everything joined to the seed (the vertices within <seed radius> of the tip) through vertices that are
NOT body; <body test> is a numpy expression of x, y, z (the OBJ's frame: Y up, +Z front, 100 tall), e.g.
"((y > 14) & (z > -5)) | ((y <= 14) & (z > -7) & (((x > -8) & (x < 0)) | ((x > 3) & (x < 10))))".
Faces touching a cut vertex go too; texture coordinates, normals and material lines stay as written.
"""
import sys

import numpy as np

src, out, seed, seed_r, body_expr = sys.argv[1:6]
lines = open(src).read().splitlines()
V = np.array([[float(c) for c in l.split()[1:4]] for l in lines if l.startswith("v ")])
faces = [[int(p.split("/")[0]) - 1 for p in l.split()[1:]] for l in lines if l.startswith("f ")]
x, y, z = V[:, 0], V[:, 1], V[:, 2]
body = eval(body_expr)

nbr = [[] for _ in range(len(V))]
for f in faces:
    for i in range(len(f)):
        a, b = f[i], f[(i + 1) % len(f)]
        nbr[a].append(b)
        nbr[b].append(a)

s = np.array([float(c) for c in seed.split(",")])
part = (np.linalg.norm(V - s, axis=1) < float(seed_r)) & ~body
stack = list(np.nonzero(part)[0])
while stack:
    v = stack.pop()
    for n in nbr[v]:
        if not part[n] and not body[n]:
            part[n] = True
            stack.append(n)
print(f"GROW seed {part.sum()} part vertices of {len(V)} (body test true for {body.sum()})")
P = V[part]
print(f"GROW part bounds {P.min(0).round(1)} .. {P.max(0).round(1)}")

keep_index = np.cumsum(~part)
res, vi, faces_cut = [], 0, 0
for l in lines:
    if l.startswith("v "):
        vi += 1
        if not part[vi - 1]:
            res.append(l)
    elif l.startswith("f "):
        ps = l.split()[1:]
        idx = [int(p.split("/")[0]) for p in ps]
        if any(part[i - 1] for i in idx):
            faces_cut += 1
            continue
        res.append("f " + " ".join(str(keep_index[i - 1]) + p[len(str(i)):] for p, i in zip(ps, idx)))
    else:
        res.append(l)
open(out, "w").write("\n".join(res) + "\n")
print(f"GROW wrote {out}, {faces_cut} faces cut")
