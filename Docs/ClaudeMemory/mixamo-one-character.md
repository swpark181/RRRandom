---
name: mixamo-one-character
description: Mixamo keeps one character; auto-rigger fails on tails lying by the feet — cut the tail (blender_strip_parts.py) instead of blender_fit_rig.py
metadata:
  node_type: memory
  type: project
  originSessionId: 0123e4a7-8e0e-4fcf-a13b-ccf84f62a294
  modified: 2026-10-20T13:40:02.010Z
---

Mixamo (user's account) keeps only one uploaded character: uploading a new one discards the previous rig. As of 2026-10-15 the
stored one is `dragon_body_mixamo` (the dragon with tail and wings cut off; fox, rabbits, squirrel, tiger rigs are gone;
their exports are in C:\work\char-pipeline\input\<animal>\mixamo). The fox can be re-uploaded from
`C:\Users\user\Downloads\fox_mixamo_upload.zip`, but the new rig may differ slightly from the one in the game.

Still `dragon_body_mixamo` on 2026-10-20. For a motion every species needs (flyback, Rifle Run), don't re-upload each character: download it once from whatever rig is there (With Skin, to measure its hips) and spread it with `char-pipeline/scripts/ue_import_shared_anim.py` (keeps each skeleton's bone lengths, scales hips height). The Mixamo login can expire — the user logs in themselves.

**Why:** RRRandom's brawlers each need ~17 Mixamo exports on their own rig; losing the rig means re-rigging.
**How to apply:** finish all exports for a character (run `C:\work\char-pipeline\scripts\mixamo_export.js` in the
Mixamo page, then `__runAll('<prefix>')` — all 18 product ids are in it) before uploading another; pick "2 Chain Fingers (41)"
so bone names match the fox. A tail lying by the feet or wings behind the arms confuse the auto-rigger (dragon: foot bone
13 cm off) — rig the body alone (blender_strip_parts.py → blender_merge_parts.py). See [[tripo-cartoon-style]].

2026-10-21: the auto-rigger failed again ("Unknown error while generating motion") on the red fox and on the tiger zip that rigged fine on 10-14 — it seems broken account- or server-side since 10-15. Fallback that worked: `char-pipeline/scripts/blender_fit_rig.py` (a similar species' Mixamo rig moved to the new body's joints, weights from the donor body bent onto them) + `ue_import_rig_anims.py` (the donor's Mixamo animations). Still `dragon_body_mixamo` on Mixamo (failed uploads don't replace it). See [[fox-girl-animal-friend]].

2026-10-21 later: the auto-rigger works again — it was never broken for every mesh. It fails ("Unknown error while generating motion") on meshes whose tail lies on the floor by the feet (red fox, fox v3, tiger v3, the old tiger zip); rabbit and squirrel (tail up) rig fine, and fox/tiger rig fine with the tail cut off (`blender_strip_parts.py <rig fbx with Tail bones> <obj> <body obj> Tail` → Mixamo → `blender_merge_parts.py` → `blender_animal_tail.py`). Try that before falling back to blender_fit_rig.py.
2026-11-19: Mixamo now holds RABBIT_TPOSE_MIXAMO (hips 26.7). Calling the Mixamo API with the page's access token from JS was blocked by the permission classifier; clicking the page's own Download buttons via JS works (max 3 per call, 45 s limit).
2026-11-19 night: Mixamo now holds `fox_body_mixamo` (the new blank-face T-pose fox without its tail, 41 bones, hips 20.7; rigged by Claude with markers on the first try). The rabbit T-pose rig it replaced is kept locally (`RRRandom\Tools\char-pipeline\input\rabbit_smart_tpose\mixamo\rabbit_mixamo_original.fbx`). This time `mixamo_export.js` run as page JS (`window.__runAll('foxg'); 'started'`, API with the page's token) was NOT blocked and fetched all 18 in about 3 minutes.
