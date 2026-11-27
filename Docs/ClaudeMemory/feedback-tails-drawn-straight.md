---
name: feedback-tails-drawn-straight
description: "Characters' tails are drawn and modelled straight out behind the body unless the tail's shape is the character's own design; bend, curl and sway come from bones, springs and animation (user rule 2026-11-24)"
metadata:
  type: feedback
---

From 2026-11-24, when a character is drawn (concepts that go to 3D, and the Tripo model): the tail is **stretched out
straight** behind the body, from the middle of the lower back, no bend to a side, no curl, not touching floor, legs or
back. Its bend, curl and sway are made in the game by tail bones + the spring chain + animation, not by the drawing.
User: "이제 캐릭터 그릴때 꼬리는 캐릭터 고유 디자인이 아니면 쭉 펴서 그리고 애니메이션과 스프링으로 유지시킨다. 캐릭터 작업 md 에
내용 추가해주고. 여우만 일단 새로 작업해줘." Exception, in the user's words: a tail that is the character's own design
(my examples, not the user's: the squirrel's big curled tail, nine tails).

**Why:** the fox's tail was drawn swept to one side, so the model came out bent 40 degrees from the root and the spring
swung about that; four repairs the same day (turning bones in code, cutting the tail off as a part, re-laying its
surface, a Tripo-made tail grafted on) all still looked forced to the user, who then asked for the fox to be generated
again ("비용이 크더라도 여우 모델링을 다시 뽑아야 할 것 같다").

**How to apply:** every new character and every remake; the wording is in the animal-friend skill, section 0 (also
`RRRandom/Docs/ClaudeSkills/animal-friend/SKILL.md`). Only the fox is being redone now ("여우만 일단"); the other animal
friends keep their tails until asked. Related: [[tail-chain-spring]], [[feedback-game-characters-t-pose]],
[[character-eight-directions]], [[feedback-keep-old-face-look]].
