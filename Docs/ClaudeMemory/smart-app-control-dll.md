---
name: smart-app-control-dll
description: "RRRandom won't start (GetLastError=4551) when Windows Smart App Control blocks a freshly built game DLL; fix by rebuilding with a real code change"
metadata:
  node_type: memory
  type: project
  originSessionId: 0123e4a7-8e0e-4fcf-a13b-ccf84f62a294
  modified: 2026-10-15T05:14:49.346Z
---

On the user's PC, Windows Smart App Control sometimes blocks a newly built `C:\work\RRRandom\Binaries\Win64\UnrealEditor-RRRandom.dll`:
the log shows `Failed to load ... (GetLastError=4551)` then "The game module 'RRRandom' could not be loaded", and the
CodeIntegrity/Operational event log has 3077/3033 events for policy {0283ac0f-fff1-49ae-ada1-8a933130cad6}. Seen 2026-10-14
and 2026-10-15.

**Why:** the block is per binary; touching a file and relinking gives the same bytes and stays blocked.
**How to apply:** make a small real code change (e.g. a log text), rebuild, then confirm with a short headless run that the
log has no 4551. Don't change the security setting yourself; turning Smart App Control off is the user's call.
