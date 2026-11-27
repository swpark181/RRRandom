#!/bin/bash
# usage: run_sky.sh <tag> <seconds> "<x y z>" [extra args...]   round 103: every bot walks to a sky path spot (rr.Bot.SkyTestGoal),
# its sky routes logged (rr.Bot.LogSky), positions tracked each second; killed after <seconds>
TAG=$1; SECS=$2; GOAL=$3; shift 3
"/c/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "C:/work/RRRandom/RRRandom.uproject" -game -windowed -ResX=960 -ResY=540 -nosound \
  -RRSolo -RRThirdPerson -RRRule=Rounds -RRSpectate -RRNoOnlinePanel -RRMatches=1 -benchmark -UseFixedTimeStep -FPS=30 \
  -abslog=C:/work/RRRandom/Saved/Parkour/$TAG.log "-ExecCmds=rr.Bot.LogStalls 1, rr.Bot.LogSky 1, rr.Bot.SkyTestGoal $GOAL, py C:/work/RRRandom/Saved/Parkour/botmatch_track.py" "$@" &
PID=$!
sleep $SECS
taskkill //IM UnrealEditor.exe //F > /dev/null 2>&1
wait $PID 2>/dev/null
echo done
