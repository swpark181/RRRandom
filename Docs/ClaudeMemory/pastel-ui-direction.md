---
name: pastel-ui-direction
description: "RRRandom UI look the user picked (2026-10-16) — clean simple pastel; air-line HUD, card brawler pick, marshmallow+air-line results with no background shading"
metadata:
  node_type: memory
  type: feedback
  originSessionId: f8ab3ea1-14b3-4f45-aa84-a8a2a407030f
  modified: 2026-10-16T04:10:42.508Z
---

RRRandom's whole UI is "깔끔한 파스텔톤, 단순함". From Claude mockups A 마시멜로 / B 스티커 / C 에어 라인 the user built a combo (2026-10-16):
in-game HUD = air line (no boxes, text + thin rounded bars, round minimap); brawler pick = sticker layout (5 portrait cards in a row)
with the air-line feel; results = marshmallow panels + air-line type, and **no background shading on the results screen**.
Applied in code as RRUI (Source/RRRandom/*/RRRandomUIStyle.*, Nunito font) — see RRRandom HANDOFF 49차.

**Why:** the user asked to see mockups first and apply only after choosing; they compared them side by side ("한번에 비교하기 편하게").

**How to apply:** new UI should reuse RRUI's palette, Nunito and shapes. Show UI changes as a side-by-side mockup page before
touching the game. Text over the game needs a soft white plate (RRUI::DrawPlate): bare glow/halo text was unreadable on the darker
island map. Tripo mockups were planned but skipped (Chrome extension not connected). Related: [[dice-land-art-direction]]
