"""Smooth a chibi's head-to-body weights across the neck seam (run in Blender).

Mixamo's auto-rig puts a big-headed character's face on Head and the shirt collar just under it on Spine2, with almost
nothing on Neck and the change over a single row of faces where the chin's ruff meets the collar. Every head movement
then stretched those faces up to 18 times their length (the remade fox's face looked stretched: "얼굴 리깅 잘못됨" →
"얼굴이 찌그러지거나 늘어남", 2026-10-21; measured on Mixamo's own weights over the walk, running man and crying).

Usage: blender -b --factory-startup --python blender_head_weights.py -- <in fbx> <out fbx> [BELOW] [ABOVE] [ITER]
  The band from BELOW (default 0.04 m) under the Neck joint up to ABOVE (default 0.02 m) over the Head joint is
  re-weighted over the mesh: vertices above the band keep their weights (the head: Head only), vertices below keep theirs
  (the body), and the band's weights are relaxed ITER times (default 200) toward the average of their mesh neighbours, a
  harmonic blend along the surface, so the turn of the head spreads over the neck instead of one row of faces. Only the
  spine-to-head bones (and the shoulders, which reach the collar) take part; others stay.
  The rig and everything else stay as they are; written like blender_animal_tail.py writes.
"""
import sys

import bpy
import numpy as np

args = sys.argv[sys.argv.index("--") + 1:]
src, out = args[0], args[1]
BELOW = float(args[2]) if len(args) > 2 else 0.04
ABOVE = float(args[3]) if len(args) > 3 else 0.02
ITER = int(args[4]) if len(args) > 4 else 200
CHAIN = ["Spine1", "Spine2", "Neck", "Head", "LeftShoulder", "RightShoulder"]

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
arm.animation_data_clear()
for action in list(bpy.data.actions):
    bpy.data.actions.remove(action)
arm.data.pose_position = "REST"
mesh = next(o for o in bpy.data.objects if o.type == "MESH")
prefix = next(b.name for b in arm.data.bones if b.name.endswith("Hips")).replace("Hips", "")
head = arm.matrix_world @ arm.data.bones[prefix + "Head"].head_local
neck = arm.matrix_world @ arm.data.bones[prefix + "Neck"].head_local
# Mixamo can put the Neck joint above the Head joint, inside a big head (the red fox): the seam is under the lower one
lo_z, hi_z = min(neck.z, head.z) - BELOW, head.z + ABOVE

groups = [mesh.vertex_groups.get(prefix + n) or mesh.vertex_groups.new(name=prefix + n) for n in CHAIN]
gidx = {g.index: k for k, g in enumerate(groups)}
nv = len(mesh.data.vertices)
P = np.array([(mesh.matrix_world @ v.co)[:] for v in mesh.data.vertices])
W = np.zeros((nv, len(CHAIN)))
for v in mesh.data.vertices:
    for g in v.groups:
        if g.group in gidx:
            W[v.index, gidx[g.group]] = g.weight
share = W.sum(1)  # how much of each vertex this chain holds; kept as is
band = (P[:, 2] > lo_z) & (P[:, 2] < hi_z) & (share > 1e-4)
# The face above the band follows Head only
top = (P[:, 2] >= hi_z) & (share > 1e-4)
W[top] = 0.0
W[top, CHAIN.index("Head")] = share[top]

E = np.array([e.vertices[:] for e in mesh.data.edges])
nbr = [[] for _ in range(nv)]
for a, b in E:
    nbr[a].append(b)
    nbr[b].append(a)
idx = np.nonzero(band)[0]
N = [np.array(nbr[i]) for i in idx]
# Fractions (rows summing to 1) relax; each vertex's chain share is put back after
F = W / np.maximum(share[:, None], 1e-9)
for _ in range(ITER):
    F_new = F.copy()
    for k, i in enumerate(idx):
        if len(N[k]):
            nb = N[k][share[N[k]] > 1e-4]
            if len(nb):
                F_new[i] = 0.5 * F[i] + 0.5 * F[nb].mean(0)
    F = F_new
W[band] = F[band] * share[band, None]
for i in np.nonzero(band | top)[0]:
    for k, g in enumerate(groups):
        if W[i, k] > 0.002:
            g.add([int(i)], float(W[i, k]), "REPLACE")
        else:
            g.remove([int(i)])
print(f"HEADW {band.sum()} band vertices relaxed {ITER} times (z {lo_z:.3f}..{hi_z:.3f}), {top.sum()} above on Head only")
bpy.ops.export_scene.fbx(
    filepath=out, use_selection=False, object_types={"ARMATURE", "MESH"}, add_leaf_bones=False, bake_anim=False,
    primary_bone_axis="Y", secondary_bone_axis="X", use_armature_deform_only=False, mesh_smooth_type="OFF",
    path_mode="STRIP")
print(f"HEADW wrote {out}")
