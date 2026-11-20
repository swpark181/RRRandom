---
name: fox-texface-model
description: "Blank-face T-pose Smart Mesh fox for texture expressions; in the game as Fox since 2026-11-19; face = old fox's eyes/brows/mouth carried over (2026-11-20); pages, files, what failed; muzzle-swap and talking tests"
metadata:
  node_type: memory
  type: project
  originSessionId: 8157475a-393c-4743-a078-f51a7701b693
  modified: 2026-11-19T12:40:59.639Z
---

2026-11-19 (user: "3d 모델링에 얼굴을 텍스쳐로 표현하는 게임을 10가지 찾아보고 참고해서 지금 여우 캐릭터의 얼굴을 텍스쳐 표정에
최적화 … 과정을 페이지로 … 여우 캐릭터 얼굴 모델링은 트라이포 단계 까지만"). Done up to Tripo; **not rigged, not in the game,
the cinematic still uses the old fox** (`fox_film_b2` + the Blender face patch of the same day).

- Page: https://claude.ai/artifact/6QufAZMWFDzDB4Y9cdRn2n (source `C:\work\char-pipeline\output\fox_texface\page\`): 10 games
  (Animal Crossing, Toontown, Pokémon, Bluey's Happy Snaps, Wind Waker, Ocarina, Mario 64, Mega Man Legends, Mii, Splatoon),
  rules, before/after, expression test. No game screenshots on it (other companies' images), links only.
- Final model: `C:\work\char-pipeline\input\fox_texface\tripo\fox_tpose_final.fbx` (+ .fbm, 4K basecolor), Tripo task
  233d8062-33ba-440c-bdeb-ccfe7e895754, Smart Mesh, 21,487 faces, **T-pose, face has no eyes/brows/mouth, only the nose**.
  Earlier attempts kept: `fox_texface_try1.fbx` (A-pose, textured, task 6867861d), `fox_tpose_mesh.fbx` (T-pose 1, mesh
  only, eye pads dished in, dropped, task e54154d2).
- Concepts: `input\fox_texface\concept\fox_texface_<dir>.png` (A-pose, 8 dirs + top), `concept_tpose\fox_tpose_front|left|right|back.png`
  (3D inputs; first T set with eye-pad ovals in `concept_tpose\v1_pads\`).
- Expression test on the final model (5: neutral, grin, scared, determined, blink): pictures `input\fox_texface\expr\cand_final\`,
  baked textures `expr\T_final_<name>.png`, renders `output\fox_texface\expr\`.
- Cost: 360 credits (20,500 → 20,140): 130 A-pose + 100 T-pose 1 mesh + 130 T-pose 2. Free images 426 → 486/1000.
- Left open, told to the user: muzzle not shortened (image edits refused twice), faint arch ridge over each eye area, ear-side
  spikes (Smart Mesh; `blender_shave_spikes.py` later), far eye bends in 3/4 (single front projection). Next steps when asked:
  Mixamo rig, face UV channel + expression atlas material, Unreal.
- How-to and pitfalls are in the animal-friend skill, section 9. Related: [[feedback-game-characters-t-pose]],
  [[cinematic-sequence-plan]], [[fox-v3-remake]], [[feedback-tripo-always-smart-mesh]].

Muzzle swap proposal (2026-11-19, user: "툰타운 주둥이 모델 관련해서 여우 캐릭터에 어떻게 적용하면 좋을지 페이지로 설명"): page
https://claude.ai/artifact/CD9sabePuKahQrohxaJDxU (source `C:\work\char-pipeline\output\fox_muzzle\page\`). Explanation + mockups only,
**no muzzle pieces were built**. Proposal: eyes/brows/closed mouths stay textures; 3 open-mouth muzzle pieces (laugh, "oh", shout) as
static meshes on the head bone, picked by one integer (like `ARRRandomCineAnimal.Expression`); cut = sphere behind the nose tip, below
the eye pictures, behind the mouth corners (692 faces, rim 78 edges on `fox_tpose_final`; preview script
`scripts\blender_muzzle_split.py --r 0.135 --back 0.10 --down 0.04 --zmin 0.475 --zmax 0.612`). Mockup pictures (Nano Banana edits of
the final model's renders) in `input\fox_muzzle\mock\`. Free images now 498/1000. Waiting for the user's decisions: how many pieces
(I suggested laugh first, 130 credits), cinematic only or emotes too; rigging comes before the pieces.

Later wish (user, 2026-11-19): "나중에 만화적인 표현으로 입 텍스쳐만 바꿔가면서 말하는 느낌" — cartoon talking by flipping mouth textures only.
I answered yes: 3-4 mouth frames on a separate mouth layer (needs the face UV channel + layered material, not whole-texture swaps),
works on the closed muzzle piece; weak in pure side view because the jaw outline does not move. Not started.
Talking test done 2026-11-19 (user: "응 GIF로 확인해줘"): page https://claude.ai/artifact/BXLrqY5dxifz25tdNaX3r3, GIF
`C:\work\char-pipeline\output\fox_talk\fox_talk.gif` (4 mouth frames closed/eh/ah/oo at 10 fps; front, 3/4, side). Reads from the front
and 3/4, weak from the side. Finding: a mouth projected from straight ahead smears along the muzzle's underside — render the muzzle from
front-below (`scripts\blender_view_render.py --pitch 38`), paint the mouth there, bake with that camera; paste only the mouth ellipse onto
the base picture first (`scripts\blender_image_region.py`) so the eyes don't jitter. Textures `input\fox_talk\T_talk2_<eh|ah|oo>.png`.
Free images 510/1000. Not in the game; still needs the mouth-only UV layer + material.

**IN THE GAME since 2026-11-19 night** (user: "새로 뽑은 여우 모델링으로 기존 여우 캐릭터 대체해줘 … 내일 확인할 수 있도록"): Fox = `input\fox_game\rig_mx\fox_rig.fbx`
(the `fox_tpose_final` mesh, own Mixamo rig 41 bones + Tail1-4, hips 20.7), texture `input\fox_game\face\T_Fox_face.png` (eyes/brows projected
from the front, closed smile from front-below), 49 animations (base 19 re-downloaded from Mixamo for the new rig; emotes, sword, cine re-imported
with hips 20.7), shell fur rebuilt (`--orange-g 0.27 --orange-r 0.70`), Tunings `{Fox, 1.8, 20.7, …, FRotator(29.0, -82.0, 89.7)}`, new cheat `RRAim`.
Old assets backed up in `input\fox_game\ue_backup_fox_film\`. Not committed. Check page https://claude.ai/artifact/5BQA2bSHgdNY2V3eo7dacc (source `outputox_gamepage`),
captures `output\screens\fox_game\`. **Open:** the cinematic (Content\Cinematics\FoxVsTiger: face-mask meshes, expression materials) still targets the
OLD fox and needs a rebuild on the new one (user to decide); portrait and title art are the old drawing; motorbike/kickboard/plane rides, giant,
online, fps not checked. Full runbook = animal-friend skill section 10.

**Face redone 2026-11-20** (user: "얼굴 텍스쳐가 이전 느낌하고 너무 변했어. 기존 느낌을 살리는 쪽으로 최대한 맞춰봐"): the 11-19 game face was a
Nano Banana redraw (cartoon, thick outlines). Now `input\fox_game\face\T_Fox_face.png` = the OLD fox's own eyes, brows and mouth moved over
from its texture (`scripts\blender_feature_transfer.py`, all from the front view; work files `input\fox_game\face2\`); the 11-19 one is kept
as `face\T_Fox_face_cartoon_1119.png`, its fur in `output\fox_game\fur_1119\`. In Unreal only T_Fox_BaseColor (`ue_import_basecolor.py`) and the
shell fur were redone (fur grew over the orange irises until `blender_fur_shells_skel.py` got `--eye-r 0.05`). Check page updated (same link).
The 11-19 expression set (`T_final_*`) and talk mouths (`T_talk2_*`) are in the cartoon style and would need redoing to match. No credits used.
See [[feedback-keep-old-face-look]].

Second pass the same day (user: "눈과 눈 사이에 기존에 있던 굴곡을 조금 살려야 더 자연스러울 것 같다. 그리고 입 끝쪽에 끊기는 느낌을 보완해줘"):
a low nose bridge between the eyes (1% of the height, 0.6% valleys at the inner eye corners; the eye areas themselves stay flat) put into the
game rig with `scripts\blender_face_relief_transfer.py --bridge …` (mesh cut finer there: 23,127 faces; previous rig kept as
`rig_mx\fox_rig_flat_1119.fbx`), and the mouth line joined at both corners with `scripts\blender_stroke.py` on the from-below render
(previous texture `face\T_Fox_face_1120a.png`). Unreal: `ue_reimport_mesh.py` (mesh only, same skeleton), base colour, fur rebuilt.
Checked in Blender and on the Studio map (backlit only). Exact commands: animal-friend skill section 10 step 1, HANDOFF top entry.

Third pass the same day (user: "지금 코에 들어간 굴곡은 어색해. 기존 여우처럼 부드러운 굴곡이 필요한데. 얼굴쪽 모델링만 트라이포로 다시 그려서 적용할 수 있을까?"):
the numeric nose bridge was rejected as awkward. Now the game rig's face region carries the face SHAPE of the unused Tripo T-pose mesh 1
(`input\fox_texface\tripo\fox_tpose_mesh.fbx`: shallow eye dishes, broad soft bridge), moved over as depth with
`blender_face_relief_transfer.py … --amount 1.6 --smooth 2 --cuts 1` (23,547 faces; 0 credits, nothing newly generated). Tripo cannot
regenerate part of a mesh; a fresh face would be a whole new Smart Mesh (100 credits, mesh only) used the same way. Rig backups:
`rig_mx\fox_rig_flat_1119.fbx` (flat), `fox_rig_ridge_1120b.fbx` (rejected ridge). Waiting for the user's verdict on this shape.
