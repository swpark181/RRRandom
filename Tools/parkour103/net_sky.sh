#!/bin/bash
# usage: net_sky.sh <tag> [lag ms] [variance ms] [loss %]   (round 103: the sky path under network lag)
# A host (tiger, bots) and a joiner (red fox) on this PC over LAN, both with the engine's packet simulation. The joiner is
# put at the foot of the blue half's sky path stairs (RRGoSky 0) and runs it to the roof pressing jump (video_seq route=),
# with p.NetShowCorrections 1 on both: every position the server corrects is logged ("Error for"). Logs in Saved/Parkour.
TAG=$1; LAG=${2:-100}; VAR=${3:-60}; LOSS=${4:-3}
UE="/c/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe"
PROJ="C:/work/RRRandom/RRRandom.uproject"
OUT=C:/work/RRRandom/Saved/Parkour
HOSTSEQ="C:/work/char-pipeline/scripts/ue_game_seq.py"
JOINSEQ="C:/work/char-pipeline/output/video/r88/video_seq.py"
NET="-PktLag=$LAG -PktLagVariance=$VAR -PktLoss=$LOSS"
(RR_SEQ="12:RRGod;150:quit" "$UE" "$PROJ" -game -windowed -ResX=960 -ResY=540 -WinX=0 -WinY=40 -nosound -RRHost -RRLan -RRThirdPerson \
  -RRRule=Rounds -RRSpecies=Tiger -RRRoundRule=1 -RRKillTarget=60 $NET -abslog=$OUT/netsky_${TAG}_host.log \
  "-ExecCmds=p.NetShowCorrections 1, py $HOSTSEQ" > /dev/null 2>&1 &)
sleep 28
RR_NETWORLD=1 RR_SEQ="${JOIN_SEQ:-20:RRGod;45:RRGoSky 0;48:route=600:-2500|600:-2700|600:-2900|600:-3130|300:-3100|0:-3100|-300:-3100|-750:-3100|-1250:-3100|-1500:-3100;85:dump;88:quit}" \
  "$UE" "$PROJ" -game -windowed -ResX=960 -ResY=540 -WinX=960 -WinY=40 -nosound -RRJoin -RRLan -RRSpecies=RedFox $NET -abslog=$OUT/netsky_${TAG}_join.log \
  "-ExecCmds=p.NetShowCorrections 1, py $JOINSEQ" > /dev/null 2>&1
sleep 3
for i in $(seq 1 20); do if ! ps -W | grep -qi 'UnrealEditor.exe'; then break; fi; sleep 3; done
ps -W | grep -i 'UnrealEditor.exe' | awk '{print $4}' | while read p; do taskkill //PID $p //F > /dev/null 2>&1; done
echo "join corrections: $(grep -c 'Error for' $OUT/netsky_${TAG}_join.log), host: $(grep -c 'Error for' $OUT/netsky_${TAG}_host.log)"
grep "GAMESEQ route point\|to sky hop" $OUT/netsky_${TAG}_join.log $OUT/netsky_${TAG}_host.log | sed 's/.*LogPython: //; s/.*LogTemp: //' | cut -c1-160
