---
name: fur-mask-face-textures
description: "Every face texture carries a fur mask twin (<texture>_FurMask) that keeps shell fur off the painted eyes/mouth; made and swapped automatically by the import scripts (2026-11-23, fox and tiger); fur_chars.json; lost FurBare patch restored"
metadata:
  node_type: memory
  type: project
  originSessionId: 01b70a0c-fd4e-460b-89a1-f962ea75fc2a
  modified: 2026-11-23T02:49:44.003Z
---

2026-11-23 (user: "캐릭터 입이나 눈 위에 털이 생기지 않도록 별도의 uv맵 같은거 만들어서 얼굴 텍스쳐 갈아낄때 마다 같이 적용해줘").
Done for the fox and (second request the same day) the tiger, in the game and in the cinematic's expressions; not committed. Page https://claude.ai/artifact/CCGC6tdG2uJ8TjB5gFdDpG
(source `C:\work\char-pipeline\output\fur_mask\page\`). The user has not given a verdict yet. Full notes: HANDOFF "털 마스크".

- Standing rule from this request: whenever a face texture is swapped (game face replaced, an expression added or redrawn), its fur
  mask goes with it. The import scripts do it by themselves: `ue_import_basecolor.py`, `ue_import_animal.py`,
  `ue_import_fur_shells.py`, `RRRandom\Tools\cinematic\ue_cine_expressions.py`, all through `scripts\ue_fur_mask_lib.py`
  (starts Blender: `blender_fur_mask.py <rig> <face texture> <mask png>`). Mask = `<texture asset>_FurMask`, body UV0, 1 fur /
  0 bare / between = shorter fur; `M_<Name>Fur` reads it as texture parameter `FurMaskTex` (`FurMaskShow` 1 paints it cyan).
- Which animal friends have masks and their options: `scripts\fur_chars.json` (`rig`, `mask`, `face_fur`; now Fox and Tiger). A new
  blank-face animal friend = add it there and make its shells with `blender_fur_shells_skel.py --face-fur <same numbers>`.
- Why 3D: Tripo's atlas cuts the face into many UV islands (one eye lies across two), so growing/blurring a mask in UV space
  breaks; every texel's rest-pose surface position is used and distances are measured in 3D.
- Tiger done the same day (user: "호랑이도 여우랑 똑같이 적용해줘"): fur grown back over its face front (was `--face-bare`) and
  over the muzzle part, masks for the game face + 3 expressions. Its stripes are as dark as eye lines and its fur as white as
  teeth, so features are searched only inside three ellipses (`--only`, each eye "dc" = dark + cream, the muzzle "dp" = dark +
  pink), plus `--fill-inside`. Its eye outline is only mid grey (luma 0.53; it just looks black in lit renders) and the eye
  white is a warm white, the fur a cold white: found by measuring colours on an unlit front render, not by eye. Shells from
  `rig_mx\tiger_parts.fbx` (body + muzzle joined and welded, `--no-loose 100` keeps the whiskers bare). The muzzle's fur is the
  "Muzzle" section of SK_TigerFur: fine with the default muzzle part (same shape), but **a muzzle variant that changes shape
  outside the mouth needs its own fur or that section hidden — no code for that yet** ([[feedback-swappable-muzzle-part]]);
  the ten expression parts made 2026-11-23 only move the surface inside the mouth, where the mask leaves no fur, so they need none.
- Found on the way: the round 99 eye/clothes patch (`ue_fur_bare.py`, a node inside the fur material) had been lost on the
  squirrel, red fox and rabbit when the 2026-11-20 tidy rebuild recreated the materials. Its numbers now live in `fur_chars.json`
  (`bare`) and `ue_import_fur_shells.py` re-applies them after every rebuild. Lesson: a script that deletes and recreates a
  material drops every later patch to it — keep such patches' inputs in a config the rebuild reads.
- Checking: `scripts\fur_mask_shots.ps1 -Species Fox -Tag <name> -Show -Expr` (Studio views + each expression on the cinematic
  map through the harness's `cineface=`); "before" pictures = swap in git HEAD's `M_FoxFur`, `SK_FoxFur`, `M_FoxFur_Expr_*`.
  In a `-game` run Python cannot spawn actors and `EditorAssetLibrary` calls crash (use the asset registry).
- The cinematic's final video has the masks since v6 (now v7) (2026-11-23, [[expression-drafts-restraint]]). Open: brows are bare
  too (every painted feature is); moving pictures, 30-animal fps, online not checked; no in-game expression swap exists yet.

Related: [[fur-tidy-up-close]], [[fox-texface-model]], [[tiger-texface-model]], [[cinematic-sequence-plan]],
[[feedback-swappable-muzzle-part]], [[feedback-comparison-page]].
