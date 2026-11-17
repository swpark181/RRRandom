#!/bin/bash
# usage: perf_wreck.sh <tag> [extra args]   (round 99: real-time frame hitches round smashing and mending buildings)
TAG=$1; shift
export RR_SEQ="${RR_SEQ_OVERRIDE:-9:RRGod;10:stat dumphitches;12:RRWreck 500;14:RRWreck 700;16:RRWreck 400 1;17:RRWreck 900;19:RRWreck 600;36:quit}"
"/c/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "C:/work/RRRandom/RRRandom.uproject" -game -windowed -ResX=1280 -ResY=720 -nosound \
  -RRSolo -RRThirdPerson -RRRule=Rounds -RRNoOnlinePanel -RRIdleBots -abslog=C:/work/RRRandom/Saved/Rounds99/$TAG.log \
  "-ExecCmds=rr.LogHitches 25, rr.Wreck.RestoreSeconds 6, t.HitchFrameTimeThreshold 25, py C:/work/char-pipeline/scripts/ue_game_seq.py" "$@"
echo "EXIT $?"
