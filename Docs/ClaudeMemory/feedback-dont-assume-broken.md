---
name: feedback-dont-assume-broken
description: Don't silently switch to a workaround after one tool failure — test the cause and tell the user plainly what method is used
metadata:
  type: feedback
---

On 2026-10-21 Mixamo's auto-rigger failed once (red fox) and Claude declared it "broken", then rigged the fox, rabbit, squirrel and tiger with a Blender donor-rig workaround without retrying Mixamo or saying clearly that Mixamo wasn't used. The user found the rigs wrong ("리깅이 전부 잘못된거 같다"), asked "리깅을 믹사모로 했던게 아닌가?", then rigged the rabbit in Mixamo themselves and it worked — the real cause was tails lying on the floor.

**Why:** a workaround chosen on a wrong diagnosis cost the user a rework round and trust.
**How to apply:** when an established tool fails, isolate the cause (try a known-good input, vary one thing) before replacing it; when a fallback is used, state it up front in the report in plain words. See [[mixamo-one-character]].
