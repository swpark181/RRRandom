---
name: feedback-dice-not-die
description: "RRRandom on-screen text always says DICE, never the singular DIE (user asked 2026-11-18)"
metadata:
  node_type: memory
  type: feedback
  originSessionId: 435a1fa3-a86b-4427-98c5-0420d02e2cbd
  modified: 2026-11-17T15:36:14.050Z
---

In RRRandom's HUD and any other on-screen English text, write DICE even for one of them ("ULTIMATE DICE!", "+1 DICE", "GIANT DICE"). The user said on 2026-11-18: "DICE 를 DIE 로 줄이지 말아줘."

**Why:** the user reads DIE as a shortening of DICE (and it also reads as the verb); they want the game's key word spelled the same everywhere.
**How to apply:** new HUD strings, comic words, kit names, page mock-ups. Code comments and developer logs may keep "die" (my interpretation: only what players see was meant). Related: [[call-them-animal-friends]].
