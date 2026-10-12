@echo off
rem Runs RRRandom in game mode with the installed Unreal Engine 5.8 (no packaged build needed).
rem Options after the project: windowed 1280x720. Add -RRSolo to skip the title screen.
start "" "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "%~dp0RRRandom.uproject" -game -windowed -ResX=1280 -ResY=720
