"""Cut parts the Mixamo auto-rigger gets wrong (a tail, wings) out of a character's Mixamo OBJ by where they are, for a
cleaner body rig (run in Blender). blender_strip_parts.py does the same from a first rig's tail/wing weights; this needs
no first rig: the parts are told apart by position alone.

Usage: blender -b --factory-startup --python blender_strip_region.py -- <obj> <out obj> <rule> [preview png]
  The rule is True for a vertex to cut, in x, y, z: the OBJ imported as blender_strip_parts.py imports it (100 tall,
  Z up, the character facing -Y, so +Y is behind it and +X its left). Either a python expression, e.g.
  "(38 <= z <= 64 and y > 0.5) or y > 10", or a .py file defining cut(x, y, z) (the shirtless dragon's:
  input/dragon_v2/strip_rule.py).
The preview shows the cut red over the rest, from the front, behind and the side. The OBJ keeps blender_tripo_to_obj.py's
frame and material lines, so blender_merge_parts.py can put the whole mesh back on the body's rig. Loose crumbs the cut
leaves go and the holes it opens are closed over (faces only), so the auto-rigger gets one clean body.
"""
import math
import sys

import bpy
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
obj, out, rule = args[0], args[1], args[2]
preview = args[3] if len(args) > 3 else None
# Pieces left loose smaller than this go too
MIN_PIECE = 300

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.wm.obj_import(filepath=obj, forward_axis="NEGATIVE_Z", up_axis="Y")
mesh = next(o for o in bpy.data.objects if o.type == "MESH")
pts = [mesh.matrix_world @ v.co for v in mesh.data.vertices]
if rule.endswith(".py"):
    # A rule file: def cut(x, y, z) -> bool
    scope = {}
    exec(open(rule, encoding="utf-8").read(), scope)
    test = scope["cut"]
else:
    test = eval("lambda x, y, z: " + rule)
gone = {i for i, p in enumerate(pts) if test(p.x, p.y, p.z)}
print(f"STRIPREGION cutting {len(gone)} of {len(pts)} vertices")

if preview:
    # Red where it cuts, a vertex color shown flat
    layer = mesh.data.color_attributes.new(name="Cut", type="BYTE_COLOR", domain="POINT")
    for i in range(len(pts)):
        layer.data[i].color = (1.0, 0.05, 0.05, 1.0) if i in gone else (0.75, 0.75, 0.75, 1.0)
    mat = bpy.data.materials.new("cut")
    mat.use_nodes = True
    attr = mat.node_tree.nodes.new("ShaderNodeVertexColor")
    attr.layer_name = "Cut"
    mat.node_tree.links.new(attr.outputs["Color"], mat.node_tree.nodes["Principled BSDF"].inputs["Base Color"])
    mesh.data.materials.clear()
    mesh.data.materials.append(mat)
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = scene.render.resolution_y = 500
    sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
    sun.data.energy = 3.0
    scene.collection.objects.link(sun)
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    scene.collection.objects.link(cam)
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = 110
    scene.camera = cam
    for name, yaw in (("front", 0), ("back", 180), ("side", 90), ("back3q", 140)):
        r = math.radians(yaw)
        cam.location = Vector((math.sin(r) * 300, -math.cos(r) * 300, 50))
        cam.rotation_euler = (math.radians(90), 0, r)
        sun.rotation_euler = (math.radians(50), 0, r + math.radians(20))
        scene.render.filepath = f"{preview}_{name}.png"
        bpy.ops.render.render(write_still=True)
    mesh.data.materials.clear()

bpy.context.view_layer.objects.active = mesh
for o in bpy.data.objects:
    o.select_set(o == mesh)
bpy.ops.object.mode_set(mode="EDIT")
bpy.ops.mesh.select_mode(type="VERT")
bpy.ops.object.mode_set(mode="OBJECT")
for v in mesh.data.vertices:
    v.select = v.index in gone
bpy.ops.object.mode_set(mode="EDIT")
bpy.ops.mesh.delete(type="VERT")
# One clean body for the auto-rigger: crumbs the cut left loose go, and the holes it opened are closed over (with faces
# only, no new vertices, so the body's vertices stay a subset of the whole mesh's for blender_merge_parts.py)
bpy.ops.mesh.select_all(action="DESELECT")
bpy.ops.object.mode_set(mode="OBJECT")
import bmesh
bm = bmesh.new()
bm.from_mesh(mesh.data)
seen = set()
crumbs = []
for start in bm.verts:
    if start.index in seen:
        continue
    piece = [start]
    seen.add(start.index)
    for v in piece:
        for e in v.link_edges:
            other = e.other_vert(v)
            if other.index not in seen:
                seen.add(other.index)
                piece.append(other)
    if len(piece) < MIN_PIECE:
        crumbs += piece
bmesh.ops.delete(bm, geom=crumbs, context="VERTS")
holes = bmesh.ops.holes_fill(bm, edges=bm.edges[:], sides=0)
bm.to_mesh(mesh.data)
bm.free()
print(f"STRIPREGION dropped {len(crumbs)} loose vertices, closed holes with {len(holes['faces'])} faces")
bpy.ops.wm.obj_export(filepath=out, export_selected_objects=True, export_materials=False, export_uv=True,
                      export_normals=True, export_colors=False, forward_axis="NEGATIVE_Z", up_axis="Y")
with open(obj, encoding="utf-8") as f:
    head = [line for line in f if line.startswith(("mtllib", "usemtl"))]
with open(out, encoding="utf-8") as f:
    body = [line for line in f if not line.startswith(("mtllib", "usemtl"))]
with open(out, "w", encoding="utf-8") as f:
    f.writelines(head)
    f.writelines(body)
print(f"STRIPREGION wrote {out}: {len(mesh.data.vertices)} vertices left")
