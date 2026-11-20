"""Texture expressions for the cinematic (2026-11-19, RRRandom/Docs/CinematicSequencePlan.md section 5: texture swaps).

render: an orthographic, unlit picture of the character's head from the front, in its rest pose, with its base color
        texture, plus the camera in a JSON, so an image editor can repaint the face on exactly that picture.
  blender -b --factory-startup --python blender_expression.py -- render <rig.fbx> <basecolor> <out.png> <out.json> [size]
bake:   the parts of an edited picture that differ from the original render (eyes, brows, mouth) projected back from the
        same camera onto the mesh and painted into a copy of its base color texture: only texels that face the camera and
        are the front-most surface there; everything else keeps the original texel.
  blender -b --factory-startup --python blender_expression.py -- bake <rig.fbx> <basecolor> <orig.png> <edited.png> <json> <out.png>
"""
import json
import math
import os
import sys

import bpy
import numpy as np
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
mode = args[0]


def load_rig(path):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=path, automatic_bone_orientation=False)
    arm = next((o for o in bpy.data.objects if o.type == "ARMATURE"), None)  # None: a plain Tripo FBX (bake only)
    meshes = [o for o in bpy.data.objects if o.type == "MESH"]
    body = max(meshes, key=lambda o: len(o.data.polygons))
    return arm, body


def bone_head(arm, name):
    for b in arm.data.bones:
        if b.name.split(":")[-1] == name:
            return arm.matrix_world @ b.head_local
    raise KeyError(name)


def head_camera(arm):
    head = bone_head(arm, "Head")
    top = bone_head(arm, "HeadTop_End")
    left = bone_head(arm, "LeftArm") - bone_head(arm, "RightArm")
    up = Vector((0, 0, 1))
    forward = left.normalized().cross(up).normalized()
    size = (top - head).length
    center = head + (top - head) * 0.45
    return center, forward, up, size


if mode == "render":
    rig, tex_path, out_png, out_json = args[1:5]
    res = int(args[5]) if len(args) > 5 else 1024
    arm, body = load_rig(rig)
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
    for m in [x for x in bpy.data.objects if x.type == "MESH"]:
        m.data.materials.clear()
        m.data.materials.append(mat)
    center, forward, up, size = head_camera(arm)
    ortho = size * 2.3
    cam_data = bpy.data.cameras.new("Cam")
    cam_data.type = "ORTHO"
    cam_data.ortho_scale = ortho
    cam_data.clip_start = 0.001
    cam_data.clip_end = size * 40
    cam = bpy.data.objects.new("Cam", cam_data)
    bpy.context.scene.collection.objects.link(cam)
    cam.location = center + forward * size * 10
    cam.rotation_euler = (-forward).to_track_quat("-Z", "Y").to_euler()
    scene = bpy.context.scene
    scene.camera = cam
    scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items] else "BLENDER_EEVEE"
    scene.render.resolution_x = scene.render.resolution_y = res
    scene.render.film_transparent = False
    scene.view_settings.view_transform = "Standard"
    world = bpy.data.worlds.new("W")
    world.color = (0.86, 0.87, 0.89)
    scene.world = world
    scene.render.filepath = out_png
    bpy.ops.render.render(write_still=True)
    with open(out_json, "w") as f:
        json.dump({"center": list(center), "forward": list(forward), "up": list(up), "size": size, "ortho": ortho,
                   "cam": list(cam.location), "res": res}, f, indent=1)
    print(f"EXPR render {out_png} head size {size:.3f}, ortho {ortho:.3f}, forward {tuple(round(v, 3) for v in forward)}")

elif mode == "bake":
    rig, tex_path, orig_png, edit_png, cam_json, out_png = args[1:7]
    arm, body = load_rig(rig)
    cam = json.load(open(cam_json))
    forward = np.array(cam["forward"])
    up = np.array(cam["up"])
    right = np.cross(forward, up)  # image +x as seen from the camera looking along -forward
    right = -right
    center = np.array(cam["center"])
    half = cam["ortho"] / 2.0
    res = cam["res"]

    def load_rgb(path):
        im = bpy.data.images.load(path)
        w, h = im.size
        px = np.array(im.pixels[:], dtype=np.float32).reshape(h, w, im.channels)[:, :, :3]
        return px, w, h  # row 0 = bottom

    orig, ow, oh = load_rgb(orig_png)
    edit, ew, eh = load_rgb(edit_png)
    if (ew, eh) != (ow, oh):
        # the editor may return another size: resample to the render's
        yy = (np.arange(oh) * eh / oh).astype(int)
        xx = (np.arange(ow) * ew / ow).astype(int)
        edit = edit[yy][:, xx]
    tex_img = bpy.data.images.load(tex_path)
    tw, th = tex_img.size
    tex = np.array(tex_img.pixels[:], dtype=np.float32).reshape(th, tw, tex_img.channels)[:, :, :3].copy()

    # What changed: colour difference, softened and grown a little so edges blend
    diff = np.abs(edit - orig).max(axis=2)

    def blur(a, r):
        k = np.ones(2 * r + 1) / (2 * r + 1)
        a = np.apply_along_axis(lambda v: np.convolve(v, k, mode="same"), 0, a)
        return np.apply_along_axis(lambda v: np.convolve(v, k, mode="same"), 1, a)

    mask = np.clip((blur(diff, 2) - 0.05) / 0.10, 0.0, 1.0)
    mask = np.clip(blur(mask, 10) * 2.2, 0.0, 1.0)

    deps = bpy.context.evaluated_depsgraph_get()
    ev = body.evaluated_get(deps)
    me = ev.to_mesh()
    me.calc_loop_triangles()
    mw = np.array(body.matrix_world)
    verts = np.array([v.co for v in me.vertices], dtype=np.float64)
    verts = (np.c_[verts, np.ones(len(verts))] @ mw.T)[:, :3]
    uv_layer = me.uv_layers.active.data
    tris = me.loop_triangles
    vidx = np.array([t.vertices[:] for t in tris])
    lidx = np.array([t.loops[:] for t in tris])
    uvs = np.array([[uv_layer[l].uv[:] for l in ls] for ls in lidx])
    P = verts[vidx]                                       # (n,3,3)
    rel = P - center
    ix = (rel @ right) / half * 0.5 + 0.5                 # 0..1 across the picture
    iy = (rel @ up) / half * 0.5 + 0.5
    depth = -(rel @ forward)                              # smaller = nearer the camera
    nrm = np.cross(P[:, 1] - P[:, 0], P[:, 2] - P[:, 0])
    nrm /= np.linalg.norm(nrm, axis=1, keepdims=True) + 1e-12
    facing = nrm @ forward
    near_head = (np.abs(ix - 0.5) < 0.55).all(1) & (np.abs(iy - 0.5) < 0.55).all(1)

    # Depth buffer of the picture, from every triangle in view
    zbuf = np.full((res, res), np.inf)
    for t in np.nonzero(near_head)[0]:
        x = ix[t] * res
        y = iy[t] * res
        x0, x1 = int(max(0, math.floor(x.min()))), int(min(res - 1, math.ceil(x.max())))
        y0, y1 = int(max(0, math.floor(y.min()))), int(min(res - 1, math.ceil(y.max())))
        if x1 < x0 or y1 < y0:
            continue
        gx, gy = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
        d = (y[1] - y[2]) * (x[0] - x[2]) + (x[2] - x[1]) * (y[0] - y[2])
        if abs(d) < 1e-12:
            continue
        a = ((y[1] - y[2]) * (gx - x[2]) + (x[2] - x[1]) * (gy - y[2])) / d
        b = ((y[2] - y[0]) * (gx - x[2]) + (x[0] - x[2]) * (gy - y[2])) / d
        c = 1 - a - b
        inside = (a >= -1e-4) & (b >= -1e-4) & (c >= -1e-4)
        z = a * depth[t, 0] + b * depth[t, 1] + c * depth[t, 2]
        sub = zbuf[y0:y1 + 1, x0:x1 + 1]
        sub[inside] = np.minimum(sub[inside], z[inside])

    # Paint: every texel of a camera-facing, front-most triangle takes the edited picture where it changed
    painted = 0
    eps = cam["size"] * 0.02
    for t in np.nonzero(near_head & (facing > 0.15))[0]:
        u = uvs[t, :, 0] * tw
        v = uvs[t, :, 1] * th
        u0, u1 = int(max(0, math.floor(u.min()))), int(min(tw - 1, math.ceil(u.max())))
        v0, v1 = int(max(0, math.floor(v.min()))), int(min(th - 1, math.ceil(v.max())))
        if u1 < u0 or v1 < v0:
            continue
        gu, gv = np.meshgrid(np.arange(u0, u1 + 1) + 0.5, np.arange(v0, v1 + 1) + 0.5)
        d = (v[1] - v[2]) * (u[0] - u[2]) + (u[2] - u[1]) * (v[0] - v[2])
        if abs(d) < 1e-12:
            continue
        a = ((v[1] - v[2]) * (gu - u[2]) + (u[2] - u[1]) * (gv - v[2])) / d
        b = ((v[2] - v[0]) * (gu - u[2]) + (u[0] - u[2]) * (gv - v[2])) / d
        c = 1 - a - b
        inside = (a >= -0.02) & (b >= -0.02) & (c >= -0.02)
        if not inside.any():
            continue
        px = (a * ix[t, 0] + b * ix[t, 1] + c * ix[t, 2]) * res
        py = (a * iy[t, 0] + b * iy[t, 1] + c * iy[t, 2]) * res
        z = a * depth[t, 0] + b * depth[t, 1] + c * depth[t, 2]
        pxi = np.clip(px.astype(int), 0, res - 1)
        pyi = np.clip(py.astype(int), 0, res - 1)
        vis = inside & (z <= zbuf[pyi, pxi] + eps)
        w = mask[pyi, pxi] * vis * min(1.0, (facing[t] - 0.15) / 0.25)
        if not (w > 0.001).any():
            continue
        region = tex[v0:v1 + 1, u0:u1 + 1]
        region[:] = region * (1 - w[..., None]) + edit[pyi, pxi] * w[..., None]
        painted += int((w > 0.5).sum())
    out = bpy.data.images.new("out", tw, th)
    rgba = np.concatenate([tex, np.ones((th, tw, 1), dtype=np.float32)], axis=2)
    out.pixels = rgba.ravel().tolist()
    out.filepath_raw = out_png
    out.file_format = "PNG"
    out.save()
    print(f"EXPR bake {out_png}: {painted} texels painted ({tw}x{th}), mask covers {(mask > 0.5).mean() * 100:.1f}% of the picture")
