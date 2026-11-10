---
name: tripo-smart-mesh
description: "Tripo Studio Smart Mesh facts (2026-11-10): 100 credits geometry only, texture is a separate 30-credit click, export ignores the file name, topology resets to Quad 5000"
metadata:
  node_type: memory
  type: reference
  originSessionId: 088b3322-cd63-437c-9f6c-6ea30ee6446a
  modified: 2026-11-10T11:50:34.190Z
---

Tripo Studio Model tab → "Smart Mesh" (P2.0): 100 credits, outputs an untextured mesh; the "Texture 30" button under the viewer must be clicked afterwards (base color only). Topology defaults to Quad 5000 in a fresh tab/reload; switch to Triangle and type the polycount each time. Several tasks queue fine server-side (mesh + textures in parallel across tabs). Export (FBX/Blender) ignores the File Name field for Smart Mesh: zips come as tripo_convert_<id>.zip with textures named tripo_image_* or "<auto name>_basecolor", so move each zip to a named file right after its export. User asked for all RRRandom cover props as Smart Mesh on 2026-11-10 (see [[battle-royale-plan]], [[tripo-midjourney-queue]]).
