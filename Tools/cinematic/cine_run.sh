#!/bin/sh
# Fox vs tiger cinematic (2026-11-24, the re-cut): builds and renders the real map and sequence from Git Bash.
# (cine_blocking.ps1 is the runner of the film before it: its review sheet has that film's nine cuts.)
#   cine_run.sh build                        Map_FoxVsTiger + LS_FoxVsTiger from ue_cine_build.py (overwrites hand edits)
#   cine_run.sh py <script> [done mark]      any editor Python script (environment variables pass through)
#   cine_run.sh render <take> [WxH]          frames -> char-pipeline/output/cinematic_blocking/render/<take>/fvt.NNNN.jpeg
#                                            (RR_CINE_FINAL=1: the final quality, 8 samples a frame, textures at full size)
#                                            a 4th argument "from,to" renders only those frames (one cut, for a quick look)
#   cine_run.sh mp4 <take> <name> [crf] [height]   render/<name>.mp4 from a take's frames
#   cine_run.sh clip <take> <from> <to> <name> [slow]      render/<name>.mp4: those frames with their numbers, played <slow> times slower
#   cine_run.sh compare <takeA> <takeB> <from> <to> <name> [slow]   the same part of two takes side by side (A left, B right)
UE="/c/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64"
PROJ="C:/work/RRRandom/RRRandom.uproject"
HERE="C:/work/RRRandom/Tools/cinematic"
OUT="C:/work/char-pipeline/output/cinematic_blocking/render"
FF="/c/work/char-pipeline/tools/ffmpeg/ffmpeg.exe"
FOLDER="/Game/Cinematics/FoxVsTiger"
export MSYS_NO_PATHCONV=1 MSYS2_ENV_CONV_EXCL="*"
runpy() {
  LOG="C:/work/RRRandom/Saved/Logs/cine_$(basename "$1" .py).log"; rm -f "$LOG"
  "$UE/UnrealEditor-Cmd.exe" "$PROJ" -run=pythonscript "-script=$HERE/$1" -unattended -nosplash "-abslog=$LOG" > /dev/null 2>&1 &
  PID=$!; t=0; res=timeout
  while [ $t -lt 500 ]; do
    sleep 5; t=$((t+5))
    if ! kill -0 $PID 2>/dev/null; then res=exited; break; fi
    if [ -f "$LOG" ] && grep -q -E "${2:-CINEBUILD DONE}|Traceback \(most recent call last\)" "$LOG"; then res=done; sleep 8; break; fi
  done
  kill -9 $PID 2>/dev/null
  WPID=$(ps -W | grep -i "UnrealEditor-Cmd" | awk '{print $4}' | head -1)
  [ -n "$WPID" ] && taskkill //PID $WPID //F > /dev/null 2>&1
  echo "cine_run: $1 $res after ${t}s"
  grep -E "LogPython: (CINEBUILD|BOARD|CINECFG|Error)|Traceback|Error:" "$LOG" | grep -v "LogInit\|LogEOS\|EOS:" | sed 's/^.*LogPython: //' | tail -${3:-60}
}
case "$1" in
  build) runpy ue_cine_build.py "CINEBUILD DONE" "$2" ;;
  py) runpy "$2" "$3" "$4" ;;
  render)
    TAKE=$2; RES=${3:-1280x720}; W=${RES%x*}; H=${RES#*x}
    RR_CINE_RANGE=$4 RR_CINE_TAKE=$TAKE RR_CINE_RES=$RES RR_CINE_OUT=$OUT runpy ue_cine_render_config.py "CINECFG DONE" 5
    LOG="C:/work/RRRandom/Saved/Logs/cine_render.log"; rm -f "$LOG"; rm -rf "$OUT/$TAKE"
    "$UE/UnrealEditor.exe" "$PROJ" $FOLDER/Map_FoxVsTiger -game "-LevelSequence=$FOLDER/LS_FoxVsTiger" "-MoviePipelineConfig=$FOLDER/MRQ_Blocking720" -windowed -ResX=$W -ResY=$H -nosound -unattended -dpcvars=r.PSOPrecaching=0 "-abslog=$LOG" > /dev/null 2>&1 &
    PID=$!; t=0
    while [ $t -lt 2400 ] && kill -0 $PID 2>/dev/null; do sleep 5; t=$((t+5)); done
    kill -9 $PID 2>/dev/null
    grep -E "Movie Pipeline completed|RRCine|Error:" "$LOG" | grep -v "LogEOS\|EOS:" | tail -5
    echo "render: $(ls "$OUT/$TAKE" 2>/dev/null | wc -l) frames in $OUT/$TAKE after ${t}s" ;;
  mp4)
    TAKE=$2; NAME=$3; CRF=${4:-18}; HEIGHT=$5
    EXT=$(ls "$OUT/$TAKE" | head -1 | sed 's/.*\.//')
    VF="format=yuv420p"; [ -n "$HEIGHT" ] && VF="scale=-2:$HEIGHT,format=yuv420p"
    "$FF" -y -loglevel error -framerate 24 -i "$OUT/$TAKE/fvt.%04d.$EXT" -vf "$VF" -c:v libx264 -crf $CRF -movflags +faststart "$OUT/$NAME.mp4"
    ls -la "$OUT/$NAME.mp4" ;;
  clip|compare)
    FONT="C\:/Windows/Fonts/arialbd.ttf"
    if [ "$1" = clip ]; then A=$2; B=""; FROM=$3; TO=$4; NAME=$5; SLOW=${6:-1}; else A=$2; B=$3; FROM=$4; TO=$5; NAME=$6; SLOW=${7:-1}; fi
    N=$((TO - FROM + 1)); EXT=$(ls "$OUT/$A" | head -1 | sed 's/.*\.//')
    TAG="drawtext=fontfile='$FONT':text='%{eif\:n+$FROM\:d}':x=16:y=14:fontsize=30:fontcolor=white:box=1:boxcolor=black@0.5:boxborderw=6"
    if [ -z "$B" ]; then
      "$FF" -y -loglevel error -framerate 24 -start_number $FROM -i "$OUT/$A/fvt.%04d.$EXT" -frames:v $N -vf "$TAG,setpts=$SLOW*PTS,format=yuv420p" -c:v libx264 -crf 18 -movflags +faststart "$OUT/$NAME.mp4"
    else
      "$FF" -y -loglevel error -framerate 24 -start_number $FROM -i "$OUT/$A/fvt.%04d.$EXT" -framerate 24 -start_number $FROM -i "$OUT/$B/fvt.%04d.$EXT" -frames:v $N         -filter_complex "[0]scale=960:540[a];[1]scale=960:540[b];[a][b]hstack,$TAG,setpts=$SLOW*PTS,format=yuv420p" -c:v libx264 -crf 18 -movflags +faststart "$OUT/$NAME.mp4"
    fi
    ls -la "$OUT/$NAME.mp4" ;;
esac
