---
name: feedback-keep-old-face-look
description: "Remaking an existing character's model — keep the old face look; carry the old eye/brow/mouth texture over instead of redrawing (user 2026-11-20)"
metadata:
  node_type: memory
  type: feedback
  originSessionId: 8157475a-393c-4743-a078-f51a7701b693
  modified: 2026-11-20T01:01:53.411Z
---

When an animal friend that is already in the game gets a new model, its face must keep the old character's look.
User, 2026-11-20, after the blank-face fox went into the game with a face redrawn by Nano Banana (thick black eye
outlines, amber irises, a wide thick smile): "얼굴 텍스쳐가 이전 느낌하고 너무 변했어. 기존 느낌을 살리는 쪽으로 최대한 맞춰봐."

**Why:** the image model's redraw came out as a flat cartoon style and changed the character's impression, even though
the prompt asked for "the same style as the reference".

**How to apply:** do not redraw the face of an existing character. Render the old model and the new blank-face model
from the same front view and move the old eye, brow and mouth pixels over (`scripts\blender_feature_transfer.py`, then
`blender_expression.py bake`); steps and the fox's numbers are in the animal-friend skill, section 10 step 1. Show an
old / new face comparison before calling a replacement done. My reading (not said by the user): this covers the facial
features; they did not comment on the new model's fur colour or body. See [[fox-texface-model]].
