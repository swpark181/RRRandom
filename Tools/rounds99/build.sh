#!/bin/bash
# Round 99: builds the editor target; prints errors/warnings and the last lines
"/c/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" RRRandomEditor Win64 Development "-Project=C:\work\RRRandom\RRRandom.uproject" -WaitMutex -NoHotReloadFromIDE > /c/work/RRRandom/Saved/Rounds99/build.log 2>&1
echo "EXIT $?"
grep -E 'error|warning C|Error' /c/work/RRRandom/Saved/Rounds99/build.log | head -40
tail -4 /c/work/RRRandom/Saved/Rounds99/build.log
