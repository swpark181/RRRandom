---
name: dice-spawn-zones
description: "RRRandom 3:3 main mode spawns (round 102, 2026-11-26) — 11 flat-ground spots numbered 2..12, team start and every respawn by two dice; not committed"
metadata:
  node_type: memory
  type: project
  originSessionId: 5627b37d-3248-4e51-8715-2d8262c6ce52
  modified: 2026-11-26T06:55:13.978Z
---

User asked 2026-11-26: in 3:3 matches, random start points; roll two dice and respawn at the area matching the number; only ordinary flat ground. Built as round 102 (HANDOFF "102차"), not committed, waiting for the user's check.

- ARRRandomSpawnZones (RRRandomRounds.h/.cpp) + game mode FindSpawnZones / RollTeamZones / RollRespawnZone / PlaceForRespawn; only in ERRMatchRule::Rounds on Dice Town (studio has no spots → old homes).
- My own choices to confirm with the user: red rerolls until ≥4000 from blue at the start; respawn next to enemies is allowed (pure dice); numbering is clockwise from the map top, so 7 (most likely sum) lands wherever the order puts it.
- Floor marker material M_MRRing doesn't work on instanced meshes (shows the grid material) → one mesh per pad.
- Test: `Saved/Rounds99/seq.sh <tag> "<seq>" -RRRule=Rounds -RRRoundRule=0` (RRHurt 900 to see the dice; not with RRGod), bot match `run_match.sh <tag> 1`.

Related: [[tps-overhaul-plan]]
