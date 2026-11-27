---
name: fur-tidy-up-close
description: "Shell fur tidies itself up close on every furred animal friend (2026-11-20): Tidy* material parameters, per-animal face ellipsoid, how to place it and how to see the cat and rat"
metadata:
  node_type: memory
  type: project
  originSessionId: 5aee74b3-a9cc-4e63-9e82-a9a43d8c5f06
  modified: 2026-11-23T01:34:18.725Z
---

2026-11-20 (user: "여우 얼굴을 가까이서 볼 때 털이 너무 듬성듬성한것 처럼 보이는데 가까워질수록 외곽털은 그대로 비교적 얼굴 안쪽 털들은
정돈되게 보이게 해줘", then "나머지 동물도 똑같이 처리해줘"). Done in the fur materials only, for all seven furred ones (fox, rabbit,
squirrel, tiger, red fox, cat, rat; the dragon has no fur), not committed. Page https://claude.ai/artifact/T7MBGysfir79NWG675Ds5L
(source `C:\work\char-pipeline\output\fox_game\fur_tidy\page\`, animal tabs). The user has not given a verdict yet.

- `M_<Name>Fur` (built by `char-pipeline\scripts\ue_import_fur_shells.py`) has a `FurTidy` amount = near the camera × max(facing the
  camera × 0.55, inside the face ellipsoid). Tidy = long strands shorter and thinner + a short fine undercoat between them. The
  silhouette (fur seen from the side) is untouched. `TidyNear` 90 / `TidyFar` 320 cm, `TidyLength` 0.4, `TidyFacing` 0.55;
  `TidyNear` = `TidyFar` = -1 turns it off. Face ellipsoids ("x,y,z,rx,ry,rz", mesh space, REFERENCE pose): fox 10,40,60,23,17,16 ·
  rabbit -0.3,17,61.7,16.7,12.3,11.7 · squirrel 0,22.2,58.7,22.1,16.3,15.5 · tiger 12.2,37.4,66,30,22,21 · red fox
  0.3,23.6,53,25,18.5,16 · cat 0,21,64,22,16,15 · rat 0,22,58,22,16,16. Full notes: HANDOFF "털 — 가까이서 얼굴 안쪽만 정돈".
- Material-only rebuild: `ue_import_fur_shells.py "Fox|||"` (keeps tuned values and the face ellipsoid), several at once with
  `ue_import_fur_shells_batch.py "Rabbit|||;Tiger||||<face>"`.
- The tiger barely changes: its face front has no fur (`--face-bare`).
- Not done: 30-animal fps. The cinematic's fur copies (`M_FoxFur_Expr_*`, `M_TigerFur_Expr_*`) got the tidy on 2026-11-23, when
  `ue_cine_expressions.py` was run again for the fur masks ([[fur-mask-face-textures]]); the final video was not rendered again.
- 2026-11-23: a material-only rebuild also re-applies the eye/clothes patch from `fur_chars.json` and wires the fur mask.
- My reading, not the user's words: "얼굴 안쪽" = muzzle, mouth, between the eyes; cheeks keep some grain on purpose (fully tidy
  looked like clay).

**Testing fur values in the game** (`ue_game_seq.py`): `facecam=<dist>,<yaw>`, `furparam=<name>,<value>,...`, `furvec=`,
`faceguess`, after `RRSlomo 0.01` so before/after frames match. The fur mesh has 6 material slots with the game's own instances on
each: values must go on every slot (`set_scalar_parameter_value_on_materials`) — setting slot 0 alone changed almost nothing and
cost an hour.
**Seeing the cat and rat:** `-RRSpecies=Cat` silently gives a fox (only the 6 playable names parse). Use `-RRRule=Defense`,
`RRWave 7`, slow down at ~34 s (the cat boss arrives ~30 s), then `target=SK_Cat` and `tplace=0,1400,2500,180` (lifts it into open
sky), and the fur commands work on it.
Related: [[fox-texface-model]], [[tiger-texface-model]], [[tripo-smart-mesh]], [[feedback-comparison-page]].
