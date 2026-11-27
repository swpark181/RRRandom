---
name: fox-straight-tail-model
description: "The fox generated again with a straight tail (2026-11-24), in the game on the OLD skeleton without Mixamo; cinematic not converted yet; files, credits, how the rig and face were carried over"
metadata:
  type: project
---

2026-11-24 (user: "비용이 크더라도 여우 모델링을 다시 뽑아야 할 것 같다 ... 여우만 일단 새로 작업해줘"), after four tail
repairs on the old model were not good enough ([[tail-chain-spring]]). In the game as Fox since that evening, not committed.

- Concepts: the old T-pose inputs (`input\fox_texface\concept_tpose\`) with ONLY the tail redrawn straight back
  (Nano Banana Pro edits, 10 free images, 600/1000 used). Sheet: `input\fox_straight\concept\fox_straight_sheet.png`.
- Model: Smart Mesh multiview, 20,617 faces, 130 credits (19,620 left), Tripo task 66496f2a-d799-4082-a794-9b79496d4e64,
  `input\fox_straight\tripo\fox_straight.fbx`.
- Rig WITHOUT Mixamo: `scripts\blender_reskin.py` fits the new mesh onto the old body (1 cm), copies the old weights,
  lays Tail1-8 along the straight tail. Same 49-bone skeleton, so the 49 animations, FoxMM motion matching and the
  species numbers were not touched. `input\fox_straight\rig\fox_straight_rig.fbx` (+ `_parts`, `_muzzle`, `_body`).
- Face: old eyes, brows and mouth moved over in the front render and baked from the front
  (`blender_feature_transfer.py` + `blender_expression.py bake`; numbers in HANDOFF). The surface-to-surface bake
  (`blender_face_bake.py`) distorted the eyes - only for meshes of the same shape.
- Old game assets: `input\fox_straight\ue_backup_before_1124\`; older states in `input\fox_game\ue_backup_*`.
- Idle tail (user the same evening: "아이들 상태일때 꼬리를 기존처럼 살짝 들어서 한쪽으로 말고 있으면 좋을것 같다"): the
  model stays straight; standing still the code curls the tail right and lifts it (`TailIdleCurl` 120, `TailIdleLift` 22
  in the species table, my pick of five; `rr.Tail.IdleCurl` / `rr.Tail.IdleLift` to try others). Then "끝을 조금 더
  들어올리고 싶다. 이전 말린 느낌이 좋았어": `TailIdleTipLift` 80 (`rr.Tail.IdleTipLift`), the far end turned up; a bone now lifts
  about its own side axis, not the mesh's. This is the pattern
  for the rule's "애니메이션과 스프링으로 유지": pose in code, not in the model.
- Size (2026-11-25, user: "예전엔 꼬리가 더 컷던거같은데"): true, my redraw prompt said half her height. The tail was
  46 cm / 9.6 cm radius against the old 65 / 11.5; scaled in the rig step to 64 / 11.6
  (`blender_reskin.py --tail-long 1.4 --tail-fat 0.2,0.4`). When a part is redrawn, measure the old one and compare
  before showing it.
- **Open:** the cinematic still expects the old mesh (7 expression textures, 5 expression muzzle parts, cinematic fur) and
  is broken until converted - offered to the user, waiting for their look at the new model first. Mouth line faint on
  the muzzle's sides. Portrait/title art unchanged. Comparison page: the tail page, section "8판"
  (https://claude.ai/artifact/MT1Vr8JV8x8RrJa7CcJ58z).

Lesson: when a character is generated again from the same concept pictures, keep the old skeleton and transfer the skin
(`blender_reskin.py`) instead of going back through Mixamo - nothing downstream has to be redone. In Tripo's image
history, pick result images by file name `image_N.png`; `input.png` thumbnails are also full size.
Related: [[fox-texface-model]], [[feedback-keep-old-face-look]], [[mixamo-one-character]].
