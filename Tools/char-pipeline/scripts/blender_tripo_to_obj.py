"""Turn a Tripo FBX (Blender preset) into the OBJ the Mixamo auto-rigger gets, scaled like the fox (run inside Blender).

Usage: blender -b --factory-startup --python blender_tripo_to_obj.py -- <tripo fbx> <out obj> <base color file name>

Newer Tripo FBX exports are 1 m tall and need their node transforms (fbx_to_obj.py reads raw geometry, which comes out
lying down for them). Here Blender applies the transforms; the mesh is then made 100 units tall (like the fox's OBJ,
so the Mixamo rig and its animations come out at the fox's scale), feet at 0, the feet centered, and written Y-up facing +Z
with one material whose MTL points at the base color texture next to it.
"""
import os
import sys

import bpy
from mathutils import Matrix, Vector

src, out, tex = sys.argv[sys.argv.index("--") + 1:][:3]
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
mesh = bpy.context.view_layer.objects.active
pts = [v.co for v in mesh.data.vertices]
lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
scale = 100.0 / (hi.z - lo.z)
# Centered on the feet (the bottom 6%), not the bounds: a tail off to one side would pull the body off Mixamo's
# symmetry line
feet = [p for p in pts if p.z < lo.z + (hi.z - lo.z) * 0.06]
mid_x = (min(p.x for p in feet) + max(p.x for p in feet)) / 2
mid_y = (min(p.y for p in feet) + max(p.y for p in feet)) / 2
mesh.data.transform(Matrix.Translation(Vector((-mid_x, -mid_y, -lo.z))))
mesh.data.transform(Matrix.Scale(scale, 4))
bpy.ops.wm.obj_export(filepath=out, export_selected_objects=True, export_materials=False, export_uv=True,
                      export_normals=True, forward_axis="NEGATIVE_Z", up_axis="Y")
base = os.path.splitext(os.path.basename(out))[0]
with open(out, "r", encoding="utf-8") as f:
    body = [line for line in f if not line.startswith(("mtllib", "usemtl"))]
with open(out, "w", encoding="utf-8") as f:
    f.write(f"mtllib {base}.mtl\nusemtl {base}\n")
    f.writelines(body)
with open(os.path.join(os.path.dirname(out), base + ".mtl"), "w", encoding="utf-8") as f:
    f.write(f"newmtl {base}\nKd 1 1 1\nmap_Kd {tex}\n")
print(f"TRIPO2OBJ {out}: {len(mesh.data.vertices)} verts, {len(mesh.data.polygons)} faces, was {hi.z - lo.z:.3f} tall, x{scale:.1f}")
