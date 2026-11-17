@echo off
rem Round 101: joins a friend's game by its address, without the room search (which only finds games on the same network).
rem The host opens a game from the title (NETWORK - HOST GAME) and tells you its address; its UDP port 7777 must be reachable.
set /p RRIP=Host address (for example 192.168.0.12 or 203.0.113.7):
set /p RRWHO=Animal friend (Fox, Tiger, Rabbit, Squirrel, Dragon, RedFox) [Fox]:
if "%RRWHO%"=="" set RRWHO=Fox
start "" "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "%~dp0RRRandom.uproject" -game -windowed -ResX=1280 -ResY=720 -RRLan -RRJoinIP=%RRIP% -RRSpecies=%RRWHO%
