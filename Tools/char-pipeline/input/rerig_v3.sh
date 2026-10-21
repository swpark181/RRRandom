#!/bin/sh
# Rebuild the round 69/70 rigs (fox, rabbit, squirrel, tiger) on the tiger's Mixamo rig: blender_fit_rig.py (with the
# limb reskin) + blender_animal_tail.py, with the tail settings each one was made with.
B="/c/Program Files/Blender Foundation/Blender 5.2/blender.exe"
I=C:/work/char-pipeline/input; S=C:/work/char-pipeline/scripts
rig() { a=$1; tail=$2; shift 2
  "$B" -b --factory-startup --python $S/blender_fit_rig.py -- $I/tiger/mixamo/tiger_SK.fbx $I/tiger/mixamo_obj/tiger.obj \
    $I/${a}_v3/mixamo_obj/$a.obj $I/${a}_v3/rig/joints.txt $I/${a}_v3/rig/${a}_body.fbx $I/${a}_v3/rig/check/fit 2>&1 | grep -E "FITRIG (limbs|wrote)|Error"
  "$B" -b --factory-startup --python $S/blender_animal_tail.py -- $I/${a}_v3/rig/${a}_body.fbx $I/${a}_v3/rig/${a}_rig.fbx "$tail" "$@" 2>&1 | grep "TAIL bones"
  cp $I/${a}_v3/rig/${a}_rig.fbx $I/${a}_v3/ue/${a}_SK.fbx
}
rig fox "-0.05,-0.07,0.215;-0.02,0.03,0.155;0.05,0.11,0.10;0.14,0.18,0.105;0.24,0.255,0.16" 0.16 1.3
rig rabbit "0.0,0.08,0.20;0.0,0.13,0.20;0.0,0.18,0.20" 0.07 1.0
rig squirrel "0.02,0.10,0.28;0.06,0.26,0.27;0.08,0.34,0.45;0.10,0.38,0.65;0.11,0.40,0.90" 0.34 1.15 0.03
rig tiger "-0.055,-0.01,0.27;-0.05,0.07,0.15;0.02,0.12,0.06;0.15,0.165,0.07;0.28,0.18,0.16" 0.07 1.1
