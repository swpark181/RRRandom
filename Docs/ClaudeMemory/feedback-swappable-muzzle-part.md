---
name: feedback-swappable-muzzle-part
description: "Expressions and talking swap only the lower-face part (muzzle/mouth/chin) as separate geometry; keep the jaw's sculpted curves"
metadata:
  node_type: memory
  type: feedback
  originSessionId: d1951319-148b-483f-8cd9-e7e74b779a40
  modified: 2026-11-23T05:06:35.140Z
---

For animal friends, other expressions and talking must use a different PART for the lower face only (muzzle: nose underside, mouth, chin, whiskers), not a whole new head or only texture swaps. The default lower face keeps the old model's jaw/lower-face curves as far as possible.

2026-11-23, the user again, when two new expressions went in: "표정별로 얼굴 3d 별도로 사용하기로 했는데 입 모양에 맞게 조금씩 변형을 주면 좋을것 같다" — each expression's muzzle part should be shaped a little to that expression's mouth, not just reuse the default shape. Done with `char-pipeline\scripts\blender_mouth_part.py` (reads the painted mouth from the expression texture at the part's vertices: dark = sunk in, `--bump` = a fang raised) → `SK_<Name>Muzzle_<Expression>` → `ARRRandomCineAnimal::ExpressionParts` (the cinematic actor swaps the part with the expression). The fox's muzzle was split into a part for this (`fox_parts.fbx`, SK_FoxMuzzle). My reading: "조금씩" = small moves (about 1 cm), not re-sculpted jaws. See [[expression-drafts-restraint]].

Same day ("나머지 표정도 파츠 만들고…"): every cinematic expression now has its part (all commands in `char-pipeline\input\expr_drafts\build_parts.sh`; blink faces reuse the open-eyed face's part through `PART_OF` in `ue_cine_build.py`). What I learned making them: place the mouth ellipse from `--probe --plot` pictures, not by guessing; thin mouth lines on the coarse fox part need `--box 8 --gain 4` or the crease is next to nothing; an open mouth needs `--fill` (teeth and tongue sink as one hollow) and, when it wraps round the chin like the fox's Joy, `--back 1` (sink straight back) — sunk along the normals its lower edge rolled up into view and looked worse than no part. **Check every new part in the engine from the front against the picture without the part** (`fur_mask_shots.ps1 -NoGame -Expr`), the Blender part render alone did not show that problem clearly.

**Why:** user, 2026-11-20 on the tiger: "턱 하관의 굴곡을 최대한 살려줘. 나중에 다른 표정이나 말을 할 때는 해당 부분만 다른 파츠를 써야함."

**How to apply:** when remaking a character or adding mouth shapes, cut the muzzle into its own part (`blender_split_part.py`, body slot "Muzzle" + `<name>_muzzle.fbx`), make new mouths by editing that part file, import with `ue_import_part.py` as SK_<Name>Muzzle_<Variant>, show with `SetFacePart` / `RRFacePart`. Eyes and brows stay textures. Since 2026-11-23 the tiger's fur also runs over its muzzle, as the "Muzzle" section of SK_TigerFur shaped for the default part: a variant with another mouth shape needs its own fur shells, or `SetFacePart` must hide that section (no code for that yet — do it with the first variant; see [[fur-mask-face-textures]]). See [[tiger-texface-model]], [[feedback-keep-old-face-look]], [[fox-texface-model]].

2026-11-23 later: the user then asked for the expression textures to be redrawn to fit the changed parts — a part and its expression texture are a pair; when one changes, redo the other ([[mouth-redraw-four-views]]).
