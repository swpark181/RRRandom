"""Motion matching test, step 2 (run inside Unreal, in the Game Animation Sample project): list what the sample's
Pose Search databases hold, so the same sets can be rebuilt for the fox.

A database's animation list is a private property that Python cannot read, so every database is exported as text (T3D)
into <out dir>/t3d/<name>.t3d and the list is read from there. Writes <out dir>/gasp_dbs.json:
{database path: {schema, animations: [{path, mirror, enabled, sampling range, length}]}} and logs one line per set.

Usage: UnrealEditor-Cmd GameAnimationSample.uproject -run=pythonscript "-script=mm_gasp_dump_dbs.py <out dir>"
"""
import json
import os
import re
import sys

import unreal

out_dir = sys.argv[1]
ROOT = "/Game/Characters/UEFN_Mannequin/Animations/MotionMatchingData"
reg = unreal.AssetRegistryHelpers.get_asset_registry()
os.makedirs(os.path.join(out_dir, "t3d"), exist_ok=True)


def log(msg):
    unreal.log(f"MM {msg}")


def prop(obj, name, default=None):
    try:
        return obj.get_editor_property(name)
    except Exception:  # noqa: BLE001
        return default


dbs = {}
shown = False
for ad in reg.get_assets_by_path(ROOT, recursive=True):
    if str(ad.asset_class_path.asset_name) != "PoseSearchDatabase":
        continue
    db = ad.get_asset()
    path = str(ad.package_name)
    t3d = os.path.join(out_dir, "t3d", path.split("/")[-1] + ".t3d")
    task = unreal.AssetExportTask()
    task.object = db
    task.filename = t3d
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    task.exporter = unreal.ObjectExporterT3D()
    ok = unreal.Exporter.run_asset_export_task(task)
    text = open(t3d, encoding="utf-8", errors="replace").read() if ok and os.path.exists(t3d) else ""
    if not shown:
        shown = True
        log(f"T3D sample ({path.split('/')[-1]}, ok={ok}, {len(text)} chars): {text[:1500]!r}")
    schema = prop(db, "schema")
    entry = {
        "schema": schema.get_path_name().split(".")[0] if schema else None,
        "tags": [str(t) for t in (prop(db, "tags") or [])],
        "count": db.get_num_animation_assets(),
        "animations": [],
    }
    for i in range(db.get_num_animation_assets()):
        asset = db.get_animation_asset(i)
        item = {"path": asset.get_path_name().split(".")[0] if asset else None,
                "class": asset.get_class().get_name() if asset else None}
        if asset and isinstance(asset, unreal.AnimSequence):
            item["length"] = round(asset.get_play_length(), 3)
        entry["animations"].append(item)
    # the private list, from the text export: one "(...)" per animation
    m = re.search(r"DatabaseAnimationAssets=\((.*)\)\s*$", text, re.M)
    entry["raw_list"] = m.group(1) if m else None
    dbs[path] = entry

with open(os.path.join(out_dir, "gasp_dbs.json"), "w", encoding="utf-8") as f:
    json.dump(dbs, f, indent=1)

sets = {}
for path, e in dbs.items():
    rel = path[len(ROOT) + len("/Databases/"):]
    key = rel.split("/")[0] if "/" in rel else "(top)"
    s = sets.setdefault(key, {"dbs": 0, "classes": {}, "secs": {}})
    s["dbs"] += 1
    for a in e["animations"]:
        if a["path"]:
            s["classes"][a["class"]] = s["classes"].get(a["class"], 0) + 1
            s["secs"][a["path"]] = a.get("length", 0.0)
for key, s in sorted(sets.items()):
    log(f"SET {key}: {s['dbs']} databases, {len(s['secs'])} different animations, "
        f"{sum(s['secs'].values()):.0f} s, classes {s['classes']}")
for name in ("Dense/PSD_Dense_Stand_Run_Loops", "Dense/PSD_Dense_Stand_Idles", "Relaxed/PSD_Relaxed_Stand_Run_Loops"):
    e = dbs.get(f"{ROOT}/Databases/{name}")
    if e:
        log(f"DB {name}: schema {e['schema'].split('/')[-1]} "
            f"anims {[(a['path'].split('/')[-1], a.get('length')) for a in e['animations']][:10]} raw {str(e['raw_list'])[:600]}")
log(f"MMDONE dump {len(dbs)} databases -> {out_dir}")
