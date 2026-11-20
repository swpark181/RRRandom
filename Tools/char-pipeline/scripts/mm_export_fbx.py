"""Export animation sequences with their mesh as FBX, for a look in Blender (run inside Unreal).

Usage: UnrealEditor-Cmd <project> -run=pythonscript "-script=mm_export_fbx.py <mesh asset path>|<out dir>|<anim asset path>|<anim asset path>..."
Each animation gets the mesh as its preview mesh (saved that way) and goes to <out dir>/<asset name>.fbx.
"""
import os
import sys

import unreal

args = " ".join(sys.argv[1:]).split("|")
mesh = unreal.load_asset(args[0].strip())
out_dir = args[1].strip()
os.makedirs(out_dir, exist_ok=True)
for path in [a.strip() for a in args[2:] if a.strip()]:
    seq = unreal.load_asset(path)
    if not seq:
        unreal.log(f"MM export: no {path}")
        continue
    seq.set_preview_skeletal_mesh(mesh)
    opt = unreal.FbxExportOption()
    opt.export_preview_mesh = True
    opt.ascii = False
    opt.collision = False
    opt.level_of_detail = False
    task = unreal.AssetExportTask()
    task.object = seq
    task.filename = os.path.join(out_dir, seq.get_name() + ".fbx")
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    task.exporter = unreal.AnimSequenceExporterFBX()
    task.options = opt
    ok = unreal.Exporter.run_asset_export_task(task)
    unreal.log(f"MM export {seq.get_name()} ok={ok} {seq.get_play_length():.2f}s -> {task.filename}")
unreal.log("MM MMDONE export")
