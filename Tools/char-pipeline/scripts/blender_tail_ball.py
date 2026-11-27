"""Make a pom tail (a ball on the back, no tail bones) bigger and rounder by moving its own surface onto a sphere
(2026-11-25, the user: "토끼 꼬리는 더 동글동글 하고 풍성하게 다시 작업해줘"). Texture, UVs, weights and the rest of the body
are untouched; no Tripo.

Usage: blender -b -P blender_tail_ball.py -- <rig fbx> <out fbx> [--cut 0.17] [--radius 0.085] [--rim 0.03]
           [--centre x,y,z] [--check <png prefix>]

The tail = the surface joined to the backmost vertex (the model faces -Y, so its back is +Y) behind y = --cut. Its
sphere: --radius (body units: the model is 1 tall), centred on --centre or, by default, at the tail's middle height and
across, --radius * 0.55 behind the cut (so the ball sits on the body rather than inside it). Each tail vertex goes
along the line from the centre to it onto the sphere, by a share that grows from 0 at the cut to 1 at --rim behind it
(the join with the body stays where it was). Writes <out fbx> (blender_reskin.py's export settings) and, with --check,
side/back renders before and after.
"""
import sys

import bmesh
import bpy
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
rig, out = args[:2]


def opt(name, default):
    return type(default)(args[args.index(name) + 1]) if name in args else default


CUT = opt("--cut", 0.17)
RADIUS = opt("--radius", 0.085)
RIM = opt("--rim", 0.03)
CHECK = opt("--check", "")


def smoothstep(e0, e1, x):
    t = max(0.0, min(1.0, (x - e0) / (e1 - e0)))
    return t * t * (3.0 - 2.0 * t)


bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=rig)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
arm.data.pose_position = "REST"
m = max((o for o in bpy.data.objects if o.type == "MESH"), key=lambda o: len(o.data.vertices))
mw = m.matrix_world
co = [mw @ v.co for v in m.data.vertices]
seed = max(range(len(co)), key=lambda i: co[i].y)
bm = bmesh.new()
bm.from_mesh(m.data)
bm.verts.ensure_lookup_table()
tail = {seed}
todo = [seed]
while todo:
    i = todo.pop()
    for e in bm.verts[i].link_edges:
        j = e.other_vert(bm.verts[i]).index
        if j not in tail and co[j].y > CUT:
            tail.add(j)
            todo.append(j)
bm.free()
pts = [co[i] for i in tail]
lo = Vector([min(p[k] for p in pts) for k in range(3)])
hi = Vector([max(p[k] for p in pts) for k in range(3)])
if "--centre" in args:
    centre = Vector([float(c) for c in opt("--centre", "").split(",")])
else:
    centre = Vector(((lo.x + hi.x) / 2, CUT + RADIUS * 0.55, (lo.z + hi.z) / 2))
print(f"BALL tail {len(tail)} vertices, x {lo.x:.3f}..{hi.x:.3f}, y {lo.y:.3f}..{hi.y:.3f}, z {lo.z:.3f}..{hi.z:.3f}; "
      f"sphere at {tuple(round(c, 3) for c in centre)} radius {RADIUS}")


def render(tag):
    if not CHECK:
        return
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.render.resolution_x = scene.render.resolution_y = 500
    scene.world = scene.world or bpy.data.worlds.new("w")
    scene.world.color = (0.15, 0.15, 0.17)
    if "c" not in bpy.data.objects:
        cd = bpy.data.cameras.new("c")
        cd.type = "ORTHO"
        cd.ortho_scale = 0.45
        cam = bpy.data.objects.new("c", cd)
        scene.collection.objects.link(cam)
        scene.camera = cam
    cam = bpy.data.objects["c"]
    mid = Vector((centre.x, centre.y - 0.08, centre.z))
    for view, off in (("side", Vector((1, 0, 0))), ("back", Vector((0, 1, 0)))):
        cam.location = mid + off * 3.0
        cam.rotation_euler = (-off).to_track_quat("-Z", "Y").to_euler()
        scene.render.filepath = f"{CHECK}_{tag}_{view}.png"
        bpy.ops.render.render(write_still=True)


render("before")
inv = mw.inverted()
for i in tail:
    p = co[i]
    d = p - centre
    if d.length < 1e-6:
        continue
    target = centre + d.normalized() * RADIUS
    s = smoothstep(CUT, CUT + RIM, p.y)
    m.data.vertices[i].co = inv @ p.lerp(target, s)
# Relax (the old pointed tip folds into creases once on the sphere): each fully moved vertex goes to the middle of its
# neighbours and back onto the sphere, --relax times
RELAX = opt("--relax", 20)
bm = bmesh.new()
bm.from_mesh(m.data)
bm.verts.ensure_lookup_table()
nbrs = {i: [e.other_vert(bm.verts[i]).index for e in bm.verts[i].link_edges] for i in tail}
bm.free()
now = {i: mw @ m.data.vertices[i].co for i in tail}
free = [i for i in tail if smoothstep(CUT, CUT + RIM, co[i].y) > 0.99]
for _ in range(RELAX):
    nxt = {}
    for i in free:
        avg = sum((now.get(j, mw @ m.data.vertices[j].co) for j in nbrs[i]), Vector()) / max(len(nbrs[i]), 1)
        d = avg - centre
        nxt[i] = centre + d.normalized() * RADIUS if d.length > 1e-6 else now[i]
    now.update(nxt)
for i in free:
    m.data.vertices[i].co = inv @ now[i]
m.data.update()
render("after")

# --bone <share rim> (2026-11-25, the user: the rabbit's round tail "뛸 때 이상하게 변형"): the pom was weighted over Hips, Spine
# and Spine1 (Mixamo), so a running lean squashed it. A bone of its own, Tail1 under Hips, from the middle of where it
# meets the body to the ball's middle and on; the ball wholly on it past --bone metres behind the cut (nearer, shared with
# the body's weights as they were, so the join still follows the body). The game's tail chain then sways it as one piece.
if "--bone" in args:
    BRIM = opt("--bone", 0.02)
    prefix = next(b.name for b in arm.data.bones if b.name.endswith("Hips")).replace("Hips", "")
    name = prefix + "Tail1"
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="EDIT")
    eb = arm.data.edit_bones
    for b in [b for b in eb if b.name.startswith(prefix + "Tail")]:
        eb.remove(b)
    to_arm = arm.matrix_world.inverted()
    bone = eb.new(name)
    bone.head = to_arm @ Vector((centre.x, CUT, centre.z))
    bone.tail = to_arm @ (centre + Vector((0.0, RADIUS, 0.0)))
    bone.parent = eb[prefix + "Hips"]
    bone.align_roll(eb[prefix + "Hips"].z_axis)
    bpy.ops.object.mode_set(mode="OBJECT")
    for g in [g for g in m.vertex_groups if g.name.startswith(prefix + "Tail")]:
        m.vertex_groups.remove(g)
    tg = m.vertex_groups.new(name=name)
    groups = list(m.vertex_groups)
    full = 0
    for i in tail:
        s = smoothstep(CUT, CUT + BRIM, co[i].y)
        v = m.data.vertices[i]
        old = {g.group: g.weight for g in v.groups if g.weight > 0.0 and g.group != tg.index}
        total = sum(old.values()) or 1.0
        for g, w in old.items():
            if s >= 0.999:
                groups[g].remove([i])
            else:
                groups[g].add([i], w / total * (1.0 - s), "REPLACE")
        if s > 0.001:
            tg.add([i], s, "ADD" if False else "REPLACE")
        full += s >= 0.999
    print(f"BALL bone {name} under Hips: {full} of {len(tail)} tail vertices wholly on it")
for o in bpy.data.objects:
    if o.type == "CAMERA":
        bpy.data.objects.remove(o)
bpy.ops.export_scene.fbx(
    filepath=out, use_selection=False, object_types={"ARMATURE", "MESH"}, add_leaf_bones=False, bake_anim=False,
    primary_bone_axis="Y", secondary_bone_axis="X", use_armature_deform_only=False, mesh_smooth_type="OFF",
    path_mode="STRIP")
print(f"BALL wrote {out}")
