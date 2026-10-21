"""Cut a part (a tail lying on the floor) out of a Mixamo OBJ by its centerline, so the auto-rigger sees the body alone
(run inside Blender, -b, or any Python 3 with numpy).

Usage: blender -b --factory-startup --python blender_strip_polyline.py -- <obj> <out obj> <joints> <radius> [<max z>]
  <joints>: the part's centerline from root to tip, "x,y,z;x,y,z;..." in the OBJ's own frame (blender_tripo_to_obj.py:
  Y up, facing +Z, 100 tall; read them off blender_obj_grid.py's plots). A vertex goes if it is within <radius> of the
  centerline and, with <max z>, behind it (z < max z): a tail runs behind the body, so the shorts and legs in front of
  its root are kept. Faces touching a cut vertex go too; texture coordinates, normals and material lines stay as written.
Then blender_merge_parts.py puts the whole mesh back on the body's Mixamo rig and blender_animal_tail.py gives the part
its bones. Unlike blender_strip_parts.py this needs no rig with tail bones first (round 72, the fluffy tiger).
"""
import sys

import numpy as np

args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
src, out = args[0], args[1]
J = np.array([[float(c) for c in p.split(",")] for p in args[2].split(";")])
radius = float(args[3])
max_z = float(args[4]) if len(args) > 4 else None

lines = open(src).read().splitlines()
V = np.array([[float(c) for c in l.split()[1:4]] for l in lines if l.startswith("v ")])


def dist_to_polyline(P):
    best = np.full(len(P), np.inf)
    for a, b in zip(J[:-1], J[1:]):
        ab = b - a
        t = np.clip(((P - a) @ ab) / max(ab @ ab, 1e-9), 0, 1)
        best = np.minimum(best, np.linalg.norm(P - (a + t[:, None] * ab), axis=1))
    return best


cut = dist_to_polyline(V) < radius
if max_z is not None:
    cut &= V[:, 2] < max_z
keep_index = np.cumsum(~cut)  # new 1-based index for kept vertices
print(f"STRIP cut {cut.sum()} of {len(V)} vertices")

res, vi, faces_cut = [], 0, 0
for l in lines:
    if l.startswith("v "):
        vi += 1
        if not cut[vi - 1]:
            res.append(l)
    elif l.startswith("f "):
        parts = l.split()[1:]
        idx = [int(p.split("/")[0]) for p in parts]
        if any(cut[i - 1] for i in idx):
            faces_cut += 1
            continue
        res.append("f " + " ".join(str(keep_index[i - 1]) + p[len(str(i)):] for p, i in zip(parts, idx)))
    else:
        res.append(l)
open(out, "w").write("\n".join(res) + "\n")
print(f"STRIP wrote {out}, {faces_cut} faces cut")
