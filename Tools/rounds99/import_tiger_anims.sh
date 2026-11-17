#!/bin/bash
# Round 101: the tiger's hammer and shield motions (blender_tiger_shield_arms.py, on the tiger's own rig) into Unreal.
A=C:/work/char-pipeline/input/tiger_r101_tripo/arms
for pair in hammer1=HammerSwing1 hammer2=HammerSwing2 hammer3=HammerSwing3 shield_block=ShieldBlock shield_carry=ShieldCarry; do
  f=${pair%%=*}; n=${pair##*=}
  LOG=/c/work/RRRandom/Saved/Rounds99/py_anim_$n.log; rm -f "$LOG"
  ("/c/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "C:/work/RRRandom/RRRandom.uproject" -run=pythonscript -dpcvars=Interchange.FeatureFlags.Import.FBX=0 "-script=C:/work/char-pipeline/scripts/ue_import_shared_anim.py $A/$f.fbx|$n|25.57|Tiger=25.57" -abslog=C:/work/RRRandom/Saved/Rounds99/py_anim_$n.log > /dev/null 2>&1 &)
  for i in $(seq 1 90); do sleep 4; if grep -q "SHAREDANIM DONE\|SHAREDANIM saved\|Traceback\|RuntimeError" "$LOG" 2>/dev/null; then break; fi; done
  sleep 8
  ps -W | grep -i 'UnrealEditor-Cmd' | awk '{print $4}' | while read p; do taskkill //PID $p //F > /dev/null 2>&1; done
  echo "$n: $(grep -h 'SHAREDANIM' "$LOG" | sed 's/^\[[0-9.:-]*\]\[ *[0-9]*\]//' | tail -2 | cut -c1-200 | tr '\n' ' ')"
done
