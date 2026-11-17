#!/bin/bash
# usage: net_lag.sh <tag> <smooth 0|1> [lag ms] [variance ms] [loss %]   (round 100)
# A host and a joiner on this PC over LAN, both with the engine's packet simulation (each delays what it sends by
# <lag> +- <variance> ms and drops <loss> % of it), bots playing. Both log rr.Net.LogSmooth lines: how far other
# machines' bodies jump on their screen. Logs: Saved/Rounds99/net_<tag>_host.log and net_<tag>_join.log.
TAG=$1; SMOOTH=$2; LAG=${3:-100}; VAR=${4:-60}; LOSS=${5:-3}
UE="/c/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe"
PROJ="C:/work/RRRandom/RRRandom.uproject"
OUT=C:/work/RRRandom/Saved/Rounds99
SEQ="C:/work/char-pipeline/scripts/ue_game_seq.py"
NET="-PktLag=$LAG -PktLagVariance=$VAR -PktLoss=$LOSS"
(RR_SEQ="${HOST_SEQ:-12:RRGod;110:quit}" "$UE" "$PROJ" -game -windowed -ResX=960 -ResY=540 -WinX=0 -WinY=40 -nosound -RRHost -RRLan -RRThirdPerson \
  -RRRule=Rounds -RRSpecies=Tiger -RRRoundRule=1 -RRKillTarget=60 $NET -abslog=$OUT/net_${TAG}_host.log \
  "-ExecCmds=${HOST_EXEC}rr.Net.Smooth $SMOOTH, rr.Net.LogSmooth 1, py $SEQ" > /dev/null 2>&1 &)
sleep 28
RR_NETWORLD=1 RR_SEQ="${JOIN_SEQ:-30:RRGod;32:walk=1,0,6;40:walk=0,1,6;48:walk=-1,0,6;56:walk=0,-1,6;64:walk=1,0,6;75:quit}" "$UE" "$PROJ" -game -windowed -ResX=960 -ResY=540 -WinX=960 -WinY=40 -nosound \
  ${JOIN_HOW:--RRJoin} -RRLan -RRSpecies=RedFox $NET -abslog=$OUT/net_${TAG}_join.log \
  "-ExecCmds=rr.Net.Smooth $SMOOTH, rr.Net.LogSmooth 1, py $SEQ" > /dev/null 2>&1
sleep 3
# The host ends by its own script; make sure nothing is left
for i in $(seq 1 20); do if ! ps -W | grep -qi 'UnrealEditor.exe'; then break; fi; sleep 3; done
ps -W | grep -i 'UnrealEditor.exe' | awk '{print $4}' | while read p; do taskkill //PID $p //F > /dev/null 2>&1; done
echo "host: $(grep -c 'net smooth' /c/work/RRRandom/Saved/Rounds99/net_${TAG}_host.log) lines, join: $(grep -c 'net smooth' /c/work/RRRandom/Saved/Rounds99/net_${TAG}_join.log) lines"
