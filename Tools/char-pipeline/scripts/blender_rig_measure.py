"""Measure a Mixamo character FBX (run inside Blender): bone count and names, the hips' and head top's height in the rest
pose, the mesh's bounds, all in Unreal mesh units (cm, X = the character's left, -Y = backward, Z = up). The game draws
every species with its hips as high as the fox's (27.5 at scale 1.8 = 49.5), which sets the species' mesh scale.

Usage: blender -b --factory-startup --python blender_rig_measure.py -- <fbx> [<fbx> ...]
"""
import sys

import bpy

FOX_HIPS = 27.5 * 1.8

for src in sys.argv[sys.argv.index("--") + 1:]:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=src)
    arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
    meshes = [o for o in bpy.data.objects if o.type == "MESH"]
    bones = {b.name.split(":")[-1]: b for b in arm.data.bones}

    def height(name):
        return (arm.matrix_world @ bones[name].head_local).z * 100.0

    pts = [m.matrix_world @ v.co for m in meshes for v in m.data.vertices]
    top = max(p.z for p in pts) * 100.0
    hips = height("Hips")
    print(f"RIG {src.split('/')[-1]}: {len(bones)} bones, hips {hips:.1f}, head top bone {height('HeadTop_End'):.1f}, "
          f"mesh top {top:.1f}, scale for fox hips {FOX_HIPS / hips:.3f} (mesh top then {top * FOX_HIPS / hips:.0f}); "
          f"prefix {next(iter(arm.data.bones)).name.split(':')[0]}")
    print("RIG bones " + " ".join(sorted(bones)))
