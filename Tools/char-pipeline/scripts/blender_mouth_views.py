"""An expression's mouth redrawn for its own muzzle part, from four views (run inside Blender; 2026-11-23).

The user: "변경된 파츠에 맞게 표정 텍스쳐 다시 그려줘." The expression textures were painted from the front onto the default
muzzle; each expression now has a part whose mouth is sunk in (blender_mouth_part.py), and from the side the old
picture is smeared and faceted (blender_expression.py's bake weighs whole triangles). So the mouth is drawn again ON the
part: the head with that part is rendered unlit from four directions into one 2 x 2 sheet (front | from below; from its
right | from its left), an image editor repaints the mouth on the sheet (one picture, so the four views agree), and the
sheet is projected back onto the texture. The views are taken in an order: the first one paints everything it sees
well, each later one only what the earlier ones could not see (an editor draws the inside of a mouth with depth, so the
four views do not agree about where teeth and tongue are; mixed, they ghost).

render: blender -b --factory-startup --python blender_mouth_views.py -- render <body fbx> <part fbx> <texture>
            <out sheet png> <out json> --mouth x,z,rx,rz [--span 0.36] [--px 3072]
bake:   blender -b --factory-startup --python blender_mouth_views.py -- bake <body fbx> <part fbx> <texture>
            <sheet png> <edited sheet png> <json> <out texture> [--grow 1.45] [--feather 0.012] [--thr 0.06]
            [--order front,low,xneg,xpos] [--lo 0.22] [--hi 0.5]
            [--full] [--thick 0] [--also <texture in> <texture out>]... [--show <png>] [--check] [--noalign]
  --mouth   the same front ellipse as blender_mouth_part.py (x from the Head bone, z a height; rigs are 1 tall)
  --span    the side of every view's square, in body heights; the mouth is in its middle
  --grow    only texels inside the mouth ellipse grown this many times are repainted (soft edge: --feather)
  --thr     how much the edited sheet must differ from the original for a texel to change
  --order   which view paints first; a view paints a texel fully when the surface faces it by --hi (cosine) or more,
            not at all below --lo
  --also    the same repainting put into another texture (a blink face has its open-eyed face's mouth)
  --check   bake nothing: print how far the texture is from the ORIGINAL sheet where each view sees it (camera maths)
Prints "MOUTHVIEWS ..." lines.
"""
import json
import math
import os
import sys
import tempfile

import bpy
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

argv = sys.argv[sys.argv.index("--") + 1:]
mode = argv[0]


def opt(name, default, cast=float):
    return cast(argv[argv.index(name) + 1]) if name in argv else default


VIEWS = [  # name, yaw (deg, + = camera toward +X), elevation (deg, + = camera above), sheet column, sheet row from top
    ("front", 0.0, 0.0, 0, 0),
    ("low", 0.0, -38.0, 1, 0),
    ("xneg", -55.0, -12.0, 0, 1),
    ("xpos", 55.0, -12.0, 1, 1),
]


def load_meshes(body_fbx, part_fbx):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=body_fbx)
    bpy.ops.import_scene.fbx(filepath=part_fbx)
    arms = [o for o in bpy.data.objects if o.type == "ARMATURE"]
    for a in arms:
        a.animation_data_clear()
        a.data.pose_position = "REST"
    bpy.context.view_layer.update()
    arm = arms[0]
    hx = next((arm.matrix_world @ b.head_local).x for b in arm.data.bones if b.name.split(":")[-1] == "Head")
    meshes = [o for o in bpy.data.objects if o.type == "MESH"]
    return meshes, hx


def view_basis(yaw, elev):
    y, e = math.radians(yaw), math.radians(elev)
    d = Vector((math.sin(y) * math.cos(e), -math.cos(y) * math.cos(e), math.sin(e)))  # from the mouth toward the camera
    f = -d
    r = f.cross(Vector((0, 0, 1))).normalized()
    u = r.cross(f).normalized()
    return d, r, u


def mouth_point(meshes, hx, mx, mz, rx, rz):
    # the front-most surface point inside the ellipse: what every camera looks at
    best = None
    for o in meshes:
        mw = o.matrix_world
        for v in o.data.vertices:
            w = mw @ v.co
            if ((w.x - hx - mx) / rx) ** 2 + ((w.z - mz) / rz) ** 2 < 1.0 and (best is None or w.y < best):
                best = w.y
    return Vector((hx + mx, best, mz))


if mode == "render":
    body_fbx, part_fbx, tex_path, out_png, out_json = argv[1:6]
    mx, mz, rx, rz = (float(c) for c in opt("--mouth", "0,0.5,0.07,0.03", str).split(","))
    span = opt("--span", 0.36)
    px = opt("--px", 3072, int)
    half = px // 2
    meshes, hx = load_meshes(body_fbx, part_fbx)
    T = mouth_point(meshes, hx, mx, mz, rx, rz)
    img = bpy.data.images.load(tex_path)
    mat = bpy.data.materials.new("Flat")
    mat.use_nodes = True
    nt = mat.node_tree
    nt.nodes.clear()
    t = nt.nodes.new("ShaderNodeTexImage")
    t.image = img
    e = nt.nodes.new("ShaderNodeEmission")
    o = nt.nodes.new("ShaderNodeOutputMaterial")
    nt.links.new(t.outputs["Color"], e.inputs["Color"])
    nt.links.new(e.outputs["Emission"], o.inputs["Surface"])
    for m in meshes:
        m.data.materials.clear()
        m.data.materials.append(mat)
    scene = bpy.context.scene
    engines = [x.identifier for x in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items]
    scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in engines else "BLENDER_EEVEE"
    scene.render.resolution_x = scene.render.resolution_y = half
    scene.render.film_transparent = False
    scene.view_settings.view_transform = "Standard"
    world = bpy.data.worlds.new("W")
    world.use_nodes = False
    world.color = (0.86, 0.87, 0.89)
    scene.world = world
    cam_data = bpy.data.cameras.new("Cam")
    cam_data.type = "ORTHO"
    cam_data.ortho_scale = span
    cam_data.clip_start = 0.001
    cam_data.clip_end = 10.0
    cam = bpy.data.objects.new("Cam", cam_data)
    scene.collection.objects.link(cam)
    scene.camera = cam
    sheet = np.zeros((px, px, 4), dtype=np.float32)
    info = {"target": list(T), "span": span, "px": px, "mouth": [mx, mz, rx, rz], "hx": hx, "views": []}
    tmp = tempfile.mkdtemp()
    for name, yaw, elev, col, row in VIEWS:
        d, r, u = view_basis(yaw, elev)
        cam.location = T + d * 2.0
        cam.rotation_euler = (-d).to_track_quat("-Z", "Y").to_euler()
        scene.render.filepath = os.path.join(tmp, name + ".png")
        bpy.ops.render.render(write_still=True)
        im = bpy.data.images.load(scene.render.filepath)
        a = np.empty(half * half * 4, dtype=np.float32)
        im.pixels.foreach_get(a)
        sheet[(1 - row) * half:(2 - row) * half, col * half:(col + 1) * half] = a.reshape(half, half, 4)  # row 0 = bottom
        info["views"].append({"name": name, "d": list(d), "r": list(r), "u": list(u), "col": col, "row": row})
    out = bpy.data.images.new("sheet", px, px)
    out.pixels.foreach_set(sheet.ravel())
    out.filepath_raw = out_png
    out.file_format = "PNG"
    out.save()
    json.dump(info, open(out_json, "w"), indent=1)
    print(f"MOUTHVIEWS render {out_png}: {px}x{px}, span {span}, mouth at {tuple(round(c, 4) for c in T)}")

elif mode == "bake":
    body_fbx, part_fbx, tex_path, orig_png, edit_png, info_json, out_png = argv[1:8]
    grow = opt("--grow", 1.45)
    feather = opt("--feather", 0.012)
    thr = opt("--thr", 0.06)
    show = opt("--show", "", str)
    also = [(argv[i + 1], argv[i + 2]) for i, a in enumerate(argv) if a == "--also"]
    order = opt("--order", "front,low,xneg,xpos", str).split(",")
    lo, hi = opt("--lo", 0.22), opt("--hi", 0.5)
    thick = opt("--thick", 0.0)
    info = json.load(open(info_json))
    T = np.array(info["target"])
    span = info["span"]
    mx, mz, rx, rz = info["mouth"]
    meshes, hx = load_meshes(body_fbx, part_fbx)

    def load_rgb(path):
        im = bpy.data.images.load(path)
        w, h = im.size
        a = np.empty(w * h * im.channels, dtype=np.float32)
        im.pixels.foreach_get(a)
        return a.reshape(h, w, im.channels)[:, :, :3], w, h  # row 0 = bottom

    orig, ow, oh = load_rgb(orig_png)
    edit, ew, eh = load_rgb(edit_png)
    if (ew, eh) != (ow, oh):
        yy = np.clip((np.arange(oh) + 0.5) * eh / oh, 0, eh - 1).astype(int)
        xx = np.clip((np.arange(ow) + 0.5) * ew / ow, 0, ew - 1).astype(int)
        edit = edit[yy][:, xx]
    half = ow // 2

    def box(a, r):
        # box blur by running sums
        k = 2 * r + 1
        for ax in (0, 1):
            p = np.concatenate([np.repeat(np.take(a, [0], axis=ax), r + 1, axis=ax), a,
                                np.repeat(np.take(a, [-1], axis=ax), r, axis=ax)], axis=ax)
            c = np.cumsum(p, axis=ax)
            a = (np.take(c, np.arange(k, k + a.shape[ax]), axis=ax) - np.take(c, np.arange(0, a.shape[ax]), axis=ax)) / k
        return a

    # An editor sometimes hands the picture back moved by a few pixels. Per panel: the shift (within 4% of the panel)
    # that makes the edited band lie best on the original, found on a small copy; the panel is moved back by it.
    def shift_of(col, row):
        y0, x0 = (1 - row) * half + half // 4, col * half
        step = max(1, half // 256)
        a = orig[y0:y0 + half // 2:step, x0:x0 + half:step].mean(axis=2)
        b = edit[y0:y0 + half // 2:step, x0:x0 + half:step].mean(axis=2)
        m = int(a.shape[1] * 0.04)
        best, at = None, (0, 0)
        for dy in range(-m, m + 1):
            for dx in range(-m, m + 1):
                e = np.minimum(np.abs(a[m:-m, m:-m] - b[m + dy:b.shape[0] - m + dy, m + dx:b.shape[1] - m + dx]), 0.25).mean()
                if best is None or e < best:
                    best, at = e, (dy, dx)
        return at[0] * step, at[1] * step, best

    if "--noalign" not in argv:
        for vw in info["views"]:
            dy, dx, err = shift_of(vw["col"], vw["row"])
            print(f"MOUTHVIEWS align {vw['name']}: edited panel lies {dx:+d},{dy:+d} px off (of {half}), difference {err:.4f}")
            if dy or dx:
                y0, x0 = (1 - vw["row"]) * half, vw["col"] * half
                pan = edit[y0:y0 + half, x0:x0 + half]
                moved = np.roll(pan, (-dy, -dx), axis=(0, 1))
                band = slice(half // 4 + abs(dy), half - half // 4 - abs(dy))  # only the captured band is the edit
                pan[band] = moved[band]
    s = ow / 2048.0
    diff = np.abs(edit - orig).max(axis=2)
    changed = np.clip((box(diff, max(1, int(3 * s))) - thr) / thr, 0.0, 1.0)
    changed = np.clip(box(changed, max(2, int(9 * s))) * 2.5, 0.0, 1.0)
    if "--full" in argv:
        # the whole mouth region from the edited sheet, changed or not (faint leftovers such as the base face's mouth
        # corners differ too little to be told from capture noise)
        changed[:] = 1.0

    # every triangle near the mouth, in world space, with its UVs and corner normals
    P, UV, N = [], [], []
    allv, allt, alluv = [], [], []
    for o in meshes:
        me = o.data
        me.calc_loop_triangles()
        mw = np.array(o.matrix_world)
        rot = mw[:3, :3]
        co = np.array([v.co for v in me.vertices], dtype=np.float64)
        co = (np.c_[co, np.ones(len(co))] @ mw.T)[:, :3]
        uvl = me.uv_layers.active.data
        cn = me.corner_normals
        base = len(allv)
        allv.extend(map(tuple, co))
        for t in me.loop_triangles:
            vi = list(t.vertices)
            allt.append(tuple(base + i for i in vi))
            alluv.append([uvl[l].uv[:] for l in t.loops])
            p = co[vi]
            e2 = ((p[:, 0] - hx - mx) / (rx * (grow + 0.5))) ** 2 + ((p[:, 2] - mz) / (rz * (grow + 0.5) + 2 * feather)) ** 2
            if e2.min() > 1.0 or p[:, 1].min() > T[1] + 0.20:
                continue
            P.append(p)
            UV.append([uvl[l].uv[:] for l in t.loops])
            N.append([rot @ np.array(cn[l].vector) for l in t.loops])
    P, UV, N = np.array(P), np.array(UV), np.array(N)
    bvh = BVHTree.FromPolygons(allv, allt)
    tex_img = bpy.data.images.load(tex_path)
    tw, th = tex_img.size
    ta = np.empty(tw * th * tex_img.channels, dtype=np.float32)
    tex_img.pixels.foreach_get(ta)
    tex = ta.reshape(th, tw, tex_img.channels)[:, :, :3].copy()

    # texels of those triangles: position and smooth normal
    ty, tx, tp, tn = [], [], [], []
    for k in range(len(P)):
        u = UV[k, :, 0] * tw
        v = UV[k, :, 1] * th
        u0, u1 = int(max(0, math.floor(u.min()))), int(min(tw - 1, math.ceil(u.max())))
        v0, v1 = int(max(0, math.floor(v.min()))), int(min(th - 1, math.ceil(v.max())))
        d = (v[1] - v[2]) * (u[0] - u[2]) + (u[2] - u[1]) * (v[0] - v[2])
        if u1 < u0 or v1 < v0 or abs(d) < 1e-12:
            continue
        gu, gv = np.meshgrid(np.arange(u0, u1 + 1) + 0.5, np.arange(v0, v1 + 1) + 0.5)
        a = ((v[1] - v[2]) * (gu - u[2]) + (u[2] - u[1]) * (gv - v[2])) / d
        b = ((v[2] - v[0]) * (gu - u[2]) + (u[0] - u[2]) * (gv - v[2])) / d
        c = 1 - a - b
        ins = (a >= -0.03) & (b >= -0.03) & (c >= -0.03)
        if not ins.any():
            continue
        w3 = np.stack([a[ins], b[ins], c[ins]], axis=1)
        ty.append(gv[ins].astype(int))
        tx.append(gu[ins].astype(int))
        tp.append(w3 @ P[k])
        tn.append(w3 @ N[k])
    ty, tx = np.concatenate(ty), np.concatenate(tx)
    tp, tn = np.concatenate(tp), np.concatenate(tn)
    tn /= np.linalg.norm(tn, axis=1, keepdims=True) + 1e-12
    # (a texel on a shared edge is listed by both triangles: the later one wins, they agree)
    e2 = np.sqrt(((tp[:, 0] - hx - mx) / (rx * grow)) ** 2 + ((tp[:, 2] - mz) / (rz * grow)) ** 2)
    region = np.clip((1.0 - e2) * min(rx, rz) * grow / max(feather, 1e-6) + 1.0, 0.0, 1.0)  # 1 inside, fading out over ~feather
    region = region * region * (3 - 2 * region)
    keep = region > 0.001
    ty, tx, tp, tn, region = ty[keep], tx[keep], tp[keep], tn[keep], region[keep]
    n = len(ty)

    def sample(img, col, row, fx, fy):
        # bilinear, fx/fy 0..1 in the panel, fy from the panel's bottom
        x = np.clip(fx * half - 0.5, 0, half - 1.001) + col * half
        y = np.clip(fy * half - 0.5, 0, half - 1.001) + (1 - row) * half
        x0, y0 = x.astype(int), y.astype(int)
        ax, ay = (x - x0)[:, None], (y - y0)[:, None]
        if img.ndim == 2:
            ax, ay = ax[:, 0], ay[:, 0]
        return (img[y0, x0] * (1 - ax) * (1 - ay) + img[y0, x0 + 1] * ax * (1 - ay)
                + img[y0 + 1, x0] * (1 - ax) * ay + img[y0 + 1, x0 + 1] * ax * ay)

    acc = np.zeros((n, 3))
    accm = np.zeros(n)
    accq = np.zeros(n)
    left = np.ones(n)
    for vw in sorted([v for v in info["views"] if v["name"] in order], key=lambda v: order.index(v["name"])):
        d, r, u = np.array(vw["d"]), np.array(vw["r"]), np.array(vw["u"])
        rel = tp - T
        fx = rel @ r / span + 0.5
        fy = rel @ u / span + 0.5
        facing = tn @ d
        q = np.clip((facing - lo) / (hi - lo), 0.0, 1.0)
        q = q * q * (3 - 2 * q)
        q *= (fx > 0.01) & (fx < 0.99) & (fy > 0.01) & (fy < 0.99)
        dv = Vector(d)
        offs = [Vector(r) * (sx * thick * span / half) + Vector(u) * (sy * thick * span / half)
                for sx, sy in ((1, 0), (-1, 0), (0, 1), (0, -1))]
        for i in np.nonzero(q > 0)[0]:
            o = Vector(tp[i]) + Vector(tn[i]) * 4e-4 + dv * 4e-4
            if bvh.ray_cast(o, dv, 3.0)[0] is not None:
                q[i] = 0.0
            elif thick > 0:
                # a whisker in front of the surface is a few pixels wide in the captured picture and soft-edged: texels
                # whose line of sight passes within --thick pixels of anything take nothing from this view
                o2 = Vector(tp[i]) + dv * 0.004
                if any(bvh.ray_cast(o2 + off, dv, 3.0)[0] is not None for off in offs):
                    q[i] = 0.0
        if "--check" in argv:
            ok = q > 0.6
            err = np.abs(sample(orig, vw["col"], vw["row"], fx[ok], fy[ok]) - tex[ty[ok], tx[ok]]).mean() if ok.any() else -1
            print(f"MOUTHVIEWS check {vw['name']}: {int(ok.sum())} texels seen well, texture vs sheet mean difference {err:.4f}")
            continue
        w = left * q
        acc += sample(edit, vw["col"], vw["row"], fx, fy) * w[:, None]
        accm += sample(changed, vw["col"], vw["row"], fx, fy) * w
        accq += w
        left = left * (1 - q)
    if "--check" in argv:
        sys.exit(0)
    seen = accq > 1e-6
    colour = np.where(seen[:, None], acc / np.maximum(accq, 1e-6)[:, None], 0.0)
    alpha = region * np.where(seen, accm / np.maximum(accq, 1e-6), 0.0) * np.clip(accq, 0.0, 1.0)

    # The texels between the UV islands round the mouth: bilinear filtering reads them along every seam, and they still
    # hold whatever was there before the expressions were painted (the base face's mouth line shows as thin stray
    # lines). They are refilled from the island texels next to them, 6 texels deep.
    near = np.zeros((th, tw), dtype=bool)
    near[ty, tx] = True
    ys, xs = np.nonzero(near)
    by0, by1, bx0, bx1 = max(0, ys.min() - 8), min(th, ys.max() + 9), max(0, xs.min() - 8), min(tw, xs.max() + 9)
    near = box(near[by0:by1, bx0:bx1].astype(np.float32), 6) > 0
    covered = np.zeros(near.shape, dtype=bool)
    auv = np.array(alluv)
    au, av = auv[:, :, 0] * tw - bx0, auv[:, :, 1] * th - by0
    for k in np.nonzero((au.max(1) >= 0) & (au.min(1) < near.shape[1]) & (av.max(1) >= 0) & (av.min(1) < near.shape[0]))[0]:
        u, v = au[k], av[k]
        u0, u1 = int(max(0, math.floor(u.min()))), int(min(near.shape[1] - 1, math.ceil(u.max())))
        v0, v1 = int(max(0, math.floor(v.min()))), int(min(near.shape[0] - 1, math.ceil(v.max())))
        d = (v[1] - v[2]) * (u[0] - u[2]) + (u[2] - u[1]) * (v[0] - v[2])
        if u1 < u0 or v1 < v0 or abs(d) < 1e-12 or not near[v0:v1 + 1, u0:u1 + 1].any():
            continue
        gu, gv = np.meshgrid(np.arange(u0, u1 + 1) + 0.5, np.arange(v0, v1 + 1) + 0.5)
        a = ((v[1] - v[2]) * (gu - u[2]) + (u[2] - u[1]) * (gv - v[2])) / d
        b = ((v[2] - v[0]) * (gu - u[2]) + (u[0] - u[2]) * (gv - v[2])) / d
        covered[v0:v1 + 1, u0:u1 + 1] |= (a >= 0) & (b >= 0) & (1 - a - b >= 0)
    gutter = near & ~covered

    def pad(t):
        sub = t[by0:by1, bx0:bx1]
        known = covered.copy()
        todo = gutter.copy()
        for _ in range(6):
            acc_c = np.zeros_like(sub)
            acc_n = np.zeros(known.shape, dtype=np.float32)
            for dy, dx in ((0, 1), (0, -1), (1, 0), (-1, 0)):
                kn = np.roll(known, (dy, dx), axis=(0, 1))
                acc_c += np.roll(sub, (dy, dx), axis=(0, 1)) * kn[..., None]
                acc_n += kn
            fill = todo & (acc_n > 0)
            sub[fill] = acc_c[fill] / acc_n[fill][:, None]
            known |= fill
            todo &= ~fill
        return int(gutter.sum() - todo.sum())

    def apply(t):
        t[ty, tx] = t[ty, tx] * (1 - alpha[:, None]) + colour * alpha[:, None]
        pad(t)
        return t

    def save(t, path):
        out = bpy.data.images.new("out", tw, th)
        out.pixels.foreach_set(np.concatenate([t, np.ones((th, tw, 1), dtype=np.float32)], axis=2).ravel())
        out.filepath_raw = path
        out.file_format = "PNG"
        out.save()
        bpy.data.images.remove(out)

    save(apply(tex), out_png)
    print(f"MOUTHVIEWS bake {out_png}: {n} texels in the mouth region, {int((alpha > 0.5).sum())} repainted, "
          f"{int((accq[region > 0.5] < 0.5).sum())} of the region seen by no view")
    for src, dst in also:
        im = bpy.data.images.load(src)
        a = np.empty(tw * th * im.channels, dtype=np.float32)
        im.pixels.foreach_get(a)
        save(apply(a.reshape(th, tw, im.channels)[:, :, :3].copy()), dst)
        print(f"MOUTHVIEWS also {dst}")
    if show:
        vis = edit.copy()
        vis[:, :, 0] = np.maximum(vis[:, :, 0], changed * 0.9)
        vis[:, :, 1] *= 1 - changed * 0.5
        out = bpy.data.images.new("show", ow, oh)
        out.pixels.foreach_set(np.concatenate([vis, np.ones((oh, ow, 1), dtype=np.float32)], axis=2).ravel())
        out.filepath_raw = show
        out.file_format = "PNG"
        out.save()
