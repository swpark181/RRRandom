---
name: tps-overhaul-plan
description: "RRRandom TPS overhaul rounds 98–101 (2026-11-17~18): main mode, playtest fixes, network smoothing, view ghosts, lava escape, tiger shield; shipped together 2026-11-18; open choices listed"
metadata:
  node_type: memory
  type: project
  originSessionId: 088b3322-cd63-437c-9f6c-6ea30ee6446a
  modified: 2026-11-17T12:29:41.130Z
---

User answered Docs/TPSOverhaulQuestions.md on 2026-11-17: recommended defaults except:
- Match = 3 rounds (best of 3); each round's rule is one of deathmatch (first to N kills), most kills at time end, meteor collecting (Meteor Rush). Remaining old modes feed the random rounds; top-view hidden; Defense + Battle Royale go to an "Events" menu.
- No team cards / vote / comeback cards; only personal cards (pick 1 of 3) during play.
- Roles: Fox assault SMG + stealth; Tiger tank high HP + big hammer + shield; Rabbit scout high jump + sniper; Squirrel traps acorn grenade launcher; Dragon air fire while flying; RedFox ambush disguise + melee dagger.
- Respawn required: time grows each death, grows more when deaths are close together.
- Tripo credit cap 5,000. Don't /ship at the end — user reviews first.

Built the same day as round 98 (ERRMatchRule::Rounds; README "메인 모드 (98차)", HANDOFF 98차), result page https://claude.ai/artifact/5Byzowjs6uBtJHVSHEtDiU. Left uncommitted on branch claude/project-thread-ykg56g, waiting for the user's check before /ship.

Round 99 (2026-11-17, same branch, also uncommitted): the user's 15 playtest items (bigger dice/ultimate hints, friend-or-foe outlines, robot board/role/key HUD, enemy robot damage stages, per-gun shot trails, sprint feel, camera-in-body, acorn lob bug, bot routes, head turn limit, wreck frame drops). Result page https://claude.ai/artifact/V9XssS5aB7U6yTwuhgyFWG, details in HANDOFF "99차". Only auto captures and logs were checked; the user still has to play it. Same day follow-up (also uncommitted): Meteor Rush ore that lands where nobody can walk now hops to walkable ground and bots skip unreachable ore; shell fur kept off eyes and clothes by patching M_<Name>Fur with `ue_fur_bare.py` (Squirrel, Tiger, RedFox, Rabbit); town blocks split into 22-cell chunk meshes (no wreck hitch left). Test scripts: `Saved/Rounds99/` (run_match.sh + stalls.py, perf_wreck.sh, seq.sh).

Rounds 100–101 (2026-11-17~18): round 100 = network smoothing fix, view ghosts (FRRViewCut: top view, victory screen, kill cam), dragon/clone/acorn ultimates; round 101 = lava mash-to-escape, tiger shield in hand + Tripo motions with hand-set arms, DICE wording, proposals page https://claude.ai/artifact/13nkma8uTado7FPxtRbhCR and online play guide https://claude.ai/artifact/NMH9H85MtrgojL5Z9T7G5J. The user said "끝나면 모두 ship" on 2026-11-18, so rounds 98–101 were shipped together then. Still waiting on the user: which concept images (star lamp, kickboard, speech bubble) to build, block gloss and light level, 3.5-heads test, which ultimate redesign and new animal friend first, match length defaults (12 KO / 50 ore / 35 s). Test scripts are kept in `Tools/rounds99/` (copy to `Saved/Rounds99/` to run).

**Why:** direction analysis ([[battle-royale-plan]] etc.) said the game lacks one identity; this is the chosen main mode.
**How to apply:** bot matches use `Saved/Rounds98/run_match.sh <tag> <n>` (-RRSpectate, -benchmark); new weapon models start from Midjourney concepts in Tripo ([[feedback-first-concepts-midjourney]], [[feedback-tripo-always-smart-mesh]]); run UE python imports from bash, not PowerShell (PowerShell splits `-dpcvars=Interchange.FeatureFlags.Import.FBX=0`).
