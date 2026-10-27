---
name: feedback-fix-textures-in-tripo
description: "Fix a model's texture details (spots, marks) through Tripo re-texturing, not by painting the texture directly in Blender"
metadata:
  node_type: memory
  type: feedback
  originSessionId: a8ba2ceb-b91a-4317-9207-445fcc4a2cfb
  modified: 2026-10-21T13:52:30.180Z
---

When a Tripo character model has an unwanted texture detail (2026-10-21: white spots on the fluffy tiger's ear backs),
the user wants it fixed **in Tripo**, not painted over by script. A Blender script that filled the spots flat black
was rejected: "tripo 에서 해당부분 지워줘. 직접한거는 이상하네" (the hand fix lost the fur look).

What worked: edit the multiview input picture(s) in Tripo Image (free, "Edit this exact image ... change only one
thing: ...") → Texture tab, multiview mode, replace only that slot (trash icon on the slot opens its file input) →
Remove Lighting on, 2K → Generate Texture (10 credits at 2K) → export FBX. The fur texture stays natural.
What did not work: Texture → Edit → Magic Brush Gen Mode re-drew the spots both times (it follows the current look).

**Why:** direct paint gives flat, textureless patches that read as wrong on a fluffy character.
**How to apply:** for texture-detail fixes, go the Tripo retexture route first; script painting only if the user asks.
Related: [[tiger-fluffy-concepts]].
