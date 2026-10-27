---
name: cannon-plane-design
description: "RRRandom round 88 scope (2026-10-26): only the human cannon + dice-rolled planes; building/terrain destruction and lava dropped"
metadata:
  node_type: memory
  type: project
  originSessionId: 6e91fd45-50bb-49fb-9deb-3c683a54091e
  modified: 2026-10-26T13:24:05.849Z
---

2026-10-26 the user cut the idea list (flying vehicle, rideable cannon, all buildings destructible, terrain destruction + lava from the mantle) down to **cannon and planes only**. Plan doc: https://claude.ai/artifact/6wE76g4UFdoURXDwWG7CER (written before the cut; its destruction/lava sections are not being built).

User's rules:
- Cannon = jump-pad style: fixed landing pad, always built as a cannon + pad set, **one-way, no return cannon**, about **4 sets** in Dice Town, for crossing the map end to end. **Shooting is allowed while flying** from a cannon.
- Planes: board at a station; F → a die rolls → 1-2 wooden plane, 3-4 fighter, 5-6 spaceship appears and you board. **1 seat, the pilot flies and shoots**. Each type has its **own weapon** plus tiered stats. Shot down → the plane drops straight to the floor below and explodes, breaking blocks; the pilot is **thrown out and glides down**.
- Concepts via Tripo (Midjourney / Nano Banana Pro) straight through to 3D without a pick step.

**Done 2026-10-26 (round 88):** Midjourney (in Tripo Image) 4 images per object → picks cannon #2, pad #2, station #1, wood #2, fighter #1, spaceship #3 → single-image H3.1 Ultra 4K 20k (55 credits each, 21,335 → 21,005). Assets `/Game/Props/Cannon/SM_Cannon|SM_CannonPad`, `/Game/Vehicles/SM_PlaneWood|Fighter|Spaceship|Station`; sources `char-pipeline\input\r88_vehicles\`. Code `ARRRandomCannon`, `ARRRandomPlaneStation`, plane kinds in `RRRandomVehicle` (HANDOFF 88차). Result page https://claude.ai/artifact/7FJZkYEr1nLYzPpnWKVJAg. Gotcha: `blender_vehicles.py` puts the longest side on X, which is wrong for planes (wingspan ≈ length) — pass an explicit turn and check with `scripts/blender_top_profile.py` (also finds seat height/X from the cockpit dip).

**Why:** the user wants travel/mobility toys, not map destruction.
**Update 2026-10-26 (later):** the user asked again for the other two → round 89 = all-buildings destruction + terrain
digging with mantle lava, Dice Town only. Picks (all the recommended ones): explosive attacks + CANNON gun break things,
giant dice/pedestal breakable, unsupported upper parts collapse; normal holes mend after 15 s, collapses and lava after 45 s;
lava boils until mended, then blocks regrow over it. Spec = plan doc sections 3-4. Related: [[tripo-cartoon-style]], [[call-them-animal-friends]]
