---
name: mouth-redraw-four-views
description: "How expression mouths were redrawn to fit their muzzle parts (2026-11-23): a 2x2 four-view sheet edited in Tripo, baked back front-first; what went wrong on the way; where the files are"
metadata:
  node_type: memory
  type: project
  originSessionId: 01b70a0c-fd4e-460b-89a1-f962ea75fc2a
  modified: 2026-11-23T06:38:25.123Z
---

2026-11-23, the user after every expression got a muzzle part: "변경된 파츠에 맞게 표정 텍스쳐 다시 그려줘. 여우하고 호랑이 둘 다."
My reading: keep each expression's design, but paint its mouth again ON the part (the old textures were projected from the
front only and smeared/faceted from the side). Done for all 10 cinematic expressions (fox 7, tiger 3), final video v8, page
version 4 (https://claude.ai/artifact/Md7fyo8H8EtUpZmYhvgado). The user has not given a verdict yet. Full steps: HANDOFF
"파츠에 맞춰 표정 입 텍스처 다시 그리기"; work folder `C:\work\char-pipeline\input\expr_redraw\` (prompts.md, render_sheets.sh,
bake_all.sh, rebuild_parts.sh, apply_redraw.sh).

**How (reusable for any texture fix that must hold from several angles):** `scripts\blender_mouth_views.py render` makes ONE
2x2 sheet of the head from four directions (front | below; two three-quarter sides), Tripo Image (Nano Banana Pro, 4K) edits
the sheet as one picture, so the four views agree and the framing stays put; `bake` projects it back per texel.

**Why the bake is front-first, not a blend:** the editor draws a mouth's inside with depth, so teeth and tongue sit in
different places in each view; averaged, they ghost. The first view paints all it sees well, later views only the rest.
Line mouths need the front view to paint nearly everything (`--lo 0.03 --hi 0.16`): a line redrawn in a side view never
lies exactly on the front's and forks where they mix. Open mouths keep the default (0.22/0.5).

**Other lessons:** tell the editor "hair-thin, do not thicken" for line mouths (first results were ~3x thicker, more
cartoony — the user dislikes that, see [[expression-drafts-restraint]]); replace the whole mouth region (`--full`), faint
leftovers are below any difference threshold; fill the texels between UV islands or the base face shows as thin lines along
seams; keep whiskers from printing onto the fur (`--thick 4`); a saved browser capture with `scale` set is saved small — never
pass `scale` with `save_to_disk`; the Tripo page sometimes needs a reload to show finished jobs.

**The parts were rebuilt too** (same recipes, `rebuild_parts.sh`): the redrawn mouths are a little larger, and the hollow
has to lie under the paint. Texture and part are made in a loop: bake on old part → rebuild part → bake on new part.

Related: [[feedback-swappable-muzzle-part]], [[fur-mask-face-textures]], [[feedback-fix-textures-in-tripo]],
[[cinematic-sequence-plan]].
