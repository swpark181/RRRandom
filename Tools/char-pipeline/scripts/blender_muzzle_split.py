"""Where a swappable muzzle would be cut from a head (2026-11-19, the Toontown way: eyes as textures, the muzzle a
separate piece of geometry that is swapped per expression). Preview only: nothing is exported.

The muzzle = the faces of a Tripo FBX (stands on +Z, faces -Y) inside a sphere placed from the nose tip. Renders the
head three-quarter and from the side: textured with the muzzle tinted, as clay with the muzzle coloured, and "exploded"
(the muzzle piece pulled forward, the head left with its opening), and prints the piece's size and its rim.

  blender -b --factory-startup --python blender_muzzle_split.py -- <fbx> <basecolor> <out prefix>
        [--r 0.11] [--back 0.075] [--down 0.012] [--pull 0.14] [--px 700] [--cz 0.64] [--span 0.6] [--zmin 0.44] [--zmax 1.0]
  --zmin / --zmax keep the piece above the neck and below the eye pictures (heights from the feet, in body heights).
  --r radius, --back / --down the sphere's centre behind / below the nose tip, --pull how far the piece is pulled,
  all in body heights.
"""
import math
import sys

import bmesh
import bpy
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:]
opt = {"r": 0.11, "back": 0.075, "down": 0.012, "pull": 0.14, "px": 700, "cz": 0.64, "span": 0.6, "zmin": 0.44, "zmax": 1.0}
rest = []
i = 0
while i < len(argv):
    if argv[i].startswith("--"):
        opt[argv[i][2:]] = float(argv[i + 1])
        i += 2
    else:
        rest.append(argv[i])
        i += 1
fbx, tex_path, prefix = rest[:3]
px = int(opt["px"])

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
bpy.ops.import_scene.fbx(filepath=fbx)
mesh = max((o for o in bpy.data.objects if o.type == "MESH"), key=lambda o: len(o.data.polygons))
bpy.context.view_layer.objects.active = mesh
mesh.select_set(True)
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
me = mesh.data
verts = [v.co.copy() for v in me.vertices]
lo = Vector([min(v[k] for v in verts) for k in range(3)])
hi = Vector([max(v[k] for v in verts) for k in range(3)])
H = hi.z - lo.z
head = [v for v in verts if v.z > lo.z + H * 0.55]
cx = (min(v.x for v in head) + max(v.x for v in head)) / 2
snout = [v for v in verts if abs(v.x - cx) < H * 0.04 and lo.z + H * 0.48 < v.z < lo.z + H * 0.64]
tip = min(snout, key=lambda v: v.y)
centre = Vector((cx, tip.y + H * opt["back"], tip.z - H * opt["down"]))
R = H * opt["r"]
print(f"MUZZLE nose tip {tuple(round(c, 3) for c in tip)}, sphere centre {tuple(round(c, 3) for c in centre)}, r {R:.3f}")

img = bpy.data.images.load(tex_path)


def emit_mat(name, tint):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    nt.nodes.clear()
    t = nt.nodes.new("ShaderNodeTexImage")
    t.image = img
    mix = nt.nodes.new("ShaderNodeMixRGB")
    mix.inputs[0].default_value = tint
    mix.inputs[2].default_value = (1.0, 0.33, 0.05, 1.0)
    e = nt.nodes.new("ShaderNodeEmission")
    o = nt.nodes.new("ShaderNodeOutputMaterial")
    nt.links.new(t.outputs["Color"], mix.inputs[1])
    nt.links.new(mix.outputs["Color"], e.inputs["Color"])
    nt.links.new(e.outputs["Emission"], o.inputs["Surface"])
    return m


body_mat = emit_mat("Body", 0.0)
body_mat.diffuse_color = (0.80, 0.80, 0.80, 1.0)
muz_mat = emit_mat("Muzzle", 0.42)
muz_mat.diffuse_color = (0.95, 0.42, 0.12, 1.0)
me.materials.clear()
me.materials.append(body_mat)
me.materials.append(muz_mat)
n_muz = 0
for p in me.polygons:
    inside = (p.center - centre).length < R and lo.z + H * opt["zmin"] < p.center.z < lo.z + H * opt["zmax"]
    p.material_index = 1 if inside else 0
    n_muz += inside
# the rim: edges between a muzzle face and a head face
bm = bmesh.new()
bm.from_mesh(me)
rim = [e for e in bm.edges if len(e.link_faces) == 2 and e.link_faces[0].material_index != e.link_faces[1].material_index]
rim_len = sum(e.calc_length() for e in rim)
mv = {v.index for f in bm.faces if f.material_index == 1 for v in f.verts}
print(f"MUZZLE piece {n_muz} faces of {len(me.polygons)}, {len(mv)} vertices, rim {len(rim)} edges, {rim_len / H * 100:.1f} % of the height round")
bm.free()

cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
scene.collection.objects.link(cam)
scene.camera = cam
cam.data.type = "ORTHO"
cam.data.ortho_scale = H * opt["span"]
scene.render.resolution_x = scene.render.resolution_y = px
scene.view_settings.view_transform = "Standard"
world = bpy.data.worlds.new("W")
world.color = (0.78, 0.79, 0.81)
world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.78, 0.79, 0.81, 1.0)
scene.world = world
face_v = [v for v in verts if abs(v.z - (lo.z + H * opt["cz"])) < H * 0.12]
cy = (min(v.y for v in face_v) + max(v.y for v in face_v)) / 2
target = Vector((cx, cy - H * 0.05, lo.z + H * opt["cz"]))
VIEWS = {"q34": Vector((1, -1, 0.05)).normalized(), "side": Vector((1, 0, 0)), "front": Vector((0, -1, 0))}
engines = {e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items}
EEVEE = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in engines else "BLENDER_EEVEE"


def render(tag):
    for mode in ("tint", "clay"):
        if mode == "tint":
            scene.render.engine = EEVEE
        else:
            scene.render.engine = "BLENDER_WORKBENCH"
            sh = scene.display.shading
            sh.light = "STUDIO"
            sh.color_type = "MATERIAL"
            sh.show_cavity = True
            sh.cavity_type = "BOTH"
            sh.show_shadows = False
        for name, d in VIEWS.items():
            cam.location = target + d * H * 4
            cam.rotation_euler = (-d).to_track_quat("-Z", "Y").to_euler()
            scene.render.filepath = f"{prefix}_{tag}_{mode}_{name}.png"
            bpy.ops.render.render(write_still=True)


render("joined")
# exploded: the muzzle faces become their own piece, pulled forward
bpy.ops.object.mode_set(mode="EDIT")
bm = bmesh.from_edit_mesh(me)
for f in bm.faces:
    f.select = f.material_index == 1
bmesh.update_edit_mesh(me)
bpy.ops.mesh.separate(type="SELECTED")
bpy.ops.object.mode_set(mode="OBJECT")
piece = next(o for o in bpy.data.objects if o.type == "MESH" and o is not mesh)
piece.location.y -= H * opt["pull"]
target.y -= H * opt["pull"] * 0.5
render("pulled")
print(f"MUZZLE wrote {prefix}_*.png")
