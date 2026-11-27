---
name: feedback-perl-rep-dollar1
description: "Editing C++ with perl helper scripts — never pass \"$1...\" into a sub as the replacement; it interpolates before the match and silently deletes the anchor text"
metadata:
  node_type: memory
  type: feedback
  originSessionId: 01b70a0c-fd4e-460b-89a1-f962ea75fc2a
  modified: 2026-11-23T02:01:42.548Z
---

In round 81 a perl helper `rep(qr/(anchor)/, "$1new text")` deleted every anchor line (the replacement string was built before the match, so `$1` was empty), breaking the robot header, AI and HUD until repaired. The bash tool also failed to parse long quoted heredocs with apostrophes several times.

**Why:** costly silent damage across several files; caught only by the compiler.
**How to apply:** for multi-line C++ edits use the Edit tool, or perl `s/(...)/$1.../` written directly (not via a sub), or anchor-insert scripts that insert before/after a literal anchor with \Q..\E and check the match count; write long scripts with the Write tool, not heredocs. Check `git diff` after scripted edits.

Same trap in Python (2026-11-23): replacement text with Windows paths inside a quoted Python string (`rig_mx\tiger…`, `output\fur…`) turned `\t` and `\f` into a tab and a form feed in HANDOFF.md without any error. For prose with backslash paths use the Edit tool; in a script build the backslash with `chr(92)` or raw strings, and grep the result for the paths afterwards.
