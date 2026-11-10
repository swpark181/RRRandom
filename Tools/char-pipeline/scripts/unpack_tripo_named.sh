#!/bin/sh
# Round 94: unpacks every Tripo FBX export zip in Downloads whose textures are named <name>_basecolor.* (the Export
# dialog's file name) into <dir>/<name>.fbx + <name>.fbm/, and moves the zip to <dir>/zips/.
# Usage: unpack_tripo_named.sh <dir> [<name prefix, e.g. bc_>]
dir=$1; prefix=${2:-}
mkdir -p "$dir/zips"
for z in /c/Users/user/Downloads/tripo_convert_*.zip; do
  [ -f "$z" ] || continue
  name=$(unzip -l "$z" | grep -o "[A-Za-z0-9_]*_basecolor\.[A-Za-z]*" | head -1 | sed 's/_basecolor\..*//')
  case "$name" in "$prefix"*) ;; *) continue ;; esac
  sh "$(dirname "$0")/unpack_tripo.sh" "$z" "$dir" "$name" > /dev/null && mv "$z" "$dir/zips/" && echo "unpacked $name"
done
