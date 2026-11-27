---
name: parkour-gate-path
description: "Dice Town sky path (parkour) — round 103 (2026-11-27): on by default, rule-placed, bot nav links, spawn rule, stats, tiger hammer breaks walls"
metadata:
  node_type: memory
  type: project
  originSessionId: 5627b37d-3248-4e51-8715-2d8262c6ce52
  modified: 2026-11-27T07:58:16.435Z
---

2026-11-27 the user asked for a parkour space in Dice Town (proposal page https://claude.ai/artifact/WXmBzgb4PdW1Vv2Hq1pMDx), then "같이 손볼 내용 포함해서 그대로 진행해줘" + "호랑이는 길이 막히면 망치로 파괴하면서 전진" → round 103, ON by default (`-RRNoSkyPath` turns it off), then "끝나면 다 올려줘" (commit + push to PR #21).

- `ARRRandomDiceTown::AddSkyPaths` places it by rule from the cross road gate (`RRGate` constants) + the first flat roof past a gate end: dice stairs 2/4/6/8 m, stepping dice, 3 roof cover dice. Hops → one NavLinkProxy (`MakeSkyLinks`) — **must be spawned after the navmesh exists**, else links are silently ignored.
- Bots: `MarkRouteHops`/`HopAlong` (jump at edges, climb ≤2 m), roam perches, long guns perch to shoot. Test cvars `rr.Bot.SkyTestGoal "x y z"`, `rr.Bot.LogSky 1`; cheat `RRGoSky N`.
- Spawn spots under perches excluded; "sky stats" log lines for playtests.
- Tiger hammer swing breaks the wall ahead (`SwingSword`); tiger bots smash through when the detour is long (`SmashThrough`).

**How to apply:** details and test scripts in RRRandom HANDOFF "103차" and Tools/parkour103. Related: [[dice-spawn-zones]], [[tps-overhaul-plan]]
