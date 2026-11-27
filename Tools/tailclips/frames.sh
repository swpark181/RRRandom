#!/bin/sh
# frames.sh <tag> <crop w:h:x:y> <out prefix> <game seconds...>: stills from a rec_tail.ps1 take at game times
# (the take's .txt: "began" and "GAMESEQ clock starts"), cropped (2026-11-25, ear checks)
TAG=$1; CROP=$2; OUT=$3; shift 3
C=/c/work/char-pipeline/output/video/tail/clips
F=/c/work/char-pipeline/tools/ffmpeg/ffmpeg.exe
b=$(grep -o "began [0-9.]*" $C/$TAG.txt | awk '{split($2,a,"."); print a[1]*3600+a[2]*60+a[3]+a[4]/1000}')
# (game time 0 = the first step's log time minus its game time; the "clock starts" line comes a varying 0-2 s before
# that, and timing from it put frames and clip ends after the quit, on the desktop - 2026-11-25)
s=$(grep -m1 -E "GAMESEQ [0-9.]+ frame" $C/$TAG.txt | sed 's/.*-\([0-9]*\)\.\([0-9]*\)\.\([0-9]*\):\([0-9]*\)\].*GAMESEQ \([0-9.]*\) frame.*/\1 \2 \3 \4 \5/' | awk '{print $1*3600+$2*60+$3+$4/1000-$5}')
# never past the quit: a frame there shows whatever is behind the game window
q=$(grep -m1 -E "GAMESEQ [0-9.]+ frame [0-9]+ quit" $C/$TAG.txt | sed 's/.*GAMESEQ \([0-9.]*\) frame.*/\1/')
for t in "$@"; do
  if [ -n "$q" ] && awk -v t=$t -v q=$q 'BEGIN{exit !(t > q - 0.3)}'; then echo "frames.sh: $t is at or past the quit ($q), skipped"; continue; fi
  v=$(awk -v t=$t -v b=$b -v s=$s 'BEGIN{print t + s - b}')
  $F -y -loglevel error -ss $v -i $C/$TAG.mp4 -frames:v 1 -vf "crop=$CROP" ${OUT}_$t.png
done
