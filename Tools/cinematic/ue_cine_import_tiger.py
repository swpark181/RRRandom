# Cinematic (2026-11-19): puts the Mixamo sword-and-shield set (taken from the rabbit rig Mixamo holds, hips 26.7) onto the
# tiger (hips 25.6) as /Game/Characters/Tiger/Anims/A_Tiger_Cine_<Name>, by running char-pipeline's
# ue_import_shared_anim.py once per file. Run with UnrealEditor-Cmd -run=pythonscript
# -dpcvars=Interchange.FeatureFlags.Import.FBX=0 (the legacy FBX importer, like the shared script wants).
import os
import sys

import unreal

SRC = os.environ.get("RR_CINE_SRC", "C:/work/char-pipeline/input/tiger_cine/mixamo")
SHARED = "C:/work/char-pipeline/scripts/ue_import_shared_anim.py"
RIG_HIPS = "26.7"
names = sorted(f[:-4] for f in os.listdir(SRC) if f.endswith(".fbx"))
code = compile(open(SHARED, encoding="utf-8").read(), SHARED, "exec")
os.environ["RR_ANIM_FPS"] = "30"
for name in names:
    suffix = "Cine_" + "".join(p.capitalize() for p in name.replace("_SK", "").split("_"))
    # RR_CINE_TARGET: who gets it (the species' hips height at scale 1, RRRandomSpecies.cpp): the tiger by default,
    # "Fox=19.8" for the fox's stop
    sys.argv = [SHARED, f"{SRC}/{name}.fbx|{suffix}|{RIG_HIPS}|{os.environ.get('RR_CINE_TARGET', 'Tiger=25.6')}"]
    try:
        exec(code, {"__name__": "__main__"})
    except Exception as e:  # noqa: BLE001 - one bad file must not stop the rest
        unreal.log(f"CINEIMPORT {name} failed: {e}")
    unreal.log(f"CINEIMPORT {name} -> A_Tiger_{suffix}")
unreal.log("CINEIMPORT DONE")
