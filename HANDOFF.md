# 작업 이어하기 메모 — 팀전 프로토타입 (FPS식 사격 + 엄폐물)

최종 업데이트: 2026-10-07 (6차)

Claude 기억 메모(작업 방식·아트 방향·주의점)는 `Docs/ClaudeMemory/` 에 복사해 둠 — 다른 PC 세션은 먼저 읽기.

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
- (21차) TPS 모드 전용 맵 "주사위 랜드": 파스텔 → (피드백) 진하고 생동감 있는 사탕색 주사위 섬, 무지개 주사위, 주사위 숲, 구름. TPS 용으로 엄폐물 많이, 디자인을 해치지 않게 자연스럽게. Tripo 로 3D 를 뽑아 TPS 로 시작할 때 나오게.
- (22차) TPS 카메라: 거인·비행 때 각도/거리 조절, 조준 줌 때 캐릭터가 화면을 너무 가림. 거대화·날개·피격 이펙트를 만들어 강화.
- (23차) 토끼·다람쥐·호랑이를 여우와 비슷한 느낌으로 추가, 시작할 때 고름. 봇은 랜덤. Tripo + Mixamo 로.
- (24차) 차에 치이면 뒤로 날아가는 모션과 함께 날아가며 쓰러짐. Mixamo 에서 모션.
- (26차) 궁극기 주사위를 얻는 치트, 궁극기 주사위를 굴릴 때 더 화려하게.
- (27차) 호랑이 기운 범위 지정: 캐릭터와 상관없이 주변 원하는 곳으로 뛸 수 있고, 범위 안 적은 모두 피해.
- (28차) 범위 표시가 바닥에 고정 안 되고 멀수록 커짐 → 홀로그램 같은 이펙트로, 데미지도 정상으로.
- (29차) 여우·토끼·다람쥐 궁극기(추천 조합): 여우 둔갑술, 토끼 달토끼 낙하, 다람쥐 도토리 비.
- (25차) 호랑이 뒤통수 호피무늬, 다람쥐 머리-꼬리 사이 깨짐, 토끼 주둥이를 더 귀여운 카툰으로, 호랑이 궁극기 "호랑이 기운"(궁극기 주사위 → 레벨만큼 거대해져 주사위 숫자만큼 앞발 내려치기, 범위 캐릭터의 약 10배).
- (30차) 드래곤 캐릭터 추가 시작: 서양 문화권의 붉은 드래곤. 먼저 Tripo 로 컨셉아트를 앞·뒤·옆·위 4각도로. → (덧붙임) 정면 그림으로 3D 모델 만들고 리깅까지.
- (31차) 드래곤 궁극기에 쓸 고전적인 네 발 드래곤 모습: 앞뒤옆위 컨셉아트 + 3D 모델. 아트 느낌은 크게 벗어나지 않으면서 멋지게.
- (32차) 드래곤 캐릭터를 게임에 추가. 궁극기: 두 번째(고전) 용이 하늘을 날며 지정한 경로에 브레스를 뿜고 맵을 가로질러 날아감. 브레스 굵기와 용의 크기는 궁극기 주사위 숫자만큼.
- (33차) 용이 더 날렵하고 빠르게. 범위는 캐릭터 위치부터 쭉 브레스를 뿜으며 가야 하고, 주사위로 좌우 브레스 폭이 넓어짐.
- (34차) 용 디자인은 그대로, 자세만 고속으로 나는 모습으로 모델링을 새로.
- (35차) 그 모델을 궁극기에 바로 적용.
- (36차) TPS 맵 컨셉으로 10배쯤 큰 맵: 평지보다 들어가고 올라갈 건물·구조물이 많게, 메이플스토리2처럼 사각 블록을 쌓은 느낌. 컨셉 3장 중 첫 번째(전경) 스타일로 제작. TPS 시작할 때 맵 고르는 UI. 2층이 있는 건물은 올라갈 길이 꼭 있어야 하고, 창문은 모든 캐릭터가 뛰어내릴 수 있게 커야.
- (37차) 궁극기 용을 지금의 두 배로. 주사위 숫자에 따라 용 크기·브레스 폭이 커지는지 확인하고 아니면 커지게.
- (38차) 용 궁극기 방향 설정 이펙트가 방향을 지시하는 느낌이 나게.
- (39차) 방향을 설정하면 용이 나타나서 빠르게 주변을 한바퀴 돌고 출발.
- (40차) 빙글 돌고 나서 그대로 이어서 날아가게, 용한테 중간에 보이는 이펙트 없애기.
- (41차) 용 인간폼 뒷모습 티셔츠를 컨셉 뒷모습 참고해서 보정.
- (42차) 트라이포 원본부터 다시 작업해서 덮어쓰기: 날개에 흰색이 침범하지 않게, 등 돌기는 무시하고 날개만 셔츠를 뚫고 나온 것처럼.
- (43차) 용은 티셔츠 없는 버전으로.
- (44차) 기존 걸 고치지 말고 트라이포 단계부터 티셔츠 없는 모델링을 새로 뽑아서 적용(입은 상태로 뽑힌 모델이라 이상함).
- (45차) 용 궁극기 브레스가 지나간 자리에 작은 불씨가 남고 3초 동안 도트 데미지.
- (46차) 불씨가 더 잘 보이게, 궁극기가 지나간 자리에 생기는 텍스처를 어울리게 변경.
- (47차) 레퍼런스 그림(사용자 `Downloads\level_design.png`: 파스텔 주사위 탑, 주사위 무지개로 이은 떠 있는 섬, 지붕 위 주사위, 물길·폭포)을 보고 주사위 마을 레벨 디자인 보완. 상자만 쌓이고 반복되는 디자인이라 꿈나라 같은 오브젝트와 주사위가 강조되는 오브젝트가 필요. 게임에 실제 올리기 전에 먼저 보여줄 것. 트라이포든 직접이든 더 나은 쪽으로. → 비교 페이지를 보고 "적용해줘".
- (48차) 건물의 지붕이나 나무 같은 오브젝트까지 블록 형태일 필요는 없음. 필요하면 트라이포를 써서 보완.
- (49차) 프로젝트 전체 UI 를 깔끔한 파스텔톤, 단순하게. 트라이포와 클로드로 시안 3개씩 뽑아 먼저 보여주고, 적용은 확인 후. → 클로드 시안 A 마시멜로 / B 스티커 / C 에어 라인 비교 페이지. 사용자 선택(조합안): 인게임 HUD 는 에어 라인, 캐릭터 선택은 스티커 구조에 에어 라인 느낌, 결과 화면은 마시멜로 + 에어 라인, 결과 화면에 배경 음영 없음. → "조합안으로 적용해줘".

## 49차 — 파스텔 UI 조합안 적용 (사용자: 깔끔한 파스텔톤·단순함; 인게임 HUD 는 에어 라인, 캐릭터 선택은 스티커 구조 + 에어 라인 느낌, 결과는 마시멜로 + 에어 라인, 결과 화면 배경 음영 없음)

- 시안: 클로드 HTML 시안 3개(A 마시멜로: 반투명 흰 알약 패널 / B 스티커: 흰 테두리 납작한 색면 / C 에어 라인: 상자 없이 글자·가는 막대·동그란 미니맵) + 조합안, 비교 페이지 https://claude.ai/artifact/48P8TVxc3WhCP533pVnEHJ (원본 `scratchpad\ui-mockups\index.html`). **Tripo 시안은 못 뽑음** — 그 세션에 Chrome 확장이 연결 안 돼 있었고 사용자가 조합안으로 바로 적용을 골랐음.
- 공통 스타일 `RRUI`(새 `Public/RRRandomUIStyle.h`, `Private/RRRandomUIStyle.cpp`):
  - 팔레트(sRGB): Ink 74,63,92 / InkSoft 122,112,138(불투명 — 반투명이면 어두운 맵에서 묻힘) / Pink 247,168,196, PinkDeep 242,127,166 / Mint 143,221,190, MintDeep 124,203,172 / Lavender 189,166,240, LavenderDeep 154,123,224 / Butter 247,216,138 / Gold 233,169,63 / Sky 151,201,245, SkyDeep 102,165,230. 파랑 팀 = 하늘, 빨강 팀 = 분홍.
  - 글꼴 Nunito SemiBold/ExtraBold/Black(Google Fonts 정적 TTF, OFL → `Content/Fonts/Nunito/`, 원본 `char-pipeline\input\fonts_nunito\`). **런타임 UFont**(`FontCacheType=Runtime`, `AppendFont(파일 경로, LazyLoad)`)를 한 번 만들어 루트에 두고 `FSlateFontInfo`(크기 = 픽셀 × 0.75 포인트, 정수로 반올림해 캐시 폭증 방지)로 `FCanvasTextItem` 에 그림 → 어떤 크기든 선명. 측정은 `FSlateApplication` 의 FontMeasure(Build.cs 에 Slate, SlateCore 추가). 매 프레임 크기가 변하는 글자(카운트다운 팝, 궁극기 배너)는 `Zoom`(아이템 Scale)으로. 패키징용 `DefaultGame.ini` `+DirectoriesToAlwaysStageAsNonUFS=(Path="Fonts/Nunito")`.
    - Font Face 에셋으로 가져오기(`char-pipeline\scripts\ue_import_font.py`, 새)는 커맨드릿에서 **Slate 가 없어 `CurrentApplication.IsValid()` 어서트로 죽음** → 파일 직접 읽기로 감. 스크립트는 에디터 안에서 쓸 때를 위해 남김.
  - 모양: 흰 스프라이트 두 장(`char-pipeline\scripts\make_ui_shapes.py`, 새 — 블렌더 파이썬으로 `python -I make_ui_shapes.py <out>` → `output\ui\T_UICircle.png`(256, 꽉 찬 원), `T_UISoft.png`(128, 가운데 20% 단단 + smoothstep 감쇠) → `ue_import_ui_texture.py` → `/Game/UI/T_UICircle`, `/Game/UI/T_UISoft`). 사분면을 모서리로 쓰는 9-슬라이스(조각끼리 안 겹쳐 반투명 색이 고름)로 둥근 사각형·알약·부드러운 그림자, 원은 원 스프라이트 통째, 고리·점선 고리는 삼각형 띠, **텍스처를 원/둥근 사각형으로 자르기**는 둘레 점(모서리마다 8단계 호)에서 가운데로 부채꼴 삼각형 + UV(초상화, 미니맵 — 새 텍스처 필요 없음). 스프라이트는 각 HUD 가 UPROPERTY 로 붙잡고 그리기 전에 `RRUI::UseShapes` 로 넘김.
  - 게임 위 글자: 처음엔 흰 글자 복사본 고리(헤일로) + 큰 흰 번짐(wash)을 깔았는데, 섬 맵(진한 초록 + 안개)에선 작은 글자가 울퉁불퉁한 외곽선처럼 보이고 번짐이 안개 덩어리 같았음 → **`DrawPlate`**: 글자 몸통을 감싸는 반투명 흰 알약(0.5) + 바깥으로 부드러운 테두리(T_UISoft 9-슬라이스, 0.4). 점수·주사위 줄·탄약 줄·팀원 카드는 묶음마다 받침 하나, 나머지(머리 위 숫자·태그·온라인 줄)는 글자마다. 큰 제목(`DrawGradientText`: 글자마다 색 보간)은 받침 대신 흰 테두리 고리 12방향 두 겹 + 잉크 그림자.
- 인게임 HUD(`RRRandomHUD.cpp`, `RRRandomHUDPanels.cpp` 거의 다시 씀; 모든 크기 화면 높이 720 기준 × `UIScale`):
  - 점수: `BLUE 3 · 2 RED`(팀 글자색), 아래 가는 선이 양 끝에서 목표 쪽으로 차오름(하늘/분홍), `FIRST TO 10`.
  - 미니맵: 지름 190 원(`MinimapSize` 220 → 190, 화면 비례), 흰 테두리 + 옅은 바깥 고리. 텍스처는 지형색을 22% 밝히고 안개는 검게 대신 라벤더(222,214,236)로 덮음(가 봄 45%, 안 가 봄 82%). 마커: 나 흰 화살표(잉크 테두리), 아군 SkyDeep·적 PinkDeep 점(흰 테두리), KO X. 원 밖 마커는 원 둘레에 붙임. 클릭 판정도 원(`IsOnCornerMap`). 전체 화면은 옅은 라벤더 막 + 흰 패널 + 둥근 모서리 지도.
  - 머리 위: 둥근 체력 바(나 MintDeep / 아군 SkyDeep / 적 PinkDeep, 흰 트랙 + 잉크 테두리), 숫자, 오른쪽 ◆ 주사위 수(라벤더, 궁극기 금색), 왼쪽 총 이름(등급색을 잉크 쪽으로), 아래 탄창 칸(금색)·게이지(라벤더), 흰 테두리 피아 삼각형, 위로 태그(왼쪽 색 막대 + 글자)들과 굴림 결과.
  - 왼쪽 아래: 큰 주사위 수 + `DICE` + 게이지 + `E` 키 캡(궁극기면 금색 수 + `ULTIMATE` 태그), 위로 궁극기·비행·거인·버프 태그(남은 시간, 3초 남으면 깜빡임). 오른쪽 아래: `PISTOL 12 / 15 R`(¼ 이하면 분홍), 재장전은 `RELOADING` + 버터 진행 바, 새 총이면 그 위에 `RARE RIFLE DMG x1.3 · SHOT SPD x1.3` 태그. 팀원 카드: 상자 없이 동그란 초상화(팀색 + 흰 고리), 이름·체력·민트 바·`DICE 1 · CANNON`. 데미지 숫자: 흰 글자 + 잉크 테두리(우리 편은 분홍 + 흰 테두리). 카운트다운 `GET READY` + 라벤더→분홍 숫자, `GO!` 분홍→금. 3인칭 조준점: 점 + 8조각 점선 고리(조준 시 좁아지고 쏘면 벌어짐). 온라인/치트 안내는 치트 줄이 화면 가운데까지 넘어가서 두 줄로 나눔.
  - 결과: **음영·위아래 띠 없음**. `VICTORY`(라벤더→분홍→금) / `DEFEAT`(하늘→라벤더) / 관전 `BLUE WINS`, 점수 알약, 양쪽 반투명 흰 팀 패널(머리 줄 아래 팀색 선, 행마다 가는 구분선, YOU 라벤더, MVP 금색), 가운데 MVP 알약(동그란 초상화 + 버터 고리, 그 캐릭터의 그림), 버튼 NEW GAME(민트 + 진한 민트 바닥, 주 버튼)/TITLE(흰색), 마우스를 올리면 2 위로. 일시정지: 옅은 라벤더 막 + `PAUSED`(라벤더→분홍) + RESUME(민트)/TITLE.
- 타이틀(`RRRandomTitleHUD.cpp`): 왼쪽 어둡게 → 흰 빛(가장자리 0.85, 화면 42% 에서 0.6, 58% 에서 0), 로고 `RRR`(라벤더)`ANDOM`(잉크) + 파스텔 막대 5개 + `TEAM BRAWL PROTOTYPE`, 버튼은 반투명 흰 알약(고른 것 흰색 + 분홍 테두리 + 그림자). 캐릭터 페이지: 폭 570 안에 카드 5장(높이 128, 초상화 원 80), 고른 카드는 8 떠오름 + 분홍 테두리 + 이름 밑줄, 그 아래 설명 한 줄(분홍 표시), BACK 은 작은 알약(200×44). 버튼 사각형이 카드 자리라 마우스 선택도 그대로. `ARRRandomTitlePlayerController` 에 A/←(위), D/→(아래) 키 추가, 안내 `A/D SELECT`.
- 확인(창 캡처, 스크래치 `ui_tour.ps1`: 타이틀 → 페이지들 → 캐릭터 → 5초 카운트다운 → 게임 → E 굴림 → Esc 일시정지 → M 전체 지도 → F6 승리): 탑뷰 섬과 3인칭 주사위 마을 모두 → `C:\work\char-pipeline\output\screens\ui_pastel\`(`top3_*`, `tps_*`; `top_*` 는 받침 전 헤일로판). 빌드 경고 없음(UFont::CompositeFont 직접 접근 경고 → `GetMutableInternalCompositeFont`, 지역 변수 `Tags` 가 AActor::Tags 를 가려 C4458 → `Going`).
- 확인 못 한 것: 해상도·DPI 가 다른 화면(캡처는 1280×720 창이 150% DPI 로 1920×1080), 온라인 참가자 화면(`WAITING FOR HOST` 버튼), DEFEAT 화면, 패키징 빌드에서 TTF 가 실제로 실려 가는지.

## 48차 — 지붕·나무류를 블록 대신 모델로 (사용자: 지붕이나 나무 같은 오브젝트까지 블록일 필요 없음, 필요하면 트라이포)

- 30~48차 커밋에 포함(브랜치 `claude/project-thread-ykg56g` → PR 로 main 에 머지). 꿈나라 패스(`bDream`, `-RRTownClassic` 이면 예전 그대로)에만 들어감.
- 방향: 벽·주사위 구조물(오르고 숨는 곳)은 블록 그대로. 장식은 모델로 — 나무류는 트라이포, 지붕·기둥·덤불은 크기·색을 바꿔 써야 해서 블렌더/엔진 도형. 충돌·봇 길은 계속 격자: 새 "숨은 룩"(`FLook::bUnderMesh`, `HiddenLook(색)`) = 격자에선 단단하지만 블록으로 안 그림(다른 메시가 그림), 미니맵엔 그 색. (멤버 이름 `bHidden`/`Solid`/`Puffs` 는 AActor·클래스 멤버와 겹쳐 C4458 — 피함.)
- 지붕: `C:\work\char-pipeline\scripts\blender_dream_roof.py <out>` → `output\dream_shapes\SM_BTRoof`(모임지붕, 바닥 100×100(처마 ±50, 기와 띠 ±55) 높이 100, 겹친 기와 띠 4단 + 처마 판·꼭대기 캡은 Accent; 볼록 껍질 → `dissolve_limit` 로 평면 합치고 모서리만 둥글게 — 그냥 매끈하게 하면 얼룩 음영) → `ue_import_dream_shapes.py <dir> SM_BTRoof`(이름 인자 추가). `AddBuilding` 의 계단식 지붕 칸은 숨은 룩(충돌 그대로), 그 위에 `Roofs` 인스턴스(가로 (W+2)/1.1, 세로 (D+2)/1.1, 높이 = 단 수, 몸 = 지붕 색, Accent = 지붕 색 + 흰 65%). 지붕·마시멜로·기둥 MID 는 `PixelStrength` 0.02, 거칠기 0.42.
- 트라이포(Claude in Chrome, studio.tripo3d.ai): 이미지(Nano Banana Pro, 1:1, 2장, 무료 — 프롬프트 여러 개를 연달아 넣으면 동시에 생성됨, 한 번은 클릭이 빠져 다시) → 왼쪽 그림마다 "Generate 3D" → HD Model(H3.1, 2K PBR, 2만, 30씩 × 5 = 150, 23,250 → 23,100). 둥근 나무(공 셋), 층층 나무(연두·민트 3단), 솜사탕 나무(분홍·라일락 + 스프링클, 줄무늬 기둥), 점박이 버섯(빨강 + 흰 점), 막대사탕(4색 소용돌이 + 풀 받침). 사용자 허락 후 FBX(Blender, 2K, 이름 `form_input` 으로 `dt_*`) → `C:\work\char-pipeline\input\dicetown_props\<이름>\`(각 2.3~3.1MB zip).
  - `blender_props.py`(`PROPS_TARGET_TRIS` 환경 변수 추가 — 3만으로 줄이지 않음; 21차 때 6000 감축이 음영을 망쳤음) `=100h` → `output\dicetown_props\SM_DT*`(높이 100, 피벗 바닥 가운데; 미리보기 렌더는 백그라운드 텍스처 오류로 검게 나옴 — 무시) → `ue_import_props.py`(`nanite` 옵션 추가, 머티리얼 `used_with_nanite`) `/Game/Environment/DiceTown/Props|SM_DTRoundTree=…=gloss=nanite|…` → `SM_DTRoundTree/TierTree/CandyTree/Mushroom/Lollipop` + `M_DT*`(사탕 광택).
  - `AddPropTree`(나무 Kind 0 작은 초록 → 둥근 430~520, 1 큰 초록 → 둥근/층층 620~740, 2 솜사탕 580~680, 3 버섯 460~540, 4 막대사탕 640~720; 막대사탕은 축 방향 ±12°, 나머지 아무 방향): 줄기만 숨은 룩(높이 × 0.3/0.15/0.4/0.45), 버섯은 + 자 기둥과 갓(반경 √5 원판 한 단). 잎은 충돌 없음(총알·시야 통과). 다섯 개가 다 있어야 쓰고 아니면 블록 나무. 하늘 섬 나무도 이 모델(꿈 섬: 솜사탕 6배 + 둥근 4배, 먼 섬: 층층/둥근 5.5배). 꿈나라면 나무 배치 시도 4배(숲이 듬성해서).
- 덤불(엄폐물 1번): 2칸×2칸 숨은 룩 + `Marshmallows`(SM_BTPuff) 큰 것 하나 + 작은 것 둘. 별 가로등 기둥: 숨은 룩 + 엔진 Cylinder 6띠(흰/분홍, 지름 38) + 받침 원판(`Poles`).
- 확인: 빌드 경고 없음. 로그 `dice town meshes: 24 roofs, 56 Tripo trees, 48 marshmallows`. 캡처 `output\screens\dicetown_dream\props_final\`(11시점), 비교 페이지 갱신(https://claude.ai/artifact/23Z95m1iY47DXo7MaDGGZY, "보완 후" = 이번). `-nullrhi` 봇전 170초: 넉아웃 15, 탈것, 오류 없음(한 번은 90초에 넉아웃 0 — 우연). `-RRTownClassic` 은 예전 수치(54705 blocks …) 그대로.
- 확인 못 한 것: 프레임(트라이포 2만 삼각형 × 56, 나나이트), 나무 잎에 충돌이 없어 생기는 손맛 차이, 지붕(매끈)과 숨은 계단 충돌의 어긋남(지붕 위에 서면 계단 모서리에 걸림 — 보통 못 올라감).

## 47차 — 주사위 마을 꿈나라 보완, 미리보기 후 적용 (사용자: 레퍼런스 보고 보완, 꿈나라 같은·주사위 강조 오브젝트, 게임에 올리기 전에 먼저 보여줘 → 적용해줘)

- 30~48차 커밋에 포함(브랜치 `claude/project-thread-ykg56g` → PR 로 main 에 머지). 처음엔 `-RRTownDream` 미리보기로만 보여 줌 → 사용자 "적용해줘" → **기본으로 켜짐**. `-RRTownClassic` 이면 예전 마을(같은 시드, 같은 배치) 그대로. 메시 셋(`SM_BTPuff/Star/Moon`)이 없으면 자동으로 예전 마을.
- 방향: 트라이포 대신 전부 블록으로 직접. 트라이포 메시는 매끈해 블록 마을과 안 어울리고, 충돌·봇 길이 격자에서 나오므로 격자에 칠해야 새 구조물이 저절로 막히고 봇이 오름. 새 메시는 셋뿐: `C:\work\char-pipeline\scripts\blender_dream_shapes.py <out>` → `output\dream_shapes\SM_BTPuff`(모서리를 크게 둥글린 마시멜로 블록, 윗면 Accent), `SM_BTStar`(두꺼운 오각별, +X 를 봄), `SM_BTMoon`(초승달) → `ue_import_dream_shapes.py <dir>`(새, 블록 머티리얼 재사용) → `/Game/Environment/DiceTown/`. (주의: `-script=` 경로에 역슬래시 `\u…` 가 있으면 파이썬이 이스케이프로 먹음 — 슬래시로.)
- 코드(`RRRandomDiceTown.cpp` 의 "The dreamier pass" 구역 + 헤더): `bDream` 일 때
  - 숲의 주사위 언덕 → `AddDiceMountain`(4블록 주사위 3×3 + 2×2(뒤쪽) + 1(뒤 구석), 계단 Rise 4 세 번 → 꼭대기 높이 12, 위에 도는 별). 프레임 `Sub(Island, -39, -66, 2)`. 2·3번째 계단은 층 앞 띠를 꽉 채워 첫 칸 앞에 디딜 곳이 없었음(봇 길도 1층에서 끊김) → 계단 앞에 작은 주사위 4×4 디딤대 둘(앞쪽 땅 위 U 3~6·V -1 높이 1~4, 1층 띠 위 U 0~3·V 3 높이 5~8), 둘째 계단은 U 4~5 로.
  - `AddDiceArch`: 7색 주사위 블록(`MakeLook(색, 흰, bDie)`) 반원 띠(반지름 11.5~4.5, 두께 2칸) — 큰길 X(기지 뒤 x -54/-55)와 Y(시내 밖 y -45/-44), 점대칭으로 넷. 길 위 빈 높이 4블록(거인 통과). 발치에 마시멜로 구름.
  - `AddDieHouse`: 12칸 정육면체(색 몸 + 밝은 모서리 + 흰 2×2 눈; 정면 3·오른쪽 2·뒤 4·왼쪽 5, 옥상 6은 한 칸 솟은 엄폐물), 문 앞·왼쪽(3×4), 평지붕 건물과 같은 바깥 계단 두 번 → 옥상. 안에 상자와 매달린 별. 팀마다 시내 1 + 바깥 1(배치 전에 예약 → 시내 일반 건물 6 → 5, 다른 배치도 바뀜). (처음엔 흰 몸 + 남색 눈이었는데 얼굴처럼 보여 색 몸 + 흰 눈으로.)
  - 나무: 3분의 1만 초록, 나머지 `AddDreamTree` — 2 솜사탕(줄무늬 기둥, 반지름 2.5 둥근 머리, 스프링클), 3 점박이 버섯(+자 기둥, 3단 갓, 윗면 흰 점, 반경 3), 4 막대사탕(소용돌이 원판).
  - `AddBuilding`: 흰 테두리 → 벽 색 파스텔, 흰 창틀 → 지붕 색 계열, 윗층 무늬(체크/세로줄/가로 띠) 무작위, 정문 위 줄무늬 차양(`Decor`, 충돌 없음, 높이 ~455), 뾰족 지붕 꼭대기 2칸 주사위, 평지붕 앞 왼쪽 구석 주사위 2 + 1, 1층 집 40% 는 지붕 대신 테두리 + 거대 주사위(3~5). 이 선택들은 `FRandomStream(Seed + 4701)` 로 — 상자 배치 난수는 그대로.
  - `AddStarLamps`(배치 끝): 큰길 양옆 6칸마다 주변이 비어 있으면 사탕 줄무늬 기둥 3 + 도는 별(팀마다 21).
  - `MakeDreamSky`: 섬 아래 -6500 에 연하늘 구름 바닥(어두운 남색 지면 대신), 팀 끝 밖 떠 있는 섬 2(분홍·라일락 밑면, 솜사탕 나무, 주사위) + 주사위 무지개, 먼 하늘 초승달 2(빛남 0.45) + 별, 광장 거대 주사위 위를 도는 주사위 6, 섬 위·바깥을 떠다니는 주사위 52·별 20. 구름 메시를 `SM_BTPuff` 로.
  - 떠다니는 것(`FFloater`: 흔들림·자전·구르기·공전)은 `Tick` → `AnimateFloaters`(전용 서버 제외, 인스턴스 변환 갱신 후 렌더 상태 한 번).
  - 새 컴포넌트: `Decor`(블록, 충돌 없음), `Stars`/`Moons`(빛나는 MID: `M_BTBlock` Glow 0.9/0.45), `FloatDice`.
- 확인: 빌드 경고 없음. 로그 `dice town 56829 blocks, 2880 dice, 604 steps, ... 2801 solid boxes, 74 stairs, 33917 walk nodes (27541 roamable)`, `dream pass: 272 decor, 82 stars, 2 moons, 58 floating dice, 140 floaters`. 화면 없이 봇전 ~90초(`-RRTownDream`): 에러 없음, 넉아웃·드래곤 비행·탈것 정상. 같은 시점 11곳 전후 캡처(스크래치 `town_views.py`/`townshots.sh` — 플레이어 시작점을 카메라로, `showhud` 끔): `C:\work\char-pipeline\output\screens\dicetown_dream\before|after\`, 비교 페이지 https://claude.ai/artifact/23Z95m1iY47DXo7MaDGGZY (전후 슬라이더).
- 적용 뒤 확인: 임시 `-RRTEMPDreamWalk`(플레이어 브롤러를 땅에서 `FindPath` 길 따라 걷게 — 지움): 주사위 산 꼭대기(1200), 주사위 집 옥상 두 채(1200) 모두 걸어서 도착(8~12초). 스위치 없이 시작해 꿈나라 마을로 뜨는 것 캡처 `output\screens\dicetown_dream\applied\`.
- 확인 못 한 것: 프레임(떠다니는 것 140개를 매 프레임 갱신), LAN 참가자 화면(같은 코드·시드라 같아야 함), 실제 마우스·키로 오르는 손맛.

## 46차 — 불씨 더 잘 보이게 + 지나간 자리를 그을린 바닥으로 (사용자: 불씨가 더 잘 보이게, 궁극기가 지나간 자리에 생기는 텍스처 어울리게)

- 30~48차 커밋에 포함(브랜치 `claude/project-thread-ykg56g` → PR 로 main 에 머지).
- 지나간 자리의 "텍스처" = 경고 홀로그램 띠(`WarnHolo`, `M_FXHolo` — 원래 호랑이 원기둥용이라 상자에 씌우면 줄무늬·동심원)가 비행 내내 선 전체를 덮고 있던 것, 그리고 45차의 납작한 붉은 바닥 빛. 이제:
  - `WarnHolo` 는 활강(Pass) 중 **브레스 앞쪽(아직 탈 곳)만** 덮음(`PlaceStrip(WarnHolo, OnLine(Front), LineEnd, …)`), 지나간 뒤쪽엔 없음.
  - 지나간 자리엔 **그을린 바닥** `Scorch`(새 컴포넌트: 엔진 큐브를 납작하게(두께 1) 선 전체 + 양끝 1.2R 더, 바닥 4 위) + `/Game/Effects/DiceFX/M_FXScorch`(새, `C:\work\char-pipeline\scripts\ue_make_scorch.py`): 로컬 위치로 선 위 위치(S)·옆(A)을 계산 → 앞머리(`Front`)까지만, 시작은 둥글게, 가장자리는 노이즈로 들쭉날쭉; 검갈색 그을음 + 주황 **빛나는 균열·불씨 점**(깜빡임, 막 지나간 곳일수록 밝고 3초에 걸쳐 식음), 지점마다 나이(`PassTime - S/FlySpeed`)가 `BurnSeconds` 되면 사라짐. 파라미터 `Length/Pad/Width/Radius/Front/PassTime/FlySpeed/BurnSeconds/Glow/EmberColor/CharColor`.
  - 머티리얼: **언릿 + 마스크(디더 커버리지, TAA 가 섞음)**. 시행착오: ①디퍼드 데칼 → 이 프로젝트에선 데칼이 색과 상관없이 검게 나옴(스키드 마크는 원래 검정이라 몰랐음), 월드 위치도 데칼에서 이상. ②반투명 언릿(기본 패스) → 툰 후처리가 어두운 덮개를 땅의 그림자 띠로 바꿈. ③반투명 After Motion Blur → 깊이 가림이 안 돼 나무 위에 그려짐. ④월드 위치 기준 → 사각형으로 꽉 참(원인 미확인) → 로컬 위치(`M_FXHoloArrow` 와 같은 방식)로.
- 불씨(`GroundEmbers`): 지름 26~42, 초당 26개(지점당), 주황·빨강 위주(노랑 12%, 밝기 2.4~3.4 — 더 밝거나 노라면 하얀 금가루처럼 바램), 작은 불꽃이 피어오름(초당 5), 불티 별 늘림. 45차의 납작한 바닥 빛은 뺌(그을음 머티리얼이 대신).
- 확인(엔진 안 HighResShot, 스크래치 `game_ult.py`): 섬(직후: 그을린 띠 + 빛나는 균열, 1~2초 뒤: 식어 가는 그을음 + 불씨), 주사위 마을 TPS(길 위 그을음·불씨, 건물·가운데 단상이 가림). 도트는 그대로(마을에서 2.8초까지). `C:\work\char-pipeline\output\screens\dragon_embers\scorch_*.png`.
- 시험용 `/Game/Scratch/M_DecalTest*` 는 만들었다가 스크래치로 옮김(콘텐츠에 없음).

## 45차 — 브레스 자리에 불씨 3초 + 도트 (사용자: 용 궁극기 브레스가 지나간 자리에 작은 불씨가 남고 3초 동안 도트 데미지)

- 30~48차 커밋에 포함(브랜치 `claude/project-thread-ykg56g` → PR 로 main 에 머지).
- 전: 지나간 자리가 `BurnSeconds` 2.5초 동안 큰 불길(`GroundFire`, 점점 약하게)로 타며 그 안의 적 0.25초마다 4.
- 지금(`ARRRandomDragonFlight`): 브레스가 지난 지점마다 `BlazeSeconds` 0.7초 동안만 불길, 0.35초부터 **작은 불씨**(`ARRRandomEffects::GroundEmbers` 새: 바닥에 흩어진 지름 20~34 작은 숯이 깜빡이듯 밝아졌다 어두워짐, 은은한 붉은 바닥 빛(납작한 공), 가끔 위로 떠오르는 불티)가 **`BurnSeconds` 3초**까지(끝으로 갈수록 줄어듦). 도트는 그대로 그 띠 안의 적에게 `BurnTick` 0.25초마다 `BurnDamage` 4(3초 = 최대 48), 이제 3초까지. 공 파티클 풀 1400 → 2200(선 7000·불씨가 많을 때).
- 끝 로그에 도트 횟수와 가장 늦은 도트가 브레스 지난 뒤 몇 초였는지(`BurnHits`, `LastBurnAge`): "the dragon flight is over: N hits, M burn ticks (the last X s after the breath went by)".
- 확인(PC 잠김 → `-RenderOffscreen` + `-ExecCmds="py …"`, 스크래치 `game_ult.py`: `RRGod`·`RRUltimate` 후 `Input.+key E`/`Input.-key E` 로 굴림, 적 하나를 선 위에 계속 옮겨 둠): 적이 브레스 지난 뒤 **2.9초까지** 도트를 받고 쓰러짐(봇 용 궁극기도 3.0초). 화면 `C:\work\char-pipeline\output\screens\dragon_embers\`(지난 띠가 주황 바닥 빛 + 작은 불씨 점).
- 참고: 처음 불씨(지름 8~16, 초당 14)는 궁극기 시점(멀리서 옆으로)에선 거의 안 보여서 키움.

## 44차 — 드래곤 브롤러를 Tripo 부터 새로: 처음부터 셔츠 없는 모델 (사용자: 기존 걸 고치지 말고 트라이포 단계부터 티셔츠 없는 모델링 새로 뽑아서 적용)

- 30~48차 커밋에 포함(브랜치 `claude/project-thread-ykg56g` → PR 로 main 에 머지; 코드 변경 없음, 에셋만). 43차의 텍스처 덧칠 방식은 대체됨.
- 그림: Tripo Image(Nano Banana Pro, 1:1, `input\dragon\concept\dragon_front.png` 참고)로 셔츠 없는 정면 → `input\dragon_v2\concept\dragon_front_noshirt_capture.png`. 프롬프트에 "no shirt / no clothes" 를 쓰면 가이드라인 오류가 나서 안 씀.
- 3D: Tripo HD Model(H3.1, 2K PBR, 20k, 30 크레딧, 23,280 → 23,250) → 19,346 삼각형. FBX(Blender, 2K) → `input\dragon_v2\tripo\`(베이스컬러 `…fbm\dragon_noshirt_basecolor.JPEG`) → `blender_tripo_to_obj.py` → `input\dragon_v2\mixamo_obj\dragon.obj`(9,677 정점).
- **Mixamo 오토리거가 이날 모든 메시에서 실패**("ERROR occured on rig: Unknown error while generating motion") — 날개·꼬리를 위치로 잘라낸 몸(`scripts\blender_strip_region.py` 새, 규칙 `input\dragon_v2\strip_rule.py`, 구멍 메움), 전체 메시, 그리고 **예전에 성공했던 옛 용 `input\dragon\dragon_mixamo.zip` 까지** 같은 오류(2 Chain Fingers·Standard 둘 다). Mixamo 의 남은 캐릭터는 그대로 `dragon_body_mixamo`(실패한 업로드는 안 바꿈).
- 대신 `scripts\blender_transfer_rig.py <옛 리그 fbx> <옛 obj> <새 obj> <out fbx> [미리보기]`(새): 새 Tripo 메시가 옛 것과 거의 같은 모양(같은 정면 그림에서 나옴)이라, 옛 최종 리그 `input\dragon\dragon_rig.fbx`(Mixamo 몸 41뼈 + 꼬리·날개 8뼈 = 49)를 그대로 쓰고 가중치만 옮김: 새 OBJ 를 옛 OBJ↔리그 메시 경계 매핑으로 리그 좌표에 넣고 이동만으로 옛 표면에 맞춤(높이 대비 거리 중앙 0.3%, 90% 1.1%), 정점마다 옛 표면의 가장 가까운 점(같은 쪽을 보는 면 중, 721개는 뒤쪽 면 대신 다음 면) 무게를 면 안에서 보간, 큰 4개 정규화 → `input\dragon_v2\dragon_rig.fbx`. 같은 뼈대라 **옛 Mixamo 애니메이션(`input\dragon\mixamo\`)과 게임 튜닝(`{Dragon, 2.15, 20.4}`)을 그대로 씀**. 블렌더 확인 `input\dragon_v2\check_transfer\`(걷기·사격·죽음 프레임, 시험 자세).
- 언리얼: 옛 `Content\Characters\Dragon` 은 스크래치 `backup_dragon_content\Dragon` 으로 옮겨 두고 `ue_import_animal.py Dragon|input/dragon/mixamo|<새 베이스컬러 JPEG>|input/dragon_v2/dragon_rig.fbx`(뼈 41 → 49, 다른 뼈 변화 0) → `ue_import_flyback.py input/dragon/mixamo/dragon_flyback.fbx|20.4|Dragon=20.4` → 얼굴: 리그 FBX 엔 애니메이션·텍스처가 없어서 스크래치 래퍼 `portrait\run_portrait2.py`(불러온 뒤 `dragon_SK.fbx` 의 소총 대기 자세 + 새 베이스컬러) → `output\ui\T_DragonPortrait.png` → `/Game/UI/T_DragonPortrait`.
- 확인(PC 가 잠겨 창 캡처가 안 돼서 `-RenderOffscreen` + `-ExecCmds="py <스크립트>"` 로 엔진 안 HighResShot; 앞모습은 PlayerStart 를 움직여 시점으로 씀): TPS 주사위 마을에서 앞·좌우 40°·뒤 모두 셔츠 없는 새 모델, 날개·꼬리 정상, 봇 얼굴 그림 새 것 → `C:\work\char-pipeline\output\screens\dragon_v2\`.
- Mixamo 가 다시 되면 정식 리그로 바꿀 수 있음: `input\dragon_v2\dragon_whole_mixamo.zip`(전체) 또는 `dragon_body_mixamo.zip`(날개·꼬리 뺀 몸) → 30차 방식(`blender_merge_parts.py` + `blender_animal_tail.py`).

## 43차 — 드래곤은 티셔츠 없이 (사용자: 그냥 용은 티셔츠 없는 버전으로)

- 30~48차 커밋에 포함(브랜치 `claude/project-thread-ykg56g` → PR 로 main 에 머지; 텍스처만). 41·42차의 등 셔츠 텍스처는 버림(스크립트 `blender_dragon_shirt_back.py` 는 남음).
- `C:\work\char-pipeline\scripts\blender_dragon_no_shirt.py <rig fbx> <베이스컬러> <png> [미리보기]`(새): Tripo 원본 베이스컬러에서, 게임 메시(`input\dragon\dragon_rig.fbx`)의 z 0.33~0.53(몸통·윗팔, 벨트 아래·얼굴 위 제외) 면 텍셀마다 색을 "빨간 비늘(위 등에서 딴 0.69, 0.06, 0.16) ↔ 셔츠 흰색(앞 소매에서 딴)" 선으로 나눠 셔츠 성분 t 와 선에서 벗어난 정도를 봄 — 선 가까운 색(흰색·주름 회색·분홍 경계)만 빨간 비늘로(주름 명암 0.75~1.1 유지), 선에서 0.1~0.2 넘게 벗어난 크림색 배·주황 날개막은 그대로. (처음엔 벗어남을 안 봐서 배와 날개막까지 빨개졌음.) 소매의 살짝 솟은 형상은 메시라 주름처럼 남음.
- 결과 `output\animals\T_Dragon_BaseColor_noshirt.png` → `T_Dragon_BaseColor` 다시 들여옴(스크래치 `ue_reimport_tex.py`). 미리보기 `output\animals\dragon_noshirt\p2_*.png`(`p2_sheet.png`).
- 얼굴 그림도: `blender_portrait.py input\dragon\mixamo\dragon_SK.fbx` 가 FBX 에 묶인 원래 텍스처를 써서 소매가 보였음 → 스크래치 래퍼(불러온 뒤 이미지를 새 베이스컬러로 바꿔 렌더) → `output\ui\T_DragonPortrait.png` → `ue_import_ui_texture.py /Game/UI|…=T_DragonPortrait`.
- 확인: 게임 TPS 용 뒷모습 빨간 비늘 + 날개(`C:\work\char-pipeline\output\screens\dragon_noshirt`), 얼굴 그림 셔츠 없음.

## 42차 — 등 티셔츠 다시: 날개는 셔츠를 뚫고 나온 것만 (사용자: 트라이포 원본부터 다시 작업해서 덮어쓰기, 날개에 흰색이 침범해서 더 이상해짐, 등 돌기는 무시하고 날개만 뚫고 나온 걸 표현)

- 30~48차 커밋에 포함(브랜치 `claude/project-thread-ykg56g` → PR 로 main 에 머지; 텍스처만).
- 41차 문제: 날개 뿌리 쪽 막·뼈가 희게 칠해짐 — 날개 정점 대부분은 날개 뼈 가중치 0.8 이상이지만 뿌리 쪽 막 일부는 30차에 날개를 고를 때 빠져 가중치 0이고, 0.15~0.45 로 서서히 빼던 경계가 면 안에서 보간돼 날개 쪽으로 번짐.
- `blender_dragon_shirt_back.py` 다시(인자 `<rig fbx> <베이스컬러> <png> [미리보기]`): Tripo 원본 베이스컬러(`input\dragon\tripo\…fbm\dragon_tripo_basecolor.JPEG`)에서 새로 시작, 게임 메시 `input\dragon\dragon_rig.fbx` 로 직접(위치·법선·날개 가중치 — 척추 x 는 허리 높이 몸통 폭 가운데 -0.06 으로 자동). 날개는 통째로 뺌: 꼭짓점 하나라도 날개 가중치 > 0.01 이면 그 면 전체, 또 척추에서 0.035 넘게 떨어진 곳 중 등면(y -0.058, 몸통 등은 가시 빼고 이보다 뒤로 안 나옴)보다 뒤로 튀어나온 곳. 등 가시는 빼지 않음(셔츠 아래 흰 돌기). 셔츠 영역·흰색·텍셀 한 번 칠하기는 41차 그대로. 미리보기에 게임 카메라처럼 뒤 위 25° 시점 추가.
- 결과 `output\animals\dragon_back\v5.png` → `output\animals\T_Dragon_BaseColor_shirt.png` 덮어씀 → `T_Dragon_BaseColor` 다시 들여옴. 미리보기 `output\animals\dragon_back\v5_*.png`(`v5_sheet.png`).
- 확인: 게임 TPS 용 뒷모습 — 날개 전체 원래 색, 셔츠는 날개 뿌리에서 끝남(`C:\work\char-pipeline\output\screens\dragon_shirt2`).

## 41차 — 드래곤 브롤러 등 티셔츠를 컨셉 뒷모습대로 (사용자: 인간폼 뒷모습 티셔츠가 애매, 컨셉 뒷모습 참고해서 보정)

- 30~48차 커밋에 포함(브랜치 `claude/project-thread-ykg56g` → PR 로 main 에 머지; 코드 변경 없음, 텍스처만).
- 문제: Tripo 가 정면 그림만으로 만들어 등은 허리 위 가는 흰 띠뿐, 그 위 등·어깨는 빨간 비늘, 띠 위에 빨간 얼룩. 컨셉(`input\dragon\concept\dragon_back.png`)은 목 아래부터 벨트 바로 위까지 흰 반팔 티, 날개가 셔츠에서 나오고 등 가시가 셔츠를 뚫고 나옴.
- `C:\work\char-pipeline\scripts\blender_dragon_shirt_back.py <tripo fbx> <rig fbx> <베이스컬러> <png> [미리보기]`(새, 호랑이 줄무늬 스크립트 방식): Tripo FBX(높이 1, 등 +Y, 척추 x 0)의 등 쪽 면 텍셀 중 셔츠 영역(z 0.356~0.505, 반폭 몸통 0.115 / 어깨 위 z ≥0.42 는 소매 포함 0.165)을 앞 소매에서 딴 흰색(60 백분위, 가장자리·밑단 쪽 5% 어둡게)으로. 빼는 곳: 등 가시(가운데 |x| <0.05 에서 등 곡면 y = -0.028 - 6x² 보다 0.007 넘게 튀어나온 곳), 날개(`input\dragon\dragon_rig.fbx` 의 WingL/R 가중치를 UV 로 찾아 면 안에서 보간, 0.15~0.45 사이로 서서히), 밑단 아래(원래 밑단과 벨트 위 빨간 띠·가시는 컨셉처럼 그대로). 법선은 정점 법선 보간. **텍셀마다 한 번만**(자기 면이 정한 값, 없으면 틈 메우기로 가장 큰 값) 정해서 한 번 섞음 — 면마다 섞으면 경계마다 흰색이 겹쳐 털처럼 들쭉날쭉했음.
- 결과 `C:\work\char-pipeline\output\animals\T_Dragon_BaseColor_shirt.png`(2048²) → 스크래치 `ue_reimport_tex.py <png>|/Game/Characters/Dragon/Textures/T_Dragon_BaseColor`(sRGB·압축·LOD 그룹 유지). 미리보기 `output\animals\dragon_back\after4_*.png`(앞·옆·3/4 뒤·뒤, `before_*` 와 비교).
- 확인: 게임 TPS 에서 용 뒷모습 — 흰 셔츠 등에 빨간 가시, 날개는 셔츠에서(`C:\work\char-pipeline\output\screens\dragon_shirt`).

## 40차 — 한 바퀴에서 그대로 이어서 비행, 중간 이펙트 없앰 (사용자: 빙글 돌고 나서 그대로 이어서 날아가는 모습, 용한테 중간에 이펙트가 보임)

- 30~48차 커밋에 포함(브랜치 `claude/project-thread-ykg56g` → PR 로 main 에 머지).
- 중간 이펙트 = 바퀴 끝에서 서 있는 용 → 고속 비행 용으로 바꾸며 터지던 불꽃(`TakeFlightForm`). 이제 처음부터 고속 비행 모델 하나(`FlightBody`)만: 서 있는 용 `Body`(SK_DragonClassic), `StandRig`, 서 있는 자세 코드, `TakeFlightForm`, `bFlightForm`, `GetRig` 없앰. 에셋 `/Game/Characters/DragonClassic` 은 남음(코드에서 안 씀). `PoseBody(Time, bBreathing)`.
- 이어지는 비행: 꺾어 들어가던 `Approach` 단계와 `SwoopTime` 을 없애고, 바퀴를 선 위 `LapExit`(시작점에서 `Lead` 뒤, `Height`) 에서 시작·끝나게: 중심은 그 옆(선의 왼쪽) 반지름 `Lead + CircleExtraRadius`, 반시계라 끝에서 진행 방향이 선 방향과 같음. 각도는 시간의 3차식(한 바퀴, 처음 1.5바퀴 속도로 빠르게 → 끝에서 `FlySpeed` 와 같은 속도), 솟음은 sin² 라 끝에서 수평 → 바퀴 끝에서 곧장 `Pass`(위치·방향·속도 연속). 브레스 시작 때 입의 불꽃 펑(FireBurst 0.8)도 뺌. 나타날 때의 불꽃은 그대로.
- 확인: 빌드 경고 없음. 창 + `-RRSpecies=Dragon -ExecCmds="RRUltimate"` + E(LV2): 나타남(불꽃) → 시작점 옆을 한 바퀴 → 그대로 선을 따라 브레스 → 떠남, 중간 불꽃 없음, 적 맞힘 정상. 0.2초 간격 캡처 `C:\work\char-pipeline\output\screens\dragon_lap2`(`sheet.png`).

## 39차 — 방향을 정하면 용이 나타나 한 바퀴 돌고 출발 (사용자: 방향을 설정하면 용이 나타나서 빠르게 주변을 한바퀴 돌고 출발)

- 30~48차 커밋에 포함(브랜치 `claude/project-thread-ykg56g` → PR 로 main 에 머지).
- `ARRRandomDragonFlight` 흐름: 시작 = 작은 용 자리 불꽃만(큰 용 안 보임, `Hover` 는 보이지 않게 대기 — `TakeOff` 단계와 `TakeOffTime` 없앰) → 화살표로 방향 고르기 → `MulticastFly` 에서 서 있는 용(Body)이 불꽃 속에 나타나 새 단계 `Circle`: 시작점 위(`Height`) 를 중심으로 반지름 `Lead + CircleExtraRadius`(600) 를 반시계로 한 바퀴, `CircleTime` 1.2 + `CirclePerScale` 0.05 × Scale 초(LV1 1.7초, LV6 2.2초), 등속, 중간에 300 솟음, 처음 0.35초 동안 0.3 → 1 크기로 자람. 끝점은 시작점 바로 뒤(선 반대 방향) → 거기서 고속 비행 용으로 바뀌며(불꽃) `Approach`(`SwoopTime` 1.1 → 0.7초, 나가는 접선 = 바퀴 끝 속도 × 0.6) → `Pass` 이후 그대로.
- 옆 카메라: 도는 동안 바퀴 중심을 보며 반지름 × 1.4 만큼 더 물러나고 14° 더 내려다봄(`ViewExtra`, 다이브 때 다시 들어옴).
- 확인: 빌드 경고 없음. 창 + `-RRSpecies=Dragon -ExecCmds="RRUltimate"` + E(Dice Land, LV1): 고르는 동안 용 없음 → 방향 확정(6초 자동) → 용이 나타나 시작점 둘레를 기울어 돌고 → 내려꽂아 브레스 → 떠남, 적 맞힘 정상. 연속 캡처 `C:\work\char-pipeline\output\screens\dragon_lap`(`sheet.png` 에 24장).

## 38차 — 용 궁극기 방향 고르기를 화살표로 (사용자: 방향 설정 이펙트가 방향을 지시하는 느낌이 나게)

- 30~48차 커밋에 포함(브랜치 `claude/project-thread-ykg56g` → PR 로 main 에 머지).
- 새 머티리얼 `C:\work\char-pipeline\scripts\ue_make_holo_arrow.py` → `/Game/Effects/DiceFX/M_FXHoloArrow`(엔진 큐브를 X 로 늘린 띠용; unlit 반투명, 양면, 깊이 무시 — M_FXHolo 와 같음). Custom HLSL `HoloArrow`: 로컬 위치로 띠 위 거리 S·폭 V 를 구해 ① 시작에서 끝으로 흐르는 "∧" 갈매기 무늬(`Speed` 900/초, 간격 `Spacing` 260 과 폭 ×1.3 중 큰 쪽, 끝으로 갈수록 밝게, 시작은 서서히) ② 양옆 레일 ③ 마지막 구간(폭 ×1.8, 길이의 40% 이하)은 끝이 뾰족한 화살촉. 게임이 `Length`(띠 길이 = 선 + 양끝 브레스 반경)·`Radius`(브레스 폭) 를 넘김.
- `ARRRandomDragonFlight`: 고르는 띠(`PathHolo`)만 이 머티리얼(색 1, 0.32, 0.02, Glow 1.5, Opacity 0.9±0.1 깜빡임). 비행 중 남에게 보이는 경고 띠(`WarnHolo`)는 그대로 M_FXHolo.
- 확인: 빌드 경고 없음. 창 + `-RRSpecies=Dragon -ExecCmds="RRUltimate"` + E 로 Dice Land·Dice Town 에서 고르는 화면 — 용 자리에서 맵 끝으로 주황 화살표가 흐르고 끝이 뾰족(`C:\work\char-pipeline\output\screens\dragon_arrow3`, `dragon_arrow4`). 처음엔 밝은 바닥에서 너무 흐려서(`dragon_arrow2`) 갈매기를 굵게·진하게, 색을 더 주황으로.

## 37차 — 궁극기 용 2배 (사용자: 궁극기 용이 지금의 두 배는 되어야, 주사위 숫자에 따라 용·브레스 폭이 커지는지 확인하고 아니면 커지게)

- 30~48차 커밋에 포함(브랜치 `claude/project-thread-ykg56g` → PR 로 main 에 머지).
- 확인: 이미 주사위 숫자로 커지고 있었음 — 크기 `BaseScale + ScalePerFace × 숫자`(4 + 숫자: 5~10), 브레스 좌우 폭 `140 + 55 × 숫자`(195~470, 불꽃 입자 끝 크기도 폭 / 50 에 비례, 바닥 불·경고 띠도 같은 폭), 나는 높이·옆 카메라 거리도 크기에 비례.
- 바꿈: `ARRRandomDragonFlight::BaseScale` 4 → 8, `ScalePerFace` 1 → 2(10~20 배, 모든 숫자에서 두 배). 브레스 폭(피해 범위)은 그대로.
- 확인: 빌드 경고 없음. 창 + `-RRSpecies=Dragon -ExecCmds="RRUltimate"` + E: 로그 `level 6: scale 20.0, breath 470 wide each side, 1800 up`, 비행·브레스 피해·넉아웃 정상(`C:\work\char-pipeline\output\screens\dragon_x2`).

## 36차 — TPS 큰 맵 "주사위 마을"(Dice Town) + 맵 고르기 (사용자: 첫 번째 스타일로 시작, TPS 는 맵 고르는 UI, 2층 건물엔 올라갈 길 필수, 창문은 모든 캐릭터가 뛰어내릴 만큼 크게)

- 30~48차 커밋에 포함(브랜치 `claude/project-thread-ykg56g` → PR 로 main 에 머지).
- 컨셉: Tripo 웹 이미지(Nano Banana Pro, 16:9) 3장 — 전경/마을 TPS 시점/주사위 성. 화면 캡처만 `C:\work\char-pipeline\output\dice_land_v2_concept\01_overview.png`(이걸로 제작), `02_town_tps.png`, `03_dice_castle.png`. 원본은 Tripo 기록에만.
- 블록 메시: `C:\work\char-pipeline\scripts\blender_block_town.py <out>`(Blender 5.2) → `output\block_town\SM_BTBlock`(100 블록, 모서리 베벨 4.5, 슬롯 Body = 옆·바닥, Accent = 윗면 + 옆면 위 18% 띠 — 잔디 블록 테두리), `SM_BTDieBlock`(같은 블록 + 눈, Accent = 눈) + 미리보기. `ue_import_block_town.py <dir>` → `/Game/Environment/DiceTown/`: `M_BTBlock`(커스텀 데이터 0~2 = 몸 색, "Accent" 1 이면 3~5; 면마다 12.5 단위 8×8 "픽셀" 밝기 흔들림 — Custom HLSL `BlockPixels`, `PixelSize`/`PixelStrength` 0.1; 거칠기 0.6, `Glow`, `Saturation`/`Brightness`), `MI_BTAccent`, `MI_BTCloud`, `MI_BTWater`. 나나이트, 충돌 없음. (UV 가 없어 FBX 탄젠트 경고가 뜨지만 노멀맵을 안 써서 무관.)
- `ARRRandomDiceTown`(새, 복제, 같은 시드로 모든 기기가 같게): 100 블록 격자(셀 ±88 = 17,600 정사각, 레벨 -3~30; 레벨 L 은 (L-1)×100~L×100, 레벨 0 윗면 = 바닥) 에 룩(몸/액센트 색, 최대 254) 번호를 칠하고 → 보이는 면이 있는 블록만 인스턴스(색 ±5% 흔들림) → 충돌은 격자에서 욕심쟁이 상자 병합(X→Y→위로, 숨은 큐브 BlockAll ≈ 2,800개). 255 = 단단하지만 큰 주사위가 대신 그림.
  - 점대칭(파랑 -X, 빨강 +X): `FFrame`(원점 셀 + 4분의 1 회전, `Mirror()` 는 가운데 기준 반 바퀴 + `bRed`), `Both()`. 무작위 배치는 `IsFree`(섬 안 가장자리 2칸, 길·광장 밖, 예약 사각형과 그 거울과 안 겹침)/`Reserve`.
  - 지역: 광장(체비셰프 <13, 분홍/크림 체크, 2단 받침대 + 6블록 거대 주사위, 네 방향 경사) / 큰길 X·Y(폭 6, |좌표| <56) + 순환로(체비셰프 46~52) / 시내(≤42): 2층 집(경사 지붕)·상가(평지붕+난간) 6쌍 / 각 팀 기지 마당(X -45~-33, 팀 색, 낮은 벽, 상자, 앞 모서리 감시탑 2) + 큰길 입구 문 2(기둥·보, 차 지나감) / 기지 뒤 3단 언덕 + 꼭대기 감시탑 / 팀 오른쪽 운하 구역(Y 72~75, 가장자리에서 폭포, 아치 다리 3) + 집 / 팀 왼쪽 숲(나무 55쌍) + 거대 주사위 언덕(3단, 첫 단 계단) / 바깥 오두막(1층) 8쌍, 감시탑 5쌍, 엄폐물(블록 벽·덤불·상자·주사위) 약 50쌍. 섬 아래 큰 블록 원뿔(흙→복숭아→분홍→라일락), 블록 구름 바다, 픽셀 무지개 2, 떠 있는 작은 섬 4.
  - 건물(`AddBuilding`): 층 = 6블록(바닥 1 + 방 5 = 500, 거인 384). 문(앞·왼쪽)·창문 모두 3×4 블록(300×400, 거인 168×384 가 지나감), 창턱 1블록. 2층이면 오른쪽 바깥 계단(폭 2) → 2층 문 앞 층계참(바깥 난간); 평지붕이면 층계참을 뒤까지 늘이고 뒤쪽 벽 위로 두 번째 계단 → 옥상(난간 틈). 1층 건물은 모두 경사 지붕(올라갈 2층이 없음). 층마다 상자 몇 개. 창·문 둘레는 틀 색.
  - 계단(`AddStairs`): 반 블록 깊이·높이 45° 단(보이는 건 단마다 늘린 블록); 막는 건 단 앞모서리를 지나는 45° 판 + 칸마다 아래 상자 + 마지막 반 칸. 이 맵에선 Tick 이 모든 캐릭터에 `SetWalkableFloorAngle(46)`(기본 44.77 이면 45° 못 걸음)과 `SetFlightMaxHeightInBodies(9)`(새 세터, 옥상까지 날기).
  - 봇 길(`FindPath`): 칸마다 서 있을 곳(위로 2블록 비어 있음; 계단 칸은 계단 위 한 곳), 이웃 링크 = 같은 높이(대각선은 모서리 안 자름) / 계단 진행 방향 ±100 / 한 블록 오르기(점프로 기어오름, 비용 3배) / 650 까지 떨어지기. A*(최대 8만 확장, 못 가면 가장 가까운 곳) 후 평지에선 스윕으로 곧게. `GetRandomWalkSpot`: 광장에서 닿는 곳 중 무작위.
  - 미니맵 `GetMapColor`(칸 맨 위 색·높이), 드래곤 비행 `GetMapBounds`.
- 게임 모드: `InitGame` 에서 타이틀 선택이 TPS + 주사위 마을이면 `bDiceTown`, `TeamSpawnDistance` 8000(로컬 플레이어는 StartPlay 전 로그인 때 놓이므로 여기서). `StartPlay` 맨 앞에서 마을 생성. 탈것 자리는 그대로(기지 마당·Y 길 위).
- 타이틀: THIRD PERSON 다음 "PICK A MAP" 페이지(DICE TOWN / DICE LAND / BACK, 마지막 선택이 선택된 채). 캐릭터 페이지 BACK/Esc → 맵 페이지 → 보기 페이지. `URRRandomSessionSubsystem::Set/GetThirdPersonMap`(`ERRThirdPersonMap`, 기본 DiceTown), 명령줄 `-RRMap=DiceLand|DiceTown`. 참가자는 호스트 맵(복제 액터).
- 캐릭터: `UpdateLedgeWalking` 이 바닥이 주사위 마을이면 어디서나 가장자리에서 걸어 내려감(계단 옆·층계참·옥상·창턱; 섬 가장자리는 숨은 벽). `TryClimb` 가 가슴 높이에 벽이 없으면 무릎 높이(발+75)도 봄 — 거인의 가슴은 창턱 위로 지나가서 창을 못 넘었음.
- 봇: 마을이 있으면 가까운 교전(보이고 선호 거리 안)이 아닐 때 `FollowRoute`(1.5초마다·목표가 300 움직이면 새 길, 모서리 60 안이면 다음, 0.5초 멈추면 점프)로 이동하고 장애물 회피 스윕은 안 씀(계단 판을 벽으로 봄). 순찰은 반경 5500 안 걸을 수 있는 아무 곳(옥상·2층 포함), 25초.
- 확인: 빌드 경고 없음. 로그 `dice town 54705 blocks, 460 dice, 592 steps, ... 2843 solid boxes, 72 stairs, 33747 walk nodes (28253 roamable)`. `-nullrhi` 봇전 끝까지(10킬 종료), 봇이 높이 314·572 까지 올라감, 기지끼리 8000 을 길 따라 이동. 임시 `-RRTEMPWindow`/`-RRTEMPWindowGiant`(2층 앞 창 안쪽에 세우고 바깥으로 걷기 + 점프 — 지움): 보통·거인 모두 바닥 605 → 창턱 702 → 떨어져 지면 2. 타이틀을 실제 키로(Enter, S, Enter → 맵 페이지, Esc 로 돌아오기, DICE TOWN → FOX → 시작). Dice Land TPS·탑뷰 섬도 그대로. 캡처 `C:\work\char-pipeline\output\screens\dicetown1~2`, `dicetown_window2`, `dicetown_title`(전경은 임시 `-RRTEMPCam` — 지움).
- 확인 못 한 것: 실제 마우스로 계단 오르기 손맛, LAN 참가자 화면(같은 시드라 같아야 함), 프레임(블록 5.5만 + 계단 등, 나나이트), 봇이 창문을 쓰지 않음(길에 창 링크 없음), 밀폐된 경사 지붕 속 다락에도 길 점이 생김(광장에서 안 닿아 순찰엔 안 쓰임).

## 35차 — 고속 비행 모델을 드래곤 비행에 (사용자: 바로 진행시켜)

- 30~48차 커밋에 포함(브랜치 `claude/project-thread-ykg56g` → PR 로 main 에 머지).
- 리그: `blender_creature_base.py` → `input\dragon_flight\rig_chains.sh`(`blender_animal_tail.py` 4번: Neck1~3(머리 포함, 반경 0.16), Tail1~4(뒷다리 뒤부터, 0.08), WingL/R 1~2(가운데 선, 0.20, 몸 비율 1.0, 연결 모드). 다리는 접힌 채라 뼈 없음) → `dragon_flight_rig.fbx`(12뼈) → `ue_import_creature.py DragonFlight|DragonFlight|…` → `/Game/Characters/DragonFlight/SK_DragonFlight`, `M_DragonFlight`. 부위 칠 `output\animals\dragon_flight\parts_sheet.png`.
- 코드(`ARRRandomDragonFlight`): 메시 둘 — `Body`(서 있는 고전 용: 이륙·맴돌기), `FlightBody`(고속 비행: 활강부터). 뼈 다루기를 `FRig`(메시, 뼈, 부모, 참조 회전, 지금 회전, 주둥이, 엉덩이 높이)로 묶고 `SetupRig`/`ApplyPose`/`GetRig()`. 클릭으로 선이 정해지면(`MulticastFly`) `TakeFlightForm`: 서 있는 용을 숨기고 비행 용을 보이며 불꽃 펑(크기 Scale×0.3). 비행 용 자세: 날개는 젖힌 기본 자세에서 빠르고 얕게(8±24°, 바깥 4±12° 늦게), 목 살짝, 브레스 때 머리 24° 숙임, 꼬리는 좌우·위아래 잔물결. 불이 닿는 앞거리 `Lead` 는 비행 용 주둥이(0, 54, 21) 기준.
- 확인: 빌드 경고 없음, 스마트 앱 컨트롤 막힘 없음. 창 + 실제 키·클릭(E 키가 한 번 안 들어가 여러 번 누르게 다시): 맴돌기(서 있는 용) → 클릭 → 불꽃 속에 비행 용으로 → 옆 시점 돌진·브레스 → 맵 끝 너머로(`output\screens\dragon_flightform`, LV4).

## 34차 — 고전 용 고속 비행 자세 모델 (사용자: 용 디자인 자체는 바꾸지 말고 자세를 조금 더 고속으로 나는 모습으로 모델링을 새로)

- 30~48차 커밋에 포함(브랜치 `claude/project-thread-ykg56g` → PR 로 main 에 머지; 게임 프로젝트 파일 변경 없음 — 아직 게임에 넣지 않음).
- 그림(Tripo Image, Nano Banana Pro, 무료): 31차 고전 용 옆·정면 그림을 참고로 "디자인 전부 그대로, 자세만 제트기처럼 고속 비행 — 몸 수평으로 길게, 목·머리 앞으로 일직선, 입 다묾, 날개 펼쳐 뒤로 젖힘, 앞다리 가슴에 접고 뒷다리 꼬리 쪽으로 뻗음, 꼬리 곧게" 옆모습(왼쪽 프로필) 2장 중 고름 → 그 그림을 참고로 정면·뒤 각 2장. 정면·뒤는 살짝 위에서 본 느낌.
  - 결과 `C:\work\char-pipeline\input\dragon_flight\concept\dragon_flight_side|front|back.png`, 모음 `dragon_flight_concept_sheet.png`.
- 3D: Tripo HD 멀티뷰(Front = 정면, Left = 옆, Back = 뒤; H3.1, 2K, 20k, 30 크레딧 → 23,280 남음) → 19,030 삼각형. FBX(Blender, 2K, 이름은 `form_input` 으로) → `input\dragon_flight\tripo\`(크기 비 가로 0.92 × 길이 1.0 × 높이 0.41). 확인 렌더 `output\animals\dragon_flight\model_sheet.png`(앞/뒤/옆/위/3/4): 수평 몸, 뒤로 젖힌 날개, 접은 다리, 곧은 꼬리.
- 다음(요청 시): 리그(날개 2마디씩, 꼬리, 목 — `blender_creature_base.py` + `blender_animal_tail.py`, 다리는 고정이라 뼈 불필요) → `ue_import_creature.py` → `ARRRandomDragonFlight` 의 활강·비행·떠나기 단계에서 이 모델로(이륙·맴돌기는 지금 모델 그대로 두고 바꿀 때 불꽃으로 가리거나, 전부 이 모델로).

## 33차 — 드래곤 비행: 캐릭터 자리부터 맵 끝까지, 더 빠르고 날렵하게 (사용자: 용이 조금 더 날렵하고 빠르게, 범위는 캐릭터 위치부터 쭉 브레스를 발사하면서, 주사위로 좌우 브레스 넓이)

- 30~48차 커밋에 포함(브랜치 `claude/project-thread-ykg56g` → PR 로 main 에 머지).
- 선: 클릭 두 번(시작·끝) → **한 번(방향)**. 불길은 작은 용이 서 있던 자리(`StartFeet`)부터 그 방향으로 **맵 끝까지**(`GetLineLength`: 섬/주사위 랜드 `GetMapBounds` 사각형을 나가는 거리, 1500~7000). 고르는 동안 미리보기 띠도 자기 자리에서 커서 방향으로 맵 끝까지(브레스 폭 그대로). 상태 줄 `[CLICK THE WAY TO BREATHE FIRE]`. 봇: 적이 가장 많은 곳 쪽(기반 `PickTarget`), 플레이어가 6초 안에 안 고르면 바라보던 쪽. `MulticastStart`·`bHasStart`·봇 끝점 기억 없앰.
- 주사위: 브레스 좌우 폭(반지름 140 + 55×눈 = 195~470)과 용 크기(5~10) 그대로.
- 빠르게: `FlySpeed` 1500 → 2800(맵을 1.5~2초에 가로지름), 이륙 1.1 → 0.8초, 떠나기 1.8 → 1.2초, 활강 `SwoopTime` 1.1초 고정.
- 날렵하게: 활강은 머리 위에서 뒤·옆으로 크게 돌아(Hermite 시작 접선 = 뒤 0.6 + 옆 0.9 + 위) 선 뒤에서 다이빙해 들어옴(뱅크). 활강부터 `Dash` 0→1(부드럽게): 날갯짓 1.6배 빠르고 얕게(22±42° → 40±18°), 날개를 뒤로 28° 젖힘(위 축 회전: 왼날개 끝 +X → -Y), 목 12° 더 앞으로, 다리 10° 더 접음, 꼬리 거의 곧게. 날갯짓 위상은 `FlapPhase` 로 누적(속도를 바꿀 때 위상이 튀지 않게).
- 브레스 불덩이는 앞머리가 움직이는 만큼 앞을 겨눔(`DragonBreath` 의 `Carry`), 따라가는 시점은 더 빨리 붙음(보간 6 → 9, 처음 돌아가는 시간 1.2 → 0.8초).
- 확인: 빌드 경고 없음. 창 + 실제 키·클릭 한 번(첫 시도는 E 키가 안 들어가 다시): 미리보기 띠가 자기 자리→맵 끝, 옆 시점으로 다이빙 후 자기 자리부터 불, 맵 끝 너머로 날아감(`output\screens\dragon_dash`, LV1). 화면 없이 임시 `-RRTEMPDragonTest`(지움): 모든 선이 각 용의 자리에서 시작해 맵 끝(±3240)까지, 봇 선이 적 쪽, 맞힘·피해 정상.
- 확인 못 한 것: 높은 레벨에서의 화면(폭은 32차 그대로라 수치만), LAN 에서 새 흐름(32차와 같은 멀티캐스트 하나라 로그 확인은 생략), TPS 에서의 고르기.
- 그 뒤 "실행이 안 된다": Windows 스마트 앱 컨트롤(코드 무결성 이벤트 3077/3033, 정책 {0283ac0f-…})이 새로 빌드한 `UnrealEditor-RRRandom.dll` 로드를 막음(로그 `GetLastError=4551` → "The game module 'RRRandom' could not be loaded"). 파일 시간만 바꿔 다시 링크하면 같은 바이너리라 계속 막힘 → 로그 문구 하나를 바꿔(드래곤 변신 로그 "breath %.0f wide each side") 다시 빌드하니 통과. 확인: 화면 없이 솔로 시작, 브롤러 6명 생성.

## 32차 — 드래곤 브롤러 + 궁극기 "드래곤 비행" (사용자: 드래곤 캐릭터 추가, 궁극기를 쓰면 두 번째 용이 하늘을 날며 지정한 경로에 브레스를 뿜으면서 맵을 가로질러 날아가게, 브레스 굵기와 용 크기는 주사위 숫자)

- 30~48차 커밋에 포함(브랜치 `claude/project-thread-ykg56g` → PR 로 main 에 머지).
- 브롤러(30차 리그): `ue_import_animal.py Dragon|input/dragon/mixamo|<베이스컬러>|input/dragon/dragon_rig.fbx` → `/Game/Characters/Dragon`(뼈 41 → 49: Tail1~4, WingL/R1~2, 다른 뼈 변화 0), `ue_import_flyback.py dragon_flyback.fbx|20.4|Dragon=20.4`(자기 리그라 그대로), 얼굴 `blender_portrait.py mixamo/dragon_SK.fbx` → `/Game/UI/T_DragonPortrait`. 코드: `RRSpecies::Dragon = 4`, `Count = 5`, 튜닝 `{Dragon, 2.15, 20.4}`(여우 엉덩이 맞춤 2.43과 머리 맞춤 1.8 사이, 뿔이 높아 엉덩이 쪽으로), 꼬리 흔들기는 Tail1~4 그대로(날개 뼈는 브롤러에선 안 움직임). 타이틀 `ERRTitleChoice::Dragon`("Little red dragon. Its true form breathes fire."), 봇 랜덤 5종.
- 고전 용(31차 모델) 리그 — Mixamo 는 네 발이 안 되므로 블렌더에서: `scripts\blender_creature_base.py <tripo fbx> <out> <grid>`(새: 메시 합치고 발 바닥 0·가운데, 뼈 하나 Hips 에 전부, 옆/뒤/위 10cm 격자 렌더) → `input\dragon_classic\rig_chains.sh`(`blender_animal_tail.py` 8번: Neck1~3(머리 포함), Tail1~4, LegFL/FR/BL/BR 1~2, WingL/R 1~2(몸 비율 1.0, 연결 모드 — 마지막에)) → `dragon_classic_rig.fbx`(20뼈). 블렌더 자세 시험(날개 위/중간/아래, 다리 접기, 목 앞으로): 목을 45° 숙이면 머리가 늘어나서 게임에선 15°만. → `scripts\ue_import_creature.py`(새) → `/Game/Characters/DragonClassic/SK_DragonClassic`, `M_DragonClassic`(평평, "Glow" 발광 파라미터). (처음엔 머티리얼 슬롯이 비어 회색으로 나옴 — 읽어 온 항목을 고쳐 넣는 방식이 이 메시에선 안 먹어서 `unreal.SkeletalMaterial` 새로 만들어 넣게 고침.)
- `ARRRandomDragonFlight`(새, `RRRandomDragonFlight.h/.cpp`, 궁극기 기반 위):
  - 흐름: 작은 용 자리에서 불꽃 펑(`FireBurst`) → 고전 용이 커지며 떠오름(TakeOffTime 1.1) → 머리 위에서 날갯짓하며 맴돎(Hover) → 클릭 두 번(`Left` = 2: 불 시작점, 끝점; 상태 줄 `[CLICK WHERE THE FIRE STARTS/ENDS]`) → 선으로 부드럽게 활강(Hermite, `Approach`) → 선을 따라 `FlySpeed` 1500 으로 날며 브레스(`Pass`, 불이 닿는 곳은 용보다 `Lead` = 주둥이 + 높이×0.55 앞) → 계속 날아 올라가며 작아져 불꽃으로 사라짐(`Leave` 1.8 s) → 땅의 불이 꺼지면 끝, 작은 용이 그 자리에 다시 펑.
  - 주사위 눈(1~6): 크기 `Scale` = 4 + 1×눈(5~10, 모델 1m → 날개폭 5~10m), 브레스 반지름 140 + 55×눈(195~470), 높이 400 + 70×Scale.
  - 선: 두 번째 클릭은 첫 클릭에서 700~3800 사이로 맞춤. 첫 클릭 뒤엔 시작점 원(경고 홀로그램)과, 이 기기 플레이어에게 시작점→커서 띠(`PathHolo`, 엔진 큐브 + `M_FXHolo`, 높이 70); 날기 시작하면 모든 기기에 경고 띠(`WarnHolo`, 아주 옅게 — 처음엔 밝아서 용과 불을 가렸음). `ShowCursorSpot` 을 기반 클래스에서 virtual 로.
  - 피해(서버): 브레스 앞머리가 닿은 적 25 + 선 바깥으로 450/320 튕김(한 번), 지나간 자리는 BurnSeconds 2.5 동안 불타며 그 안의 적 0.25초마다 4. 자기 궁극기에 붙잡힌(숨은) 브롤러는 제외(처음엔 변신 중인 드래곤도 '맞음'으로 셌음). 너무 높이 나는 적 제외.
  - 봇: 적이 가장 많은 곳(기반 `PickTarget`)을 지나는 선, 용→무리 방향, 길이 ≥ 1400 + 2×반지름. 플레이어가 6초 안에 안 고르면 앞쪽(첫 점) / 첫 점에서 용이 보는 쪽 1600(끝점).
  - 몸: `UPoseableMeshComponent`, 매 프레임 코드로 — 날개 위아래(1.6 회/s, 바깥쪽이 늦게), 다리 뒤로 접기, 목 15° 앞, 브레스 때 머리 32° 숙임, 꼬리 살랑. 각 뼈의 회전은 참조 자세 축 기준(부모 회전에 실려 감, FBX 가 Y 를 뒤집어서 X·Z 축 회전은 블렌더와 부호 반대). 참조 자세는 컴포넌트가 아니라 에셋의 RefSkeleton 에서. 몸 기울기: 진행 방향, 선회 땐 뱅크(최대 35°), 오르내림 피치.
  - 시점: 고르는 동안은 기반 클래스의 높은 시점, 활강부터는 선 오른쪽에서 비스듬히(피치 -28, 거리 2600 + 220×Scale) 용을 따라가며 왼쪽→오른쪽으로 날아가게, 떠날 땐 선 끝, 그 뒤 불타는 선 가운데.
  - 모든 기기: `MulticastStart`(첫 점), `MulticastFly`(선) 뒤 각자 시계로 같은 비행·불.
- 이펙트(`ARRRandomEffects`): `DragonBreath`(입 → 착지점 불덩이 줄기, 착지 튀김, 불티), `GroundFire`(불길, 세기 따라 줄어듦), `FireBurst`(변신 펑). 공 풀 500 → 1400. 불은 Glow 1.6~2.4, 주황·빨강 위주(밝으면 하얗게 날아감).
- 덤: `RRRandomMoonHop` 의 애니메이션 찾기가 `FObjectFinderOptional(*FString::Printf(...))` — 임시 문자열이 사라진 뒤 경로를 읽어 "Failed to find <깨진 글자>" 오류(토끼 달토끼 애니메이션이 못 잡힐 수 있었음) → 문자열을 지역 변수로.
- 확인: 빌드 경고 없음. 임시 `-RRTEMPDragonTest`(봇 전부 드래곤 + 모두 궁극기 주사위 자동) — 지움. 화면 없이: 6마리 모두 비행, 브레스·불 피해, 넉아웃, 봇이 무리를 지나는 선. 창: 고르기 원/띠, 이륙, 옆 시점 비행과 브레스, 불타는 선(`output\screens\ult_dragon3`), **실제 마우스 클릭 두 번**으로 LV6 비행 성공(`dragon_click`), 타이틀 DRAGON 버튼과 드래곤으로 시작(`dragon_title`). LAN 호스트+참가자: 참가자 로그에 같은 선·같은 레벨 비행, 참가자 브롤러 여우 → 드래곤 전환, 에러 없음. 일반 솔로 1분 에러 없음.
- 확인 못 한 것: 3인칭(TPS) 시점에서의 고르기·비행 시점, 참가자 화면에서 본 모습(로그만), 여러 마리가 동시에 불을 뿜을 때 파티클 풀(1400) 부족 여부, 비행 중 다른 탈것/거인 등과 겹칠 때, 고전 용 날갯짓을 가까이서 본 모습(멀리서만 봄), 작은 드래곤 브롤러의 날개(지금 고정).

## 31차 — 궁극기용 고전 드래곤(네 발) 컨셉아트 + 3D 모델 (사용자: 사람 폼이 아니라 고전적인 드래곤 모습으로, 궁극기에 쓸, 아트 느낌은 유지하면서 멋진)

- 30~48차 커밋에 포함(브랜치 `claude/project-thread-ykg56g` → PR 로 main 에 머지; 게임 프로젝트 파일 변경 없음 — 전부 `char-pipeline` 쪽).
- 그림(Tripo Image, Nano Banana Pro, 무료): 30차 드래곤 정면 그림을 참고로 "그 드래곤의 진짜 모습 — 네 발로 선 고전 서양 드래곤, 같은 뿔·볏·금색 눈, 크게 펼친 박쥐 날개(주황 막), 화살촉 꼬리, 진홍 비늘 + 크림색 배 비늘판, 같은 3D 애니메이션 영화 느낌이지만 위엄 있게". 고른 정면을 참고로 뒤·옆(왼쪽, 그림 왼쪽을 봄)·위를 뽑음(각 2장 중 고름; 정면은 날개 끝이 잘려 "줌 아웃" 넣어 한 번 더).
  - 주의: 프롬프트에 "no clothes" 가 있으면 Tripo 가 "This image does not meet our content guidelines" 로 거절함(그림 탓이 아님 — 그림 없이도 같은 오류). "with no outfit and no accessories" 로 바꾸니 됨.
  - 결과 `C:\work\char-pipeline\input\dragon_classic\concept\dragon_classic_front|back|side|top.png`, 모음 `dragon_classic_concept_sheet.png`.
- 3D: Tripo HD Model **멀티뷰**(앞 = 정면, Left = 옆 그림, Back = 뒤, Right 비움; H3.1, 2K PBR, 20k, 30 크레딧 → 23,310 남음) → 19,810 삼각형, 날개·꼬리·등 가시까지 그림과 맞음. FBX(Blender, 2K) `input\dragon_classic\tripo\`(크기 비 가로 1.0 × 길이 0.96 × 높이 0.85, 날개 폭 기준).
  - Export 창의 파일 이름 칸에 좌표로 클릭해 타이핑했더니 글자가 단축키로 들어가 뷰어가 다른 에셋(토끼·다람쥐)으로 넘어감(모델 손상 없음) — 이름은 `form_input` 으로 넣을 것.
- 확인 렌더: `scripts\blender_turnaround.py`(새, 앞/뒤/옆/위/3/4) → `output\animals\dragon_classic\model_sheet.png`.
- 아직: 리깅(네 발 + 날개 — Mixamo 는 두 발 사람형만 되므로 블렌더에서 직접 뼈를 만들거나 Tripo Rig 를 써야 함), 언리얼 들여오기, 궁극기 코드.

## 30차 — 붉은 드래곤 컨셉아트 4각도 (사용자: 서양 붉은 드래곤을 형상화해서, 트라이포로 컨셉아트 앞뒤옆위 4각도)

- 30~48차 커밋에 포함(브랜치 `claude/project-thread-ykg56g` → PR 로 main 에 머지; 게임 프로젝트 파일 변경 없음 — 그림은 `char-pipeline` 쪽).
- Tripo Image(Nano Banana Pro, 1:1, 무료 횟수 — 크레딧 안 씀, 23,370 그대로). 정면: 여우 정면(`output\animals\fox_front.png`)을 참고로 "같은 3D 카툰·치비 비율의 아기 붉은 드래곤"(진홍 비늘, 크림색 배 비늘판, 뒤로 휜 상아색 뿔 둘, 머리 뒤 작은 가시, 금색 눈, 박쥐 날개(주황빛 막), 화살촉 꼬리, 흰 티·짙은 반바지·주머니 달린 갈색 벨트, A 포즈). 뒤·옆·위: 고른 정면 그림을 참고 이미지로 넣고(페이지 JS 로 그림을 받아 파일 입력에 넣음 — 편집(Edit Image) 창은 3분 넘게 멈춰서 안 씀) 같은 캐릭터를 그 각도로. 각 2장씩 뽑아 고름(위는 두 번).
- 결과: `C:\work\char-pipeline\input\dragon\concept\dragon_front|back|side|top.png`(1024²), 모음 `dragon_concept_sheet.png`. Tripo 이미지 기록에도 남아 있음.
- 아쉬운 점: 위 그림은 머리가 몸을 거의 가리고(치비 비율이라 당연), 꼬리 끝 화살촉이 안 나옴(두 번 다). 정면 그림은 셔츠 앞이 열린 조끼처럼 보임(배 비늘이 보임).
- 다음(요청 시): 정면 그림 → Tripo 3D(30 크레딧) → Mixamo 리깅 → 애니메이션 → 꼬리·날개 처리 → `RRSpecies::Dragon` + 궁극기.

### 30차 덧붙임 — 드래곤 3D 모델 + 리깅 (사용자: 정면 그림으로 3D 모델 만들고 리깅까지)

- 30~48차 커밋에 포함(브랜치 `claude/project-thread-ykg56g` → PR 로 main 에 머지; 게임 프로젝트 파일 변경 없음 — 전부 `char-pipeline` 쪽, 언리얼 들여오기·코드는 아직).
- 3D: `dragon_front.png` → Tripo HD Model(H3.1, 2K PBR, 20k, 30 크레딧, 23,370 → 23,340) → 18,824 삼각형. FBX(Blender, 2K) → `input\dragon\tripo\` → `blender_tripo_to_obj.py` → `input\dragon\dragon_mixamo.zip`.
- 1차 Mixamo 리그(`input\dragon\mixamo_v1_badlegs\`, 쓰지 않음): 바닥에 깔린 꼬리가 발 옆에 있어서 **오른발 뼈가 몸 13cm 뒤**에 잡히고 꼬리는 왼발(469 정점), 날개 아래쪽은 팔에 스킨됨 → 걷기에서 꼬리가 왼발 따라 휘둘림.
- 고친 방법(몸만 리깅 → 전체 메시로 되돌림):
  1. 1차 리그 위에서 `blender_animal_tail.py` 로 꼬리·날개 정점을 골라(`input\dragon\picked_parts.fbx`) `scripts\blender_strip_parts.py`(새) 로 OBJ 에서 잘라냄(2,058 정점) → `dragon_body_mixamo.zip`.
  2. 몸만 Mixamo 에 다시 올림(같은 마커, 2 Chain Fingers) → 다리 좌우 대칭(발 뼈 x ±7cm, 같은 y) 확인 후 18개 다운로드 → `input\dragon\mixamo\`(`dragon_SK.fbx` = 몸 리그). **지금 Mixamo 에 남은 캐릭터 = `dragon_body_mixamo`.** 다운로드 스크립트(상품 id 18개 포함)를 `scripts\mixamo_export.js` 로 저장해 둠 — Mixamo 페이지에서 실행 후 `__runAll('<접두어>')`.
  3. `scripts\blender_merge_parts.py`(새): 전체 메시를 몸 리그 좌표로(몸 OBJ ↔ 몸 FBX 경계로 맞춤, 몸 정점 7,356개 정확히 일치) 옮기고 몸 정점은 Mixamo 무게 그대로, 잘린 정점은 높이로 뿌리 뼈(`Hips,0.35,Spine2`) → `input\dragon\dragon_merged.fbx`.
  4. `blender_animal_tail.py` 세 번 → **`input\dragon\dragon_rig.fbx`(최종)**: 꼬리 `Tail1..4`(Hips, 61cm, 반경 0.15, 1,200 정점 — 0.10 이면 바닥에 닿은 꼬리 밑면이 왼허벅지에 남아 걸을 때 바닥 아래로 늘어남), 날개 `WingL1..2`/`WingR1..2`(Spine2, 34cm, 막 가운데를 지나는 선, 반경 0.20, 몸 비율 1.0, 연결 모드 — 840/745 정점; 윗변을 따라 잡으면 막 아래쪽이 팔(96%)에 남아 팔이 내려갈 때 20cm 넘게 늘어남).
- `blender_animal_tail.py` 확장: 인자 `[NAME] [PARENT] [CONNECTED]`(기본 Tail, Hips, 끔 — 기존 호출 그대로). CONNECTED 1 = 고른 정점 중 끝 관절에 가장 가까운 정점과 고른 정점끼리 이어진 것만(날개 높이의 머리 옆 지느러미가 날개로 잡히던 것 제외). 꼬리엔 쓰지 않음(화살촉 끝이 따로 떨어진 조각이라 94개만 남음).
- 확인(블렌더): 부위 칠 렌더(`output\animals\dragon_check\rig_sheet.png`), 17개 애니메이션 전부 모서리 늘어남 측정 — 최대 6~8cm(겨드랑이·꼬리 뿌리와 다리 뒤, 쓰러짐/날아감 13~15cm; 다른 동물과 같은 수준), 처음엔 20~48cm. 텍스처 렌더 `anim_walk|fire|death|victory.png`(앞/옆/뒤 × 4프레임): 다리 정상, 날개는 등에, 꼬리는 엉덩이 따라감.
- 아직: 언리얼 들여오기(`ue_import_animal.py Dragon|…\mixamo|<베이스컬러>|…\dragon_rig.fbx`), 얼굴 그림, `RRSpecies::Dragon` 코드, 궁극기. 날개 뼈는 지금 움직이는 코드가 없음(Spine2 에 붙어 고정) — 날갯짓은 `URRRandomBodyAnimInstance` 에 꼬리처럼 추가해야 함. 쓰러짐·날아감에서 꼬리가 바닥을 뚫음(엉덩이에 고정이라).

## 29차 — 여우 둔갑술, 토끼 달토끼 낙하, 다람쥐 도토리 비 (사용자: 추천 조합으로 모두 만들어 줘)

- 29차 커밋에 포함(브랜치 `claude/project-thread-ykg56g`).
- 구조: 호랑이 기운의 공통 부분을 `ARRRandomUltimate`(`RRRandomUltimate.h/.cpp`, 추상)로 분리 — 브롤러·남은 수(`Left`)·주사위 눈(`Face`) 복제, 끝내기(`Finish` → 복제 `bEnding` → `OnEnding` → `Done` → `ARRRandomCharacter::EndUltimate`), 지점 고르기(`WantsSpots`, `IsBusy`, `IsReady`, `RequestSpot` + 예약, 플레이어 6초 → `GetDefaultSpot`, 봇 0.8초 → `PickTarget` 적 많은 곳), 높은 시점 `Overview`, 커서/경고 홀로그램(`ShowCursorSpot`, `ShowTargetHolo(지점, 반경, 세기)`, `ReleaseTargetHolo`), `FindGround`, `FindEnemiesNear`(원기둥), `HoldsBrawler`(못 움직이고 무적), `HidesBrawler`, `KeepsOverview`(고르는 동안 + 브롤러를 붙잡는 궁극기는 끝날 때까지 — 마지막 한 방을 보게), `GetTitle`/`GetStatusLine`(HUD). `ARRRandomTigerSpirit` 는 이 위로 옮김(동작 같음, `StrikeRadius` → `AreaRadius`).
- 캐릭터: `Spirit` → 복제 `Ultimate`, `IsUsingUltimate()` = 붙잡는 궁극기일 때만. 궁극기 주사위 확률 `UltimateDieChance` 0.15 를 **모든 종**에(분신 제외). `OnUltimateDie` 가 종별 클래스 스폰. 치트 U/F11 = 지금 캐릭터에 궁극기 주사위(호랑이로 바꾸지 않음). HUD: 굴린 결과·배너·아래 줄이 종별 이름(`RRSpecies::GetUltimateName/GetUltimateBanner`: FOX CLONES xN / MOON HOP xN / ACORN RAIN Ns / TIGER SPIRIT xN), 치트 줄 `[U] ULTIMATE DIE`. `ARRRandomGameMode::GetArenaCenter` public 으로.
- 여우 둔갑술(`ARRRandomFoxClones`): 지점 없음, 붙잡지 않음. 연기·나뭇잎 펑(`ARRRandomEffects::FoxPoof`) 속에서 여우 둘레 반지름 170 고리에 분신 N개 + 여우 자신이 무작위 자리로(`TeleportTo`), 모두 바깥으로 700 튕겨 흩어짐. 분신 = 같은 클래스 `ARRRandomCharacter` 에 복제 `bDecoy`(`MakeDecoyOf`: 팀·종·총·체력 복사) + 봇 AI. 분신 규칙: 총알 피해 0(맞는 쪽 `TakeDamage` 가 공격자가 분신이면 0, 숫자도 안 뜸), 진짜 피해를 받으면 펑 사라짐(`VanishDecoy` → `MulticastDecoyPoof`, 0.2초 뒤 파괴, 넉아웃·점수 없음), 주사위 안 굴림, 탈것 안 탐, 궁극기 주사위 없음, 체력은 여우 것으로 시작. MVP·승리 대열·결과표·팀원 카드·봇 이름 번호에서 제외(미니맵엔 보임 — 적에게는 속임수). 12초 뒤, 또는 여우가 쓰러지거나 경기가 끝나면 남은 분신 모두 펑.
- 토끼 달토끼 낙하(`ARRRandomMoonHop`): 반지름 380, 파란 커서/보라 경고 홀로그램. 토끼 메시(×1.4)가 클릭한 곳으로 높이 1500 포물선 1.35초 — 앞구르기(공중제비) → 꼭대기에서 큰 달빛(`MoonFlash`: 크림색 원판 + 은빛 고리 + 별) → 낙하 자세 → 엉덩방아 착지(`TigerStrike` 이펙트 은빛): 가운데 45 → 가장자리 25 피해, 700/500 튕김. 날아가는 동안 은빛 궤적. 진짜 토끼는 숨고 무적, 끝나면 마지막 착지 자리로 순간이동. Mixamo(지금 Mixamo 에 남은 새 토끼 리그 그대로): Jumping Up, Big Front Flip, Mid-Air Falling Idle, Mid-Air Falling To A Hard Landing → `input\rabbit\mixamo_extra\` → `ue_import_extra_anims.py` → `A_Rabbit_JumpUp|FrontFlip|FallIdle|HardLanding`.
- 다람쥐 도토리 비(`ARRRandomAcornRain`): 반지름 650 구역을 한 번 클릭(그동안만 붙잡음), 3 + 눈 초 동안 초당 32개 도토리가 높이 1400 에서 0.6초에 떨어짐. 각 도토리 착지(`AcornPop` 먼지·조각·별) 반지름 130 안 적 8 피해 + 520 으로 미끄러뜨림. 비는 모든 기기 같음(`MulticastRain` 의 지점·시드로 도토리마다 시간·자리 계산, 피해는 서버). 비가 오는 동안 구역 경고 옅게(세기 0.3), 다람쥐는 바로 다시 싸움. 도토리 = `scripts\blender_acorn.py`(열매 + 모자 + 꼭지 두 메시) → `scripts\ue_import_acorn.py` → `/Game/Effects/Acorn/SM_AcornNut|Cap`(M_DLCandy, 인스턴스 색 갈색/진갈색), 그리는 크기 ×2.5(만화처럼 크게 — 1배는 위에서 안 보였음).
- 확인: 빌드 경고 없음. 임시 `-RRTEMPAutoUlt`(내 캐릭터가 시작 뒤 궁극기 주사위 두 번)·`-RRTEMPNear`(내 지점을 바로 앞에) — 지움. 화면 없이 네 종: 여우 분신 5·4개, 토끼 1·2번 착지(넉아웃 포함), 다람쥐 4·6초 비(20·23번 맞힘, 넉아웃 3), 호랑이 그대로; 봇 여우·다람쥐도 스스로 씀, 분신이 쓰러뜨려지거나 사라져도 점수 없음. 화면: 여우 TPS 펑·분신(`output\screens\ult_fox`), 토끼 탑뷰 연속 착지·달빛·홀로그램(`ult_rabbit2`, 봇 토끼 6번 `in_rabbit`), 다람쥐 탑뷰 큰 도토리 비(`ult_squirrel3`). LAN 호스트+참가자 150초: 봇 여우 분신 3번, 봇 토끼 6번 착지, 봇 호랑이 — 참가자 로그에도 궁극기 시작, 에러 없음.
- 확인 못 한 것: 이번엔 자동 키·마우스 입력이 게임 창에 안 들어가서(사격도 안 됨 — 다른 창이 포커스를 잡은 듯) 플레이어가 직접 클릭해 고르는 토끼·다람쥐 시험은 못 함(같은 `RequestSpot` 경로는 27·28차 호랑이 클릭 시험에서 확인), TPS 에서 본 도토리 비, 참가자 화면의 분신 모습, 성능(분신 6 + 봇).

## 28차 — 호랑이 기운 범위 홀로그램, 지점은 늘 바닥 (사용자: 범위 표시가 바닥에 고정 안 되고 멀어질수록 커짐, 홀로그램 같은 이펙트로, 데미지 정상으로)

- 21~28차와 함께 한 커밋에 포함(브랜치 `claude/project-thread-ykg56g`).
- 원인: 27차의 지점 찾기가 커서 선이 처음 닿는 WorldStatic/Dynamic 을 지점으로 써서, 주사위 랜드 가장자리의 보이지 않는 림 벽(높이 6000), 나무·엄폐물 꼭대기 등 공중에 잡힘 → HUD 원이 공중(카메라 가까이)에 그려져 크게 보이고, 서버의 피해 원기둥(지점 −500 ~ +3500)도 공중에 떠서 바닥의 적이 빠짐.
- 지점: 커서 선과 바닥 높이 평면의 교점(`ARRRandomTigerSpirit::GetGroundZ` = 호랑이가 서 있던 발 높이) → `FindGround`: 그 XY 에서 +250 위부터 아래로 WorldStatic 만, 폰 무시, 위를 향한 면(법선 Z > 0.6) 첫 번째. 서버 `MulticastStrike` 도 같은 `FindGround`(전엔 +600 위에서 Visibility 채널).
- 홀로그램(`/Game/Effects/DiceFX/M_FXHolo`, `scripts\ue_make_holo.py`): 엔진 `/Engine/BasicShapes/Cylinder` 를 반지름 `StrikeRadius` 500, 높이 `HoloHeight` 260 으로. 반투명(가산은 주사위 랜드의 밝은 바닥에서 안 보였음), 언릿, 양면, 깊이 테스트 끔(거인 몸 너머로도 보임). 물체 좌표로 H(높이 0..1)·R(반경 0..1): 벽 = (1−H)² × (0.35 + 위로 흐르는 스캔라인), 바닥 = 0.12 + 바깥으로 퍼지는 물결 + R>0.88 테두리 + 테두리를 도는 점선 12개. 발광 = Color × Glow × (0.6 + 양), 불투명도 = 양 × 2.2 × "Opacity".
  - 커서 홀로그램(`CursorHolo`, 주황): 그 호랑이 플레이어 기기에서만, 플레이어 컨트롤러 `PlayerTick` 이 매 프레임 `ShowCursorSpot`. 쓸 수 있는 동안 계속(치는 중 클릭 예약도). HUD 의 선 원은 지움.
  - 칠 곳 경고(`TargetHolo`, 진한 빨강·맥동): 모든 기기에서 `MulticastStrike` 때 그 자리에, 앞발이 닿으면 0.35초에 사라짐 — 상대도 피할 곳을 봄.
- 확인: 빌드 경고 없음. 주사위 랜드 TPS 에서 화면 여러 곳(가장자리 쪽 포함) 클릭 6번 → 지점 모두 바닥(Z 1.5), 임시 `-RRTEMPUltPlace`(지점 옆 100/400 에 적 둘 — 지움)로 매번 원 안 적 피해(넉아웃 포함). 탑뷰 섬·TPS 주사위 랜드 캡처(`output\screens\holo3`, `holo4`, `holo5`): 가까운 곳·먼 곳 원 크기 같음, 바닥에 붙어 물결·테두리·벽이 보임.
- 확인 못 한 것: 언덕 위·다리 위처럼 호랑이 발 높이와 많이 다른 곳(교점이 조금 어긋날 수 있음 — FindGround 가 +250 위부터 찾음), 참가자 화면의 칠 곳 경고.

## 27차 — 호랑이 기운: 아무 곳이나, 범위 안 적 전부 (사용자: 범위 지정은 캐릭터와 상관없이 주변 원하는 곳으로 뛸 수 있고 범위 내 적은 모두 피해)

- 21~28차와 함께 한 커밋에 포함(브랜치 `claude/project-thread-ykg56g`).
- 고친 것 (`ARRRandomPlayerController::GetUltimateSpot`, `ARRRandomTigerSpirit`):
  - 지점: 전엔 WorldStatic 지형에만 맞춰서 바다·맵 밖·하늘 쪽 클릭은 무시됨 → 캐릭터(모든 폰)·탈것·거인은 통과, WorldStatic+WorldDynamic 에 맞추고, 아무것도 없으면 바닥 높이(호랑이 발) 평면과 만나는 점.
  - 내려치는 중·커지는 중 클릭: 전엔 버려짐 → 서버가 하나 기억(`QueuedTarget`, 마지막 클릭)했다가 준비되면 바로 침. 클라이언트 재요청 간격 0.5 → 0.3초. HUD 원은 치는 중에도 커서 아래 표시, 안내 `[CLICK ANYWHERE]`.
  - 플레이어가 6초 안 고르면: 전엔 적이 모인 곳 → 거인 바로 앞(앞발 거리 × 1.5). 적 모인 곳 고르기는 봇 호랑이만.
  - 피해: 전엔 높이 차 ±500 넘으면 제외 → 지점 아래 500 ~ 위 `GiantHeight` 원기둥(날거나 엄폐물 위 적도). 2D 거리 ≤ 반지름 500 + 캡슐 반경이면 모두. 맞을 때마다 로그 `tiger spirit hits X <거리> away: <체력> -> <체력>`(빗나감은 Verbose).
  - 맞지 않는 예외(의도): 같은 팀, 이미 쓰러진 적, 무적(치트), 자기 호랑이 기운을 쓰는 중인 상대 호랑이(숨은 본체는 데미지 없음).
- 확인: 빌드 경고 없음. 임시 `-RRTEMPUltTest`(지점 주변 150/480/700 에 적을 옮기고 그 지점을 침, 궁극기 주사위 자동 굴림 — 확인 후 지움)로 탑뷰·TPS 7번: 치는 순간 원 안(≤542) 적은 모두 맞음(36~526), 빗나간 3명은 모두 밖(591, 644, 700). 처음 몇 번 "가까이 놓은 적이 안 맞음"은 1초 사이 봇이 걸어 나간 것. 실제 클릭(탑뷰, `-ExecCmds="RRUltimate"` + E, `output\screens\ult_click`): 커지는 중 왼쪽 위 클릭 → 기억했다가 그 자리, 오른쪽 개울가, 맵 밖 바다(왼쪽 아래)까지 찍은 곳마다 뛰어가 내려침.
- 캡처 스크립트(scratchpad `capture_keys.ps1`): 클릭마다 화면 위치 `"시간:1:dx:dy"`.

## 26차 — 궁극기 주사위 치트, 굴릴 때 연출 (사용자: 궁극기 주사위를 얻는 치트, 굴릴 때 조금 더 화려하게)

- 21~28차와 함께 한 커밋에 포함(브랜치 `claude/project-thread-ykg56g`).
- 치트: **U** 키(F11 도 그대로 — 단 에디터 PIE 에선 F11 이 에디터 전체 화면 키라 게임에 안 감) / 콘솔 `RRUltimate`. 호랑이가 아니면 먼저 호랑이로 바꾼 뒤(`SetSpecies`, 로그 `is now the Tiger`) 궁극기 주사위 하나 저장. E 로 굴림. 치트 줄 `[U] TIGER ULT DIE`.
- 굴리기 연출(각 기기, 보는 사람 모두):
  - `URRRandomDiceBuffComponent::GetSpinTime()`: 궁극기 주사위는 `UltimateSpinTime` 1.6초(보통 0.6) 동안 굴러간 뒤 효과. HUD 결과 표시·머리 위 주사위 모두 이 값을 씀.
  - `URRRandomOverheadDieComponent`: 궁극기면 금색(`/Game/Props/Dice/M_DiceGold`, `scripts\ue_make_gold_die.py` — 금색 베이스, 금속 0.9, 거칠기 0.35, 발광 = 색 × "Glow") + 진한 빨강 눈(M_DicePip MID), 크기 × `UltimateScale` 1.8, 높이 × `UltimateHop` 1.5(2.4 는 TPS 에서 화면 위로 잘림), 회전 `UltimateTurns` 6바퀴, 빛 맥동 + 착지 때 번쩍.
  - `ARRRandomEffects::UltimateDieSpin`: 주사위 둘레 3갈래 금빛 별 소용돌이(점점 빨라짐), 발밑에서 주사위까지 금빛 기둥, 기둥 타고 오르는 불꽃, 바닥 파동 고리. `UltimateDieBurst`(착지): 금·흰·주황 고리 3겹, 섬광, 빛 기둥, 별과 금 주사위 분수, `GiantBurst` 충격파.
  - 내 HUD: 굴러가는 동안 화면 아래쪽(크로스헤어 밑) 금색 `ULTIMATE DIE!`(맥동), 착지하면 화면 전체 금빛 섬광 + `TIGER SPIRIT xN!` 크게 쾅 → 1.8초 뒤 사라짐(`ARRRandomHUD::UltimateBannerTime`). 처음엔 화면 위쪽 0.28 에 뒀다가 머리 위 표시들과 겹쳐서 0.66 으로.
- 확인: 빌드 경고 없음. `-ExecCmds="RRUltimate"` + E 키로 여우(TPS)·토끼(TPS)·다람쥐(탑뷰)에서 시작 → 각각 호랑이로 바뀌고 궁극기 주사위 굴림 → 호랑이 기운까지 이어짐(로그·캡처 `output\screens\ultdie`, `ultdie_tps`, `ultdie_top`).
- 확인 못 한 것: U 키를 사람이 직접 누른 것(자동 입력은 E 만, 치트는 콘솔 명령으로), 다른 사람(참가자) 화면에서 본 금색 주사위.

## 25차 — 외형 손질 3가지 + 호랑이 기운 (사용자: 1 호랑이 뒤통수 호피무늬 2 다람쥐 머리와 꼬리 사이 그래픽 깨짐 3 토끼 주둥이 더 귀여운 카툰 4 호랑이 궁극기)

- 21~28차와 함께 한 커밋에 포함(브랜치 `claude/project-thread-ykg56g`).
- 호랑이 뒤통수: `scripts\blender_tiger_back_stripes.py <tripo fbx> <베이스컬러> <png> [미리보기]` — 머리 가운데 기준 각도(뒤 0°, 옆 ±)·높이로 정의한 줄무늬 5개(정수리에서 굵고 아래로 뾰족, 이마 줄무늬와 같은 진갈색, 가장자리 1.6° 부드럽게)를 뒤를 향한 머리 면의 텍셀에만 칠함(UV 이음매 양쪽이 겹치게 1.5텍셀 여유, 옆 64° 넘으면 귀·흰 볼털이라 제외) → `output\animals\T_Tiger_BaseColor_striped.png` → `T_Tiger_BaseColor` 다시 들여옴(스크래치 `ue_reimport_tex.py`). 게임 TPS 뒤 화면에서 확인(`output\screens\look_Tiger`).
- 다람쥐 깨짐: 원인은 Tripo 메시에서 꼬리와 뒤통수가 붙어 있어 꼬리 뼈 정점과 머리 정점이 같은 면 58개를 공유 → 머리가 돌거나 꼬리가 흔들리면 볼에서 꼬리까지 길쭉한 조각으로 늘어남(머리 35° 돌린 렌더로 재현, `output\animals\tail\seam_compare.png`). `blender_animal_tail.py` 에 6번째 인자 SEAM(m): 꼬리와 다른 부위(Hips 제외)가 만나는 면에서 그 거리 안 정점의 뼈 무게를 모든 그룹 스무딩 12번 + 정규화 → 다람쥐 6cm(411 정점). 늘어난 조각이 작은 볼록함 정도로 줄어듦. 게임 TPS 뒤 화면에서 확인(`look_Squirrel`).
- 토끼 주둥이: 입이 벌어지고 이빨·잇몸이 보이는 모양이 메시에 파여 있어 텍스처로는 안 되므로 새로 만듦 — 기존 토끼 정면 렌더를 Tripo Image(Nano Banana Pro, 무료)에 넣고 "나머지는 그대로, 주둥이만: 작은 하얀 볼 주둥이, 하트 분홍 코, 다문 w 미소, 작은 앞니 두 개" → HD Model(30, 23,400 → 23,370) → FBX → Mixamo(호랑이를 덮어씀) 18개(날아가기 포함, 토끼 자기 리그) → 꼬리 2뼈 → 언리얼. 옛 토끼 원본은 `input\rabbit_v1`. 새 토끼는 다리가 짧고 귀·머리가 커서 엉덩이 17.5, 크기 2.45(걷기 실측 52/s), 얼굴 그림 다시 렌더.
- 호랑이 기운(`RRRandomTigerSpirit.h/.cpp`):
  - 주사위(`URRRandomDiceBuffComponent`): `UltimateDieChance`(호랑이만 `TigerUltimateDieChance` 0.15, 종 적용 때 설정) — 게이지로 얻는 주사위가 그 확률로 궁극기 주사위(한 번에 하나), 저장 수 `UltimateCharges` 복제, 굴리면 그것부터(`FRRDiceRoll::bUltimate`), 착지하면 `OnUltimateDie(눈)`. 치트 F11 / `RRUltimate`(`GiveUltimateDie`).
  - 캐릭터: `OnUltimateDie` 가 `ARRRandomTigerSpirit` 를 스폰(Instigator = 호랑이 → 킬 인정), 복제 `Spirit`. 쓰는 동안 몸·총 숨김, 이동 속도 0, 사격·점프·주사위·데미지 없음(`IsUsingUltimate`). 끝나면 `EndUltimate`.
  - 거인: 키 `GiantHeight` 3500(맵 지름 약 6000, 탑뷰 카메라 높이 약 4900 이하), 호랑이 메시·머티리얼 + 주황 빛 맥동(HitFlash 파라미터), `A_Tiger_Roar`(Mixamo Mutant Roaring)로 2초 동안 커짐 → 대기 → 내려치기 `A_Tiger_JumpAttack`(Mixamo Mutant Jump Attack: 1.7초에 두 앞발이 리그 앞 77.4·높이 9.5에 닿고 엉덩이는 50 앞으로 — 블렌더로 잼) 0.35초부터 1.3배속. 앞발이 정확히 지점에 떨어지게 지점 − 바라보는 방향 × 77.4 × 크기 자리로 도약하며 돌고, 끝나면 엉덩이가 간 만큼 앞에 섬. 반지름 `StrikeRadius` 500(캐릭터 폭의 약 10배) 안 상대 팀에게 가운데 70 → 가장자리 35 데미지, 살아남으면 밖으로 날림. 다 치면 1.2초 줄어들어 호랑이로.
  - 지점: 호랑이 플레이어는 클릭(커서 아래 바닥, HUD 에 반지름 원), 6초 안 고르면 거인이 고름. 봇 호랑이는 0.8초 뒤 적이 가장 많이 모인 곳.
  - 시점(그 플레이어만): 모든 캐릭터 평균 위치 위 높은 카메라(피치 -60, 7000 뒤, FOV 70)로 0.8초 블렌드, 커서 보임, 전장의 안개 즉시 걷힘(서서히 걷으면 시점이 넘어가는 순간 검은 화면 한 장이 나왔음), 끝나면 돌아옴.
  - 다른 사람 화면: 카메라가 거인이 차지하는 공간(몸 + 앞발이 닿는 앞쪽) 안이면 그 기기에서만 거인을 `M_FXGhost` 반투명 주황으로(TPS 에서 화면이 털로 가득 찼음).
  - 이펙트 `ARRRandomEffects::TigerStrike`(충격파 고리 3개, 섬광, 먼지, 별·주사위), 커질 때 `GiantBurst`, 서 있는 동안 `GiantAura`, 사라질 때 `TeleportPoof`.
  - 네트워크: 거인 액터 복제(항상), 내려치기는 `MulticastStrike`, 줄어들기는 복제 `bEnding`; 참가자 기기에서 호랑이가 늦게 도착해도 그때 시작(`TryStart`).
  - HUD: 궁극기 주사위가 있으면 머리 위 주사위 상자 금색 + 아래 `ULTIMATE DIE: TIGER SPIRIT!`, 굴린 결과 `6 TIGER SPIRIT`, 쓰는 동안 `TIGER SPIRIT [CLICK] PAW STRIKE xN LEFT`, 치트 줄에 `[F11] TIGER ULT`.
- 확인: 빌드 경고 없음. 임시 `-RRTEMPUlt`(호랑이마다 시작 2초 뒤 궁극기 주사위를 주고 굴림)·`-RRTEMPAllTiger`(봇 모두 호랑이) — 확인 후 지움. 화면 없이 호랑이 3~4마리가 각자 1~6번씩 끝까지 치고 맞은 수 기록, 탑뷰 플레이어 호랑이 화면(`output\screens\ult2`, `ult_input2`), 토끼 TPS 에서 본 거인(`ult3` 불투명 → `ult5` 반투명), 실제 입력(F11 → E → 6 → 클릭 6번 모두 커서 자리, `ult_input`), LAN 참가자 화면에도 거인들 등장. 일반 실행 중 봇 호랑이가 확률로 얻어 쓰는 장면도 캡처됨(`rabbit2`).
- 확인 못 한 것: 실제 사람이 탑뷰/TPS 에서 여러 곳을 골라 치는 손맛(시험은 커서 고정), 참가자 호랑이 플레이어의 클릭(서버 RPC 경로), 거인이 맵 밖 바다에 서는 모습, 성능(거인 그림자·이펙트). 자동 키 입력은 가끔 게임 창에 안 들어가서 몇 번은 `-ExecCmds="RRUltimate"` 로 대신함.
- Mixamo 에 지금 남은 캐릭터: 새 토끼(호랑이 리그는 사라짐 — 호랑이에 쓰는 모션은 모두 받아 둠: `input\tiger\mixamo\tiger_roar|swipe|jumpattack|punch|flyback.fbx`).

## 24차 — 차에 치이면 날아감 (사용자: 뒤로 날아가는 모션과 함께 날아가면서 죽게, Mixamo 에서 적당한 모션)

- 21~28차와 함께 한 커밋에 포함(브랜치 `claude/project-thread-ykg56g`).
- 모션: Mixamo "Flying Back Death"(Dying Flying Backward, 3.0초, 엉덩이 0.4초에 가장 높고 0.93초에 땅, 뒤로 43cm). Mixamo 엔 호랑이만 남아 있어(23차 메모) 호랑이 리그로 받음 → `C:\work\char-pipeline\input\tiger\mixamo\tiger_flyback.fbx`.
- 네 종에 옮기기: `scripts\ue_import_flyback.py <fbx>|26.3|Fox=27.5|Rabbit=22.1|Squirrel=21.2|Tiger=26.3` → `A_<Name>_FlyBack`. Mixamo 리그는 뼈 이름·축이 같아서 로컬 회전은 그대로, 뼈 길이(엉덩이 말고 모든 뼈의 이동)는 각자 스켈레톤 기준 포즈로, 엉덩이 높이는 엉덩이 높이 비로, 엉덩이의 땅 위 이동은 첫 프레임 자리로 고정(게임이 몸을 던짐). 다람쥐에 없는 엄지 트랙은 빠짐. 각 종 자기 대기 애니메이션 뼈 길이 = 기준 포즈(차 0.00) 확인. 주의: UE 파이썬이 트랙 이름을 소문자로 돌려줌, 이미 있는 애니메이션을 덮어쓰면 압축 설정이 깨져서 파일을 먼저 지우고 들여옴.
- 코드:
  - `FRRSpeciesLook::FlyBack` → `ARRRandomCharacter::FlyBackAnimation`.
  - `ARRRandomVehicle::UpdateRam` 이 데미지 직전에 `Victim->SetRamImpact(속도)`. `KnockOut` 이 같은 순간(월드 시간 double 로 비교 — float 로 했다가 항상 달라서 안 됐음)의 충격이면 복제 `FlingVelocity` = 차 방향 × clamp(차 속도 × `FlingSpeedShare` 0.8, `FlingSpeedRange` 600~1300) + 위 `FlingLift` 480(중력 980 에서 약 1초, 모션 착지와 맞음).
  - `ApplyKnockedOutBody`(서버·OnRep 모두): 날림이면 캡슐 충돌은 켠 채 Pawn 만 무시(다른 캐릭터·운전자 통과, 지형엔 막힘), 차 반대쪽을 보게 돌리고(움직임 방향/컨트롤러 회전 끔), `MOVE_Falling` + 속도, `FlyBackAnimation` 한 번. `UpdateFling`(쓰러진 동안 Tick): 0.2초 뒤 땅(또는 서버가 이미 멈춤)이면 `EndFling` — 이동 끄고 캡슐 충돌 끔, Pawn 응답 되돌림, 서버 로그 `run over, lands <거리> away after <초>`. 맵 밖으로 날아가면 부활 때까지 떨어짐. 부활(`ApplyStandingBody`)이 날던 중이면 정리.
- 확인(임시 `-RRTEMPRam`: 시작 뒤 가까운 브롤러를 속도 1400 으로 옆/카메라 쪽으로 친 것처럼 — 확인 후 지움): 화면 없이 4번 모두 0.98~1.00초 뒤 착지, 약 11m(장애물에 걸리면 6.8m). TPS 창 캡처(`output\screens\fling2`)에서 호랑이가 팔다리를 뻗고 뒤로 날아 플레이어 뒤를 지나 등으로 떨어져 누움. LAN 호스트+참가자: 참가자 화면에서도 같은 몸이 0.99초 뒤 착지(732 vs 호스트 768). 일반 봇전 끝까지 에러 없음.
- 확인 못 한 것: 실제 차가 사람을 치는 장면(봇이 차로 치는 경우가 드묾 — 경로는 같은 `SetRamImpact` → 데미지), 거인이 치였을 때, 탑뷰에서의 모습.

## 23차 — 토끼·다람쥐·호랑이 (사용자: 여우와 비슷한 느낌으로 추가하고 시작할 때 고르게, 봇은 랜덤, Tripo·Mixamo 로)

- 21~28차와 함께 한 커밋에 포함(브랜치 `claude/project-thread-ykg56g`).
- 그림: 여우 OBJ 정면 렌더(`scripts\blender_fullbody.py` → `output\animals\fox_front.png`)를 Tripo Image(Nano Banana Pro, 1:1, 무료)에 참고로 올려 "같은 스타일·비율·A 자세, 비슷한 옷(티셔츠, 반바지, 갈색 주머니 벨트)" — 토끼(흰 털, 하늘색 티, 청 반바지), 다람쥐(적갈색, 흰 티, 올리브 반바지, 등 뒤로 솟아 말린 큰 꼬리), 호랑이(주황 줄무늬, 흰 티, 짙은 회색 반바지, 옆으로 휜 줄무늬 꼬리).
- 3D: 그림 → Tripo HD Model(H3.1, 2K PBR, 삼각형 20,000 목표, 1회 30 크레딧 × 3, 23,490 → 23,400). FBX(Blender, 2K)로 받음 → `C:\work\char-pipeline\input\<rabbit|squirrel|tiger>\tripo\`(19,290 / 19,262 / 19,842 삼각형).
- Mixamo 용 OBJ: 새 Tripo FBX 는 1m 높이에 노드 변환이 있어서 `fbx_to_obj.py`(원시 좌표)로는 누워 나옴 → `scripts\blender_tripo_to_obj.py <fbx> <obj> <베이스컬러 파일명>`: 블렌더가 변환 적용, 높이 100(여우 OBJ 와 같게), 발(아래 6%) 기준 가운데, Y-up → `input\<동물>\<동물>_mixamo.zip`(obj+mtl+jpg).
- Mixamo(사용자 계정, Claude in Chrome): 업로드 → 마커 → **2 Chain Fingers (41)**(여우와 같은 뼈 이름). Mixamo 는 대칭선을 모델 범위 가운데에 그려서 꼬리가 옆으로 큰 다람쥐·호랑이는 대칭을 끄고 마커를 하나씩 찍음. **Mixamo 는 올린 캐릭터를 하나만 보관** — 새로 올리면 이전 것(여우 `fox_mixamo_upload` → 토끼 → 다람쥐 → 지금 호랑이)이 사라짐. 여우 애니메이션을 더 받으려면 `C:\Users\user\Downloads\fox_mixamo_upload.zip` 을 다시 올려 리깅해야 함(리그가 조금 달라질 수 있음).
- 다운로드(페이지 안 API, `X-Api-Key: mixamo2` + localStorage 토큰): 캐릭터마다 17개 — `<동물>_SK.fbx`(Two Hand Rifle Idle, With Skin), walk(Quickly Walk Forward With Rifle), fire(Firing A Rifle While Standing, 짧은 것), death(Rifle Getting Hit To Ground), victory(House Dance Variation Two), aim_idle, run_f/b/l/r(in place), dance1~4, defeat1~3 — 여우가 쓴 것과 같은 상품(프레임 수로 확인). 결과 파일은 CORS 때문에 fetch 로 못 받아 링크 클릭으로 받음, 너무 빨리 요청하면 429 → 4초 간격. → `input\<동물>\mixamo\`.
- 꼬리: Mixamo 자동 리그가 다람쥐 꼬리는 **왼팔(56%)·목**, 호랑이 꼬리는 **왼허벅지 100%**, 토끼 꼬리 공은 **머리 83%** 에 붙임 → `scripts\blender_animal_tail.py <fbx> <out> "x,y,z;..." [반경] [몸 비율]`: 블렌더 좌표(m, +Y 뒤, +X 캐릭터 왼쪽) 중심선을 따라 Hips 자식 `Tail1..N`, 중심선에서 반경 안이고 다른 뼈보다 가까운(× 몸 비율) 정점만 꼬리 뼈로 다시 스킨(뼈마다 구간, 3cm 섞기, 뿌리 5cm 는 Hips 와 섞음). 중심선은 `scripts\blender_rest_grid.py`(옆/뒤 10cm 격자 렌더)에서 읽음. 다람쥐 4뼈 97cm(반경 0.34, 몸 비율 1.15, 4,003 정점), 호랑이 4뼈 53cm(0.08, 0.85, 801), 토끼 2뼈 8cm(0.05, 0.85, 233). 확인은 꼬리 무게 칠한 렌더(`output\animals\tail\*_paint_*.png`).
  - 덤으로 찾은 것: `blender_fox_tail.py` 의 마지막 꼬리 뼈 무게 식(`hi = 1.0`)이 0 이 되어 여우 Tail4 엔 정점이 없고 끝을 지난 정점은 무게가 없을 수 있음. 새 스크립트에선 고침(`hi = 0.0`), 여우는 손대지 않음.
- 언리얼: `scripts\ue_import_animal.py <Name>|<mixamo 폴더>|<베이스컬러>|<꼬리 fbx>` → `/Game/Characters/<Name>/`: `<name>_SK.fbx` 로 `SK_<Name>`+스켈레톤을 만들고 꼬리 FBX 를 그 스켈레톤으로 다시 들여옴(다른 뼈 기준 포즈 변화 0.000 확인), `Anims/A_<Name>_Idle..AimRunR`(걷기·춤은 엉덩이 이동 제거), `Textures/T_<Name>_BaseColor`, `M_<Name>` = 여우 머티리얼 지금 모양(베이스 × DiffuseColor 틴트, 평평, 그리고 `ue_fox_pastel.py`·`ue_fox_hitflash.py` 를 그대로 실행). 이미 있던 폴더를 다시 들여올 땐 게임 코드가 시작 때 에셋을 잡고 있어 지우기가 실패함 → `Content\Characters\<Name>` 을 먼저 파일로 지울 것. 다람쥐는 엄지 뼈가 없음(33 + 꼬리 4) — 코드가 쓰는 손 뼈(RightHand, RightHandIndex1, LeftHand)는 있음.
- 얼굴: `blender_portrait.py <SK fbx> <png> 256 [머리 x]`(다람쥐는 꼬리 때문에 머리 x 0 지정) → `ue_import_ui_texture.py` → `/Game/UI/T_<Name>Portrait`.
- 코드:
  - `RRRandomSpecies.h/.cpp`: `RRSpecies`(Fox, Rabbit, Squirrel, Tiger; 이름 ↔ 번호), `FRRSpeciesLook`(메시, 머티리얼, 대기/걷기/사격/쓰러짐, 춤들, 좌절들, 조준 세트, 메시 크기, 메시 오프셋, 걷기 속도, 얼굴) — `GetAll()` 이 생성자에서 경로 규칙으로 찾음. 크기 = 여우 엉덩이 높이 맞춤과 머리 끝 맞춤의 중간(토끼 2.14, 다람쥐 2.05, 호랑이 1.75; 엉덩이만 맞추면 다람쥐 머리가 여우보다 훨씬 큼). 걷기/조준 달리기 속도 = 여우 값 × 엉덩이 높이 비 × 크기(토끼 걷기 실측 65 ≈ 계산 63.5). (처음엔 기본 자세 엉덩이 XY 로 메시 오프셋을 줬다가 24차에서 뺌: Mixamo 애니메이션은 기본 자세와 상관없이 엉덩이를 원점 근처(0.3, 0.8)에 두고 재생해서, 호랑이가 오히려 30cm 어긋났음.)
  - `ARRRandomCharacter`: `SpeciesLooks`(모두 로드 유지), 복제 `Species`(OnRep), `SetSpecies`(서버), `UseLook`, `ApplySpecies`(시작 전이면 BeginPlay 가 적용; 시작 뒤면 메시·머티리얼 교체 + `URRRandomEventComponent::RefreshMesh` + 팀 색 + 잔상 지움 + 자세), `GetPortrait`. 클라이언트에서 시작할 때 `arrives as the <종>` 로그.
  - 게임 모드: `SpawnBrawler(…, Species)` 지연 스폰에서 설정 + `is a <종> on team` 로그. 봇 = `FMath::RandRange` 4종, 플레이어 = 컨트롤러가 알려 준 값 또는 이 기기 플레이어면 세션 서브시스템 값.
  - `URRRandomSessionSubsystem::Get/SetSpecies`(레벨 넘어 유지, `-RRSpecies=<이름>`). `ARRRandomPlayerController`: 참가자가 BeginPlay 에서 `ServerChooseSpecies` → 서버가 기억(`GetChosenSpecies`, 새 게임 때 스폰에 씀)하고 지금 브롤러를 바꿈.
  - 타이틀: 시점(TOP VIEW / THIRD PERSON) 고른 뒤, 참가(JOIN)는 바로 `PICK YOUR BRAWLER` 페이지 — FOX / RABBIT / SQUIRREL / TIGER(버튼 왼쪽에 얼굴, 설명 한 줄) / BACK. 마지막 고른 것이 선택돼 있음, Esc/BACK 은 이전 페이지로.
  - HUD 팀원 카드 얼굴 = 그 브롤러의 종(`GetPortrait`, 없으면 여우).
- 확인:
  - 빌드 경고 없음. `-nullrhi` 봇전(`-RRSpecies=Rabbit -RRScoreToWin=3`): 플레이어 토끼, 봇 여우·다람쥐×2·호랑이 등 랜덤, 경기 끝까지 에러 없음.
  - 창 캡처: 탑뷰 호랑이(`output\screens\species1`), TPS 다람쥐(`species2`), 타이틀 키 입력(싱글 → THIRD PERSON → 캐릭터 페이지 → 다람쥐 → 시작, `species_title`; 캡처 스크립트에 키/마우스 입력 추가), 결과 화면에서 다람쥐·호랑이 춤(`species3`), TPS 에서 네 종 모두 마우스 왼쪽 누른 채 사격 — 총이 손에, 같은 조준 자세(`species_fire\compare.png`).
  - LAN 두 프로세스(`-RRHost -RRLan -RRSpecies=Tiger` / `-RRJoin -RRLan -RRSpecies=Squirrel`): 호스트에서 참가자 브롤러가 여우로 생겼다가 `is now the Squirrel`, 참가자 화면에서도 자기 것이 다람쥐로 바뀌고 봇들은 호스트와 같은 구성(호랑이 3, 토끼 1, 다람쥐 2)으로 도착.
- 확인 못 한 것: 실제 마우스로 해 본 타이틀 클릭, 참가자가 새 게임(`?Restart`) 뒤에도 고른 종 유지(서버가 컨트롤러에 기억하지만 재접속 때 컨트롤러가 새로 생기면 다시 RPC 로 보냄 — 로그로는 안 봄), 꼬리 흔들림의 실제 모습(다람쥐 꼬리는 크고 위로 솟아 여우용 흔들기 축이 덜 맞을 수 있음), 각 종의 쓰러짐·거인·비행 모습, 연습 더미(`ARRRandomDummy`)는 여전히 여우, 타이틀 그림도 여우.

## 22차 — TPS 카메라, 거대화·날개·피격 이펙트

- 21~28차와 함께 한 커밋에 포함(브랜치 `claude/project-thread-ykg56g`).
- 카메라(`ARRRandomCharacter::UpdateCamera`, 3인칭): 팔 길이와 어깨 오프셋에 몸 크기(`GetActorScale3D().Z`, 거인 2배)를 곱함 — 스프링암은 액터 크기를 따라 커지지 않아 거인이 화면을 덮었음. 비행·활공 중(`FlightCameraPull`, 이제 3인칭에서도 계산)엔 팔 × (1 + 0.55), 위로 +70, 화각 +8°. 조준: 200 / (0, 95, 115) / 50° → 290 / (0, 125, 95) / 52° — 캐릭터가 왼쪽 아래 가장자리에 어깨·귀만 걸침(145 까지 비켰더니 귀 끝만 보여 되돌림).
- `ARRRandomEffects`(새, 기기마다 하나, 복제 안 함, `Get()` 이 처음 쓸 때 만듦): 풀 방식 파티클 — 구·링·별(날개 반짝이 메시)·주사위를 `/Game/Effects/DiceFX` 의 `M_FXGlow`(언릿 반투명, 인스턴스 커스텀 데이터 0~2 색, 3 밝기, 4 불투명) 인스턴스로, Tick 에서 날고 커지고 사라짐. 별과 일부 링은 카메라를 향함. 밝기 1 넘는 값은 `GlowSqueeze` 0.3 으로 눌러 넣음(그대로면 밝은 하늘 앞에서 색이 다 하얗게 탐). 카메라 500 안쪽 파티클은 반지름을 빼고 가까울수록 옅게(최저 0.1) — 내 캐릭터 주변 폭발이 화면을 덮던 것.
  - 거대화 시작: 바닥 충격파 링 두 개(금색, 팀색), 먼지 고리 18, 위로 튀는 별 40·사탕색 주사위 14, 가슴 섬광, 가는 빛기둥. 유지 중: 발밑 링 초당 2.2, 몸 둘레로 올라가는 빛 알갱이 초당 16, 가끔 별. 끝: 작은 링 + 옅은 먼지 + 별.
  - 날개 시작: 카메라를 향한 하늘색 링 + 바닥 링 + 섬광 + 별 32. 비행·활공 중: 날개 위치에서 별과 빛 뭉치가 꼬리처럼(속도에 비례).
  - 피격(`MulticastDamagePopup` — 모든 기기): 총마다 색과 세기(`HitBurstStrengths`/`HitBurstColors`, ERRHitEffect 순서) 로 카메라 쪽으로 튀는 불꽃 + 별 + 카메라를 향한 링 + 섬광, 그리고 몸 번쩍임.
  - 상태 변화는 각 기기에서 복제된 `bGiant`/`bFlying` 을 `bWasGiant`/`bWasFlying` 과 비교해 감지(쓰러진 동안은 조용히 맞춤 — 부활 때 엉뚱한 효과 안 나게).
- 에셋: `C:\work\char-pipeline\scripts\ue_make_fx.py <dice_land fbx 폴더>` → `M_FXGlow`, `SM_FXBall/Ring/Die`(주사위 랜드 도형을 나나이트 없이 — 나나이트는 반투명을 못 그림). `ue_fox_hitflash.py /Game/Characters/Fox/M_Fox` → 이미시브 += `HitFlashColor` × `HitFlash`(기본 0); 캐릭터가 맞으면 1 → 0.18초에 사라짐(제곱 × 2.5).
- 확인(임시 `-RRFxTest=giant|fly|aim|hit` — 5초마다 거인/비행 반복, 조준 고정, 0.6초마다 자기 피격; 확인 후 지움): 캡처 `C:\work\char-pipeline\output\screens\fx_giant*`, `fx_fly`, `fx_aim*`, `fx_hit` + `*_sheet.png`(여러 장 한 장으로). `-nullrhi` 80초: TPS 쓰러짐 6·거인 3·비행 3, 탑뷰 8·2·3, 오류 없음.
- 수정 (사용자: 기존 2D 표시는 해골 말고 다 지우기): `ARRRandomHUD` 가 피격 스티커(`T_HitEffects`) 중 해골(폼 핑거 원킬)만 그림 — 총 스티커는 월드 이펙트로 대신. 데미지 숫자는 그대로. 캡처 `output\screens\nosticker*`(교전 중 스티커 없음).
- 수정 (사용자: 바다를 TPS 에서 볼 때도 예쁘게 — 위에서 볼 때만 신경 쓴 것 같음): 섬의 바다(탑뷰 맵을 3인칭으로 볼 때, F9 치트 등).
  - `ue_make_water.py`: 거칠기 0.28 → 파라미터 `Roughness` 0.08(엔진 반사로 하늘이 비침), 이미시브에 프레넬(Schlick, 물결 노멀 기준) × `SkySheen`(0.42, 0.78, 0.95) × `SheenAmount` 0.3 — 비스듬할수록 하늘빛, 먼 바다가 수평선으로 이어짐. 코스틱·반짝임·짧은 물결은 `PixelDepth` 와 `DetailFade` 3500 으로 멀수록 줄이되 비스듬히 볼 때만(카메라 벡터의 z 가 0.3~0.65 아래) — 탑뷰 카메라(약 70° 아래, 거리 약 5000)에선 그대로. 처음엔 거리만 봐서 탑뷰 코스틱이 다 사라질 뻔함. `SheenAmount` 0.6 은 바다가 하늘색으로 하얗게 떠서 0.3.
  - `ARRRandomIsland`: 바닥 밖 바다 평면을 레벨 바닥 경계 + `SeaReach` 80000 까지 — 3인칭에서 수평선에 바다 끝 선(그 너머 대기 지면의 짙은 띠)이 보였음.
  - 확인(임시 `-RRIslandTPS`(3인칭인데 섬) + `-RRLook=Yaw,Pitch`(시선 고정), 지움 — `FParse::Value` 는 기본으로 쉼표에서 끊으니 `false` 필요했음): `output\screens\sea_before3*`(납작한 청록 띠 + 수평선 끝 선) → `sea_after3`(하늘 반사, 수평선까지 이어짐), `sea_after3_high`(내려다보면 해안 코스틱·거품 그대로). `-nullrhi` 탑뷰 60초 정상.
- 추가 (사용자: 머리 위 주사위가 돌 때 반짝임 / 탈것을 걷기보다 확실히 빠르게, 킥보드 < 오토바이 < 자동차 / 자동차로 걷기보다 빠르게 들이받으면 즉사, 그 이하는 속도 비례 데미지):
  - 주사위: `URRRandomOverheadDieComponent` 가 구르는 동안 매 프레임 `ARRRandomEffects::DiceSpinSparkle`(주사위 둘레에서 사탕색 별 초당 65 + 흰 빛 알갱이 12), 착지하는 순간 한 번 `DiceLandBurst`(카메라를 향한 금색 링, 섬광, 별 14).
  - 탈것(`Handlings`, 걷기 300 대비): 킥보드 1.3 → 2.0, 오토바이 1.6 → 2.7, 자동차 1.9 → 3.5. 가속 1800/1500/1300 → 2600/2400/2200, 제동 1600/700/600 → 2200/1100/1000.
  - 자동차 돌진(`ARRRandomVehicle::UpdateRam`, 서버, 자동차만): 운전자 수평 속도 60 이상일 때, 다른 팀 캐릭터의 캡슐이 차체 메시 상자(+15)에 닿고 차가 그쪽으로 가고 있으면(옆·뒤에서 걸어와 닿은 건 제외) — 속도 > 운전자의 기본 걷기 속도(`GetBaseWalkSpeed`, 버프·이벤트 전) 면 최대 체력 × 100 데미지(거인도 즉사), 이하면 최대 체력 × 속도/걷기 + 속도 비례로 밀려남. 같은 상대는 0.8초에 한 번. `ApplyPointDamage`, 데미지 원인은 차, 차의 Instigator = 운전자(탈 때 지정, 내릴 때 해제) → 킬은 운전자 점수.
  - 확인(임시 `-RRRamTest=fast|slow` — 플레이어를 자동차에 태워 앞에 고정한 적에게 돌진 / `-RRDiceTest` — 2초마다 주사위; 지움): fast `rams ... at 981 (walk 300): knockout` → `knocked out by RRRandomCharacter_0`, slow `at 126: 42 damage` 반복 후 쓰러짐. 주사위 캡처 `output\screens\dice_sparkle*`. `-nullrhi` 80초: TPS 쓰러짐 7·탑승 14, 탑뷰 13·8, 오류 없음.
  - 확인 못 한 것: 실제로 몰아 본 속도감(코너링·드리프트가 빨라진 속도에 맞는지), 봇이 자동차로 일부러 들이받지는 않음(AI 는 그대로), 네트워크.
- 추가 (사용자: 승리 화면에서 MVP 주변으로 캐릭터들이 몰려와 대열을 맞춰 춤, 너무 멀거나 3초 넘게 걸리면 오다가 순간이동):
  - `ARRRandomGameMode::ArrangeVictoryFormation`(`EndMatch` 에서 MVP 부활 뒤): MVP 는 제자리(`SetFormationLeader`), 나머지는 쓰러진 사람도 부활시켜 MVP 뒤 V 자 — 둘씩 오른쪽·왼쪽, 한 쌍마다 뒤로 130 · 옆으로 150 더(MVP 팀 먼저, 그다음 진 팀). 자리는 `FindFormationSpot`: MVP 와 같은 바닥 높이(±60, 엄폐물 위 X), 캡슐이 WorldStatic/Dynamic 과 안 겹치고 다른 자리와 120 이상 — 원하는 점에서 바깥으로 70 씩 고리 5바퀴까지.
  - `ARRRandomCharacter::SetFormationSpot`: 탈것에서 내리고 거인·비행 즉시 끝, `FormationSpot`/`bHasFormation`/`bInFormation` 복제. `UpdateFormation`(매 틱): 움직이는 쪽 기기(봇은 서버, 플레이어는 자기 기기)가 그 자리로 걷기 입력. 서버가 45 안이면 도착, 0.8초 걸은 뒤 남은 거리 ÷ 걷기 속도로 3초(`VictoryGatherTime`) 안에 못 오면, 또는 3초가 지나면 순간이동 + `MulticastFormationTeleport`(사라진 곳·나타난 곳에 `ARRRandomEffects::TeleportPoof` — 섬광, 금색 링, 별 16).
  - `IsCelebrating`: 대열이 있으면 도착한 사람만(양 팀 모두) 춤, 없으면 예전처럼 이긴 팀 + MVP 제자리. 결과 카메라 거리 900 → 1100(대열이 들어오게).
  - 처음엔 MVP 바로 뒤에 가로줄로 세워서 정면 카메라에서 MVP 몸에 가려 2~3명만 보임 → V 자로.
  - 확인(임시 `-RRWinAt=초` 로 그때 플레이어 팀 승리; 지움): 캡처 `output\screens\victory_line2_sheet.png` — 이동 중엔 HUD, 결과 화면에서 여우 5~6이 V 자로 카메라 보며 춤. 로그 `5 brawlers line up ...`, 먼 봇은 `teleports into the dance line (1066 away after 0.8 s)`. `-RRScoreToWin=3` 탑뷰 자연 종료: 5명 중 4명 순간이동, 1명은 걸어서 도착.
  - 확인 못 한 것: 걸어오는 모습을 화면으로(이번 판들은 대부분 멀어서 순간이동), 엄폐물 많은 곳에서 자리가 없을 때(로그 `no room in the dance line`, 그 캐릭터는 제자리), 네트워크 참가자 화면.
- 수정 (사용자: 이긴 팀만 모여서 춤, 진 팀은 좌절 모션, Mixamo 로 춤을 몇 개 더 받아 랜덤으로):
  - Mixamo(로그인된 사용자 계정, 기본 캐릭터 `fox_mixamo_upload`): 페이지 안에서 Mixamo API(`/api/v1/products`, `/animations/export`, `/characters/{id}/monitor`)로 스킨 없이 30fps FBX 7개 → `C:\work\char-pipeline\input\fox\mixamo_celebrate\`. 춤: Silly Dancing(Twist), Hip Hop Dancing Shimmy, Dancing The Running Man, Doing The Chicken Dance. 좌절: Defeated(Showing Frustration After A Loss), Crying(And Rubbing Eyes), Sad Idle(Standing In A Sad Disposition). 중간에 Mixamo 세션이 풀려 사용자가 다시 로그인함.
  - `ue_import_fox_anim.py ...|A_Fox_Dance1..4|inplace`, `A_Fox_Defeat1..3` → `/Game/Characters/Fox/Anims/`(모두 시작=끝 엉덩이 위치, 반복 가능; 4.0~12.0초).
  - `ARRRandomCharacter`: `VictoryAnimations`(A_Fox_Victory + Dance1~4), `DefeatAnimations`(Defeat1~3), 복제 `CelebrationPick` 로 각자 하나(`% 개수`). `IsDespairing()` = 경기 끝 + 살아 있음 + 진 팀 + MVP 아님 → 제자리에서 좌절 반복, 총 숨김, 카메라 쪽으로 몸 돌림(춤과 같이).
  - 게임 모드: `ArrangeVictoryFormation` 이 이긴 팀만 V 자 대열(진 팀은 부활·이동 없음), 모든 캐릭터에 무작위 시작 + 순서대로 `CelebrationPick`(옆끼리 다른 춤). MVP 는 이긴 팀 안에서만(킬 → 살아 있음 순) — 진 팀 봇이 MVP 가 되어 이긴 팀이 진 팀 MVP 뒤에서 춤추던 것(캡처 `victory_dances`).
  - 확인(임시 `-RRWinAt` + 애니메이션 로그, 지움): 이긴 팀 Dance2/3/4 — 셋이 서로 다른 춤(캡처 `output\screens\victory_dances2_sheet.png`), 진 팀 Defeat3/1/2. 탑뷰 3점 자연 종료 정상.
  - 확인 못 한 것: 좌절 모션을 화면으로(진 팀은 제자리라 결과 카메라에 안 잡힘 — 로그로만), 네트워크.
- 수정 (사용자: 길찾기가 되면 3초 안에 도착할 속도를 계산해 엄청 빠르게 달려오게, 플래시·소닉 같은 효과, 텔레포트는 걸어서 못 오거나 3초가 지나면):
  - `FRRGridPath`(새, `RRRandomGridPath.h/.cpp`): 내비메시가 없어서(레벨을 런타임에 지음) 직접 A* — 시작~목표를 감싸는 사각형 + 1200 여유, 100 칸, 8방향(대각선은 양옆이 열려 있을 때만). 칸이 열림 = 아래로 바닥(Visibility, 법선 z ≥ 0.7)이 있고 그 위 캡슐이 WorldStatic/WorldDynamic(엄폐물·언덕·소품 상자·물가/가장자리 숨은 벽)과 안 겹침; 이웃 바닥 높이 차 50 넘으면 못 감. 캐릭터들은 무시. 칸은 필요할 때만 검사, 4만 칸 넘으면 포기. 결과는 캡슐 스윕으로 모서리를 건너뛰어 펴서 꺾이는 점만. 시작이 공중이면(비행 중 경기 끝) 아래 바닥 기준 — 처음엔 공중 높이를 그대로 써서 옆 칸과 490 차이 → "길 없음" 으로 텔레포트됐음.
  - `SetFormationSpot`(서버): 길이 있으면 `FormationPath`(복제) + `FormationDashSpeed` = 길이 / `FormationDashTime`(처음 2.5초 → 사용자 요청으로 1.5초 안 도착, 출발·모서리 여유로 1.3초), 걷기 × 2.5 ~ 6000 사이(가까워도 번개처럼). 길이 없으면 바로 순간이동. `UpdateFormation`: 움직이는 기기가 다음 꺾임점으로 입력(속도에 비례한 거리 안이면 다음 점), 서버가 도착(45 또는 한 프레임 이동량 × 1.2) 판정, 3초가 지나면 순간이동. 달리는 동안 `MaxWalkSpeed` = 대시 속도, 가속·제동 30000, 회전 3000, 이동 방향으로 몸 돌림(3인칭 컨트롤러 회전 끔).
  - 연출(`UpdateDashEffects`, 모든 기기): 잔상 — `UPoseableMeshComponent` 8개를 돌려 쓰며 0.035초마다 몸 자세 복사, `M_FXGhost`(새: 언릿 반투명, 프레넬 가장자리 발광, `Color`/`Opacity`, 스켈레탈용; `ue_make_fx.py` 에 추가)로 0.3초에 사라짐(금색 + 팀색). `ARRRandomEffects::SpeedTrail`: 진행 방향으로 늘린 빛줄기 초당 70, 튀는 전기 불꽃 55, 발밑 먼지 30. `SonicBoom`: 출발·도착 때 진행 방향을 축으로 선 링 두 개(금색, 팀색) + 섬광 + 불꽃 18.
  - 확인(임시 `-RRWinAt` + 길찾기 실패 로그, 지움): 화면 없이 TPS/탑뷰 여러 판 — `dashes to the dance line: N corners, L long, at S` → `arrives ... after 0.2~2.5 s`, 순간이동 없음. 캡처 `output\screens\dash3_sheet.png`(빛줄기·금색 잔상으로 달려와 도착 링, 이어서 대열 춤). 탑뷰 3점 자연 종료 정상.
  - 확인 못 한 것: 3초 안에 못 와서 순간이동하는 경우(이번 판들은 모두 도착), 네트워크 참가자(플레이어 폰은 자기 기기가 경로를 따라감 — 서버 이동 검증과 맞는지 미확인), 아주 높은 속도(이번엔 대부분 750~1100)에서 모서리 끼임.
- 확인 못 한 것: 실제 마우스로 조준해 본 느낌, 탑뷰 시점에서 이펙트 크기(같은 이펙트가 나옴), 네트워크 참가자 화면, 많은 효과가 겹칠 때 프레임(풀 크기: 구 500, 링 80, 별 500, 주사위 120 — 넘치면 새 파티클 생략).

## 21차 — TPS 전용 맵 "주사위 랜드" (사용자: 파스텔 주사위 랜드 컨셉 → 색 진하게 + 엄폐물 많이 → Tripo 3D 로 만들어 TPS 시작 시 나오게)

- 21~28차와 함께 한 커밋에 포함(브랜치 `claude/project-thread-ykg56g`).
- 컨셉: Tripo 웹 이미지 생성(Nano Banana Pro, 16:9, 무료 할당)으로 5장씩 두 번(전경/TPS 어깨 너머/주사위 숲/하늘 섬/노을 폭포). 첫 판은 "물 빠짐" → 두 번째 판 "vibrant saturated candy colors ... not washed out" + 엄폐물(주사위 담, 바리케이드, 반쯤 묻힌 큰 주사위, 덤불, 기둥). 다운로드는 안 함(Tripo 기록에만). 같은 장면의 3D 스케치 아티팩트: https://claude.ai/artifact/43gnXuFWhYp7MeMM6o2xZm
- Tripo 3D(Claude in Chrome 으로 studio.tripo3d.ai, 텍스트 → HD Model H3.1, 1회 30, 23,700 → 23,490 = 7개; 주사위 담은 뒤에 판 조각이 떠 있어 무료 재시도): 주사위 나무 A(분홍·보라·주황) / B(민트·하늘·노랑, 2층), 주사위 담(5×3), 둥근 덤불, 광장 받침대(보라·분홍 2단), 주사위 무지개 아치(양끝 구름), 구름. 프롬프트마다 "cute stylized cartoon ... chunky rounded balloon-like shapes, glossy soft clay look, vibrant saturated candy colors". 사용자 허락 후 FBX(Blender, 2K) → `C:\work\char-pipeline\input\dice_land\<treeA|treeB|dicewall|hedge|pedestal|rainbow|cloud>`.
  - `blender_props.py` → `output\dice_land_props\SM_DL*.fbx`(나무 A 560h, B 640h, 덤불 150h, 담 165h(275 폭), 받침대 560w(높이 217), 무지개 1000w, 구름 600w) → `ue_import_props.py` 에 `=gloss` 옵션 추가(Tint 1, 거칠기 0.4, 스페큘러 0.5 — 사탕 광택) → `/Game/Environment/DiceLand/SM_DL*`, `M_DL*`.
  - 주의: props 스크립트는 머티리얼 이름을 `SM_X` → `M_X` 로 만들므로 `SM_DLRainbow` 가 `M_DLRainbow` 를 덮어씀 → 무지개 주사위 머티리얼은 `M_DLRainbowDie`.
- 절차 메시: `C:\work\char-pipeline\scriptslender_dice_land.py <out>` → `output\dice_land\SM_DLDie`(초타원체 p=6, 100 크기, 슬롯 Body/Pips, 마주 보는 면 합 7), `SM_DLDieBlank`(눈 없음), `SM_DLRainbowDie`(면마다 버텍스 컬러), `SM_DLTile`(쿠션 타일 200×200×50, 피벗은 윗면 가운데). `ue_import_dice_land.py <dir>` → 같은 폴더에 메시(충돌 없음, 나나이트) + `M_DLCandy`(인스턴스 커스텀 데이터 0~2 = 선형 색, 거칠기 0.35), `MI_DLTile`(거칠기 0.6), `M_DLRainbowDie`(버텍스 컬러), `M_DLPip`.
- `ARRRandomDiceLand`(새, 복제, 모든 기기가 같은 시드로 같게 지음; 에셋은 BeginPlay 에서 `LoadObject` — 생성자 `ConstructorHelpers` 로 하면 에셋이 없을 때 커맨드렛이 CDO 오류로 종료됨):
  - 게임 모드 `StartPlay`: `bSpawnDiceLand` + `ARRRandomGameState::IsThirdPersonMode` 면 섬·잔디·기본 엄폐물 대신 이것만. 탑뷰는 그대로(확인: 로그 `island 36 x 36 ...`).
  - 바닥: 레벨 바닥 위(보이지 않게 숨김, 충돌은 그대로 — 밟는 건 레벨 바닥), 흔들리는 원(반지름 약 3050) 안에 쿠션 타일(체크, 잔디엔 가끔 라임/레몬/분홍), 타일 아래 어두운 판(이음매가 구멍 대신 줄눈으로). 길: 가운데 큰길(X), 팀 출발 줄(X ±1530 = `TeamSpawnDistance`/2), 양옆 차선(Y ±1800 — 게임 모드가 차를 세우는 -1890/1710 이 들어감), 폭 360. 광장 반지름 560, 테 720.
  - 광장: Tripo 받침대(충돌 상자) 위에 무지개 주사위(2.6 배)가 돌며 떠 있음(Tick), 둘레에 작은 주사위 28개(길 쪽은 비움).
  - 필드 엄폐물은 점대칭(파랑 쪽에 손으로 놓고 반 바퀴 돌려 빨강 쪽): 덤불 2, 주사위 담 4(+ 가운데 줄 1쌍), 큰 주사위 4, 바리케이드(주사위 2 + 위 1) 2. 바깥: 왼쪽 먼 끝/오른쪽 가까운 끝은 주사위 숲(나무·덤불·주사위), 반대 끝은 거대 주사위 들판, 팀 뒤뜰에 나무·덤불·작은 주사위 — `FRandomStream` 으로 뽑되 점과 그 반대편이 모두 비었을 때만(`IsClear`: 섬 안 `RimMargin` 170, 광장·길·출발 줄 밖, 서로 70 간격).
  - 막는 것: 모두 숨긴 큐브(`BlockAll`, 섬과 같음) — 담/덤불/받침대는 메시 경계 × 비율, 나무는 줄기만, 주사위는 0.94 배. 가장자리: 높이 6000 숨은 벽 160조각(캐릭터만 막음, 비행해도 못 넘음), 그 안쪽에 작은 주사위 줄(보기만).
  - 하늘: 섬 아래 주사위 원뿔(8단, 주황 → 분홍 → 보라, 14% 는 눈 있음), Tripo 구름 바다 240개(멀수록 크고 높게 — 32,000 까지, 먼 것은 바닥보다 조금 위까지 올라와 수평선을 덮음), 높은 하늘의 작은 구름 10, 양 끝 너머 무지개 아치 2(긴 쪽을 가로로, 발은 구름 속), 필드 위에 떠서 출렁이며 도는 무지개 주사위 26(Tick 에서 `BatchUpdateInstancesTransforms`), 닿지 않는 하늘 섬 3(주사위 탑 / 나무 / 나무 + 덤불).
  - 색 보정: 바운드 없는 `APostProcessVolume`(노출 -0.35, 대비 1.1, 미드톤·하이라이트 채도 1.3) — 카메라가 전체 채도(1.08)를 덮어쓰므로 그것 외의 항목만. 이게 없으면 밝은 하늘 탓에 사탕색이 파스텔로 빠짐.
  - 미니맵: `ARRRandomHUD::BuildMinimapBase` 가 섬이 없으면 `GetMapColor`(가장자리 밖 하늘색, 놓인 것들의 발자국 상자, 그 외 바닥색).
  - 로그 `RRRandom: dice land 700 tiles, 1151 dice, 87 solid boxes, 14 trees, 17 hedges, 10 walls.`
- 확인: 빌드 경고 없음. 창 캡처 `C:\work\char-pipeline\output\screens\diceland1~5_*`(TPS 시작 시점, 위에서 전경, 숲, 필드 — 임시 `-RRLandCam=X,Y,Z,Pitch,Yaw` 고정 카메라로 찍고 지움). `-nullrhi` 80초 봇전: TPS 쓰러짐 2번, 탑뷰 6번.
- 수정 (사용자: 색을 전체적으로 파스텔로, "이전 게임 색감이 더 좋았어"; 오브젝트 퀄리티가 낮으니 Claude 가 그린 주사위 랜드 3D 스케치처럼 깔끔하게):
  - 색 보정 볼륨(노출/대비/채도) 뺌 → 게임 원래 색감. 팔레트를 마카롱 파스텔로(분홍 255,143,177 / 복숭아 / 레몬 / 민트 / 하늘 / 페리윙클 / 라벤더; 바닥 민트 150,225,180 등). 처음엔 스케치 값 그대로(거의 흰색에 가까운 파스텔)를 넣었더니 밝은 하늘 아래 하얗게 날아가서 한 단계 진하게.
  - Tripo 소품(나무/담/덤불/받침대/무지개/구름)은 더 안 씀(에셋과 `input\dice_land` 는 남음) — 울퉁불퉁한 표면 + 6000 삼각형 감축으로 음영이 깨져 보였음. 대신 스케치처럼 매끈한 기본 도형을 조합: `blender_dice_land.py` 에 `SM_DLBall`(구), `SM_DLTrunk`(위로 가늘어지고 윗모서리 둥근 줄기), `SM_DLRing`(얇은 링) 추가, 주사위 해상도 10 → 18 분할, 눈 32×12(주사위 18,672 삼각형, 나나이트), 무지개 주사위는 다각형 중심 기준으로 면 색을 칠해 모서리에서 색이 번지지 않게, 파스텔로.
  - `ue_import_dice_land.py`: `M_DLCandy` 에 "Glow"(색 × 값 → 이미시브), `MI_DLCloud`(거칠기 0.85, Glow 0.2), `M_DLPip` 은 커스텀 데이터 3~5 로 눈 색(없으면 흰색) → 흰 주사위에 파스텔 눈, 색 주사위에 짙은 눈.
  - `ARRRandomDiceLand` 다시: 나무 = 줄기 + 주사위 잎(하나 / 둘 쌓기 / 셋 묶음 + 위 하나), 덤불 = 공 5개 + 위 3개 + 작은 꽃 공(담 높이 약 143), 주사위 담 = 78 주사위 4×2(무지개 순서, 약간씩 틀어짐, 높이 156), 받침대 = 둥근 판 두 장(라벤더 500, 분홍 350, 45° 돌림) 위에 무지개 주사위 + 흰색·라일락 링 두 개가 반대로 돎, 구름 = 공 4~7개 더미(바다 170 더미 + 하늘 12 더미), 무지개 = 주사위 7줄 반원(앞쪽 반지름 7600, 뒤쪽 6800), 풀밭에 작은 덤불과 꽃 공 220개. 배치(점대칭, 바깥 숲/거대 주사위/뒤뜰)는 그대로.
  - 로그 `dice land 700 tiles, 2173 dice, 474 balls, 988 cloud balls, 13 trunks, 88 solid boxes.` `-nullrhi` 80초: TPS 쓰러짐 4번, 탑뷰 12번(탑뷰 섬 그대로). 캡처 `output\screens\diceland6~8_*`.
- 수정 (사용자: 배경 오브젝트 색만 조금 더 진하게): `Richer()`(채도 × 1.35 + 0.06, 밝기 × 0.95)를 무지개 아치, 가장자리 주사위 줄, 하늘 섬(타일·탑·덤불·주사위)에만 적용, 섬 아래 원뿔 색을 진하게(255,165,115 → 255,130,165 → 160,125,255), 구름 색조를 조금 더 분홍·라일락·하늘빛으로. 필드(바닥·엄폐물·나무)는 그대로. 캡처 `output\screens\diceland9_tps`.
- 수정 (사용자: Claude 주사위 랜드 색감과 비슷하되 살짝 진하게, 캐릭터도 어울리는 톤으로 조금):
  - 맵 전체: `AddColored` 가 모든 색을 `Toned()`(채도 × 1.12 + 0.03, 밝기 × 0.97)로 한 번 거침(흰 눈은 제외). 배경의 `Richer()` 는 그 위에 그대로.
  - 여우: `C:\work\char-pipeline\scripts\ue_fox_pastel.py /Game/Characters/Fox/M_Fox` — 베이스 컬러 뒤에 `lerp(색, lerp(desaturate(색, PastelDesaturate 0.12), PastelCream (1, 0.92, 0.86), PastelLift 0.07), Pastel)`. "Pastel" 기본 0(탑뷰는 그대로), `ARRRandomDiceLand::Tick` 이 1초마다 모든 캐릭터 몸 MID 에 1 → 부드러운 살구색 털. 처음엔 0.28 / 0.2 로 했더니 선형 색에서 크림을 섞어서 털이 회색빛 베이지가 됨. 다시 실행하면 이전 노드는 연결 없이 그래프에 남음(셰이더엔 안 들어감, 경고 없음). 총 등 다른 머티리얼은 그대로.
  - 캡처 `output\screens\diceland10_tps`(0.28 / 0.2, 너무 빠짐), `diceland11_tps`(0.12 / 0.07).
  - 수정 (사용자: 조금 더 주황): Dice Land 가 `PastelDesaturate` 0.05, `PastelLift` 0.03 도 함께 넘김(`FoxDesaturate`, `FoxCreamLift` — 머티리얼 기본값 0.12 / 0.07 은 그대로). 캡처 `diceland12_tps`.
- 수정 (사용자: 맵도 살짝 더 진하게, 보면서 조절할 수 있게 값을 따로): 콘솔 변수(게임 중 `~` 콘솔에서 바로 반영) `rr.DiceLand.Saturation`(1 = 만든 그대로, 기본 1.15), `rr.DiceLand.Brightness`(1), `rr.DiceLand.FoxDesaturate`(0.05), `rr.DiceLand.FoxCream`(0.03). 기본값은 `Config/DefaultEngine.ini` `[SystemSettings]`(로그에 `deferred - dummy variable created` 가 뜨지만 모듈이 변수를 등록할 때 값이 넘어감).
  - `ue_import_dice_land.py` 의 `tone()`: 네 머티리얼(Candy/RainbowDie/Pip, 타일·구름 인스턴스 포함) 베이스 컬러를 `max(0, lerp(회색, 색, Saturation) × Brightness)` 로. `ARRRandomDiceLand` 가 다 지은 뒤 자기 메시 컴포넌트의 머티리얼을 전부 MID 로 바꿔 `Tunable` 에 모으고, Tick 에서 값이 바뀌면 모두에 넣음. 여우 값은 0.25초마다.
  - 확인: 기본(1.15) `output\screens\diceland13_default`, `-dpcvars=rr.DiceLand.Saturation=1.6` `diceland13_sat16`(확연히 진해짐). 콘솔에 직접 쳐서 바꾸는 건 캡처로 못 해 봄(같은 Tick 경로).
- 확인 못 한 것: 실제 마우스로 TPS 플레이 감각(엄폐물 높이 약 145~160 이 적당한지), 봇이 숲·담에 걸리는 빈도(로그에 `is stuck and failed to move` 가 가끔), 네트워크 참가자, 프레임 비용(타일 700 + 주사위 2,170 + 공 1,460, 나나이트), 비행으로 하늘 섬까지 가는지(가장자리 벽에 막혀야 함).

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
- 수정 (사용자: 안개 계단 현상 없애기 + 구름이 걷히는 느낌, 미니맵은 반투명에 얇은 프레임):
  - 계단 원인(격자를 PNG 로 덤프해 확인, 임시 코드는 지움): ① 그림자 **옆** 경계 — 이웃 광선 두 개만 보간해서 엄폐물 뒤 그늘의 양옆이 한 칸 폭으로 딱 끊김, 그게 '가 봤던 곳'에 그대로 쌓임 ② 128 격자(칸 62.5) ③ 머티리얼이 그 격자를 쌍선형 한 번으로만 읽음.
  - C++: `Resolution` 128 → **256**(칸 31). `Stamp` 의 그늘은 `ShadowSoftness` 80(월드 단위, 칸 크기와 무관)에 걸쳐 앞뒤로, 그리고 옆으로도 그 폭만큼 퍼진 다섯 방향(텐트 1-2-3-2-1)을 섞음. `EdgeSoftness` 200 → 300(구름 경계가 흔들릴 폭; 반쯤 보임 = 판정 경계는 그대로 `VisionRadius`). 원 밖 칸은 건너뜀. `GetSightAt` 은 네 칸 쌍선형(미니맵도 매끈하게), 가 봤는지는 > 0.5. 머티리얼에 `FogResolution` 넘김.
  - 머티리얼 `ue_make_fog.py` 를 **Custom(HLSL) 노드 하나**로 다시: 칸 중심 4×4 를 하나씩 읽어 3차 B-스플라인(텍스처 필터와 무관 — 텐트 블러 + 쌍선형으로는 칸 크기 네모가 남았음), 값 노이즈 fbm 으로 조회 위치를 흔들고(`WarpAmount` 70), 같은 노이즈로 보임/가 봤음 경계의 문턱을 움직여(`EdgeNoise` 0.6, `EdgeWidth` 0.35) 뭉게뭉게 걷힘. 안 가 본 곳엔 큰 구름(`CloudScale` 700, `CloudSpeed` 0.05/s, `CloudBrightness` 0.16 × `FogColor`), 가 봤던 곳엔 그 35% 옅은 안개, 걷히는 가장자리는 살짝 밝게(`RimBrightness` 0.12). 텍스처는 sRGB 로 읽히므로 `pow(1/2.2)` 로 게임 값으로 되돌림. 실행: `UnrealEditor-Cmd RRRandom.uproject -run=pythonscript "-script=C:/work/char-pipeline/scripts/ue_make_fog.py /Game/PostProcess/M_PP_Fog"`(경로는 슬래시로 — 역슬래시는 `\u` 가 이스케이프로 먹혀 파일을 못 찾음).
  - 미니맵(`DrawMinimap`): 3px 검은 판을 없애고 지도 그림을 `BLEND_Translucent` 로 70%(전체 화면 지도는 90%), 둘레에 1px 흰색 45% 테두리(`MapFrameColor`/`MapFrameWidth`).
  - 확인 (창 캡처 — 사용자가 PC 를 쓰는 중이라 화면 복사 대신 `PrintWindow`, 키는 `PostMessage`): 격자 덤프에서 그늘 옆 경계가 부드러움, 화면에서 시야/그늘/가 봤던 곳 경계가 구름처럼 매끈(칸 계단 없음), 미니맵 아래로 게임이 비침 + 얇은 테두리. F6 결과 화면도 그대로(안개 걷힘, 이번 판은 거인 MVP 카메라 1800). 빌드 경고 없음, 셰이더 컴파일 오류 없음.
  - 재수정 (사용자 스크린샷: 여전히 안개 속 직선 경계·각진 면): 디버그 화면(마스크만, 조회 좌표 frac, 업로드 격자 덤프)으로 원인 셋을 확인해 고침.
    ① 머티리얼 노이즈가 값 노이즈(칸마다 네모 모양) → **그래디언트(Perlin) 노이즈** + 옥타브마다 회전. ② 조회 위치가 실제 표면이라 Tripo 블록의 울퉁불퉁한 윗면(블록마다 높이 다름, 면 사이 주름)에서 좌표가 직선으로 튐 → 시선이 **평평한 단 높이**(`GroundZ` = 바닥 높이, `FogOrigin` 의 B 채널로 넘김, `LevelHeight` 80 단위로 스냅, 단 ±45% 안은 평평)와 만나는 점으로 조회. ③ 보임/가 봤음 = 여러 원뿔의 max 라 만나는 곳에 직선 주름이 생기고 경계를 날카롭게 하는 머티리얼이 그걸 선으로 보여 줌 → **업로드 직전 격자에 작은 블러**(`UploadBlurPasses` 2, 1-4-6-4-1 가로·세로, 약 2칸; 게임 판정 `GetSightAt` 은 원래 격자). 덤으로 `SightRays` 240, `ShadowSoftness` 120, 머티리얼 `EdgeNoise` 0.5 / `EdgeWidth` 0.65(덜 날카롭게).
    - 확인: 마스크 전용 화면에서 직선·면 사라짐, 조회 좌표가 매끈한 격자, 평소 화면 경계 매끈. 빌드 경고 없음. 임시 디버그 코드/에셋(`M_PP_FogDebug`) 지움.
  - 재재수정 (사용자 스크린샷: 엄폐물 그늘 옆으로 플레이어 쪽에 모이는 선 여러 줄 + 어둠 속 삼각형 면): 그늘 옆 경계를 방향 5개(±2)로만 섞어서 날카로운 경계가 5단 계단(부채꼴 선)으로 남았고, 그게 '가 봤던 곳'에 삼각형으로 쌓였음. `Stamp` 의 `OpenAt`: 방향 13개(`ShadowTaps` 6, 텐트 가중), 간격이 업로드 블러보다 좁게. 반그림자 폭 = `ShadowSoftness` + 장애물 뒤 거리 × `ShadowSpread` 0.4(멀수록 넓게 퍼짐). 확인: 엄폐물 그늘 가장자리가 부드럽게 퍼지고 선/면 없음. 빌드 경고 없음.
  - 남은 것: '가 봤던 곳'(회색) 안에서 바닥 잔디 타일·모래 판의 명암 차이가 옅은 큰 사각형으로 보임 — 안개 칸보다 훨씬 커서 장면 자체로 보임. 비용: 프레임당 팀원마다 칸 ~3,000 × 5 방향, 화면 픽셀마다 텍스처 16번 + 노이즈 — 느린 PC 에서는 확인 안 함.

## 16차 — 타이틀 그림 (사용자: 트라이포로 여우가 연기 나는 총구를 불고, 머리 위에서 주사위가 번쩍이며 도는 모습을 만들어 타이틀에)

- 아직 커밋 안 됨.
- 그림: Claude in Chrome 으로 studio.tripo3d.ai 이미지 생성(Generate Image) → Nano Banana Pro, 16:9, 2장씩(무료 할당, 크레딧 0). 첫 프롬프트는 가짜 게임 제목/버튼이 그려지거나 총을 가슴 높이로 들어서, 두 번째 프롬프트("no text, no logo, no buttons…", 총을 얼굴 앞에, 입을 오므려 총구 연기를 부는, 머리 위 무지개 주사위가 반짝이며 도는, 가운데·왼쪽 비움)의 두 번째 그림을 골라 사용자 허락 후 다운로드(`image_1.jpeg` 592KB, 1376×768) → `C:\work\char-pipeline\input\title\title_art.jpeg`.
- 연출 소스(모두 `C:\work\char-pipeline\scripts`):
  - `blender_dice_spin_sheet.py`: `output\dice\SM_Dice.fbx` + Tripo 무지개 텍스처(조금 자체 발광), 흰 눈, 기울어진 축으로 한 바퀴 24프레임, 160px, 6×4 → `output\ui\T_TitleDiceSpin.png`(정육면체라 이음새 없이 반복).
  - `make_title_fx_textures.py`(numpy, PNG 직접 씀): `T_TitleGlow`(둥근 빛), `T_TitleSparkle`(네 갈래 별) — 더하기 블렌드는 알파를 무시하므로 **미리 곱한 색(검정 바탕)**, `T_TitlePuff`(연기, 흰색 + 알파). 처음엔 흰색 + 알파로 만들어 더하기로 그렸더니 흰 네모가 화면을 덮음.
  - `ue_import_ui_texture.py`(이제 여러 장: `<폴더>|<이미지>=<이름>|...`) → `/Game/UI/T_TitleArt`, `T_TitleDiceSpin`, `T_TitleGlow`, `T_TitleSparkle`, `T_TitlePuff`(UI 그룹, 밉 없음).
- `ARRRandomTitleHUD`: 그림을 화면을 덮게(긴 쪽을 고르게 잘라) 그리고, 그림 픽셀 좌표 → 화면(`ArtToScreen`)으로 연출을 얹음:
  - 주사위: 그림 속 주사위 자리(1186,116)에 그림 픽셀 178 크기로 시트 프레임(초당 16) — 그림의 주사위를 덮음. 뒤에 빛(330, 1.3Hz 맥동 + 2.6초마다 0.35초 번쩍), 둘레 6곳에 반짝임(각자 리듬으로 깜빡이며 천천히 돎).
  - 연기: 총구(921,314)에서 퍼프 9개가 3.2초 동안 위로 48/s 오르며 흔들리고 12→64 로 커지며 옅어짐(알파 0.32), 그림 속 연기와 겹쳐 보임.
  - 메뉴: 제목·버튼·설명·키 안내를 화면 폭 27% 를 가운데로 한 왼쪽 세로 단, 왼쪽 58% 를 위로 갈수록 0 이 되는 검정 그라데이션(32줄, 픽셀 경계 딱 맞춤 — 1px 겹치게 했더니 세로 줄무늬가 보였음). 그림이 없으면 예전 배경색.
- 확인 (창 캡처 `C:\work\char-pipeline\output\screens\title2`, 0.3초 간격 4장 + 주사위 부분 `die_frames.png`): 프레임마다 주사위 각도 다름, 빛/반짝임 크기 변함, 총구 연기, 버튼/글자 읽힘. 빌드 경고 없음.
- 확인 못 한 것: 16:9 가 아닌 창(좌우/위아래 잘림은 계산상 맞음), NETWORK 페이지·상태 글자 표시.
- 추가 (사용자: 타이틀의 여우도 살짝 움직이게): 퍼펫 워프(Live2D 식). `ARRRandomTitleHUD::DrawFox` — 그림 전체를 그린 뒤 여우 둘레 패치(그림 픽셀 860~1376 × 150~768, 칠한 주사위 아래)를 12px 격자 삼각형(`FCanvasTriangleItem`, 불투명, ~4,400개)으로 한 번 더 그림: 텍스처 좌표는 제자리, 화면 위치는 `FoxNudge` 만큼 밂. 패치 가장자리 30px 에서 0 으로 줄어 밑의 그림과 딱 맞음(이중 그림 없음).
  - 움직임: 숨쉬기(여우 둘레 타원 안 — 머리·귀·총 포함, 발 0 → 머리 꼭대기 4px, 3.4초), 머리 기울기(목 축 ±1.6°, 5.1초), 꼬리(뿌리 축 ±4°, 1.9초, 뒤 언덕이 따라 돌지 않게 범위 좁게), 오른쪽 귀 까딱(4.3초마다 0.3초, 7°). 총구 연기 시작점도 `FoxNudge(MuzzleSpot)` 로 따라감.
  - 확인 (타이틀 창 캡처 0.85초 간격, 0초/1.7초 차이 이미지): 머리·귀·총·상체·꼬리 윤곽만 움직이고 발·다리·왼쪽 그림은 그대로, 이음새/찢어짐 없음. 빌드 경고 없음.
  - 한계: 그림 한 장을 비트는 방식이라 그림에 없는 동작(눈 깜빡임 등)은 못 함. 숨쉬기 타원이 넓어서 여우 오른쪽 뒤 언덕 가장자리도 1~2px 같이 오르내림(좁히면 귀·머리가 따로 늘어남).
- 수정 (사용자: 꼬리 쪽 왜곡이 신경 쓰임 → 트라이포로 캐릭터와 배경을 나눠 더 자연스럽게): **레이어로 바꿈** — 위 한계(배경이 같이 휨) 해결.
  - Tripo 이미지 편집(Claude in Chrome 으로 studio.tripo3d.ai → Image → Nano Banana Pro, 원본 `title_art.jpeg` 첨부, 16:9, 무료 할당): ① "여우·총·총구 불꽃·연기를 지우고 뒤 풍경(선착장, 물, 언덕, 하늘)으로 채움, 주사위는 그대로" ② "여우만 그대로 두고 나머지는 #00FF00 단색". 각 2장 중 왼쪽 것을 사용자 허락 후 다운로드 → `C:\work\char-pipeline\input\title\tripo_background.png`, `tripo_fox_green.png`(둘 다 1376×768, 원본과 같은 자리 — ②의 오른쪽은 선착장이 남아서 안 씀).
  - `C:\work\char-pipeline\scripts\make_title_layers.py`(Blender 의 Python 으로: `blender -b --python make_title_layers.py -- <초록 여우> <배경> <출력 폴더>`) → `output\title_layers\T_TitleBack/T_TitleFoxBody/T_TitleFoxTail.png`(원본 크기 그대로라 그림 좌표 공유):
    - 키: 초록 초과량(G − max(R,B)) 0.10~0.40 → 알파, 여우 칸(860~1376 × 140~720) 밖은 0, 알파 0.3 조이고 2px 침식 + 1px 흐림(털과 초록이 섞인 노란 테두리는 키로 안 잡혀서), 반투명 가장자리 색은 안쪽 불투명 털 색으로 4px 채움, 초록 > max(R,B) 금지. 투명 픽셀 RGB 도 6px 까지 가까운 여우 색(필터링 때 초록/검정이 안 섞이게). 처음엔 언믹스(초록 몫 빼기)만 했다가 연두 테두리가 남았고, 꼬리 색을 알파 > 0.5 로만 남겨서 검은 테두리가 생긴 것도 고침.
    - 꼬리: 팔·총 개머리판·반바지 윤곽을 따라 다각형(`TAIL_POLYGON`)으로 잘라 따로, 몸 쪽으로 16px 꼬리 색을 늘려 둠(돌 때 뿌리에 틈 안 생김, 몸이 덮음).
  - 가져오기: `ue_import_ui_texture.py` → `/Game/UI/T_TitleBack`, `T_TitleFoxBody`, `T_TitleFoxTail`.
  - `ARRRandomTitleHUD`: 세 장이 있으면 배경(불투명) → 꼬리(`TailBox` 1190~1376 × 420~625 부분만, `TailPivot`(1222,578) 축으로 통째 회전 ±6°, 2.2초 + 두 배 빠른 25% 흔들림, 뿌리는 숨쉬기를 따라 이동) → 몸(격자 워프, 반투명; 숨쉬기 4px·머리 ±1.6°·귀 까딱 — 여우만 있는 레이어라 배경은 안 휨, 그래서 가장자리 페이드/몸 타원 없앰). 없으면 예전 `T_TitleArt` 한 장을 움직임 없이.
  - 확인 (타이틀 창 캡처, 차이 이미지): 배경 완전히 고정, 여우(머리·귀·총·꼬리)와 주사위·연기만 움직임, 꼬리는 휘지 않고 통째로 흔들림, 뿌리 틈 없음, 머리·꼬리 윤곽에 초록/검은 테두리 없음. 빌드 경고 없음.
  - 추가 (사용자: 타이틀 바다에 광택 + 흐르는 효과): `make_title_layers.py` 가 `T_TitleWater.png`(배경 색 + 바다 알파: min(G,B)/R 1.06~1.18, 수평선 y570 위는 0, 가장자리 3×3 두 번 부드럽게 — 밝은 물 면은 R 이 높아 처음 기준(B−R)에선 빠졌음)도 만듦 → `/Game/UI/T_TitleWater`. `C:\work\char-pipeline\scripts\ue_make_title_water.py` → `/Game/UI/M_TitleWater`(UI 도메인, 반투명, Custom HLSL 하나): 바다에서만 그림을 그래디언트 노이즈로 살짝 흔들고(`RippleAmount` 2.5px, 아래쪽일수록 큰 물결, `FlowSpeed` 0.35 로 오른쪽으로 흐름; 흔든 곳이 바다가 아니면 원래 색), 물결 마루를 따라 가로로 긴 광택(`SheenAmount` 0.22)과 깜빡이는 작은 반짝임(`SparkleAmount` 0.45), 수평선 쪽이 더 밝음. 캔버스가 감마 색으로 받으므로 출력에 pow(1/2.2)(안 하면 바다가 진하게 나옴). `ARRRandomTitleHUD` 가 배경 다음, 여우 전에 `DrawMaterial` 로 그림 전체 위에.
    - 확인 (타이틀 캡처 0.85초 간격): 물 색 원래와 같음, 광택·반짝임이 프레임마다 흐름, 선착장·모래·언덕은 그대로. 빌드 경고 없음.
  - 남은 것: 꼬리가 몸에서 멀어지는 쪽으로 돌 때 개머리판과 꼬리 사이로 배경이 조금 보임(실제로 꼬리가 비켜난 자리라 자연스러움). 몸 레이어는 Tripo 가 다시 그린 여우라 원본과 아주 미세하게 다를 수 있음.
  - 참고: 캡처 스크립트의 `Stop-Process`(Start-Process 가 준 Id)가 게임을 바로 못 닫는 경우가 있어, 남은 창이 DLL 을 잡아 다음 빌드 링크가 실패(LNK1104)한 적 있음 — 빌드 전에 `UnrealEditor.exe` 가 남아 있는지 확인.

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

## 17차 — 탈것 (사용자: 트라이포로 킥보드·오토바이·자동차를 만들어 탈 수 있게. 킥보드는 조금 빠르게, 오토바이·자동차 순으로 더 빠르게, 오토바이와 자동차는 드리프트)

- 아직 커밋 안 됨.
- 모델(Claude in Chrome 으로 studio.tripo3d.ai, 텍스트 → HD Model, 1회 30 크레딧, 24,780 → 24,630 = 5회): 킥보드(주황 갑판), 오토바이(빨강), 자동차 — 처음 자동차는 지붕 있는 소형차라 사용자 요청으로 "둥근 오픈카, 얼굴이 보이게" 다시, 또 "더 알록달록하고 신형으로" 다시: `stylized cartoon modern electric sports convertible ... open top with no roof and no windshield frame, very low sides so a seated driver's head and shoulders stay visible ... candy turquoise body with bright magenta and orange racing stripes ...`. 3개를 사용자 허락 후 FBX(Blender, 2K) 다운로드 → `C:\work\char-pipeline\input\vehicles\{kickboard,motorbike,car}`(안 쓰는 노란 차 2개는 안 받음).
- `C:\work\char-pipeline\scripts\blender_vehicles.py <out> 이름=fbx=길이[=추가회전]`: 합치기, 긴 쪽을 X 로(오토바이·자동차 90° 돌림), 바닥 가운데 피벗, 길이 맞춤(여우 키 192 기준 킥보드 140 / 오토바이 220 / 자동차 340), 8000 삼각형으로 줄임, 옆·게임 카메라 프리뷰. 결과 140×96×115, 220×120×140, 340×161×101, 셋 다 앞이 +X. 처음엔 `bound_box` 가 메시 직접 회전 뒤 갱신이 안 돼서 크기가 틀렸음(오토바이 405) → 정점으로 경계 계산.
- `ue_import_props.py`(소품과 같은 방식: 충돌 없음, 텍스처 × Tint, 거칠기 1) → `/Game/Vehicles/SM_Kickboard|SM_Motorbike|SM_Car`, `M_*`, `Textures/T_*_BaseColor`.
- `ARRRandomVehicle`(복제: `Kind`, `Rider`): 루트 씬 + `Body` 메시(충돌 없음, 커스텀 뎁스 = 외곽선). `Mount` = 탑승자 캡슐 바닥에 붙임, `Dismount` = 발 위치에 내려놓고 탑승자 방향. Tick 에서 드리프트 각(속도 방향 − 바라보는 방향, 8° 이상·150 이상 속도)만큼 `Body` 롤(오토바이 ×0.7 최대 28°, 자동차 ×0.15 최대 6°).
  - 핸들링(`FRRVehicleHandling`, 캐릭터 이동에 매 틱 적용 `UpdateVehicleHandling`, 내리면 원래 값): 킥보드 속도 ×1.3/가속 1800/접지 8/회전 720/감속 1600, 오토바이 ×1.6/1500/1.6/560/700, 자동차 ×1.9/1300/1.1/460/600. 접지력(GroundFriction)이 낮으면 속도가 바라보는 방향으로 늦게 따라가서 미끄러짐 = 드리프트. 몸은 `SeatHeight`(12/22/18)만큼 메시를 올림, 타는 동안 다리 애니메이션은 서 있기(속도 0 취급).
- `ARRRandomCharacter`: `Vehicle`(복제), F → `ToggleRide`(클라이언트면 `ServerToggleRide`): 가장 가까운 빈 탈것(평면 170 안, 높이 차 키 안)에 타기 / 내리기. 안 되는 때: 쓰러짐·기어오르는 중·비행 중·시작 전/게임 끝. 내리면 탈것 옆(오른쪽, 막히면 왼쪽)으로 `TeleportTo`. 쓰러지면(`KnockOut`) 그 자리에 놓고, 비행 시작하면 내려놓고, `EndPlay`(봇이 플레이어에게 자리 양보 등)에도 내려놓음. 타는 동안 점프 안 됨.
- 플레이어 컨트롤러 `RideAction` F → `OnRide`. HUD `DrawRidePrompt`: 발밑에 `[F] RIDE CAR` / `[F] GET OFF`. 안개(`HideBrawler`)는 붙어 있는 액터(탈것)도 같이 숨김.
- 게임 모드 `SpawnVehiclesIfNoneExist`(`bSpawnVehicles`): 플레이어 시작점 기준 파랑 앞 (300, ±1100) 킥보드·오토바이, 빨강 앞 (1700, ±1100) 반대로, 가운데 (1000, ±470) 자동차 2대(안쪽을 봄).
- 확인 (임시 `-RRRideTest=종류` 로 플레이어 옆에 하나 세우고 `PostMessage` 로 F → W 1.4초 → D 0.8초 → F, 창 캡처 + 임시 로그; 확인 후 지움): 셋 다 `gets on`, 여우가 킥보드 위·오토바이 위·오픈카 안(머리·어깨 보임)에서 달림, 오토바이는 회전 때 안쪽으로 기울고 내리면 옆에 섬. 최고 속도/최대 미끄러짐/최대 기울기: 킥보드 390 / 20°(걷기와 같은 수준) / 0, 오토바이 480 / 49° / 23°, 자동차 570 / 53° / 6°. 빌드 경고 없음.
- 확인 못 한 것: 타고 사격(코드상 막지 않음 — 쏘면 바라보는 방향이 바뀌어 탈것도 돎), 네트워크(참가자의 탑승/드리프트 예측), 거인이 타기(탈것은 안 커짐), 언덕 계단 오르내리기, 봇은 안 탐.
- 수정 (사용자: 자동차를 더 컬러풀하게 + 앞뒤로 한 칸씩 의자, 두 명이 타게):
  - 모델: Tripo 가 넓은 스포츠카는 계속 좌우 2인승으로 만듦 — 텍스트 → 3D 2회(무지개 그라데이션 스포츠카 2개, 좌석 좌우), Nano Banana 이미지(앞뒤 좌석 그림) → 3D 2회(그중 날렵한 쪽은 3D 에서 좌석이 좌우로 바뀜, 통통한 색동 블록 장난감 차만 앞뒤 유지). 사용자가 날렵한 그라데이션 디자인을 원해서 참고 이미지 편집·폭 좁힌 레이서 그림까지 시도(이미지는 무료) → 넓은 차체로는 앞뒤 좌석이 안 남아서 사용자 선택으로 **색동 블록 장난감 차**(Tripo `9bfbce5f…`, 분홍 앞뒤 + 노랑·초록·청록·보라 띠, 검은 좌석 2개가 한 줄). 크레딧 24,630 → 24,510(4회 × 30). 다운로드 → `C:\work\char-pipeline\input\vehicles\car2`(참고용 `reference_rainbow_sleek.png` 도). `blender_vehicles.py ... SM_Car=…=360` → `output\vehicles\car2\SM_Car.fbx`(360×162×114), `ue_import_props.py` 로 `/Game/Vehicles/SM_Car` 교체. `blender_vehicles.py` 에 위에서 본 프리뷰(`vehicles_top.png`, 20 단위 격자) 추가 — 좌석 위치를 이걸로 잼: 앞좌석 X −7, 뒷좌석 X −93.
  - `FRRVehicleHandling` 에 `DriverSeatX`, `bRearSeat`, `RearSeatX`(자동차만 −7 / 있음 / −93). 운전자에 붙일 때 `-DriverSeatX` 만큼 밀어서 운전자가 앞좌석에.
  - `ARRRandomVehicle`: `Passenger`(복제), `HasRoom`(운전석 비었거나, 뒷좌석 있고 비었음), `Mount` 가 좌석 번호 반환(0 운전, 1 뒷좌석, −1 자리 없음), `RemovePassenger`, `GetRearSeatLocation`. `Dismount` 는 이제 탈것이 있던 자리 그대로 수평으로 내려놓음(운전석이 가운데가 아니라서).
  - `ARRRandomCharacter`: `bPassenger`(복제), `Vehicle`/`bPassenger` 둘 다 `OnRep_Ride` → `ApplyRide`(모든 기기): 뒷좌석이면 이동 끔(`DisableMovement`), 캡슐의 Pawn 충돌 무시(운전자 캡슐이 바로 앞에서 스윕해도 안 막히게), 탈것 루트에 붙여 뒷좌석 위치. 내리면 떼고 Pawn 충돌·걷기 되돌림. `GetOffVehicle`: 뒷사람이면 자리만 비우고 옆으로 내림, 운전자면 뒷사람을 먼저 내리고 주차. 뒷사람도 쓰러지면/비행이면 내림. HUD `[F] RIDE CAR (BACK SEAT)`. 안개 `HideBrawler` 는 붙어 있는 캐릭터(뒷사람)는 건드리지 않음(자기 시야 판정대로).
  - 확인 (임시 `-RRRideTest=2 -RRPassengerTest`: 플레이어가 타면 가장 가까운 아군 봇이 뒷좌석에 탐; 확인 후 지움): 로그 `gets on a CAR` → `gets on a CAR (rear seat)`, 화면에서 두 여우가 앞뒤로 앉아 같이 달리고 회전, 운전자가 쓰러지니 차가 그 자리에 남고 뒷사람도 내림. 빌드 경고 없음.
- 추가 (사용자: 플레이어가 탄 차가 주변에 서 있으면 봇이 타게, 봇도 가까운 탈것을 타고 이동 — 교전 중엔 안 타고 긴 거리일 때만):
  - `ARRRandomAIController::UpdateVehicles`(Tick 에서 이동 방향을 정한 뒤): 교전 = 숨으려는 중 || 맞는 중(`UnderFireTimer`) || 보이는 적이 사거리 × `EngageRangeShare` 안. 타고 있으면 교전 시 F(내림). 운전 중이면 평소 방향대로 몰다가 목적지(`GetTravelGoal`: 보이는 적 → 마지막으로 본 곳 → 순찰 지점) `RideArriveDistance` 500 안이거나 속도 60 미만 1.5초(막힘)면 내림. 뒷좌석이면 그대로(운전자가 내리면 같이 내려짐).
  - 안 타고 있고 교전 아니면 0.5초마다 `FindVehicleToRide`: ① 아군이 운전하는 차가 `PassengerJoinRadius` 600 안에서 속도 40 이하로 서 있고 뒷좌석 빔 → 우선(거리와 상관없이) ② 목적지까지 `RideTripDistance` 1400 이상이고 `VehicleSearchRadius` 1000(처음 700 — 100초 봇전에 두 번밖에 안 타서 늘림) 안, 여행 거리의 절반 이내, 다른 봇이 노리는 중 아님(`GetVehicleTarget`), 높이 차 120 이내. 그쪽으로 걸어가 `MountRange` × 0.8 안이면 `ToggleRide`. 8초 안에 못 가거나 자리가 없어지면 포기.
  - 확인: 시험용 차 옆에서 봇이 먼저 타고 중앙으로 몰고 가다 교전 거리에서 내림(창 캡처). 임시 `-RRRideTest=2` 로 플레이어가 차를 세워 두니 `RRRandomCharacter_2 gets on a CAR (rear seat)`, 같은 판에 적 봇 둘이 자동차/오토바이를 타고 옴. `-nullrhi` 100초: 오토바이 탑승 2번(반경 700 때).
- 추가 (사용자: 탈것 드리프트 때 속도에 따라 스키드 마크가 생겼다 지워지게): `ARRRandomSkidMarks`(월드에 하나, 처음 쓸 때 스폰, 복제 안 함): 데칼 풀(최대 400, 오래된 것부터 재사용), `AddMark(From, To, Width, Strength)` 한 조각 = 아래로 투영하는 데칼(깊이 30, 길이 방향 Z, 폭 Y), 머티리얼 인스턴스의 `Opacity` 를 `HoldTime` 2초 뒤 `FadeTime` 3초에 걸쳐 0 으로. 머티리얼 `C:\work\char-pipeline\scripts\ue_make_skid_mark.py` → `/Game/Effects/Skid/M_SkidMark`(디퍼드 데칼, 거의 검정, 거칠기 1, 폭 방향(V) 가장자리 부드럽게 + 약한 결) — 디퍼드 데칼은 베이스 컬러에 들어가서 툰 후처리가 지우지 않음. 처음엔 U 를 폭으로 잡아 자국이 점선처럼 끊겨서 V 로 바꿈.
  - `ARRRandomVehicle::UpdateSkidMarks`: 드리프트(기울기와 같은 판정) + 탑승자가 땅 위일 때 뒷바퀴 자리(메시 반길이 × `RearAxleShare` 뒤, 반폭 × `TrackShare` 양옆, 오토바이는 가운데 하나)가 24 움직일 때마다 한 조각. 진하기 0.3 → 1, 폭 × 0.6 → 1 (속도 150 → 520). 오토바이 폭 16, 자동차 22. 캐릭터와 탈것 메시는 데칼을 안 받음(`SetReceivesDecals(false)`).
  - 확인 (플레이어 자동차 드리프트 캡처): 휘어진 두 줄 자국, 3초쯤 옅어짐, 5초 뒤 없음. 빌드 경고 없음.
  - 확인 못 한 것: 언덕 단 경계에서 데칼이 벽면에 늘어지는지, 400개가 다 찼을 때 비용.
- 수정 (사용자: 거인이 되면 탈것에서 자동으로 내려지게): 캐릭터 Tick 의 비행 하차 조건에 `bGiant` 추가(서버, 운전자·뒷사람 모두), `ToggleRide` 는 거인이면 안 탐, HUD `[F] RIDE` 안내는 거인·비행 중 숨김, 봇 `UpdateVehicles` 는 거인·비행 중 탈것을 찾지 않음(헛걸음 방지). 확인(임시 `-RRRideTest=2 -RRNoBotRide`, 확인 후 지움): 차 운전 중 F7 → 거인이 되며 내려 차 옆에 섬, 다시 F 눌러도 안 탐, 안내 없음. 빌드 경고 없음.
  - 확인 못 한 것: 네트워크에서 뒷사람(참가자) 화면 — 이동을 끄고 붙이는 걸 각 기기에서 하지만 자기 캐릭터 예측과 어긋나는지 안 봄, 뒷사람이 사격할 때 몸이 돌아가는 모습.

## 18차 — 폼 핑거 (사용자: 슈퍼볼 응원 큰 손가락 무기, 공격력 1, 뒤에서 찌르면 즉사, 낮은 확률, 보이게 Tripo 로)

- 아직 커밋 안 됨.
- 모델: Tripo 텍스트 → HD Model 2회(24,510 → 24,450). 첫 노란 "#1" 폼 핑거는 사용자가 "안 이쁘다, 풍선 같은 느낌에 파란색, 오브젝트는 항상 카툰 스타일로" → `cute stylized cartoon giant foam finger, inflated like a shiny balloon, very puffy round chubby shapes ... glossy bright blue all over with a small white star on the palm ...`. 사용자 허락 후 다운로드 → `C:\work\char-pipeline\input\foam_finger`.
- `C:\work\char-pipeline\scripts\blender_foam_finger.py <fbx> <out fbx> <preview>`: 세워진 손을 눕혀 손가락 +X, 손바닥(별) 위, 손목 끝이 원점, 길이 100, 6000 삼각형; 위에서 본 확인 그림. 손끝 위치 출력(98, −8, −2 → FBX 에서 Y 뒤집혀 Unreal (98, 8, −2)). 처음엔 손가락이 −X 라 Z 180° 추가. `ue_import_props.py` → `/Game/Props/FoamFinger/SM_FoamFinger`, `M_FoamFinger`.
- `FRRWeapon`: `bFoamFinger`, `RangeMultiplier`(복제). `MakeFoamFinger`: 이름 FOAM FINGER, 티어 Legendary(HUD 색), 탄창 기본 × 4, 연사 × 1.2, 재장전 × 0.5, 사거리 × 0.25(325), 총알 속도 × 1.5. `Roll` 이 `FoamFingerChance` 0.1 로 먼저 고름(= 무기 주사위의 10%, 전체 굴림의 약 1.7%). 치트 7번 / `RRGun FINGER`(`GetTypeCount` 7).
- `ARRRandomCharacter::GetAttackRange` = AttackRange × 무기 `RangeMultiplier`(봇 사격 거리·교전 판정·투사체 `MaxRange` 모두 이걸로). 봇의 선호 거리는 이제 사거리 비율(`PreferredShare`)이라 폼 핑거를 들면 바짝 붙음.
- `ARRRandomProjectile::bPoke`(복제): 안 그림(메시·빛·총구 효과·꼬리 없음), 데미지 = `PokeDamage` 1 고정(공격력 버프·거인 배율 무시), 맞은 캐릭터가 보는 방향과 찌른 방향이 `BackstabAngle` 60° 안이면(= 등 뒤) 최대 체력 × 10 → 거인도 즉사, 로그 `pokes X in the back with the foam finger`.
- `URRRandomGunComponent`: 무기가 폼 핑거면 메시를 바꿔 오른손에 머리 위로 높이(`FoamFingerHoldOffset` (10, 25, 95), 앞으로 70° 치켜듦, 길이 110) — 손 높이로 들면 큰 머리에 가려 안 보였음. 공격할 땐 라이플 조준과 같은 방식으로 앞으로 뻗음(손끝이 총구 자리). 메시를 바꿀 때 `EmptyOverrideMaterials` — 랜덤 이벤트 컴포넌트가 시작할 때 라이플 머티리얼로 MID 를 박아 둬서 처음엔 폼 핑거가 라이플 색(회색+주황)으로 나왔음.
- HUD 무기 줄: `FOAM FINGER  DMG 1  BACK POKE = KO`, 치트 안내 `[1-7] ... FINGER`.
- 확인 (임시 `-RRFingerTest`: 봇 AI 멈추고 적 하나를 플레이어 앞 150 에 세워 서버에서 직접 찌름; 확인 후 지움): 정면 찌르기 100 → 99, 등 찌르기 → 즉사(`knocked out by`), 화면에서 파란 손 + 흰 별이 머리 위에 보임. 빌드 경고 없음.
- 수정 (사용자: 평소엔 살짝 대각선 위로 들고 공격 때 앞으로 살짝 찌르는 효과): `URRRandomGunComponent` 폼 핑거는 라이플 조준 블렌드를 안 쓰고 자체 자세 — 손바닥 + `FoamFingerHoldOffset` (15, 25, 70)에서 앞 `FoamFingerLift` 35° 위를 가리킴, 찌르기마다(`ARRRandomCharacter::GetTimeSinceShot`, 모든 기기에서 샷 시각을 기록하므로 복제 그대로) `FoamFingerPokeTime` 0.22초 동안 sin 곡선으로 `FoamFingerPokeDistance` 45 앞으로 뻗으며 10°로 내려옴. 확인(창 캡처): 쉬는 자세는 머리 위 앞쪽으로 비스듬히, 클릭 직후 프레임에서 앞으로 뻗음.
- 추가 (사용자: 무적 치트키): F10 / `RRGod` → `ServerCheatGod` → `ARRRandomCharacter::SetInvincible` 토글(`bInvincible` 복제). `TakeDamage` 첫 조건에서 0 반환(모든 공격·폼 핑거 즉사 포함), 섬 밖 낙사 대신 시작 위치로 텔레포트. HUD 머리 위 흰 `GOD` 칩(안 깜빡임), 치트 안내에 `[F10] GOD`. F9 는 엔진 스크린샷 키라 F10. 확인: 로그 `cheat, ... is invincible`, 칩 표시, 교전 근처 10초 뒤 체력 100(그 사이 실제로 맞았는지는 캡처로 확인 못 함 — 데미지 경로는 `TakeDamage` 하나).
- 확인 못 한 것: 실제 클릭으로 등 뒤 돌아 찌르기(봇이 계속 몸을 돌림), 봇이 폼 핑거를 들고 싸우는 모습, 네트워크.

## 18차 덧붙임 — 피격 효과 (사용자: 트라이포에서 2D 로 그려 적용, 파란 손 원킬은 해골, 나머지 무기는 분위기에 맞게)

- 그림: Tripo 이미지 생성(Nano Banana Pro, 16:9, 2장, 무료 할당) — 8개 카툰 스티커를 4×2 격자, 순수 초록 배경, 굵은 외곽선(프롬프트에 칸별 내용: 노란 별 폭발 / 주황 지그재그 불꽃 / 빨강·주황 연사 불똥 고리 / 흰·금 섬광 / 청록·보라 에너지 / 주황 폭발 구름 / 파란 별구름 / 귀여운 해골). 사용자 선택으로 왼쪽 장을 다운로드 → `C:\work\char-pipeline\input\hit_effects\hit_effects_sheet.png`(1376×768).
- `C:\work\char-pipeline\scripts\make_hit_effects.py`(Blender Python: `blender -b --python make_hit_effects.py -- <sheet> <out>`): 칸마다 초록 키(0.12~0.4) + 초록 번짐 제거, 스티커 bbox 로 잘라 6% 여백의 정사각형, 256 으로 쌍선형(미리 곱한 알파로) → 1024×512 아틀라스 `output\ui\T_HitEffects.png` → `ue_import_ui_texture.py` → `/Game/UI/T_HitEffects`.
- `ERRHitEffect`(Pistol, Smg, Minigun, Rifle, Blaster, Cannon, Poke, Skull = 아틀라스 칸 순서), `FRRWeapon::GetHitEffect`(이름/폼 핑거로). 사격 때 `ARRRandomProjectile::HitEffect`(서버) → `TakeDamage` 가 읽어 `MulticastDamagePopup(Damage, Effect)`; 폼 핑거 찌르기로 체력이 0 이 되면 Skull. `FRRDamagePopup` 에 Effect·Spin(±25°)·Nudge(±14)·Lifetime(해골 1.6초, 나머지 0.8초).
- HUD `DrawDamagePopups`: 숫자 아래에 스티커 — 0.1초에 1.25 배까지 튀었다 1 로, 수명 55% 부터 페이드하며 0.8 배로, 크기는 효과별(720p 기준 40~96, 캐논·해골 큼), 해골은 회전 없이 위로 떠오름. 쓰러진 캐릭터에도 팝업을 그리므로 해골이 시체 위에 남음.
- 확인 (임시 `-RRHitTest`: 봇 AI 멈춤, 적 셋을 앞에 세워 팝업을 차례로 + 폼 핑거 등 찌르기; 확인 후 지움): 별 폭발·지그재그·에너지·폭발 구름이 맞은 여우 위에 튐, 찌르기 별구름, 등 찌르기 로그 + 쓰러진 여우 위에 해골. 빌드 경고 없음.
- 남은 것: 등 찌르기 데미지 숫자가 `1000`(최대 체력 × 10)으로 뜸 — 원하면 "KO" 같은 글자로 바꿀 수 있음. 아틀라스는 UI 그룹(밉 없음). 막 장착한 순간 텍스처가 아직 안 올라와 잠깐 회색으로 보인 프레임 있음(스트리밍).

## 18차 덧붙임 — 무기별 총 모델 (사용자: 무기 종류별로 다른 오브젝트를 Tripo 로 만들어 붙이기)

- 모델: Tripo 텍스트 → HD Model, 권총·SMG·미니건·블래스터·대포 5종. 첫 세트는 사용자가 "조금 더 카툰 스타일로" → 통통한 장난감 비율(chibi, 굵은 둥근 형태, 밝은 사탕색)로 다시 만듦. 크레딧 현재 24,150. 원본: `C:\work\char-pipeline\input\guns\<pistol|smg|minigun|blaster|cannon>\`.
- `C:\work\char-pipeline\scripts\blender_guns.py -- <out dir> <SM_X>=<fbx>=<grip x>,<grip z>[=flip] ...`: 메시 합치고 긴 쪽을 X 로(Tripo 가 총구를 -X 로 둔 권총·미니건·대포는 `flip`), 원점을 손잡이(옆모습 기준 길이 대비 비율), 길이 100, 6000 삼각형으로 줄임. 총구 끝(앞 3% 버텍스 중심)을 언리얼 좌표로 출력하고 `<out>\<SM_X>_side.png` 옆모습(빨간 점 손잡이, 파란 점 총구) 렌더. 쓴 값: 권총 -0.24,0.24 flip / SMG -0.15,0.18 / 미니건 -0.03,0.12 flip / 블래스터 0.16,0.22 / 대포 0.02,0.18 flip.
- `ue_import_props.py "/Game/Props/Guns|SM_Pistol=<fbx>=<basecolor>|..."` → `/Game/Props/Guns/SM_{Pistol,Smg,Minigun,Blaster,Cannon}`, `M_*`, `Textures/T_*_BaseColor`(소품과 같은 평평한 툰 머티리얼).
- `URRRandomGunComponent`: `GunLooks` 표(`ERRHitEffect` 순서: 경로, 손에 든 길이, 100 길이 메시 기준 총구 끝) — 권총 80 (72,0,33), SMG 100 (63,0,7), 미니건 130 (52,0,27), 라이플 = 기존 `GunLength`/`MuzzleTip`, 블래스터 95 (33,0,30), 대포 120 (46,0,29). 생성자에서 `GunMeshes` 로 로드(없으면 라이플로). 매 틱 `GetWeapon().GetHitEffect()` 로 메시를 고르고 바뀌면 `EmptyOverrideMaterials`, 길이·총구 끝도 그 무기 값으로(걸으며 쏠 때 총알이 그 총구에서 나오게).
- 확인 (창 캡처, 치트 1–6): 권총 초록/노랑, 미니건 빨강/노랑, SMG 연두/보라, 라이플, 블래스터 흰/보라, 대포 파랑/주황이 손에 각각 보임, 걸을 때 대포 옆모습 확인.
- 확인 못 한 것: 걸으며 쏠 때 총알이 각 총구에서 정확히 나오는지(측정값만 넣음), 거인 크기.

## 20차 — 두 가지 모드 (사용자: 탑뷰 게임과 TPS 를 골라서 입장, TPS 에선 안개 개념 없음)

- 타이틀: SINGLE PLAY 와 HOST GAME 다음에 보기 선택 페이지(TOP VIEW / THIRD PERSON / BACK, 위 안내 "HOW DO YOU WANT TO PLAY?"). JOIN 은 호스트 방식. `ARRRandomTitlePlayerController`(`ERRTitleChoice::TopView/ThirdPerson`, `bViewPage`, `bViewForHost`; BACK 은 시작한 페이지로).
- `URRRandomSessionSubsystem::SetThirdPersonMode/IsThirdPersonMode`(게임 인스턴스라 레벨이 바뀌어도 유지 → 새 게임 `?Restart`, F1 호스팅도 같은 모드). `-RRThirdPerson` 명령줄도.
- `ARRRandomGameState::bThirdPersonMode`(복제) + 정적 `IsThirdPersonMode(context)`. 게임 모드 `StartPlay` 가 세션에서 가져와 설정.
- 플레이어 컨트롤러: TPS 게임이면 안개 액터를 안 만들고(늦게 복제되면 PlayerTick 에서 없앰), 폰이 생기면 `SetThirdPersonView(true)`(예전 F9 치트 본체를 이 함수로 뺌). F9 치트는 탑뷰 게임에서만. 마우스 시선(Mouse2D)·조준(오른쪽 버튼) 키는 이제 쉬핑 빌드에도 매핑.
- 안개 없는 시야: `ARRRandomCharacter::IsSeenByTeam` 이 TPS 게임에선 `VisionRadius` 800 대신 `ThirdPersonSightRange` 3000(벽 가림은 그대로) → 봇은 탁 트인 곳의 적을 멀리서 보고 싸움. 적 숨기기 없음(안개 액터가 없으므로), 미니맵은 TPS 에선 적 표시 없음(팀원만).
- 확인: 타이틀 → SINGLE PLAY → 보기 페이지 → THIRD PERSON 으로 바로 어깨 너머 시점, 안개 없음, 미니맵 밝음.
- 참고: 이 빌드 직후 게임이 프로젝트 DLL 을 불러오기 직전에서 두 번 멈춤(창 없음, 로그는 Turnkey 장치 검사 뒤에서 끝) — 몇 분 뒤 다시 실행하니 정상. 스마트 앱 컨트롤이 새 DLL 을 검사하느라 붙잡은 것으로 보임(예전 4551 차단과 비슷).

- 수정 (사용자: TPS 에서 마우스가 안 보이는 상태로 계속 움직여서 문제): 게임 창이 마우스를 쥐고 있는지 안 보고 마우스 움직임을 늘 시선으로 받았음. 이제 `IsViewportFocused`(뷰포트 포커스 + `FPlatformApplicationMisc::IsThisApplicationForeground`, 모듈 `ApplicationCore` 추가) 이고 커서가 숨었을 때만 시선 입력, 한 프레임 최대 60 카운트. 커서가 다시 나오는 때: 일시정지/결과 화면, **왼쪽 Alt 누르는 동안**(`IA_FreeCursor`), 다른 창 사용 중(Alt+Tab 등). 게임 창을 다시 클릭하면 다시 잡음(그 클릭은 사격 안 함). 커서가 나와 있을 땐 클릭이 사격 대신 미니맵 클릭. 확인(임시 로그): 게임 창 활성 = 커서 숨김, 다른 창으로 전환 = 커서 보임 + 시선 그대로.

## 20차 덧붙임 — TPS 마우스 고정, 창 크기 변경 GPU 크래시 (사용자: TPS 에선 마우스를 가운데 고정, 사격 중 안 보이는 커서가 창 밖으로 나감 / 창 크기 바꿀 때 GPU crash)

- 마우스: `ARRRandomPlayerController::LockMouse` — 게임이 마우스를 가진 동안(TPS, 커서 숨김) 뷰포트 `SetMouseLockMode(LockAlways)`, `SetMouseCaptureMode(CapturePermanently_IncludingInitialMouseDown)`, `CaptureMouse`, `LockMouseToViewport` → 창 안에 묶임; `CenterMouse` 가 매 틱 가운데에서 40 픽셀 넘게 벗어나면 가운데로(시선은 원시 마우스 이동으로 돌아서 되돌림은 회전 안 함). Alt/다른 창/메뉴/탑뷰로 돌아갈 땐 풀어 줌. 확인: 게임 창 앞에서 상대 이동 주입 → 커서 계속 창 가운데(1280,764), 시선 0 → 18.4°, 다른 창으로 바꾸면 풀림.
- GPU 크래시: 크래시 기록(`SavedCrashesECC-...359758...`) — 창 크기 변경 중(TSR 1292X727 → 1352X760) ASYNC COMPUTE 큐 작업이 5초 타임아웃 → 다음 PRESENT 에서 `DXGI_ERROR_DEVICE_REMOVED`(RTX 5060 LAPTOP, 하이브리드 그래픽, SWAPCHAIN ALLOWTEARING=1). `CONFIGDEFAULTENGINE.INI [SYSTEMSETTINGS]` 에 `R.D3D12.ALLOWASYNCCOMPUTE=0`, `R.RDG.ASYNCCOMPUTE=0`, `R.D3D12.USEALLOWTEARING=0`. 확인: 창 크기 24번 연속 변경(800X500~1700X950) — GPU 타임아웃/크래시 없음, 게임 정상. (원래도 매번 나던 건 아니라 완전히 없어졌다고 단정은 못 함.)

## 20차 덧붙임 — TPS 조준 사격 (사용자: TPS 모드에선 에임에 맞게 발사체가 날아가게)

- `ARRRandomCharacter::ShootAt`: 3인칭 플레이어면 총구에서 조준점(`GetCrosshairTarget`)으로 곧장(같은 층이면 수평으로 펴던 것, 목표를 총구 높이로 올리던 것 없음), 위아래 한계 `ThirdPersonMaxShotPitch` 80°(탑뷰/봇은 그대로 50°, 같은 층 수평). 발사체는 원래 중력 없이 직선.
- `URRRandomBodyAnimInstance`: 3인칭이면 조준 자세의 목표 방향을 `GetBaseAimRotation`(내 기기/서버는 카메라, 다른 기기는 복제된 시선 피치)의 피치로(±55°) → `PointGun` 이 가슴을 그만큼 기울여 총이 위아래로 조준점을 향함.
- 확인(임시 `-RRShotTest`: 카메라 피치 고정 후 연사, 로그; 지움): 수평 조준(-10°) → 총알 -5.7° 로 조준점 바닥에, 위 조준(25°) → 25.3° 로 하늘의 조준점 쪽, 캡처에서 노란 총알이 조준점으로.

## 20차 덧붙임 — 물을 타이틀처럼 (사용자: 인게임 물 그래픽을 타이틀처럼)

- `C:\work\char-pipeline\scripts\ue_make_water.py /Game/Environment/Island/M_IslandWater`(예전 `ue_make_island_materials.py` 의 물을 대체): 불투명, 월드 노멀.
  - 색: 해안 근처 밝은 청록(`Shallow`) → 중간 청록 → 먼 바다 깊은 남색. `ShoreMask`(섬이 만듦: 땅 흰색, 칸당 8 텍셀, 반경 5 박스 블러 × 2, sRGB 인코딩) 를 `MapArea` 위에 깔아 해안에서의 거리로. 큰 얼룩 노이즈로 깊은 곳도 단조롭지 않게.
  - 코스틱: 시간 따라 움직이는 셀룰러 노이즈 두 겹의 경계선(가는 선), 느린 노이즈로 휘게, 밝기 얼룩, 얕은 곳에서 진하고 깊은 곳에선 희미.
  - 해안 거품: 마스크 0.44 근처의 가는 띠, 노이즈로 끊기고 사인으로 들락날락.
  - 반짝임: 22 단위 셀마다 일부 점이 깜빡임(이미시브).
  - 물결 노멀: 여섯 방향 사인파 기울기(정확한 미분). 처음엔 노이즈를 유한 차분해 노멀을 만들었는데 가로 줄무늬가 생김 → 사인파로 교체. 거칠기 0.28, 스페큘러 0.7.
  - 폭포(세로 면): 아래로 흐르는 밝은 줄.
- `ARRRandomIsland`: 물 평면을 1.04 배로 겹쳐 이음매 없앰, 물과 폭포에 MID(`ShoreMask`, `MapArea`).
- 확인: 탑뷰 해변·만·선착장(청록, 코스틱, 해안 거품, 반짝임), TPS 시점 강(맑은 청록, 둑의 흰 거품) — 임시 `-RRWaterLook`(카메라를 강 쪽으로) 로 캡처 후 지움.

## 19차 덧붙임 — 3인칭 치트 (사용자: 배그 3인칭 같은 느낌으로 보고 싶음, 치트로 3인칭 실행하고 조준사격)

- F9 / `RRThirdPerson`(쉬핑 빌드 제외) 토글. `ARRRandomPlayerController`: 마우스 커서 숨김 + `FInputModeGameOnly`, `IA_Look`(Mouse2D) → `AddYawInput`/`AddPitchInput(-Y)`(감도 `LookSensitivity` 0.35, 조준 중 × `SightsSensitivity` 0.5), 시선 상하 -70~50°, `IA_Aim`(오른쪽 버튼 누르는 동안) 조준, WASD 는 카메라 요 기준, 왼쪽 버튼은 `GetCrosshairTarget`(카메라에서 화면 가운데로, 자기 몸 거리부터 20,000 까지 Visibility 트레이스, 맞은 곳 아니면 끝점) 로 `FireAt`. 미니맵 클릭/맵 위 사격 막기는 3인칭에선 무시. 일시정지 메뉴/결과 화면 땐 커서 다시 보이게(PlayerTick). 안개 음영은 숨김(`ARRRandomFogOfWar::SetShadingVisible`, 볼륨 끔 — 위에서 보는 그림용) — 안 보이는 적 숨김은 그대로.
- `ARRRandomCharacter`: `SetThirdPerson`(로컬) + `ServerSetThirdPerson`, `bThirdPerson` 복제(소유자 제외). 3인칭이면 `bUseControllerRotationYaw`(탈것 위는 제외), 이동 방향 회전 끔(탈것이면 켬). `UpdateCamera`(로컬, 매 틱): 3인칭 = 스프링암 `bUsePawnControlRotation`, 벽 충돌 검사, 길이 340 / 오프셋 (0, 75, 95) / FOV 80, 조준 = 200 / (0, 95, 115) / 50(초당 14 로 부드럽게); 위에서 보기 = 원래 피치 -70, FOV 30, 비행 때 물러나기(원래 `UpdateLocomotionAnimation` 에 있던 것을 옮김). 여우 머리가 커서 처음 조준 위치(140 / (0, 45, 60))에선 귀가 화면을 가림 → 높고 오른쪽으로.
- HUD `DrawCrosshair`: 3인칭일 때 가운데 점 + 네 개 짧은 선(검은 테두리), 조준 땐 간격 14 → 6, 쏜 직후 0.18초 벌어짐. 치트 안내에 `[F9] 3RD PERSON`.
- 확인(임시 `-RRTPSTest`: 3인칭에서 카메라 자동 회전, 자동 사격, 5~8초 조준, 무적; 지움): 컨트롤 요와 캐릭터 요가 같이 돎(25° → 24.8°, 99.9° → 99.9°), 어깨 너머 화면에 마을/버섯 집/울타리, 조준 시 확대, 노란 총알이 조준점 쪽으로. F9 다시 누르면 위에서 보기로 돌아옴. 진짜 마우스 입력은 캡처로 못 함(코드 경로는 Enhanced Input 콜백 → 회전 적용 전이라 정상이어야 함).
- 확인 못 한 것: 실제 마우스로 돌리기, 네트워크 참가자(서버에 3인칭 알림은 있음), 3인칭에서 오르기/비행/탈것.

## 19차 덧붙임 — 흙길 텍스처, 카메라, 비행 느낌 (사용자: 길이 어색하니 자연스러운 텍스처를 뽑아 입히기, 카메라 각도를 조금 더 세워 날아갈 때 느낌, 그림자나 부수 효과로 날아가는 느낌)

- 흙길: Tripo 이미지 생성(Nano Banana Pro, 1:1, 무료 할당, 사용자 허락 후 1장 다운로드) "seamless tileable top-down ... cartoon dirt path ... pebbles ..." → 2×2 반복으로 나온 그림 `C:\work\char-pipeline\input\path\dirt_path_2x2.png`. `scripts\make_path_tile.py`(블렌더 파이썬, numpy): 자기 자신과 가장 잘 맞는 이동량으로 반복 주기(512, 오차 0.004)를 찾아 한 주기를 잘라 가장자리를 맞은편과 섞고 1024 로 → `output\path\T_DirtPath.png`. `scripts\ue_make_path_material.py <png>` → `/Game/Environment/Island/Textures/T_DirtPath`, `M_IslandPath`(마스크드 0.5, 인스턴스 메시용): 흙을 월드 공간 `TileSize` 420 마다 반복, `RoadMask`(섬이 만듦)를 `MapArea`(먼 X, 왼쪽 Y, X 크기, Y 크기; RGBA 출력으로 연결해야 함 — RGB 로 연결해 `.w` 컴파일 에러를 한 번 냄) 위에 깔고 가장자리를 월드 노이즈(`EdgeNoise` 0.28)로 흔듦. 마스크 샘플러는 엔진 흰 사각형과 맞춰 Color(sRGB) — 처음에 LinearColor 로 해서 컴파일 실패(기본 회색 격자 머티리얼로 땅 전체가 회색이 됨).
- `ARRRandomIsland::Build`: 모래 칸에서 마스크(칸당 8 텍셀, 반경 3 박스 블러 × 2번으로 모서리 둥글게, sRGB 인코딩해서 써서 선형으로 읽힘) → 섬 전체를 덮는 평면 하나(모래판 위 0.5)에 그 머티리얼 MID. 모래판 큐브는 숨기고(충돌은 그대로) 밟는 높이 그대로.
- 카메라: 피치 -60 → -70(더 내려다봄 — 비행 중 바로 아래 그림자가 보이게), FOV 20 → 30(같은 화면 폭이 되게 팔 길이 계산 그대로 → 약 5,225): 약간의 원근으로 높이 오를수록 캐릭터가 커 보임. 내 캐릭터가 비행(또는 활공) 중이면 카메라 팔이 최대 12% 길어짐(초당 2 로 부드럽게).
- 비행 중 몸 기울기: 활공/비행 + 공중이면 지상 속도 600 에서 최대 18° 앞으로(액터의 오른쪽 축 기준 회전을 메시 상대 회전 앞에 곱함, 초당 6 으로 부드럽게).
- `URRRandomFlightEffectsComponent`(새, 모든 캐릭터, 순수 연출): 발밑에서 WorldStatic 만 아래로 트레이스해 바닥을 찾음.
  - 둥근 그림자 데칼(`/Game/Effects/Flight/M_BlobShadow`): 반지름 85, 높이 900 에서 0.75 배, 진하기 0.75(땅, 몸이 대부분 가림) → 0.85(공중). 처음엔 너무 흐리고 작아(부드러운 감쇠 0.35~1, 진하기 0.5→0.16) 안 보였음 → 절반까지 고르게 진하고 가장자리만 흐린 원으로. 데칼은 회전을 등록 뒤에 위치와 같이 매번 지정.
  - 흙먼지 고리(`M_DustRing`, 데칼 3개 재사용): 위로 500 이상 빠르게 땅을 떠날 때(점프/비행 상승) 0.6 세기, 140 이상 떨어져 착지하면 낙차에 따라 0.5~1, 0.55초 동안 퍼지며 사라짐.
  - 바람 줄기(`M_WindStreak`, 가산 무광, 인스턴스 커스텀 데이터로 투명도): 공중 + 지면에서 120 이상 + 속도 250 이상이면 초당 최대 30개, 몸 주위에 진행 방향으로 누운 가는 줄(길이 110~220, 폭 10)이 0.35초에 사라짐.
  - 머티리얼은 `scripts\ue_make_flight_fx.py`.
- 확인(캡처 스크립트: F8 비행 치트 → W+스페이스 상승 → D 활공 → 재상승 → 착지): 흙길이 칸 모양이 아니라 둥글고 구불한 가장자리로 잔디와 이어짐, 비행 중 몸 아래(화면상 아래쪽)에 둥근 그림자 + 화면 위쪽에 해 그림자 + 흰 바람 줄 + 날개 반짝이, 땅에선 발밑에 옅은 접촉 그림자. 로그에 머티리얼/게임 경고 없음.
- 수정 (사용자: 여전히 길이 안 이쁨): `M_IslandPath` 다시 — 타일의 조약돌을 평균색 쪽으로 70% 만 남기고 흙색(선형 값으로 지정; 처음엔 sRGB 값을 그대로 넣어 거의 흰색이 됨)을 입힘, 큰(760)·중간(210)·작은(38) 월드 노이즈로 밝고 어두운 흙 얼룩, 길 가운데는 조금 밝고 가장자리는 0.58 배 어둡게(다진 흙과 잔디 그늘), 가장자리 경계는 두 겹 노이즈(150, 45)로 구불구불 + 0.36~0.64 구간을 픽셀 디더(인터리브드 그래디언트 노이즈, 프레임마다 이동 → TAA 가 섞음)로 풀어서 잔디가 길 가장자리로 스며들게. 섬은 마스크 G 채널에 해변(물에 닿은 모래 칸)을 따로 넣어 그곳은 고운 모래색. 확인: 평소 카메라와 맵 위쪽/아래쪽 내려다본 캡처(임시 `-RRMapTest`, 지움).
- 수정 (사용자: 길 텍스처가 흐리멍텅): 원인은 (1) `T_DirtPath` 가 스트리밍이라 낮은 밉으로 그려짐 → `never_stream`, 밉 `TMGS_SHARPEN4`, (2) AI 그림 자체가 부드러움 → `make_path_tile.py` 에 대비(1.25)·언샤프 마스크(0.9) 인자 추가해 다시 자름, (3) 머티리얼이 조약돌을 평균색으로 30% 끌어당김 → 원래대로(`PebbleStrength` 1). 색은 흙빛(`DirtTint` 선형 0.74, 0.66, 0.5), 큰 얼룩 0.86~1.12, 반복 `TileSize` 520 → 620, 가장자리는 구불구불한 실제 경계를 따라 0.6 배 어두운 테두리 + 디더 구간 0.46~0.54 로 좁혀 또렷하게. 확인: 평소 카메라 캡처(조약돌·풀 또렷, 테두리 선명).
- 수정 (사용자: 바닥이 너무 2D, 이미지가 그냥 깔린 것 같음): 입체감 추가. `C:workar-pipelinescriptsmake_path_height.py <tile> <out>`(블렌더 numpy): 주변 흙색과 다른 곳(조약돌·풀·외곽선)을 채워 블러로 둥근 돔 + 흙 얼룩의 약한 굴곡 → `outputpathT_DirtPath_H.png` → `/Game/Environment/Island/Textures/T_DirtPath_H`(선형 그레이스케일, 스트리밍 안 함). `M_IslandPath` 는 월드 공간 노멀(`tangent_space_normal` 끔 — 평면 탄젠트가 월드 UV 와 안 맞으므로): 높이맵 기울기(`BumpHeight` 9, 텍스처 U = 월드 +Y, V = 월드 -X), 9 단위 노이즈로 흙 알갱이(`GrainHeight` 1.6), 가장자리는 커버리지의 월드 기울기(화면 미분으로)로 잔디 쪽이 올라가게(`EdgeBevel` 45) → 길이 잔디에 파인 느낌. 거칠기는 높이로 조약돌 0.62 / 흙 0.95, 스페큘러 0.35. 섬: 길 가장자리(마스크 0.45~0.7)에만 칸당 4번 시도·40% 로 진짜 돌(바위 묶음 0.14~0.28 배, 그림자 있음) — 길 가운데에 작은 돌도 뿌렸었는데 회색 점처럼 어색해 뺌. 확인: 평소 카메라 캡처(조약돌이 볼록하게 빛과 그림자, 흙 알갱이, 가장자리 턱과 돌).
- 다시 (사용자: 방향을 못 잡음 — 풀 바닥이나 꽃처럼 Tripo 3D 로 만들어 전체 조화): 평면 텍스처 길(`M_IslandPath`, `T_DirtPath*`, 마스크) 은 안 씀(에셋/스크립트는 남음). Tripo 텍스트 → HD Model 5개(150 크레딧, 23,700 남음; 하나는 모양이 깨져 무료 재시도): 흙길 바닥 타일 3종(조약돌 / 작은 풀 / 맨 흙 + 작은 돌, "square flat dirt path floor tile ... perfectly flat even top surface" — 잔디 타일과 같은 식), 작은 꽃 달린 풀 덤불, 둥근 조약돌 무더기 → `C:workar-pipelineinputpath3d*`. `blender_grass_tiles.py` 로 잔디 타일과 똑같이 잘라 180 정사각형(Tripo 가 두 개를 세워서 모델링 → 가장 얇은 축이 Z 가 아니면 정점이 많이 몰린(울퉁불퉁한) 쪽이 위로 오게 세우는 단계 추가) → `/Game/Environment/Path/SM_DirtTile_A/B/C`(`ue_import_grass_tiles.py` 에 `opt:` 옵션 추가: brightness 0.6, desaturate 0.35, normals=mesh — 잔디처럼 법선을 위로 고정하지 않고 메시 그대로 받아 흙덩이에 빛/그늘, under=0). 풀 덤불 `SM_RoadTuft`(폭 110), 조약돌 `SM_RoadPebbles`(폭 60) 는 `blender_props.py` + `ue_import_props.py`. 섬: 모래 칸마다 흙 타일(무작위 종류, 90° 단위 회전, Z 0.6 배로 납작하게, 윗면 바닥 + 7 — 밟는 판 높이도 7), 길이 잔디('.'/'g')와 맞닿은 변마다 풀 덤불 2~3개(0.55~0.95 배, 이음매 위)와 30% 확률로 조약돌. 잔디 바닥은 섬의 길 칸에서 자기 타일을 뺌(`ARRRandomGrassFloor::ClearRoadTiles`, 섬/잔디 중 나중에 깔리는 쪽이 정리 — 로그 `grass floor gave 186 tiles to the island's roads`). 처음엔 흙 타일 윗면을 + 3 에 둬서 흙의 파인 곳 사이로 잔디 밑판 초록이 얼룩처럼 비침 → 납작하게 + 7 로. 확인: 평소 카메라 캡처(흙덩이 입체, 가장자리 풀 덤불, 잔디 바닥과 같은 결). 남은 것: 흙 타일끼리 이음매가 가까이서 희미하게 보임.
- 확인 못 한 것: 흙먼지 고리가 화면에서 잘 보이는지(캡처 타이밍에 안 잡힘), 네트워크 참가자 화면, 거인 비행.

## 19차 — 카툰 끄기, 장난감 마을 섬 (사용자: 카툰 효과 끄고 전체적으로 자연스럽게, Tripo 로 오브젝트 몇 개 더 만들어 메이플스토리/로블록스 느낌으로 다시 배치, 탈것을 타야 하니 레벨 약 2배 — 질문에 "면적 2배(한 변 약 1.4배)" 선택)

- 카툰: `RRRandomCharacter.cpp` 의 `bCartoonLook = false` → `MI_PP_Toon` 블렌더블을 안 붙임. 대신 AO 0.6, 채도 1.08(툰일 땐 AO 0, 채도 1.15). 모션 블러 0 은 항상. 툰 에셋/스텐실 설정은 그대로(다시 켜기 쉬움). 팀색 외곽선도 툰 안에 있었으므로 같이 꺼짐(머리 위 마커로 구분).
- Tripo(사용자 허락, 텍스트 → HD Model 10개, 24,150 → 23,850, 프롬프트마다 "cute stylized cartoon, chunky rounded puffy shapes"): 빨간 큰 버섯, 버섯 집, 화살표 표지판, 울타리, 가로등, 나무 상자+통, 꽃 무더기, 나무 다리, 풍차, 장난감 블록. FBX(Blender, 2K) → `C:\work\char-pipeline\input\props2\<이름>`. `blender_props.py` → `output\props2\SM_*.fbx`(높이/폭: 버섯 380h, 집 440h, 표지판 210h, 울타리 190w(길이 X), 가로등 320h, 상자 170h, 꽃 150w, 다리 580w(길이 Y), 풍차 800h, 블록 230h) → `ue_import_props.py` → `/Game/Environment/Village/SM_*`, `M_*`, `Textures/T_*_BaseColor`. Tripo 모델은 정면(문)이 언리얼 +Y → 게임에선 요 90 으로 카메라(-X) 쪽.
- `C:\work\char-pipeline\scripts\make_island_map.py`(블렌더 파이썬으로 실행: `blender -b --factory-startup --python make_island_map.py -- <out .inc> <preview .txt>`): 36×36 지도를 구역별로 칠하고(시드 36) 검사(시작 칸 모래, 물/다리/길 위 소품 종류, 가장자리, 벽 높이 ≥3) 후 `TypeRows/HeightRows/PropRows` C++ 초기화 코드와 미리보기 출력. 집·풍차 주변 한 칸은 비움. 그 출력을 `ARRRandomIsland` 생성자에 그대로 넣음.
- `ARRRandomIsland`: 종류 `b`(다리: 물 표면 + 숨긴 발판, 물 위 30, 두께 20, 물벽 없음). 소품은 `PropKinds` 표(글자, 메시, 크기 범위, 칸당 개수, 비껴 놓기, 방향 — 아무/90° 단위/정한 요±, 막는 것 — 없음/캐릭터만/전부 + 폭 비율): 막는 소품은 메시 경계 × 크기 × 폭 비율 상자를 숨긴 `Solid`(전부) 또는 `WaterWalls`(캐릭터만)에 같은 회전으로 추가. 로그 `island 36 x 36 cells: 231 solid, 186 sand, 249 water, 18 dock or bridge, 342 props (78 solid)`.
- 게임 모드: `GetArenaCenter()` = 플레이어 스타트 아래 레벨 바닥의 가운데(지금 (0,0,92)), 한 번만 계산. 섬·잔디·엄폐물·탈것·봇의 기준이 플레이어 스타트+1000 에서 이 중심으로. `TeamSpawnDistance` 2000 → 3060(파랑 26행, 빨강 9행 = 중심 ∓1530). 엄폐물 새 배치(점대칭: 큰길 양옆 벽, 더 바깥 벽, 팀 앞 기둥 4, 각 팀 쪽 가로 벽 — 길/언덕/소품 피함). 탈것: 각 팀 길 바로 앞 ±900 에 킥보드·오토바이, 차는 서쪽 길(-1890)과 동쪽 길(1710) 가운데. 잔디 바닥을 섬보다 먼저 스폰(가운데에 큰길 모래판이 생겨서 섬 다음에 스폰하면 그 판을 바닥으로 잡아 엔진 체크무늬 바닥이 드러났음).
- 봇 순찰 반경 최대 1300 → 1800. 미니맵 텍스처 208 → 288 픽셀(칸당 8), 다리는 선착장 색, 집·풍차·상자·블록·버섯은 빨간 지붕색 칸.
- 확인(임시 `-RRMapTest`: 안개 끔, 카메라 거리/각도/위치 지정, 무적 / `-RRBridgeTest`: 파랑 시작에서 오른쪽으로 계속 걷기; 확인 후 지움): 섬 전체(위 숲·풍차·빨강 마을·들판·파랑 마을·선착장·강과 다리), 집 문과 표지판이 카메라 쪽, 울타리 줄, 다리를 건너 발판 위(Z 143)로 강둑까지 가서 동쪽 벽에서 멈춤. 평소 카메라로 마을 길 근처 모습. 로그에 게임 쪽 경고 없음. 첫 판에 풍차와 집이 겹쳐 집 열을 (4,10,24) → (9,13,24) 로 옮김, 가로등이 위에서 너무 커 보여 0.72~0.8배로, 상자·블록은 조금 키움.
- 확인 못 한 것: 봇이 집/버섯/울타리에 걸리는지(순찰 지점이 막힌 곳이면 3~6초 뒤 다음 지점), 봇이 다리를 건너는지, 긴 경기 성능(소품 342개 + 인스턴스), 네트워크.

## 18차 덧붙임 — 사격 애니메이션, 총구, 총 크기 (사용자: Tripo·Mixamo 로 사격 애니메이션을 전체적으로 자연스럽게, 총구 위치 맞게, 캐릭터 크기에 맞게 총 크기, 이동 사격도 자연스럽게)

- Mixamo(로그인된 사용자 계정, 기존 캐릭터 `fox_mixamo_upload`)에서 소총 조준 세트 9개를 애니메이션만(스킨 없음, 30fps, In Place) 받음 → `C:\work\char-pipeline\input\fox\mixamo_aim\{aim_idle,walk_f|b|l|r,run_f|b|l|r}.fbx` → `ue_import_fox_anim.py` 로 `/Game/Characters/Fox/Anims/A_Fox_AimIdle`, `A_Fox_AimWalkF|B|L|R`, `A_Fox_AimRunF|B|L|R`. 크롬이 mixamo.com 의 연속 다운로드를 막아 사용자가 허용함.
- `C:\work\char-pipeline\scripts\blender_anim_speed.py -- <fbx> ...`: 제자리 애니메이션의 이동 속도(딛은 발이 뒤로 미끄러지는 속도의 중앙값), 엉덩이 이동, 중간 프레임의 오른손→왼손 방향. 결과(스케일 1): 조준 걷기 ~59, 조준 달리기 앞 148 / 뒤 134 / 왼 125 / 오른 148 cm/s → 게임 속도 300(× 메시 1.8)엔 달리기가 맞아 달리기만 씀. 모든 Mixamo 소총 자세가 두 손(총)을 몸 정면보다 33~40° 왼쪽으로 듦.
- `FRRAimAnimations`(새 헤더 `RRRandomAimAnimations.h`): Idle, Fire(= 기존 `A_Fox_Fire`), Move[4], MoveSpeed[4]. `ARRRandomCharacter::AimAnimations`, `WantsAimPose()` = 세트 있음 + 살아 있음 + 안 오름 + 폼 핑거 아님 + `IsAiming` + 춤 아님. `FireAnimationHold` 0.35 → 0.6초(연사 사이에 자세가 풀리지 않게).
- `URRRandomBodyAnimInstance`: 게임 스레드 `UpdateAim` → 프록시 `EvaluateAim`/`PointGun`.
  - 가중치 `AimInTime` 0.1 / `AimOutTime` 0.25초. 서 있으면 조준 대기와 사격(발사부터 다시 재생, `FireHold` 0.3초) 섞음. 움직이면(`AimRunSpeed` 150 에서 다 넘어감) 바라보는 쪽 대비 이동 각도로 앞/뒤/왼/오른 달리기 가중치(`DirectionBlendSpeed` 6 로 부드럽게), 네 개 같은 위상, 재생 속도 = 지상 속도 / 가중 평균 애니메이션 속도(거인 스케일 포함, 0.6~1.8). 탈것 위에선 다리 정지(서 있는 조준).
  - 포즈는 직접 섞음(`GetAnimationPose` + `AccumulateWithShortestRotation`) 후 기본 애니메이션 위에 `BlendWith`.
  - `PointGun`: 오른손바닥(`RightHand`~`RightHandIndex1` 중간)→`LeftHand` 방향을 메시 공간에서 재서, 바라보는 쪽과의 요 차이를 `Spine`/`Spine1`/`Spine2` 에 0.3/0.3/0.4 로 나눠 돌리고, `Spine2` 에서 피치를 수평으로 + 반동(`KickAngle` 6°, `KickTime` 0.16초, 달릴 때만 — 서 있는 사격 애니메이션엔 자체 반동). 머리는 원래 총을 따라 보므로 그대로 앞을 봄.
- `ARRRandomCharacter::UpdateLocomotionAnimation`: 세트가 있으면 기본 애니메이션에서 사격을 빼고(대기/걷기만), 메시 사격 자세 회전 `FireStanceYaw` 38° 를 조준 가중치 × (1 - 달리기 가중치) 만큼 — 서 있으면 몸 전체를 틀고, 달리면 다리는 이동 방향 그대로 척추만. `UpdateVehicleHandling` 에서 `bOrientRotationToMovement` = 탈것 || !조준 → 쏘는 동안 쏘는 쪽을 본 채 옆/뒤로 이동(전엔 샷 사이에 이동 방향으로 돌아 640°/s 로 흔들림).
- 총구: `URRRandomGunComponent` 가 조준 가중치 0.95 이상일 때 매 프레임 실제 총구 끝을 캐릭터 기준(회전 제거, 크기로 나눔)으로 기억(`GetAimMuzzleOffset`, 총 모델 바뀌면 초기화, `MuzzleFollowSpeed` 20). `ShootAt` 이 그 자리에서 쏨(처음엔 `MuzzleOffset`). 높이 판정도 그 총구 높이. 서버는 안 그리는 캐릭터도 뼈를 갱신(`AlwaysTickPoseAndRefreshBones`, 권한 있을 때만). 조준 세트가 있으면 예전 "총을 손에서 떼어 총알 선에 맞추기"(`AimBlend`)는 안 함.
- 총 크기/쥐는 자리(`GunLooks`: 길이, 총구 끝, 쥐는 자리 — 100 길이 메시 기준): 권총 65 (-30,0,30), SMG 75 (-25,0,5), 미니건 90 (-22,0,27), 라이플 `GunLength` 120 → 95 (손잡이), 블래스터 75 (-50,0,28), 대포 90 (-22,0,29). Mixamo 자세가 총을 뺨 높이·몸 가까이 들어서 통통한 장난감 총은 몸통이 큰 머리 속에 묻히고 위 카메라에선 총구가 머리에 가려졌음 → 몸통을 쥐도록 아래로, 뒤쪽을 쥐도록 앞으로.
- 확인 (임시 `-RRAimTest`: 무적, 탄 무한, 앞 -Y 로 계속 사격, 1.2초마다 정지/±X/±Y 이동, 옆 카메라 또는 `-RRAimTop` 위 카메라; 확인 후 지움): 이동 방향 전부에서 총 방향 (0.03, -0.999, 0.01~0.1)(반동 포함) = 사격 방향. 옆에서 보면 여섯 총 모두 가슴 높이 수평, 총알이 총구 앞에서 같은 높이로 나감. 위 카메라에서 여섯 총 모두 총구가 머리 앞으로 보임. 라이플 서서 쏠 때 기억된 총구 (70.5, 16.5, -0.4) — 예전 측정 (70, 12, 2) 과 거의 같음. 일반 실행 로그에 에러 없음.
- 확인 못 한 것: 네트워크 참가자 화면(시뮬레이트 프록시도 같은 계산이지만 총알은 서버 총구 위치에서 복제), 거인 크기의 조준 달리기, 탈것 위 사격 모습.

## 15차 덧붙임 — 승리 춤 (사용자: `House Dancing.fbx` 를 넣고 승리 여우가 승리 포즈로)

- 15차 커밋에 포함. 원본 `C:\work\char-pipeline\input\fox\mixamo_dance\House Dancing.fbx`(Mixamo With Skin, 같은 여우 메시 9,451 정점, 뼈 41개).
- `C:\work\char-pipeline\scripts\ue_import_fox_anim.py <fbx>|<이름>[|inplace]`: 애니메이션만 `SK_Fox_Skeleton` 으로 `/Game/Characters/Fox/Anims/A_Fox_Victory` 에(26.17초, 786키). 엉덩이 시작=끝(0,0)이라 제자리 보정은 0, 춤 중 엉덩이는 메시 기준 x -35~0.6, y -7~21 정도 돌아다님(게임에선 ×1.8). 꼬리 뼈는 트랙이 없어 기본 포즈 + 코드 꼬리 흔들림.
- `ARRRandomCharacter::VictoryAnimation`, `IsCelebrating()` = 게임 끝 + 살아 있음 + (이긴 팀이거나 MVP). `UpdateLocomotionAnimation` 이 그때 다른 애니메이션 대신 이것을 반복. 총(`URRRandomGunComponent`)은 쓰러졌을 때처럼 춤추는 동안 숨김(총 컴포넌트가 매 틱 스스로 보임을 정하므로 그쪽 조건에 추가).
- 확인 (창 캡처 `C:\work\char-pipeline\output\screens\victory1`, `-RRScoreToWin=2`): 빨강 승, MVP 와 옆 빨강 여우가 총 없이 춤(프레임마다 자세 다름), 폭죽/결과 UI 그대로, Enter 로 새 게임(`new match`). 빌드 경고 없음.
- 확인 못 한 것: 춤의 전체 26초(캡처는 앞 6초쯤), 진 팀 MVP 의 춤, 네트워크.
- 수정 (사용자: 승리 포즈로 넘어갈 때 하이라이트가 하늘을 비춤 + 승리 포즈 캐릭터는 모두 정면):
  - 원인: `SetViewTargetWithBlend` 는 위치와 회전을 따로 직선 보간해서, 높은 탑다운 카메라(피치 -60, 7,900 거리, FOV 20) → MVP 정면(피치 -2, 900) 사이 1초쯤에 시선이 MVP 를 벗어나 물/하늘만 보였음(재현 캡처 `w_1000`).
  - 카메라 전환을 `ARRRandomResultsDirector` 가 직접: 컨트롤러가 `PlayerCameraManager->GetCameraCacheView()`(지금 화면)를 `Setup(MVP, 시작 뷰, ResultsBlendTime)` 에 넘기고 블렌드 없이 `SetViewTarget`. 1.6초 동안(EaseInOut 2) MVP 를 중심으로 궤도 — 피치/요/거리 보간(위에서 내려오기만 해서 하늘을 안 봄), 시선은 시작 카메라가 보던 점 → MVP 로 옮김, FOV 20 → 42, 시작 카메라의 포스트 프로세스(툰)를 복사해 가중치 1 → 0(예전 블렌드와 같은 모습). 엄폐물 피하기(`GetClearReach`)는 진행만큼 섞어서 첫 프레임이 원래 화면과 같게. 폭죽 방향은 최종 카메라 요(`StartYaw`) 기준.
  - 정면: `Setup` 이 최종 카메라 자리(흔들림 없는)를 모든 캐릭터에 `SetCelebrationViewPoint` 로 알려 주고, 춤추는 동안(`CurrentAnimation == VictoryAnimation`) 메시를 그쪽으로 초당 `CelebrationTurnRate` 360° 돌림(`CelebrationTurn`, 메시 상대 회전 — 복제 안 되는 각 기기 연출, 액터 회전/이동 복제는 그대로). 진 팀(춤 안 춤)은 그대로.
  - 확인 (창 캡처, F6): 전환 중 0.4초/1초/1.7초 모두 MVP 가 화면 안(하늘만 보이는 프레임 없음), 1.7초쯤 정면. 임시 로그(확인 후 지움)로 3초 시점 춤추는 셋의 몸 방향 vs 카메라 방향 차이 -6°, -1°, -15°(카메라 좌우 흔들림만큼), 진 팀 셋은 그대로. 빌드 경고 없음.
  - 확인 못 한 것: 거인 MVP, 네트워크 참가자 화면. (내가 MVP 가 아닐 때는 아래 수정에서 확인)
- 수정 (사용자: 팀원이 카메라를 가림): 각도 고르기(`GetViewOpenness`)가 엄폐물/언덕(WorldStatic)만 봐서, 카메라 바로 앞 팀원이 화면을 크게 가렸음(캡처 `win1/w_4000`, `win4/w_4000` 오른쪽 앞 큰 여우).
  - `GetBrawlerReach(View, Reach)`: MVP 말고 살아 있는 캐릭터마다 캡슐 반경 + `BrawlerClearance` 70(춤에서 엉덩이가 ~60 벗어남, 귀/팔) + 25 원기둥으로, 카메라가 Reach 뒤에 있을 때 ① MVP 바로 앞을 가리거나(시선 축에서 반경 안) ② MVP 에서 카메라 쪽으로 `MinBrawlerPull` 300 넘게 나와 있으면서 **실제 화면(가로 FOV 42, 16:9) 안**에 들어오면 → 그 앞까지 거리. MVP 옆(300 안)에 선 캐릭터는 같이 찍히는 것이라 안 셈.
  - 원래 열린 정도 검사의 다섯 선은 MVP→카메라 쪽으로 넓어지는 모양이라(화면은 반대로 카메라 쪽이 좁음) 카메라 근처 캐릭터를 놓쳐서, 캐릭터는 선 대신 위 화면 판정으로. 열린 정도 = min(엄폐물 평균, 캐릭터 거리 / Distance).
  - 어느 각도도 안 비면: `Setup` 에서 흔들림 양 끝까지 한 번 계산한 `BrawlerReach` 로 카메라를 그 캐릭터 앞까지 당김(흔들리며 들락날락 안 하게 고정; 300 보다 가까우면 안 당김). 정면 바라보기 지점도 그 거리 기준. 로그에 `, in front of a brawler`.
  - 확인 (창 캡처, F6): 카운트다운 중(팀원이 옆에 줄 섬) — 수정 전 yaw 255 에서 오른쪽 앞에 팀원 크게 → 수정 뒤 yaw 105 `view 100% open`, 화면에 MVP 만. 35초 플레이 뒤(봇 MVP, 진 팀) — 전환 중 하늘 없이 MVP 로 넘어가고 끝 화면 가림 없음. 빌드 경고 없음.
  - 이 작업 중 사용자 실행이 안 됨: 스마트 앱 컨트롤이 10:57 빌드 DLL 을 막음(로그 `GetLastError=4551`, CodeIntegrity 3033/3077, 아래 참고 메모와 같은 문제). 다시 빌드한 DLL 은 통과.

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
