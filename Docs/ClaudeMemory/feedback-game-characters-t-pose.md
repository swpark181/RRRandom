---
name: feedback-game-characters-t-pose
description: "Every game character (animal friend etc.) is generated in a T-pose from 2026-11-19 on: concepts that go to 3D and the Tripo model itself; no more relaxed A-pose"
metadata:
  node_type: memory
  type: feedback
  originSessionId: 8157475a-393c-4743-a078-f51a7701b693
  modified: 2026-11-19T12:13:26.912Z
---

From now on, when a character for the game is generated, it is always in a **T-pose** (arms straight out sideways at
shoulder height, clear space under the arms). User, 2026-11-19, in the middle of the fox texture-face work:
"이제 게임용 캐릭터 뽑을 때 무조건 t자 포즈로 해줘. 캐릭터 만드는 스킬에도 해당 내용 기록해줘."

**Why:** the user did not say; my reading is rigging — A-pose arms glued to the body made Mixamo move the arms with the
torso (rabbit 2026-10-27, dragon round 85), and the T-pose rabbit rigged cleanly.

**How to apply:** the concept images that feed Tripo 3D (front/left/right/back) and the 3D model are T-pose; replace
"relaxed A-pose" in the old prompt templates. The `animal-friend` skill (section 0 and the prompt templates) was updated
the same day. T-pose prompt that worked: `C:\work\RRRandom\Tools\char-pipeline\input\rabbit_smart_tpose\README.md`.
My interpretation: mood-only first concepts that never go to 3D may stay in any pose, but anything headed to modeling is
redrawn in T-pose. Related: [[feedback-tripo-always-smart-mesh]], [[character-eight-directions]], [[mixamo-one-character]].
