---
name: tiger-texface-model
description: "The game tiger replaced 2026-11-20 like the fox: blank-face T-pose Smart Mesh, old tiger's eyes/brows/mouth carried over, motion matching (TigerMM); files, credits, what failed; not committed"
metadata:
  node_type: memory
  type: project
  originSessionId: d1951319-148b-483f-8cd9-e7e74b779a40
  modified: 2026-11-20T11:57:59.204Z
---

2026-11-20 (user: "어제부터 진행한 여우 개선사항(얼굴, 모션매칭, 모델링 t자로 새로 만들기) 호랑이에게 모두 진행해줘"). Done and IN THE GAME as Tiger. Page https://claude.ai/artifact/MZaCw2p4pK2SwqHTwmXwUk (source `char-pipeline/output/tiger_game/page/`). Full runbook in RRRandom HANDOFF top section "호랑이 교체".

- Model: `input/tiger_texface/tripo/tiger_tf_final.fbx` (Smart Mesh, 21,730 faces), rig `input/tiger_game/rig_mx/tiger_rig.fbx` (45 bones, hips 22.47), face `input/tiger_game/face/T_Tiger_face.png`. Old assets backed up in `input/tiger_texface/ue_backup_tiger_film/`. Mixamo now holds `tiger_game_body`.
- Cost: 260 credits (20,140 -> 19,880: two meshes, two textures), free images 514 -> 532.
- Lesson: the first mesh carved closed-eye grooves and a smile crease from faint shading in the input pictures; Blender smoothing on the coarse mesh made it lumpy. What worked: re-edit the inputs so the whole face carries NO stripes or shading (only forehead stripes), then regenerate. Tiger cheek stripes/eye rims read as eye shapes to Tripo.
- Face transfer needed `blender_feature_transfer.py --shift 0` (new): the edge colour correction turned the old white eye patches orange. Fur needed `blender_fur_shells_skel.py --face-bare rx,rz,zc` (new): shells grew in chunks over the painted eye patches and nose.
- Face redone the same day (user: face looked off vs the original, wanted whiskers): whole old face front carried over from a whisker-less render of the old rig (`blender_drop_loose.py`), plus 3D whiskers (`blender_add_whiskers.py`, 4 a side, grey patch in a free atlas cell) -> `rig_mx/tiger_rig_whiskers.fbx`, `face/T_Tiger_face_v3.png`, in game.
- Muzzle/whiskers 3rd pass (user: texture "연결도 안 되고 이상해"): front projection smeared on steep sides and showed the pasted oval. Fixed with new `scripts/blender_face_carry3d.py` (per-texel lookup on the old mesh in 3D: straight from the front where the surface faces front, along the normal on the sides; old whisker pieces excluded by loose/small-UV-island/stand-off tests) -> `face/T_Tiger_face_v4.png`; whiskers redone thin and light (root 0.0012, length 0.115, grey 0.45). Use this carry instead of 2D paste for future face carry-overs.
- 4th pass (user: keep the jaw/lower-face curves "최대한", and later expressions/talking must swap ONLY that part): old lower-face shape moved by front-depth relief (`blender_face_relief_transfer.py … --fill 6,0.4,0.006 --blur 20 --cuts 4`, window round the muzzle; snapping vertices tore, 3D high-pass gave lumps) -> `rig_mx/tiger_rig_jaw.fbx`; muzzle (nose underside, mouth, chin, whiskers) cut into its own part by `blender_split_part.py` -> `rig_mx/tiger_parts.fbx` (slots Body/Muzzle) + `parts/tiger_muzzle.fbx` (default part = base for new mouths) + `rig_mx/tiger_body.fbx` (fur only on body). Game: `ARRRandomCharacter::SetFacePart(FName)` / console `RRFacePart <Variant|Off>` shows SK_<Name>Muzzle[_<Variant>] on a leader-pose component and hides the body's Muzzle section; verified identical in game. No mouth variants made yet. Build passed; not committed.
- 5th pass (user: chin still wrong from 3 angles, "굴곡이 문제가 아니라 입이나 주변 흰색이 전혀 달라"): relief-only kept the new head's wide ball, so mouth/chin/white layout differed. Fix = `blender_jaw_shape.py` rewritten as a regularised wrap of the lower face onto the old surface (nearest point + neighbour-averaged moves, short lower feather so the shirt collar stays), then old texture carried along normals (`--front 2,3`) + a narrow neck-front patch -> `face/T_Tiger_face_v6.png`, parts re-split, in game. Lesson: compare old vs new LIT and textured (`blender_lit_views.py`) at front/3-4/side before reporting; unlit or clay views hid the mismatch.
- MM scripts now take `RR_MM_NAME` (default Fox). Cinematic now uses the new tiger (2026-11-20, see [[cinematic-sequence-plan]]). Open: portrait still old art, ultimate/vehicles/online unchecked. Related: [[fox-texface-model]], [[fox-motion-matching-test]], [[gun-support-hand-ik]], [[feedback-keep-old-face-look]].

Shipped 2026-11-21: RRRandom PR #20 (merge 2a60731) carries all of this; earlier "not committed" notes above are out of date.
