#!/bin/sh
# Storyboard draft (2026-11-23): runs one editor Python script, or renders a sequence of the draft map with Movie Render Queue.
#   board_run.sh py <script> [done mark]                 (environment variables pass through to the script)
#   board_run.sh render <sequence> <take> [WxH]          frames -> char-pipeline/output/storyboard/board2/<take>/fvt.NNNN.jpeg
UE="/c/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64"
PROJ="C:/work/RRRandom/RRRandom.uproject"
HERE="C:/work/RRRandom/Tools/cinematic"
OUT="C:/work/char-pipeline/output/storyboard/board2"
FOLDER="/Game/Cinematics/FoxVsTiger/Board"
runpy() {
  LOG="C:/work/RRRandom/Saved/Logs/board_$(basename "$1" .py).log"; rm -f "$LOG"
  "$UE/UnrealEditor-Cmd.exe" "$PROJ" -run=pythonscript "-script=$HERE/$1" -unattended -nosplash "-abslog=$LOG" > /dev/null 2>&1 &
  PID=$!; t=0; res=timeout
  while [ $t -lt 500 ]; do
    sleep 5; t=$((t+5))
    if ! kill -0 $PID 2>/dev/null; then res=exited; break; fi
    if [ -f "$LOG" ] && grep -q -E "${2:-BOARD DONE}|Traceback \(most recent call last\)" "$LOG"; then res=done; sleep 8; break; fi
  done
  kill -9 $PID 2>/dev/null
  WPID=$(ps -W | grep -i "UnrealEditor-Cmd" | awk '{print $4}' | head -1)
  [ -n "$WPID" ] && taskkill //PID $WPID //F > /dev/null 2>&1
  echo "board_run: $1 $res after ${t}s"
  grep -E "LogPython: (BOARD|CINECFG|Error)|Traceback|Error:" "$LOG" | grep -v "LogInit" | sed 's/^.*LogPython: //' | tail -${3:-60}
}
case "$1" in
  py) runpy "$2" "$3" "$4" ;;
  render)
    SEQ=$2; TAKE=$3; RES=${4:-1280x720}; W=${RES%x*}; H=${RES#*x}
    RR_CINE_PRESET=MRQ_Board RR_CINE_TAKE=$TAKE RR_CINE_RES=$RES RR_CINE_OUT=$OUT runpy ue_cine_render_config.py "CINECFG DONE" 5
    LOG="C:/work/RRRandom/Saved/Logs/board_render.log"; rm -f "$LOG"; rm -rf "$OUT/$TAKE"
    "$UE/UnrealEditor.exe" "$PROJ" $FOLDER/Map_FvT_Board -game "-LevelSequence=$FOLDER/$SEQ" "-MoviePipelineConfig=/Game/Cinematics/FoxVsTiger/MRQ_Board" -windowed -ResX=$W -ResY=$H -nosound -unattended -dpcvars=r.PSOPrecaching=0 "-abslog=$LOG" > /dev/null 2>&1 &
    PID=$!; t=0
    while [ $t -lt 900 ] && kill -0 $PID 2>/dev/null; do sleep 5; t=$((t+5)); done
    kill -9 $PID 2>/dev/null
    grep -E "Movie Pipeline completed|RRCine|Error:" "$LOG" | tail -5
    echo "render: $(ls "$OUT/$TAKE" 2>/dev/null | wc -l) frames in $OUT/$TAKE after ${t}s" ;;
esac
