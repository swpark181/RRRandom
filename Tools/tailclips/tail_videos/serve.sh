#!/bin/sh
# serves this folder at http://127.0.0.1:8790/ (player.html needs a real web origin: YouTube refuses embeds on file://)
cd "$(dirname "$0")" && exec "/c/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe" -m http.server 8790 --bind 127.0.0.1
