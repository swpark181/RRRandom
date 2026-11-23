---
name: gun-support-hand-ik
description: "Left hand on held guns (2026-11-20): per-gun Support point + left-arm IK in the body anim instance, on by default for every animal friend; how it was measured and tuned; not committed"
metadata:
  node_type: memory
  type: project
  originSessionId: d1951319-148b-483f-8cd9-e7e74b779a40
  modified: 2026-11-20T07:36:21.453Z
---

2026-11-20 (user: "총기류에 손 위치를 어디에 둬야 적당한지 파악해서 체크해두고 모션 매칭을 사용해서 해당 위치에 손을 맞쳐줘. 지금은 공중에 뜨거나 허공에 손을 올리는 장면이 많이 나온다"). Cause: toy guns sit in the right fist like a socket; the left hand was never tied to anything, so it hung where the Mixamo rifle poses / matched clips put it.

Done (game, all species, `rr.Gun.TwoHands` default 1): `FRRGunLook::Support` + `SupportTurn` per gun in `RRRandomGunComponent.cpp` (SMG front grip; others just ahead of the trigger guard, 3.5 cm under the body, pitched 60), `URRRandomGunComponent::GetSupportHold` / `IsHeld`, and `URRRandomBodyAnimInstance::UpdateGunHands` + proxy `EvaluateGunHands` (runs after EvaluateBodyFacing: rebuilds the gun from the posed right hand, mirrored left fist like the sword grip's LeftFix, two-bone IK, collarbone up to 25 deg, fingers mirrored from the right hand). Live tuning `RRGunSupport <gun> x y z p y r`.

I told the user plainly that Pose Search cannot place hands (the GASP clips are unarmed); matching picks the body, IK puts the hand on the gun afterwards.

Measuring: `char-pipeline/scripts/blender_gun_support.py` (side view, underside profile, red/green fist rings). Checking: off/on pairs on Studio with `showhud` (hides the HUD) and `RRStudioCam 70 170 -15`; `guncam=body` and `guncam=left` views end up inside the big head or chest, don't use them. Page https://claude.ai/artifact/FKkYK2dgZaN52zZp2MZvyb (source `char-pipeline/output/gun_hands/page/`). HANDOFF top section + Docs/FoxMotionMatching.md 7-1. Not committed. Related: [[fox-motion-matching-test]], [[animation-studio-map]].

Shipped 2026-11-21: RRRandom PR #20 (merge 2a60731) carries all of this; earlier "not committed" notes above are out of date.
