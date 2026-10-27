---
name: ui-portraits-from-concept
description: "RRRandom UI character pictures (title picker, HUD, MVP) are cut from the front concept art, not 3D renders"
metadata:
  node_type: memory
  type: feedback
  originSessionId: a8ba2ceb-b91a-4317-9207-445fcc4a2cfb
  modified: 2026-10-21T14:29:47.160Z
---

From 2026-10-21 (round 73) the user wants every picture of an animal friend shown in the UI (title character picker,
HUD team panel, results MVP: all use `/Game/UI/T_<Name>Portrait`) to come from its **front concept art**, not a Blender
render of the 3D model. `C:\work\char-pipeline\scripts\concept_portraits.ps1` holds one crop row per species.

**Why:** the user's own call: "캐릭터 선택이나 ui에 노출되는 사진은 컨셉아트 정면 사진으로 대체해줘".
**How to apply:** for a new or remade animal friend, add its row to that script and import with ue_import_ui_texture.py
(one image per run); don't render a portrait with blender_portrait.py. Related: [[tiger-fluffy-concepts]].
