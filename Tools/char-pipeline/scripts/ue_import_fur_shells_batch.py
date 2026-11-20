"""Runs ue_import_fur_shells.py for several animal friends in one editor start (run inside Unreal; 2026-11-20).

Usage: UnrealEditor-Cmd <project> -run=pythonscript -dpcvars=Interchange.FeatureFlags.Import.FBX=0
           "-script=ue_import_fur_shells_batch.py <args of one>;<args of the next>;..."
  e.g. "Rabbit|||;Squirrel||||12,38,60,20,16,14" (each part is what ue_import_fur_shells.py takes: see there).
Logs "FURBATCH <Name> ok" or "FURBATCH <Name> FAILED <error>" for each, and "FURBATCH DONE" at the end.
"""
import os
import sys
import traceback

import unreal

here = os.path.dirname(os.path.abspath(__file__))
one = os.path.join(here, "ue_import_fur_shells.py")
with open(one, encoding="utf-8") as f:
    code = compile(f.read(), one, "exec")
for part in [p.strip() for p in " ".join(sys.argv[1:]).split(";") if p.strip()]:
    name = part.split("|")[0]
    old = sys.argv
    sys.argv = [one, part]
    try:
        exec(code, {"__name__": "__main__", "__file__": one})
        unreal.log_warning(f"FURBATCH {name} ok")
    except Exception as e:  # one animal friend's failure doesn't stop the others
        unreal.log_warning(f"FURBATCH {name} FAILED {e}")
        unreal.log_warning(traceback.format_exc())
    sys.argv = old
unreal.log_warning("FURBATCH DONE")
