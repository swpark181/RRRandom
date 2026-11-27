---
name: tail-chain-spring
description: "RRRandom tails are a per-bone spring chain in the game code since 2026-11-24 (not committed, waiting for the user's verdict): switches, what reacts, which animals have tail bones, how the before/after clips are recorded"
metadata:
  type: project
---

2026-11-24: the user asked whether AnimDynamics fits the game; I said no (the body animation is all native C++ in
`URRRandomBodyAnimInstance`, no anim graph, and the tail was already bent by code) and recommended extending that code.
They said "꼬리에 추천안 대로 진행해서 각 동물별 꼬리 움직임 (걷기, 뛰기, 점프, 사격시) 4가지 경우를 영상으로".

Built (not committed, the user has not judged the feel yet): every tail bone is its own second-order spring toward the
old wag / lift / turn-swing targets, softer toward the tip, left behind by the bones above it (ChainInertia); new
reactions to falling and rising, braking, and each shot; all `TailN` bones a mesh has are used. Console, no rebuild:
`rr.Tail.Chain 0` = the old way, `rr.Tail.RootHz / TipHz / Damping / Inertia / FallLift / BrakeLift / ShotKick`,
`rr.Tail.Log 1`. Page with six animals, before and after side by side: https://claude.ai/artifact/MT1Vr8JV8x8RrJa7CcJ58z

Version 2 the same day, after the user's verdict on version 1: "효과가 너무 미약해서 꼬리 끝 부분만 느낌이 좀 생긴다. 3배 정도 더
move ... 끝 부분까지 스프링 강도가 센것이 문제". Now ChainGain 3 (`rr.Tail.Gain`), bends shared half evenly so the root takes part
(ChainEven), springs 2.0 Hz at the root and 0.8 at the tip, a floor (14° down in all) and a per-bone ceiling (the tail
rolled into a ball under the body and on landing without them). Fox measured: lift -6.5..+57°, swing up to 76°. The
user's taste here: big, soft, whole-tail motion - start bold and cap the extremes rather than start subtle.

Version 3 the same day: "용과 토끼를 제외하고 꼬리의 흔들리는 부분이 조금 더 뿌리에 가까워져야 ... 지금은 전과 차이가 거의 없어".
Now the bend is shared toward the ROOT (per species: `FRRSpeciesLook::TailRootBias` 1, root spring 1.3 Hz; the dragon
keeps 0.25 / 2.0 Hz and its old wag size, `bTailBigWag` false) and the wag itself grows (x1.5 standing, x Gain running).
Two things I had missed: the wag was never scaled, and a SIDE camera cannot show the side-to-side swing at all - film
tail work from the side AND from above-behind (`RRStudioCam 180 230 330`), `cut_tail.py grid <Name>` makes the 2x2.

Version 4 the same day, fox only: "꼬리가 처음부터 한쪽으로 휘어 있는게 문제 ... 뿌리쪽 휘어있는걸 바로잡고 기본 자세에서
꼬리를 지금 휘어있는 방향으로 유지". All five tails are MODELLED swept 40-60 degrees right from the root, root bones off
the body's middle. The user chose the code way first ("여우부터 방법 1로"): per-bone rest angles
`FRRSpeciesLook::TailRestSwing`, fox `-40,25,30,15` (my pick of five candidates: root straight, tip back out right);
`rr.Tail.RestSwing "a,b,c,d"` tries others. Moving the root bone sideways in code leaves a kink at the body - only
Blender can centre it (WRONG, see version 5: the fox's root bone was at the body's middle all along).

Version 5 the same day, fox only: "꺽인게 보여서 문제가 된다. 몸통하고 꼬리 따로 모델링 만들어서 붙이는 방식으로 하면 좀
해결이 될까?" -> "권하는 방식으로 일단 진행하고 결과 비교해서 보여줘". The fox's tail is now its own part, cut from the
existing model (no Tripo): `char-pipeline\scripts\blender_tail_part.py` (straight back out of the body, curling right 75
degrees, 8 bones, skin shared evenly, root run 9 cm into the body), game mesh `input\fox_game\rig_tailpart\fox_parts_tailpart.fbx`,
fur from `fox_rig_tailpart.fbx`; old assets in `input\fox_game\ue_backup_tail4bones_1124\`. The code rest stance is off
the fox again. Check tails with `blender_tail_view.py` before Unreal. My mistake to remember: I told the user the root
bone sat 9 off the body's middle - the body itself sits at x 10 in the mesh; compare with the Hips bone, not with 0.
Then "꼬리가 끊어진 부분이 보이는데 이건 못 고치나?": not the cut but the FUR - the tail's texture darkens toward
the body, under the fur colour floor, so the root was a bare band with a step. Fixed with the new
`blender_fur_shells_skel.py --fur-bones Tail` (use it for every tail made a part). Render the fur FBX alone
(`blender_tail_view.py`) and look at close game crops of a junction before showing it; I showed it without.
Version 6 the same day, what is in the game now: "여전히 연결 부위가 노출되어 보인다 ... 꼬리만 일자로 새로 만들어서 몸에
이어 붙이는게 좋을 것 같다. 그래도 어색할 것 같으면 미리 알려줘." A part stuck into the body always shows where it goes in.
`char-pipeline\scripts\blender_tail_straight.py` instead keeps the mesh whole: the tail's surface is laid out straight
(no sideways bend, the user's "일자") by a share that grows over 7 cm of surface from the edge of the tail's old skin,
the same share splitting hips / tail weights; texture untouched, 0 credits. I did NOT generate a new tail in Tripo and
said why (its own texture would band at the join, 7 expression textures to redo). Game mesh
`input\fox_game\rig_tailstraight\fox_parts_tailstraight.fbx`. Lesson: for "remodel this part of an existing character",
try re-laying the existing surface (connected, geodesic falloff) before cutting or generating.
Version 7 the same day, for comparison only: "따로 만든 꼬리 영상으로 업데이트 해줘. 직접 보고 결정할께." (I asked: Tripo,
not the cut-off part of v5.) A tail generated in Tripo from renders of the straight one (Nano Banana Pro redraw of a 2x2
four-view sheet -> Smart Mesh multiview, 130 credits), grafted on with `blender_tail_graft.py` (ICP fit, eased onto the
old surface near the join, its texture pasted into a free square of the fox's). Video on the tail page next to v6.
**The game is back on v6**; v7's Unreal assets wait in `input\fox_game\ue_tailnew_1124\`. If v7 is picked: paste the tail
square into the 7 expression textures and rebuild the cinematic fur. The user picked neither (version 8 below).
Version 8 the same day, what is in the game now: the user dropped v6 and v7 and had the whole fox generated again
with a straight tail (see [[feedback-tails-drawn-straight]], [[fox-straight-tail-model]]).
Screen recording: check a frame of every take - the user's browser covered the game once (rec_tail.ps1 now keeps the
game window topmost while recording); frames of the user's own windows are private, delete them.

Rigs (`RRRandom/Tools/cinematic/ue_cine_bones.py`): fox, tiger, squirrel, red fox, dragon have Tail1-4; cat 9, rat 11;
the RABBIT HAS NO TAIL BONES (its pom is on the hips, so nothing changes for it); no animal has ear bones.

Recording kit: `RRRandom/Tools/tailclips/` (originals in `char-pipeline/output/video/tail/`): `record_all.ps1`, then
`cut_tail.py pair <Name>`. Film on the Studio map at `goto=-1500,-2800,0` (the bots stand at X ±1500, Y 0 and ±300 and
block the lens otherwise), `RRStudioCam 105 300 10` (the tail's up-and-down only shows from the side), `hudoff`,
`-RRIdleEnemies -RRIdleBots`; walk = plain move, run = `RRSprint 1`.

Not done: gravity droop, ground contact, per-animal values, the cat and rat on screen, ears, baking the same maths for
the cinematic. Related: [[cinematic-storyboard-page]], [[animation-studio-map]], [[highlight-video-recording]].
