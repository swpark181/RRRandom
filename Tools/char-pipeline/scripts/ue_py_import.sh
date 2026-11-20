#!/bin/bash
# Run one Unreal editor python script as a commandlet with the legacy FBX importer, wait for its done word (or an
# error) in its own log, then end the process: on this PC the commandlet stays alive long after the script is done
# (2026-11-19, from RRRandom/Saved/Rounds99/ue_py.sh, which lacks the importer flag).
# usage: ue_py_import.sh <tag> <done regex> "<script and its |-separated arguments>" [<seconds to wait, 600>]
# The log is C:/work/char-pipeline/output/ue_logs/<tag>.log; prints its lines that match <done regex> or look like errors.
# Environment (RR_ANIM_FPS, RR_CINE_SRC ...) passes through to the script.
TAG=$1; WORD=$2; SCRIPT=$3; WAIT=${4:-600}
mkdir -p /c/work/char-pipeline/output/ue_logs
LOG=/c/work/char-pipeline/output/ue_logs/$TAG.log
rm -f "$LOG"
("/c/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "C:/work/RRRandom/RRRandom.uproject" -run=pythonscript -dpcvars=Interchange.FeatureFlags.Import.FBX=0 "-script=$SCRIPT" -unattended -nosplash -nullrhi -abslog=C:/work/char-pipeline/output/ue_logs/$TAG.log > /dev/null 2>&1 &)
n=$((WAIT / 4))
for i in $(seq 1 $n); do sleep 4; if grep -qE "$WORD|Traceback|RuntimeError" "$LOG" 2>/dev/null; then break; fi; done
sleep 8
ps -W | grep -i 'UnrealEditor-Cmd' | awk '{print $4}' | while read p; do taskkill //PID $p //F > /dev/null 2>&1; done
grep -hE "$WORD|Traceback|LogPython: Error|RuntimeError" "$LOG" | sed 's/^\[[0-9.:-]*\]\[ *[0-9]*\]//; s/^LogPython: //' | cut -c1-260 | tail -${UE_TAIL:-12}
