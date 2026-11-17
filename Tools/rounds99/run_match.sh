#!/bin/bash
# usage: run_match.sh <tag> <matches> [extra args...]   (round 99: bot stalls logged, rr.Bot.LogStalls 1)
TAG=$1; N=$2; shift 2
"/c/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "C:/work/RRRandom/RRRandom.uproject" -game -windowed -ResX=960 -ResY=540 -nosound \
  -RRSolo -RRThirdPerson -RRRule=Rounds -RRSpectate -RRNoOnlinePanel -RRMatches=$N -benchmark -UseFixedTimeStep -FPS=30 \
  -abslog=C:/work/RRRandom/Saved/Rounds99/$TAG.log "-ExecCmds=rr.Bot.LogStalls 1, rr.LogHitches 50, py C:/work/RRRandom/Saved/Rounds98/botmatch.py" "$@"
echo "EXIT $?"
