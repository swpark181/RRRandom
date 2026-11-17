#!/bin/bash
# usage: ue_py.sh <tag> <done word> "<script and its arguments>"   (round 99)
# Runs an editor python script as a commandlet, waits for <done word> (or a Traceback) in its log, then ends the process:
# on this PC the commandlet stays alive long after the script is done.
TAG=$1; WORD=$2; shift 2
LOG=/c/work/RRRandom/Saved/Rounds99/py_$TAG.log
rm -f "$LOG"
("/c/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "C:/work/RRRandom/RRRandom.uproject" -run=pythonscript "-script=$*" -abslog=C:/work/RRRandom/Saved/Rounds99/py_$TAG.log > /dev/null 2>&1 &)
for i in $(seq 1 90); do sleep 4; if grep -q "$WORD DONE\|Traceback\|RuntimeError" "$LOG" 2>/dev/null; then break; fi; done
sleep 6
ps -W | grep -i 'UnrealEditor-Cmd' | awk '{print $4}' | while read p; do taskkill //PID $p //F > /dev/null 2>&1; done
grep -h "$WORD\|Traceback\|Error:\|RuntimeError" "$LOG" | sed 's/^\[[0-9.:-]*\]\[ *[0-9]*\]//' | cut -c1-220 | head -30
