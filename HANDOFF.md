# 작업 이어하기 메모 — 팀전 프로토타입 (FPS식 사격 + 엄폐물)

최종 업데이트: 2026-10-07 (6차)

## 요청 내용

- 브롤스타즈를 참고해 기본적인 공방이 되는 상태까지 (스킬은 제외).
- AI도 이동, 공격, 주사위 굴림을 할 수 있게.
- 캐릭터 머리 위에 주사위 쌓인 상태와 버프 효과를 알 수 있는 단순한 인디케이터.
- (2차) 3발 제한을 없애고 일반 FPS처럼 사격: 예비 탄약 무한, 한 탄창 15발. 기본 이동 속도 절반. 공격을 피할 엄폐물. AI는 더 조심스럽게.

- (3차) Niagara 모듈 추가. 랜덤 무기: 데미지가 좋을수록 화려한 이펙트, 탄창 수/총알 속도도 바뀜. 새 무기는 인디케이터에 표시.

- (4차) Space = 점프. 엄폐물 앞에서 점프하면 기어 올라감. 위에서 쏘면 아래층도 맞힘.
- (4차 수정) "위에서 아래 적을 못 맞힘" 버그 수정.
- (5차) 서버 없이 소규모 네트워크 플레이. EOS(Epic Online Services) P2P 기준, 테스트 가능한 상태까지. 게임 전체 동기화.
- (6차) 임시 타이틀 화면. 싱글 / 네트워크를 골라 시작.
- (7차) 랜덤(주사위)에 거인화 추가. 거인이 되면 방어력과 파워가 좋아짐.
- (8차) 비행 추가 (오버워치 파라처럼): Space 누르는 만큼 떠오르고, 떼면 서서히 떨어짐. 거인화처럼 주사위로 일정 확률.
- (9차) Tripo 로 만들고 Mixamo 로 리깅한 여우 캐릭터를 실제 프로젝트에 적용. 이동, 사격, 죽음 애니메이션.
- (10차) Tripo 주사위 모델을 프로젝트에 추가.
- (10차 추가) 그 주사위를 머리 위에 띄우고 굴리는 연출. E 키는 모아둔 주사위를 전부가 아니라 하나씩 굴림.
- (11차) 날아다닐 때 등에 날개 느낌이 나는 FX.
- (12차) 여우가 드는 라이플을 Tripo 로 만들어 손에 들림.
- (13차) Tripo 잔디 타일(캐릭터 크기)로 바닥을 깔되, 이어 붙인 티가 덜 나게 랜덤 배치.
- (14차) 참고 그림(복셀 섬: 절벽, 폭포, 물, 선착장, 언덕, 길) 같은 레벨. 사용자 선택: 가운데는 평평하게 싸우고 절벽/폭포/물은 가장자리 배경, 바위/모래/나무는 임시 블록으로 먼저(Tripo 블록이 오면 교체).
- (15차) 스타크래프트 같은 전장의 안개: 처음엔 다 가려져 있고 캐릭터 주변 원형 시야, 지나간 곳은 안개가 걷힘.

## 15차 — 전장의 안개

- 15차 커밋에 포함(브랜치 `claude/project-thread-ykg56g`).
- 해석: 세 단계 — 안 가 본 곳(거의 검정), 가 봤던 곳(어둡고 회색, 지형만), 지금 시야(선명). 시야는 팀 공유(나 + 아군 봇, 살아 있을 때). 적은 우리 팀 시야 안에서만 보임(몸/총/날개/주사위/머리 위 HUD). **보이는 것만** 바뀌고 총알/봇 판단/게임 판정은 그대로.
- `ARRRandomFogOfWar`(복제 안 함): `ARRRandomPlayerController::BeginPlay` 에서 로컬 컨트롤러만 스폰(`bFogOfWar`, 기본 켬) → 각 기기가 자기 화면용. 첫 틱에 내 폰 아래 바닥(캐릭터/엄폐물/섬은 무시 — 파랑 시작이 모래 판 위라 그대로면 모래 인스턴스 묶음의 바운즈를 바닥으로 잡음)을 찾아 `Resolution` 128 격자로 덮음(지금 맵: 8000 각, 칸 62.5). 매 틱(`TG_PostPhysics`) 아군마다 `VisionRadius` 800 원(바깥 `EdgeSoftness` 200 에서 흐려짐)을 `Visible` 에 찍고 `Explored` = max. R=지금, G=가 봤음 으로 128×128 BGRA 텍스처(sRGB, 쌍선형)에 `UpdateTextureRegions`.
  - 화면: 로컬에서만 스폰한 무한 `APostProcessVolume` 에 `M_PP_Fog` 동적 인스턴스(`FogTex`, `FogOrigin`, `FogSize`).
  - 적 숨기기: 칸의 `Visible` ≥ 0.5 일 때만 보임. 액터 `SetActorHiddenInGame` 은 `bHidden` 이 복제돼 호스트에서 쓰면 참가자 화면까지 숨기므로, **컴포넌트마다 `SetHiddenInGame`**(복제 안 됨). HUD 는 `GetMesh()->bHiddenInGame` 이면 그 캐릭터 위 HUD/데미지 숫자 생략. 액터가 사라질 때(`EndPlay`) 원래대로.
- 머티리얼 `C:\work\char-pipeline\scripts\ue_make_fog.py` → `/Game/PostProcess/M_PP_Fog`: 톤매핑 뒤(`BL_SceneColorAfterTonemapping`, 툰 후처리 다음, HUD 는 그 위). 픽셀 월드 위치 → 격자 UV. 지금 시야 = 원래 색, 가 봤던 곳 = `ExploredDesaturate` 0.6 회색 × `ExploredLevel` 0.45, 안 가 본 곳 = × `UnexploredLevel` 0.06 × `FogColor`(살짝 푸름). 텍스처 파라미터 기본값이 엔진 Black(sRGB)이라 색 샘플러, 게임 텍스처도 sRGB.
- 확인 (창 캡처 `C:\work\char-pipeline\output\screens\fog1`): 빌드 경고 없음, 로그 `fog of war 128 x 128 over 8000 units from (-4000, -4000)`. 나와 아군 주변 원(카메라 기울기로 타원) 선명, 지나온 왼쪽 위는 어두운 회색, 아래 구석은 검정, 경계 부드러움, HUD 그대로.
- 확인 못 한 것: 적이 안개 속에서 실제로 숨는지(그 순간 캡처 없음), 네트워크 참가자 화면, 쓰러진 뒤/부활 때.
- 수정 (사용자: 적 AI 가 시야 밖을 알고 쏘면 안 됨): 봇도 안개를 따름.
  - 시야 반경을 캐릭터로 옮김: `ARRRandomCharacter::VisionRadius` 800, `IsSeenByTeam(World, Team, Location)` = 그 팀의 살아 있는 누군가의 `VisionRadius` 안(평면 거리). 안개 화면과 봇이 이 하나를 씀 — 안개의 적 표시도 격자 대신 이 함수, 흐린 경계는 반경을 가운데로(`Radius ± EdgeSoftness/2`)라 반쯤 보이는 곳 = 판정 경계. 안개 액터의 `VisionRadius` 는 없앰.
  - 봇(`ARRRandomAIController`): `FindBestOpponent` 는 팀 시야 안의 적만, 매 틱 목표가 시야 밖으로 나가면 놓침. 놓친 뒤 `SearchTime` 4초 동안 마지막으로 본 자리(`LastSeenLocation`)로 가고, 그다음은 예전처럼 아레나 가운데로. 사격/재장전/엄폐 판단은 모두 목표가 있을 때만이라 안 보이는 적은 안 쏨(시야에 들어오면 반응 시간 뒤). 안개 속에서 맞으면 피하는 동작만 함.
  - 게임 모드: 쓰러질 때 `X (team N) knocked out by Y; point to team M.` 로그 추가.
  - 확인: 빌드 경고 없음. `-game -RRSolo -nullrhi` 90초: 쓰러짐 13번(파랑 5 : 빨강 8), 거인/비행 굴림도 계속 — 봇이 안개 속에서도 서로 찾아 싸움. 화면으로는 확인 안 함.
- 수정 (사용자: 자기 키보다 큰 장애물 너머는 시야가 밝혀지지 않게):
  - `ARRRandomCharacter::GetEyeLocation` = 캡슐 꼭대기(머리; 거인/엄폐물 위/비행이면 더 높음). `GetSightDistance(World, Eye, Direction, Max)` = 눈높이에서 **수평으로** WorldStatic 오브젝트 라인 트레이스 → 그 높이에 걸리는 것 = 키보다 큰 것만 막음. 여우 머리 192 기준: 엄폐물 160, 언덕 1~2단(80/160), 모래/판자 판은 안 막고, 바위/폭포 3단(240)부터 막음. 물 벽은 WorldDynamic 이라 안 막음.
  - `IsSeenByTeam`: 반경 안 + 그 팀원 눈에서 목표까지 `GetSightDistance` 가 거리 이상(트레이스 1번) → 안개의 적 표시와 봇이 같은 판정.
  - 안개 그림(`ARRRandomFogOfWar::Stamp(Brawler)`): 팀원마다 `SightRays` 120 방향으로 `GetSightDistance`, 칸마다 각도로 두 이웃 광선 거리를 보간해 그보다 한 칸 넘게 멀면 어둡게(경계 한 칸 부드럽게, 장애물 앞면은 보이게). 팀원 3명이면 프레임당 트레이스 360번.
  - 확인 (플레이어를 왼쪽 바위 절벽 3~4단까지 걷게 한 창 캡처 `C:\work\char-pipeline\output\screens\fog_occlusion`): 절벽 너머는 반경 안이어도 검정, 절벽에 안 가린 왼쪽 아래 물가는 보였다가 위로 가면 '가 봤던 곳'으로 어두워짐, 엄폐물 블록 너머는 그대로 보임. 빌드 경고 없음.
- 수정 (사용자: 엄폐물이 자기 키의 **절반**을 넘으면 가리게):
  - `GetEyeLocation` = 캡슐 중심(키의 절반, 여우 96 — 거인/엄폐물 위/비행이면 더 높음). 나머지(수평 트레이스, `IsSeenByTeam`, 안개 그림)는 그대로. 이제 엄폐물(160), 언덕 2단(160) 이상, 바위/폭포가 막고, 언덕 1단(80)·모래/판자 판은 안 막음.
  - 그러자 봇 90초 쓰러짐 0번. 디버그 로그로 확인: 트레이스는 정상이고, 두 팀이 다 아레나 가운데로 모이는데 가운데 엄폐물(`RRRandomCover_0`)이 그 사이를 가려서 서로 못 보고 멈춤.
  - 봇 순찰(`ARRRandomAIController`): 보이는 적도 쫓을 흔적도 없으면 아레나 가운데에서 `PatrolRadiusMin`~`Max`(300~1300) 떨어진 랜덤 지점(`PatrolGoal`)으로 가고, 도착하거나 3~6초 지나면 새 지점. (예전: 가운데에 서 있음)
  - 확인: 빌드 경고 없음. 캡처(`output\screens\fog_half`)에서 엄폐물 블록 뒤로 그늘이 짐. `-game -RRSolo -nullrhi` 90초: 쓰러짐 12번(전 규칙 13번과 비슷).
- 수정 (사용자: 거인 키를 장애물 너머가 보일 만큼): `GiantScale` 1.5 → **2**. 눈(캡슐 중심) 96×2 = 192 > 엄폐물/2단 언덕 160 → 거인은 그 너머까지 봄(3단 이상 바위/폭포는 여전히 가림). 시야 반경(800)은 그대로.
  - 확인: 임시 `-RRGiantTest` 스위치(플레이어를 바로 거인으로, 확인 뒤 지움)로 창 캡처 `output\screens\giant2x`: 거인 주변 엄폐물 뒤로 그늘이 안 지고, 가운데 엄폐물 바로 뒤 빨강 봇 2명이 보임(남은 그늘은 보통 키 아군 시야 쪽). `-nullrhi` 90초: 쓰러짐 7번, 거인 1번 — 끼어서 못 움직이는 건 안 보였지만 2배 몸으로 엄폐물 사이 길을 오래 지켜본 건 아님.

## 15차 덧붙임 — 시작 카운트다운, 팀원 카드, 미니맵 (사용자: 로드 후 5초 카운트 뒤 시작, 오른쪽 아래 팀원 몽타주·체력·스킬 상태, 왼쪽 위 2D 맵 — 클릭하면 전체 화면, 시야 안 적과 아군 위치 실시간)

- 15차 커밋에 포함(브랜치 `claude/project-thread-ykg56g`).
- 카운트다운: `ARRRandomGameMode::StartCountdown` 5 → `StartPlay` 에서 `ARRRandomGameState::SetPlayStartTime(지금 + 5)`(복제, 서버 시계). `GetCountdownRemaining()`/`HasPlayStarted()`, `ARRRandomGameState::IsPlayLocked(this)` = 시작 전이거나 게임 끝. 예전에 `IsMatchOver` 로 막던 곳(사격 `ShootAt`, 주사위 `RollDice`, 피해 `TakeDamage`, 봇 Tick, 플레이어 입력 `IsGameplayInputBlocked`)을 `IsPlayLocked` 로 → 카운트다운 동안 봇도 서 있음. 부활 카운트다운/결과 화면 쪽은 그대로 `IsMatchOver`. HUD 가운데 `GET READY` + 큰 숫자(초마다 1.6배에서 줄어듦), 시작 뒤 1초 `GO!`(커지며 사라짐).
- 팀원 카드(`DrawTeamPanel`/`DrawTeamCard`, 오른쪽 아래 탄약 패널 위 96 비우고 위로 쌓음, 나 빼고 같은 팀): 초상화 54(팀 색 테두리, 쓰러지면 회색 + 빨간 KO), 이름(결과 화면과 같은 규칙: 플레이어 이름 / 이름순 `BOT n`), 체력 막대 150 + 숫자, 칩: `DICE n`(노랑), `GIANT ns`, `FLY ns`, 총 이름(등급 색, 권총이면 없음), `RELOAD`. 초상화: `C:\work\char-pipeline\scripts\blender_portrait.py`(Mixamo `SK_Fox.fbx` 첫 프레임, 목~귀 끝, 18° 비스듬히, 투명 배경, 256) → `output\ui\T_FoxPortrait.png` → `ue_import_ui_texture.py` → `/Game/UI/T_FoxPortrait`(UI 그룹, 밉 없음).
- 미니맵(`RRRandomHUDPanels.cpp`): 섬(`ARRRandomIsland::GetMapBounds`, `GetCellAt`)의 칸마다 색 — 물/모래/선착장/폭포/바닥/언덕(높을수록 밝고 노랗게), 소나무·둥근 나무 칸은 가운데 둥근 진초록(숲), 엄폐물(`GetComponentsBoundingBox`) 진초록, 아래(카메라 쪽)로 낮아지는 곳은 0.62 어둡게(단 면) — 208×208 `MapBase` 를 한 번(엄폐물 수가 바뀌면 다시) 만들고, 0.12초마다 `ARRRandomFogOfWar::GetSightAt`(새로 추가: 지금 보임 0~1 + 가 봤는지)로 어둡게(안 가 봄 0.22, 가 봤음 0.55, 보임 1) 칠해 `MapTexture` 에 올림(`UpdateTextureRegions`). 왼쪽 위 220×220, 그 아래 `[M] / CLICK: MAP`, 온라인 안내는 그 밑으로 내림(`[ESC] MENU [M] MAP`).
  - 표시: 나 = 초록 화살표(바라보는 쪽, 쓰러지면 회색), 아군 = 파랑 점, 쓰러진 아군 = 회색 X, 적 = 빨강 점 — 살아 있고 `IsSeenByTeam`(안개/봇과 같은 판정)일 때만. 검은 테두리. 지도 밖은 가장자리에 붙임.
  - 전체 화면: 미니맵 클릭 또는 M(`RRMap`) → 화면 짧은 변의 86% 정사각형, 뒤는 60% 어둡게, 위에 `MAP [M] OR CLICK TO CLOSE`; 아무 클릭/M/Esc 로 닫힘(Esc 는 지도가 열려 있으면 메뉴 대신 지도 닫기). 지도 위(전체 화면이면 어디든) 클릭은 `OnFire` 에서 사격 안 함. 플레이어 컨트롤러가 자기 안개 액터를 `FogOfWar` 로 들고 있음(`GetFogOfWar`).
- 확인 (창 캡처 `C:\work\char-pipeline\output\screens\hud1`): 카운트다운 5 → 1 동안 6명 모두 제자리, 시작 뒤 싸움. 팀원 카드에 `DICE 0  CANNON`, `GIANT 12s  RELOAD`, 체력 57 반영. 미니맵에 안개 음영, 숲·엄폐물, 나/아군/보이는 적. 클릭 → 전체 화면(그 클릭에 총 안 나감, 15/15), 다시 클릭 → 닫힘. `-nullrhi` 75초: 쓰러짐 9번. 빌드 경고 없음.
- 확인 못 한 것: 네트워크(참가자 화면의 카운트다운 시계, 카드의 상대 정보 복제), 1280×720 외 해상도에서 카드/지도 겹침, 내가 쓰러졌을 때 카드 위치(부활 카운트다운 글자와 겹치는지).

## 15차 덧붙임 — 섬 다시 꾸미기 (사용자: 참고 그림 같은 맵, "지금 배치는 안 이쁘다" → 지형은 기존 잔디 오브젝트 최대한, 절벽은 맵 끝에만 자연스럽게, 나무/오브젝트는 군락으로. 복셀에 색 입히기는 반대)

- 15차 커밋에 포함(브랜치 `claude/project-thread-ykg56g`).
- 흙 블록: `C:\work\char-pipeline\scripts\blender_soil_blocks.py` — 잔디 블록 A/B/C(높이 0~60% 흙, 62~90% 잔디 치마, 90%~ 잔디 윗면)에서 60% 위 면을 지우고 60% 평면으로 잘라(벽이 위로 삐져나오면 위 블록 벽과 겹쳐 깜빡임) 100 높이로 늘림 → `output\grass\SM_SoilBlock_*.fbx`. `ue_import_soil_blocks.py` → `/Game/Environment/Grass/SM_SoilBlock_A/B/C`(충돌 없음, 같은 `M_GrassBlock_*` 머티리얼, UV 그대로).
- `ARRRandomIsland`: 언덕·절벽(`g`, 옛 `r` 도 같게) = 단마다 흙 블록, 맨 위 단에 잔디 블록(잔디 치마가 이음새를 덮음), 각각 세트 중 아무거나 + 90° 회전, 폭 1.08 겹침. 흙 블록이 없으면 예전처럼 잔디 블록 하나를 늘림. 바위 큐브(`M_IslandRock`, `RockShrink/Jitter`)는 코드에서 뺌(에셋은 남음).
- 지도: 바위(`r`)를 모두 잔디(`g`)로. 위쪽: 3행 낮은 단(0~3, 군데군데 바닥까지 패임) → 2행 벽(3행보다 3단 이상: 4~8) → 0~1행 들쭉날쭉 꼭대기(7~9), 폭포 열(F 8/6/4)은 그대로. 왼쪽: 1열 낮은 단(0~3) → 0열 벽(1열 + 3 이상: 3~6). 그래서 1~2단 계단으로 낮은 단까지는 올라가도 그 뒤 벽(240 이상)은 못 넘음 → 섬 밖으로 못 나감. 가운데 아레나, 엄폐물, 시작 줄, 강, 만, 선착장은 그대로. awk 로 26×26, 높이/종류 짝, 소품 칸 조건 검사.
- 소품 군락: `PropRows` 를 같은 글자 덩어리로 다시(P 25칸, T 17, B 23, R 10, L 16), 칸당 개수 `PropCounts` P 2, T 1~2, B 2~3, R 1~2, L 1~2, 칸의 ±40% 안에서 서로 0.35 × 크기 이상 떨어지게(8번 시도), 크기 범위도 넓힘. 위쪽 꼭대기 숲(소나무+둥근 나무 섞음), 왼쪽 벽 숲 세 덩어리(사이에 빈칸), 낮은 단과 언덕의 덤불 무리, 폭포 아래와 선착장 옆 바위, 강과 만의 물가에만 물풀(첫 판은 물풀 38칸 × 2~3개가 만 한가운데까지 채워 어수선 → 물가로 모으고 줄임, 물 한가운데 홀로 뜬 바위도 뺌). 로그 `169 props`.
- 확인: 빌드 경고 없음. 임시 스위치(`-RRPropsTest`: 안개 끔 + 카메라 3배 멀리 + 위로 2000 + 7초에 게임 안 스크린샷 후 종료, 확인 후 지움)로 `Saved\PropsTest\overview_top.png`: 위쪽 잔디/흙 계단 + 꼭대기 숲 띠 + 단마다 덤불, 왼쪽 숲. 창 캡처 `output\screens\terrain1/2`(만·강의 물가 군락). `-nullrhi` 70초 봇전: 쓰러짐 7번, 안개 정상.
- 실수 기록: 테스트 줄 지울 때 `sed '/RRPROPSTEST/d'` 가 같은 줄의 `if (bFogOfWar)` 까지 지워 안개가 무조건 켜지게 됨 → 바로 되돌림. 파워셸 패치에서 `$s` 와 `$S` 가 같은 변수라 지도 배열이 비었던 것도 바로 채움.
- 확인 못 한 것: 평소 카메라로 위쪽 숲이 아레나 위쪽을 가리는 정도(높이 7~9단 꼭대기 + 나무 5m), 폭포 주변 모습, 네트워크.

## 15차 덧붙임 — 배경 소품 5종 (사용자: Tripo 스튜디오를 직접 조작해 나무, 물 등 배경 오브젝트 5종을 만들어 배치)

- 15차 커밋에 포함(브랜치 `claude/project-thread-ykg56g`).
- 생성(Claude in Chrome 으로 studio.tripo3d.ai 를 직접 조작, 사용자 계정 크레딧 24,930 → 24,780, 1회 30 × 5, 텍스트 → HD Model, H3.1): 프롬프트
  - `stylized low poly pine tree, chunky cartoon style, dark green layered needles, short brown trunk, game asset, single object`
  - `stylized low poly round leafy tree, cartoon style, bright green rounded canopy, thick brown trunk, game asset, single object`
  - `stylized low poly green bush with small white flowers, cartoon style, game asset, single object`
  - `stylized low poly grey boulder cluster, three rounded rocks with a little moss, cartoon style, game asset`
  - `stylized pond plants: cattail reeds and floating lily pads with a pink flower, low poly cartoon style, game asset`
  - 내보내기: FBX, Blender 프리셋, 2K 텍스처 → 다운로드 폴더 zip 5개(각 3~4MB, 사용자 허락 후) → `C:\work\char-pipeline\input\props\{pine,round,bush,rocks,pond}`.
- `C:\work\char-pipeline\scripts\blender_props.py <out>|이름=fbx=크기h|w[=strip]`: 메시 합치기, 바닥 가운데 피벗, 높이(h)/가장 긴 폭(w) 맞춤, 6000 삼각형으로 줄임. 소나무 520h(367×368×520), 둥근 나무 460h(413×378), 덤불 140h(185×177), 바위 220w(높이 192), 물풀 240w(높이 169). 물풀은 Tripo 가 깔아 준 회색 판이 물 위에 회색 네모로 보여서 `strip`: 높이 6% 아래이면서 텍스처가 회색(채널 차 < 0.12)인 면 3,413개 지움.
- `ue_import_props.py`: `/Game/Environment/Props/SM_*`(충돌 없음), `Textures/T_*_BaseColor`, `M_*`(텍스처 × `Tint` 0.8, 거칠기 1, 스펙큘러/메탈 0, 인스턴스용).
- `ARRRandomIsland::PropRows`(26×26, `TypeRows` 와 같은 칸): `P` 소나무 15, `T` 둥근 나무 9, `B` 덤불 16, `R` 바위 14, `L` 물풀 24 = 78개. 칸 꼭대기(언덕/바위 = 단 × 80, 모래 16, 선착장, 물 = `WaterLift`, 물속 바위는 40 잠김)에 칸의 ±20% 비껴, 아무 방향, 종류별 0.7~1.2배(시드 777, 모든 기기 같음). 큰 나무는 위쪽 절벽과 왼쪽 바위 벽에만(카메라가 아래에서 위로 보므로 화면 아래쪽 큰 물체는 아레나를 가림), 아래쪽 만과 오른쪽 강엔 바위/물풀만. 충돌 없음 → 이동·총알·시야(안개, 봇) 그대로. 지도는 awk 로 26×26 과 종류 조건(물풀은 물, 나무/덤불은 언덕·바위) 검사함.
- 확인: 빌드 경고 없음, 로그 `island ... 78 props`. 창 캡처 `C:\work\char-pipeline\output\screens\props1`(평소 카메라: 덤불, 만의 바위·물풀 — 첫 판에 회색 판 발견), `props2`(임시 `-RRPropsTest`: 안개 끔 + 카메라 2.6배 멀리, 확인 후 지움: 왼쪽 벽과 위 절벽의 나무들, 회색 판 없어짐).
- 확인 못 한 것: 프레임 비용(인스턴스라 가벼울 것), 위쪽 절벽 나무를 평소 카메라로 볼 때 아레나 위쪽 가림 정도, 네트워크.

## 15차 덧붙임 — 일시정지 메뉴, 승리 치트, MVP 조명 (사용자: ESC 로 중단 후 타이틀, 바로 승리창 치트, 폭죽 빛 속 캐릭터가 잘 안 보임 → 리플렉션)

- 15차 커밋에 포함(브랜치 `claude/project-thread-ykg56g`).
- Esc(`OnEscape`): 결과 화면이면 바로 `RRTitle`, 아니면 `RRPause` — 일시정지 메뉴(`bPauseMenuOpen`). `NM_Standalone` 일 때만 `SetPause(true)`(온라인은 모두가 멈추므로 메뉴만, HUD 에 `ONLINE GAME KEEPS RUNNING`). 메뉴에서 Esc = 계속, Enter(`OnConfirm`) = 타이틀(결과 화면에선 Enter = 새 게임), 마우스로 RESUME / TITLE. 멈춘 동안에도 되게 Fire/Title/Confirm 액션에 `bTriggerWhenPaused`. 메뉴가 열려 있거나 게임이 끝나면 이동/사격/점프/주사위/재장전 입력 무시(`IsGameplayInputBlocked`). 메뉴 중 게임이 끝나면 메뉴 닫음. HUD `DrawPauseMenu`(어둡게 + PAUSED + 버튼, 결과 화면과 같은 `DrawMenuButton`/`ResultsButtons`, `ERRResultsButton::Resume` 추가).
- 승리 치트: F6 / `RRWin` → `ServerCheatWin` → `ARRRandomGameMode::CheatWin(내 팀)`: 그 팀 점수를 `ScoreToWin` 으로(`ARRRandomGameState::SetTeamScore`) 하고 `EndMatch`. F6 은 엔진 디버그 키와 안 겹침.
- MVP 조명(`ARRRandomResultsDirector`): 툰 재질(거칠기 1, 스펙큘러 0)이라 진짜 반사(리플렉션 캡처/스크린 스페이스)는 안 보여서, 빛으로 "폭죽이 비치는" 느낌:
  - Key: 카메라 위 250 에서 MVP 를 향한 스포트라이트(따뜻한 흰색, Unitless 40000, 콘 10/24°, 그림자 없음), 어두워지는 2초에 맞춰 켜짐.
  - Glow: MVP 에서 카메라 쪽 220, 위 120 에 포인트 라이트(반경 700). 폭죽이 터질 때(`ARRRandomFirework::OnBurst` 콜백) 그 색으로 25000 → 0.9초에 걸쳐 제곱으로 꺼짐.
- 카메라 각도 고르기 개선: 첫 캡처에서 MVP 는 보였지만 카메라 옆 엄폐물이 화면 반을 까맣게 가림 → 각 후보 각도를 흔들림 양 끝(±6°, 전엔 ±10°)까지 포함해 MVP 에서 카메라 쪽 다섯 선(화면 가운데와 좌우 끝 근처, `GetViewOpenness`)을 스윕해 열린 정도가 가장 큰 것(로그 `view 99% open`).
- 확인 (창 캡처 `C:\work\char-pipeline\output\screens\pausewin`, `pausewin2`): Esc → PAUSED + RESUME/TITLE, 2초 간격 두 캡처가 바이트까지 같음(멈춤), Esc → 다시 움직임, F6 → `cheat, team 0 wins` → VICTORY 10 : 0, MVP YOU, 결과 화면에서 Esc → 타이틀 맵 로드. MVP 여우가 밝게 보이고 폭죽 색(파랑 등)이 몸에 비침. 빌드 경고 없음.
- 확인 못 한 것: 메뉴 버튼 마우스 클릭(키로만), 메뉴의 Enter → 타이틀, 온라인에서 메뉴.
- 참고: 이번 작업 중 파워셸 패치로 `RRRandomFirework.h` 두 줄의 탭이 `/` 로 바뀐 걸 발견해 고침(빌드가 잡음). 다른 파일엔 같은 손상 없음(`^/[A-Za-z]` 검색).

## 15차 덧붙임 — 승리 춤 (사용자: `House Dancing.fbx` 를 넣고 승리 여우가 승리 포즈로)

- 15차 커밋에 포함. 원본 `C:\work\char-pipeline\input\fox\mixamo_dance\House Dancing.fbx`(Mixamo With Skin, 같은 여우 메시 9,451 정점, 뼈 41개).
- `C:\work\char-pipeline\scripts\ue_import_fox_anim.py <fbx>|<이름>[|inplace]`: 애니메이션만 `SK_Fox_Skeleton` 으로 `/Game/Characters/Fox/Anims/A_Fox_Victory` 에(26.17초, 786키). 엉덩이 시작=끝(0,0)이라 제자리 보정은 0, 춤 중 엉덩이는 메시 기준 x -35~0.6, y -7~21 정도 돌아다님(게임에선 ×1.8). 꼬리 뼈는 트랙이 없어 기본 포즈 + 코드 꼬리 흔들림.
- `ARRRandomCharacter::VictoryAnimation`, `IsCelebrating()` = 게임 끝 + 살아 있음 + (이긴 팀이거나 MVP). `UpdateLocomotionAnimation` 이 그때 다른 애니메이션 대신 이것을 반복. 총(`URRRandomGunComponent`)은 쓰러졌을 때처럼 춤추는 동안 숨김(총 컴포넌트가 매 틱 스스로 보임을 정하므로 그쪽 조건에 추가).
- 확인 (창 캡처 `C:\work\char-pipeline\output\screens\victory1`, `-RRScoreToWin=2`): 빨강 승, MVP 와 옆 빨강 여우가 총 없이 춤(프레임마다 자세 다름), 폭죽/결과 UI 그대로, Enter 로 새 게임(`new match`). 빌드 경고 없음.
- 확인 못 한 것: 춤의 전체 26초(캡처는 앞 6초쯤), 진 팀 MVP 의 춤, 네트워크.

## 15차 덧붙임 — 10킬 승리와 결과 화면 (사용자: 10킬이면 게임 종료 + 결과창, 새 게임/타이틀, MVP 줌 + 포인트 라이트 폭죽)

- 15차 커밋에 포함. 해석: "10킬" = **팀 점수(쓰러뜨린 수) 10 먼저** (3대3 팀전이라). 개인 킬은 MVP 고르기와 결과표에 씀.
- 판정(서버): `ARRRandomGameMode::ScoreToWin` 10 (`-RRScoreToWin=N` 명령줄로 덮어씀) → 게임 상태에 복제. `OnBrawlerKnockedOut` 에서 공격자 `AddKill()`(같은 팀/낙사는 안 셈), 점수가 목표에 닿으면 `EndMatch`: MVP = 킬 × 4 + 이긴 팀 2 + 서 있음 1 이 가장 큰 캐릭터, 쓰러져 있으면 `ReviveNow()`(바로 시작 위치에서 일어남), `ARRRandomGameState::EndMatch(승리 팀, MVP)`(복제: `bMatchOver`, `WinningTeam`, `MVP`).
- 멈춤: `ARRRandomGameState::IsMatchOver(this)` 로 — 캐릭터 `ShootAt`/`RollDice`/`TakeDamage`(날아가던 총알 무효)/부활 카운트다운, 봇 Tick(멈추고 점프 뗌), 플레이어 이동/점프/사격 입력. 게임 끝난 뒤 쓰러짐은 점수 안 됨.
- 새 게임: `ARRRandomGameMode::RestartMatch` = `ServerTravel("?Restart")`(`AGameModeBase` 엔 `RestartGame` 없음). `ARRRandomPlayerController::RRNewGame`(Enter, 결과 화면 NEW GAME 버튼) — 게임 모드가 있는 기기(호스트/혼자)만, 참가자는 메시지. TITLE 버튼/Esc = 기존 `RRTitle`.
- 연출(각 기기 로컬): 플레이어 컨트롤러 `PlayerTick` 이 게임 끝 + MVP 복제를 보면 `ARRRandomResultsDirector` 스폰 → `SetViewTargetWithBlend`(1.6초, EaseInOut).
  - 카메라: MVP 캡슐 중심 높이, 거리 900(거인이면 배율), 피치 -2(위로 하늘 남김), FOV 42. MVP 정면 기준 25°, -25°, 0°, 50°… 순서로 반지름 25 구 스윕해 처음으로 안 가리는 각도(첫 캡처에서 엄폐물 바로 옆이라 얼굴만 크게 나와서 추가). 좌우 ±10° 14초 주기 흔들림, 가리면 그 앞까지 당김.
  - 조명: 움직일 수 있는 디렉셔널/스카이 라이트를 2초에 걸쳐 30% 로(로그 `dims 1 sun and 1 sky lights`), 끝나면(EndPlay) 원래대로.
  - 폭죽 `ARRRandomFirework`: 0.35~0.8초마다(처음 3발 한꺼번에) MVP 뒤 700~1400, 좌우 ±900, 높이 260~480. 0.75초 상승(꼬리 불꽃 0.03초마다, 작은 따뜻한 빛) → 70개 불꽃(30% 고리, 15% 흰색), 속도 320~520, 저항 1.6, 중력 260, 1.3~2초, 끝 40% 깜빡임, 크기 16cm. 포인트 라이트(그림자 없음, Unitless 60000, 반경 1800) 1.3초에 제곱으로 꺼짐. 불꽃 = ISM 인스턴스 + 커스텀 데이터 3(색 × 밝기), 머티리얼 `ue_make_fireworks.py` → `/Game/Effects/Fireworks/M_FireworkSpark`(Unlit, 발광 = 커스텀 데이터 × `Glow` 6 → 툰 후처리 그대로 통과).
  - 안개: 게임 끝이면 숨긴 적을 모두 보이고 볼륨 `BlendWeight` 를 초당 3 씩 0 으로(1 이었을 때 카메라가 다가가는 동안 검은 안개가 비쳐서 올림).
- HUD 결과 화면(`DrawResults`, 게임 끝이면 다른 HUD 대신): 위아래 검은 띠(0.8초), 0.9초 뒤 0.6초에 걸쳐 나타남 — `VICTORY`(금색)/`DEFEAT`(빨강) + `X TEAM WINS a : b`, 양옆 팀 표(킬 순, `YOU`/플레이어 이름/`BOT n`, MVP 금색), `MVP  이름  N KO` 띠, NEW GAME / TITLE 버튼(마우스 올리면 금색, 참가자는 `WAITING FOR HOST` 흐리게), 아래 키 안내. 클릭은 컨트롤러 `OnClick`(사격 키 Started) → `GetResultsButtonAt`. 평소 점수 아래 `FIRST TO 10`.
- 덤으로 고친 버그: 안개가 적을 다시 보일 때 **모든** 프리미티브를 `SetHiddenInGame(false)` 해서 원래 숨겨진 충돌 캡슐까지 보였음(적 주위 빨간 와이어프레임 — 결과 화면 클로즈업에서 발견). 이제 숨길 때 보이던 것만 기록해서 그것만 되돌림(`HideBrawler`/`ShowBrawler`/`ShowAll`).
- 확인:
  - 빌드 경고 없음. `-nullrhi` 봇전: `-RRScoreToWin=3` → 3점에서 끝, 이후 쓰러짐 없음. 기본(10) 150초 → 13번 쓰러짐 뒤 `match over, team 1 wins; MVP ... with 4 knockouts`.
  - 화면: 잠금 화면이라 창 캡처가 안 돼서, 임시 `-RRResultsTest` 스위치(게임 안 스크린샷 + 8초 뒤 새 게임, 확인 후 지움)로 `Saved\ResultsTest\*.png`: 띠, DEFEAT, 팀 표, MVP 띠, 버튼, 어둑한 하늘에 분홍/초록/흰 폭죽, MVP 여우 가운데. 새 게임 뒤 `BLUE 0 : 0 RED  FIRST TO 2` 로 다시 시작(로그 `new match` → `LoadMap`).
- 확인 못 한 것: VICTORY 화면(테스트 판들은 매번 빨강 승), 버튼 마우스 클릭(Enter 경로만), TITLE, 네트워크(참가자 화면, 호스트 새 게임 때 참가자 이동), 거인 MVP, 폭죽 빛이 땅을 비추는 정도(스크린샷이 작아서 판단 어려움).

## 15차 덧붙임 — 여우 꼬리 (사용자: 꼬리가 자연스럽게 움직이게)

- 15차 커밋에 포함(브랜치 `claude/project-thread-ykg56g`). 꼬리 전 `SK_Fox`/`SK_Fox_Skeleton` 은 그 전 커밋(`4116a14`)에 있음.
- 원인: Tripo 자동 리그에 꼬리 뼈가 없고, 꼬리가 대부분 **왼쪽 허벅지(`LeftUpLeg`)** 에 스킨돼 있어서 걸을 때 왼다리를 따라 퍼덕였음(슬라이스 측정: 꼬리 정점 무게 LeftUpLeg ≈ 70%).
- `C:\work\char-pipeline\scripts\blender_fox_tail.py`: `input\fox\mixamo\SK_Fox.fbx` 에 `mixamorig:Tail1~4`(Hips 자식, 메시 공간 (0,-6,18.5)→(0,-32.5,13.6), 꼬리 중심선 따라) 추가, 꼬리 정점(메시 Y < -4.5, Z < 30, |X| < 8 — 490개)을 꼬리 뼈로만 다시 스킨(뿌리 -4.5~-9 에서 Hips→Tail1 섞음, 관절마다 ±3 섞음). 나머지 무게/뼈는 그대로, 애니메이션 없이 FBX → `output\fox_tail\SK_Fox_Tail.fbx`.
- `ue_import_fox_tail.py test|<fbx>`: 임시 폴더에 따로 들여와 기준 포즈 비교 → 위치 차 0(Hips 만 5.4cm), 회전만 큼(팔/손 최대 122°) = 기존 스켈레톤 기준 포즈가 라이플 대기 자세, 새 FBX 는 T 포즈라서(뼈 축 차이면 뼈 길이 방향 이동도 달라짐). 애니메이션은 뼈마다 전체 로컬 변환을 갖고 있어서 영향 없음 — 실제 화면으로 확인.
  - `apply|<fbx>`: `SK_Fox` 를 기존 스켈레톤으로 다시 들여옴 → 스켈레톤에 Tail1~4 만 추가, 기존 41개 뼈 기준 포즈 변화 0. 메시 슬롯(전엔 비어 있었음, 게임은 코드에서 넣음)에 M_Fox, 애니메이션 스켈레톤 그대로 확인.
- `URRRandomBodyAnimInstance`(`UAnimSingleNodeInstance` 상속, 프록시 `Evaluate` 에서 애니메이션 뒤 꼬리 뼈 회전): 캐릭터 메시를 `AnimationBlueprint` 모드 + 이 클래스로, `PlayBodyAnimation` 은 컴포넌트 `PlayAnimation`(단일 노드 모드로 바꿔 인스턴스를 갈아치움) 대신 `GetSingleNodeInstance()->SetAnimationAsset`+`SetPlaying`. 굽힘은 메시 공간 축(위 = 좌우, 옆 = 들기)이라 꼬리 뼈 방향과 무관.
  - 움직임(게임 스레드, `NativeUpdateAnimation`): 메시 요(yaw) 변화 × `TurnDrag` 0.7 만큼 꼬리가 남음, 옆 가속도 × `SideAccelSwing` 0.006 만큼 반대로 끌림, 스프링 `SpringFrequency` 1.4 Hz / `SpringBounce` 0.65(감쇠비 약 0.45), 최대 `MaxSwing` 55°. 흔들기 `WagAmplitude` 9→14°, `WagFrequency` 0.6→1.6 Hz(서 있음→`WagRunSpeed` 600), 뿌리에서 끝으로 `WagDelay` 2 rad 늦게. 걸으면 `RunLift` 14° + 걸음 `RunBob` 4°. 뼈마다 끝으로 갈수록 많이(1:2:3:4). 쓰러지면 1.5초에 걸쳐 멈춤. 순간이동(한 프레임 90° 넘는 회전, 8000 넘는 가속)은 잘라냄.
- 확인: 빌드 경고 없음. 창 캡처 연속(`output\screens\tail1`, `zoom_*.png`): 서 있을 때 프레임마다 꼬리가 좌우로 조금씩, 걸을 때 더 크게 흔들림, 오른쪽으로 꺾을 때 꼬리가 바깥으로 휘었다 따라옴, 왼다리 따라 퍼덕임 없음. `-nullrhi` 75초 봇전: 쓰러짐 5번(래그돌 포함), 애니메이션 오류/ensure 없음.
- 확인 못 한 것: 네트워크 참가자 화면, 쓰러져 래그돌일 때 꼬리 모습(화면), 연습용 더미(`ARRRandomDummy`, 단일 노드 그대로 — 꼬리는 기준 포즈로 고정).

## 15차 덧붙임 — 치트 (사용자: 거인화, 날개, 총별로 바로 얻는 치트)

- 15차 커밋에 포함. `!UE_BUILD_SHIPPING` 에서만 키 매핑/효과.
- `ARRRandomPlayerController`: F7 `RRGiant`, F8 `RRFly`, 1~6 `RRGun <이름|번호>` (콘솔 명령도 같음) → 서버 RPC(`ServerCheatGiant/Fly/Gun`) → 캐릭터 `StartGiant(CheatSeconds)` / `StartFlight(CheatSeconds)` (주사위의 `OnGiantDie`/`OnFlightDie` 도 이제 이걸 부름) / `EquipWeapon(FRRWeapon::Make(Type, CheatGunQuality, GetDefaultMagazineSize()))`. `CheatSeconds` 60, `CheatGunQuality` 6.
- `FRRWeapon::Make(Type, Quality, BaseMagazine)`: 0 = 기본 권총, 1~5 = `WeaponTypes` 순서(MINIGUN, SMG, RIFLE, BLASTER, CANNON), 랜덤 편차 없이 가운데 값. `GetTypeCount`, `GetTypeName`.
- HUD 왼쪽 위에 `CHEATS [F7] GIANT [F8] FLY [1-6] ...` 한 줄.
- 처음엔 F5/F6 이었는데 F5 가 엔진 `DebugExecBindings`(viewmode shadercomplexity)라 화면이 셰이더 복잡도로 바뀜 → F7/F8. (참고: 같은 엔진 바인딩에 F1 wireframe, F2 unlit, F3 lit 도 있음 — 기존 HOST/JOIN/LEAVE 키와 겹침, 이번엔 확인 안 함.)
- 확인 (창 캡처 `output\screens\cheats2`, 키를 차례로 누름): 로그 `turns giant for 60 s`, `can fly for 60 s`, `cheats a MINIGUN/SMG/RIFLE/BLASTER/CANNON/PISTOL`. 화면에 GIANT/FLY 칩과 날개, 오른쪽 아래 `RARE RIFLE DMG x1.4 SHOT SPD x1.2 20/20`, `LEGEND CANNON DMG x2.9 SHOT SPD x0.4 5/5`. 빌드 경고 없음. 네트워크 참가자 쪽은 확인 안 함.

## 14차 — 복셀 섬 레벨 (임시 블록)

- 커밋 `4116a14` 에 포함 (8차 나머지~14차를 한 번에). **작업 중**: 첫 판을 띄워 사용자에게 보여 주는 단계.
- `ARRRandomIsland`(복제, 항상 관련, 모든 기기가 같은 지도로 같은 섬): 26×26 칸(`CellSize` 180, 섬 4,680 각) 글자 지도 두 장 — 종류(`.` 잔디 바닥, `g` 잔디 언덕, `r` 바위, `s` 모래길, `w` 물, `d` 선착장, `F` 폭포)와 높이(숫자 × `LevelHeight` 80). 위 행 = 화면 위(+X), 왼쪽 열 = 화면 왼쪽(-Y), 액터가 가운데. 생성자에 지도가 있음(행 길이와 종류/높이 짝은 awk 로 검사함: 26×26, 어긋남 0).
  - 배치: 위쪽 바위 절벽(6~9단), 오른쪽 위 폭포(8→6→4)가 오른쪽 강으로, 왼쪽 아래 선착장 있는 만, 가장자리 낮은 잔디 언덕(1~3단), 두 팀 시작 줄(행 18 파랑, 행 7 빨강)을 모래길로, 가운데 세로 길. 기존 엄폐물 자리(행 10~15)와 시작 자리는 평지.
  - 충돌: 언덕/바위/폭포 = 숨긴 큐브 인스턴스(BlockAll, WorldStatic — 엄폐물과 같아서 총알/봇 시야/오르기 그대로; 1~2단 = 80~160 이라 올라갈 수 있고 3단부터 벽). 물 = 숨긴 벽(WorldDynamic, 폰만 막음 → 총알·봇 스윕·바닥 트레이스는 통과). 모래길/선착장 = 얇은 판(BlockAll, 한 걸음 올라섬). 섬 밖 바닥은 큰 물 평면 4장.
  - 보이는 것: 언덕 = 잔디 블록(늘림, 1.08 겹침, 랜덤 종류/90° 회전), 바위 = 엔진 큐브를 층마다 쌓되 폭 6%·위치 ±5 흔들고 인스턴스 커스텀 데이터로 색 섞음, 폭포 = 물 머티리얼 기둥, 물 = 평면, 선착장 = 칸마다 판자 3장.
  - 물/모래가 바닥 잔디 타일(+2, 풀잎이 ~+12까지) 위로 안 올라오면 풀잎이 점처럼 뚫고 나옴 → 물 `WaterLift` 16, 모래 윗면 16(두께 10).
- 머티리얼 `C:\work\char-pipeline\scripts\ue_make_island_materials.py` → `/Game/Environment/Island`: `M_IslandRock`(`ColorA`↔`ColorB`, 인스턴스 커스텀 데이터 0), `M_IslandSand`(색 + 월드 얼룩), `M_IslandWood`(X 방향 판자 틈), `M_IslandWater`(`Deep`↔`Light`, 시간 따라 흐르는 월드 노이즈 물결, `Glow` 0.04). 함정: Power 노드 입력 이름은 `Base`(A 아님 — 컴파일 실패하면 기본 머티리얼), 음수 밑 방지로 |sin|.
- 게임 모드: `bSpawnIsland`(기본 켬), 레벨에 없으면 엄폐물/잔디/봇보다 먼저 아레나 가운데에 스폰.
- 확인: 빌드 경고 없음, 로그 `island 26 x 26 cells: 112 solid, 61 sand, 183 water, 11 dock`. 창 캡처(`output\screens\island2`, 파랑 시작 근처): 모래길, 잔디 언덕, 물가, 선착장 판자. 그때 물이 너무 밝고 풀잎 점이 박혀서 물/모래를 올리고 색을 바꿈 — **그 뒤 모습, 위쪽 절벽/폭포는 아직 못 봄**(사용자가 바로 실행을 원해서 띄워 둠).
- 남은 것: 봇이 언덕/물가에 막히는지, 성능, Tripo 바위/모래/판자 블록으로 교체(`MakeInstances` 의 메시/머티리얼만 바꾸면 됨).

## 13차 — 잔디 바닥

- 커밋 `4116a14` 에 포함 (8차 나머지~14차를 한 번에).
- 출처: Tripo 웹에서 사용자가 만든 "grass block" 3종(기본 / 흰 꽃 큰 것 / 흰 꽃 작은 것) → `C:\work\char-pipeline\input\grass\a|b|c`. 흙 큐브 위에 잔디, 정사각형 아님(0.83×1, 0.76×1, 1×1), 가장자리에 풀이 삐져나오고 윗면이 가장자리에서 둥글게 내려감 → 그대로는 틈/겹침.
  - 처음 받은 건 덤불 하나(타일 아님) → 프롬프트를 "square flat grass floor tile ... grass reaches all four straight edges, perfectly flat even top surface ..." 식으로 다시 만들어 줌.
- `scripts\blender_grass_tiles.py` → `output\grass\SM_GrassTile_A|B|C.fbx`, `tiles.json`, `tiles_preview.png`(무작위 3×3 위에서 본 것):
  - 평평한 윗면 높이 = 가운데 위쪽 정점들의 중앙값, 그 높이 근처(폭의 3%)에 있는 정점들이 펼쳐진 범위의 `INSET` 0.92 정사각형을 평면 4개로 자르고 윗면 아래 `DEPTH`(변의 8%)만 남김 → 원점 = 윗면 가운데, 변 180. 바운딩 박스 기준으로 자르면 삐져나온 풀 때문에 중심이 어긋나 한쪽 변이 모자랐음.
  - 타일마다 윗면 평균 색(위를 보는 면의 UV 가운데에서 텍스처 샘플, 넓이 가중, 선형)을 재서 세 타일 평균에 맞추는 틴트를 `tiles.json` 에. 파랑은 거의 0이라 잡음 → 초록 틴트를 따름. 안 맞추면 A 는 노란빛, C 는 짙어서 타일 경계가 색으로 보임.
  - 800 삼각형으로 줄임(바닥이 7,920 각이라 44×44 = 1,936장 — 3,000 이면 580만 삼각형).
- `scripts\ue_import_grass_tiles.py` → `/Game/Environment/Grass`: `SM_GrassTile_*`(충돌 없음), `M_GrassTile_*`(텍스처 × `Tint`, 평평한 툰 규칙, 인스턴스 메시용), `M_GrassUnder`(공통 평균 초록), 텍스처. **`BRIGHTNESS` 0.7** 을 틴트와 밑판 색에 곱함 — 툰 후처리(밝은 면 ×1.4, 채도 1.15)를 거치면 형광 연두가 돼서.
- 코드 `ARRRandomGrassFloor`(복제, 항상 관련, `Seed` 는 처음 한 번만 복제): `BeginPlay`(모든 기기)에서 자기 아래를 Visibility 트레이스(게임 모드가 바닥 찾는 것과 같은 채널 — `ECC_WorldStatic` 오브젝트 트레이스는 이 맵 바닥에 안 걸렸음)로 바닥 컴포넌트를 찾고, 그 바운즈 전체를 `TileSize` 180 칸으로(반올림 개수, 가운데 맞춤) 덮음. 칸마다 가중 랜덤(A 5 : C 3 : B 2), 앞 칸/옆 칸과 같으면 다시 뽑음(최대 4번), 90° 단위 랜덤 회전, 종류별 HISM. 타일 윗면은 바닥 + `Lift` 2, 그 절반 높이에 엔진 Plane + `M_GrassUnder` 를 바닥 크기만큼 → 틈이 있어도 잔디색. 충돌/그림자 없음.
  - 게임 모드: `bSpawnGrassFloor`(기본 켬), `GrassSeed` 7. 레벨에 `ARRRandomGrassFloor` 가 없으면 엄폐물보다 먼저, 플레이어 시작과 빨강 팀 시작 사이 가운데에 스폰.
- 확인: 빌드 경고 없음. 로그 `grass floor 44 x 44 tiles over StaticMeshComponent0 (seed 7)`. 캡처 1장(사용자 파일 탐색기 창이 대부분 가림): 보이는 곳은 이음매 없이 꽃 섞인 잔디가 이어짐, 다만 형광 연두라 밝기 0.7 과 800 삼각형으로 다시 임포트함 — **그 뒤 모습은 확인 못 함**(사용자가 PC 를 쓰는 중이라 창을 앞으로 가져오는 캡처를 멈춤).
- 수정 (사용자: 조금 어둡고 자연스럽게 섞이게): `ue_import_grass_tiles.py` 의 `meadow()` — 모든 잔디 머티리얼(밑판 포함)의 색을 `Desaturate` 0.25 만큼 채도를 빼고, 월드 위치 / `PatchSize` 900 의 노이즈(2 옥타브, 0..1)로 `ShadePatch` (0.72, 0.8, 0.82, 짙고 차가움) ↔ `SunPatch` (1.12, 1.05, 0.8, 밝고 따뜻함)를 곱함 → 타일 경계와 상관없이 몇 미터짜리 얼룩이 바닥 전체에 이어져 격자/반복이 묻힘. **법선을 월드 위쪽으로 고정**(`tangent_space_normal` 끔, Normal = (0, 0, 1)) — 풀잎 요철의 법선을 툰 후처리가 주름으로 읽어 잔디 전체에 검은 잉크 점이 자글자글했음(깊이 선은 풀잎 높이가 작아 안 걸림). 법선이 평평하면 다 밝은 면이 되므로 `BRIGHTNESS` 0.7 → 0.45.
  - 확인 (창 캡처 `output\screens\grass_meadow2`): 차분한 짙은 초록, 잉크 점 없음, 작은 흰 꽃, 은은한 얼룩, 타일 격자 안 보임, 캐릭터/엄폐물 잘 보임. (0.55 + 법선 그대로였던 `grass_meadow` 는 아직 쨍하고 검은 점투성이.)
- 확인 못 한 것: 성능(1,936장 × 800 삼각형) 측정, 네트워크 참가자 화면.

### 13차 추가 — 엄폐물도 잔디 블록으로 (사용자 요청)

- 같은 Tripo 블록을 통째로(흙 옆면 + 잔디 윗면) 씀. `scripts\blender_grass_blocks.py` → `output\grass\SM_GrassBlock_A|B|C.fbx`, `blocks_preview.png`(게임 카메라 각도로 3칸):
  - 100 정육면체로 맞춤, 피벗은 바닥 가운데. 흙 몸통 크기는 **각 방향을 보는 큰 면들의 위치를 넓이 가중 중앙값**으로 잼(흙벽). 가장 바깥 정점으로 재면 옆면을 타고 바닥까지 늘어진 잔디/바닥 판 때문에 몸통이 실제보다 크게 잡혀 블록 사이가 벌어졌음(아래 절반 → 아래 1/4 → 맨 아래 3% 모두 실패).
  - 바깥 ±`KEEP` 0.56(잔디가 위에서 삐져나온 만큼)까지만 남기고, 높이 30%(`PLATE_BELOW`) 아래에선 흙벽(±0.505) 밖을 지움 — 한쪽으로 튀어나온 바닥 판이 엄폐물 끝에 회색 턱으로 보였음.
  - 법선: 면마다 주 축 방향으로 고정(사용자 분할 법선), 위 25%(`TOP_FROM` 0.75)는 전부 위. 안 그러면 세로 풀잎마다 옆 법선이라 툰 잉크가 윗면을 검게 자글자글하게 만듦. 8,000 삼각형(2,500 은 윗면 얇은 풀잎에 구멍).
- `ue_import_grass_tiles.py` 가 블록도 처리: 항목 `SM_GrassBlock_X=<fbx>=@SM_GrassTile_X` → 그 타일의 텍스처/틴트/`meadow()` 를 그대로 쓰는 `M_GrassBlock_X`, 단 법선은 메시 것(위/옆 음영이 갈림). 법선 가져오기 `FBXNIM_IMPORT_NORMALS`.
- `ARRRandomCover`: 엔진 큐브(`Block`)는 충돌용으로 남기고 숨김(`SetVisibility(false)`, 충돌은 그대로 → 이동/사격/엄폐/오르기/봇 판정 변화 없음). 블록 메시마다 `UInstancedStaticMeshComponent`(`BlockLooks`, 충돌 없음). `RebuildLook`(BeginPlay, `OnRep_Size`, `OnConstruction`): 가로/세로를 `BlockCellSize` 130 근처의 정수 칸으로, 높이는 한 칸(쌓으면 중간에 잔디 띠가 생김). 칸마다 블록 종류 랜덤, 정사각형에 가까운 칸은 90° 단위 / 긴 칸은 180° 단위 랜덤 회전(늘어난 방향 유지), `BlockOverlap` 1.08 배로 겹쳐 윗면이 맞닿게. 시드는 엄폐물 위치(반올림 X, Y) → 모든 기기 같은 모양. 에셋이 없으면 예전 큐브가 보임.
- 확인 (창 캡처 `output\screens\grass_cover3`): 빌드 경고 없음. 잔디 윗면(바닥과 같은 초록, 작은 흰 꽃) + 흙 옆면 + 잉크 테두리, 블록 사이 틈/회색 턱 없음. (`grass_cover`: 윗면 검은 점 + 끝 회색 턱, `grass_cover2`: 회색 턱만 남음.)

## 12차 — 라이플

- 커밋 `4116a14` 에 포함 (8차 나머지~14차를 한 번에).
- 출처: Tripo **웹**에서 사용자가 생성(프롬프트: stylized cartoon assault rifle ... dark gunmetal gray body with warm orange accents ...) → `C:\work\char-pipeline\input\rifle` (원본 FBX + PBR 텍스처). 짧은 AK 모양, 회색 + 주황, 18,476 삼각형.
  - API 로 하려고 `scripts\tripo_text_model.py`(텍스트 → 3D, `tripo_pipeline.py` 헬퍼 재사용)를 만들었지만 못 씀: 사용자 키(`tsk_`)는 v2 주소(`api.tripo3d.ai/v2/openapi`)에서만 인증되고 v3(`openapi.tripo3d.com/v3`, 두 스크립트가 쓰는 주소)는 401, 그리고 **API 크레딧 잔액 0**(웹 크레딧과 별개). 쓰려면 크레딧 충전 + 스크립트를 v2 형식(`POST /task {type: text_to_model}`)으로 바꿔야 함.
- `scripts\blender_rifle.py` → `output\rifle\SM_Rifle.fbx`: 원점을 권총 손잡이 가운데(`GRIP` = 길이 대비 (-0.19, 0.255), 옆모습 렌더에 빨간 점으로 확인)로, 총구 +X, 위 +Z, 길이 100. `scripts\ue_import_rifle.py` → `/Game/Props/Rifle/SM_Rifle`, `M_Rifle`(베이스 컬러만, 거칠기 1, 스페큘러/금속 0 — `M_Fox`/`M_Dice` 와 같은 평평한 툰 규칙), `Textures/T_Rifle_BaseColor`.
- 코드 `URRRandomGunComponent`(UStaticMeshComponent, 캐릭터의 `Gun`, `TG_PostUpdateWork` 에 틱 = 애니메이션 뒤): 매 프레임 손잡이를 오른손바닥(`righthand` 과 `righthandindex1` 의 중간)에, 총구를 왼손(`lefthand`) 쪽으로, 위는 하늘 쪽으로. 그래서 사격/걷기/대기 어떤 자세든 두 손 사이를 따라감(대기는 몸 앞에 비스듬히). 손 뼈가 없으면(마네킹) 숨김, KO 중 숨김. 절대 위치/회전/스케일, 거인이면 같이 커짐.
  - `HoldOffset` (30, 0, 10) — 캐릭터 기준 앞/오른쪽/위로 밀어냄. 손 그대로면 통통한 몸 안에 묻히고 큰 머리에 가려 위 카메라에선 총 끝만 삐져나옴(손이 몸에 붙어 있음). `GunLength` 120.
  - 처음엔 애니메이션 포즈를 오프라인으로(파이썬, 뼈 체인 합성) 계산해 오른손 소켓에 고정하려 했으나 방향이 틀어져 총이 서 버림 → 버리고 매 프레임 손에서 계산하는 방식으로.
- 확인 (`-RRSolo` 창 캡처 `C:\work\char-pipeline\output\screens\rifle`): 빌드 경고 없음, 스마트 앱 컨트롤 차단 없음. 사격 중인 봇의 총이 총알 방향을 향하고 앞 손잡이(주황)까지 보임, 총구 쪽에서 총알이 나감. 걷는 내 캐릭터(뒷모습)는 오른쪽 옆구리에 총.
- 수정 (사용자: 총구 방향이 사격 방향과 너무 다름): 손을 따라가면 애니메이션 속 두 손 방향이라 캐릭터가 바라보는(=총알이 나가는) 방향과 어긋남. 쏘는 동안(`ARRRandomCharacter::IsAiming` = 마지막 발사 후 `FireAnimationHold` 0.35초, 모든 기기에서 `ShotCount` 복제로 앎)은 총을 캐릭터 정면으로 돌리고 총구 끝(`MuzzleLength` 0.69 × 길이)을 총알이 생기는 자리(`GetMuzzleOffset`, 캡슐 중심 + (70, 0, 20) 회전만, 스케일 없음)에 맞춤. 손 자세 ↔ 조준 자세는 `AimBlendSpeed` 10 으로 부드럽게(0.1초). 걸으면서 쏴도 정면. 확인 (캡처 `output\screens\rifle_aim`): 미니건 봇이 연사할 때 총구가 가리키는 선 위로 총알이 일직선. 위아래 기울여 쏘는 경우(다른 층)는 총이 수평 그대로.
- 수정 (사용자: 라이플 사격에 어울리게 파지 위치도): 위 수정은 쏠 때 총을 손에서 떼어 몸 중앙을 지나는 총알 선에 놓아서 손이 총에서 떨어짐. 원인은 Mixamo 사격 애니메이션이 **몸을 비스듬히 튼 소총 자세**라 두 손의 총이 몸 정면보다 약 38° 왼쪽을 향하는 것(뼈 체인 계산의 손 방향 (0.62, 0.79) — 메시 +Y 가 정면, 메시 +X 는 캐릭터 왼쪽).
  - 캐릭터: 사격 애니메이션 중엔 메시를 `FireStanceYaw` 38° 오른쪽으로 돌림(`FireStanceAlpha`, `FireStanceSpeed` 8 로 부드럽게, `UpdateLocomotionAnimation`, 모든 기기). 그러면 손에 든 총이 그대로 사격 방향 — 게임 중 측정: 방향 (0.999, -0.001, 0.04), 오른손 손잡이 / 왼손 앞 손잡이 그대로. KO(`ApplyKnockedOutBody`)에선 바로 원래 각도.
  - 총: 자세 중엔 `HoldOffset` 을 빼서(×(1-Alpha)) 손에 딱 쥠. 걸으며 쏠 땐(걷기 애니메이션, 자세 없음) 이전처럼 정면 조준 덮어쓰기. `MuzzleTip` (69, 0, 14) — 손잡이에서 총구 끝 가운데까지(100 길이 메시 기준, 옆모습 렌더에서 잼).
  - **총알이 생기는 자리 `MuzzleOffset` (70, 0, 20) → (70, 12, 2)**: 자세에서 잰 총구 끝(70.5, 12.4, 2.3). 봇 시야 스윕(`SweepHitsCover`)도 `ShotHeight` 20 → 2, `ShotSide` 12(쏘는 쪽 오른쪽으로 평행 이동). 엄폐물이 160 높이라 엄폐 판정은 그대로. 평평하게 쏘면 총알은 정면과 평행하게 오른쪽 12 에서 날아감(캡슐 반지름 42 라 맞히는 데 지장 없음).
  - 확인 (마우스 왼쪽 버튼을 눌러 플레이어가 직접 연사하는 캡처 `output\screens\rifle_player`): 오른쪽으로 쏘면 비스듬히 선 자세에서 두 손의 총이 오른쪽을 향하고 노란 총알이 총구 앞으로. 왼쪽 위(카메라 반대쪽)로 쏘면 총알은 맞게 가지만 총이 큰 머리 뒤에 가려 거의 안 보임 — 탑다운 시점 한계.
- 남은 것: 모든 무기(SMG/MINIGUN/CANNON...)가 같은 라이플 모양. 쏘지 않을 땐 손이 총을 정확히 감싸진 않음(밀어낸 만큼 떨어짐). 쓰러질 땐 총이 사라짐(떨어뜨리는 연출 없음). 다른 높이로 기울여 쏠 때 총은 수평.

## 11차 — 비행 날개

- 커밋 `4116a14` 에 포함 (8차 나머지~14차를 한 번에).
- 방식: 반투명 파티클은 툰 후처리에서 바닥 베이스 컬러로 다시 칠해질 위험이 있어서(9차 "남은 것" 참고), **언릿·불투명·양면 메시 날개**로 함. 언릿은 베이스 컬러가 0이라 툰 후처리가 그대로 통과시킴(외곽선 오버레이와 같은 원리) → 평평하게 빛나는 만화풍 모양.
- 메시 `/Game/Effects/Wings/SM_Wing` (`C:\work\char-pipeline\scripts\blender_wing.py` → `output\wings\SM_Wing.fbx` → `ue_import_wing.py`, 로그 `output\ue_wing_rr.log`): 오른쪽 날개 하나, 뿌리 원점, +Y 로 142, 깃털은 -X. 끝이 뾰족한 타원 깃털 7장을 팔(뿌리→손목)을 따라 부채꼴로 겹치고(안쪽은 뒤로, 바깥은 바깥-뒤로, 바깥일수록 김) 그 위에 덮깃 띠(물결 모양 뒤 가장자리). 조각마다 inset 테두리: 정점 색 R=1 테두리, G=1 덮깃. UV U=뿌리→끝, V=앞→뒤. 623 삼각형.
  - Blender 함정: `bm.faces.new` 직후엔 면 법선이 0이라 inset 폭이 0이 됨 → `face.normal_update()` 먼저. bmesh 연산이 요소 tag 를 지우므로 조각 구분은 면 집합 차이로. 색을 안 준 코너는 흰색(=테두리+덮깃)이 됨.
- 머티리얼 `M_Wing`: 언릿, 양면. 발광 = lerp(lerp(lerp(`RootColor`, `TipColor`, U), `CovertColor`, G), `RimColor`, R) × `Glow` × (1 + `ShimmerAmount`·sin(U·`ShimmerFrequency` − 시간·`ShimmerSpeed`)) — 끝 쪽으로 흐르는 반짝임. 처음 값(옅은 분홍, Glow 1.3)은 톤매핑 후 거의 흰색이라 진하게: Root (1, 0.45, 0.85), Tip (0.75, 0.08, 0.55), Covert (1, 0.72, 0.92), Rim 흰색, Glow 1.0.
- 코드 `URRRandomWingsComponent`(USceneComponent, 캐릭터의 `Wings`): `OnRegister` 에서 게임 월드일 때만 날개 메시 컴포넌트 2개를 만듦(왼쪽은 Y 스케일 -1, 양면 머티리얼이라 뒤집혀도 보임). 매 틱 여우 `spine2` 뼈 + 캐릭터 기준 (-16, 0, 4) 에 놓고 캐릭터 회전을 따름(뼈가 없으면 붙인 자리 (-20, 0, 10)). 각 날개 회전 = 앞축 기준 들어 올림(Lift + 날갯짓) × 위축 기준 뒤로 젖힘(Sweep), 왼쪽은 각도 부호 반대.
  - 자세(Lift, Sweep, 날갯짓 각도, 초당 날갯짓, 크기): 땅(비행 능력만) 50/55/3/0.7/0.65 접힘, 상승(공중 + 위 속도 30 초과) 15/10/38/4/1, 활공 10/15/6/1.2/1, 대쉬 5/60/4/7/0.9. 자세 사이 `FInterpTo` 8, 처음 보일 땐 바로 그 자세.
  - 보임: 살아 있고 (비행 능력 || 활공 중 공중). `ShowTime` 0.25초에 펼쳐지고(크기 1-(1-t)²) 접히며 사라짐, KO 는 즉시. `WingScale` 0.85, `WingRootGap` 16. 대쉬(`IsDashing`)는 복제되지 않아서 다른 기기에선 활공 자세로 보임.
- 확인 (임시 `-RRFlyTest`: 모든 주사위 = 비행, 확인 후 지움. 1280×720 창 캡처 `C:\work\char-pipeline\output\screens\wings`): 빌드 경고 없음. 땅에선 등 뒤에 작게 접힌 날개, Space 상승/활공 중 양쪽으로 펼친 분홍 깃털 + 흰 테두리 + 밝은 덮깃, 봇마다 날갯짓 위상이 달라 한쪽이 들린 모습, 카메라 쪽을 보는 봇은 등 뒤로 날개가 보임.
- 확인 못 한 것: 대쉬 자세, 거인일 때, 네트워크 참가자 화면. 날개 가장자리 옆 바닥에 잉크 점이 조금 찍힘(바닥 쪽 픽셀의 깊이 선) — 거슬리면 툰 후처리에서 언릿 옆 바닥 잉크도 빼는 걸 검토.

### 11차 추가 — 더 반짝이게 (사용자 요청)

- `M_Wing` (`ue_import_wing.py` 재작성, 세 번째 인자로 별 FBX): 테두리 HDR(`RimGlow` 1.6, 처음 2.5는 접힌 날개가 하얗게 떠서 낮춤). 물결 반짝임은 `Sine` 노드 주기가 1이라 예전 값(12, 6)이 초당 6번 번쩍였음 → `ShimmerFrequency` 3, `ShimmerSpeed` 1.5, `ShimmerAmount` 0.2. **별빛 반짝임**: UV 를 `SparkleDensityU`×`SparkleDensityV`(14×5) 칸으로 나눠 칸마다 frac(sin(dot)) 해시로 위상을 주고, sin(시간·`SparkleSpeed` + 위상)을 `SparkleSharpness`(24) 제곱해 가끔만 번쩍, 칸 가운데 둥근 점(`SparkleSize`) × `SparkleColor` × `SparkleBrightness`(8).
- 별 가루: `blender_wing.py` 두 번째 인자로 `SM_WingSparkle`(네 갈래 별, 반지름 5, +X 를 봄, 정점 색 R=중심) + `M_WingSparkle`(언릿, 인스턴스 메시용, `SparkleColor` (1, 0.3, 0.8) 중심 흰색, `Brightness` 2.5 — 6 은 화면에서 흰색으로 날아감).
- `URRRandomWingsComponent`: 월드 원점에 둔 `UInstancedStaticMeshComponent` 하나에 별들을 매 틱 다시 넣음(최대 `MaxSparkles` 80). 날개 바깥쪽 70%·앞전에서 40 안의 랜덤 점에서 생겨, 랜덤 밀림(45) + 천천히 가라앉음 + 공기 저항, `SparkleLife` 0.4~0.9초, 크기 `SparkleScale` 1.3~2.6 × sqrt(sin(πt)) 로 톡 생겼다 줄어들고, 크기 깜빡임 + 천천히 돌며 카메라를 봄. 초당 개수는 자세에 포함: 땅 5, 활공 20, 상승 45, 대쉬 55 (펼침 정도만큼). 날개가 접혀 사라져도 남은 별은 끝까지.
- 확인 (첫 버전, 창 캡처 `output\screens\wings_sparkle` 은 차단 때문에 빈 화면만 남음): 날 고 있는 봇 주위에 네 갈래 별이 흩날림, 접힌 날개 위 반짝임. 그때 별이 하얗게 날아가고 작아서 색/크기/테두리를 조정했는데, **조정 뒤 모습은 확인 못 함** — 아래 차단 때문.
- **Windows 스마트 앱 컨트롤**(`HKLM\SYSTEM\CurrentControlSet\Control\CI\Policy` `VerifiedAndReputablePolicyState` = 1, 켜짐)이 새로 빌드한 서명 없는 `UnrealEditor-RRRandom.dll` 을 간헐적으로 막음: 게임 창에 "잘못된 이미지 0xc0e90002", 이벤트 로그 `Microsoft-Windows-CodeIntegrity/Operational` ID 3077. 2026-10-09 11:11 한 번, 16:51 한 번(재실행하니 통과), 16:53~16:55 네 번 연속(다시 빌드해도). 코드 문제 아님.

## 10차 추가 — 머리 위 3D 주사위 + 하나씩 굴리기

- 커밋 `4116a14` 에 포함 (8차 나머지~14차를 한 번에).
- `URRRandomDiceBuffComponent::RollDice` 는 이제 주사위 하나만 씀(bool 반환). 굴리는 순간 눈/종류를 정해 복제하고(`LastRoll` 은 배열 → 구조체 하나), `RollSpinTime`(0.6초) 뒤 착지할 때(`TickComponent` → `ApplyRoll`) 효과 적용. 구르는 동안(`IsRolling`)은 다음 굴림 거부. `ClearBuffs`(KO)는 구르던 주사위도 취소. 여러 개일 때 "가장 높은 눈 한 번만" 규칙은 의미가 없어져 없앰.
- `OnRep_RollCount` 는 `HasActorBegunPlay` 전(중간 참가 시 첫 복제)에는 무시 → 예전 굴림이 다시 튀어나오지 않음.
- 새 `URRRandomOverheadDieComponent`(UStaticMeshComponent, 캐릭터의 `OverheadDie`): 모든 기기에서 각자 연출. 팝(0.15초, 오버슈트) → 화면 위쪽 호(`HopHeight` 45)를 그리며 `SpinTurns` 2.5바퀴, (1-t)³ 로 감속해 결과 면에서 정확히 멈춤(최종 회전 = 결과 면이 카메라를 보는 회전에 남은 각도만큼 랜덤 축 회전을 곱함) → 착지 바운스(1.18배, 0.18초) → `HoldTime` 1.3초 → `ExitTime` 0.3초 축소. 절대 위치/회전/스케일(거인이어도 크기 그대로), 충돌/그림자 없음, `DieScale` 0.55 (약 55).
- 면 방향 표(`FaceAxes`): 임포트된 메시의 Pip 삼각형을 면별로 모아 눈 개수를 셈 → UE 메시 공간에서 1 +Z, 6 -Z, 3 +X, 4 -X, 2 +Y, 5 -Y (Blender 의 Y 가 뒤집힘). 6 은 두 줄이 화면 세로가 되게 위쪽 축을 Y 로.
- 위치: HUD 가 머리 위 표시를 다 그린 맨 위(`DrawRollPopup` 이 결과 줄 자리까지 포함해 반환)를 카메라 위쪽 방향으로 월드에 되돌려 `SetStackTop` 으로 넘김(거의 정사영이라 한 배율로 충분). 주사위는 그 위 `StackGap` + 반 크기에 뜨고, 칩이 생기고 없어질 때는 부드럽게 따라감. HUD 가 없으면 캡슐 위 `DefaultLift`.
- HUD 결과 줄(`5 ATK`)은 착지한 뒤부터 `HoldTime + ExitTime` 동안, 마지막 `ExitTime` 에 흐려짐(주사위와 같이 사라짐). 굴리는 동안엔 빈 줄로 자리만 잡음. 위로 떠오르던 움직임은 없앰.
- 봇: 굴리기 시작하면(`bSpendingDice`) 다 쓸 때까지 하나씩, 매번 착지 + 반응 시간(0.2~0.8초) 뒤 다음.
- 수치 조정 (사용자 요청): 주사위 3개로 시작(`StartingCharges`, 서버 `BeginPlay`), 쌓이는 간격 5 → 15초(`GaugeFillTime`, 사용자 표현 "쿨타임"을 게이지로 해석), 능력치 버프 지속 15초 고정 → 10~30초 랜덤(`BuffDurationRange`, 주사위마다 따로). 거인/비행 지속 시간(7~12초)은 그대로. 확인 (창 캡처): 약 20초에 두 번 굴린 뒤 `DICE x2`, 버프 `27s`/`24s`.
- 확인 (`-RRSolo` 1280×720 창 캡처 연속, `C:\work\char-pipeline\output\screens\dice_roll`): 빌드 경고 없음. 봇 주사위가 머리 위 표시 바로 위에서 구르다 멈추고 결과 면이 라벨과 같음(`2 MOVE`/`2 FLY`/`6 MOVE`). 내 E 굴림: 공중일 때 비행 없음 → 5 면으로 착지 후 `5 FLY` 와 `FLY ... 11s`(6+5). 주사위 약 40픽셀, 눈 읽힘.
- 확인 못 한 것: 네트워크 참가자 화면(복제 경로), 거인일 때 위치, 여러 캐릭터가 붙어 있을 때(머리 위 표시끼리 원래 겹침 — 주사위도 겹칠 수 있음).
- 캡처: `C:\work\char-pipeline\scripts\capture_dice_roll.ps1 -OutDir <폴더>` — `-RRSolo` 로 띄우고 창 제목 `RRRandom*` 으로 창을 찾아 E 를 보내고 연속 캡처 (DPI 인식 필요, `-log` 콘솔 창은 띄우지 않기 — 핸들이 콘솔로 바뀜). 파일 이름의 ms 는 대략적임(캡처 저장이 느려 실제로는 더 늦음). 플레이어가 이미 쓰러져 있으면 E 가 무시됨.

## 10차 — 주사위 3D 모델 (에셋)

- 커밋 `4116a14` 에 포함 (8차 나머지~14차를 한 번에). 머리 위 연출에서 씀(위 "10차 추가"). 에셋: `/Game/Props/Dice` — `SM_Dice`, `M_Dice`, `M_DicePip`, `Textures/T_Dice_BaseColor`.
- 출처: Tripo 가 숫자를 제대로 못 그려서(깨지거나 1~6 이 아닌 숫자) 무늬 없는 컬러 큐브만 Tripo 로 만들고, 눈(점)은 Blender 에서 구 21개를 Boolean 으로 파냄. 마주보는 면 합 7 (+Z 1 / -Z 6, -Y 2 / +Y 5, +X 3 / -X 4, Blender 축 기준).
  - `C:\work\char-pipeline\scripts\blender_dice_pips.py` (원본 `input\dice`, 결과 `output\dice\SM_Dice.fbx`) → `ue_import_dice.py` (예전 FBX 임포터, 로그 `output\ue_dice_rr.log`).
  - 스크립트가 확인하는 것: 눈이 둥근 모서리가 아니라 평평한 면에 놓였는지(레이캐스트), 파인 자리가 정확히 21개인지, 메시가 닫혀 있는지.
- 크기: 약 91 × 96 × 100 (Tripo 상자가 정육면체가 아님, 비율 유지), 피벗은 가운데 → 머리 위에서 돌리기 좋음. 머리 위에 쓸 때는 컴포넌트 스케일로 줄임 (예: 0.3 → 30).
- 콜리전 없음 (머리 위 장식용). 35,205 삼각형, LOD 1개 — 여러 개를 띄우면 LOD 추가나 단순화를 고려.
- 머티리얼은 `ue_make_toon.py` 가 `M_Fox` 에 한 것처럼 평평하게: 노멀맵/금속 없음, 거칠기 1, 스페큘러 0. `M_Dice` = Tripo 베이스 컬러, `M_DicePip` = `PipColor` 파라미터 (진한 남색 0.04/0.045/0.07). 툰 후처리는 베이스 컬러 휘도 0.01 아래를 언릿으로 보고 그대로 통과시키므로 눈 색은 그보다 밝게 유지.
- 확인: 새 세션에서 다시 열어 메시/슬롯(Dice → M_Dice, Pip → M_DicePip)/텍스처(2048, sRGB)/머티리얼 연결 확인. Blender 렌더로 면마다 눈 개수 확인. 확인 못 한 것: 게임 화면(툰 후처리 적용)에서의 실제 모습.
- 면 방향이 메시에 달려 있으므로 `SM_Dice` 를 다시 만들면 `URRRandomOverheadDieComponent` 의 `FaceAxes` 를 다시 확인.

## 9차 — 여우 캐릭터 (이동 / 사격 / 쓰러짐 애니메이션)

- 커밋 `4116a14` 에 포함 (8차 나머지~14차를 한 번에). 처음으로 프로젝트 에셋이 생김: `Content/Characters/Fox` (커밋하면 `.gitattributes` 대로 LFS).
- 출처: Tripo 모델(18,890 삼각형, 2K PBR) → Mixamo 자동 리깅 + 애니메이션 5개(소총 대기/걷기/사격/연속 사격/뒤로 쓰러짐, 왼쪽 돌기). 에셋을 만드는 스크립트와 원본 FBX는 `C:\work\char-pipeline` (`scripts\ue_import_mixamo.py`, `scripts\ue_setup_fox.py`).
- 에셋: `SK_Fox`(+`SK_Fox_Skeleton`), `M_Fox`, `Textures/T_Fox_*`, `Anims/A_Fox_Idle|Walk|Fire|FireBurst|Death|TurnLeft`. `FireBurst`, `TurnLeft` 는 아직 안 씀.
- 임포트 메모: UE 5.8 Interchange FBX 는 `AssetImportTask` 옵션을 무시해서(애니메이션마다 스켈레톤/메시가 따로 생김) `-dpcvars=Interchange.FeatureFlags.Import.FBX=0` 로 예전 FBX 임포터를 씀. 스켈레톤 이름을 바꾸면 리다이렉터가 저장되지 않아 메시가 옛 스켈레톤을 찾음("has no skeleton") → 메시 FBX 이름을 처음부터 `SK_Fox` 로 해서 스켈레톤은 바꾸지 않음.
- 걷기/연속 사격은 Mixamo 에서 "In Place" 가 아니라 엉덩이가 앞으로 감(걷기 1.43초에 113). 루트 고정은 엉덩이를 T포즈 높이(27.5, 걷기는 22)로 고정해 발이 뜨므로, 엉덩이 X/Y 의 직선 이동만 빼서 제자리로 만듦 (위아래 흔들림 유지).
- 캐릭터: 여우 에셋이 있으면 여우, 없으면 예전 마네킹. 여우는 키 100(귀 끝까지) → 메시 스케일 1.8 (캡슐 192). 걷기 재생 속도 기준 `WalkAnimationSpeed` = 79 × 1.8. Mixamo 여우도 +Y 를 보고 있어서 기존 -90도 회전 그대로.
- 애니메이션 선택(`UpdateLocomotionAnimation`): 움직이면 걷기 → 마지막 발사 후 `FireAnimationHold`(0.35초) 안이면 사격 → 대기. 다른 기기는 복제되는 `ShotCount`(uint8) 의 `OnRep` 으로 발사를 앎, 조종 클라이언트는 `FireAt` 에서 바로. `PlayBodyAnimation` 이 재생 속도를 1로 되돌림(전엔 걷기 속도 배율이 대기에 남았음).
- 쓰러짐(`ApplyKnockedOutBody`): `DeathAnimation` 이 있으면 마지막 총알 반대쪽을 보게 돌린 뒤(뒤로 쓰러지는 동작이라 총알 방향으로 넘어짐) 한 번 재생하고 마지막 자세 유지. 래그돌은 없을 때만. 부활은 기존 `ApplyStandingBody` 그대로.
- 팀 색: `M_Fox` 가 텍스처에 `DiffuseColor` 를 `TintStrength` 0.35 만큼 섞음 → 팀 구분이 약함(주황 계열 그대로). 필요하면 `M_Fox` 의 `TintStrength` 를 올리거나 팀 색 테두리 등 추가.
- 확인 (2026-10-08, `-RRSolo` 창 캡처): 빌드 경고 없음, 로그에 여우/스켈레톤 경고 없음. 여우 7마리가 텍스처를 입고 섬, 내 캐릭터는 뒤(꼬리), 적은 얼굴이 보임(방향 맞음). 기둥 옆 봇이 소총 겨누는 자세, 쓰러진 봇이 등을 대고 누운 마지막 자세(점수 1:0 직후).
- 확인 못 한 것: 네트워크 참가자 화면의 사격/쓰러짐 애니메이션, 거인/비행 중 모습, 걷기 발 미끄러짐 정도(재생 속도 2배 근처).
- 남은 것: ~~총 모델이 없어서 빈손으로 겨눔~~ (12차에서 라이플), ~~총알이 머리 높이(캡슐 중심 +20)에서 나감~~ (12차에서 총구 끝 (70, 12, 2)), 오르기 애니메이션.

## 9차 추가 — 2.5D 망원 카메라

- 사용자 요청: 2.5D 느낌. 카메라 시야각 90 → `CameraFieldOfView` 20, 붐 길이는 예전 화면 폭(90도, 1400)과 같아지게 계산 (약 7940). 피치 -60 그대로.
- 결과 (창 캡처 비교, `C:\work\char-pipeline\output\screens`): 원근이 거의 없어져 엄폐물 벽이 평행, 화면 위아래 캐릭터 크기 같음. 그림자/HUD/조준 그대로.
- 대신 위아래로 약 900 까지만 보임 (예전엔 원근 때문에 위쪽이 더 멀리 보였음). 사거리 1300 끝의 적은 화면 밖 — 사용자가 "지금 그대로" 선택. 넓히려면 `FramingArmLength` 를 올림 (1300 다 보이려면 약 2000).

## 9차 추가 — 팀 색 외곽선

- `/Game/Characters/M_CharacterOutline` (`C:\work\char-pipeline\scripts\ue_make_outline.py` 로 만듦): inverted hull. Unlit, Masked, Two Sided, 법선 방향으로 `OutlineWidth`(4) 만큼 밀고 `TwoSidedSign` 으로 앞면을 잘라 뒷면만 남김 → 실루엣 둘레만 보임. 색 `OutlineColor`.
- 캐릭터: `OutlineMaterial` → `BeginPlay` 에서 동적 인스턴스를 메시의 오버레이 머티리얼로(`SetOverlayMaterial`). `ApplyTeamColor` 가 `OutlineColor` = `GetTeamColor(Team)`. 오버레이는 머티리얼 슬롯이 아니라 랜덤 색 이벤트가 건드리지 않음.
- 확인 (창 캡처): 파랑 팀은 파란 테두리가 뚜렷함(약 2픽셀). 빨강 팀 테두리는 주황 털과 색이 비슷해서 잘 안 보임 → 빨강 외곽선 색을 따로 정하거나 두께를 올리는 게 다음 조정 거리.

## 9차 추가 — 카툰 렌더링 (셀 셰이딩 + 잉크 선)

- 사용자 의견: 외곽선만으로는 "전혀 카툰 렌더링처럼 보이지 않음".
- `/Game/PostProcess/M_PP_Toon` (`C:\work\char-pipeline\scripts\ue_make_toon.py`, 머티리얼 노드로만 구성, HLSL 없음). 위치 Scene Color Before Bloom. 탑다운 카메라 `PostProcessSettings.WeightedBlendables` 에 코드로 붙임 + 앰비언트 오클루전 0.
  - 셀: 빛 = 장면 색 / 베이스 컬러 (휘도). `LitThreshold`(0.5) 위는 베이스 컬러 × `LitLevel`(1.4), 아래는 × `ShadowTint`(0.85, 0.88, 1.15 — 푸른 그림자), `BandSoftness` 0.08. 베이스 컬러가 거의 없거나(언릿: 외곽선 오버레이) 빛이 `GlowThreshold`(4) 넘으면(발광/이펙트) 그대로 통과.
  - 잉크 선: `LineWidth`(1) 픽셀 간격 4방향 샘플의 깊이 라플라시안 ÷ 중심 깊이 > `DepthLineThreshold`(0.004), 또는 법선 라플라시안 길이 > `NormalLineThreshold`. 라플라시안이라 평평한 바닥은 거리와 상관없이 선이 안 생김. 법선 0.8 은 여우 몸 안쪽 선이 너무 많음, 1.5 는 엄폐물 90도 모서리(1.41)를 놓침 → 1.2.
- `M_Fox`: 노멀맵/금속 끊고 거칠기 1, 스페큘러 0, 노멀 (0,0,1). 커맨드릿에서 `delete_all_material_expressions` 가 `!IsRooted()` 어서트로 죽어서 노드는 지우지 않고 입력만 상수로 다시 연결(안 쓰는 텍스처 노드가 그래프에 남아 있음).
- 외곽선 두께 4 → `OutlineWidth` 6 (UPROPERTY, 동적 인스턴스에 넣음). 잉크 선 바깥에 팀 색 띠가 붙어 스티커처럼 보이고, 빨강 팀도 털과 구분됨.
- 확인 (창 캡처): 엄폐물 테두리/윗면 모서리 잉크 선, 여우 잉크 선 + 팀 색 띠, 평평한 주황. 바닥/엄폐물은 엔진 기본 회색이라 아직 밋밋함.
- 남은 것: Tripo 텍스처에 구운 갈색 음영(귀 등)이 남음, 이펙트(반투명)가 셀 처리에 걸리는지 확인 못 함, 바닥/엄폐물 색 팔레트.
- 수정 (사용자: 선이 지저분하고 떨림): 위치를 Scene Color After DOF 로 옮김 → TSR 앞이라 선이 안티에일리어싱됨(Before Bloom 은 TSR 뒤라 계단/지글거림). `DepthLineThreshold` 0.008 (팔-몸 겹침 선 줄임), 선 경계 램프 ×4 → ×2.
- 카메라는 이제 `/Game/PostProcess/MI_PP_Toon` (인스턴스)을 씀 → 에디터에서 PIE 중 값 바꾸면 바로 보임. `ue_make_toon.py` 로 다시 만들어도 인스턴스에 맞춘 값은 읽어 두었다가 되살림.
- 수정 (사용자: 외곽선만 피아식별 색, 다른 세부 선은 검정이나 캐릭터 색):
  - 외곽선 색은 이제 보는 사람 기준: 내 캐릭터 초록, 아군 파랑, 적 빨강 (HUD 체력 바와 같은 값). `UpdateOutlineColor` 가 매 틱 로컬 플레이어 폰과 비교해 바뀔 때만 설정. `ApplyTeamColor` 는 몸 색만.
  - `M_PP_Toon`: 다섯 샘플 중 하나라도 베이스 컬러가 거의 없으면(언릿 = 외곽선 오버레이) 잉크를 안 그림 → 실루엣은 색 선 하나만. 안쪽 선 색 `InkFromBaseColor`(0 = `InkColor` 검정, 1 = 표면 색 × `InkDarken` 0.35).
  - 사용자가 에디터에서 `MI_PP_Toon` 에 저장한 값: `DepthLineThreshold` 5 (깊이 선 사실상 끔), `LineWidth` 0.5. 그래서 지금 잉크 선은 엄폐물 모서리의 가는 법선 선 정도만 남음.
  - 확인 (창 캡처): 초록/파랑/빨강 외곽선, 검은 이중선 없음.
- 다시 수정 (사용자: 외곽선만 적 빨강/아군 파랑, 세부 선은 검정):
  - 오버레이(부풀린 메시) 외곽선은 팔이 몸 앞에 있으면 팔 둘레에도 색 선이 생김 → 버림. `M_CharacterOutline` 지움.
  - 대신 커스텀 스텐실: 캐릭터 메시 `SetRenderCustomDepth(true)`, `UpdateOutlineStencil` 이 매 틱 로컬 플레이어 기준 아군 1 / 적 2 (내 캐릭터도 아군). `DefaultEngine.ini` 에 `r.CustomDepth=3` (스텐실 포함).
  - `M_PP_Toon`: 캐릭터 밖 픽셀 중 `OutlineWidth`(3) 픽셀 안 8방향에 보이는 캐릭터 픽셀이 있으면 `AllyColor`(0.05, 0.35, 1) / `EnemyColor`(1, 0.08, 0.05). 이웃의 커스텀 깊이가 그 자리 장면 깊이보다 뒤면 안 셈 → 엄폐물에 가려진 부분엔 선 없음. 캐릭터 바깥 경계 양쪽엔 검은 잉크를 안 그림 → 실루엣은 색 선 하나, 안쪽(팔/얼굴)은 검정.
  - 사용자가 맞췄던 `MI_PP_Toon` 값(깊이 선 5, 선 두께 0.5)은 지움 (세부 선이 안 보여서). 기본값 `LineWidth` 1.5, `DepthLineThreshold` 0.004.
  - 확인 (창 캡처 확대): 아군/내 캐릭터 파란 실루엣, 적 빨간 실루엣, 얼굴/팔 검은 선.
  - 사용자: 안쪽 세부 선이 안 보임 → 맞음. 위 "검은 선"은 대부분 텍스처 무늬였고, 엄폐물에 맞춘 기준(깊이 0.004 ≈ 32cm, 법선 1.2 ≈ 90도)으로는 캐릭터 안쪽이 거의 안 잡힘. 캐릭터(스텐실 있는 픽셀)만 `CharacterDepthLineThreshold` 0.0015, `CharacterNormalLineThreshold` 0.6 을 쓰게 함. 1600×900 캡처에서 귀 안쪽, 머리-몸, 팔/손 둘레 검은 선 확인.
  - 사용자: 선이 지저분함, 깔끔하고 조금 더 굵게. 원인: 캐릭터 법선 선(0.6)이 메시 면 꺾임까지 잡아 점선/잔선, 깊이 라플라시안은 경계 양쪽에 끊긴 두 줄.
    - 깊이 선을 한쪽으로: 8방향(`LineWidth` 거리) 이웃 중 가장 가까운 것이 중심보다 (깊이 비율로) 기준 이상 가까우면 잉크 → 윤곽마다 먼 쪽에 그 두께의 한 줄. 바닥은 2.5픽셀 안 깊이 변화가 약 0.0003 이라 안 걸림.
    - 법선 선은 라플라시안 그대로지만 엄폐물 모서리용으로만: `CharacterNormalLineThreshold` 10 (캐릭터에선 사실상 끔).
    - `LineWidth` 1.5 → 2.5, `OutlineWidth` 3 → 3.5.
    - 카메라 모션 블러 끔 (`MotionBlurAmount` 0) — 움직이는 캐릭터의 선이 번졌음.
    - 확인 (1600×900 캡처 확대): 적 빨강/아군·나 파랑 굵은 실루엣, 귀 안쪽/목/팔 경계에 검은 선 한 줄씩.
  - 사용자: "원신 같은 느낌". 기본값을 애니메 쪽으로:
    - 세부 선 색을 표면 색 × 0.3 으로 (`InkFromBaseColor` 1, `InkDarken` 0.3) — 여우는 진갈색 선. 원신처럼 검정 대신 색 선.
    - 그림자 따뜻한 분홍 (`ShadowTint` 0.98, 0.78, 0.86), `LitLevel` 1.35, `BandSoftness` 0.05.
    - 림 라이트 새로 추가: 캐릭터 픽셀 중 실루엣에서 `RimWidth`(3) 픽셀 안이면 베이스 컬러 × `RimColor`(1, 0.92, 0.82) × `RimStrength`(0.45) 만큼 더함 (피아식별 선 바로 안쪽).
    - 카메라 채도 1.15 (`ColorSaturation`).
    - 몸에 섞던 팀 색 끔: 새 UPROPERTY `BodyTeamTint`(0) 을 `ApplyTeamColor` 가 `TintStrength` 로 넣음. 피아식별은 외곽선이 맡음.
    - 확인 (캡처 확대): 밝고 채도 높은 주황, 갈색 세부 선, 가장자리 림, 파란 외곽선. 엄폐물 그림자/모서리도 분홍·갈색 톤.
    - 남은 것(원신 느낌의 가장 큰 차이): 회색 바닥/베이지 엄폐물 배경, 하늘/조명. 얼굴 그림자(SDF)·머리 하이라이트 같은 원신 캐릭터 셰이딩 세부는 캐릭터 머티리얼 작업이 필요.
  - 사용자: 외곽선 피아식별 제거, 머리 위에 피아식별 인디케이터.
    - 캐릭터: 스텐실은 이제 모두 1 (`CharacterStencil`, `BeginPlay` 에서 한 번). `UpdateOutlineStencil`/매 틱 갱신 지움.
    - `M_PP_Toon`: `AllyColor`/`EnemyColor` 대신 `SilhouetteColor`(0.09, 0.04, 0.03 진갈색) 한 색, `OutlineWidth` 2.5.
    - HUD: `DrawFriendOrFoeMarker` — 머리 위 막대들 아래(주사위 게이지 밑 3픽셀), 아래를 가리키는 삼각형 16×10 + 어두운 테두리. 아군(내 캐릭터 포함) `AllyHealthColor` 파랑, 적 `EnemyHealthColor` 빨강. `FCanvasTriangleItem` + `GWhiteTexture` 라서 `Build.cs` 에 `RenderCore` 추가.
    - Smart App Control: `Build.cs` 변경으로 모듈 전체를 다시 빌드한 DLL 이 막힘 (CodeIntegrity 이벤트 3077/3033, 게임 창이 검은 화면으로 멈추고 로그가 `InternalLoadLibrary: 'RRRandom'` 에서 끝남). `RRRandom.cpp` 수정 시각만 바꿔 다시 빌드하니 통과.
    - 확인 (1600×900 캡처): 나/아군 머리 위 파란 삼각형, 적 빨간 삼각형, 쓰러진 캐릭터는 표시 없음(머리 위 HUD 전체가 원래 안 그려짐). 실루엣은 진갈색 한 줄.

## 8차 조정 — 더 빨리 뜨고 대쉬 자주

- 사용자 의견: "천천히 뜨니까 FLY 가 나왔는데 이득이 없네."
- `FlightRiseSpeed` 500 → 900, `FlightMaxHoldTime` 1.2 → 0.75 (최고 높이는 비슷하게, 도달은 1.3초 → 0.75초), `GlideBrakeGravityScale` 2.5 → 4 (빠른 상승이 뗀 뒤 너무 떠오르지 않게), `DashCooldown` 1 → 0.4, 봇 `FlightHoldTime` 0.4~1.2 → 0.25~0.75.
- 확인 (임시 `-RRFlyTest` 로그, 확인 후 지움): 상승 vz ≈ 885, 0.75초에 680, 뗀 뒤 0.2초에 769 에서 멈춤, 0.74초 간격 대쉬 두 번 모두 발동(각 약 410). 빌드 경고 없음.
- 엄폐물 위(160)에서 이륙하면 약 930 → 상한 960 근처.

## 8차 변경 — 대쉬를 "누르고 있는 동안"으로

- 사용자 요청: 일정 거리 대쉬 대신, 더블 클릭 후 마지막 클릭을 떼기 전까지 빠르게 날아감. 떼면 다시 날기 전까지 사용 불가.
- `DashDuration`/`DashCooldown`/`LastDashTime`/`GetDashCooldownRemaining` 삭제. `bDashUsed` (시작 시 켜짐, `Landed` 에서 꺼짐 → 착지 후 다시 떠야 또 사용), `HasDash()`.
- 끝내는 조건: `StopJumping` 오버라이드 (두 번째 누름을 뗌, 조종하는 기기) → `EndDash` → 클라이언트면 `ServerEndDash`. 비행 시간 끝(`!bFlying`)이면 `UpdateDash` 가 `EndDash`. 서버(원격)는 `EndDash` 에서 신뢰 0.5초 연장 후 해제.
- 대쉬 중 WASD 로 방향 바뀜 (`GetLastMovementInputVector`, 없으면 이전 방향). `DashSpeed` 1600 → 1200.
- HUD: `[SPACE x2 HOLD] DASH` / `DASHING` / `DASH AFTER LANDING`.
- 확인 (임시 `-RRFlyTest`: 코드가 정해진 시각에 `Jump`/`StopJumping` 호출 + 테스트 중 플레이어 체력 고정 + 0.1초 로그, 확인 후 지움): ① 1초 누름 → 1초 대쉬, 속도 1200, 높이 457 유지, x 0→1169, 떼자 300 ② 바로 다시 더블 탭+누름 → `used=1` 로 대쉬 안 됨 ③ 착지(`used=0`) → 다시 떠서 0.5초 누름 → 0.5초 대쉬. 빌드 경고 없음.
- 테스트 메모: PostMessage 로 키를 보내는 동안 사용자가 같은 창에서 직접 조작하면 입력이 섞여서(걷기 300 이 찍힘) 결과를 못 믿음 → 코드로 입력을 흉내 내는 방식이 확실함. 처음 시도에선 대쉬 직후 봇에게 맞아 쓰러져서 체력 고정을 추가함.

## 8차 추가 — 비행 중 대쉬 (Space 더블 탭) — 위 "변경"으로 대체됨

- 커밋 `4116a14` 에 포함 (8차 나머지~14차를 한 번에, 높이 상한과 함께).
- `Jump()`: 비행 중이고 지난 누름에서 `DashDoubleTapTime`(0.3초) 안이면 `TryDash`. 성공하면 `JumpPressedTime` 을 지워서 세 번째 누름이 또 대쉬하지 않게 함. 실패(쿨다운 등)하면 보통 점프 처리(공중이라 효과 없음).
- 네트워크는 오르기와 같은 방식: 조종하는 기기만 `TryDash` → `StartDash` + 클라이언트면 `ServerStartDash`. 서버는 원격 클라이언트의 속도를 건드리지 않고 `SetTrustClientMovement(true)`, 끝나고 `ClimbTrustMargin`(0.5초) 뒤 해제. 서버 쿨다운 검사는 `FireTimeSlack` 만큼 여유.
- `UpdateDash`: 조종하는 기기에서 매 틱 `Velocity = 방향 × DashSpeed` (Z 0, 수평 유지). 끝나면 `MaxWalkSpeed`(300)로 줄임 → 공중엔 제동이 없어서 그 속도로 계속 흐르며 활공.
- 상수: `DashSpeed` 1600, `DashDuration` 0.25, `DashCooldown` 1. KO 시 `bDashing` 끔. HUD 왼쪽 아래 `FLY  HOLD [SPACE] RISE  [SPACE x2] DASH  Ns` / 쿨다운 중 `DASH 0.7s`.
- 확인 (임시 `-RRFlyTest` 0.05초 위치 로그, 확인 후 지움): 0.6초 상승(높이 350) 뒤 더블 탭 → x 0 → 409 를 0.25초에 속도 1600, 높이 341 유지, 이후 속도 300 으로 활공. HUD 쿨다운 표시 스크린샷. 빌드 경고 없음.
- 확인 못 한 것: 네트워크 참가자의 대쉬, 엄폐물에 부딪히는 대쉬, 봇 대쉬(없음).

## 8차 수정 — 비행 높이 상한 (캐릭터 키의 5배)

- `FlightMaxHeightInBodies` 5: 발이 경기장 바닥(시작 위치 발 높이) + 5 × 보통 키(192) = 960 위로 못 감. 캐릭터가 `UpdateFlight` 에서 `URRRandomMovementComponent::FlightCeilingZ`(캡슐 중심 기준)를 매 틱 넣음.
- 이동 컴포넌트: 상한 이상이면 `NewFallVelocity` 와 `DoJump(bool, float)` 둘 다 상승 속도를 0 으로. **`DoJump` 도 필요한 이유**: 누르고 있는 점프는 매 프레임 `DoJump` 로 상승 속도를 다시 넣고, `PhysFalling` 은 (이전 속도 + 새 속도)/2 로 움직여서 `NewFallVelocity` 만 막으면 초속 약 245로 계속 올라감 (첫 테스트에서 1226까지 올라감). 한 인자 `DoJump(bool)` 은 엔진에서 두 인자 버전으로 넘기기만 하므로 `using Super::DoJump` 하고 두 인자만 오버라이드 (둘 다 오버라이드하면 무한 재귀).
- 확인 (임시 `-RRFlyTest`: 비행 주사위 100% + `FlightMaxHoldTime` 3초 + 높이 로그, 확인 후 지움): 960~961 에서 멈추고 누르는 동안 유지, 떼면 -250 까지 늘며 하강. 빌드 경고 없음.
- 기본 설정(1.2초)으로는 약 640 이라 상한에 닿지 않음. 상한은 상승 값을 올렸을 때의 안전장치.
- 높이의 득실 메모: 사격 피치 ±50도, 사거리 1300(비행 거리)이라 높이 h 에서 맞힐 수 있는 수평 거리는 h×0.84 ~ √(1300²−h²). 640: 537~1131, 960: 805~877 (거의 없음). 아래에서 위로 쏘는 쪽도 똑같이 제한 → 높을수록 서로 못 맞히는 교착. 이득은 엄폐물 너머 보기(200 정도면 충분), 넓은 시야(카메라가 같이 올라감), 긴 활공(960 → 약 3.8초).

## 8차 — 비행

- 커밋 "Add flight dice" (`git log` 참고).
- 주사위: `FRRDiceRoll::bFlight`, `FlightDieChance` 0.1, `OnFlightDie(Face)`, 색 `GetFlightColor` (분홍). 한 번의 `FRand` 로 무기 / 거인 / 비행 / 버프.
- 상승 = 엔진의 "누르는 만큼 높이 뛰는 점프" 그대로: 비행 중엔 `JumpZVelocity` = `FlightRiseSpeed`(500), `JumpMaxHoldTime` = `FlightMaxHoldTime`(1.2). `bDontFallBelowJumpZVelocityDuringJump`(기본 켜짐) 덕에 누르는 동안 등속 상승. 점프 입력은 원래 네트워크 예측(압축 플래그)에 들어 있어서 따로 RPC 없음.
- 하강 = 새 `URRRandomMovementComponent` (캐릭터 생성자를 `FObjectInitializer` 로 바꿔 `SetDefaultSubobjectClass`): `bGliding` 이면 `GetGravityZ` 가 올라가는 중엔 x2.5 (뗀 뒤 빨리 멈춤), 내려가는 중엔 x0.25, `NewFallVelocity` 에서 낙하 속도 -250 제한. 서버/조종 클라이언트가 같은 계산을 하므로 예측됨.
- 캐릭터 `UpdateFlight` (모든 기기, 매 틱): 점프 값/AirControl 전환, `bGliding` = 비행 중 || (전에 글라이드 중이었고 아직 공중이고 시작 바닥 높이 - 캡슐 반높이 위). 비행 중 `Jump()` 는 `TryClimb` 을 건너뜀, 점프 직후 벽 잡기도 안 함. KO 시 끔.
- 봇: `UpdateFlight` (AI) — 비행 중이고 후퇴 중 아니고 적이 사거리x1.2 안이고 땅에 있으면 `FlightHopDelay`(0.3~1.5초)마다 `Jump`, `FlightHoldTime`(0.4~1.2초) 뒤 `StopJumping`.
- HUD: 머리 위 칩을 `DrawPowerChips` 로 일반화 (`[GIANT] [FLY]` 나란히), 굴림 팝업 `N FLY`, 왼쪽 아래 `FLY  HOLD [SPACE] TO RISE  Ns`.
- 로그: `RRRandom: <이름> (team N) can fly for N s.`
- 확인 (2026-10-08, `-RRSolo` + 임시 `-RRFlyTest` 스위치: 플레이어 주사위를 전부 비행으로 하고 0.2초마다 높이 로그, 확인 후 코드 지움): 빌드 경고 없음. Space 를 누르는 동안 vz ≈ 490 으로 상승, 1.2초 제한에서 높이 약 640, 뗀 뒤 0.2초 안에 상승 멈춤(+10), 하강 속도가 -250 까지 늘었다가 유지, 640 에서 약 2.6초 뒤 착지 → 다시 Space 로 상승. 스크린샷: 머리 위 `FLY`, 아군 `[GIANT] [FLY]`, 굴림 팝업 `5 FLY`, 왼쪽 아래 줄, 공중에 뜬 아군(그림자와 몸이 떨어짐). 봇 비행 로그 여러 번.
- 확인 못 한 것: 네트워크 참가자의 비행(예측/보정이 거슬리는지), 봇이 실제로 떠서 쏘는 모습을 오래 관찰, 맵 밖으로 날아가 떨어지는 경우.
- 메모: 카메라가 캐릭터를 따라 올라가서 화면으로는 높이가 잘 안 느껴짐 (그림자와의 거리로 보임). 필요하면 카메라를 바닥 높이에 고정하거나 그림자/높이 표시 추가.

## 7차 — 거인화

- 커밋 `4ee3a8e` (6차 타이틀과 함께).
- 주사위: `FRRDiceRoll::bGiant`, `GiantDieChance` 0.1, `OnGiantDie(Face)` (무기 주사위와 같은 방식, 가장 높은 눈 한 번). 한 번의 `FRand` 로 무기(< 1/6) / 거인(< 1/6 + 0.1) / 버프를 가름.
- 캐릭터: `bGiant`, `GiantRemaining` 복제 (서버가 줄임). `UpdateGiantSize` 가 모든 기기에서 액터 스케일을 0.3초에 걸쳐 1 ↔ 1.5 로 바꿈. `SetBodyScale` 은 서버/조종하는 클라이언트에서만 캡슐 반높이 변화만큼 Z를 올려 발을 바닥에 둠 (나머지는 위치 복제로 따라옴). 카메라 붐은 `SetUsingAbsoluteScale(true)` 라 시야 그대로.
- 파워: 투사체 `DamageMultiplier` 에 `GiantDamageMultiplier`(1.5) 곱함. 방어: `TakeDamage` 에서 `Super::TakeDamage` 전에 `GiantDamageTakenMultiplier`(0.5) 곱함 → 데미지 숫자도 줄어든 값.
- KO: `bGiant` 끔. 쓰러진 동안엔 크기를 안 건드림 (래그돌 스케일 변경 피함). `ApplyStandingBody` 에서 스케일을 맞춤 → 부활 시 원래 크기.
- 총구 높이(`MuzzleOffset`)는 스케일 안 함. 거인 캡슐 중심이 48 높아도 `SameLevelHeight`(50) 안이라 같은 층 사격은 그대로 수평.
- HUD: 굴림 팝업 `N GIANT` (하늘색 `GetGiantColor`), 머리 위 `GIANT` 칩, 왼쪽 아래 패널 첫 줄. `DrawDicePanel` 인자가 캐릭터로 바뀜.
- 로그: `RRRandom: <이름> (team N) turns giant for N s.`
- 확인 (2026-10-07, `-RRSolo`): 빌드 경고 없음. 약 40초 동안 봇/플레이어 6명 중 5번 거인화 로그. 플레이어 E 굴림 `5 GIANT` → 몸 커짐(아군 대비), 머리 위 칩, 왼쪽 아래 `GIANT ATK x1.5 DMG TAKEN x0.5 10s` (스크린샷). 그 뒤 KO → 부활 시 원래 크기, PISTOL.
- 확인 못 한 것: 시간이 다 돼서 줄어드는 장면(KO 로 먼저 끝남), 데미지 수치 실측(1.5배/절반), 네트워크 참가자 쪽 크기/위치 보정, 엄폐물 옆에서 커질 때 끼임.
- 밸런스 메모: 40초에 5번이면 자주 나오는 편. `GiantDieChance` 로 조절.

## 6차 — 임시 타이틀 화면

- 커밋 `4ee3a8e` (7차 거인화와 함께).
- 타이틀 맵 = 엔진의 빈 맵 `/Engine/Maps/Entry` (`GameDefaultMap`). `GameModeMapPrefixes` 로 이름이 `Entry` 로 시작하는 맵에 `ARRRandomTitleGameMode` (폰 없음, `PlayerCanRestart` false). 맵 에셋을 만들지 않으려고 이렇게 함.
- 새 파일: `RRRandomTitleGameMode`, `RRRandomTitlePlayerController` (메뉴 상태 + Enhanced Input: W/S/화살표, Enter/Space, Esc, 왼쪽 클릭), `RRRandomTitleHUD` (캔버스. 버튼 사각형을 저장해 두고 마우스가 **움직였을 때만** 그 버튼을 선택 → 키보드 선택을 가만히 있는 마우스가 덮어쓰지 않음).
- 메뉴: SINGLE PLAY / NETWORK(→ HOST GAME / JOIN GAME / BACK) / QUIT. 진행 중엔 버튼이 흐려지고 Esc = 취소.
- 세션 서브시스템: `LeaveGame` → `PlaySolo` 로 이름 바꿈, `ReturnToTitle`, `CancelPending`, `OnTitleReady`, `IsOnTitle` 추가. 나간 뒤 어디로 갈지는 `bReturnToSoloAfterDestroy` 대신 `EAfterLeave`(Nothing/PlaySolo/Title). Config `TitleMap`, `ArenaMap` (`DefaultGame.ini`). `GetArenaMap` 은 타이틀이면 `ArenaMap`, 아레나 안이면 지금 맵 (F1 동작 그대로).
- 아레나: Esc(콘솔 `RRTitle`) → 타이틀. HUD 왼쪽 위 키 안내에 `[ESC] TITLE`.
- 연결이 끊기면 엔진이 기본 맵으로 보내므로 이제 참가자는 타이틀로 감 (전엔 혼자 플레이 아레나).
- 실행 옵션 `-RRSolo` 추가 (타이틀 건너뛰고 혼자 플레이). `-RRHost`/`-RRJoin` 은 타이틀에서 바로 실행됨.
- **`RRRandom.Build.cs` 에 `bUseUnity = false`**: 파일이 늘어 unity 묶음이 바뀌자 `ColorParameter`/`RagdollProfile` (Character/Projectile/Cover/Dummy 각각 익명 namespace, 값이 다름) 이 충돌해 빌드 실패. 모듈이 작아서 unity 를 끔.
- 확인 (2026-10-07, `-game` 두 인스턴스, 키는 PostMessage 로 보냄): 빌드 경고 없음. 타이틀 표시(로그 `Game class is 'RRRandomTitleGameMode'`), 클릭으로 SINGLE PLAY → 아레나, 아레나 Esc → 타이틀, NETWORK 화면(`ONLINE VIA LAN` 표시), 호스트 없이 JOIN → `Looking for a game...` + `[ESC] CANCEL` → `No game found...`, 인스턴스 1 HOST GAME → 리슨 서버, 인스턴스 2 JOIN GAME → 호스트 로그 `Join succeeded`, 빨강 0번 자리. 호스트 Esc → 참가자 타이틀에 `Disconnected: ...`. 두 창 QUIT 로 종료.
- 확인 못 한 것: Esc 취소를 진행 중에 실제로 누르는 경우, EOS 경로(키 없음), PIE 에서의 동작 (PIE는 지금 레벨에서 바로 시작하므로 타이틀 안 거침, Esc 는 PIE 종료).
- 캔버스 기본 폰트를 4배로 키워서 제목 글자가 약간 흐림. 임시 화면이라 그대로 둠.

## 5차 — 온라인 플레이 (EOS P2P + LAN 대체)

- 작업 전 상태를 커밋 `91cb123` 으로 저장해 둠. 5차 변경은 `82f6d28` 로 커밋됨.
- 구조: 리슨 서버(호스트가 서버). 체력/탄약/무기/주사위/KO/점수/봇은 호스트 권한, 복제. 자세한 내용은 README "온라인 플레이".
- `URRRandomSessionSubsystem` (새 파일): F1 호스트 / F2 참가 / F3 나가기, 실행 옵션 `-RRHost` `-RRJoin` `-RRLan` `-RRDevAuth=`. EOS면 Dev Auth Tool 로그인 → EOS 세션(로비 아님, presence 없음) → `?listen` 으로 맵 다시 열기. 참가는 `GetResolvedConnectString` → `ClientTravel`.
- EOS 키(`DefaultEngine.ini` 의 `+Artifacts`)가 비어 있어서 EOS 초기화가 실패하고("ProductId cannot be null") 엔진이 Null 서브시스템으로 대체함 → LAN 세션 + IP(7777). 게임 코드는 그대로.
- 넷 드라이버: `NetDriverEOS` (IpNetDriver 대체). LAN 호스트는 `?listen?bUseIPSockets` 로 IP 소켓을 씀. UE 5.8에선 `bIsUsingP2PSockets` 설정이 폐기돼서 넣지 않음.
- `ARRRandomGameState` (새 파일): 팀 점수 복제. HUD는 게임 모드 대신 이걸 읽음.
- 게임 모드: `AllyBotCount`/`EnemyBotCount` → `TeamSize` + 자리(slot). 플레이어가 파랑/빨강 번갈아 봇 자리 대체, `Logout` 시 다음 틱에 봇으로 채움.
- 캐릭터: `Team/Health/Weapon/Ammo/bReloading/bAlive/KnockdownDirection` 복제, `ReloadRemaining/RespawnRemaining` 은 본인만. `ServerFireAt/ServerStartReload/ServerRollDice/ServerStartClimb`, 데미지 숫자는 `MulticastDamagePopup`. KO/부활의 몸 처리(`ApplyKnockedOutBody`/`ApplyStandingBody`)는 `OnRep_Alive` 로 모든 기기에서.
- 오르기: 캐릭터 이동 예측에 없어서, 서버가 `ServerStartClimb` 을 받으면 그 클라이언트 위치를 믿음(`bIgnoreClientMovementErrorChecksAndCorrection` + `bServerAcceptClientAuthoritativePosition`), 오르기 끝 + `ClimbTrustMargin`(0.5초) 뒤 해제. 해제할 때 MOVE_None 이면 Walking 으로 돌림.
- 총알: 호스트만 스폰/판정. 클라이언트 복사본은 충돌 끔, `PostNetReceiveVelocity` 로 무기 속도 반영. 맞으면 `bImpacted`+위치 복제 후 0.2초 숨긴 채 남았다가 사라짐(클라이언트가 이펙트를 낼 시간). 총알 색은 각 기기에서 계산(내 총알만 노랑).
- `FRRWeapon` 을 USTRUCT로, `ERRWeaponTier` 를 UENUM 으로 바꿈(복제하려고).
- 확인 (2026-10-07, 같은 PC 두 인스턴스, LAN): 빌드 경고 없음. `-RRHost` / `-RRJoin` 으로 LAN 검색 → 참가 → 호스트 로그 `Join succeeded`, 참가자는 빨강 0번 자리. 양쪽 스크린샷에서 점수/주사위/엄폐물/봇 복제, `HOSTING (LAN) 2 PLAYERS` / `ONLINE (LAN) 2 PLAYERS`.
  임시 `-RRSelfTest` 코드(참가자 폰을 코드로 조종)로 참가자 쪽 RPC 확인 후 코드는 지움: 오르기(z 98 → 257, 위에서 걷기 유지, 서버가 끌어내리지 않음), 발사(탄약 14→0, 자동 재장전), 주사위(4개 → 버프 2 + SMG 23발), R 재장전, F3 나가기(참가자 혼자 플레이로, 호스트 로그 `left; a bot takes over`, 호스트 `1 PLAYER`).
- 확인 못 한 것: EOS 경로 전체(키 없음), 실제 지연이 있는 인터넷 환경, 사람이 직접 마우스로 하는 사격, 참가자 화면의 데미지 숫자/착탄 이펙트(스크린샷 타이밍에 안 잡힘), 3명 이상.
- 이 PC는 테스트 중 한 번 장시간 멈췄음(로그 시간 09:02 → 10:22 UTC 공백). 게임 문제는 아님.

## 4차 수정 — 위에서 아래 사격 버그

- 원인: `Template_Default` 의 바닥(`Floor_0`)은 **Movable, 오브젝트 타입 WorldDynamic**. 조준 트레이스가 WorldStatic 만 찾아서 바닥을 못 찾고 "내 높이" 로 대체 → 엄폐물 위에선 수평으로 쏴서 머리 위로 지나감. 바닥에선 내 높이가 곧 바닥 높이라 우연히 맞았음.
- 같은 이유로 총알(WorldDynamic 무시하던)이 바닥을 그냥 뚫고 지나갔음.
- 수정:
  - `ARRRandomPlayerController::GetAimTarget` — 커서가 캐릭터 위(캡슐 반지름 + 30 안)면 그 캐릭터 중심을 바로 겨눔. 아니면 `ECC_Visibility` 채널로 트레이스 (캐릭터는 무시) 해서 바닥/윗면 + 캡슐 반높이.
  - `ARRRandomProjectile` — WorldDynamic 도 막도록 바꾸고, 총알끼리는 서로 `IgnoreActorWhenMoving` (새 총알이 날아가는 총알 전부와 양방향). 쓰러진 래그돌은 여전히 WorldDynamic 무시라 총알이 넘어감.
- 확인: 임시 테스트(기둥 위에서 가장 가까운 적의 화면 위치로 조준해 연사)로 조준점이 적 중심(Z 97.6)으로 잡히고, 적 체력이 100 → 0, 파랑 팀 점수 +1. 테스트 코드는 지움.
- 주의: 봇 시야/엄폐/장애물 판정은 여전히 WorldStatic 만 봄 (엄폐물만 대상으로 하려는 의도). 레벨에 Movable 벽을 놓으면 봇은 그걸 엄폐물로 인식 못 함.

## 현재 상태 (4차 작업)

- 구현 완료, 빌드 성공 (경고 없음). 입력을 넣을 수 없어서 임시 테스트 코드(커맨드라인 스위치)로 확인 후 그 코드는 지움:
  - 기둥 앞에서 `Jump()` → Z 98 에서 260 으로 올라감 (0.45초)
  - 위에서 바닥 쪽 `FireAt` → 총알이 비스듬히 내려가 맞은 곳에서 레전드 폭발 (스크린샷)
  - 위에서 앞으로 걸으면 속도 300 으로 걷다가 가장자리에서 떨어져(`Falling`) 바닥(`Floor_0`)에 착지, 바닥에선 다시 `bCanWalkOffLedges = false`
- 오르기 판정 (`ARRRandomCharacter::TryClimb`): 이동 입력 방향(없으면 바라보는 방향)으로 캡슐 중심 높이에서 `WorldStatic` 라인 트레이스 → 벽이면 벽면 안쪽 (반지름+10) 지점을 위에서 아래로 트레이스해 윗면 찾기 (발 위 50~200) → 그 위에 캡슐이 들어갈 자리 확인. 이동은 `DisableMovement` 후 위로 60%, 앞으로 40% 보간, 끝나면 `MOVE_Walking`.
- 오르기 애니메이션은 없음 (마네킹이 그대로 미끄러지듯 올라감).
- 높이 사격: `FireAt` 의 인자는 이제 "대상 캡슐 중심". 총구→(대상+총구 높이) 방향으로 쏘되 높이 차가 50 미만이면 수평, 피치는 ±50도 제한. 플레이어는 커서 광선이 처음 닿는 `WorldStatic` 평면(바닥/엄폐물 윗면) + 캡슐 반높이를 겨눔.
- 아래에서 엄폐물 윗면을 겨누면 위로 쏘게 됨. 위에 아무도 없으면 총알은 계속 올라가서 그 뒤 바닥의 캐릭터는 못 맞힘 (엄폐물 너머로 쏘는 꼼수는 안 됨).
- 봇은 오르지 않음. 위에 있는 적은 기울여 쏘기만 함.
- Space 의 랜덤 이벤트는 입력에서 빠짐. `URRRandomEventComponent` 자체는 몸 색/속도 배율 때문에 남겨 둠.
- 맵 밖으로 떨어지면 (시작 높이보다 1500 아래) 쓰러진 것으로 처리.

## 현재 상태 (3차 작업)

- 구현 완료, 빌드 성공 (경고 없음). `-game` 으로 실행해 스크린샷 확인:
  - 무기 주사위 → 새 무기 (`5 MOVE  4 GUN` 팝업 뒤 LEGEND CANNON 장착), 머리 위 무기 칩(등급 색 테두리), 오른쪽 아래 무기 정보
  - 등급별 빛(레어 파랑/에픽 보라/레전드 금색), 레전드 착탄 시 연기 + 불꽃 줄기
  - 로그에 Niagara/우리 코드 경고 없음
- 이펙트는 엔진 Niagara 템플릿(`/Niagara/DefaultAssets/Templates/Systems/...`)을 그대로 씀. **템플릿엔 User 파라미터가 없어서 색을 바꿀 수 없음** (흰색/회색 불꽃과 연기). 등급 색은 총알에 붙인 포인트 라이트로 냄. 제대로 색을 입히려면 템플릿을 복제해 `User.Color` 를 노출한 프로젝트 에셋이 필요.
- 템플릿은 미리보기용으로 반복 재생할 수 있어서, 한 번만 터지는 이펙트는 0.25초 뒤 `Deactivate` 해서 남은 파티클만 사라지게 함 (`SpawnOneShot`).
- 꼬리 이펙트(`FountainLightweight`)는 총알에 붙이지 않고 따로 스폰해서 Tick 에서 위치를 옮김. 총알이 사라져도 파티클이 남게 하려는 것.
- **Smart App Control 주의**: 이 PC는 Smart App Control 이 켜져 있어서, 새로 빌드한 서명 없는 `UnrealEditor-RRRandom.dll` 을 가끔 막음 (로그에 `GetLastError=4551`, "The game module 'RRRandom' could not be loaded" 창). 소스 파일 하나 저장 시각을 바꾸고 다시 빌드하니 통과됨. 이벤트 뷰어 `Microsoft-Windows-CodeIntegrity/Operational` 에서 확인 가능.
- 확인 못 한 것: 에픽 꼬리 이펙트가 스크린샷에 뚜렷이 안 잡힘, 무기 밸런스, 무기가 바뀌는 빈도(40초쯤에 6명 중 대부분이 무기를 가짐)

## 현재 상태 (2차 작업)

- 구현 완료, 빌드 성공 (경고 없음). `-game` 으로 실행해 스크린샷 확인:
  - 엄폐물 9개가 의도한 위치에 섬 (`Template_Default` 바닥이 양옆 ±1400 긴 벽까지 덮음)
  - 봇이 엄폐물 바로 뒤에 붙어 숨는 모습, 총알 연사, 오른쪽 아래 `10 / 15`, `RELOADING 61%` 표시
  - 실행 중 게임 창이 포커스를 갖고 있었고 플레이어 캐릭터가 움직이고/쏘고/주사위를 굴림 → 사용자가 직접 플레이한 것으로 보임 (AI가 플레이어 폰을 조종하는 경로는 코드상 없음)
  - 로그에 우리 코드 관련 경고/에러 없음
- 확인 못 한 것: R 키 수동 재장전 직접 입력, 봇이 엄폐물 모서리에서 끼는지 장시간 관찰

## 이전 상태 (1차 작업)

- 구현 완료, `RRRandomEditor Win64 Development` 빌드 성공 (경고 없음).
- `-game -windowed` 로 실행해서 스크린샷으로 확인한 것:
  - 3 대 3 스폰, 봇끼리 교전, 플레이어 KO → 점수 `0 : 1` → 카운트다운 → 시작 위치 부활 → 체력 회복
  - 머리 위 체력 바(나/아군/적 색 구분), 주사위 개수 박스, 게이지, 버프 칩(`ATK+20`, `ASPD+15` 등) — 아군 봇과 적 봇 모두 주사위를 굴린 것 확인
  - 로그에 우리 코드 관련 경고/에러 없음
- **직접 확인 못 한 것** (테스트 중 입력을 넣을 수 없었음, 플레이어는 가만히 서 있었음):
  - 플레이어 마우스 사격/WASD (입력 바인딩은 그대로이고, 사격은 봇과 같은 `FireAt` 경로)
  - 굴림 결과 팝업과 데미지 숫자 (스크린샷 타이밍에 안 잡힘)
- 화면이 잠긴 상태에서도 `PrintWindow` 로 게임 창을 캡처할 수 있음 (데스크톱 캡처는 잠금 화면만 찍힘).

## 동작 규칙

| 항목 | 값 | 조절 위치 |
| --- | --- | --- |
| 팀 구성 | 나 + 아군 봇 2 vs 적 봇 3 | `ARRRandomGameMode` `AllyBotCount`, `EnemyBotCount` |
| 적 시작 위치 | 플레이어 스타트에서 +X(화면 위) 2000 | `TeamSpawnDistance`, 옆 간격 `SpawnSpacing` 300 |
| 체력 | 100 (원래 50의 두 배) | `ARRRandomCharacter` `MaxHealth` |
| 회복 | 3초 비전투 후 초당 최대 체력의 15% | `RegenDelay`, `RegenPerSecond` |
| 부활 | 4초 뒤 시작 위치 | `RespawnDelay` |
| 탄창 | 15발, 예비 탄약 무한, 전체 재장전 1.5초 (빈 탄창은 자동, R 키 수동), 자동 연사 0.15초 간격 | `MaxAmmo`, `ReloadTime`, `FireInterval` |
| 이동 속도 | 300 (원래 600의 절반) | `ARRRandomCharacter` 생성자 `MaxWalkSpeed` |
| 엄폐물 | 9개 (가운데 벽 3, 팀 앞 기둥 2+2, 양옆 긴 벽 2), 높이 160 | `ARRRandomGameMode` `CoverLayout`, `bSpawnCover` |
| 사거리 | 1300 | `AttackRange` (투사체 `MaxRange` 로 전달) |
| 데미지 | 8~15 랜덤 × 공격력 배율 | `ARRRandomProjectile` |
| 총알 속도 | 5000 (원래 2500의 두 배), 사거리 1300 기준 약 0.26초에 도달 | `ARRRandomProjectile` 생성자 `InitialSpeed`/`MaxSpeed` |
| 주사위 | 3개로 시작, 15초마다 1개, 버프 10~30초 랜덤, 눈 1개당 +5% | `StartingCharges`, `GaugeFillTime`, `BuffDurationRange` (`URRRandomDiceBuffComponent`) |
| 오르기 | 발 위 50~200 높이 윗면, 벽까지 캡슐 앞 60, 0.45초, 점프 후 0.5초 안에 벽에 닿아도 오름 | `MaxClimbHeight`, `ClimbReach`, `ClimbDuration` |
| 높이 사격 | 높이 차 50 이상이면 기울여 쏨, 최대 ±50도 | `MaxShotPitch`, `SameLevelHeight` |
| 무기 주사위 | 주사위마다 1/6 확률. 여러 개면 가장 높은 눈으로 무기 1개 | `WeaponDieChance` |
| 무기 종류 | MINIGUN / SMG / RIFLE / BLASTER / CANNON, 배율은 README 표 | `RRRandomWeapon.cpp` `WeaponTypes` |
| 무기 등급 | 한 발 데미지 배율 x1.1 RARE, x1.5 EPIC, x2.1 LEGEND | `RareDamage`, `EpicDamage`, `LegendaryDamage` |
| 등급별 이펙트 | 빛 세기/반경, 총구/꼬리/착탄 이펙트와 크기 | `RRRandomProjectile.cpp` `TierLooks` |
| 봇 선호 거리 | 사거리의 60~85% (봇마다 다름) | `ARRRandomAIController` `PreferredRangeShare` |
| 봇 후퇴/복귀 | 체력 45% 아래면 엄폐물 뒤로, 90% 회복 시 복귀 | `RetreatHealthShare`, `ReturnHealthShare` |
| 봇 엄폐 | 재장전 중, 후퇴 중, 또는 1.5초 안에 맞았고 체력 70% 아래면 적 반대편 엄폐물 뒤로 | `UnderFireCoverHealthShare`, `CoverSearchRadius` |
| 봇 사격 | 엄폐물에 안 가릴 때만. 적이 처음 보이면 0.25~0.55초 반응, 3~5발 점사 후 0.4~0.9초 쉼 | `ReactionTime`, `BurstSize`, `BurstPause` |
| 봇 재장전 | 보이는 적이 없고 탄창 60% 이하면 미리, 점사 사이에 3발 이하면 바로 | `TacticalReloadShare` |
| 봇 조준 오차 | ±6도 | `AimErrorDegrees` |
| 봇 주사위 | 적이 사거리×1.2 안이면 굴림, 아니면 2~4개 쌓일 때 굴림 | `DiceHoldRange` |

- 공격속도 버프는 연사 간격과 재장전 속도 둘 다에 곱해짐.
- KO 시 활성 버프는 사라지고 모아둔 주사위는 유지.
- 아군 오사 없음: 투사체가 쏜 쪽 팀 전원을 무시 + `TakeDamage` 에서도 같은 팀이면 무시.
- 투사체끼리는 서로 통과 (`WorldDynamic` 무시). 쓰러진 캐릭터 래그돌도 투사체를 막지 않음.
- 모두 `bCanWalkOffLedges = false` 라 바닥 끝에서 떨어지지 않음.
- 과녁 자동 배치는 기본 꺼짐 (`bSpawnDummies`). 과녁 클래스 자체는 그대로.

## 머리 위 인디케이터 (HUD 캔버스, `ARRRandomHUD::DrawOverhead`)

위에서 아래로: 3D 주사위(`URRRandomOverheadDieComponent`, 10차 추가) → 굴림 결과(`5 ATK`, 착지 후 1.6초) → 파워 칩(GIANT/FLY) → 버프 칩(능력치별 합계, 3초 남으면 깜빡임) → 체력 숫자
→ 체력 바(나 초록/아군 파랑/적 빨강) + 오른쪽 노란 박스에 모아둔 주사위 개수 → 탄창(내 캐릭터만) → 다음 주사위 게이지.
데미지 숫자는 몸 오른쪽에서 떠오름 (우리 편이 맞으면 빨강).
월드 위치를 `Project` 로 화면에 옮겨 그리므로 에셋/위젯 없음. 겹치면 `OverheadLift`, `HeadClearance` 로 위치 조절.

## 변경 파일 (4차)

- `RRRandomPlayerController.h/.cpp` — `RollAction` → `JumpAction` (Space, 누름/뗌), 커서 아래 바닥/윗면 높이로 조준
- `RRRandomCharacter.h/.cpp` — `Jump` 오버라이드, `TryClimb`/`UpdateClimb`, `UpdateLedgeWalking`, `FireAt` 높이 사격, `AirControl` 0.35, 맵 밖 낙하 처리
- `RRRandomAIController.cpp` — 대상 높이 그대로 조준 (점프 중 속도는 리드하지 않음)
- `README.md`

## 변경 파일 (3차)

새로 추가
- `Public/RRRandomWeapon.h`, `Private/RRRandomWeapon.cpp` — `FRRWeapon` (일반 C++ 구조체, UHT 없음), 무기 종류 표, 등급/색

수정
- `RRRandom.Build.cs` — `Niagara`, `RRRandom.uproject` — Niagara 플러그인 명시
- `RRRandomDiceBuffComponent.h/.cpp` — `WeaponDieChance`(1/6), `FRRDiceRoll::bWeapon`, `OnWeaponDie` 델리게이트
- `RRRandomCharacter.h/.cpp` — `Weapon`, `EquipWeapon`, `GetReloadTime`, 무기 배율을 연사/재장전/탄창/데미지/총알 속도·크기에 적용, KO 시 PISTOL
- `RRRandomProjectile.h/.cpp` — `Tier`, `SetSpeedMultiplier`, `Glow` 라이트, Niagara 이펙트 5종, `TierLooks` 표
- `RRRandomAIController.cpp` — 점사 길이를 탄창 크기에 비례, 저탄약 재장전 기준을 `min(3, 탄창/5)`
- `RRRandomHUD.h/.cpp` — 무기 칩, `GUN` 주사위 팝업, 오른쪽 아래 무기 정보, 20발 넘는 탄창은 한 줄 바
- `README.md`

## 변경 파일 (2차)

새로 추가
- `Public/RRRandomCover.h`, `Private/RRRandomCover.cpp` — 엄폐물 블록 (`FRRCoverPlacement` 도 여기)

수정
- `RRRandomCharacter.h/.cpp` — `Ammo` 를 int로, `bReloading`/`StartReload`/`GetReloadProgress`, 빈 탄창 자동 재장전, `MaxWalkSpeed` 300
- `RRRandomPlayerController.h/.cpp` — R 키 `ReloadAction`
- `RRRandomGameMode.h/.cpp` — `CoverLayout`/`bSpawnCover`, 봇보다 먼저 엄폐물 배치
- `RRRandomAIController.h/.cpp` — 거의 새로 씀: 시야 판정, 엄폐 지점 찾기, 장애물 회피, 반응 시간, 점사, 전술 재장전 (`ShotDelay` 는 `BurstPause` 로 바뀜)
- `RRRandomHUD.h/.cpp` — 탄창 15칸/재장전 바, 오른쪽 아래 탄약 표시
- `README.md`

## 변경 파일 (1차)

새로 추가
- `Public/RRRandomAIController.h`, `Private/RRRandomAIController.cpp` — 봇 컨트롤러

수정
- `RRRandom.Build.cs` — `AIModule`, `GameplayTasks`
- `RRRandomCharacter.h/.cpp` — 팀, 체력/회복, 탄창, KO/부활, 데미지 팝업, `FireAt` 이 bool 반환 + 투사체 지연 스폰
- `RRRandomProjectile.h/.cpp` — `MaxRange`, `ShotColor`, 팀원/투사체 통과
- `RRRandomDiceBuffComponent.h/.cpp` — 화면 디버그 메시지 제거, `FRRDiceRoll`/`GetLastRoll`, `ClearBuffs`, 능력치 색/짧은 이름
- `RRRandomGameMode.h/.cpp` — 봇 스폰, 팀 점수, `bSpawnDummies`
- `RRRandomHUD.h/.cpp` — 머리 위 인디케이터, 점수, 부활 카운트다운
- `RRRandomPlayerController.cpp` — KO 중 Space/E 무시
- `README.md` — 기본 플레이 설명, 코드 구조

어제(주사위 버프) 작업까지 포함해서 아직 git 커밋 안 됨.

## 빌드 / 실행 명령

```powershell
# 빌드 (에디터가 열려 있으면 Live Coding 때문에 실패할 수 있음 — 에디터를 닫고 실행)
& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" RRRandomEditor Win64 Development "-Project=C:\work\RRRandom\RRRandom.uproject" -WaitMutex -NoHotReloadFromIDE

# 게임 모드로 바로 실행
& "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "C:\work\RRRandom\RRRandom.uproject" -game -windowed -ResX=1280 -ResY=720 -log
```

로그: `Saved/Logs/RRRandom.log`

## 참고 / 결정 사항

- HUD 글자는 영어. 캔버스 기본 폰트에서 한글이 깨질 수 있어서.
- 버프는 시간제(10~30초 랜덤, 10차 추가 전엔 15초 고정). 무한히 강해지는 것을 막기 위함.
- 봇은 내비메시 없이 `AddMovementInput` 으로 직접 이동. 앞 120 거리를 구체로 스윕해서 엄폐물이 있으면 35/70/105/140도씩 꺾어 비켜 감 (지난번에 비켜 간 쪽을 먼저 시도). 엄폐물이 복잡해지면(ㄷ자 벽 등) 갇힐 수 있으니 그땐 내비메시 필요.
- 시야/엄폐 판정은 모두 `WorldStatic` 오브젝트 대상 스윕 (총알 높이 = 캡슐 중심 +2, 쏘는 쪽 오른쪽 12, 반지름 12 — 12차에서 총구 끝에 맞춤, 전엔 +20). 엄폐물은 `BlockAll` 이라 `WorldStatic`. 바닥도 `WorldStatic` 이지만 수평 스윕이라 안 걸림.
- 엄폐물은 봇보다 먼저 세움. 봇 스폰 위치를 찾는 바닥 트레이스(`ECC_Visibility`)가 블록 위에 떨어지지 않도록.
- 체력은 처음엔 브롤스타즈처럼 3~5발에 쓰러지게 50이었으나, 자동 연사로 교전이 너무 빨라 100으로 올림 (평균 9발 정도에 쓰러짐).

## 다음에 할 만한 것

- [ ] EOS 키 넣고 Dev Auth Tool 두 계정으로 EOS 경로 확인 (README "EOS로 인터넷 플레이")
- [ ] 지연 상황 테스트: 실행 옵션 `-PktLag=150` 등으로 사격/오르기 체감 확인
- [ ] 과녁(`RRRandomDummy`) 동기화, 호스트 이전, 로비 UI(지금은 첫 번째 게임에 자동 참가 — 타이틀 JOIN 에 게임 목록을 보여 주는 식으로 확장 가능)
- [ ] 제대로 된 타이틀 (UMG 위젯/폰트, 배경 맵), 게임 중 일시정지 메뉴

- [ ] 실제 플레이로 마우스 사격, 굴림 팝업, 데미지 숫자, 밸런스(체력/재장전/봇 난이도) 확인
- [x] 엄폐물 + 봇 시야 체크 (덤불은 아직)
- [ ] 밸런스: 체력 100, 총알 속도 5000으로 올린 뒤의 교전 속도 확인 (체력 50일 땐 40초쯤에 11:5)
- [ ] 엄폐물 파괴, 덤불(숨기)
- [ ] 봇도 엄폐물에 올라가기 (높은 곳 차지), 오르기 애니메이션
- [ ] 무기 이펙트 색: 템플릿 복제 후 `User.Color` 노출해서 등급 색으로 칠하기
- [ ] 무기 바뀌는 빈도/밸런스 (`WeaponDieChance`, `WeaponTypes`)
- [ ] 승리 조건 (예: 10킬 먼저) 과 라운드 재시작
- [ ] 사거리 표시 (조준선), 피격 번쩍임
- [ ] 주사위 최대 보유 개수 / 버프 상한
