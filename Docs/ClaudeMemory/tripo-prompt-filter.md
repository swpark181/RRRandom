---
name: tripo-prompt-filter
description: "Tripo Image rejects prompts containing \"no clothes\" with a misleading \"image does not meet content guidelines\" error"
metadata:
  node_type: memory
  type: reference
  originSessionId: 0123e4a7-8e0e-4fcf-a13b-ccf84f62a294
  modified: 2026-10-15T04:02:12.277Z
---

Tripo Image (studio.tripo3d.ai) answers a flagged *prompt* with the toast "This image does not meet our content
guidelines. Please upload a different image." — it blames the reference image even when there is none. Seen 2026-10-15
with "no clothes, no shirt" in a creature prompt; "with no outfit and no accessories" passed.

**How to apply:** when that toast appears, test the prompt alone before swapping the reference image; avoid nudity-like
wording ("no clothes", "naked") for creatures. In the Export dialog set the file name with form_input — typing after a
coordinate click can land in the 3D viewer as hotkeys and switch assets. See [[tripo-cartoon-style]].
