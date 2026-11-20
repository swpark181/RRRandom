# 시네마틱 시퀀스 계획 (2026-11-19 검토)

언리얼 시퀀서로 RRRandom 동물친구 영상을 만드는 방안을 검토한 기록입니다. 아직 아무것도 만들지 않았습니다.
토큰·시간·크레딧 숫자는 전부 측정값이 아닌 추정이며 ±50% 오차를 봐야 합니다.
토큰은 매 턴 다시 읽는 캐시 입력을 포함한 총 처리량입니다.

## 1. 현재 프로젝트 상태

- 엔진 UE 5.8. 시퀀서 관련 플러그인(SequencerScripting, MovieRenderPipeline, LevelSequenceEditor 등)은 엔진에 있지만
  `RRRandom.uproject`에서 켜져 있지 않습니다. 레벨 시퀀스 애셋은 0개입니다.
- 맵(`Content/Maps/*.umap`)은 전부 57KB 안팎의 빈 껍데기이고, 다이스 타운 같은 세트는 게임 실행 중 C++로 지어집니다.
  에디터에서 시퀀서를 열면 배경이 없으므로, 게임을 실행한 상태에서 렌더하거나 촬영용 세트를 레벨에 따로 구워야 합니다.
- 캐릭터 애니메이션 수: RedFox·Fox·Rabbit·Squirrel·Tiger·Dragon 각 33~40개, Rat·Cat 각 7개.
  - RedFox: 춤 4, 이모트 6(Cheer, Clap, Dance, Laugh, Taunt, Wave), Victory, Defeat 3, 검 동작 8, Fire, Run, Walk, FlyBack 등.
  - Tiger: Run, Roar, JumpAttack, HammerSwing 3, Shield, 검 동작, 이모트 등.
- 게임 쪽 재료
  - 여우 궁극기 "여우 둔갑술"(`ARRRandomFoxClones`): 연기와 나뭇잎이 터지며 분신 등장(`ARRRandomCharacter::MakeDecoyOf`).
  - 궁극기 주사위(`URRRandomDiceBuffComponent`, 황금 주사위 연출), `Content/Effects/DiceFX`.
  - 권총 `Content/Props/GunsR90/SM_Pistol`.
- 기존 촬영 도구: `Tools/char-pipeline/scripts/ue_game_seq.py`(`RR_SEQ` 명령 시퀀스), `RRStudioCam`, `-RRMap=Studio`.

## 2. 만드는 방법

1. `.uproject`에 SequencerScripting과 MovieRenderPipeline 플러그인을 켭니다.
2. 파이썬 스크립트로 레벨 시퀀스를 만듭니다: 캐릭터별 샷(캐릭터 + 애니메이션 트랙 + 시네 카메라 컷)과 이를 잇는 마스터 시퀀스.
3. Movie Render Queue로 프레임을 렌더하고 ffmpeg로 mp4를 만듭니다.
   실시간 화면 녹화(지난 하이라이트 영상의 gdigrab)와 달리 끊김 없이 매번 같은 결과가 나옵니다.

**핵심 제약**: 궁극기·무기 이펙트·해태 합체는 C++ 게임 로직이라 시퀀서 애니메이션 트랙만으로는 재생되지 않습니다.
필요하면 시퀀서는 카메라만 맡고 동작은 `RR_SEQ`로 실제 게임에서 실행하는 혼합 방식이나, 이펙트를 시퀀서 타임라인에 직접 배치하는 방식을 씁니다.

## 3. 캐릭터 소개 영상 비용 추정 (참고용, 처음 검토한 안)

| 단계 | 내용 | 토큰 | 시간 |
|---|---|---|---|
| A. 쇼케이스 | 6종, 캐릭터당 10초: 등장 → 모션 2~3개 → 이름 자막, 카메라 회전·달리 | 150만~300만 | 1.5~3시간 |
| B. 연출 소개 | A + 궁극기·무기 이펙트, 팀 컬러 배경, 타이틀 카드, 해태 로봇 | 500만~1,000만 | 5~8시간 |
| C. 시네마틱 | 6종 × 10~15초(총 60~90초), 조명·심도·전용 연기 | 1,000만~2,000만 | 작업일 2~4일 |

## 4. 시네마틱 수준의 가능 범위

**할 수 있는 것**
- 시네 카메라의 렌즈·심도·포커스 이동, 달리·크레인, 컷 편집.
- 샷별 조명(림라이트, 안개, 팀 컬러), Movie Render Queue의 고샘플 안티에일리어싱과 모션 블러.
- 기존 모션 + Mixamo 연기 모션(캐릭터가 Mixamo 리그라 바로 적용 가능), 나이아가라 이펙트 타이밍 배치.

**한계**
- 얼굴: Tripo 모델 + Mixamo 자동 리그에는 얼굴 뼈나 표정 모프가 없는 것으로 보입니다(메시를 열어 확인하지는 않음).
  눈·입이 텍스처로 그려진 통짜 메시라면 3D 표정 툴을 쓰기 전에 눈꺼풀과 입 안을 먼저 모델링해야 합니다.
- 맞춤 연기는 기성 모션을 이어 붙여 근사하는 수준입니다. Tripo Animate는 팔이 몸에 붙는 문제가 있어 블렌더 손질이 필요합니다.
- Claude는 영상을 재생해 볼 수 없고 프레임을 이어 보며 확인합니다. 움직임의 리듬과 컷 호흡은 사람이 보고 피드백해야 합니다.
- 음악·효과음은 Claude가 만들 수 없습니다. 곡을 주면 박자에 맞춰 편집할 수는 있습니다.

## 5. 얼굴 표정 방식

**결정: 텍스처 표정 교체** (사용자 승인 2026-11-19)
- 캐릭터마다 표정 그림 4~6장을 Tripo 이미지 편집으로 만들고, 시퀀서에서 머티리얼 파라미터를 키프레임해 컷마다 바꿉니다.
- 메시와 리그를 건드리지 않고, 카툰 스타일에 잘 맞습니다.
- 위험: Tripo UV가 조각나 있으면 얼굴 영역만 교체하기 어렵습니다. 그 경우 머리 정면에서 표정 그림을 투영하는 머티리얼로 풉니다.
  한 마리로 먼저 확인해야 합니다.

**검토한 3D 표정 툴** (2026-11 검색 기준)

| 종류 | 이름 | 내용 | 상태 |
|---|---|---|---|
| AI 연구 | OmniFaceRig (2026-06) | 입 안 없는 통짜 메시 → 입 안 포함 FACS 리그(최대 155 블렌드셰이프), 비인간·스타일라이즈드 대상 | 논문·데이터셋만, 코드 공개 확인 못 함 |
| AI 연구 | RigAnyFace (NeurIPS 2025) | 무표정 메시 → FACS 리그, 분리된 눈알 처리 | 코드 공개 확인 못 함 |
| 유료 서비스 | Polywink | 머리 메시 업로드 → 블렌드셰이프/리그, 카툰 지원 주장 | 현재 가격 확인 못 함 |
| 블렌더 애드온 | Faceit | ARKit 52 반자동, 토끼 데모, Blender 3.0~5.2 | 사용 가능 |
| 블렌더 애드온 | FaceKeys, ShapeKeyGen | ARKit 셰이프 키 자동 생성 | 사용 가능 (FaceKeys 문서: 그려진 눈은 깜빡일 수 없고 그려진 입은 열 수 없음) |
| Tripo | — | 몸 자동 리깅만 | 표정 리그 공식 확인 못 함 |

텍스처 표정이 부족하면 다음 선택지는 Faceit을 토끼 한 마리에 시험하는 것입니다.

## 6. 본 작업: 여우 vs 호랑이 10초 시네마틱

**연출**: 여우가 호랑이에게 쫓기다 막다른 골목에 몰림 → 궁극기 주사위가 마침 걸림 → 분신(여우 둔갑술) → 총구를 "후" 불며 씩 웃음.
(2026-11-19 콘티에서 바뀜: 총구 불기 대신 포위해서 겨누며 끝남, 14초 7컷. 8장 참고.)

**방침 (사용자 지정)**
- 애니메이션은 Mixamo나 Tripo로 뽑아서 씁니다.
- 줌 컷의 디테일을 위해 시네마틱 전용 모델을 Tripo로 만듭니다.
- 시퀀서로 약 10초 내외.

**작업별 추정**

| 작업 | 내용 | 토큰 | 시간 |
|---|---|---|---|
| 파이프라인·세트 | 시퀀서·렌더 설정, 다이스 타운 블록으로 막다른 골목 세트 굽기 | 200만~300만 | 2~3시간 |
| 시네마틱 모델 | 여우·호랑이 Tripo 재생성, 리그 이식, 털 셸, 4K 텍스처, 게임 모델과 비교 페이지 | 400만~600만 | 4~6시간 |
| 애니메이션 | 6~8개: 뒤돌아보며 도망, 급정지, 뒷걸음, 호랑이 다가오기·두리번, 총구 불기 | 300만~500만 | 3~5시간 |
| 텍스처 표정 | 여우 4~5장(겁먹음, 결심, "후", 씩 웃음, 눈 감음), 호랑이 3장, 얼굴 투영 머티리얼 | 150만~300만 | 2~3시간 |
| 시퀀스 조립 | 카메라, 조명, 이펙트·분신 배치, 렌더와 수정 반복 | 400만~700만 | 4~6시간 |
| **합계** | | **1,500만~2,500만** | **작업일 3~5일** |

**Tripo 크레딧** (기존 단가: Smart Mesh 100 + 텍스처 30, 모션 20)
- 모델 2종 × 2~3회 시도: 500~800
- 모션 5~8개: 100~160
- 표정 이미지 편집: 단가 미확인, 여유분으로 포함
- 합계 약 1,000 안팎

**위험 요소**
- 총구 불기: Mixamo에 딱 맞는 동작이 없을 가능성이 큽니다. 클로즈업 컷이라 가장 여러 번 고치게 될 부분입니다.
- 모델 재생성: 같은 컨셉으로 다시 뽑아도 얼굴 인상이 달라질 수 있어 게임 모델과 나란히 비교해 골라야 합니다.
- 10초 분량: 5박자라 컷당 2초로 빠듯합니다. 12~15초가 더 자연스럽습니다.
- Mixamo는 캐릭터를 하나만 유지합니다. 새 모델을 올리면 기존 리그가 교체되니 순서를 맞춰야 합니다.
- 음악·효과음은 별도 준비가 필요합니다.

**정할 것**
1. ~~기본 여우(Fox)와 레드폭스(RedFox) 중 누구로 할지.~~ → 기본 여우(Fox)로 결정 (2026-11-19).
2. 모델 방식: "항상 Smart Mesh" 규칙을 유지하고 면 수와 텍스처 해상도만 올리는 것으로 가정. 시네마틱만 HD 예외로 할지.
3. Mixamo 업로드 순서.

## 7. 진행 순서 (추천)

1. **블로킹 버전**: 지금 게임 모델과 기존 애니메이션으로 컷 구성·카메라·타이밍을 확정합니다.
   3~4시간, 300만~400만 토큰 추정(전체의 약 1/6).
2. 블로킹 전후 `/usage`의 주간 막대를 비교해 실측 소모율을 구하고, 전체 작업 기간을 다시 계산합니다.
3. 블로킹에서 클로즈업으로 정해진 컷에 필요한 모델·동작·표정만 고품질로 만듭니다.
4. 최종 조립, 렌더, 피드백 반복.

모델 운용: Opus 5.5 high를 기본으로 하고, 컷 구성 결정과 최종 룩 검토에만 Fable 5.2로 바꿔 씁니다.

## 8. 콘티 확정 (2026-11-19, 2판)

콘티 페이지 https://claude.ai/artifact/6rHqqDG4XUey7MjmxSqLUw, 그림 `C:\work\char-pipeline\input\cinematic_board\picks\cut1~7.png`.
호랑이는 모든 컷에서 게임의 방패(왼팔)와 뿅망치(오른손)를 듭니다. 쏘지 않고 겨눈 채 끝냅니다(사용자 확인).

| 컷 | 시간 | 내용 |
|---|---|---|
| 1 추격 | 0~2초 | 로우 앵글 트래킹. 겁먹은 여우, 뒤에서 호랑이가 뿅망치를 휘둘러 집 모서리를 부숨 |
| 2 막다른 골목 | 2~3.5초 | 측면 고정. 큐브 벽 앞에서 급정지 |
| 3 호랑이 등장 | 3.5~5.5초 | 여우 어깨 너머, 노을 역광. 망치를 어깨에 걸치고 방패를 든 호랑이, 발밑에 블록 잔해 |
| 4 궁극기 주사위 | 5.5~7.5초 | 황금 주사위가 5로 멈춤. 호랑이 멈칫 |
| 5 여우 둔갑술 | 7.5~9.5초 | 연기·나뭇잎, 분신 다섯. 호랑이 두리번 |
| 6 포위, 조준 | 9.5~12초 | 하이 앵글. 여우 여섯이 둘러싸 권총을 겨눔, 호랑이는 방패 뒤에 웅크리고 망치는 바닥에 |
| 7 씩 웃음 | 12~14초 | 익스트림 클로즈업 푸시 인. 겨눈 채 씩 웃음 |

## 9. 블로킹 준비 목록 (2026-11-19)

사용자 결정: 세트는 실제 다이스 타운 골목, 새 동작은 기존 동작 + 키프레임, 이펙트는 게임 코드를 실제로 호출.
결과물은 에디터에서 여는 레벨 시퀀스 애셋이고, 검토용으로 렌더한 영상을 함께 만듭니다.

1. **설정**: `.uproject`에 SequencerScripting, MovieRenderPipeline 켜기, 에디터 재시작과 C++ 빌드. 폴더 `/Game/Cinematics/FoxVsTiger/`.
2. **세트**: 새 레벨에 `ARRRandomDiceTown` 배치(에디터에서도 지어지고, 같은 시드면 게임과 같은 마을). 큰길에서 꺾여 들어가는 좁은 골목을 탑뷰로 2~3곳 찾아 고르기. 골목 끝은 블록을 더 쌓아 막음. 노을 조명(3컷부터 역광).
3. **배우**: 여우 6(본체 + 분신 5, 분신은 시퀀서 배우로 두고 등장 순간에만 게임 연기를 부름), 호랑이 1. 게임과 같은 소켓에 뿅망치·방패·권총.
4. **동작**: 기존 Run, Walk, Idle, AimIdle, HammerSwing1~3, ShieldCarry, ShieldBlock을 섞고, 고개·팔·몸 기울기는 FK 컨트롤 릭으로 키. 뛰면서 휘두르기는 시퀀서에서 상체만 겹치기 어려우니 블로킹에서는 Run과 Swing을 이어 붙이고 발 미끄러짐은 감수.
5. **카메라 7개**(시네 카메라 + 카메라 컷 트랙): 1컷 24mm 로우 트래킹, 2컷 35mm 측면 고정, 3컷 50mm 어깨 너머(초점 호랑이), 4컷 28mm 로우, 5컷 24mm 와이드, 6컷 18mm 하이 앵글, 7컷 85mm 푸시 인. 렌즈 값은 블로킹에서 조정.
6. **게임 이펙트 연결(C++ 조금)**: 시퀀서 이벤트에서 부를 함수 셋. 둔갑술 연기·나뭇잎(AI 분신은 만들지 않음), 황금 주사위(면 5), 블록 부수기(`ApplyWrecks` + 잔해). 이 셋은 에디터에서 스크럽할 때는 보이지 않고 재생(PIE)이나 렌더 때 보일 가능성이 큽니다.
7. **검토용 렌더**: Movie Render Queue 1280x720 24fps → mp4, 구석에 컷 번호와 시간 표시. 컷별 대표 프레임 시트.
8. **블로킹에서 하지 않는 것**: 표정 텍스처, 시네마틱 전용 모델, 새 동작 제작, 소리, 색 보정.
9. **사용자에게 부탁**: 시작 전과 끝난 뒤 `/usage` 주간 막대 % 알려 주기(7장 2번).

## 출처

- [OmniFaceRig (arXiv 2606.08043)](https://arxiv.org/html/2606.08043v1), [프로젝트 페이지](https://omnifacerig.github.io/)
- [RigAnyFace (arXiv 2511.18601)](https://arxiv.org/html/2511.18601v1)
- [Polywink (CG Channel, 2018)](https://www.cgchannel.com/2018/07/polywink-creates-blendshapes-and-rigs-for-any-head-model/)
- [Faceit](https://superhivemarket.com/products/faceit), [Faceit 문서](https://faceit-doc.readthedocs.io/en/latest/)
- [FaceKeys 문서](https://superhivemarket.com/products/facekeys/docs)
- [Shape Key Gen](https://superhivemarket.com/products/shapekeygen)

**블로킹 시작 전 `/usage`** (2026-11-19 13:00 KST): 현재 세션 9%, 이번 주 전체 21%, 이번 주 Fable 31% (주간 초기화 11-22 07:59).

**골목 확정 (2026-11-19)**: 후보 페이지 https://claude.ai/artifact/7Fq2gm6oftv15cLA6RtSkR 에서 "다: 남서쪽 보라 건물 사이" 선택.
게임 다이스 타운 기준 위치(위에서 찍은 사진으로 잰 값, 오차 ±50): 남쪽 순환 큰길 X≈-4,930(Y 방향으로 뻗음),
골목은 두 보라 네모 건물 사이 Y -4,120 ~ -3,690, 건물 앞면 X≈-5,400에서 뒷면 X≈-6,470까지. 끝(X≈-6,500 너머)을 블록으로 막음.
찍는 스크립트 `Tools/cinematic/ue_cine_scout.py`, 사진 `C:\work\char-pipeline\output\cinematic_blocking\scout3\`.

## 10. 블로킹 1차 결과 (2026-11-19)

검토 페이지 https://claude.ai/artifact/EGFEaKTcJS9CdQPAQTpQ8y (7번째 렌더, 14초 7컷, mp4 포함).

- **애셋**: 맵 `/Game/Cinematics/FoxVsTiger/Map_FoxVsTiger`(DiceTown 복사, 게임 모드 GameModeBase), 시퀀스 `LS_FoxVsTiger`(24fps, 0~336), 렌더 프리셋 `MRQ_Blocking720`.
- **C++**: `ARRRandomCineAnimal`(게임 모습·털·소품을 게임 공식대로 손/팔에 붙이는 배우, Hold Pistol/Hammer/Shield 키), `ARRRandomCineDirector`(시퀀스 시간에 맞춰 FoxPoof·UltimateDieSpin/Burst·Wreckage::Wreck 호출, MRQ의 재생 액터를 따라감). Build.cs에 LevelSequence, MovieScene. `.uproject`에 SequencerScripting, MovieRenderPipeline.
- **스크립트** `Tools/cinematic/`: `cine_blocking.ps1 -Take <이름>`(빌드→프리셋→MRQ 렌더→mp4·시트), `ue_cine_build.py`(레벨·배우·카메라·시퀀스를 매번 새로 지음), `ue_cine_render_config.py`, `ue_cine_scout.py`(게임에서 위에서 찍기), `ue_cine_inspect.py`.
- **배운 점**: 파이썬에서 bool 속성은 `hold_hammer`(b 빠짐); 5.8의 PlayRate는 TimeWarpVariant; 키 하나만 있는 채널은 구간 전체에 그 값 → 0프레임에도 키; 가시성 트랙 채널은 "보임"; Wreck 공은 바닥까지 닿아야 블록이 부서짐; 게임 망치 동작(HammerSwing)은 상체용이라 몸 전체에 틀면 눕는다; 커맨드라인 MRQ에서 `{sequence_name}` 토큰이 빈 문자열.
- **남은 차이**: 표정 없음, 망치 어깨 걸치기·웅크리기·급정지 동작 없음, 4컷 섬광이 화면을 덮음, 조명 한낮(노을 없음), 벽 색 회백, 7컷은 분신 둘을 숨긴 눈속임.

**블로킹 실측 사용량** (2026-11-19 13:00 → 14:20, 약 80분, Opus만 사용): 세션 9% → 16%(7%p), 주간 전체 21% → 22%(약 1%p, 반올림 때문에 0~2%p), 주간 Fable 31% 그대로.
블로킹 = 전체의 약 1/6로 보면 남은 작업은 주간 전체 약 5%p(3~12%p), 실작업 약 8시간(1~2일). Tripo 브라우저 작업과 새 동작 손질은 블로킹보다 무거워 범위 위쪽일 수 있음.

## 11. 블로킹 2차 수정 (2026-11-19, 12번째 렌더)

사용자 요청: 주사위 절반(섬광 유지), 4컷에 여우·호랑이 둘 다, 막다른 벽 옆까지 막고 맵 색으로, 호랑이 새 동작, 1컷 부수기 키우기.
- 주사위 지름 40(DIE_ACROSS), 4컷 카메라 오른쪽 위 18mm. 벽 = `ARRRandomCineBlocks`(마을과 같은 SM_BTBlock/SM_BTDieBlock + 인스턴스 색, 7단, 왼쪽 옆벽 2겹, 91블록).
- 1컷 부수기 3번(0.55/1.05/1.55초, 반지름 240, 바닥부터) → 블록 24개.
- 호랑이 동작: Mixamo 검·방패 세트 20개를 지금 Mixamo에 올라가 있는 **토끼 리그(RABBIT_TPOSE_MIXAMO, 골반 26.7)**에서 받아 `ue_cine_import_tiger.py`(ue_import_shared_anim.py 반복)로 호랑이(25.6)에 옮김 → `A_Tiger_Cine_*`. 원본 `char-pipeline/input/tiger_cine/mixamo/`.
  합성 동작은 `ue_cine_merge_anim.py`(언리얼 안에서 하체 = 달리기/걷기, 상체 = 휘두르기/대기): RunAttackD, RunSlashA, WalkShoulder. 블렌더 왕복(`blender_run_swing.py`)은 골반 높이가 -3로 깨져 쓰지 않음.
  쓰는 순서: RunAttackD(0-2초) → SsRunB → WalkShoulder → SsIdleC → LookNervous → SsCrouchblock → SsCrouchblockIdle.
- 오디션: `ue_cine_audition.py` + `cine_blocking.ps1 -NoBuild -Sequence LS_TigerAudition -Take audition`, 시트 `output/cinematic_blocking/render/aud_sheet1|2.jpg`.
- Mixamo 페이지에서 access_token으로 API를 부르는 JS는 권한 분류기가 막음 → 화면 버튼을 JS로 눌러 다운로드(한 번에 3개씩, 45초 제한).

## 12. 블로킹 3차 수정 (2026-11-19, 15번째 렌더)

사용자 요청: 여우 급정지, 호랑이는 여우를 보며 쫓아오며 방해물을 부수기, 둘 사이에 작은 오브젝트, 7컷 정면.
- 여우 급정지: Mixamo "Run To Stop" 등 6종(토끼 리그)을 `RR_CINE_TARGET=Fox=19.8`로 여우에 옮김(`A_Fox_Cine_Stop*`, 원본 `input/fox_cine/mixamo/`). 2컷에 StopE(몸을 낮추는 급정지), 다음 후보 StopC.
- 1컷: 호랑이가 여우 바로 뒤(X -5150) 여우 쪽을 보며 달림, 앞으로 내려치기 `A_Tiger_Cine_RunAttackC`(SsAttackC 상체 + SsRunB 하체) 두 번. 둘 사이 주사위 더미 2개(`Cine_Clutter`, 작은 주사위 0.5 + 캔디 상자 0.7)를 Smash 신호(0.75/1.75초)로 날림. 길가 건물 부수기 신호는 뺌.
- C++: `ARRRandomCineBlocks::SmashAt`(맞은 블록이 던져져 굴러 멈춤, 물리 엔진 없이 매 프레임 계산), 감독에 `Smash` 신호(+AcornPop 먼지).
- 7컷: 웅크린 호랑이 머리 너머 40mm 정면 클로즈업, 분신 숨기기 눈속임 없앰.

## 13. 블로킹 4차 수정 (2026-11-19, 17번째 렌더) + 본 제작 준비 목록

4차: 여우가 급정지 뒤 0.3초에 돌아서 Scared(Mixamo)로 뒷걸음, 호랑이 망치는 9.5초에 손에서 떨어져 앞쪽 바닥(HAMMER_REST), 던져진 잡동사니가 골목까지 날아가지 않게 줄임(ThrowOut 240, ThrowUp 380).
표정 텍스처: `char-pipeline/scripts/blender_expression.py`(render: 게임 리그 FBX + 기본 색 텍스처로 머리 정면 무광 렌더와 카메라 JSON / bake: 편집한 그림과 원본의 차이만 같은 카메라로 투영해 정면·앞면 텍셀에만 칠함) →
Tripo 나노바나나 Pro 1:1 "표정만 바꿔"(여우 4, 호랑이 3, 후보 `input/expressions/cand/`) → `ue_cine_expressions.py`(텍스처 가져오기 + 몸 재질 복제 후 `URRRandomCineLibrary::ReplaceMaterialTexture`) → `ARRRandomCineAnimal.Expression`(정수 키, 털도 따라감).
여우: 0 겁먹음, 1 걱정(3.3초), 2 결심(6.3초), 3 씩 웃음(7.6초~, 분신은 처음부터). 호랑이: 0 음흉, 1 놀람(6.3초), 2 진땀(7.6초).

**본 제작에서 정할 것 / 준비할 것** (사용자에게 전달)
- 모델: 게임 모델 그대로 + 클로즈업 컷만 텍스처 해상도 보강(추천) vs Tripo 재생성(약 1,000 크레딧, Smart Mesh 규칙 예외 여부).
- 출력: 해상도(1080p/4K), 화면비(16:9/2.39 레터박스), 프레임(24/30), 형식(mp4/ProRes), 쓰는 곳(트레일러·게임 인트로·SNS 세로판).
- 조명: 3컷부터 노을 역광으로 바꿀지, 낮 그대로일지. 색 보정 톤.
- 소리: 음악·효과음은 Claude가 만들 수 없음 → 곡과 효과음(뿅망치 삑, 블록 와르르, 펑 연기, 주사위 띵, 권총 찰칵) 파일이 필요. 곡이 있으면 박자에 맞춰 컷 길이 조정.
- 얼굴: 텍스처 표정만으로 갈지, 눈 깜빡임(텍스처 1장씩 추가)·입 움직임까지 할지.
- 마무리: 로고·타이틀 카드·자막 여부, 끝에 "DICE" 문구 등.
- 남은 동작: 2컷 급정지를 옆에서 보이게 카메라 변경, 4컷 호랑이 "멈칫", 여우 결심 제스처, 7컷 권총 겨누는 손 자세, 망치 타격 순간 정밀 맞춤.
- 리소스 현황: Tripo 무료 이미지 422/1000, 크레딧 20,500, Mixamo는 토끼 리그 보유 중(로그인 유지 필요), 렌더는 720p 14초에 약 10초.

## 14. 최종 렌더 (2026-11-19, 음악 전)

사용자: "음악은 나중에 넣을거고 나머지는 추천안으로 진행". 적용한 추천안: 게임 모델 유지(렌더 때 텍스처 스트리밍 끔), 1080p 16:9 24fps mp4,
노을 하나로 통일(해 pitch -9 / yaw 172, 따뜻한 색, 3컷 역광; 골목·길에 따뜻한 보조 조명), 텍스처 표정 + 깜빡임 2장(걱정·씩 웃음 눈 감은 판),
타이틀 카드 없음, 2컷 옆 시점, 호랑이 4컷 멈칫(Mixamo Sword And Shield Impact), 여우 결심(Standing Fist Pump), 6~7컷 두 손 권총 겨누기(Pistol Aim),
1컷 타격 시각 0.9/1.9초(RunAttackC 시작 후 0.9초가 망치가 앞쪽 아래에 닿는 순간).
- 최종본: `C:\work\char-pipeline\output\cinematic_blocking\render\FoxVsTiger_final_1080p.mp4`(12.3MB, crf 18), 웹용 `FoxVsTiger_web.mp4`(6.4MB), 프레임 `render\final1080\`.
- 다시 뽑기: `$env:RR_CINE_FINAL="1"; Tools/cinematic/cine_blocking.ps1 -Take final1080 -Res 1920x1080` (1080p 렌더 약 50초).
- 남은 것: 음악·효과음(사용자가 나중에), 클로즈업 이마의 털 셸 이음매, 커밋.

## 15. 5차 수정: 얼굴 마스크, 호랑이 그림자, 7컷 카메라 (2026-11-19, 최종 2판)

사용자: 3컷 호랑이가 여우 쪽을 봐야 함 / 호랑이 그림자를 길게, 무섭게 / 마지막 장면 이음매 다듬고 카메라를 조금 낮춰 표정이 잘 보이게 / 눈 텍스처가 이상함: 눈 쪽이 평평하도록 얼굴 모델링 수정 / 기본 입이 없어야 함.
- **얼굴 마스크** `char-pipeline/scripts/blender_face_flatten.py`:
  - 먼저 시도한 방식(몸 정점을 매끈한 면으로 옮기기)은 저폴리 눈꺼풀·입술이 접혀 구멍이 생겨 버렸습니다.
  - 지금 방식(`facepatch`):
    - 얼굴 정면에 3px 격자의 매끈한 면(머리의 2차곡면 + 가장자리 나머지의 조화 채움)을 몸에서 0.25% 앞에 둡니다.
    - 주둥이(`follow`)는 원래 모양을 살짝 다듬기만 합니다(쭉 펴면 옆면이 어두워짐).
    - UV는 정면 그림 그대로입니다.
    - 정점색 R은 불투명도이고, 가장자리에서 디더로 사라집니다.
    - 가중치는 바로 뒤 몸 표면 삼각형에서 보간합니다.
    - 튀어나온 부분(눈알·눈꺼풀·입 주름·원래 입)은 마스크보다 3% 뒤로 밀어 넣습니다. 코(`keep`)만 원래 모델입니다.
    - 마스크는 몸 메시에 재질 슬롯 "Face"로 합쳐집니다.
  - 그 밖의 모드:
    - `faceimage`: 바뀐 곳만 후보 그림을 쓰고 나머지는 원래 정면 렌더를 써서 크롭합니다. 결과는 `T_<종>Face_<표정>`입니다.
    - `furfade`: 마스크 안쪽 털을 정점색 G로 지웁니다.
  - 영역 파일: `input/expressions/fox_face.json`, `tiger_face.json`. 결과물: `input/expressions/faces/`.
  - 후보와 표정의 대응: 여우 F0_0 겁먹음, F1_1 걱정, F2_0 결심, F3_0 씩 웃음, blink_A_0 걱정 깜빡임, blink_B_0 씩 웃음 깜빡임. 호랑이 T0_0 음흉, T1_0 놀람, T2_0 진땀.
- **언리얼 가져오기** `Tools/cinematic/ue_cine_faces.py`(레거시 FBX: `-dpcvars=Interchange.FeatureFlags.Import.FBX=0`) → `/Game/Cinematics/FoxVsTiger/Faces/`:
  - `SK_<종>_Cine`, `SK_<종>Fur_Cine`
  - `M_<종>Face_<표정>`: 몸 재질 복제 + 텍스처 교체 + Masked + `DitherTemporalAA`(엔진 머티리얼 함수)
  - 게임 모델은 그대로입니다.
- **C++** `ARRRandomCineAnimal`:
  - `BodyMesh`, `FurMesh`를 지정하면 게임 메시 대신 씁니다.
  - "Face" 슬롯이 있으면 표정 재질은 그 슬롯에만 들어가고, 몸은 게임 재질을 씁니다.
- **3컷 호랑이와 그림자:**
  - 호랑이 몸을 240°로 돌렸습니다. 어깨 걷기가 가슴과 머리를 60° 돌리기 때문입니다.
  - 카메라는 낮게(z62, 30mm) 뒷걸음질하는 여우 어깨 너머에 둡니다.
  - 해를 -4°로 낮추는 방법은 길 건너 건물이 골목 바닥을 다 가려서 실패했습니다.
  - 대신 `Cine_ShadowLight`를 씁니다. 골목 입구 (-5150, +120, 300)에 둔 5000cd 스포트라이트로, 그림자를 드리웁니다.
  - 그 동안 `Cine_FillLight`는 꺼서 그림자를 살립니다.
  - `Cine_TigerFaceLight`는 7° 좁은 각도의 얼굴 조명이며, 3컷에서만 켭니다.
- **7컷:**
  - 카메라를 220에서 138로 낮추고, 대상 높이를 98, y는 +125(호랑이 어깨 너머)로 했습니다.
  - Mixamo 권총 겨누기는 팔이 입을 가려서, 이 컷만 `A_Fox_Cine_PistolAimLow`를 씁니다(`ue_cine_aim_low.py`, LeftArm/RightArm을 컴포넌트 공간 X축으로 -30°).
  - `Cine_FaceKey`는 38cd 그림자 없는 얼굴 보조 조명입니다.
- **확인 도구:** `RR_CINE_CVARS="rr.Fur=0"`(털 끄고 렌더, `ue_cine_render_config.py`)를 쓰면 됩니다.
  - 볼 옆 짙은 그늘은 털이 아니라 주둥이 그늘이며, 이전 판에도 있었습니다.
  - 블렌더 확인 렌더는 `char-pipeline/scripts/blender_face_mask_view.py`(무광 + 정점색 알파)로 합니다. 사용법: `-- <SK_종_Cine.fbx> <기본색> <T_종Face_표정.png> <종_front.json> <출력 접두어>`.
- **최종 2판:** `render/FoxVsTiger_final_1080p_v2.mp4`(11.9MB), 웹용 `FoxVsTiger_web_v2.mp4`(3.3MB), 프레임 `render/final1080b/`.
  - 다시 뽑기: `$env:RR_CINE_FINAL="1"; cine_blocking.ps1 -Take final1080b -Res 1920x1080`. 페이지는 v6입니다.
  - 얼굴을 다시 만들 때: blender_face_flatten facepatch/furfade/faceimage → ue_cine_faces.py → cine_blocking.

## 16. 6차 수정: 새 여우·호랑이, 파쿠르 추격 (2026-11-20, 최종 3판)

사용자: "시네마틱에 변경된 여우와 호랑이 넣어서 변경해줘. 영상에 여우는 장애물을 파쿠르로 뛰어넘고 호랑이는 그걸 이전처럼 부수고 따라오게 해줘."
- **배우:** `ARRRandomCineAnimal` 이 게임 메시(새 여우·새 호랑이)를 그대로 씁니다. `body_mesh`/`fur_mesh` 지정과 얼굴 마스크(`Faces/`)는 쓰지 않습니다(애셋은 남겨 둠).
- **표정(새 모델용, `char-pipeline/input/expressions_new/T_<종>_Expr_<이름>.png`):**
  - 여우: 후보 그림(`input/expressions/cand/`)을 `scripts/blender_image_reframe.py`(새)로 얼굴 audit 틀로 옮긴 뒤, 기본 얼굴과 같은 값으로 `blender_feature_transfer.py`(눈·눈썹 `"655,915,300,320;1393,915,300,320" --src 1024,1083 --dst 1024,1124 --scale 0.982`, 입 `"1023,1525,380,125" --src 1023,1420 --dst 1023,1489 --scale 0.982 --gain 1.0`(벌린 입·이) 또는 `1.4`(선 입)) → `blender_expression.py bake`(fox_tpose_final + 빈 얼굴 기본색 + `face2/blank_tex_front.png`·`blank_cam.json`).
  - 호랑이: 옛 표정 텍스처(`input/expressions/T_Tiger_Expr_*.png`)를 기본 얼굴과 같은 순서로: audit 정면 → `blender_feature_transfer.py … "1040,820,860,650" --shift 0 --feather 60` → bake → `blender_face_carry3d.py` 두 번(턱·목, 5차와 같은 영역) → 수염 색 칸 복사.
  - 언리얼: `RR_CINE_EXPR_SRC=<폴더> ue_cine_expressions.py` → `Expressions/M_<종>_Expr_<이름>` + **`M_<종>Fur_Expr_<이름>`**(털 재질도 같은 텍스처로; 안 하면 입 주변 털이 기본 입을 그림). C++ `ExpressionFurs`.
- **파쿠르:** 샘플의 `M_Neutral_Traversal_Hurdle_1_0_run_F_Lfoot/Rfoot`(+V2 둘)을 `mm_gasp_retarget_extra.py Traversal|…=A_FoxMM_HurdleL|…` 로 여우에 옮겨 `Content/Characters/FoxMM/Traversal/` 에 복사.
  - `SK_Fox_Skeleton.add_compatible_skeleton(SK_FoxMM_Skeleton)` 으로 게임 여우 몸에서 바로 재생(루트 본이 없어 제자리 재생) → 배우 위치는 클립의 루트 모션을 프레임마다 읽어 키(`hurdle_keys`, 배율 1.8, 재생 1.3배, 뛰는 높이 ×1.6).
  - 장애물은 클립의 루트가 가장 높은 지점(Y -2677, -3272)에 놓습니다: 작은 주사위 7개 한 줄 + 양옆 2층 + 양끝 캔디 상자.
- **호랑이:** 같은 길로 286 cm/s, 여우가 넘고 1.15초 뒤(1.77·3.86초)에 `RunAttackC` 의 타격이 닿게 맞춤, `SMASH` 신호 반지름 260.
  합성 동작은 새 뼈대로 다시: `RR_MERGE="A_Tiger_Cine_SsAttackC|A_Tiger_Cine_SsRunB|A_Tiger_Cine_RunAttackC|1.0;A_Tiger_Cine_SsIdleC|A_Tiger_Cine_SsWalkA|A_Tiger_Cine_WalkShoulder|1.0"`.
- **시간:** 추격 5초(카메라 둘: `Cam1a_Chase` 앞에서 물러나며, `Cam1b_Barricade` 둘째 장애물 옆 낮게), 뒤는 전부 +3초(`S = 3.0`). 17초, 408프레임, 컷 8개.
- **7컷:** 새 여우는 주둥이 아래에 입이 있어 카메라를 92 높이로 낮추고, `RR_AIMLOW_DEG=30 RR_AIMLOW_AXIS=-X RR_AIMLOW_EXTRA="Head=-20;Neck=-8" ue_cine_aim_low.py` 로 고개를 듦.
- **결과:** `render/FoxVsTiger_final_1080p_v3.mp4`(15.4MB), 웹용 `FoxVsTiger_web_v3.mp4`(3.2MB), 프레임 `render/final1080c/`. 5차 빌드 스크립트는 `ue_cine_build_v2_1119.py`. 페이지 v7.
- **남은 것:** 호랑이 표정의 입은 조각된 입 위의 그림(입 파츠 변형은 아직 없음), 음악·효과음, 커밋.

## 17. 7차 수정: 호랑이 망치 내려치기 (2026-11-20, 최종 4판)

사용자: "호랑이 망치 제대로 휘두르도록 수정해줘. 지금은 너무 이상하다"
- 원인: 추격의 `Cine_RunAttackC` 는 Mixamo 검 베기 상체라 망치를 옆에서 찌르듯 움직였고, 달리기와 뚝 끊겨 이어졌습니다.
- 바꾼 것: 게임의 뿅망치 동작 상체 + 달리기 하체. `RR_MERGE="A_Tiger_HammerSwing1|A_Tiger_Cine_SsRunB|A_Tiger_Cine_RunHammer1|1.0;…2…;…3…"` → `Cine_RunHammer1`(머리 위로 들었다 앞으로 내리찍기, 1.96초, 타격 0.56초) 채택. 2는 옆으로 휘두르기, 3은 뒤로 돌며 휘두르기. `A_Tiger_JumpAttack` 합성은 몸이 누워 못 씀.
- 오디션: `RR_AUD_ANIMS=<클립들> ue_cine_audition.py` + `cine_blocking.ps1 -NoBuild -Sequence LS_TigerAudition -Take <이름>`(자리 Y -2950 으로 옮김: 장애물과 겹치지 않게). 타격 시점은 한 프레임씩 보고 잼.
- 이음: 달리기와 휘두르기 구간을 0.15초 겹치고 서로 페이드(`anim_sections` 의 5·6번째 값, 겹치는 구간은 다른 행). 구간 easing 은 파이썬에서 못 바꿔 C++ `URRRandomCineLibrary::SetSectionEasing(Section, in초, out초)` 추가.
- 결과: `render/FoxVsTiger_final_1080p_v4.mp4`, 웹용 `FoxVsTiger_web_v4.mp4`, 프레임 `render/final1080d/`. 페이지 v8.

## 18. 8차 수정: 머리 위 궁극기 주사위, 환희 표정, 여우 표정 다시 그림, 호랑이 정면 걷기 (2026-11-21, 최종 5판)

사용자: "궁극기 주사위가 여우 머리위에서 인게임 처럼 나타나고 돌아가도록 수정해줘. 돌아갈때 표정은 환희. 기본 표정 텍스쳐와 어울리게 다른 표정도 다시 그려줘. 호랑이는 장애물 파괴 후 걸어올 때 여우를 보면서 정상적으로 앞을보며 걷도록 해줘. 지금은 대각선으로 이상하게 걷고 있어."
- **주사위:** `URRRandomOverheadDieComponent::TickComponent` 의 움직임을 프레임마다 키로 옮김(튀어나오기 ease-out-back 0.22초, 호 30, 회전은 `(1-a)^3` 로 줄며 최종 각도에 더함, 착지 때 한 번 커짐). 위치 `DIE_FOX`(-6120) 위 205, 크기 80, 1.4초(게임 1.6초), 면 5. 최종 각도는 C++ `URRRandomCineLibrary::DieFaceRotation(면, 카메라)`(= `FaceToward`). `DIE_SPIN`/`DIE_BURST` 신호의 B(빛기둥 시작)는 여우 등 뒤 55(발밑이면 얼굴을 가림). `transform_keys` 가 5번째 값(배율)을 받음.
- **컷:** `Cam4_GoldDie`(18mm, 호랑이 쪽에서 여우 상반신 + 주사위), 착지 0.1초 뒤 `Cam4b_TigerStarts`(28mm, 호랑이 망치 쪽에서 놀라는 얼굴; 방패 쪽에서는 방패가 얼굴을 가림). 컷 9개. `T_DIE = S+5.5`, `T_LAND = T_DIE+1.4`.
- **여우:** `EmoteCheer` + 표정 6(Joy) → 착지 때 `Cine_FistpumpC` + 결심. 호랑이: `SsIdleC` → 착지 때 `SsImpactA` + 놀람.
- **여우 표정 다시 그림:** 지금 얼굴 정면(`blender_face_audit.py fox_tpose_final.fbx T_Fox_face.png … --px 2048`)의 1536 크롭(256,380)을 Tripo 나노바나나 Pro(4K, 무료)에 참고로 넣고 "Change ONLY the facial expression: …"(문구는 `ue_cine_build.py` 옆이 아니라 이 세션 기록; 요지: 그림체·코·눈 위치 유지, 표정만). 눈 감은 판은 고른 걱정/씩 웃음 그림을 다시 참고로 넣고 "Change ONLY the eyes: CLOSED". 결과 `input/expressions_new/redraw/dl/foxexpr6_*.jpeg`(4096²) → 1536 으로 줄여 정면 그림에 덮고 `blender_expression.py bake <fbx> T_Fox_face.png base_tex_front.png front_<X>.png base_cam.json` → `input/expressions_new/T_Fox_Expr_<X>.png`(이전에 옮긴 판은 `carried_1120/`). 무료 14장(532 → 546), 크레딧 0.
- **호랑이 걷기:** `Cine_WalkShoulder`(몸 240°) 대신 게임 `Walk` 0.87배, 몸 180°, (-5330 → TIGER_SPOT) 2.7초. 오디션으로 확인: 검·방패 걷기는 원래 몸을 틀고 걷고, 합성한 어깨 걷기는 상체가 돌아감.
- **회색 프레임(11-19 부터 있던 문제):** 표정 재질이 처음 그려지는 프레임에 기본 재질(회색)로 나옴 — 미쿠킹 실행은 재질 셰이더를 처음 그릴 때 컴파일. 1샘플 검토 렌더에선 한 프레임, 8샘플 최종에선 1/8. `ARRRandomCineAnimal::Tick` 첫 틱에 `CacheShaders(Synchronous)` 로 표정·털 재질을 미리 컴파일해 해결. PSO 사전 캐시 cvar 는 원인이 아니었음(`cine_blocking.ps1` 의 `-dpcvars=r.PSOPrecaching=0` 은 남겨 둠).
- **결과:** `render/FoxVsTiger_final_1080p_v5.mp4`, 웹용 `FoxVsTiger_web_v5.mp4`, 프레임 `render/final1080e/`. 7차 빌드 스크립트 `ue_cine_build_v4_1120.py`. 페이지 v9.
