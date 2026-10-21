#!/bin/sh
# Unpack a Tripo FBX (Blender) export zip into <dir>, renamed to <name>.fbx and <name>.fbm/ (Tripo names them tripo_convert_<id>)
# Usage: unpack_tripo.sh <zip> <dir> <name>
zip=$1; dir=$2; name=$3
mkdir -p "$dir/_tmp" && unzip -o -q "$zip" -d "$dir/_tmp" || exit 1
rm -rf "$dir/$name.fbx" "$dir/$name.fbm"
mv "$dir"/_tmp/*.fbx "$dir/$name.fbx" && mv "$dir"/_tmp/*.fbm "$dir/$name.fbm" && rmdir "$dir/_tmp"
ls "$dir/$name.fbm"
