#!/bin/bash
# usage: seq.sh <tag> "<RR_SEQ>" [extra args...]   (round 99: a scripted run with HUD screenshots: "Shot showui" steps)
# The new screenshots are copied to Saved/Rounds99/shots/<tag>_<n>.png.
TAG=$1; export RR_SEQ="$2"; shift 2
SHOTS=/c/work/RRRandom/Saved/Screenshots/WindowsEditor
OUT=/c/work/RRRandom/Saved/Rounds99/shots
mkdir -p "$SHOTS" "$OUT"
ls "$SHOTS" > /tmp/shots_before_$TAG.txt
"/c/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "C:/work/RRRandom/RRRandom.uproject" -game -windowed -ResX=1280 -ResY=720 -nosound \
  -RRSolo -RRNoOnlinePanel -abslog=C:/work/RRRandom/Saved/Rounds99/seq_$TAG.log \
  "-ExecCmds=${RR_EXEC}py C:/work/char-pipeline/scripts/ue_game_seq.py" "$@"
n=0
for f in $(ls -tr "$SHOTS"); do
  if ! grep -qx "$f" /tmp/shots_before_$TAG.txt; then cp "$SHOTS/$f" "$OUT/${TAG}_$n.png"; echo "$OUT/${TAG}_$n.png"; n=$((n+1)); fi
done
grep -h 'Traceback\|GAMESEQ' /c/work/RRRandom/Saved/Rounds99/seq_$TAG.log | sed 's/^\[[0-9.:-]*\]\[ *[0-9]*\]//' | head -30
