"""Soil-only blocks cut from the grass blocks, for stacking under them into tall hills and cliffs (run inside Blender).

Usage: blender -b --factory-startup --python blender_soil_blocks.py -- <out dir> <name>=<grass block fbx> ...
  e.g. ... -- C:/work/char-pipeline/output/grass SM_SoilBlock_A=C:/work/char-pipeline/output/grass/SM_GrassBlock_A.fbx

The grass blocks (blender_grass_blocks.py: a 100 cube, pivot in the middle of the bottom) are soil up to about 60% of
their height and grass above (the skirt hanging over the sides from ~62%, the top from ~90%). A stretched block makes a
tall hill's soil sides stretch with it, so a hill is stacked instead: soil blocks, one per level, with a grass block on
top. This keeps the faces whose middle is under CUT (the soil body), then stretches them back to a full 100 tall. The
open top is never seen: the block above (or the grass block on top, whose skirt hangs over the seam) covers it.
Same UVs, so in Unreal it takes the matching grass block's material.
"""
import sys

import bmesh
import bpy

args = sys.argv[sys.argv.index("--") + 1:]
out_dir = args[0]
blocks = [a.split("=", 1) for a in args[1:]]
CUT = 0.6

for name, src in blocks:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=src)
    obj = next(o for o in bpy.context.scene.objects if o.type == "MESH")
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

    top = max(v.co.z for v in obj.data.vertices)
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    cut = [f for f in bm.faces if f.calc_center_median().z > top * CUT]
    bmesh.ops.delete(bm, geom=cut, context="FACES")
    # Soil walls reaching past the cut are sliced at it (whole, they'd poke into the block above and flicker against
    # its own walls), then everything is stretched so the cut lands on the top
    height = max(v.co.z for v in bm.verts)
    bmesh.ops.bisect_plane(bm, geom=bm.verts[:] + bm.edges[:] + bm.faces[:], plane_co=(0.0, 0.0, top * CUT),
                           plane_no=(0.0, 0.0, 1.0), clear_outer=True)
    for v in bm.verts:
        v.co.z *= top / (top * CUT)
    kept = len(bm.faces)
    bm.to_mesh(obj.data)
    bm.free()
    obj.data.update()
    obj.name = name
    bpy.ops.export_scene.fbx(filepath=f"{out_dir}/{name}.fbx", use_selection=True, object_types={"MESH"},
                             mesh_smooth_type="FACE", path_mode="STRIP", bake_anim=False)
    zs = [v.co.z for v in obj.data.vertices]
    print(f"SOIL {name}: kept {kept} faces (cut {len(cut)}), height {min(zs) * 100:.1f}..{max(zs) * 100:.1f}, was soil up to {height * 100:.1f}")
