"""Several ue_import_shared_anim.py runs in one editor session (2026-11-19, the new fox: emotes, sword and cinematic
motions back onto its new skeleton; one editor start instead of twenty).

Usage: UnrealEditor-Cmd <project> -run=pythonscript -dpcvars=Interchange.FeatureFlags.Import.FBX=0
           "-script=ue_import_shared_batch.py <list file>"
  <list file>: one motion per line, "<fbx>|<Name>|<source hips>|<Species>=<hips>[|...]" exactly as
  ue_import_shared_anim.py takes it, optionally followed by " @<fps>" (RR_ANIM_FPS for that line: 30 for Mixamo's
  takes; none for Tripo's 24 fps clips). Lines starting with # are skipped. One bad line does not stop the rest.
"""
import os
import sys

import unreal

SHARED = os.path.join(os.path.dirname(os.path.abspath(__file__)), "ue_import_shared_anim.py")
code = compile(open(SHARED, encoding="utf-8").read(), SHARED, "exec")
lines = [l.strip() for l in open(" ".join(sys.argv[1:]), encoding="utf-8") if l.strip() and not l.startswith("#")]
ok = 0
for line in lines:
    fps = None
    if " @" in line:
        line, fps = line.rsplit(" @", 1)
    if fps:
        os.environ["RR_ANIM_FPS"] = fps.strip()
    else:
        os.environ.pop("RR_ANIM_FPS", None)
    sys.argv = [SHARED, line.strip()]
    try:
        exec(code, {"__name__": "__main__", "__file__": SHARED})
        ok += 1
    except Exception as e:  # noqa: BLE001
        unreal.log(f"SHAREDBATCH failed: {line[:120]}: {e}")
unreal.log(f"SHAREDBATCH DONE {ok} of {len(lines)}")
