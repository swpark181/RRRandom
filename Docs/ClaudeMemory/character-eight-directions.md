---
name: character-eight-directions
description: "RRRandom characters — turnaround concept art is always 8 directions (not 4), and later steps check against it (2026-10-21)"
metadata:
  node_type: memory
  type: feedback
  originSessionId: 4bf90f0d-da8d-4280-9a74-c8c0cac82a27
  modified: 2026-10-21T01:24:18.414Z
---

From 2026-10-21 the user wants every character's concept art made in **8 directions** (front, front-left 3/4, left,
back-left 3/4, back, back-right 3/4, right, front-right 3/4; plus a top view for the top-down game) before going on to
cleanup, Tripo 3D and texture work. The fox girl was done with 4 views (front/side/back/top) before this rule.

**Why:** the user said "앞으로 캐릭터는 8방향에서 시안을 뽑고 작업을 진행해줘" right after the fox girl was finished with 4 views.
**How to apply:** follow section 3 of the `animal-friend` skill; feed Front/Left/Right/Back into Tripo multiview, and use
the 3/4 views to check the model (`blender_texture_compare.py --views turn8`). See [[fox-girl-animal-friend]].
