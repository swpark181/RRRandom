"""Bake one Tripo model's base color onto another model of the same character (run inside Blender, Cycles).

Round 95: the Smart Mesh fox has cleaner shapes but flatter fur than the HD fox; this paints the HD fox's texture onto
the Smart Mesh's own UVs. The source is scaled to the target's height and moved onto its middle and floor (both Tripo
FBX: Z up, facing -Y), then each target texel takes the colour found along its normal (selected to active).
Where the two shapes differ (the face's width), the colour lands a little off, so look at the eyes and edges.

Usage: blender -b --factory-startup --python blender_bake_from.py -- <target fbx> <source fbx> <source base color>
       <out png> [--px 4096] [--reach 0.03]
  reach = how far (in the 1 m tall frame) the ray looks out from and back into the target surface.
"""
import sys

import bpy
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:]
target_fbx, source_fbx, source_tex, out = argv[:4]
px = int(argv[argv.index("--px") + 1]) if "--px" in argv else 4096
reach = float(argv[argv.index("--reach") + 1]) if "--reach" in argv else 0.03

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.render.engine = "CYCLES"
scene.cycles.device = "CPU"
scene.cycles.samples = 1


def load(path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=path)
    meshes = [o for o in bpy.data.objects if o not in before and o.type == "MESH"]
    bpy.ops.object.select_all(action="DESELECT")
    for o in meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1:
        bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    return obj


def box(obj):
    pts = [obj.matrix_world @ Vector(c) for c in obj.bound_box]
    lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    return lo, hi


target = load(target_fbx)
source = load(source_fbx)
tlo, thi = box(target)
slo, shi = box(source)
k = (thi.z - tlo.z) / (shi.z - slo.z)
source.scale = (k, k, k)
bpy.context.view_layer.update()
slo, shi = box(source)
source.location += Vector(((tlo.x + thi.x - slo.x - shi.x) / 2, (tlo.y + thi.y - slo.y - shi.y) / 2, tlo.z - slo.z))
bpy.context.view_layer.update()
print(f"BAKE source scaled {k:.3f}; target {tuple(round(c, 3) for c in thi - tlo)}, source {tuple(round(c, 3) for c in box(source)[1] - box(source)[0])}")

# Source: its texture as emission, so the bake carries the colour without lighting
smat = bpy.data.materials.new("Source")
smat.use_nodes = True
nt = smat.node_tree
nt.nodes.clear()
tex = nt.nodes.new("ShaderNodeTexImage")
tex.image = bpy.data.images.load(source_tex)
emit = nt.nodes.new("ShaderNodeEmission")
outn = nt.nodes.new("ShaderNodeOutputMaterial")
nt.links.new(tex.outputs["Color"], emit.inputs["Color"])
nt.links.new(emit.outputs["Emission"], outn.inputs["Surface"])
source.data.materials.clear()
source.data.materials.append(smat)

# Target: a fresh image as the bake destination
img = bpy.data.images.new("baked", px, px)
tmat = bpy.data.materials.new("Target")
tmat.use_nodes = True
node = tmat.node_tree.nodes.new("ShaderNodeTexImage")
node.image = img
tmat.node_tree.nodes.active = node
target.data.materials.clear()
target.data.materials.append(tmat)

bpy.ops.object.select_all(action="DESELECT")
source.select_set(True)
target.select_set(True)
bpy.context.view_layer.objects.active = target
bpy.ops.object.bake(type="EMIT", use_selected_to_active=True, cage_extrusion=reach, max_ray_distance=reach * 2,
                    margin=16)
img.filepath_raw = out
img.file_format = "PNG"
img.save()
print(f"BAKE wrote {out}")
