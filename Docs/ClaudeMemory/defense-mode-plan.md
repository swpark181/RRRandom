---
name: defense-mode-plan
description: "RRRandom defense mode decisions (2026-10-26) — top view/DiceLand hidden, 7 waves, rat/cat villains, 3 growing stages, stage unlocks, room list"
metadata:
  node_type: memory
  type: project
  originSessionId: 860a222c-a9c6-45fd-8f64-4fff69c5e33b
  modified: 2026-10-26T14:47:30.907Z
---

User's spec + answers (2026-10-26, round 90):
- Title: top view removed, only TPS; only Dice Town for battle; other maps/top view kept as resources (dev args still open them). Page after view = BATTLE (Dice Town) / DEFENSE → stage 1-3 (locked ones show a lock).
- Defense: all players one team; solo = team of 3 (bots fill), online up to 5 players. Same difficulty regardless of player count.
- Giant dice tower in the middle, ringed by walls that break after enough damage; tower has HP, destroyed = defeat. Allies respawn as usual.
- 7 waves, more and stronger villains each wave; 5 s break between waves. Cat mid-boss at wave 4, strong cat boss at wave 7, rats otherwise. Rats go for the tower but hit nearby friends / blocking walls first; cat chases friends.
- Broken walls partly repair between waves (never fully).
- Villains: cute house style, mischievous thieves (masks, came to steal dice). Rat melee bite/body slam; cat claw + one AoE slam. Waves scale size/colour/HP/damage of the same rat model.
- Character pipeline: Claude makes 2 MJ concepts per villain and picks the better one itself, then proceeds (no user pick).
- Stage size: stage 1 = 1/2 Dice Town, stage 2 = 2/3, stage 3 = full Dice Town size. Entry paths always more than the number of users.
- Vehicles: star robot only for boss fights; the other vehicles get added one per stage as you progress. Villains can't use level devices (later).
- Dice (buffs, giant, flight, ultimate) work as usual; villains drop dice.
- Map concepts: just build from these answers, user fixes later (no concept page first).
- Progress saved locally per PC. Host can only open stages it unlocked. Joiners join any room; a clear counts for a joiner only if they had cleared the stage just before it.
- Simple room list UI: list + stage info.

**Why:** user's own design for the defense mode.
**How to apply:** follow these when building or changing defense mode; see [[call-them-animal-friends]] for wording, [[fox-v3-remake]] for the character pipeline.

Built in round 90 (2026-10-27), see HANDOFF "90차": my interpretations awaiting user feedback — stage size = width (not area), "one vehicle per level" = per stage, walls block villains only (friends/shots/rides pass). Villains = rat A-3 / cat A-2 (Midjourney), in /Game/Characters/Rat, Cat; reimporting a species already loaded by game code needs its folder deleted first and one import run.
Later the same day the user replaced the wall ring: an unbreakable grid castle (3 thick, 4 high, walkway + battlements, stairs inside) that blocks players and shots, and only two gates (east/west) to break; the rush comes from 4 sides and heads for the gates. Gate doors let friends through (my call, to confirm).
