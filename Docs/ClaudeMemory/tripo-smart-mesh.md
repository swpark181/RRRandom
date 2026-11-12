---
name: tripo-smart-mesh
description: "Tripo Studio Smart Mesh facts (2026-11-10): 100 credits geometry only, texture is a separate 30-credit click, export ignores the file name, topology resets to Quad 5000"
metadata:
  node_type: memory
  type: reference
  originSessionId: 088b3322-cd63-437c-9f6c-6ea30ee6446a
  modified: 2026-11-10T11:50:34.190Z
---

Tripo Studio Model tab → "Smart Mesh" (P2.0): 100 credits, outputs an untextured mesh; the "Texture 30" button under the viewer must be clicked afterwards (base color only). Topology defaults to Quad 5000 in a fresh tab/reload; switch to Triangle and type the polycount each time. Several tasks queue fine server-side (mesh + textures in parallel across tabs). Export (FBX/Blender) ignores the File Name field for Smart Mesh: zips come as tripo_convert_<id>.zip with textures named tripo_image_* or "<auto name>_basecolor", so move each zip to a named file right after its export. User asked for all RRRandom cover props as Smart Mesh on 2026-11-10 (see [[battle-royale-plan]], [[tripo-midjourney-queue]]). Smart Mesh also has the multiview input (second icon: Front/Left/Right/Back), confirmed 2026-11-11 on the fox (comparison https://claude.ai/artifact/JindwdJLCi9iPcf5yutFsC). Since 2026-11-11 every Tripo model must be Smart Mesh ([[feedback-tripo-always-smart-mesh]]).

Fur (2026-11-11, fox test): Smart Mesh gives cleaner hard shapes (belt, pouches, shirt hems) but flatter fur than HD Ultra, in both texture and silhouette. A Texture-tab multiview retexture (concepts, Remove Lighting, 4K, 20cr) helped only a little. Baking the HD texture onto the Smart Mesh doubled the eyes because the face width differs. Thin spikes beside the ears were fixed with char-pipeline/scripts/blender_shave_spikes.py --spiky 0.25 --passes 150 (0.45 left a stub). The fix lives only in the local FBX; the Tripo viewer still shows the spikes.
In-game fur (2026-11-11): every species but the dragon got Sky-style shell fur (fox --mask orange, rabbit --mask cream with RootShade 0.97, squirrel/tiger/red fox/cat/rat --mask all with per-species --band/--cloth; the HANDOFF has the exact commands), 12 layers (SK_FoxFur + URRRandomFurComponent, blender_fur_shells_skel.py, ue_import_fur_shells.py). It costs about 0.3 ms with 30 foxes; `rr.Fur 0` turns it off. Details in HANDOFF round 94/95.
