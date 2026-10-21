#!/bin/sh
# Re-import the round 69/70 animal friends (fox, rabbit, squirrel, tiger) from input/<a>_v3/ue/<a>_SK.fbx into Unreal:
# mesh + material, the tiger's Mixamo set, the shared run and fly-back, and the rabbit's and tiger's ultimate motions.
cd /c/work/RRRandom
UE="/c/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"; P='C:\work\RRRandom\RRRandom.uproject'
I=C:/work/char-pipeline/input; S=C:/work/char-pipeline/scripts
run(){ timeout 900 "$UE" "$P" -run=pythonscript -dpcvars=Interchange.FeatureFlags.Import.FBX=0 "-script=$1" -unattended -nosplash -nullrhi > /dev/null 2>&1; grep -E "$2|Traceback|LogPython: Error" Saved/Logs/RRRandom.log | sed 's/^.*LogPython: //' | tail -${3:-2}; }
for spec in "Fox|fox|24|fox_v3.fbm/fox_v3_basecolor.JPEG" "Rabbit|rabbit|15|rabbit_v3_try2.fbm/rabbit_v3_try2_basecolor.JPEG" "Squirrel|squirrel|24|squirrel_v3_try2.fbm/squirrel_v3_try2_basecolor.JPEG" "Tiger|tiger|25|tiger_v3_try2.fbm/tiger_v3_try2_basecolor.JPEG"; do
  IFS='|' read N a h tex <<E2
$spec
E2
  echo "== $N"; rm -rf Content/Characters/$N
  run "$S/ue_import_animal.py $N|$I/${a}_v3/ue|$I/${a}_v3/tripo/$tex" "ANIMAL (assets|DONE)" 1
  run "$S/ue_import_rig_anims.py $N|$h|$I/tiger/mixamo|tiger|26.3" "RIGANIMS (DONE)" 1
  run "$S/ue_import_shared_anim.py $I/shared_anims/dragon_rifle_run.fbx|Run|20.4|$N=$h" "SHAREDANIM .*A_" 1
  run "$S/ue_import_shared_anim.py $I/tiger/mixamo/tiger_flyback.fbx|FlyBack|26.3|$N=$h" "SHAREDANIM .*A_" 1
done
run "$S/ue_import_rig_anims.py Rabbit|15|$I/rabbit/mixamo_extra|rabbit|17.5|jumpup=JumpUp|frontflip=FrontFlip|fallidle=FallIdle|hardlanding=HardLanding" "RIGANIMS DONE" 1
run "$S/ue_import_rig_anims.py Tiger|25|$I/tiger/mixamo|tiger|26.3|roar=Roar|jumpattack=JumpAttack" "RIGANIMS DONE" 1
for N in Fox Rabbit Squirrel Tiger; do echo "$N $(ls Content/Characters/$N/Anims | wc -l)"; done
