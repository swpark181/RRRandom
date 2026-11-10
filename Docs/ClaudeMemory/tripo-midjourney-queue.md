---
name: tripo-midjourney-queue
description: "Tripo Studio Image tab, Midjourney v8.2: queueing many prompts at once makes most fail; one (or two) at a time works; some prompts keep failing"
metadata:
  node_type: memory
  type: reference
  originSessionId: 088b3322-cd63-437c-9f6c-6ea30ee6446a
  modified: 2026-11-10T10:15:40.358Z
---

2026-11-10 (RRRandom round 94 prop concepts, 14 prompts): queueing 14 Midjourney v8.2 jobs back to back made 7 end in "Generation failed. Your credits will be refunded automatically." (free count is refunded) and others hang 5+ min. One or two at a time finished in ~30 s. A few prompts failed 3-4 times even alone (a stack of crates, a cafe table with cake/milkshake, a dice-stamping factory machine) — those were made with Nano Banana Pro instead and that was told to the user. Results come back as 2048 px; many files are really WebP even when saved as .png (System.Drawing then says "Out of memory") → convert with `C:\work\char-pipeline\tools\ffmpeg\ffmpeg.exe`. Signed result URLs are blocked from tool output; download in-page (fetch → blob → `<a download>`). JS calls longer than 45 s time out, and browser_batch with more than ~60 s of waits times out too.

**How to apply:** for Midjourney concepts in Tripo, queue at most two, wait, check, then the next; after 2-3 failures of the same prompt, reword or fall back to Nano Banana Pro and say so ([[feedback-first-concepts-midjourney]]).
