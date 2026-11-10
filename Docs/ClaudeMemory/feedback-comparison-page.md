---
name: feedback-comparison-page
description: Every character comparison (3D try1 vs retry, concept drafts) also gets a published Artifact comparison page, not just a PNG sheet
metadata:
  type: feedback
---

From now on, when showing character comparison results (Tripo 3D attempt vs Free Retry, concept 시안, texture before/after), also build and publish an HTML comparison page with the Artifact tool and give the link — the PNG sheet alone is not enough.

**Why:** the user said "앞으로 비교 페이지 까지 만들어서 보여줘" (2026-10-22) after getting only the rabbit 3D sheet; earlier they had asked for one for the red fox C-3 and liked it.

**How to apply:** reuse the red fox page layout (direction switcher + concept/A/B trio, A↔B wipe slider, 8-direction strip table, close-ups, verdict with the pick and file paths): template `C:workar-pipelineoutputabbit_mjcpageindex.html` ("토끼 A-3 3D 비교", https://claude.ai/artifact/MxF9WgpQQwTgiV32jUZ3iA; first one was "레드 폭스 C-3 3D 비교"). Images go in `files` as JPGs cut per view. Related: [[characters-in-8-directions]], [[answer-in-korean]].
