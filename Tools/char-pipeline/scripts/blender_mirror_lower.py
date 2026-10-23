"""Rebuild one side of a Tripo character's lower body as the mirror image of the other side (run in Blender).

The rabbit A-3 Free Retry had a second cotton tail fused to her right hip under the hand. Cutting it out and capping
the hole (blender_remove_lump.py) left a rough patch the user did not accept. Her left hip is clean, so here everything
below a horizontal cut is made symmetric from the clean side: the mesh is split exactly at the cut height, the lower
part is symmetrized (bmesh), and the new lower half is sewn to the untouched upper half along the cut (each side's cut
points are inserted into the other side's edges, then welded, so there are no gaps or T-joints). The mirrored faces
keep their UVs, so the rebuilt side shows the clean side's texture, mirrored. The head, ears and any one-sided accessory
above the cut (the moon clip) stay as they were.

Usage: blender -b --factory-startup --python blender_mirror_lower.py -- <tripo fbx> <out fbx> <cut height> [keep +X|-X]
  <cut height> is in blender_tripo_to_obj.py's OBJ units (100 = top of the head; pick a height on the shirt above the
  hands and pouches, the rabbit used 31). keep: the clean side, +X = the character's left (default).
  Pieces under 50 vertices go (Tripo's crumbs make Mixamo fail) and their holes are filled.
  The FBX comes out 1 m tall like Tripo's, Z up, facing -Y; the base color is unchanged.
"""
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector
from mathutils.geometry import intersect_point_line

args = sys.argv[sys.argv.index("--") + 1:]
src, out, cut = args[0], args[1], float(args[2])
keep = args[3] if len(args) > 3 else "+X"

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
meshes = [o for o in bpy.data.objects if o.type == "MESH"]
for o in bpy.data.objects:
    o.select_set(o in meshes)
bpy.context.view_layer.objects.active = meshes[0]
bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
if len(meshes) > 1:
    bpy.ops.object.join()
ob = bpy.context.view_layer.objects.active
for o in [o for o in bpy.data.objects if o.type != "MESH"]:
    bpy.data.objects.remove(o)
me = ob.data

co = np.array([v.co[:] for v in me.vertices])
lo, hi = co.min(0), co.max(0)
feet = co[co[:, 2] < lo[2] + (hi[2] - lo[2]) * 0.06]
mid = (feet.min(0) + feet.max(0)) / 2
zc = lo[2] + (hi[2] - lo[2]) * cut / 100.0
# symmetrize works about the local origin: put the mirror plane at x = 0 for now
shift = Vector((mid[0], 0, 0))
me.transform(Matrix.Translation(-shift))
tol = (hi[2] - lo[2]) * 1e-4

bm = bmesh.new()
bm.from_mesh(me)

# drop crumbs, fill what they leave
seen = set(); pieces = []
for v in bm.verts:
    if v in seen:
        continue
    stack = [v]; seen.add(v); part = []
    while stack:
        a = stack.pop(); part.append(a)
        for e in a.link_edges:
            b = e.other_vert(a)
            if b not in seen:
                seen.add(b); stack.append(b)
    pieces.append(part)
small = [v for p in pieces if len(p) < 50 for v in p]
bmesh.ops.delete(bm, geom=small, context="VERTS")
holes = [e for e in bm.edges if e.is_boundary]
if holes:
    old = set(bm.faces)
    bmesh.ops.holes_fill(bm, edges=holes, sides=0)
    bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if f not in old])
print(f"MIRROR dropped {len(small)} crumb verts, filled {len(holes)} hole edges")

# split exactly at the cut and on the mirror plane, drop the lower quarter on the rebuilt side and put a mirrored copy
# of the clean side's lower quarter there (bmesh.ops.symmetrize was tried: it also tore faces above the cut)
side = -1.0 if keep == "+X" else 1.0          # the side that gets rebuilt
for co_, no_ in (((0, 0, zc), (0, 0, 1)), ((0, 0, 0), (1, 0, 0))):
    geom = list(bm.verts) + list(bm.edges) + list(bm.faces)
    bmesh.ops.bisect_plane(bm, geom=geom, dist=tol, plane_co=co_, plane_no=no_)
lower = lambda f: all(v.co.z <= zc + tol for v in f.verts)
gone = [f for f in bm.faces if lower(f) and all(v.co.x * side >= -tol for v in f.verts)]
clean = [f for f in bm.faces if lower(f) and all(v.co.x * side <= tol for v in f.verts)]
bmesh.ops.delete(bm, geom=gone, context="FACES")
dup = bmesh.ops.duplicate(bm, geom=clean)
new_v = [g for g in dup["geom"] if isinstance(g, bmesh.types.BMVert)]
new_f = [g for g in dup["geom"] if isinstance(g, bmesh.types.BMFace)]
for v in new_v:
    v.co.x = -v.co.x
bmesh.ops.reverse_faces(bm, faces=new_f, flip_multires=False)
# the copy meets the clean half on the mirror plane exactly
plane = [v for v in bm.verts if abs(v.co.x) < tol and v.co.z <= zc + tol]
bmesh.ops.remove_doubles(bm, verts=plane, dist=tol)
print(f"MIRROR replaced {len(gone)} faces below z={zc:.4f} with {len(new_f)} mirrored ones")
bd = [e for e in bm.edges if e.is_boundary]
print(f"MIRROR boundary edges now {len(bd)}, on the cut {sum(1 for e in bd if all(abs(v.co.z - zc) < tol * 5 for v in e.verts))}")
def seam_edges(above):
    out = []
    for e in bm.edges:
        if not e.is_boundary:
            continue
        a, b = e.verts
        if abs(a.co.z - zc) > tol * 5 or abs(b.co.z - zc) > tol * 5:
            continue
        if (a.co.x + b.co.x) * side <= 0:
            continue
        f = e.link_faces[0]
        if (f.calc_center_median().z > zc) == above:
            out.append(e)
    return out

reach = (hi[2] - lo[2]) * 0.01
weld = {}
for src_above in (False, True):              # first the lower points into the upper edges, then the other way
    pts = list({v for e in seam_edges(src_above) for v in e.verts})
    for v in pts:
        if v in weld or v.is_valid is False:
            continue
        best = None
        for e in seam_edges(not src_above):
            a, b = e.verts
            p, t = intersect_point_line(v.co, a.co, b.co)
            t = min(1.0, max(0.0, t)); p = a.co.lerp(b.co, t)
            d = (p - v.co).length
            if best is None or d < best[0]:
                best = (d, e, t)
        if best is None or best[0] > reach:
            continue
        d, e, t = best
        a, b = e.verts
        if (a.co - v.co).length < tol * 5:
            target = a
        elif (b.co - v.co).length < tol * 5:
            target = b
        else:
            ne, target = bmesh.utils.edge_split(e, a, t)
        if src_above:
            weld[target] = v                   # lower point -> upper point
        else:
            weld[v] = target
bmesh.ops.weld_verts(bm, targetmap={k: v for k, v in weld.items() if k.is_valid and v.is_valid and k is not v})
bmesh.ops.remove_doubles(bm, verts=[v for v in bm.verts if abs(v.co.z - zc) < tol * 5], dist=tol * 5)
left = [e for e in bm.edges if e.is_boundary]
print(f"MIRROR sewed {len(weld)} points; boundary edges left {len(left)}")
if left:
    old = set(bm.faces)
    bmesh.ops.holes_fill(bm, edges=left, sides=0)
    bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if f not in old])
    print(f"MIRROR filled {len([f for f in bm.faces if f not in old])} faces in what was left")
# Mixamo's auto-rigger fails ("Unknown error while generating motion") on a mesh with an edge shared by 3 faces or a
# zero-area face, which the welding and hole filling can leave: drop such faces and refill until the mesh is closed
for rnd in range(6):
    bmesh.ops.dissolve_degenerate(bm, dist=tol, edges=list(bm.edges))
    bad = {f for e in bm.edges if len(e.link_faces) > 2 for f in e.link_faces}
    bad |= {f for f in bm.faces if f.calc_area() < tol * tol}
    bad |= {f for v in bm.verts if not v.is_manifold and v.link_faces for f in v.link_faces if len(f.verts) > 3}
    if bad:
        bmesh.ops.delete(bm, geom=list(bad), context="FACES")
    loose = [v for v in bm.verts if not v.link_faces]
    bmesh.ops.delete(bm, geom=loose, context="VERTS")
    open_e = [e for e in bm.edges if e.is_boundary]
    if not bad and not open_e:
        break
    if open_e:
        # two holes touching at one vertex (a bowtie) defeat holes_fill: split such vertices into separate fans first
        for v in {v for e in open_e for v in e.verts}:
            if v.is_valid and sum(1 for e in v.link_edges if e.is_boundary) > 2:
                bmesh.utils.vert_separate(v, [e for e in v.link_edges if e.is_boundary])
        open_e = [e for e in bm.edges if e.is_boundary]
        old = set(bm.faces)
        bmesh.ops.holes_fill(bm, edges=open_e, sides=0)
        bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if f not in old])
        # a small twisted loop on the seam is left alone by holes_fill: walk it and fill it with a triangle fan
        todo = {e for e in bm.edges if e.is_boundary}
        while todo:
            e = todo.pop(); ring = [e.verts[0], e.verts[1]]
            while True:
                nxt = [g for g in ring[-1].link_edges if g in todo]
                if not nxt:
                    break
                todo.discard(nxt[0]); w = nxt[0].other_vert(ring[-1])
                if w is ring[0]:
                    break
                ring.append(w)
            if len(ring) >= 3:
                for a, b in zip(ring[1:-1], ring[2:]):
                    try:
                        bm.faces.new((ring[0], a, b))
                    except ValueError:
                        pass
    print(f"MIRROR cleanup {rnd}: dropped {len(bad)} bad faces, refilled {len(open_e)} open edges")
print("MIRROR open edges", [[tuple(round(c * 100, 2) for c in v.co) for v in e.verts] for e in bm.edges if e.is_boundary])
print("MIRROR check: edges not on exactly 2 faces", sum(1 for e in bm.edges if len(e.link_faces) != 2),
      "zero-area", sum(1 for f in bm.faces if f.calc_area() < tol * tol), "non-manifold verts", sum(1 for v in bm.verts if not v.is_manifold))
bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
bm.to_mesh(me)
bm.free()
me.transform(Matrix.Translation(shift))
for p in me.polygons:
    p.use_smooth = True
print(f"MIRROR now {len(me.vertices)} verts, {len(me.polygons)} faces")
bpy.ops.export_scene.fbx(filepath=out, use_selection=False, object_types={"MESH"}, path_mode="STRIP")
print(f"MIRROR wrote {out}")
