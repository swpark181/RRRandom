---
name: mixamo-one-character
description: Mixamo keeps only the latest uploaded character; download all its animations before uploading the next
metadata:
  node_type: memory
  type: project
  originSessionId: 0123e4a7-8e0e-4fcf-a13b-ccf84f62a294
  modified: 2026-10-15T03:24:05.544Z
---

Mixamo (user's account) keeps only one uploaded character: uploading a new one discards the previous rig. As of 2026-10-15 the
stored one is `dragon_body_mixamo` (the dragon with tail and wings cut off; fox, rabbits, squirrel, tiger rigs are gone;
their exports are in C:\work\char-pipeline\input\<animal>\mixamo). The fox can be re-uploaded from
`C:\Users\user\Downloads\fox_mixamo_upload.zip`, but the new rig may differ slightly from the one in the game.

**Why:** RRRandom's brawlers each need ~17 Mixamo exports on their own rig; losing the rig means re-rigging.
**How to apply:** finish all exports for a character (run `C:\work\char-pipeline\scripts\mixamo_export.js` in the
Mixamo page, then `__runAll('<prefix>')` — all 18 product ids are in it) before uploading another; pick "2 Chain Fingers (41)"
so bone names match the fox. A tail lying by the feet or wings behind the arms confuse the auto-rigger (dragon: foot bone
13 cm off) — rig the body alone (blender_strip_parts.py → blender_merge_parts.py). See [[tripo-cartoon-style]].
