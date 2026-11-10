---
name: feedback-props-keep-proportions
description: "RRRandom props/cover models are never stretched to fit cells; uniform scale only, may overhang cells slightly (2026-11-10)"
metadata:
  node_type: memory
  type: feedback
  originSessionId: 088b3322-cd63-437c-9f6c-6ea30ee6446a
  modified: 2026-11-10T12:12:35.483Z
---

Never scale a placed model non-uniformly to fit its block/cells. Scale it evenly (cover models: to the block's height), line up as many as its own width allows, and let it stick out a little past its cells; collision follows the cells it actually stands on.

**Why:** the user saw odd sizes/ratios (vending squeezed to 0.6x, tables 2.7x deep) and said "칸에 맞출때 오브젝트의 배율을 조절하지 않아야 한다. 칸보다 오브젝트가 살짝 커도 되도록".
**How to apply:** any new prop placement in Dice Town/battle royale; check sizes with the RRPropBeside N cheat next to the fox (comparison page convention: [[feedback-comparison-page]]). See [[battle-royale-plan]].
