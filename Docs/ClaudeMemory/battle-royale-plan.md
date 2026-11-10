---
name: battle-royale-plan
description: "RRRandom battle royale (r92 mode, r93 cotton candy cloud ring + oracle dice meteors, built 2026-11-10): user spec, how it is built, numbers to confirm"
metadata:
  node_type: memory
  type: project
  originSessionId: 088b3322-cd63-437c-9f6c-6ea30ee6446a
  modified: 2026-11-10T11:50:27.344Z
---

User's spec (2026-11-10): new BATTLE ROYALE mode; teams of 3, max 10 teams / 30 animal friends; every empty slot is a bot at start; BR bots can do every action a player can; level in the current Dice Town style built for urban fighting (walls, buildings, castles), 20x the current level; streaming and LOD must work at that size; anything needing mid-way tests is tested on a test level. "진행시켜" (go ahead without asking).

Built round 92 (2026-11-10, not committed yet): title BATTLE ROYALE → BIG MAP / TEST MAP; levels /Game/Maps/BattleRoyale (7x7 tiles) and BRTest (3x3, 4 teams, ring x2) made by char-pipeline/scripts/ue_make_battle_levels.py; map = 49 ARRRandomDiceTown tiles (FRRBattleTile) spawned by ARRRandomBattleMap, which streams tile detail by view distance (9000) and shows a merged far look (LOD); navmesh only round invokers (URRRandomNavigationSystem); ARRRandomBattleZone 6 phases. Measured: big map builds in 8.4 s, 84-86 fps with 30 animal friends, +1 GB over Dice Town. LAN pair test: lan_pair_seq.ps1.

My interpretations (told the user, to confirm):
- 20x = area (precedent: r19 "면적 2배", r36 Dice Town): ~4.3x the width, 75,600 square, round island.
- Human players fill squads in join order (3 per team); no respawn; shrinking ring; last team standing wins; joiners after the start take over a standing bot.

Ring design, built round 93 (2026-11-10, not committed; briefing https://claude.ai/artifact/6tuVrYcz8KUVajEM7NCEA7; classes ARRRandomOracleStorm/ARRRandomOracleMeteor, cheats RROracle [1], RRRingEdge N): outside the ring = pink cotton candy cloud; inside it each screen shows floating pink sugar/cloud; slow + damage grow with distance from the ring centre (my numbers: t = depth/8000, dps x(1+2t), speed x(1-0.4t)). Oracle dice meteors, user-decided: after each shrink ENDS; every team's 3 slots roll (down/left slots auto-roll), lowest team sum; target = random LIVING member; meteor zone centred on the target = 1/4 of the clear (in-ring) AREA (radius = ring/2), tune danger by meteor count so later is deadlier; the zone is drawn on the minimap/full map ONLY, the ground shows only each meteor's landing pad; meteors hit every team, smashed buildings stay. My proposal to confirm: 30/28/20/12/4 meteors (hit chance standing still 8%→87%), fall 10→4 s, pad lead 3→1.5 s, 5 rolls (not after phase 6).


Round 94 (2026-11-10, built, not committed; briefing https://claude.ai/artifact/AhefFbehDek2n31huAJsas): user wants indoor fights: up to 3-story buildings big enough to fight inside, with cover; "objects use Tripo by default". Built 4 block-built big buildings (mall 36x28 3F atrium, toy factory 32x22 + mezzanine, school 34x14 3F corridor, parking garage 30x24 2F + roof with ramps) placed per district (~18 on the big map, all 4 on BRTest); cover is placeholder blocks. Cover props done the same day: 13 Tripo models (user picks: crate 2, shelf 4, counter 3, vending 3, rest my recommendations) drawn over hidden cover blocks (AddCoverModel), 390 credits. Pipeline: Tripo image-to-3D -> export FBX named bc_<name> -> unpack_tripo_named.sh -> blender_props.py -> ue_import_props.py (nanite). Level size was enough (quarter of a tile 47x47 m). Then the leftover blocks were finished too: kiosk, rooftop AC and water tank got Tripo models (90 more credits, now 24,720), kitchen -> shelf, control room -> machine, garage cars -> the drivable SM_Car. Every mapped cover block now has a model; cheat RRGoCover N checks one. User then had all 16 regenerated as Tripo Smart Mesh (mesh 100 + separate texture 30 = 130 each, 2,080 credits, now 22,640; HD kept in input/props94/tripo_hd), and blocks under a model no longer hide the floor under them (HidesFace).

**Why:** the user's own design; the open choices above were mine.
**How to apply:** follow when changing BR; HANDOFF "93차"/"92차" have the details and open items (8.4 s freeze while building, far look ignores wrecks, oracle roll is automatic: E is the dice key). Any material on ISMs needs the InstancedStaticMeshes usage flag (translucent ones also a non-Nanite mesh), or the game draws default grey; fix with char-pipeline/scripts/ue_instanced_usage.py <mesh>. See [[defense-mode-plan]] for the same pattern of a rule built on Dice Town, [[call-them-animal-friends]] for wording.
