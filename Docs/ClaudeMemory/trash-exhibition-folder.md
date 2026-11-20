---
name: trash-exhibition-folder
description: "C:\\work\\휴지통전시회 = the user's collection of failed images/scenes/GIFs from RRRandom work; layout and how lost scenes were re-recorded"
metadata:
  node_type: memory
  type: reference
  originSessionId: e592adc3-602c-4703-875f-2d533dad5a08
  modified: 2026-11-18T08:30:11.019Z
---

2026-11-18: the user asked me to collect things I think "망했다" into `C:\work\휴지통전시회` and kept adding to it ("…찾아서 추가해줘").
Layout: numbered Korean file names + a guide text per folder — root `전시 안내.txt` (27 stills), `움짤\움짤 안내.txt` (26 GIFs), `버려진 로봇 컨셉\로봇 안내.txt` (23 discarded robot concept sheets). New items continue the numbering and get a line in the guide; everything is a copy, originals stay.

When a requested scene has no capture left, re-record it and label the file "(재현)" / "(재촬영)":
- a fixed bug: flip the fix back, build, record with `RRRandom\Saved\Rounds99\seq.sh`, restore, rebuild, check `git diff` is empty (done for the squirrel grenade flying backward).
- an old design: `git worktree add --detach C:\work\_rr_oldNN <commit>` (Content is in git LFS, ~480 MB), build that uproject (~2 min), run it with `char-pipeline\scripts\ue_game_seq.py`, then `git worktree remove --force` (done for the round-59 navy star robot combine from commit 122b34e; `RRStarCars` + `RRRide` start the combine, `-RRIdleEnemies` keeps bots still).
GIFs from frames: ffmpeg palettegen/paletteuse, 720 px wide. Tripo Image history loads 16 rows per scroll (JS scroll works after one real wheel scroll).
