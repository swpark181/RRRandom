# Round 101 research notes (material for the proposal page; from two research agents, 2026-11-17)

Marks: [O] = the agent opened the page and the claim is on it. [S] = search-result summary only. Difficulty = agent's estimate.

## A. Ways games make characters read as cuter (18)

1. Big head / low heads-tall. Splatoon: Nogami "You can tell when a character is covered in ink from an attack because the head is big. It isn't so just to make them look cute." [O] http://nintendoeverything.com/splatoon-staff-talk-about-the-inkling-design-more-on-the-squid-sisters-and-other-topics/ — UE5: high if remodelled; low as a test (head bone scale 1.1–1.2 in AnimBP).
2. Baby/toddler framing. Astro Bot: "big cute eyes", "waddle like toddlers", "a low center of gravity and a compact frame". [O] https://blog.playstation.com/?p=395223 — medium (new walk/run cycle).
3. Low hips, wiggling run ("unathletic but ambitious"). Fall Guys: skeleton "strangely close to the ones that were made for the Minions", "super low hips". [S, page 410] https://www.ggrecon.com/articles/fall-guys-developers-reveal-what-makes-their-jelly-beans-so-lovable — medium (additive hip sway).
4. Round, volume-preserving body + squash and stretch. Kirby Forgotten Land: "always changing shape, squashing and stretching" [O] https://www.nintendo.com/sg/interview/arzg/index.html ; Kena Rot "like a squishy blob" [O] https://80.lv/articles/ember-lab-on-art-animation-behind-kena-bridge-of-spirits/ ; Brawl Stars hat stretch [S] https://medium.com/betweentheframes/animation-fundamentals-brawl-stars-34ffb10bd32c — low–medium (non-uniform scale on jump/land/hit/dash).
5. Silhouette first, one silhouette per species. Animal Crossing NH (CEDEC 2020): "It all starts with the silhouette". [O] https://www.siliconera.com/nintendo-on-how-animal-crossing-new-horizons-lovable-villagers-were-created/ — low as an audit.
6. Eyes carry emotion by shape swaps. Astro Bot: LED eyes "changing shape to convey a variety of emotions"; Splatoon: mouth corners and lower eyelids raised = "a cheeky mood". [O] (same URLs as 2 and 1) — medium (eye texture swap by material parameter on hit/KO/win).
7. Look-at and curiosity. Animal Crossing NH: "the villagers will look at the object and ponder about it". [O] Siliconera — low–medium (Look At / head aim to teammates, pickups, last attacker).
8. Secondary motion on ears, tails, hair. Kawaii Physics (free UE plugin, one AnimGraph node). [S] https://80.lv/articles/kawaii-physics-plug-in-now-supports-unreal-engine-5-5 , https://www.fab.com/listings/f870c07e-0a02-4a78-a888-e52a22794572 — low if bone chains exist.
9. Idle behaviours tied to personality, physical contact. Animal Crossing NH: "Villagers with an interest in music will be seen more frequently singing."; Kena: a Rot hops onto her while she sits. [O] — low–medium (random idle breaks in lobby/spawn/results).
10. Emote system with overhead symbols. Animal Crossing: 44 Reactions at NH launch, ~88 by 2.0 [O] https://nookipedia.com/wiki/Reaction ; Brawl Stars Pins: "Each brawler has its own set of pins that have animation and a voice line" [O] https://gamingonphone.com/guides/brawl-stars-guide-tips-to-get-and-change-pins-in-the-game — low–medium.
11. Win and lose as character moments. Fall Guys: "both ridiculous when they win and hilarious when they fail" [O] https://blog.playstation.com/?p=224711 ; Astro Bot Rescue Mission celebrations [O] https://blog.playstation.com/2019/01/24/astro-bot-rescue-mission-inside-the-art-and-animation-of-japan-studios-ps-vr-hit/ — medium.
12. Knock-outs as comedy, not pain. Kirby team; Party Animals: knocked-out animals lie unconscious then wake up [S] https://twinfinite.net/guides/how-to-wake-up-faster-party-animals/ — low–medium. (Dizzy stars as a named effect: unverified.)
13. Non-verbal vocal barks + camera close enough to hear. Astro Bot sounds "mix English and Japanese"; Pikmin 4: near-ground camera, "Pikmin's voices and footsteps loud and clear". [O] https://www.nintendo.com/sg/interview/ampy/03.html — low (sound cues on anim notifies).
14. Tactile material: fur, fuzz, vinyl. Party Animals: "make the fur look like actual fur on a small animal" [O] https://gamingbolt.com/?p=519411 ; Fall Guys: "chunky little yeti-shaped vinyl toy" [O] — low (fresnel fuzz/rim) to medium–high (denser shells).
15. Lower, closer camera at the character's eye level. Pikmin 4. [O] — low (spring arm, FOV; retest aiming).
16. Costumes that read small. Fall Guys: "Fall Guys are small in the game and little details can get lost", "be bright, be bold". [O] https://blog.playstation.com/?p=224711 — medium (socket accessories).
17. Character-to-character gestures. Fall Guys hug ("Fall Bae" trophy) [O] https://gamertweak.com/fall-guys-how-to-hug-fall-bae ; Overcooked 2 emote wheel [S] — medium (paired montage); one-sided wave low.
18. Baby versions. Squad Busters Baby form / Baby Battle [S] https://www.sportskeeda.com/esports/best-characters-baby-battle-game-mod-squad-busters — high.
Not found: developer sources for blink timing, blush, eye glints, rim light, portraits, name tags, speech bubbles.

## B. Heads-tall ratio (4등신?)

Bottom line of the agent: go to about 3.5 heads (measured without ears), not 4; keep 3 for the smallest, 4 only for big bruisers. Confidence moderate; nearly every ratio is the agent's own eyeball estimate from an official image (±0.3–0.5); no published figure was found for any game.

| Game | Heads tall (estimate) | Camera |
| Brawl Stars | 2.5–3 (Shelly ~3.0, Colt ~2.8 crouched, Edgar ~2.5, Spike ~1.6) | top-down |
| Squad Busters | ~2–2.5 (unverified) | top-down |
| Fall Guys | ~2 (bean) | third person, no aiming |
| Party Animals | ~2–2.2 | third person brawler |
| Eggy Party | ~1.2–1.5 | third person party |
| Splatoon 3 | ~3–3.5 | third-person shooter |
| Ninjala | ~3–3.5 | third person melee |
| PvZ Garden Warfare | plants ~1.5–2 (head is the gun), zombies ~3–3.5 | third-person shooter |
| Ratchet & Clank | ~4.1 | third-person shooter |
| Sly Cooper | ~4.3 | third person |
| Star Fox (Fox) | ~4.5–5 | third person on foot |
| Crash Bandicoot | ~2.7–3 | platformer |
| Biomutant | ~4–4.5 | third person |
| Gunfire Reborn | ~2.5–3 | first person |
| Super Animal Royale | ~2 | top-down 2D |
| MapleStory 2 | ~2–2.5 | high angle |
| Animal Crossing NH | ~2 (Isabelle ~2.1) | high angle |
| Overwatch 2 | Tracer ~7.5–8, Mei ~6.5 | first person |
| Foamstars | ~6.5–7 | third-person shooter |
| Sunset Overdrive | ~6.5–7 | third-person shooter |
| Fortnite | ~7 (unverified) | third-person shooter |
| Zootopia (Judy) | ~4.5–5 without ears | film |
| Kung Fu Panda (Po) | ~3.3 | film |

Developer statements: Splatoon (Famitsu 2015) big head shows ink, hands and feet made big for readable action https://www.famitsu.com/news/201509/09087846.html ; Sakurai: bird's-eye view makes "the characters' bodies seem shorter than they actually are", Isabelle vs Villager "All of her animations are different; not a single one is the same." https://sourcegaming.info/2018/09/26/sakurai-discusses-isabelle-echoes-and-newcomers ; Brawl Stars Melodie shortened April 2025, "bobblehead" complaints https://knowyourmeme.com/editorials/guides/whats-up-with-the-melodie-height-nerf-memes-on-reddit-the-jokes-about-brawl-stars-controversial-melodie-update-explained ; Overwatch (Hudson, paraphrased) taller proportions make cosmetics easier https://finalboss.io/yes-overwatch-characters-are-getting-sexier-but-it .
Findings: every game at ≤2.5 avoids behind-the-back aiming (top-down, party, first person); cartoon characters that aim from behind sit at 3–4.5; ~3 is the proven floor for holding guns (Shelly, Splatoon with oversized hands). Geometry (agent's inference): at 2 heads the arm is ~0.6–0.7 head long (hands can't meet across the chest or reach over the head), at 3 ~1.1–1.2, at 3.5 ~1.4, at 4 ~1.6. Baby schema: Lorenz 1943; Glocker et al. 2009 (Ethology 115:257–263) https://repository.upenn.edu/cog_neuro_pubs/11 — faces, not body ratio; so a 3–3.5 body with a round big-eyed face stays cute. Cost: animations don't carry over between body types (Sakurai); UE retargeting needs manual setup https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-retargeting-in-unreal-engine ; hitbox/camera/cover can stay if total height and capsule are kept (inference).

## C. Roster size (earlier agent, round 100): minimum 12 (two per role), comfortable 15–18; Brawl Stars beta and Omega Strikers both launched 3v3 with 15.

## D. Measured this session
- Bots online vs solo (100 s each, real time): stalls 0 / 0, unsticks 2 / 0, stopped 2.8% / 2.2%, pushing at a wall 0.1% / 0.1%.
- Net smoothing (joiner's screen, worst jump median per second / jumping frames): 100±60 ms 3% loss: 27 → 5, 12.7% → 1.4%; 200±100 ms 5%: 39 → 6, 15.8% → 1.1%. Host's screen: 31 → 4 and 33 → 4.
- Lava: standing still a 100-health fox is knocked out about 3.6 s after falling in; 6 presses of SPACE (about 1.2 s) and it leaps out toward dry ground.
