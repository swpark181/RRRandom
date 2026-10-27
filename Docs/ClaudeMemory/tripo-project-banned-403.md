---
name: tripo-project-banned-403
description: "Tripo image Generate silently ignored = 403 code 2032 \"Your project has been banned due to violation of our policy\" (2026-10-23)"
metadata:
  node_type: memory
  type: reference
  originSessionId: d5685139-8d8c-4e46-a744-b7ddd34cea0d
  modified: 2026-10-23T05:16:07.921Z
---

2026-10-23 14:14, Tripo Studio Image tab (Nano Banana Pro, ref `haetae_combine_1_clean.png`, 1:1): right after a successful generation, the next Generate click did nothing — no row, Free count unchanged, even after page reload and re-upload. Console showed `POST https://api.tripo3d.ai/v2/studio/image/gen_image_v2: 403`; patching `window.fetch` to log the body gave `{"code":2032,"message":"Your project has been banned due to violation of our policy","suggestion":"Please try another project"}`. The prompt was a harmless "chunky block super robot" body redesign. Did not retry further (account risk) and told the user.

**How to apply:** when Generate is silently ignored and the usual fixes (wait for history, real keystroke — see [[tripo-prompt-filter]]) don't help, read the console for gen_image errors or log the fetch response before retrying; on code 2032 stop and ask the user (maybe a new project / Tripo support), don't work around it.

Update 2026-10-23 ~14:40: the next request (a different prompt, a new reference upload, after reopening the page) went through normally with 200 OK (`remaining_free_count` 135). So code 2032 rejected that one request (prompt/reference combination), not the account. If it happens again: try a reworded prompt once, check the fetch log, and stop if it is still 403.
