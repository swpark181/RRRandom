---
name: tiger-fluffy-concepts
description: "Fluffy tiger remake (fox-like fur) — concept A-2 picked, 8 directions, outfit cleanup and Tripo 3D (1st + free retry) done 2026-10-21; waiting for the user's model pick"
metadata:
  node_type: memory
  type: project
  originSessionId: a8ba2ceb-b91a-4317-9207-445fcc4a2cfb
  modified: 2026-10-21T14:12:40.056Z
---

2026-10-21 the user asked for the tiger animal friend to look fluffier, like the foxes. Concept A-2 picked
(`C:\work\char-pipeline\input\tiger_fluffy\concept\tiger_fluffy_A_1.png`).
- 8 directions + top: each view used A-2 + the tiger v3 view of that angle as references (angle, pose, tail).
  Picks `concept\tiger_fluffy_<view>.png`, sheet `C:\work\char-pipeline\output\tiger_fluffy\tiger_fluffy_turn8.png`.
- Outfit cleanup (free): `concept\tiger_fluffy_v2_<front|left|right|back>.png` (candidates in `v2_candidates\`).
- Tripo multiview 3D, Ultra, 2K, Triangle 20000, H3.1: 45 credits (22,230 -> 22,185), task
  `studio.tripo3d.ai/workspace/generate/ed0e2d04-b0ce-4b29-8bf4-b8f64bdfd1db`; 1st 19,622 faces, free retry 19,258
  faces (the retry is the current version; the 1st is in model history). 8-view captures `output\tiger_fluffy\model1|model2\`,
  sheet `output\tiger_fluffy\tiger_fluffy_3d_compare.png`. Nearly identical; retry has cleaner, symmetric eyes.
- Tripo viewer drag is not linear (short drags ignored, ~2 px/deg otherwise): check each angle before saving.

**Why:** next steps (texture cleanup, rig, game import) start from the model the user picks.
**How to apply:** continue with the animal-friend skill step 6 (Texture) then 8; see [[fox-v3-remake]].

Update 2026-10-21: the user picked the **retry** model and asked to remove the white spots on the ear backs. Exported
free as FBX (Blender, 2K) to `C:\work\char-pipeline\input\tiger_fluffy\tripo\tiger_fluffy.fbx` (+ `.fbm\`); spots
painted out with `scripts\blender_ear_back_fill.py` -> `tripo\tiger_fluffy_basecolor_noearspot.png` (before/after
render `output\tiger_fluffy\ears_after.png`). Next: texture cleanup (skill step 6) starting from that PNG, then rig.

Update 2 (same day): the user rejected the Blender ear paint ("직접한거는 이상하네") and asked for the fix in Tripo. Done:
back view edited in Tripo Image (`concept\tiger_fluffy_v3_back.png`), Texture tab multiview with that back slot,
2K, Remove Lighting, 10 credits (22,185 -> 22,175). Export: `tripo\tiger_fluffy_retex.fbx` + `tiger_fluffy_retex.fbm\`
— **this is the model to continue with**. The Blender-painted PNG was deleted; `tiger_fluffy.fbx` is the pre-fix export.
Checks: `output\tiger_fluffy\ears_retexture_compare.png`, `retex_check.png`. See [[feedback-fix-textures-in-tripo]].

Update 3 (same day): rigged in Mixamo (body without tail, 33 bones, hips 24.9) and imported over the game's Tiger
(HANDOFF 72차). Old tiger assets backed up in `C:\work\char-pipeline\input\tiger_v3\ue_backup\`. Mixamo now holds
`tiger_fluffy_body_mixamo`. Open: walk drift 60.8/s vs 71.5 computed (lower the tuning hips to ~21 if feet slide).

Update 2026-10-23 (HANDOFF round 85): the round-77 film tiger (B) was re-rigged in **Mixamo** with markers the user placed by hand (tail-cut body zip, guide `Downloads\tiger_mixamo\`) → `input/tiger_film/rig_mx/tiger_rig.fbx` (45 bones, hips 25.6), its own 20 Mixamo anims, in game. Mixamo now holds `tiger_body_mixamo` (dragon rig gone). When Mixamo keeps refusing a body, hand the user the zip + a marker guide before falling back to blender_fit_rig.py.
