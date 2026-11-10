---
name: animation-studio-map
description: "RRRandom animation work is checked on the bare \"Studio\" test map (-RRMap=Studio) with RRStudioCam side views, not on the real maps"
metadata:
  node_type: memory
  type: feedback
  originSessionId: 6e594476-cbbe-482f-bb38-cde3146beee7
  modified: 2026-10-20T07:41:07.385Z
---

When working on RRRandom animations (walk cycles, attack motions, effects), test them on the obstacle-free Studio map:
`-RRMap=Studio` (third person, grid floor, enemy bots idle, only the star cars), with `RRStudioCam <yaw>` (0 front, 90
right side, 180 back), `RRSlomo 0.25`, and `RRRobotFire` for the star robot. Built in round 61 (2026-10-20).

**Why:** the user asked for it (2026-10-20) after robot-walk captures on Dice Land kept getting blocked by props and
behind-the-back camera angles.
**How to apply:** for any motion check, launch the Studio map first and capture from a fixed side camera; use the real
maps only to confirm it in context. Related: [[pastel-ui-direction]].
