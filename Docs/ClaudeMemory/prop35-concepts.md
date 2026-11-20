---
name: prop35-concepts
description: "RRRandom 3.5-heads-tall redesign concepts (front+back, role gear), round 1 + round 2 on 2026-11-18; what the user rejected and how proportion/charm were controlled in Nano Banana Pro"
metadata:
  node_type: memory
  type: project
  originSessionId: e592adc3-602c-4703-875f-2d533dad5a08
  modified: 2026-11-18T05:59:39.690Z
---

2026-11-18 (user: "캐릭터를 3.5 등신으로 … 6종 모두 앞 뒤 2장씩 … 역할에 맞는 모습 … 의상 디자인은 최대한 유지"): concepts only, game unchanged.

**Round 1 feedback (user, same day):** "용, 다람쥐, 여우는 매력을 잃어버린것 같아. 다시 시안 뽑아줘. 토끼는 지금 너무 디즈니 캐릭터를 닮아서 다른 디자인 개선이 필요해." Tiger and red fox were not mentioned (kept from round 1). My reading of "lost charm": round 1 made thin mannequin bodies, smaller eyes/cheek fluff/tails, fierce squirrel face, goggles hiding the dragon's forehead, huge wings.

**Round 2 (waiting for the user's picks):** fox, squirrel, dragon redrawn with the original face, big cheeks, chunky limbs, big tails (≈3.3–3.4 heads); rabbit = 3 new designs: A Dutch pattern (white + caramel, one bent ear, ≈3.0–3.1), B lop ears (apricot, ≈3.2, ears may fuse with arms in 3D), C night-sky navy (≈3.1, head shape still close to the old one). I recommended A.
Files: `C:\work\char-pipeline\input\prop35\concept\` — `fox|squirrel|dragon|tiger|redfox_front|back.png`, `rabbit_dutch|lop|night_front|back.png`; round 1 versions in `concept\v1\`; alternates in `concept\alt\`. Sheets `output\prop35\prop35_round2_sheet.png` (round 1: `prop35_sheet.png`), page https://claude.ai/artifact/Hi3Uq2pbBFXswufLJCgayi (source `output\prop35\page\`).
Gear now: fox chest strap only; squirrel acorn bandolier; dragon pilot scarf (goggles removed); rabbits goggles + scarf + moon clip; tiger shoulder guard + wrist wraps; red fox thigh sheath + wrist wraps.
Used Nano Banana Pro, not Midjourney ([[feedback-first-concepts-midjourney]]), because the outfits had to be kept and MJ in Tripo takes no reference image; told the user. Free counter 168 → 310/1000, no credits.

**Round 3 (2026-11-18, user: "2.5 등신 정도로 여우하고 용만 시안 3가지씩"):** fox ≈2.6, dragon ≈2.7 heads, three variants each that differ only in role gear (A none, B one item, C two): `input\prop25\concept\fox_A|B|C.png`, `dragon_A|B|C.png` (front only; shorter ≈2.3 and longer ≈3.0 takes in `alt_short_2p3\`, `alt_long_3p0\`), sheet `output\prop25\prop25_sheet.png`, page https://claude.ai/artifact/YUp96nyAvFHL5CAxm2MXQc. Waiting for picks. Chart `input\prop35\guide_25c.png` + "legs about 1.25 times as long" landed at ≈2.6; "1.5 times" overshot to ≈3. The 3.5 direction seems dropped in favour of ~2.5 (my reading; not said outright).

Reference board the user asked for (2026-11-18, "동물이 캐릭터로 나오는 fps/tps … 3.5등신 … 스샷과 게임이름 10개"): `C:\work\char-pipeline\output\ref_games\animal_shooter_refs.png` (15 games; closest to our look: Gunfire Reborn, Biomutant, Ratchet & Clank Rift Apart, Furball Blitz!, MOUSE P.I., Conker Live & Reloaded, Shadow the Hedgehog, Fur Fighters, Battle Bears Gold). Sources in `input\ref_games\` (Steam appdetails API + curl, no browser needed). Kept local, not published, because the screenshots are other companies' images.

**How proportion and charm were controlled (reuse this):**
- Text alone fails both ways: "same character, 3.5 heads" copies the chibi reference; "TWO CHANGES REQUIRED" overshoots to 4+ heads and swaps clothes.
- Proportion: second reference = grey mannequin chart with ruler lines (`input\prop35\guide_35b.png` thin, `guide_35c.png` chunky) + "copy its ratio of head height to standing height; chin on the chin line …". Say ears/pompoms stand ABOVE the top-of-head line. Of the 2 results, often one is unchanged and one follows the chart (sometimes with the ruler lines drawn in, or overshooting to ~4 heads) — pick, don't expect both.
- Charm: edit the tall image with the ORIGINAL chibi front as image 2 "ONLY as the reference for the charm: face, expression, eye size, fluffiness, head and tail volume; do NOT copy its chibi proportions" + "soft chunky plush-toy limbs, big paws". This brings the face back but can fall back to ~2.5–3 heads → then run the chart pass on that result.
- "REMOVE the strap / add a bandana or hat" inside such an edit was ignored; accessories need their own edit pass.
- Results sometimes drop the shorts or a belt pouch — check every pick.
- Back view = new front (+ old back JPEG for details) as references; state strap and tail sides in character and image terms.
- Right after opening the Tripo page the first Generate clicks are ignored even with history loaded; check the Free counter and click again.
- Capture without downloading: overlay the img at `innerHeight` px square (window may be 833 px while screenshots are 784) → `zoom` [0,0,784,784] save_to_disk scale 1.
- `file_upload` total per browser_batch call ≤ 10.4 MB; use small JPEG copies (`input\prop35\ref\`).
