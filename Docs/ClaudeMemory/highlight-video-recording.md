---
name: highlight-video-recording
description: "How the RRRandom 30s highlight video was recorded (2026-10-26) — real-time ffmpeg gdigrab, bots use ultimates, scripted cine camera"
metadata:
  node_type: memory
  type: project
  originSessionId: f62f7cca-ad5d-4b87-90b1-a52fd4d5d623
  modified: 2026-10-27T06:58:12.227Z
---

First RRRandom highlight video (30s, Dice Town, third person) made 2026-10-26 for the user's YouTube channel; result and
scripts in `C:\work\char-pipeline\output\video\` (scripts still point at a session scratchpad for ffmpeg/paths — fix
paths before reuse; portable ffmpeg came from gyan.dev essentials zip).

- Frame dumping (`-benchmark -fps=30 -dumpmovie`) was too slow (~3 fps) — the user asked for another way. Use real-time
  ffmpeg gdigrab of the game window's client area + h264_nvenc.
- Display is 2560x1600 at 150% scale: `-ResX=1280 -ResY=720` gives a 1920x1080 physical client; 1920x1080 requested gets
  a maximized window. A stray F11 test saved `FullscreenMode=1` in Saved/Config/WindowsEditor/GameUserSettings.ini (set back to 2).
- My own ultimate flips the view to the overhead pick camera (6s), so shots use allied bots instead: `-RRBotSpecies=X
  -RRIdleEnemies`, enemies set in a row ahead, console `RRTeamUltimate` (added for this), camera = the StudioCam actor moved
  from Python each tick. `-RRNoOnlinePanel` (added) hides the debug text under the minimap.
- Good camera on the start street: on its axis (y=0) low behind the allies; side positions end up inside buildings.
- Video has no sound; the user adds music themselves.

Second video 2026-10-26: 15s cannon + planes teaser `output\video\RRRandom_cannon_planes_15s.mp4`. Reusable kit now has
permanent paths: `output\video\r88\` (rec_cap.ps1, video_seq.py with `cinefollow=ME,...` = my own pawn, edit.py,
batch_*.ps1), ffmpeg at `char-pipeline\tools\ffmpeg\ffmpeg.exe`, Python = UE's
`Engine\Binaries\ThirdParty\Python3\Win64\python.exe`. Clip time ≈ game time + 4.5 s (compute from clips\<tag>.txt).
Lessons: plane "jump" via script sends it to the 2,700 ceiling — leave only the take-off climb; side cameras along the
street end up inside buildings; the station die is random (log line "throws the plane die: N"), so record several tries.

Third video 2026-10-26: 15s round-89 teaser `output\video\RRRandom_destruction_lava_15s.mp4` (batch_r89.ps1, edit_r89.py;
restore sped up with `rr.Wreck.BigRestoreSeconds 7` while filming; good cameras came from the round's own test logs).

Fourth video 2026-10-27: 30s "어제·오늘 업데이트" recap `output\video\r91\RRRandom_update_1026_1027_30s.mp4`
(edit_r91.py: date chip, Korean line over English title, reuses r88/r89 clips + new r91_* clips + grip90 clips).
Defense filming (stage 1): town center = origin, rats reach the east gate (x≈1795) ~12-16 s; good camera = on the
road axis outside it `cine=3150,120,430,1750,0,140` (sees tower, gate, rush, cat boss after `RRWave 4`). Shooting off the
wall hits the battlement (white-out); close cameras + cannon/blaster = white-out too — film splash from above the gate.
video_seq.py gained `look=yaw,pitch`. The small toy guns don't read in close firing shots (head/hands hide them).

Related: [[smart-app-control-dll]], [[cannon-plane-design]] (hit again on the first rebuild).
