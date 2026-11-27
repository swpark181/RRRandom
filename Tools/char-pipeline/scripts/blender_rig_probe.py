"""Print each FBX's armature bone count, mesh objects (vertices, faces, materials) and the Head bone (2026-11-25, ear
bones: which rig file matches the game). Usage: blender -b -P blender_rig_probe.py -- <fbx> [<fbx> ...]"""
import sys
import bpy
for path in sys.argv[sys.argv.index("--") + 1:]:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=path)
    arms = [o for o in bpy.data.objects if o.type == "ARMATURE"]
    meshes = [o for o in bpy.data.objects if o.type == "MESH"]
    line = f"PROBE {path}: "
    for a in arms:
        names = [b.name for b in a.data.bones]
        hb = a.data.bones.get("mixamorig:Head") or a.data.bones.get("Head")
        head = a.matrix_world @ hb.head_local if hb else None
        line += f"arm {a.name} bones {len(names)} head {tuple(round(c, 3) for c in head) if head else None} extra {[n for n in names if 'Tail' in n or 'Ear' in n][:12]}; "
    for m in meshes:
        zs = [(m.matrix_world @ v.co).z for v in m.data.vertices]
        line += f"mesh {m.name} v{len(m.data.vertices)} f{len(m.data.polygons)} mats {[s.material.name if s.material else None for s in m.material_slots]} z {min(zs):.3f}..{max(zs):.3f} groups {len(m.vertex_groups)}; "
    print(line)
