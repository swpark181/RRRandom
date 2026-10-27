---
name: smart-app-control-dll
description: "RRRandom won't start (GetLastError=4551) when Windows Smart App Control blocks a freshly built game DLL; fix by rebuilding with a real code change"
metadata:
  node_type: memory
  type: project
  originSessionId: 0123e4a7-8e0e-4fcf-a13b-ccf84f62a294
  modified: 2026-10-20T07:05:34.389Z
---

On the user's PC, Windows Smart App Control sometimes blocks a newly built `C:\work\RRRandom\Binaries\Win64\UnrealEditor-RRRandom.dll`:
the log shows `Failed to load ... (GetLastError=4551)` then "The game module 'RRRandom' could not be loaded", and the
CodeIntegrity/Operational event log has 3077/3033 events for policy {0283ac0f-fff1-49ae-ada1-8a933130cad6}. Seen 2026-10-14,
2026-10-15 and 2026-10-20 (a log-text change and rebuild fixed it again).

**Why:** the block is per binary; touching a file and relinking gives the same bytes and stays blocked.
**How to apply:** make a small real code change (e.g. a log text), rebuild, then confirm with a short headless run that the
log has no 4551. Don't change the security setting yourself; turning Smart App Control off is the user's call.

Other faces of the same block (2026-10-23, it hit 6 times in one day): the game hangs right after loading the module (log stops at "removing startup thread", CodeIntegrity 3077), or the user sees a Windows dialog "UnrealEditor-RRRandom.dll … 오류 상태 0xc0e90002". Same fix: change a log text, rebuild, check a short run gets past startup. Sometimes it takes two rebuilds.

2026-10-27: 9 rebuilds in a row (log texts and constants changed) all stayed blocked; what worked was restoring the source to the exact state of the last build that had loaded — the build came out byte-identical and loaded. So keep a copy of the last loading DLL's source (and the DLL) before changing code; if new builds keep failing, rebuild that state so the user can play, then try the change again later (the next real change did load).
