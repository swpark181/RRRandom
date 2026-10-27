---
name: chrome-needs-restart
description: "Claude in Chrome tools don't reach a running conversation after /chrome; the user must restart with claude --chrome --continue"
metadata:
  node_type: memory
  type: reference
  originSessionId: 275fad39-f5fc-417c-9c74-3fc2f992b035
  modified: 2026-10-19T06:13:52.946Z
---

On 2026-10-19 the user ran `/chrome` mid-conversation, but no `mcp__claude-in-chrome__*` tools appeared (ToolSearch found nothing). After the user restarted with `claude --chrome --continue` they loaded fine.

**How to apply:** when Tripo Studio (or any browser work) is needed and the Chrome tools are missing, ask the user straight away to restart with `claude --chrome --continue` instead of retrying `/chrome`. Tripo has no usable API key (see [[dice-land-art-direction]]), so the browser is the only way in.
