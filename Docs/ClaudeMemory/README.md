# Claude 메모

다른 PC(예: 집 PC)의 Claude Code 세션도 같은 메모를 쓰도록, 작업 PC의 Claude 기억 메모(`%USERPROFILE%\.claude\projects\C--work\memory\`)를 그대로 복사해 둔 것입니다.
목차는 `MEMORY.md`, 메모는 한 파일에 하나씩입니다.

- 세션을 시작할 때 "Docs/ClaudeMemory 읽고 HANDOFF.md 이어서 해줘" 처럼 읽게 하면 됩니다.
- 그 PC 세션의 기억으로 넣으려면 이 폴더의 `*.md` 를 그 PC 의 `%USERPROFILE%\.claude\projects\<작업 폴더 이름>\memory\` 에 복사합니다
  (작업 폴더가 `E:\work` 면 `E--work`).
- 메모 안의 경로는 작업 PC 기준(`C:\work\...`)입니다. 집 PC 는 저장소가 `E:\work\RRRandom`, 엔진이 `E:\Games\UE_5.8` 입니다.
- 원본은 작업 PC 쪽이라, 메모가 바뀌면 다시 복사해 커밋합니다.

## 스킬

직접 만든 Claude Code 스킬은 `Docs/ClaudeSkills/<이름>/SKILL.md` 에 복사해 두었습니다.

- `animal-friend`: 새 동물친구(캐릭터) 시안부터 Tripo 3D 모델까지 만드는 절차.
- `ship`: 커밋 → 푸시 → PR → main 머지 절차.
- 다른 PC 에서 쓰려면 폴더째 그 PC 의 `%USERPROFILE%\.claude\skills\` 에 복사합니다(예: `%USERPROFILE%\.claude\skills\animal-friend\SKILL.md`).
- 원본은 작업 PC 의 `%USERPROFILE%\.claude\skills\` 쪽이라, 바뀌면 다시 복사해 커밋합니다.
