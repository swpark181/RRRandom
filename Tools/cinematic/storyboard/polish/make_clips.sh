#!/bin/sh
# Polish pass (2026-11-24): before / after clips for the review page. Before = final1080i (v9), after = final1080j (v10).
#   side <name> <from> <to> <slow>                    both whole frames side by side (960x540 each)
#   zoom <name> <from> <to> <slow> <x> <y>            a 960x540 piece of the 1080p frame at its own size, side by side
R=/c/work/char-pipeline/output/cinematic_blocking/render
FF=/c/work/char-pipeline/tools/ffmpeg/ffmpeg.exe
OUT=/c/work/char-pipeline/output/storyboard/polish
A=final1080i; B=final1080j
FONT="C\:/Windows/Fonts/malgunbd.ttf"
tag() { echo "drawtext=fontfile='$FONT':text='$1':x=$2:y=14:fontsize=26:fontcolor=white:box=1:boxcolor=black@0.55:boxborderw=7"; }
num() { echo "drawtext=fontfile='$FONT':text='%{eif\:n+$1\:d}':x=w-tw-16:y=h-th-14:fontsize=24:fontcolor=white:box=1:boxcolor=black@0.55:boxborderw=6"; }
mode=$1; name=$2; from=$3; to=$4; slow=$5; n=$((to - from + 1))
if [ "$mode" = side ]; then pre="scale=960:540"; else pre="crop=960:540:$6:$7"; fi
"$FF" -y -loglevel error -framerate 24 -start_number $from -i "$R/$A/fvt.%04d.jpeg" -framerate 24 -start_number $from -i "$R/$B/fvt.%04d.jpeg" -frames:v $n \
  -filter_complex "[0]$pre[a];[1]$pre[b];[a][b]hstack,$(tag '전' 14),$(tag '후' 974),$(num $from),setpts=$slow*PTS,format=yuv420p" \
  -c:v libx264 -crf 20 -movflags +faststart "$OUT/$name.mp4"
ls -la "$OUT/$name.mp4" | awk '{print $5, $9}'
