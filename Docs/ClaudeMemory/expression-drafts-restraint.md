---
name: expression-drafts-restraint
description: "Proposal (2026-11-23) to make the fox's fleeing face (Scared) and the tiger's walking-up face (Smug) less cartoony: 10 film cases, 4 rules, 3 drafts each; the user took my picks (fox B, tiger C); then the tiger's Startled toned down too and every expression got a muzzle part (final v7); how drafts were made"
metadata:
  node_type: memory
  type: project
  originSessionId: 01b70a0c-fd4e-460b-89a1-f962ea75fc2a
  modified: 2026-11-23T05:06:35.116Z
---

2026-11-23 (user: "여우 도망갈때 놀란 표정하고 호랑이가 걸어올 때 표정이 너무 만화 느낌이야 … 지금 다른 표정들에서 크게 벗어나지 않는
방향으로 … 일단은 인게임에 바로 넣지 말고 사례 10가지와 초안 3가지 정도 제안 페이지로"). Page
https://claude.ai/artifact/Md7fyo8H8EtUpZmYhvgado (source `C:\work\char-pipeline\output\expr_drafts\page\`). **Picked 2026-11-23: "추천안 으로 진행" = fox B, tiger C; applied to the cinematic
(see the last paragraph).**

- Which faces: the cinematic's fox expression 0 "Scared" (dot irises, rows of teeth) and tiger expression 0 "Smug" (painted black
  brows, half-shut eyes, toothy grin). The tiger's "Startled" has the same dot pupils; asked on the page whether to tone it down too.
- The user's taste, as said: cartoon-gag faces are out; expressions should sit with the other, calmer ones ("적당히").
- Rules taken from the cases (my summary, on the page): irises never shrink (wider lids, bigger pupil); nothing drawn that the face
  doesn't have (no brows on the tiger: use the lids); teeth tucked away; the body carries the emotion.
- Drafts: fox A (closed mouth) / B (small gasp, my pick) / C (strongest); tiger A (relaxed lids) / B (one-sided smirk) / C (lids
  dipping inward + one fang tip, my pick). Files `C:\work\char-pipeline\input\expr_drafts\<fox|tiger>\` (front_<X>.png edited
  fronts, T_<Animal>_Draft_<X>.png baked textures, `build_drafts.sh`), renders `output\expr_drafts\`.
- How: Tripo Image, Nano Banana Pro, 4K, 2 per job, the face front picture as the reference and "Edit this exact image … Change
  ONLY the facial expression: …" (18 free images, 546 → 564/1000, no credits). **The tiger needs the whole head in the picture**
  (`blender_face_audit.py … --span 0.72 --cz 0.68`): with a tight crop that cut the head at the edges the model re-framed the face
  and the result could not be baked back. Tiger edits were pasted back only inside the eye and muzzle ellipses
  (`blender_image_region.py`) so its stripes stay sharp.
- Results were brought out as two screen captures per picture (an overlay showing the 4K result at 2x the viewport height,
  `save_to_disk`, then `scripts\stitch_paste.ps1`), not downloaded: about 1500 px, softer than the 4K originals, which are still in
  the Tripo history (tags DRAFT_FOX_A/B/C, DRAFT2_TIGER_A/B/C). The final version should use the originals — ask the user before
  downloading files.
- Applied (same day): fox B is the new T_Fox_Expr_Scared, tiger C the new T_Tiger_Expr_Smug (old ones in
  input/expressions_new/old_1123), each with a muzzle part shaped to its mouth ([[feedback-swappable-muzzle-part]]); the
  cinematic was rebuilt (ue_cine_build.py) and rendered again: final v6 = render/FoxVsTiger_final_1080p_v6.mp4 (+ web_v6), on the page. The tiger's picture was captured again at 4x zoom (two
  captures of the face, sharper than the 2048 base picture) instead of downloading. Steps: HANDOFF, section
  "표정 다듬기 적용 + 표정별 주둥이 파츠"; input/expr_drafts/apply_picks.sh.
- Same day, the user: "나머지 표정도 파츠 만들고 호랑이 깜짝 표정도 다듬어줘" → done without a draft round: tiger
  Startled redrawn by the same rules (full-size irises, eyes a little wider, a small open mouth; Tripo tags
  STARTLED_TIGER_A/B, the first A picture, 564 → 568/1000 free images), every expression has a muzzle part, final v7
  (render/FoxVsTiger_final_1080p_v7.mp4), page version 3. Steps: HANDOFF "모든 표정에 주둥이 파츠 + 호랑이 깜짝".
  The user has not given a verdict on these yet.
- The faceted mouth corners of the fox's Grin and Joy (the 2026-11-21 front-only bake) were fixed the same day by
  repainting every mouth on its part from four views: [[mouth-redraw-four-views]] (final v8).

Related: [[cinematic-sequence-plan]], [[feedback-keep-old-face-look]], [[feedback-comparison-page]], [[tripo-midjourney-queue]].
