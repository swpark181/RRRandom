#!/bin/sh
# Import one round-70 animal friend rigged in Mixamo into Unreal (replacing its folder):
# ue_import_mixamo_v3.sh <Name> <lowercase> <hips height> <base color path under input/> [<folder under input/, default <a>_v3>]
# Mesh = input/<a>_v3/rig_mx/<a>_rig.fbx (tail bones), animations = input/<a>_v3/mixamo/<a>_*.fbx (its own Mixamo set),
# shared Rifle Run from the dragon body rig, its own fly-back held in place, and input/<a>_v3/mixamo_extra (ultimates).
N=$1; a=$2; h=$3; tex=$4; d=${5:-${a}_v3}
cd /c/work/RRRandom
UE="/c/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"; P='C:\work\RRRandom\RRRandom.uproject'
I=C:/work/char-pipeline/input; S=C:/work/char-pipeline/scripts
run(){ timeout 900 "$UE" "$P" -run=pythonscript -dpcvars=Interchange.FeatureFlags.Import.FBX=0 "-script=$1" -unattended -nosplash -nullrhi > /dev/null 2>&1; grep -E "$2|Traceback|LogPython: Error" Saved/Logs/RRRandom.log | sed 's/^.*LogPython: //' | tail -${3:-2}; }
rm -rf Content/Characters/$N
run "$S/ue_import_animal.py $N|$I/$d/mixamo|$I/$tex|$I/$d/rig_mx/${a}_rig.fbx" "ANIMAL (tail mesh|A_${N}_Walk|DONE)" 3
run "$S/ue_import_shared_anim.py $I/shared_anims/dragon_rifle_run.fbx|Run|20.4|$N=$h" "SHAREDANIM .*A_" 1
run "$S/ue_import_shared_anim.py $I/$d/mixamo/${a}_flyback.fbx|FlyBack|$h|$N=$h" "SHAREDANIM .*A_" 1
if [ -d $I/$d/mixamo_extra ]; then
  args=""; for f in $I/$d/mixamo_extra/*.fbx; do s=$(basename $f .fbx); s=${s#${a}_}; case $s in jumpup) n=JumpUp;; frontflip) n=FrontFlip;; fallidle) n=FallIdle;; hardlanding) n=HardLanding;; roar) n=Roar;; jumpattack) n=JumpAttack;; *) continue;; esac; args="$args|$f=A_${N}_$n"; done
  run "$S/ue_import_extra_anims.py $N$args" "." 4
fi
# Round 74: FURRIM=1 adds the fur rim glow (ue_fur_rim.py) to M_<Name> — the red fox has it; a re-import without it drops it
if [ -n "$FURRIM" ]; then run "$S/ue_fur_rim.py /Game/Characters/$N/M_$N" "FURRIM" 1; fi
echo "$N anims: $(ls Content/Characters/$N/Anims | wc -l)"
