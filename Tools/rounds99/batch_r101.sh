#!/bin/bash
# Round 101: one after another, the captures for the look proposals and the checks of round 100's ultimates.
cd /c/work/RRRandom
S=Saved/Rounds99/seq.sh
# Block gloss and light, from a camera up over the street (no HUD)
bash $S lookB "9:RRGod;9.3:ShowHUD;9.5:goto=200,-2106,0;10:RRStudioCam 205 1500 620;12.5:Shot;13:RRLook 0.35 1 1 1 1 0;15.5:Shot;16:RRLook 0.18 1 1 1 1 0;18.5:Shot;19:RRLook 0.6 1.6 0.85 1.08 1.06 0;21.5:Shot;22:RRLook 0.6 2.2 0.7 1.15 1.1 -0.3;24.5:Shot;25:RRLook 0.3 1.8 0.8 1.1 1.08 -0.15;27.5:Shot;28:quit" -RRRule=Rounds -RRRoundRule=0 -RRIdleBots > /dev/null 2>&1
# The same from the player's own camera
bash $S lookC "9:RRGod;9.3:ShowHUD;9.5:goto=-1500,300,20;12:Shot;12.5:RRLook 0.35 1 1 1 1 0;15:Shot;15.5:RRLook 0.18 1 1 1 1 0;18:Shot;18.5:RRLook 0.6 1.6 0.85 1.08 1.06 0;21:Shot;21.5:RRLook 0.6 2.2 0.7 1.15 1.1 -0.3;24:Shot;24.5:RRLook 0.3 1.8 0.8 1.1 1.08 -0.15;27:Shot;27.5:quit" -RRRule=Rounds -RRRoundRule=0 -RRIdleBots > /dev/null 2>&1
# The dragon's breath: the buildings at its edge (roofs taken, a floor of wall with them)
bash $S dragC "9:RRGod;9.5:goto=200,-2106,0;11:RRUltimate;11.3:RRRoll;15.5:RRStrike 1500;19:Shot showui;21:Shot showui;22:RRStudioCam 180 1700 900;24:Shot showui;25:quit" -RRSpecies=Dragon -RRRule=Rounds -RRRoundRule=0 -RRIdleBots > /dev/null 2>&1
# The fox's clones
bash $S foxC "9:RRGod;9.5:goto=1100,0,0;11:RRUltimate;11.3:RRRoll;13:Shot showui;15:Shot showui;17:Shot showui;19.5:Shot showui;21.5:Shot showui;22:quit" -RRSpecies=RedFox -RRRule=Rounds -RRRoundRule=0 > /dev/null 2>&1
# The squirrel's acorn rain
bash $S acornC "9:RRGod;9.5:goto=200,-2106,0;11:RRUltimate;11.3:RRRoll;13.5:RRStrike 1100;14.6:Shot showui;15.4:Shot showui;16.4:Shot showui;17.5:Shot showui;18.5:quit" -RRSpecies=Squirrel -RRRule=Rounds -RRRoundRule=0 -RRIdleBots > /dev/null 2>&1
# Knocked out among the bots: the kill camera
bash $S killC "9:goto=1300,0,180;$(awk 'BEGIN{for(t=11;t<40;t+=1.5) printf "%.1f:Shot showui;", t}')41:quit" -RRSpecies=Rabbit -RRRule=Rounds -RRRoundRule=0 > /dev/null 2>&1
for t in lookB lookC dragC foxC acornC killC; do echo "$t: $(ls Saved/Rounds99/shots/${t}_*.png 2>/dev/null | wc -l) shots"; done
grep -h 'look test' Saved/Rounds99/seq_lookB.log | cut -c40-160 | head -3
grep -h 'splits into\|knocked out\|dragon breath hits\|acorn rain\|wreck .* made something big' Saved/Rounds99/seq_foxC.log Saved/Rounds99/seq_acornC.log Saved/Rounds99/seq_dragC.log | cut -c40-200 | head -20
grep -h 'knocked out' Saved/Rounds99/seq_killC.log | cut -c40-200 | head -6
