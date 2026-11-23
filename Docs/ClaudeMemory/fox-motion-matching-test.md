---
name: fox-motion-matching-test
description: "Pose Search (motion matching) test on the fox, started 2026-11-20: Game Animation Sample downloaded, 158 clips retargeted to a root-bone test fox and copied into RRRandom; runtime not built yet; where files and scripts are"
metadata:
  node_type: memory
  type: project
  originSessionId: 6b9e08c0-c36e-48a5-83e0-136014a972f6
  modified: 2026-11-20T05:13:24.041Z
---

2026-11-20 (user: "언리얼의 서치 포즈를 지금 프로젝트에 있는 여우에게 우선 적용하려고해", wants later to add own motions from
AI video mocap). Research + plan page: https://claude.ai/artifact/15AvcFLNqgyuxnLjd6QUe9 (source was in the session
scratchpad, may be gone; read the artifact to edit). Decision told to the user: Unreal Pose Search (Fortnite style), not
TLOU2's own system; TLOU2 only lends the "sets" idea; Game Animation Sample (GASP) data first, video mocap later for
per-character gait.

**Done (data only, nothing committed, the in-game Fox untouched):**
- GASP project at `C:\work\GameAnimationSample` (UE 5.8, 7 GB). Fab ships it as "Complete project": the launcher has no
  "add to project", only "create project".
- Test fox with a root bone: `C:\work\char-pipeline\input\fox_mm\fox_rig_root.fbx` (`scripts\blender_add_root_bone.py`;
  the Mixamo rig's top bone is Hips, and the hips sit 10 cm / 17 cm off the armature origin, so Root is placed under them).
- In the GASP project `/Game/Characters/FoxMM`: SK_FoxMM, IK_FoxMM (auto retarget definition + Root chain, toes renamed
  LeftToe/RightToe), RTG_UEFN_to_FoxMM (default ops), 158 clips `Anims/A_FoxMM_*` (585 s; idles, turn in place,
  walk/run/sprint loops, starts, stops, pivots = GASP's Sparse stand databases + Dense idles). Root motion came across
  (run loop 278.5 cm in 2.5 s at mesh scale 1; source 1250).
- Copied into `C:\work\RRRandom\Content\Characters\FoxMM` (SK_FoxMM, skeleton, Anims; 116 MB) and checked to load.
- Lists: `C:\work\char-pipeline\output\mm_logs\gasp_dbs.json` (every GASP database's clips),
  `fox_mm_dbs.json` (the 14 chosen databases -> fox clip names + schema name). Sheets: `output\fox_mm\sheets\`.
- Scripts in `C:\work\char-pipeline\scripts`: `mm_run_ue.sh` (commandlet runner that watches the log for MMDONE and
  kills Unreal; `MM_RHI=-AllowCommandletRendering` when a renderer is needed, e.g. FBX export with mesh),
  `mm_gasp_import_fox.py`, `mm_gasp_dump_dbs.py`, `mm_gasp_retarget_fox.py`, `mm_export_fbx.py`, `mm_rr_check_fox.py`,
  `blender_anim_sheet.py` (contact sheet of an animation FBX).

**Runtime built the same day** (user: "진행시켜"; off by default, not committed; details in HANDOFF.md top entry):
`-RRMotionMatch` / `rr.MotionMatch 1` turns it on, `rr.MotionMatch.Upper 0` = hips and legs only, `rr.MotionMatch.Log 1`.
A hidden driver SkeletalMeshComponent (SK_FoxMM + ABP_FoxMM: Motion Matching -> Pose History, parent
`URRRandomMotionMatchInstance` in `RRRandomMotionMatching.h/.cpp`) runs Pose Search; the visible body
(`URRRandomBodyAnimInstance::UpdateMotion` / `EvaluateMotion`) takes its local bone transforms as base pose except arms
and tail, existing layers on top. Clips play 1.5x (`RRMotionMatch::PlayRate`) and the trajectory distances are divided by
mesh scale x 1.5, because the fox walks 300/s on legs a fifth of a person's. Assets built by
`mm_rr_build_set.py Fox` -> C++ `URRRandomMotionMatchLibrary::BuildSet` (a database's animation list is private, Python
cannot write it). Checked on Studio: on / legs-only / off captured as GIFs (`char-pipeline\output\fox_mm\gif\`).

Result page for the user (2026-11-20, "페이지로 정리해서 보여줘"): https://claude.ai/artifact/JkMJKqH7cY1mqYMyAFRJBF
(three-way GIFs first, then the choice Upper 1 vs 0 — I recommended Upper 0, hips and legs only — switches, structure,
speed table, open points).

**User's decision 2026-11-20:** upper body matched too (Upper 1), not my recommended hips-and-legs-only ("상체까지 들어가야
자연스러울 것 같다"); but running with a gun looked too upright ("너무 꼿꼿하게 서서 간다 … 살짝 숙이고"). Added a
speed-scaled forward spine lean in `EvaluateMotion` (`MotionRunLean` 22 degrees at walking speed: 14 first, then the user picked 22 from a 0/14/22 sheet, "기존 처럼 22 까지 숙이면 좋겠네"; neck/head lift 70% back,
`rr.MotionMatch.Lean <deg>` to try amounts; gone while aiming). It also brings the gun down from the face to the chest.
Page updated with the 22 degree GIFs.

**In the game by default since 2026-11-20** (user: "이렇게 해서 게임속 여우에도 적용해줘"): `rr.MotionMatch` defaults to 1, off
with `rr.MotionMatch 0` or `-RRNoMotionMatch`; the fox's clones (bDecoy) get it too; the driver poses only on the host
or while the body is on screen. Checked on Studio without options: sprint, standing aim + fire, aimed walk, aimed
strafe, jump, emote (`screensox_mmsheet_act1/2.png`), no broken poses, no ensure/assert. Fps on the BR test map:
12 foxes 81.0 on / 80.0 off, 2 foxes 76.6 / 77.1. Not committed.

**Open:** user to judge the look in play; with the upper body matched the gun rides up in front of the face
(legs-only keeps it at the chest); which clips get picked not logged; 30-player fps not measured (one more component per
animal friend); aim/fire/sword/jump/vehicles/bots/online not checked; no stride or orientation warping; only the fox has
a set; video-mocap clips for per-character gait still to come.

**Lesson:** the first retarget looked right by travel distance but the fox's Root rode at the mannequin's pelvis height
(the retargeter's Root Motion op takes the source IK rig's "root motion bone", which GASP's rig sets to pelvis); the fox
sank through the floor once the root was locked. Fixed by `set_source_root_bone("root")` + snap to ground; the retarget
script now checks every clip's first-frame root height and hips height. Check absolute poses, not only deltas.

Gotchas: the Epic launcher was driven with a PowerShell capture + mouse_event script (no computer-use tool in that
session); it asked to fix the .uproject file association — answered No. Fab listing says "Allows usage with AI: No"
(my reading: not as generative-AI input; licence text not read). See [[ue-commandlet-hangs-at-exit]], [[fox-texface-model]].

**Second bug, found by the user in play 2026-11-20** ("게임 시작할대 제자리에서 걷는 모션이나오고. 실제로 앞으로만 걸어가면 서
있는 애니메이션만나와"): my root bone stood up like the other Mixamo bones (roll 90 in Unreal). Pose Search reads the
trajectory in the ROOT BONE's space and keeps only X and Y, so forward/back movement vanished from the search and the
fox stayed on `Run_Loop_B` standing or walking forward. Every GIF and "check" before that was of wrong clips; stills only
showed legs cycling, so I missed it. Fix: `blender_add_root_bone.py` lays the root along the armature's -Z with its Z up
-> rotation (0,0,0) in Unreal; FoxMM rebuilt from scratch in both projects. `rr.MotionMatch.Log 1` now logs
`MOTIONCLIP <clip name>` and `MOTIONTRAJ`: standing = Stand_Idle_Loop, forward = Run_Start_F -> Run_Loop_F -> Run_Stop_F
in the main mode (`-RRThirdPerson -RRMap=DiceTown -RRRule=Rounds`). **Check motion matching by the logged clip names, never
by stills.** The 22 degree lean was picked from wrong-clip pictures; with forward clips 14 looks closest to the old walk —
asked the user again, value left at 22.

**Climbing clips added 2026-11-20** (user: "장애물 넘어갈때나 점프해서 높은 장애물 올라갈때 모션 추가해줘"): the game's existing
climb (0.45 s, body used to slide up) now plays `FoxMM/Traversal/A_FoxMM_ClimbLow` (sample Mantle 1 m; rise under 110) or
`A_FoxMM_ClimbHigh` (Climb 2.5 m) on a second hidden body `ClimbDriver`, time-fitted to the capsule's up and over phases;
the visible body takes that whole pose, arms too. Test cheat `RRClimbTest <rise>` (Studio). Checked: both clips play by
log and frames; in Dice Town only by log. Not done: a real vault over to the far side (no such movement in the game),
hands on the ledge, remote clients (bClimbing is not replicated). High climb plays 3-4x fast; told the user that
lengthening the climb would look better but changes game speed.

**Gotcha:** rebuilding the set with `mm_rr_build_set.py` while ABP_FoxMM exists crashes the editor at the blueprint compile
(delete in-editor + recreate same name) and leaves the fox broken (PSD/PSS already deleted). Delete
ABP_/PSD_/PSS_FoxMM.uasset on disk first; the script now stops if they exist.

Write-up for people (user asked 2026-11-20: "모션관련 작업을 md 파일로 정리해서 저장해줘"): `C:\work\RRRandom\Docs\FoxMotionMatching.md` (Korean; switches, structure, pipeline order, clip-name checks, problems, open items). Keep it in step with the HANDOFF entry. Before/after page (model, face, motion): https://claude.ai/artifact/Thv5P49BdQv9NVVTsU3dby, source `C:\work\char-pipeline\outputox_compare\page\`.

Shipped 2026-11-21: RRRandom PR #20 (merge 2a60731) carries all of this; earlier "not committed" notes above are out of date.
