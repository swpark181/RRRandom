---
name: tiger-straight-tail-model
description: "The tiger generated again with a straight tail (2026-11-25) like the fox, in the game on the old skeleton; and the rabbit's pom tail made a bigger fluffier ball in Blender; files, credits, traps"
metadata:
  node_type: memory
  type: project
  originSessionId: 5627b37d-3248-4e51-8715-2d8262c6ce52
  modified: 2026-11-25T11:38:01.920Z
---

2026-11-25, user: "호랑이 꼬리도 여우 꼬리처럼 다시 작업해줘 ... 토끼 꼬리는 더 동글동글 하고 풍성하게" (asked: tiger by Tripo, rabbit
in Blender). Both in the game, not committed. Runbook: RRRandom/HANDOFF.md "호랑이 꼬리 다시". Page section "꼬리: 호랑이·토끼"
on https://claude.ai/artifact/3X2QC96JHzCevvLit1Qaji.

- Tiger: tail-only redraws of the 3 old 3D inputs (free), Smart Mesh multiview 130 credits (19,620 -> 19,490), task
  d62fee81. `input/tiger_straight/`. Same skeleton via `blender_reskin.py` (+ --tail-long 1.6 --tail-fat 0.35,0.15: the
  redrawn tail came out small again - measure the old tail and say its size in the prompt). Face = the 11-20 tiger recipe
  re-run against the CURRENT game tiger (jaw wrap, carry3d incl. eyes, whiskers, muzzle split, ear bones, fur).
  `blender_jaw_shape.py --cuts 1` (cuts 2 doubled the fur to 678k faces). Standing tail 60/5/100 (end curls up).
- Rabbit: `blender_tail_ball.py` (pom onto a sphere, relaxed) + fur `--long` (new option, longer fur in a sphere).
- Rabbit pom squashed on the run: it had no tail bone (weights over Hips/Spine/Spine1). Now Tail1 (`blender_tail_ball.py --bone`), species `TailAmount` 0.3; the chain uses only tail bones the mesh has. Any rigid tail part needs its own bone.
- Tiger tail then made even (`blender_tail_even.py`, 4.8 cm) and cat-like (species TailWave 2, TailWagSize 1.4, TailWagSpeed 0.7, TailTipFrequency 0.55, carried up + hooked while walking TailMoveLift 22 / TailMoveTipLift 55, a quarter at a sprint; root bias 0.25). The user wants cats unlike the fox.
- Then "Stray 고양이 꼬리처럼": heavy, low, slow, damped (species TailDamping 0.65; walk root -8 / tip 45; wag x1 speed x0.55; standing 50/18/90). Reference video page DzJsFoCtgWk5hbb9vPxkV7 (game/3D only - the user finds live footage hard to use).
- Then "너무 스틱 같아, 꼬불거리게": species TailSquiggle/Side/Waves/Speed (a bone-against-bone S running down the tail;
  rr.Tail.Squiggle* cvars). Tiger 14/9/1.15/0.4 - the first try 9/6/1 barely showed, so start big when the user asks for visible change.
- Then hanging + smaller + floor: species TailSize (Tail1 bone scaled in the proxy, fur follows; tiger 0.7), standing
  25/-40/48, walking -95/50 (the move's forward lean lifts the tail), and a floor guard for every tail (rr.Tail.Floor).
  The user wants the tiger's tail hanging toward the floor with only the tip up "평상시".
- TailSize scales only what is weighted to Tail bones, so a root ring half on the hips flares: fit it in the model with
  `blender_tail_game_fit.py --size <TailSize>` (game rig now `tiger_straight_fit_parts.fbx`, fur `fur_fit`). Change
  TailSize -> rerun the fit with the same --size.
- Then "막대기 같은데 … 체인같은 느낌": species TailFollow (tiger 0.85) - bones keep a share of their world heading as
  the hips turn/tip/bob (rr.Tail.Follow); "더 격하게" -> 1.7 with damping 0.35, FollowCurve 1.2; then split: walking 0.65/0.5 (heavy), only hard turns 0.35/1.7 (TailTurnDamping/TailTurnFollow, eased back over ~1 s). Turn test sequence = walk= direction changes (zigzag, U-turns), TigerTurn_off|on.
- Tripo Model tab: the 3rd input icon is BATCH (one model per picture, 300 credits); multiview is the cube icon.
- Recording timing bug fixed: time game seconds from the first `GAMESEQ <t> frame` line, not "clock starts"
  (`Tools/tailclips/frames.sh`, `clip.sh`); clips had ended on the user's desktop.
- Open: tiger cinematic parts/fur (like the fox's), portrait.
Related: [[fox-straight-tail-model]], [[tiger-texface-model]], [[ear-bones]], [[feedback-tails-drawn-straight]].
