---
name: ue-commandlet-hangs-at-exit
description: "UnrealEditor-Cmd -run=pythonscript often never exits after the script is done (RRRandom, UE 5.8); watch the log for the script's DONE line, then kill it"
metadata:
  node_type: memory
  type: reference
  originSessionId: 435a1fa3-a86b-4427-98c5-0420d02e2cbd
  modified: 2026-11-17T12:29:41.253Z
---

On this PC `UnrealEditor-Cmd.exe RRRandom.uproject -run=pythonscript "-script=..."` finished its script (asset saved, "DONE" in the log) but the process stayed alive for 40+ minutes (seen 2026-11-17, round 99, `ue_make_team_outline.py`). While it lives the module DLL is locked, so a build fails or waits.

**How to apply:** run it in the background with `-abslog=<file>`, poll that log for the script's last line, then `taskkill //PID <pid> //F` (find it with `ps -W | grep UnrealEditor-Cmd`). Check the asset's timestamp to confirm it was saved.

Related: edit scripts for this repo must cope with mixed line ends (some sources are CRLF, some LF) — see [[feedback-perl-rep-dollar1]].
