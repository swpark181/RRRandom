---
name: cinematic-storyboard-page
description: "Fox vs tiger cinematic storyboard (콘티) page with cut numbers 1-9 and panel numbers like 3-2; the user refers to cuts by these numbers, which differ from the older 1A/1B/4A/4B names"
metadata:
  node_type: memory
  type: reference
  originSessionId: 01b70a0c-fd4e-460b-89a1-f962ea75fc2a
  modified: 2026-11-23T06:52:34.650Z
---

2026-11-23, the user: "지금 시네마틱 콘티를 페이지로 만들어줘. 콘티에 번호를 붙여서." Page:
https://claude.ai/artifact/UqbffPLvmd1iaaBUh5vYmC (source `C:\work\char-pipeline\output\storyboard\`, `build_board.py` writes
`page\index.html` from its cut table; panels are frames of final v8).

**When the user names a cut by number, it is this page's number**, not the older documents' name:
new 1 = old 1A (chase), 2 = 1B (second barricade), 3 = 2 (dead end), 4 = 3 (tiger arrives), 5 = 4A (gold die),
6 = 4B (tiger flinches), 7 = 5 (clones), 8 = 6 (surrounded), 9 = 7 (grin). Panels are "cut-order", e.g. 3-2.
Cut boundaries in frames at 24 fps: 0, 58, 120, 156, 204, 240, 252, 300, 360, 408.

When the cinematic changes, redo the panel frames and the table and publish to the same address.
The user's word for this document is 콘티 (asked first what a "시퀀스 연출 스토리테이블" is usually called).

Related: [[cinematic-sequence-plan]], [[feedback-comparison-page]].

**Re-cut draft (2026-11-23, waiting for the user's review).** The user gave a new cut list against this page (1 face close-up
→ look back → zoom out with the blurred tiger; 2 long even obstacle: hands on, up, roll down; 3 hits the wall, turns,
slumps; 4 dark street, tiger through shadow; 5 scared face → die → looks up, brightens; 6 stands, clones surround WIDE,
tiger flinches (old 7 merged in, deleted); 8 foxes step closer, tiger drops the weapon; 9 as before) and asked for a rough
in-game placement storyboard BEFORE the real sequence is edited. Draft page: https://claude.ai/artifact/B9MXkSnKKFxAcNMjusBwmX
(source `storyboard\build_board2.py` → `page2\`, frames `storyboard\board2\t5`, paths `board2\layout.json`). Cut numbers
keep the user's: 1-6, 8, 9 (no 7). The real LS_FoxVsTiger / Map_FoxVsTiger were not touched: the draft lives in
`/Game/Cinematics/FoxVsTiger/Board/` (Map_FvT_Board, LS_FvT_Board), built by `RRRandom/Tools/cinematic/ue_board_build.py`,
stand-in poses `ue_board_poses.py` (A_Fox_Board_*), runner `board_run.sh` (py / render). New fox clips retargeted from GASP:
A_FoxMM_MantleRun, LandRoll, JumpOff. Open points listed on the page (my readings, to confirm): obstacle = 4 m long, 1 m
high platform along the run; tiger still smashes it; wide ring = an oval along the 3.65 m alley; cut 6 camera beyond the
dead-end wall (wall hidden for the cut); 24 s for now. Known wart: in the draft's cut 9 a clone's pistol blocks the lens at
the start (cause not found), so the page and the draft video use the current film's cut 9.
Lessons: straight-down cameras need make_rot_from_xz (find_look_at_rotation picks +X as up when looking down); the -Y
building's roof overhangs the alley by about 1.2 m (top views hide that side); a big pink town die stands beside the road
at about X -4500, Y -3200..-3400; Git Bash mangles /Game/... in env vars (MSYS_NO_PATHCONV=1, MSYS2_ENV_CONV_EXCL=*);
no system Python: use UE's `Engine/Binaries/ThirdParty/Python3/Win64/python.exe`.

**Motions pass on the draft (2026-11-23, page version 2, take `board2\m2`, 25 s).** The user asked how the stand-in poses
become animation, whether Pose Search can make them, and for a web comparison, then said "바꿀점 참고해서 진행해줘". Done in
the draft only (real sequence still untouched, user's seven reading points still unanswered):
`ue_board_motions.py` builds A_Fox_Board_* clips inside Unreal (sample any bone from any clip at any time mapping, legs
from one clip + upper body from another, bones turned by eased curves in the body's space, `face_as` = keep a bone's
direction from another clip); `ue_board_probe.py` prints a clip's root / hips / chosen bones over time;
`ue_board_audition.py` takes RR_AUD_SLOTS / RR_AUD_YAW / RR_AUD_CAM / RR_AUD_LENS. Clips: RunLookBack (3 run cycles, look
curves built in), WallHit, TurnSit (GASP bench_into from 2.15 s, back kept upright), Sit / SitLookUp (legs frozen, upper
body swaying), StandUp (GASP bench_out from 0.4 s), StepA/B and AimStepA/B (MM Walk_Start_F_Rfoot to 0.55 s + Walk_Stop_F_Lfoot
from 1.0 s = one 57 cm step). 14 more GASP clips retargeted into `FoxMM/Cine` (bench, get-up, head turn, crouch, slide).
What I found: Pose Search only picks from clips it has (stops, turns, steps yes; sitting, looking back no). GASP has
bench sit/stand but no floor sit; its ragdoll get-ups start lying down; HeadTurn / LookAtPOI assets are additive poses,
not usable as clips. UE 5.8 MetaHuman Animator does full-body capture from one webcam (forum release notes; not tried) -
the next step if the sit-down is not good enough, after Mixamo. Cascadeur not worth learning for six motions (my view).
Lessons: MM clips on the game fox lose their Root track (no root bone), so the actor must be keyed from the source clip's
root, translation AND yaw; MM clips hang the tail straight down while A_Fox clips leave it at the reference pose, so
make copies on the fox's own skeleton (`like` = an A_Fox clip); Cine_Terrified's pose looks to its right (the sitting
fox is 3/4 to the camera in cut 5 by the pose, not by a wrong yaw); Cine_PistolAim bows the head, PistolAimLow keeps it
up; sections loop past a clip's end, so holds must fit the clip; keep one transform key per frame.

**Tension / die-scene proposal (2026-11-23, waiting for the user's decision).** The user: "긴장감을 올리면서 금색 주사위가 나왔을
때 좀 더 극적이고 유머러스하게 ... 아주 간단하게 구도만 잡아서 ... 페이지로." Page https://claude.ai/artifact/6bC5aPXToCrmSUTHLhNHgT
(source `storyboard\build_board3.py` → `page3\`; seven hand-drawn SVG composition sketches made by small Python helpers
fox_head / tiger_head / hammer / die, not game renders; `page3\preview.html` has a charset line for local viewing).
My proposal (not yet accepted): 3-끝 top-down "boxed in, shadow arrives first"; 4-2 the tiger does not stop at 4 m but
comes right up; 4-3 hammer raised slowly, intercut with the fox's eyes squeezing shut; 5-1 the blow lands on the gold
die that pops up over the fox's head ("뿅!"); 5-2 both watch it rock between 1 and 5; 5-3 the burst pushes the tiger back
to the middle = the ring's place, cuts 6-9 unchanged. About 32 s. Open: die hit by the hammer vs the game's own spin,
the 1/5 gag, a new one-eye-open expression, length.

**Tension pass built in the draft (2026-11-23, draft page version 3, take `board2\n6`, 13 cuts, 32 s).** The user: "이 구도대로
게임 캐릭터 배치해서 초안 만들어줘" (the proposal's five questions were not answered; I went with the proposal as drawn).
Cuts now: 1, 2, 3, 3e ("3-끝"), 4, 4-2, 4-3 (four shots: up / eyes / up / eyes), 5-1, 5-2, 5-3, 6, 8, 9; `layout.json` has
"cuts" by those keys and "shots". ue_board_build.py: SHOTS list drives the camera cut track; `RR_BOARD_TOP=1` rebuilds with
a close top-down check camera over the dead end (use it when positions look wrong). New clips A_Tiger_Board_Raise / Bonk /
Watch / Shield and A_Fox_Board_Cower. Still the real sequence is untouched and nothing is committed.
Lessons: the game's A_Tiger_HammerSwing1 and A_Tiger_ShieldBlock are upper-body clips - played whole the tiger lies on
his back; take hips and legs from A_Tiger_Idle (faces straight ahead; Cine_SsIdleC stands side on, so the swing's chest
turned to the wall, and it holds the hammer out in front). The tiger's game clips have 24 keys a second, the fox's 30.
The swing's top (0.38 s of the clip) has the hammer back over the shoulder, not high overhead. Fox head is about 75 cm
wide and 90 tall with ears, the die 80: in the 3.65 m alley a side-on two-shot needs 10-12 mm. Side cameras go on the
tiger's hammer side (-Y when he faces the wall): from +Y the shield covers him. Cameras behind the sitting fox are
inside the dead-end wall: hide the wall for those cuts (4-2, 5-3, 6). A camera at the fox's eyes needs the fox hidden.
Long shadow for the top-down shot: a narrow spot from the road down the alley, lowered during the shot.
