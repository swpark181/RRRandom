---
name: feedback-perl-rep-dollar1
description: Editing C++ with perl helper scripts — never pass "$1..." into a sub as the replacement; it interpolates before the match and silently deletes the anchor text
metadata:
  type: feedback
---

In round 81 a perl helper `rep(qr/(anchor)/, "$1new text")` deleted every anchor line (the replacement string was built before the match, so `$1` was empty), breaking the robot header, AI and HUD until repaired. The bash tool also failed to parse long quoted heredocs with apostrophes several times.

**Why:** costly silent damage across several files; caught only by the compiler.
**How to apply:** for multi-line C++ edits use the Edit tool, or perl `s/(...)/$1.../` written directly (not via a sub), or anchor-insert scripts that insert before/after a literal anchor with \Q..\E and check the match count; write long scripts with the Write tool, not heredocs. Check `git diff` after scripted edits.
