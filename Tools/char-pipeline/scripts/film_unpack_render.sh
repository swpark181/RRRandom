#!/bin/sh
# Round 77 helper: unpack a Tripo export zip from Downloads into input/<sp>_film/tripo/<name>.fbx and render its 8 directions
# to output/film_models/<name>_turn8.png. Usage: film_unpack_render.sh <species> <name>
sp=$1; name=$2
cd /c/work/char-pipeline
B="/c/Program Files/Blender Foundation/Blender 5.2/blender.exe"
sh scripts/unpack_tripo.sh "$USERPROFILE/Downloads/$name.zip" input/${sp}_film/tripo $name > /dev/null || exit 1
mkdir -p output/film_models
T=C:/work/char-pipeline/input/${sp}_film/tripo
BC=$(ls input/${sp}_film/tripo/$name.fbm/ | grep -i "_basecolor." | head -1)
"$B" -b --factory-startup --python scripts/blender_texture_compare.py -- C:/work/char-pipeline/output/film_models/${name}_turn8.png $T/$name.fbx $T/$name.fbm/$BC --views turn8 --px 500 2>&1 | grep "VIEWS wrote"
"$B" -b --factory-startup --python scripts/blender_texture_compare.py -- C:/work/char-pipeline/output/film_models/${name}_extra.png $T/$name.fbx $T/$name.fbm/$BC --views under,face --px 500 2>&1 | grep "VIEWS wrote"
