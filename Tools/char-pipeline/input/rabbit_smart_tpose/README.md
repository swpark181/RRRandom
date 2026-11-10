# T-pose rabbit for Mixamo

The previous A-pose candidate did not rig cleanly: the user reported hands/arms
moving with the torso. Regenerated from a strict front T-pose reference, with
horizontal straight arms and wide empty space beneath hands and forearms.
The user will perform Mixamo rigging manually; stop at the Orient stage.

- Reference identity: `../rabbit_redesign/concept_front.png`.
- New front reference: `concept_tpose.png`, generated with built-in ImageGen.
- Tripo browser task: `3dd7c056-ec88-488e-9394-92c91435c6c3`.
- Settings: Smart Mesh P2.0, Quad, requested 8,000 faces, one generation.
- Geometry result reported by Tripo: 9,340 faces, 8,457 vertices.
- Texturing: Tripo's Texture operation, same T-pose input reference.
- Existing candidates and gameplay assets are preserved.
- Original FBX/2K atlas: `tripo/`. Normalized unrigged OBJ: `rabbit_tpose.obj`
  (100 units tall, Y up, front +Z), `rabbit_tpose.mtl`, `rabbit_tpose_basecolor.png`.
- Mixamo upload package: `rabbit_tpose_mixamo.zip` (7.4 MB).
- Uploaded to the user's logged-in Mixamo Chrome tab and visually verified in
  the Orient screen. Stopped before Next/marker placement/auto-rigging as requested.
  Proof: `../../output/rabbit_smart_compare/mixamo_tpose_uploaded.png`.
- `mesh_check.json`: 8,457 vertices; 7,325 quads + 2,015 triangles = 9,340
  faces / 16,665 render triangles. Width 88.37 for height 100. Far left and right
  hands both occupy height 41.77–48.51, confirming a horizontal T stance.
  UV coordinates and both far-hand vertex groups verified; no armature added.

## ImageGen prompt

Use case: precise-object-edit. Edit target: the supplied cream moon-rabbit character image. Create a production image-to-3D modeling reference of this EXACT same rabbit in a strict symmetrical rigging T-pose. Preserve the exact cute round face, brown eyes, small pink nose, fluffy cream fur, one upright ear and one folded ear, forehead tuft, plump short body, dusty plum short-sleeve crescent-moon T-shirt, ochre cuffed shorts, brown belt and two belt pouches, bare rabbit feet, and the established high-quality stylized 3D look. Change ONLY pose and framing as necessary. Both upper arms and forearms must extend perfectly straight horizontally sideways at shoulder height, elbows straight, forming a true horizontal T across the body. Hands at the far left and right, palms down, short rounded paw fingers relaxed and slightly spread with a distinct thumb. Clearly visible unobstructed empty background beneath each entire outstretched arm and hand, with wide gaps from torso, waist and pouches; no part of either hand or forearm touches or overlaps the body. Short sleeves follow the horizontal arms and leave a clear armpit silhouette. Legs straight, feet flat facing the camera, hip-width apart, with a clear narrow gap between legs and feet. Front orthographic view, centered full body including both entire hands, ears and feet, generous 8% margins on every side; no cropping, no perspective, no gesture, no bent elbows, no A-pose, no hands resting beside hips. Neutral plain light grey studio backdrop and even diffuse lighting, minimal ground shadow, no props, weapons, text, labels or other characters. This image will be used to generate a single 3D character that can be auto-rigged in Mixamo, so separation of the arms/hands from the torso is the highest priority.

## Applied to the game after the user's Mixamo rigging

Confirmed the user's finished Review and downloaded the real 57-bone skin as
`mixamo/rabbit_mixamo_original.fbx`. `mixamo/rabbit_SK.fbx` is the game export of
that same rig and weights, with `rabbit_tpose_basecolor_clean.png` assigned.
Both distal hands have zero torso-bone weights. Hips: 26.671639 cm; game scale: 1.9.
The game now uses this model with all 23 existing motions, resampled at 30 fps.

Back-texture retouch: built-in ImageGen edited the rear cloth view into
`shirt_back_edit.png`. `blender_rabbit_back_patch.py` reprojects the edit into the
original UV map using the edited/original-view ratio to remove lighting.
Only 96,722 visible purple rear-torso texels change; front, collar, cuffs, fur,
belt and shorts are protected. Original rig and atlas are preserved.
Evidence and validation: `../../output/rabbit_tpose_game/`, including
`after/shirt_back.png`, `game_front.png`, `game_back.png`, and `game_verified.log`.
Previous game assets remain in `backup/pre_mixamo_tpose/Rabbit/`.

### Back-texture ImageGen prompt

Use case: precise-object-edit. Edit target is the provided close-up 3D render of the BACK of a cream rabbit's plum-purple T-shirt. Texture retouch for a game asset. Preserve the image's EXACT framing, camera, garment silhouette, T-pose, collar, sleeve shapes, stitching positions, lighting, colors, fur, tail, belt, belt loops, shorts, and all geometry. Change ONLY the fabric texture across the purple rear torso panel: remove the unnatural large V-shaped/zigzag stretched woven patterns, random smeared creases and lumpy directional weave in the center-left and lower back. Replace them with an even, fine, soft cotton jersey knit matching the clean purple fabric near the upper-right back and sleeves. Keep the same muted plum-purple hue and existing gentle large-scale lighting/shading. The back should be a believable simple continuous plain T-shirt panel, with only very subtle natural drape and fine consistent knit grain. Preserve the collar seam, sleeve cuffs and vertical shoulder/armhole seams exactly. No print or logo on the back, no text, no new seams, no decorative patterns. Do not change the cream fur or tail, orange shorts or brown belt. Pixel-aligned full image retouch, not a new view or redesigned garment.
