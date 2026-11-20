#!/bin/sh
# Run one Unreal Python script as a commandlet and stop Unreal once the script is done.
# mm_run_ue.sh <uproject (Windows path)> <log file> <max seconds> <script path and its arguments, as one string>
# MM_RHI=-AllowCommandletRendering in the environment runs it with a renderer (FBX export with a mesh needs one).
# The script must log a line holding "MMDONE" at its end. UnrealEditor-Cmd often never exits on this PC after the
# script is done, so the log is watched and the process is killed when that line (or a Python error) shows up.
P=$1; LOG=$2; MAX=$3; SCRIPT=$4
UE="/c/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
rm -f "$LOG"
"$UE" "$P" -run=pythonscript -dpcvars=Interchange.FeatureFlags.Import.FBX=0 "-script=$SCRIPT" -unattended -nosplash ${MM_RHI:--nullrhi} "-abslog=$LOG" > /dev/null 2>&1 &
PID=$!
t=0; res=timeout
while [ $t -lt $MAX ]; do
  sleep 5; t=$((t+5))
  if ! kill -0 $PID 2>/dev/null; then res=exited; break; fi
  if [ -f "$LOG" ] && grep -q "MMDONE" "$LOG"; then res=done; sleep 3; break; fi
  if [ -f "$LOG" ] && grep -q "Traceback (most recent call last)" "$LOG"; then res=pyerror; sleep 3; break; fi
done
kill -9 $PID 2>/dev/null
WPID=$(ps -W | grep -i "UnrealEditor-Cmd" | awk '{print $4}' | head -1)
[ -n "$WPID" ] && taskkill //PID $WPID //F > /dev/null 2>&1
echo "mm_run_ue: $res after ${t}s"
grep -E "LogPython: (MM|Error|Warning)|Traceback" "$LOG" | sed 's/^.*LogPython: //' | tail -${5:-80}
