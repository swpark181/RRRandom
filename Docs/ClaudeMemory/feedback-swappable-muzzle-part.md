---
name: feedback-swappable-muzzle-part
description: "Expressions and talking swap only the lower-face part (muzzle/mouth/chin) as separate geometry; keep the jaw's sculpted curves"
metadata:
  node_type: memory
  type: feedback
  originSessionId: d1951319-148b-483f-8cd9-e7e74b779a40
  modified: 2026-11-20T11:38:10.265Z
---

For animal friends, other expressions and talking must use a different PART for the lower face only (muzzle: nose underside, mouth, chin, whiskers), not a whole new head or only texture swaps. The default lower face keeps the old model's jaw/lower-face curves as far as possible.

**Why:** user, 2026-11-20 on the tiger: "턱 하관의 굴곡을 최대한 살려줘. 나중에 다른 표정이나 말을 할 때는 해당 부분만 다른 파츠를 써야함."

**How to apply:** when remaking a character or adding mouth shapes, cut the muzzle into its own part (`blender_split_part.py`, body slot "Muzzle" + `<name>_muzzle.fbx`), make new mouths by editing that part file, import with `ue_import_part.py` as SK_<Name>Muzzle_<Variant>, show with `SetFacePart` / `RRFacePart`. Eyes and brows stay textures. See [[tiger-texface-model]], [[feedback-keep-old-face-look]], [[fox-texface-model]].
