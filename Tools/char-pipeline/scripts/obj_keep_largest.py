"""Keep only the largest connected piece of an OBJ and drop the vertices no face uses (any Python 3 with numpy, e.g.
Blender's: python.exe -I obj_keep_largest.py <obj> <out obj> [<min vertices>]).
With <min vertices>, every piece of at least that many vertices stays (a Smart Mesh comes in several real pieces: the body,
the belt pouches, a hair tuft; only the crumbs go), and each piece's bounds are printed.

After blender_strip_polyline.py cuts a tail or wings away, bits of them that the centerline missed can stay behind as
small floating pieces; one 3-vertex piece was enough to make Mixamo fail with "Unknown error while generating motion"
(round 75, the C-3 red fox). Prints the size of every piece so the pieces that go can be checked. Texture coordinates,
normals and material lines stay as written.
"""
import sys

import numpy as np

src, out = sys.argv[1], sys.argv[2]
min_keep = int(sys.argv[3]) if len(sys.argv) > 3 else 0
lines = open(src).read().splitlines()
nv = sum(1 for l in lines if l.startswith("v "))
faces = [[int(p.split("/")[0]) - 1 for p in l.split()[1:]] for l in lines if l.startswith("f ")]

parent = np.arange(nv)


def find(a):
    while parent[a] != a:
        parent[a] = parent[parent[a]]
        a = parent[a]
    return a


for f in faces:
    r = find(f[0])
    for v in f[1:]:
        s = find(v)
        if s != r:
            parent[s] = r
used = np.zeros(nv, bool)
for f in faces:
    used[f] = True
roots = np.array([find(v) for v in range(nv)])
sizes = {r: int((roots[used] == r).sum()) for r in set(roots[used].tolist())}
big = max(sizes, key=sizes.get)
print("PIECES", sorted(sizes.values(), reverse=True)[:12], "of", len(sizes))
keep = used & (roots == big)
if min_keep:
    P = np.array([[float(c) for c in l.split()[1:4]] for l in lines if l.startswith("v ")])
    keep = np.zeros(nv, bool)
    for r, n in sorted(sizes.items(), key=lambda kv: -kv[1]):
        m = used & (roots == r)
        stay = n >= min_keep
        keep |= m if stay else False
        print(f"PIECE {n:6d} verts {'kept' if stay else 'DROPPED'} bounds {P[m].min(0).round(1)} .. {P[m].max(0).round(1)}")
new_index = np.cumsum(keep)

res, vi = [], 0
for l in lines:
    if l.startswith("v "):
        vi += 1
        if keep[vi - 1]:
            res.append(l)
    elif l.startswith("f "):
        parts = l.split()[1:]
        idx = [int(p.split("/")[0]) for p in parts]
        if not keep[idx[0] - 1]:
            continue
        res.append("f " + " ".join(str(new_index[i - 1]) + p[len(str(i)):] for p, i in zip(parts, idx)))
    else:
        res.append(l)
open(out, "w").write("\n".join(res) + "\n")
print(f"LARGEST kept {keep.sum()} of {nv} vertices -> {out}")
