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
