#!/bin/sh
# clip.sh <tag> <from> <to> <crop w:h:x:y> <out.mp4> [width]: a piece of a rec_tail.ps1 take between two game times,
# cropped, for the ear page (2026-11-25). Also writes <out>_last.png, the clip's last frame, to look at before publishing
# (a take's end can show the desktop once the game has quit).
TAG=$1; A=$2; Z=$3; CROP=$4; OUT=$5; W=${6:-720}
C=/c/work/char-pipeline/output/video/tail/clips
F=/c/work/char-pipeline/tools/ffmpeg/ffmpeg.exe
b=$(grep -o "began [0-9.]*" $C/$TAG.txt | awk '{split($2,a,"."); print a[1]*3600+a[2]*60+a[3]+a[4]/1000}')
# (game time 0 = the first step's log time minus its game time; the "clock starts" line comes a varying 0-2 s before
# that, and timing from it put clip ends after the quit, on the desktop - 2026-11-25)
s=$(grep -m1 -E "GAMESEQ [0-9.]+ frame" $C/$TAG.txt | sed 's/.*-\([0-9]*\)\.\([0-9]*\)\.\([0-9]*\):\([0-9]*\)\].*GAMESEQ \([0-9.]*\) frame.*/\1 \2 \3 \4 \5/' | awk '{print $1*3600+$2*60+$3+$4/1000-$5}')
q=$(grep -m1 -E "GAMESEQ [0-9.]+ frame [0-9]+ quit" $C/$TAG.txt | sed 's/.*GAMESEQ \([0-9.]*\) frame.*/\1/')
if [ -n "$q" ]; then Z=$(awk -v z=$Z -v q=$q 'BEGIN{print (z > q - 0.5) ? q - 0.5 : z}'); fi
from=$(awk -v t=$A -v b=$b -v s=$s 'BEGIN{print t + s - b}')
len=$(awk -v a=$A -v z=$Z 'BEGIN{print z - a}')
$F -y -loglevel error -ss $from -i $C/$TAG.mp4 -t $len -vf "crop=$CROP,scale=$W:-2" -an -c:v h264_nvenc -preset p5 -cq 26 -pix_fmt yuv420p -movflags +faststart $OUT
$F -y -loglevel error -sseof -0.1 -i $OUT -frames:v 1 -update 1 ${OUT%.mp4}_last.png
