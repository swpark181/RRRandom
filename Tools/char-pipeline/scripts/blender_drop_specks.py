"""Drop the small loose bits a Tripo model sometimes has floating off it, keeping the big parts.

Usage: blender -b -P blender_drop_specks.py -- <in.fbx> <out.fbx> [share]
Separates the mesh into loose parts and deletes every part with fewer than <share> (default 0.02) of the biggest part's
vertices, then joins the rest and writes the FBX (materials and embedded textures kept as they were). Logs what went.
Round 90: the blaster projectile's stray speck.
"""
import sys

import bpy

args = sys.argv[sys.argv.index("--") + 1:]
src, dst = args[0], args[1]
share = float(args[2]) if len(args) > 2 else 0.02

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
for obj in meshes:
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.mesh.separate(type="LOOSE")
    bpy.ops.object.mode_set(mode="OBJECT")
parts = [o for o in bpy.context.scene.objects if o.type == "MESH"]
biggest = max(len(p.data.vertices) for p in parts)
kept = []
for part in parts:
    if len(part.data.vertices) < biggest * share:
        print(f"SPECKS drop {part.name}: {len(part.data.vertices)} vertices (biggest {biggest})")
        bpy.data.objects.remove(part, do_unlink=True)
    else:
        kept.append(part)
bpy.ops.object.select_all(action="DESELECT")
for part in kept:
    part.select_set(True)
bpy.context.view_layer.objects.active = kept[0]
if len(kept) > 1:
    bpy.ops.object.join()
print(f"SPECKS kept {len(kept)} parts")
bpy.ops.export_scene.fbx(filepath=dst, use_selection=False, path_mode="COPY", embed_textures=False)
