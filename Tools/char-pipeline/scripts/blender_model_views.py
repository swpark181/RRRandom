"""Quick look at a model (Tripo FBX or a rig): its size, loose pieces, and textured pictures from the front, the animal
friend's right, the back and above (run in Blender; 2026-11-24, the fox generated again with a straight tail).

Usage: blender -b --factory-startup --python blender_model_views.py -- <fbx> <basecolor> <out prefix> [--px 700]
Writes <out prefix>_front|right|back|top.png and prints MODELVIEW lines (bounds per mesh, pieces by vertex count).
"""
import sys
from collections import defaultdict

import bpy
from mathutils import Matrix, Vector

args = sys.argv[sys.argv.index("--") + 1:]
src, tex, out = args[0], args[1], args[2]
PX = int(args[args.index("--px") + 1]) if "--px" in args else 700

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
meshes = [o for o in bpy.data.objects if o.type == "MESH"]
lo = Vector((1e9, 1e9, 1e9))
hi = Vector((-1e9, -1e9, -1e9))
for mesh in meshes:
    co = [mesh.matrix_world @ v.co for v in mesh.data.vertices]
    mlo = Vector((min(p.x for p in co), min(p.y for p in co), min(p.z for p in co)))
    mhi = Vector((max(p.x for p in co), max(p.y for p in co), max(p.z for p in co)))
    lo = Vector((min(lo.x, mlo.x), min(lo.y, mlo.y), min(lo.z, mlo.z)))
    hi = Vector((max(hi.x, mhi.x), max(hi.y, mhi.y), max(hi.z, mhi.z)))
    near = defaultdict(list)
    for e in mesh.data.edges:
        a, b = e.vertices
        near[a].append(b)
        near[b].append(a)
    seen, pieces = set(), []
    for v in range(len(co)):
        if v in seen:
            continue
        todo, n = [v], 0
        seen.add(v)
        while todo:
            i = todo.pop()
            n += 1
            for j in near[i]:
                if j not in seen:
                    seen.add(j)
                    todo.append(j)
        pieces.append(n)
    print(f"MODELVIEW {mesh.name}: {len(co)} verts, {len(mesh.data.polygons)} faces, bounds {tuple(round(c, 3) for c in mlo)} .. {tuple(round(c, 3) for c in mhi)}, "
          f"pieces {sorted(pieces, reverse=True)[:8]}, uv {[u.name for u in mesh.data.uv_layers]}")
    image = bpy.data.images.load(tex)
    for mat in mesh.data.materials:
        mat.use_nodes = True
        node = mat.node_tree.nodes.new("ShaderNodeTexImage")
        node.image = image
        mat.node_tree.nodes.active = node
scene = bpy.context.scene
scene.render.engine = "BLENDER_WORKBENCH"
shading = scene.display.shading
shading.light = "STUDIO"
shading.color_type = "TEXTURE"
shading.show_object_outline = False
scene.render.resolution_x = scene.render.resolution_y = PX
scene.world = bpy.data.worlds.new("w")
scene.world.color = (0.82, 0.82, 0.84)
scene.view_settings.view_transform = "Standard"
cam_data = bpy.data.cameras.new("cam")
cam_data.type = "ORTHO"
cam_data.ortho_scale = max(hi.x - lo.x, hi.y - lo.y, hi.z - lo.z) * 1.08
cam = bpy.data.objects.new("cam", cam_data)
scene.collection.objects.link(cam)
scene.camera = cam
mid = (lo + hi) * 0.5
VIEWS = {"front": (Vector((0, -4, 0)), Vector((0, 0, 1))), "right": (Vector((-4, 0, 0)), Vector((0, 0, 1))),
         "back": (Vector((0, 4, 0)), Vector((0, 0, 1))), "top": (Vector((0, 0, 4)), Vector((0, -1, 0)))}
for name, (offset, up) in VIEWS.items():
    forward = (-offset).normalized()
    right = forward.cross(up).normalized()
    m = Matrix((right, right.cross(forward), -forward)).transposed().to_4x4()
    m.translation = mid + offset
    cam.matrix_world = m
    scene.render.filepath = f"{out}_{name}.png"
    bpy.ops.render.render(write_still=True)
print(f"MODELVIEW wrote {out}_*.png")
