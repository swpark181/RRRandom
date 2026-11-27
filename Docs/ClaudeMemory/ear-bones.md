---
name: ear-bones
description: "Ear bones + spring + state poses for the fox, red fox, rabbit, tiger and dragon fins (2026-11-25, not committed; squirrel left out by the user); scripts, the fur weight copy, two Unreal re-import traps"
metadata:
  node_type: memory
  type: project
  originSessionId: 5627b37d-3248-4e51-8715-2d8262c6ce52
  modified: 2026-11-25T04:37:14.781Z
---

2026-11-25. After a research page of references and ten techniques (https://claude.ai/artifact/3X2QC96JHzCevvLit1Qaji,
results section at the top), the user: "다람쥐는 작업에서 제외하고 나머지는 권장하는 순서로 작업해서 적용해줘". Built and in the
game, not committed, waiting for the user's look. Details and every command in RRRandom/HANDOFF.md "귀 뼈".

- Bones: `char-pipeline/scripts/blender_ear_bones.py` (base/tip points read off `blender_head_grid.py` renders; `--head`
  skull ellipsoid, `--apart`, `--cut` when the plane+reach cut leaks: the rabbit's folded ear, the dragon's horn above its
  fin). Check with `--check` and `blender_ear_pose.py` before Unreal. Bones `Ear<L|R>1..N` + unweighted `Ear<L|R>End`.
- Fur is NOT regrown: `blender_fur_reweight.py` copies the new weights onto the old shell FBX (shape/colours identical).
- Unreal: `ue_reimport_mesh.py <Name>|<fbx>` and `<Name>|<fur fbx>|Fur`. Traps: a second armature imported into the same
  Blender scene becomes "Armature.001" and Unreal adds it as a bone (reference pose moved 38 cm) - rename to "Armature";
  a re-import adds an empty material slot and the fur component draws with the mesh's own materials (grey lumps) - fill
  every slot with `M_<Name>Fur` (new SkeletalMaterial structs; editing the read-back ones does not stick).
- Code: `URRRandomBodyAnimInstance::UpdateEars` (EarThrow 2: the user found 0.45 too faint), `EarTunings` in RRRandomSpecies.cpp, `rr.Ear*` console variables.
- Game takes end on the desktop after the game quits: the user's windows got into a still - cut clips before the quit and
  look at their last frame (`Tools/tailclips/clip.sh`). Same rule as in [[tail-chain-spring]].

- Running/vehicles (user wanted a dramatic sweep): species EarRush past walking speed + wind flutter, the driver's speed when riding; film rides with `-RRStudioVehicles` (Studio, no star cars). The lag between takes differs: always check `_last.png`.

- Fox ears felt "jelly" (bent along two bones, tip most): EarRigid 0.85 (root bone turns, ear stays straight) + EarDamping 0.55 for the fox only. Stiff-cartilage ears should pivot, not bend.

Open: squirrel, cinematic ear keys, ear/ribbon overlap limits. Related: [[tail-chain-spring]], [[fox-straight-tail-model]].
