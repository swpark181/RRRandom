"""A face mask for texture expressions (2026-11-19, the cinematic; RRRandom/Docs/CinematicSequencePlan.md section 15).

The user: "눈 쪽이 평평해야 텍스쳐를 붙여도 자연스러울것 같다. 얼굴 모델링을 조금 수정해서 표정 텍스쳐가 잘 붙도록 해줘",
"캐릭터들 기본 입이 없어야 표정 텍스쳐가 잘 들어갈 것 같다". The Tripo heads are low-poly with sculpted eyeballs in sockets,
lid and brow ridges and a smile crease: a face painted from the front lay over bumps that didn't match it, and moving the
mesh's own vertices onto a smooth surface folded its lids and lips into holes. So the face gets its own smooth skin:
a fine grid over the front of the face, on a smooth surface fitted to the head (its broad round shape, a quadric, plus
what's left round the edge, filled in across), a hair in front of the body, its UVs the front picture itself, so an
expression is just a picture (blender_expression.py's front render with the face changed) shown on it as drawn, at the
picture's own resolution; its vertex colour R is its opacity, fading out at its edge (the material dithers it). Under
it, whatever of the body stood out (eyeballs, lids, the lips, the old mouth's crease) is pushed back behind it, so the
default mouth never shows. The nose stays the body's own ("keep" ellipses: the mask is clear there).
Positions are in the front render's pixels (x right, y down, 1024 across), ellipses {"c": [x, y], "r": [rx, ry], "fade"}:
  {"patch": [...], "keep": [...], "crop": [x0, y0, size]}   (crop: the square of the picture the mask's UVs span)

facepatch: the rig with the mask joined to its body as a second material ("Face", weights from the nearest body vertices)
  blender -b --factory-startup --python blender_face_flatten.py -- facepatch <rig.fbx> <cam.json> <regions.json> <out.fbx> [<check prefix>]
faceimage: an expression picture for the mask: the original front render where the picture didn't change and the picture
           where it did (so the mask's edge matches the body), cropped to the mask's square, 1024 px
  blender -b --factory-startup --python blender_face_flatten.py -- faceimage <front.png> <edited.png> <regions.json> <out.png>
furfade: shell fur (blender_fur_shells_skel.py's FBX) thinned to nothing inside the mask (its vertex colour G, the edge
         fade, which the fur material reads), fading back in toward the mask's edge
  blender -b --factory-startup --python blender_face_flatten.py -- furfade <fur.fbx> <cam.json> <regions.json> <out.fbx>
"""
import json
import math
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree
from mathutils.geometry import barycentric_transform
from mathutils.kdtree import KDTree

args = sys.argv[sys.argv.index("--") + 1:]
mode = args[0]


def ellipse_weight(px, py, e):
    """1 inside the ellipse, easing to 0 over its fade outside it (px, py arrays in picture pixels)."""
    cx, cy = e["c"]
    rx, ry = e["r"]
    fade = e.get("fade", 20)
    d = (np.sqrt(((px - cx) / rx) ** 2 + ((py - cy) / ry) ** 2) - 1.0) * min(rx, ry)
    t = np.clip(1.0 - d / max(fade, 1e-6), 0.0, 1.0)
    t = np.where(d <= 0, 1.0, t)
    return t * t * (3 - 2 * t)


def region_weight(px, py, regions, key="patch"):
    w = np.zeros(np.shape(px))
    for e in regions.get(key, []):
        w = np.maximum(w, ellipse_weight(px, py, e))
    keep = np.zeros(np.shape(px))
    for e in regions.get("keep", []):
        keep = np.maximum(keep, ellipse_weight(px, py, e))
    return w * (1 - keep)


def harmonic_fill(values, known, iters=4000):
    """Fills the unknown cells of a 2D array from the known ones (Jacobi steps of Laplace's equation, coarse to fine)."""
    h, w = values.shape
    if h > 32 and w > 32:
        hs, ws = (h + 1) // 2, (w + 1) // 2
        cv = np.zeros((hs, ws))
        ck = np.zeros((hs, ws), dtype=bool)
        sub_v, sub_k = values[::2, ::2], known[::2, ::2]
        cv[:sub_v.shape[0], :sub_v.shape[1]] = np.where(sub_k, sub_v, 0)
        ck[:sub_k.shape[0], :sub_k.shape[1]] = sub_k
        guess = np.repeat(np.repeat(harmonic_fill(cv, ck, iters), 2, 0), 2, 1)[:h, :w]
        iters = max(200, iters // 8)
    else:
        guess = np.full((h, w), values[known].mean() if known.any() else 0.0)
    v = np.where(known, values, guess)
    for _ in range(iters):
        p = np.pad(v, 1, mode="edge")
        v = np.where(known, values, (p[:-2, 1:-1] + p[2:, 1:-1] + p[1:-1, :-2] + p[1:-1, 2:]) * 0.25)
    return v


def blur(a, r):
    k = np.ones(2 * r + 1) / (2 * r + 1)
    a = np.apply_along_axis(lambda v: np.convolve(v, k, mode="same"), 0, a)
    return np.apply_along_axis(lambda v: np.convolve(v, k, mode="same"), 1, a)


def load_rig(path):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=path)
    arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
    return arm, [o for o in bpy.data.objects if o.type == "MESH"]


class Camera:
    def __init__(self, path):
        cam = json.load(open(path))
        self.fwd = np.array(cam["forward"])
        self.up = np.array(cam["up"])
        self.right = -np.cross(self.fwd, self.up)
        self.center = np.array(cam["center"])
        self.half = cam["ortho"] / 2.0
        self.res = cam["res"]
        self.size = cam["size"]

    def project(self, p):
        """World points -> picture pixels (x right, y down) and depth (smaller = nearer)."""
        rel = p - self.center
        x = ((rel @ self.right) / self.half * 0.5 + 0.5) * self.res
        y = (0.5 - (rel @ self.up) / self.half * 0.5) * self.res
        return x, y, -(rel @ self.fwd)

    def unproject(self, x, y, d):
        a = (x / self.res - 0.5) * 2 * self.half
        b = (0.5 - y / self.res) * 2 * self.half
        return self.center + np.outer(a, self.right) + np.outer(b, self.up) - np.outer(d, self.fwd)


def world_verts(obj):
    mw = np.array(obj.matrix_world)
    v = np.array([x.co[:] for x in obj.data.vertices], dtype=np.float64)
    return (np.c_[v, np.ones(len(v))] @ mw.T)[:, :3]


def zbuffer(cam, objs, grid):
    """The front-most depth at every cell of a grid x grid picture (inf where nothing)."""
    zb = np.full((grid, grid), np.inf)
    s = grid / cam.res
    for o in objs:
        me = o.data
        me.calc_loop_triangles()
        x, y, d = cam.project(world_verts(o))
        x, y = x * s, y * s
        tris = np.array([t.vertices[:] for t in me.loop_triangles])
        tx, ty, td = x[tris], y[tris], d[tris]
        inview = (tx.max(1) >= 0) & (tx.min(1) < grid) & (ty.max(1) >= 0) & (ty.min(1) < grid)
        for t in np.nonzero(inview)[0]:
            X, Y, Z = tx[t], ty[t], td[t]
            x0, x1 = int(max(0, math.floor(X.min()))), int(min(grid - 1, math.ceil(X.max())))
            y0, y1 = int(max(0, math.floor(Y.min()))), int(min(grid - 1, math.ceil(Y.max())))
            if x1 < x0 or y1 < y0:
                continue
            gx, gy = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
            den = (Y[1] - Y[2]) * (X[0] - X[2]) + (X[2] - X[1]) * (Y[0] - Y[2])
            if abs(den) < 1e-12:
                continue
            a = ((Y[1] - Y[2]) * (gx - X[2]) + (X[2] - X[1]) * (gy - Y[2])) / den
            b = ((Y[2] - Y[0]) * (gx - X[2]) + (X[0] - X[2]) * (gy - Y[2])) / den
            c = 1 - a - b
            inside = (a >= -1e-4) & (b >= -1e-4) & (c >= -1e-4)
            z = a * Z[0] + b * Z[1] + c * Z[2]
            sub = zb[y0:y1 + 1, x0:x1 + 1]
            sub[inside] = np.minimum(sub[inside], z[inside])
    return zb


def bilinear(grid_values, x, y, s):
    """Samples a grid (cells of 1/s picture pixels) at picture positions."""
    gx, gy = x * s - 0.5, y * s - 0.5
    h, w = grid_values.shape
    x0 = np.clip(np.floor(gx).astype(int), 0, w - 2)
    y0 = np.clip(np.floor(gy).astype(int), 0, h - 2)
    fx, fy = np.clip(gx - x0, 0, 1), np.clip(gy - y0, 0, 1)
    v00, v10 = grid_values[y0, x0], grid_values[y0, x0 + 1]
    v01, v11 = grid_values[y0 + 1, x0], grid_values[y0 + 1, x0 + 1]
    return (v00 * (1 - fx) + v10 * fx) * (1 - fy) + (v01 * (1 - fx) + v11 * fx) * fy


def nearest(grid_values, x, y, s):
    gx = np.clip((x * s).astype(int), 0, grid_values.shape[1] - 1)
    gy = np.clip((y * s).astype(int), 0, grid_values.shape[0] - 1)
    return grid_values[gy, gx]


def load_picture(path):
    """RGB, row 0 = top, resampled to 1024 x 1024."""
    im = bpy.data.images.load(path)
    w, h = im.size
    px = np.array(im.pixels[:], dtype=np.float32).reshape(h, w, im.channels)[:, :, :3][::-1]
    if (w, h) != (1024, 1024):
        yy = (np.arange(1024) * h / 1024).astype(int)
        xx = (np.arange(1024) * w / 1024).astype(int)
        px = px[yy][:, xx]
    return px


def save_picture(px, path):
    h, w = px.shape[:2]
    im = bpy.data.images.new("out", w, h)
    im.pixels = np.concatenate([px[::-1], np.ones((h, w, 1), dtype=np.float32)], axis=2).ravel().tolist()
    im.filepath_raw = path
    im.file_format = "PNG"
    im.save()


def shaded_checks(prefix, cam):
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "MATERIAL"
    scene.render.resolution_x = scene.render.resolution_y = 640
    cd = bpy.data.cameras.new("Check")
    cd.type = "ORTHO"
    cd.ortho_scale = cam.half * 2 * 0.62
    cd.clip_start = 0.001
    cd.clip_end = cam.size * 50
    c = bpy.data.objects.new("Check", cd)
    scene.collection.objects.link(c)
    scene.camera = c
    fwd = Vector(cam.fwd.tolist())
    head = Vector(cam.center.tolist()) + Vector((0, 0, -cam.size * 0.12))
    for name, ang, tilt in (("front", 0, 0), ("q34", 40, 0), ("side", 80, 0), ("below", 25, -25)):
        r, t = math.radians(ang), math.radians(tilt)
        d = Vector((fwd.x * math.cos(r) - fwd.y * math.sin(r), fwd.x * math.sin(r) + fwd.y * math.cos(r), 0)) * math.cos(t)
        d.z = math.sin(t)
        c.location = head + d * cam.size * 10
        c.rotation_euler = (-d).to_track_quat("-Z", "Y").to_euler()
        scene.render.filepath = f"{prefix}_{name}.png"
        bpy.ops.render.render(write_still=True)


if mode == "facepatch":
    rig, cam_json, reg_json, out_fbx = args[1:5]
    prefix = args[5] if len(args) > 5 else None
    arm, meshes = load_rig(rig)
    body = max(meshes, key=lambda o: len(o.data.vertices))
    cam = Camera(cam_json)
    regions = json.load(open(reg_json))
    GRID = 512
    s = GRID / cam.res
    zb = zbuffer(cam, meshes, GRID)
    gy, gx = np.mgrid[0:GRID, 0:GRID]
    W = region_weight((gx + 0.5) / s, (gy + 0.5) / s, regions)
    # The smooth surface: a quadric fitted to the front depth round the mask (the mask itself left out, and the sides
    # that turn away), plus what's left over round the mask's edge, filled in across it
    ys, xs = np.nonzero(W >= 0.01)
    y0, y1 = max(0, ys.min() - 4), min(GRID, ys.max() + 5)
    x0, x1 = max(0, xs.min() - 4), min(GRID, xs.max() + 5)
    my, mx = (y1 - y0) * 0.25, (x1 - x0) * 0.25
    fy0, fy1, fx0, fx1 = int(max(0, y0 - my)), int(min(GRID, y1 + my)), int(max(0, x0 - mx)), int(min(GRID, x1 + mx))
    sub_z = zb[fy0:fy1, fx0:fx1]
    fit = np.isfinite(sub_z) & (W[fy0:fy1, fx0:fx1] < 0.01)
    fit &= sub_z <= np.percentile(sub_z[fit], 70)
    qy, qx = np.mgrid[fy0:fy1, fx0:fx1]
    u, v = (qx - GRID / 2) / GRID, (qy - GRID / 2) / GRID
    A = np.stack([np.ones_like(u), u, v, u * u, u * v, v * v], axis=-1)
    coef, *_ = np.linalg.lstsq(A[fit], sub_z[fit], rcond=None)
    gu, gv = (gx - GRID / 2) / GRID, (gy - GRID / 2) / GRID
    Q = coef[0] + coef[1] * gu + coef[2] * gv + coef[3] * gu * gu + coef[4] * gu * gv + coef[5] * gv * gv
    resid = np.where(np.isfinite(zb), zb - Q, 0.0)
    known = (W < 0.01) & np.isfinite(zb)
    fill = resid.copy()
    fill[y0:y1, x0:x1] = harmonic_fill(resid[y0:y1, x0:x1], known[y0:y1, x0:x1])
    # Over the snout ("follow" ellipses) the mask keeps the snout's own round shape, only smoothed (the mouth's crease
    # goes): stretched flat from the cheeks to the nose's tip, its sides faced away from the light and went dark
    F = region_weight((gx + 0.5) / s, (gy + 0.5) / s, {"patch": regions.get("follow", [])})
    fin = np.isfinite(zb)
    num, den = np.where(fin, zb, 0.0), fin.astype(float)
    for _ in range(2):
        num, den = blur(num, 5), blur(den, 5)
    smooth_zb = np.where(den > 1e-6, num / np.maximum(den, 1e-6), 0.0)
    surface = np.where(fin, zb + W * ((1 - F) * (Q + fill) + F * smooth_zb - zb), np.inf)
    EPS = 0.0025 * cam.size          # the mask's height over the body's surface
    PUSH = 0.03 * cam.size           # how far behind the mask what stood out goes (room for posing)

    # The body: whatever is in front of the mask (and the face's own layers just behind its front) goes behind it
    P = world_verts(body)
    body.data.calc_loop_triangles()
    body_tris = [tuple(t.vertices) for t in body.data.loop_triangles]
    bvh = BVHTree.FromPolygons([Vector(p) for p in P], body_tris)    # (the body as it was, for the mask's weights)
    x, y, d = cam.project(P)
    wv = region_weight(x, y, regions)
    front = nearest(zb, x, y, s)
    gap = d - front
    surf_v = bilinear(np.where(np.isfinite(surface), surface, 0.0), x, y, s)
    target = surf_v + PUSH * wv
    move = (wv > 0.002) & (gap < 0.06 * cam.size) & np.isfinite(front) & (d < target)
    shift = np.where(move, target - d, 0.0)
    me = body.data
    old = np.array([ln.vector[:] for ln in me.corner_normals]) if me.has_custom_normals else None
    inv = np.array(body.matrix_world.inverted().to_3x3())
    co = np.array([vv.co[:] for vv in me.vertices]) + (-(shift[:, None] * cam.fwd[None, :])) @ inv.T
    me.vertices.foreach_set("co", co.ravel())
    me.update()
    if old is not None:
        loop_v = np.array([lp.vertex_index for lp in me.loops])
        me.normals_split_custom_set([(0, 0, 0)] * len(me.loops))
        new = np.array([ln.vector[:] for ln in me.corner_normals])
        wl = np.clip(shift[loop_v] / (0.004 * cam.size), 0.0, 1.0)[:, None]
        mix = old * (1 - wl) + new * wl
        mix /= np.linalg.norm(mix, axis=1, keepdims=True) + 1e-12
        me.normals_split_custom_set(mix.tolist())
    print(f"PATCH body: {int(move.sum())} vertices pushed behind the mask (up to {shift.max() / cam.size * 100:.1f}% of the head)")

    # The mask: a grid of 3 picture pixels over the mask's square, on the smooth surface, a hair in front
    cx0, cy0, csz = regions["crop"]
    STEP = 3.0
    n = int(csz / STEP) + 1
    px_ = cx0 + np.arange(n) * STEP
    py_ = cy0 + np.arange(n) * STEP
    PX, PY = np.meshgrid(px_, py_)
    PX, PY = PX.ravel(), PY.ravel()
    alpha = region_weight(PX, PY, regions)
    zfin = np.isfinite(nearest(surface, PX, PY, s))
    dep = bilinear(np.where(np.isfinite(surface), surface, 0.0), PX, PY, s) - EPS
    valid = zfin & (region_weight(PX, PY, {"patch": regions["patch"]}) > 0.002)
    idx = -np.ones(len(PX), dtype=int)
    idx[valid] = np.arange(valid.sum())
    pos = cam.unproject(PX[valid], PY[valid], dep[valid])
    faces = []
    grid_i = idx.reshape(n, n)
    grid_d = dep.reshape(n, n)
    for j in range(n - 1):
        for i in range(n - 1):
            q = (grid_i[j, i], grid_i[j, i + 1], grid_i[j + 1, i + 1], grid_i[j + 1, i])
            if min(q) < 0:
                continue
            dd = (grid_d[j, i], grid_d[j, i + 1], grid_d[j + 1, i + 1], grid_d[j + 1, i])
            # a cliff: no skin across it (only real ones, the head's edge; round the nose it climbs, clear there anyway)
            if max(dd) - min(dd) > 0.15 * cam.size:
                continue
            faces.append((q[0], q[3], q[2], q[1]))      # wound to face the camera
    pm = bpy.data.meshes.new("FaceMask")
    pm.from_pydata([tuple(p) for p in pos], [], faces)
    pm.update()
    uvl = pm.uv_layers.new(name="UVMap")
    uvx = (PX[valid] - cx0) / csz
    uvy = 1.0 - (PY[valid] - cy0) / csz
    loop_vi = np.array([lp.vertex_index for lp in pm.loops])
    uvl.data.foreach_set("uv", np.stack([uvx[loop_vi], uvy[loop_vi]], 1).ravel())
    col = pm.color_attributes.new("Col", "FLOAT_COLOR", "POINT")
    a = alpha[valid]
    col.data.foreach_set("color", np.stack([a, np.zeros_like(a), np.zeros_like(a), np.ones_like(a)], 1).ravel())
    mask_obj = bpy.data.objects.new("FaceMask", pm)
    bpy.context.scene.collection.objects.link(mask_obj)
    # Its weights: the body's own surface right behind each mask vertex (a ray from the front onto the body as it was,
    # its triangle's three vertices mixed by where it hit), so the mask bends exactly as the face under it does; the
    # three nearest vertices instead could be a jaw's or the inner mouth's, and the face then came through when posed
    kd = KDTree(len(P))
    for i, p in enumerate(P):
        kd.insert(Vector(p), i)
    kd.balance()
    groups = {g.index: g.name for g in body.vertex_groups}
    for g in body.vertex_groups:
        mask_obj.vertex_groups.new(name=g.name)
    bw = [{groups[e.group]: e.weight for e in vv.groups} for vv in me.vertices]
    fwd_v = Vector(cam.fwd.tolist())
    starts = cam.unproject(PX[valid], PY[valid], np.full(valid.sum(), -3.0 * cam.size))
    hits = 0
    for i, p in enumerate(pos):
        acc = {}
        loc, _, tri, _ = bvh.ray_cast(Vector(starts[i]), -fwd_v)
        if tri is not None:
            a, b, c = (Vector(P[k]) for k in body_tris[tri])
            bary = barycentric_transform(loc, a, b, c, Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1)))
            mix = [(body_tris[tri][k], max(0.0, bary[k])) for k in range(3)]
            hits += 1
        else:
            mix = [(j, 1.0 / max(dist, 1e-6)) for _, j, dist in kd.find_n(Vector(p), 3)]
        tot = sum(k for _, k in mix) or 1.0
        for j, k in mix:
            for gname, wt in bw[j].items():
                acc[gname] = acc.get(gname, 0.0) + wt * k / tot
        for gname, wt in acc.items():
            if wt > 1e-3:
                mask_obj.vertex_groups[gname].add([i], wt, "REPLACE")
    print(f"PATCH mask weights: {hits} of {len(pos)} from the surface behind")
    face_mat = bpy.data.materials.new("Face")
    pm.materials.append(face_mat)
    body_mat = body.data.materials[0] if body.data.materials else None
    print(f"PATCH mask: {len(pos)} vertices, {len(faces)} faces, body material {body_mat.name if body_mat else None}")
    # one mesh: the body (its own material first) and the mask
    bpy.ops.object.select_all(action="DESELECT")
    mask_obj.select_set(True)
    body.select_set(True)
    bpy.context.view_layer.objects.active = body
    bpy.ops.object.join()
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.export_scene.fbx(
        filepath=out_fbx, use_selection=False, object_types={"ARMATURE", "MESH"}, add_leaf_bones=False, bake_anim=False,
        primary_bone_axis="Y", secondary_bone_axis="X", use_armature_deform_only=False, mesh_smooth_type="OFF",
        colors_type="LINEAR", path_mode="STRIP")
    print(f"PATCH wrote {out_fbx}")
    if prefix:
        # checks: the body grey, the mask red where it shows
        for m in body.data.materials:
            if m.name == "Face":
                m.diffuse_color = (0.9, 0.35, 0.3, 1.0)
            else:
                m.diffuse_color = (0.75, 0.75, 0.75, 1.0)
        shaded_checks(prefix, cam)

elif mode == "faceimage":
    front_png, edit_png, reg_json, out_png = args[1:5]
    regions = json.load(open(reg_json))
    orig = load_picture(front_png)
    edit = load_picture(edit_png)
    diff = np.abs(edit - orig).max(axis=2)
    mask = np.clip((blur(diff, 2) - 0.05) / 0.10, 0.0, 1.0)
    mask = np.clip(blur(mask, 10) * 2.2, 0.0, 1.0)[..., None]
    out = orig * (1 - mask) + edit * mask
    cx0, cy0, csz = [int(v) for v in regions["crop"]]
    crop = out[cy0:cy0 + csz, cx0:cx0 + csz]
    # to 1024 (bilinear)
    t = (np.arange(1024) + 0.5) * csz / 1024 - 0.5
    i0 = np.clip(np.floor(t).astype(int), 0, csz - 2)
    f = np.clip(t - i0, 0, 1)
    rows = crop[i0] * (1 - f)[:, None, None] + crop[i0 + 1] * f[:, None, None]
    res = rows[:, i0] * (1 - f)[None, :, None] + rows[:, i0 + 1] * f[None, :, None]
    save_picture(res.astype(np.float32), out_png)
    print(f"FACEIMG wrote {out_png} (changed {float((mask > 0.5).mean()) * 100:.1f}% of the picture)")

elif mode == "furfade":
    fur, cam_json, reg_json, out_fbx = args[1:5]
    arm, meshes = load_rig(fur)
    cam = Camera(cam_json)
    regions = json.load(open(reg_json))
    for o in meshes:
        me = o.data
        x, y, d = cam.project(world_verts(o))
        # only the front half of the head (in front of the camera centre's plane; the tuft's upturned fur too), not the
        # back of the head behind the mask
        facing = np.clip(-d / (0.1 * cam.size), 0.0, 1.0)
        w = region_weight(x, y, {"patch": regions["patch"]})   # (the nose too: the fur round it covered the mouth)
        # the fur comes back over the mask's outer edge, where the mask fades into the body
        w = np.clip((w - 0.35) / 0.45, 0.0, 1.0) * facing
        attr = me.color_attributes.active_color or me.color_attributes[0]
        vals = np.array([c.color[:] for c in attr.data])
        if attr.domain == "POINT":
            vals[:, 1] *= 1 - w
        else:
            vals[:, 1] *= 1 - w[np.array([lp.vertex_index for lp in me.loops])]
        attr.data.foreach_set("color", vals.ravel())
        print(f"FURFADE {o.name}: {int((w > 0.5).sum())} of {len(w)} vertices bare ({attr.domain}, {attr.data_type})")
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.export_scene.fbx(
        filepath=out_fbx, use_selection=True, object_types={"ARMATURE", "MESH"}, add_leaf_bones=False, bake_anim=False,
        primary_bone_axis="Y", secondary_bone_axis="X", use_armature_deform_only=False, mesh_smooth_type="OFF",
        colors_type="LINEAR", path_mode="STRIP")
    print(f"FURFADE wrote {out_fbx}")
