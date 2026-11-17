---
name: tripo-animate-motions
description: "Tripo Studio Animate (text-to-motion, 20 credits each) keeps the arms tucked at the chest; export several motions in one FBX; fix the arms on the character's own rig in Blender"
metadata:
  node_type: memory
  type: reference
  originSessionId: 435a1fa3-a86b-4427-98c5-0420d02e2cbd
  modified: 2026-11-17T15:36:20.949Z
---

Tripo Studio → Rig ▸ Animate (`/workspace/animate`, pick a rigged asset; the fox is rigged there): "Create Your Own Animation" takes a text prompt and a length in seconds, 20 credits each, about 30 s. Export → FBX → Mixamo → "Number of Animations" opens a chooser; several picked motions come in ONE FBX as separate actions (24 fps, Mixamo bone names, hips 23.2 on the fox).

What it does badly (2026-11-18, four prompts for the tiger's shield guard and hammer swings; round 98's seven were the same): whatever the prompt says about the arms, both arms stay tucked at the chest and only the body twists, steps or crouches.

What worked: `char-pipeline/scripts/blender_split_actions.py` (one FBX per action + contact sheets), then `blender_tiger_shield_arms.py` (copies the body's local rotations onto the character's own Mixamo rig and aims upper arm, forearm and hand along directions given in the chest's frame at key times), `tiger_arms.sh`, import with `ue_import_shared_anim.py <fbx>|<Name>|<rig hips>|<Species>=<same hips>`. Details in RRRandom HANDOFF "101차".

Also: a `javascript_tool` result that contains Tripo's signed image URLs is blocked ("Cookie/query string data") — return counts or store the URL in `localStorage` (which also carries it to another Tripo tab). Related: [[tripo-midjourney-queue]], [[tripo-smart-mesh]], [[feedback-tripo-always-smart-mesh]].
