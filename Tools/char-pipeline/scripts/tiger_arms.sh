#!/bin/bash
# usage: tiger_arms.sh [names, comma separated]   (round 101: blender_tiger_shield_arms.py + its sheets)
OUT=C:/work/char-pipeline/input/tiger_r101_tripo/arms
"/c/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python /c/work/char-pipeline/scripts/blender_tiger_shield_arms.py -- \
  "C:/work/char-pipeline/input/tiger_film/mixamo/tiger_SK.fbx" "C:/work/char-pipeline/input/tiger_r101_tripo/split" "$OUT" $1 2>&1 | grep 'ARMS\|Error\|Traceback\|line ' | cut -c1-220
F=/c/work/char-pipeline/tools/ffmpeg/ffmpeg.exe
cd /c/work/char-pipeline/input/tiger_r101_tripo/arms || exit 1
for n in shield_carry shield_block hammer1 hammer2 hammer3; do
  [ -f _${n}_front_0.png ] || continue
  "$F" -nostdin -y -loglevel error -i _${n}_front_%d.png -filter_complex "tile=8x1" ${n}_front.png < /dev/null
  "$F" -nostdin -y -loglevel error -i _${n}_side_%d.png -filter_complex "tile=8x1" ${n}_side.png < /dev/null
  "$F" -nostdin -y -loglevel error -i ${n}_front.png -i ${n}_side.png -filter_complex "vstack,scale=1920:-1" ${n}_sheet.jpg < /dev/null
  rm -f _${n}_*.png ${n}_front.png ${n}_side.png
  echo "sheet $n"
done
