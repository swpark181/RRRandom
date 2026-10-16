# char-pipeline 스크립트

에셋 파이프라인 스크립트(Blender 파이썬, 언리얼 파이썬 커맨드릿, Tripo API, Mixamo 내보내기, 캡처 PowerShell)를
다른 PC 에서도 쓰도록 작업 PC 의 `C:\work\char-pipeline\scripts\` 에서 복사해 둔 것입니다.

- HANDOFF.md/README.md 가 `C:\work\char-pipeline\scripts\...` 로 가리키는 스크립트가 여기 같은 이름으로 있습니다.
- 스크립트가 읽고 쓰는 `char-pipeline\input\`(Tripo/Mixamo 원본, 약 640MB)과 `output\`(중간 결과물·캡처)은 저장소에 없습니다.
  필요하면 작업 PC 에서 따로 옮기세요. 경로는 대부분 명령줄 인자로 넘기므로 다른 위치에서도 그대로 쓸 수 있습니다.
- Tripo 스크립트는 환경 변수(또는 레지스트리) `TRIPO_API_KEY` 가 있어야 합니다(키는 저장소에 없음).
- 스크립트를 고치면 이쪽과 작업 PC 원본을 같이 맞춰 주세요.
