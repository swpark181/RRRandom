# 여우 모션 매칭 작업 정리

2026-11-20 작업. 여우 동물친구가 언리얼 5.8 Pose Search(모션 매칭)로 움직이게 한 과정과 현재 상태를 정리한 문서입니다.
게임 기본값으로 켜져 있고, **아직 커밋하지 않았습니다.**

관련 페이지

- 조사(라오어2, 모션 매칭 발표 비교, 외부 데이터): https://claude.ai/artifact/15AvcFLNqgyuxnLjd6QUe9
- 적용 결과(움직이는 그림, 클립 표, 오르기, 숙임): https://claude.ai/artifact/JkMJKqH7cY1mqYMyAFRJBF
- 기존과 지금 비교(모델·얼굴·동작): https://claude.ai/artifact/Thv5P49BdQv9NVVTsU3dby

---

## 1. 한눈에

| 항목 | 상태 |
|---|---|
| 대상 | 여우만. 다른 동물친구는 세트가 없어 기존 동작 그대로 |
| 기본값 | 켜짐 (`rr.MotionMatch` 1). 모든 모드, 여우 분신 포함 |
| 데이터 | Game Animation Sample의 서 있는 이동 158 클립(585초)을 여우로 리타깃 |
| 맡는 범위 | 엉덩이·다리·척추·목·머리. 팔과 꼬리는 기존 동작 |
| 추가 동작 | 달릴 때 상체 22도 숙임, 낮은 곳·높은 곳 오르기 클립 |
| 기존 층 | 조준, 사격, 근접, 꼬리, 시선, 탈것, 이모트는 손대지 않음 |
| 프레임 | BR 테스트 맵 여우 12명: 켬 81.0 / 끔 80.0 fps (루트 본 수정 전 측정) |
| 커밋 | 안 함 |

## 2. 켜고 끄기

| 스위치 | 뜻 |
|---|---|
| `rr.MotionMatch 0` | 끔. 기존 단일 클립 동작으로 |
| `-RRNoMotionMatch` (실행 옵션) | 끔 |
| `rr.MotionMatch.Upper 0` | 엉덩이·다리만 맡김 (기본 1 = 척추·목·머리까지) |
| `rr.MotionMatch.Lean <도>` | 달릴 때 상체 숙임 각도. -1 = 내장 값(22도) |
| `rr.MotionMatch.Log 1` | 로그에 `MOTIONCLIP`(고른 클립 이름), `MOTIONTRAJ`(속도·가중치·궤적 거리), `MOTIONMATCH`(엉덩이 좌표) 출력 |
| 치트 `RRClimbTest <높이>` | 서 있는 자리에서 강제로 오르기 (기본 90) |

## 3. 구조

```
ARRRandomCharacter
 ├─ BodyMesh (보이는 몸)            URRRandomBodyAnimInstance
 │     기본 포즈 ← 구동 몸체의 본별 로컬 변환 (팔·꼬리 제외)
 │     그 위에 조준·근접·꼬리·시선 등 기존 층
 ├─ MotionDriver (숨은 몸체)        SK_FoxMM + ABP_FoxMM
 │     Motion Matching 노드 → Pose History 노드 → 출력
 │     부모 클래스 URRRandomMotionMatchInstance 가 매 프레임 궤적을 만들어 핀으로 넘김
 └─ ClimbDriver (두 번째 숨은 몸체)  SK_FoxMM, 단일 클립 모드, 오를 때만 틱
```

- **구동 몸체** (`ARRRandomCharacter::UpdateMotionDriver`): `/Game/Characters/<이름>MM/SK_<이름>MM` 과 `ABP_<이름>MM` 이 있으면
  보이지 않는 `USkeletalMeshComponent` 를 붙입니다. 호스트에서는 항상, 그 밖에는 몸이 화면에 보일 때만 포즈를 계산합니다.
  전용 서버에서는 만들지 않습니다.
- **포즈 받기** (`URRRandomBodyAnimInstance::UpdateMotion`, 프록시 `EvaluateMotion`): 본 이름으로 짝을 짓고 마스크를 만듭니다.
  - 마스크 1: 매칭 중 항상 받음 (엉덩이, 다리, 척추, 목, 머리)
  - 마스크 2: 포즈를 통째로 받을 때만 (팔. `Upper 0` 이면 척추도 여기)
  - 마스크 0: 받지 않음 (꼬리, 짝이 없는 본)
- **언제 쓰나** (`WantsMotionMatch`): 땅 위에서 대기·걷기·달리기·사격 중일 때만. 탈것, 로봇, 비행, 점프, 착지, 이모트, 오르기,
  궁극기, 대포, 대시 중에는 꺼지고 0.2초에 걸쳐 섞입니다.
- 매칭 중에는 기존의 하체 따로 돌리기(`bSeparateBody`)와 조준 달리기 다리(`StrafeWeight`)를 끕니다.
- 클립의 루트 모션은 포즈에서 빼서 버립니다. 이동은 지금처럼 캡슐이 합니다.

## 4. 속도 맞춤

여우는 다리 길이에 비해 빠릅니다. 걷기 300 을 메시 배율 1.8 로 나누면 167 인데, 샘플의 질주 클립이 156 입니다.

- 클립을 1.5배속으로 재생합니다 (`RRMotionMatch::PlayRate`, 노드의 PlayRateMultiplier). 노드가 0.8~1.25배 안에서 더 조절합니다.
- 검색에 넣는 궤적 거리를 `1 / (메시 배율 × 1.5)` ≈ 0.370 으로 줄입니다 (`DistanceScale`).
- 노드의 궤적 자동 생성은 속도 배율을 지원하지 않아, C++ 에서 궤적을 만들어 핀으로 넘깁니다.
- 발 위치만 검색에 쓰고 발 속도는 쓰지 않습니다. 1.5배속이라 속도가 맞지 않기 때문입니다.

## 5. 데이터와 만드는 순서

원본은 Game Animation Sample (`C:\work\GameAnimationSample`, UE 5.8) 입니다. Fab 의 "완성 프로젝트"라 런처에
"프로젝트에 추가"가 없어, 샘플 프로젝트 안에서 여우로 리타깃한 뒤 폴더를 복사했습니다.

스크립트는 `C:\work\char-pipeline\scripts` 에 있고 `RRRandom\Tools\char-pipeline\scripts` 에 사본이 있습니다.
언리얼 스크립트는 `mm_run_ue.sh <uproject> <로그> <최대 초> "<스크립트와 인자>"` 로 돌립니다. 커맨드렛이 끝나도 종료되지 않아,
로그에 `MMDONE` 이 찍히면 프로세스를 끝냅니다.

| 순서 | 어디서 | 스크립트 | 하는 일 |
|---|---|---|---|
| 0 | Blender | `blender_add_root_bone.py <리그 in> <리그 out>` | 엉덩이 위에 루트 본 추가. 결과 `char-pipeline\input\fox_mm\fox_rig_root.fbx` |
| 1 | 샘플 프로젝트 | `mm_gasp_import_fox.py <fox_rig_root.fbx>` | `/Game/Characters/FoxMM/SK_FoxMM` 가져오기, IK 리그 자동 생성 |
| 2 | 샘플 프로젝트 | `mm_gasp_dump_dbs.py <out dir>` | 샘플의 Pose Search 데이터베이스 목록 → `output\mm_logs\gasp_dbs.json` |
| 3 | 샘플 프로젝트 | `mm_gasp_retarget_fox.py <gasp_dbs.json> <out json>` | 리타게터 만들고 158 클립을 `Anims\A_FoxMM_*` 로 |
| 3' | 샘플 프로젝트 | `mm_gasp_retarget_extra.py "<폴더>\|<원본>=<새 이름>\|..."` | 낱개 클립 (오르기 등)을 따로 폴더에 |
| 4 | 파일 복사 | — | `Content\Characters\FoxMM` 을 RRRandom 으로 복사 |
| 5 | RRRandom | `mm_rr_check_fox.py` | 복사한 세트가 열리는지 확인, 여우 머티리얼 입힘 |
| 6 | RRRandom | `mm_rr_build_set.py Fox` | `PSS_FoxMM`(스키마), `PSD_FoxMM`(데이터베이스), `ABP_FoxMM` 생성 |

보조: `mm_gasp_list_traversal.py`(샘플의 장애물 넘기 클립 목록), `mm_export_fbx.py`(메시와 함께 FBX 내보내기,
`MM_RHI=-AllowCommandletRendering` 필요), `mm_dump_pose.py`(클립의 루트·엉덩이 좌표), `blender_anim_sheet.py`(클립 연속 그림).

클립 구성: 대기, 제자리 돌기, 걷기·달리기·질주의 반복·출발·정지·방향 전환. 샘플의 Sparse 세트와 대기 클립입니다.
반복 클립은 `Walk_Loop`·`Run_Loop` 의 `B, BL, BR, F, FL, FR, LL, RL` 과 `Sprint_Loop` 의 `F, FL, FR` 이 들어 있습니다.

데이터베이스의 애니메이션 목록은 비공개 프로퍼티라 파이썬으로 넣을 수 없습니다. 6번은 C++ 에디터 도구
`URRRandomMotionMatchLibrary::BuildSet` 를 부릅니다.

### 세트를 다시 만들 때 반드시

**`ABP_FoxMM.uasset`, `PSD_FoxMM.uasset`, `PSS_FoxMM.uasset` 을 디스크에서 먼저 지우고** `mm_rr_build_set.py` 를 돌립니다.
에디터 안에서 옛 블루프린트를 지우고 같은 이름으로 다시 만들면 블루프린트 컴파일에서 크래시가 나고(두 번 겪음),
그때 데이터베이스와 스키마는 이미 지워져 게임의 여우가 깨집니다. 스크립트는 이제 세 파일이 남아 있으면 멈춥니다.

### 다른 동물친구에 적용하려면

**호랑이에 적용함(2026-11-20)**: `RR_MM_NAME=Tiger` 로 1·3·3'번을 돌리고(스크립트가 이제 이름을 받음), Anims·Traversal·SK·Skeleton 을 `Content\Characters\TigerMM` 으로 복사한 뒤 `mm_rr_build_set.py Tiger`. 자세한 것은 HANDOFF 의 "호랑이 교체" 절.

`/Game/Characters/<이름>MM` 에 `SK_<이름>MM`, `Anims`, `PSS/PSD/ABP_<이름>MM` 이 있으면 코드는 그대로 동작합니다.
필요한 것은 그 동물친구의 리그에 루트 본을 넣는 것(0번)과 1~6번을 그 이름으로 돌리는 것입니다.
지금 스크립트 이름과 일부 경로에는 여우가 박혀 있어 손봐야 합니다.

## 6. 상체 숙임

샘플 클립은 맨손으로 곧게 서서 뛰는 동작입니다. 총을 들고 달리기에는 너무 꼿꼿해서 숙임을 넣었습니다.

- 척추 세 본을 0.3 / 0.3 / 0.4 몫으로 앞으로 숙입니다 (`EvaluateMotion`).
- 걷기 속도에서 22도 (`MotionRunLean`). 속도에 비례하고, 목과 머리가 70% 를 되돌려 앞을 봅니다.
- 조준하면 조준 동작이 상체를 덮어 숙임이 사라집니다.
- **22도는 다시 봐야 합니다.** 이 값은 루트 본 버그 때문에 잘못된 클립이 나오던 그림을 보고 고른 값입니다.
  앞으로 달리는 클립은 자체로 조금 숙여져 있어, 지금은 14도가 기존 걸음과 비슷해 보입니다.

## 7. 오르기 동작

기존 오르기는 0.45초(위로 60% + 가장자리 넘기 40%) 동안 몸이 그대로 미끄러져 올라갔습니다. 여기에 샘플의 클립을 붙였습니다.

| 클립 | 원본 | 쓰는 때 | 잰 시각 (오르기 시작 / 다 올라옴 / 올라섬) |
|---|---|---|---|
| `Traversal\A_FoxMM_ClimbLow` | 샘플 Mantle 1m | 오르는 높이 110 이하 | 0.38 / 0.68 / 1.30초 |
| `Traversal\A_FoxMM_ClimbHigh` | 샘플 Climb 2.5m | 오르는 높이 110 초과 | 0.46 / 1.39 / 2.13초 |

- 클립의 루트 모션에서 세 시각을 재고 (`MeasureClimbClip`), 캡슐의 두 구간에 맞춰 재생 속도를 바꿉니다.
- 올라선 뒤 0.3초 동안 1.5배로 이어 재생하며 걷기로 돌아갑니다.
- 두 번째 숨은 몸체 `ClimbDriver` 에서 재생하고, 보이는 몸이 그 포즈를 팔까지 통째로 받습니다.
- `Anims` 폴더 밖에 있어 데이터베이스에는 들어가지 않습니다.

### 7-1. 총 잡는 왼손 (2026-11-20)

사용자: "총기류에 손 위치를 … 체크해두고 모션 매칭을 사용해서 해당 위치에 손을 맞쳐줘. 지금은 공중에 뜨거나 허공에 손을 올리는 장면이 많이 나온다."

- 샘플 클립은 맨손 동작이라 매칭만으로는 손을 총에 맞출 수 없다. 그래서 매칭(다리·엉덩이·상체) → 조준·숙임 → 맨 끝에 왼팔 IK 순서로 둔다.
- 총마다 왼손 자리(`FRRGunLook::Support` / `SupportTurn`, `RRRandomGunComponent.cpp`)를 `char-pipeline\scripts\blender_gun_support.py` 로 재서 정했다. IK 는 `URRRandomBodyAnimInstance::EvaluateGunHands`.
- 끄기 `rr.Gun.TwoHands 0`, 게임 중 조정 `RRGunSupport`. 자세한 내용은 HANDOFF 의 "총 잡는 왼손" 절, 결과 페이지 https://claude.ai/artifact/FKkYK2dgZaN52zZp2MZvyb .

## 8. 확인한 것

모션 매칭은 **로그의 클립 이름으로 확인합니다.** 정지 화면으로는 다리가 움직이는 것만 보여 잘못된 클립을 걸러내지 못합니다.

| 상황 | 나온 클립 | 판정 |
|---|---|---|
| 서 있기 | `Stand_Idle_Loop` 52/52 | 맞음 |
| 앞으로 달리기 | `Run_Reface_Start` → `Run_Start_F` → `Run_Loop_F` → `Run_Stop_F` | 맞음 |
| 조준, 제자리 | `Walk_Stop_RR_Rfoot` 잠깐 → `Stand_Idle_Loop` | 대체로 맞음. 시작할 때 발을 한 번 고쳐 디딜 수 있음 |
| 조준, 앞으로 | `Run_Loop_F` → `Run_Stop_F` | 맞음 |
| 조준, 왼쪽으로 | `Run_Start_LL` → `Run_Loop_LL` | 맞음 |
| 조준, 오른쪽으로 | `Run_Pivot_LR_RR_Lfoot` 뒷부분 → `Run_Stop_RL` | 방향은 맞음. 전용 반복 클립이 없음 (9번) |
| 조준, 뒤로 | `Run_Reface_Start_B` → `Run_Loop_B` | 맞음 |
| 자동차·오토바이 탑승 | 타는 동안 매칭 꺼짐, 내리면 켜짐 | 기존 탑승 자세 그대로 |
| 낮은 곳 오르기 (90) | `ClimbLow` | Studio 에서 로그와 그림, Dice Town 에서 로그로 확인 |
| 높은 곳 오르기 (180) | `ClimbHigh` | Studio 에서 로그와 그림으로 확인 |

테스트 실행 옵션: Studio `-RRMap=Studio`, 메인 모드 `-RRThirdPerson -RRMap=DiceTown -RRRule=Rounds`,
BR 테스트 `-RRRule=BattleRoyale -RRBRTest [-RRBotSpecies=Fox]`.
캡처는 `capture_game_seq.ps1 -Species Fox -Tag <이름> -Seq "<초:명령;...>"`, 로그는 `RRRandom\Saved\Logs\cap_<이름>.log`.

## 9. 겪은 문제와 고친 방법

1. **루트 본 방향.** 넣은 루트 본이 다른 Mixamo 본처럼 세워져 있었습니다(언리얼에서 roll 90). Pose Search 는 궤적을
   루트 본 공간에서 읽고 XY 만 쓰므로, 앞뒤 이동이 세로축으로 빠져 검색에서 사라졌습니다. 서 있든 걷든 처음 잡힌
   `Run_Loop_B` 에 머물렀습니다. 루트 본을 눕혀 언리얼에서 회전 (0,0,0) 이 되게 고치고 세트를 처음부터 다시 만들었습니다.
   그 전까지의 그림과 "확인"은 전부 잘못된 클립이었습니다.
2. **여우가 바닥 아래로 꺼짐.** 리타깃의 Root Motion 단계가 원본의 골반을 루트로 써서 여우 루트가 골반 높이(92)에 떠 있었습니다.
   원본 루트를 `root` 로 지정하고 바닥에 붙였습니다. 이동 거리만 보면 정상이라 놓쳤고, 첫 프레임 루트 높이 검사를 넣었습니다.
3. **세트 다시 만들 때 에디터 크래시.** 5번의 "세트를 다시 만들 때 반드시" 참고.
4. **메시와 함께 FBX 내보내기가 `-nullrhi` 에서 크래시.** `MM_RHI=-AllowCommandletRendering` 으로 돌립니다.
5. **리타게터를 지우고 다시 만들면 실패.** 있으면 불러와서 씁니다.

## 10. 남은 것

고칠 것

- **오른쪽 옆걸음 반복 클립이 없음.** 샘플에는 `Run_Loop_LR`, `Run_Loop_RR`, `Walk_Loop_LR`, `Walk_Loop_RR` (그 밖에
  `_offset`, `F_L_20`, `F_R_20`, `Strafe_*`)가 있지만 Sparse 데이터베이스에 없어 빠졌습니다. 리타깃해 `Anims` 에 넣고
  세트를 다시 만들면 됩니다.
- 높은 곳 오르기가 3~4배속으로 재생되어 급합니다. 오르는 시간 자체는 건드리지 않았습니다.
- 오를 때 손이 턱에 정확히 닿지 않습니다.
- 다른 플레이어 화면에서는 오르기 동작이 나오지 않습니다 (오르기 상태가 전달되지 않음. 전에도 미끄러져 올라갔음).
- 걸음 보정(Stride / Orientation Warping)이 없습니다.

확인하지 않은 것

- 30명 규모 큰 맵의 프레임. 동물친구마다 숨은 몸체가 하나씩 늘어납니다. 프레임 수치는 루트 본 수정 전 측정입니다.
- 검, 궁극기, 킥보드, 비행기, 로봇, 온라인.
- 사격 중 총 위치와 상·하체 이음매의 "보이는 느낌". 클립 이름과 정지 화면 몇 장으로만 봤습니다.
- Fab 표기 "Allows usage with AI: No". 라이선스 본문은 읽지 않았습니다.

정해 주실 것

- 숙임 22도 그대로 둘지, 14도로 낮출지.
- 높은 곳 오르기 시간을 늘릴지.
- 장애물을 뛰어넘는 동작(반대편으로 내려가기)을 만들지. 지금 게임에는 그 이동이 없습니다.
- 다른 동물친구로 넓힐지.

## 11. 파일 위치

코드 (`C:\work\RRRandom\Source\RRRandom`)

- `Public|Private\RRRandomMotionMatching.h|.cpp` — 구동 몸체의 애님 인스턴스, 세트 만드는 에디터 도구 (새 파일)
- `Private\RRRandomCharacter.cpp` — `UpdateMotionDriver`, `WantsMotionMatch`, `StartClimbMotion`, `UpdateClimbMotion`, `CheatClimb`
- `Private\RRRandomBodyAnimInstance.cpp` — `UpdateMotion`, `EvaluateMotion`, 숙임, 로그
- `Private\RRRandomPlayerController.cpp` — 치트 `RRClimbTest`
- `RRRandom.Build.cs`, `RRRandom.uproject` — PoseSearch 모듈과 플러그인

에셋

- `C:\work\RRRandom\Content\Characters\FoxMM\` — `SK_FoxMM`, `Anims`(158), `Traversal`(2), `PSS/PSD/ABP_FoxMM`. 약 116MB, git 추적 전
- `C:\work\GameAnimationSample\Content\Characters\FoxMM\` — 리타깃 원본 (IK 리그, 리타게터 포함)
- `C:\work\char-pipeline\input\fox_mm\fox_rig_root.fbx` — 루트 본을 넣은 여우 리그

결과물

- 캡처 `C:\work\char-pipeline\output\screens\fox_mm\`
- 움직이는 그림 `C:\work\char-pipeline\output\fox_mm\gif\`
- 파이프라인 로그 `C:\work\char-pipeline\output\mm_logs\`

빌드

```
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" RRRandomEditor Win64 Development "-Project=C:\work\RRRandom\RRRandom.uproject" -WaitMutex -NoHotReloadFromIDE
```

## 12. 조사에서 가져온 생각

- 라오어2 는 모션 매칭을 세트 단위(탐색 달리기, 긴장 달리기, 엄폐 등)로 나눠 쓰고, 근접 공격은 타임라인 태그가 붙은 데이터로 다룹니다.
- 언리얼 5.8 의 Pose Search 는 이 설치본에서 실험 단계가 아닙니다. 그래서 자체 구현 대신 엔진 기능을 썼습니다.
- 동작 데이터는 외부 "댄스 카드" 대신 Game Animation Sample 을 썼습니다. 영상에서 모캡을 뽑는 AI 도구로 클립을 모아
  같은 순서(리타깃 → `Anims` → 세트 다시 만들기)로 넣을 수 있습니다.
