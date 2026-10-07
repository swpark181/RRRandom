# RRRandom

무해한 랜덤 게임. 언리얼 엔진 5 C++ 프로젝트, 3D 탑뷰(쿼터뷰) 시점.

## 조작

| 입력 | 동작 |
| --- | --- |
| W A S D | 걸어다니기 |
| 마우스 왼쪽 클릭 | 커서 방향으로 원거리 공격 (누르고 있으면 자동 연사, 한 탄창 15발) |
| R | 재장전 (탄창이 비면 자동으로 재장전) |
| Space | 점프. 엄폐물 앞에서 누르면 위로 기어 올라감 |
| E | 모아둔 주사위를 전부 굴리기. 주사위 1개당 공격력/공격속도/이동속도 중 하나 +5~30% (15초), 또는 1/6 확률로 **무기 주사위**가 나와 새 무기를 받음, 1/10 확률로 **거인 주사위**가 나와 거인이 됨 |
| F1 / F2 / F3 | 온라인 게임 열기(호스트) / 찾아서 참가 / 나가기 (아래 [온라인 플레이](#온라인-플레이)) |
| Esc | 타이틀 화면으로 (온라인 게임 중이면 나가면서) |

## 타이틀 화면

게임(`-game` 실행)은 타이틀 화면에서 시작합니다. 임시 화면이라 캔버스에 직접 그리고 에셋이 없습니다.

- **SINGLE PLAY** — 혼자 플레이 (나 + 아군 봇 2 vs 적 봇 3)
- **NETWORK** — **HOST GAME**(게임 열기) / **JOIN GAME**(찾아서 참가) / **BACK**. 위에 EOS로 할지 LAN으로 할지 표시됩니다.
- **QUIT** — 종료

W/S 또는 화살표로 고르고 Enter/Space로 누릅니다. 마우스를 올리면 선택되고 클릭하면 눌립니다.
Esc는 NETWORK 화면에서 뒤로 가기, 참가/열기 진행 중에는 취소입니다. 진행 상황과 실패 이유(`No game found...`, `Disconnected...`)는 버튼 아래 노란 줄에 나옵니다.

- 타이틀 맵은 엔진의 빈 맵 `/Engine/Maps/Entry` 이고, `DefaultEngine.ini` 의 `GameModeMapPrefixes` 가 이름이 `Entry` 로 시작하는 맵에 `RRRandomTitleGameMode` 를 씁니다. 플레이 맵은 `DefaultGame.ini` 의 `ArenaMap`(지금은 `Template_Default`) 입니다. 우리 맵을 만들면 `ArenaMap` 을 바꿉니다.
- 에디터에서 Alt+P(PIE)는 지금 열린 레벨에서 바로 플레이하므로 타이틀을 거치지 않습니다. 또 PIE에서는 Esc가 플레이 종료라서 타이틀로 가려면 콘솔 명령 `RRTitle` 을 씁니다.
- 실행 옵션 `-RRSolo`, `-RRHost`, `-RRJoin` 을 붙이면 타이틀을 건너뜁니다.

## 기본 플레이 (브롤스타즈식 팀전)

- 3 대 3: 파랑 팀은 플레이어 스타트에서, 빨강 팀은 화면 위쪽 2000 거리에서 시작합니다. 혼자 하면 나 + 아군 봇 2 vs 적 봇 3 입니다. 온라인에서는 플레이어가 파랑, 빨강 순서로 번갈아 들어가 봇 자리를 대신합니다.
- 체력 100, 이동 속도 300. 공격은 FPS처럼 누르고 있으면 0.15초마다 한 발씩 나가고, 총알은 초속 5000으로 날아갑니다. 한 탄창 15발, 예비 탄약은 무한이고 재장전은 1.5초에 탄창 전체를 채웁니다. 사거리는 1300입니다.
- 맵 가운데에 엄폐물(회색 블록)이 있습니다. 총알과 캐릭터 모두 통과하지 못하므로 뒤에 숨으면 공격을 피할 수 있습니다. 레벨에 `RRRandomCover` 를 직접 배치하면 자동 배치는 하지 않습니다.
- 엄폐물 앞에서 Space를 누르면 위로 기어 올라갑니다 (발 위 200 높이까지, 0.45초). 점프 중에 엄폐물에 닿아도 0.5초 안이면 올라갑니다. 올라가는 동안은 쏠 수 없습니다.
- 엄폐물 위에서는 가장자리 밖으로 걸어 나가면 아래로 떨어집니다. 바닥에서는 지금처럼 맵 끝에서 멈춥니다. 점프로 맵 밖에 떨어지면 쓰러진 것으로 칩니다.
- 엄폐물 위에서 쏘면 커서가 가리키는 바닥 쪽으로 총알이 비스듬히 내려가서 아래층 캐릭터를 맞힐 수 있습니다. 커서를 캐릭터 위에 올리면 어느 층에 있든 그 캐릭터를 바로 겨눕니다. 반대로 아래에서 위에 선 캐릭터를 노리면 위로 쏩니다 (최대 50도). 같은 층끼리는 그대로 수평으로 쏩니다. 봇도 위아래에 있는 적을 향해 기울여 쏩니다.
- 3초 동안 쏘지도 맞지도 않으면 체력이 초당 15%씩 회복됩니다.
- 체력 0이 되면 쓰러졌다가 4초 뒤 시작 위치에서 다시 일어납니다. 쓰러뜨릴 때마다 상대 팀 점수 +1 (화면 위 `BLUE 0 : 0 RED`).
- 쓰러지면 받고 있던 주사위 버프는 사라지고, 모아둔 주사위는 남습니다.
- 공격속도 버프는 연사 간격과 재장전 속도 둘 다 빠르게 합니다.
- 봇도 주사위를 모으고 굴립니다. 적이 사거리 근처에 오면 굴리고, 싸움이 없어도 2~4개 쌓이면 굴립니다.
- 봇은 조심스럽게 싸웁니다. 적을 처음 보면 잠깐 반응 시간을 두고, 3~5발씩 끊어 쏘고, 엄폐물에 가려지면 쏘지 않습니다. 재장전하거나 다쳤을 때는 엄폐물 뒤로 숨고, 주변에 적이 없으면 미리 재장전합니다.

### 랜덤 무기

- 주사위를 굴릴 때 각 주사위가 1/6 확률로 무기 주사위(`4 GUN`)가 됩니다. 무기 주사위가 나오면 새 무기를 받고 탄창이 가득 찹니다. 여러 개 나오면 가장 높은 눈으로 한 번만 받습니다.
- 무기 종류마다 장단점이 있습니다. 눈이 높을수록 데미지가 조금 더 좋아집니다 (1이면 약 x0.91, 6이면 약 x1.21, 여기에 ±10%).

  | 무기 | 데미지 | 탄창 | 연사 간격 | 총알 속도 | 총알 크기 |
  | --- | --- | --- | --- | --- | --- |
  | PISTOL (시작) | x1 | 15 | x1 | x1 | x1 |
  | MINIGUN | x0.5 | 약 60 | x0.4 | x0.9 | x0.8 |
  | SMG | x0.65 | 약 30 | x0.55 | x1 | x0.8 |
  | RIFLE | x1.15 | 약 20 | x1.1 | x1.25 | x1 |
  | BLASTER | x1.6 | 약 10 | x1.8 | x0.7 | x1.5 |
  | CANNON | x2.4 | 약 5 | x3.5 | x0.45 | x2.2 |

  탄창은 ×0.75~1.3, 총알 속도는 ×0.85~1.15 범위에서 무기마다 랜덤하게 달라집니다.
- 한 발 데미지가 높을수록 등급이 오르고 이펙트가 화려해집니다 (Niagara).
  - COMMON (x1.1 미만): 이펙트 없음
  - RARE (x1.1 이상, 파랑): 총알이 파란 빛을 내고, 맞은 곳에 작은 불꽃
  - EPIC (x1.5 이상, 보라): 더 밝은 빛, 총구 불꽃, 꼬리, 큰 불꽃 폭발
  - LEGEND (x2.1 이상, 금색): 가장 밝은 빛, 큰 총구 불꽃, 꼬리, 맞은 곳에 폭발과 불꽃 고리
- 쓰러지면 시작 무기(PISTOL)로 돌아갑니다.

### 거인화

- 주사위를 굴릴 때 각 주사위가 1/10 확률로 거인 주사위(`5 GIANT`, 하늘색)가 됩니다. 나오면 그 캐릭터가 거인이 됩니다. 여러 개 나오면 가장 높은 눈으로 한 번만.
- 거인: 몸 전체(충돌 캡슐 포함)가 **1.5배**, 주는 데미지 **x1.5** (파워), 받는 데미지 **x0.5** (방어력). 공격력 버프, 무기 배율과 곱해집니다. 이동 속도는 그대로입니다.
- 지속 시간은 6초 + 눈 1개당 1초 (7~12초). 거인일 때 또 나오면 남은 시간과 새 시간 중 긴 쪽으로.
- 0.3초에 걸쳐 커지고 줄어들며, 발은 바닥에 붙은 채입니다. 쓰러지면 거인이 끝나고 (쓰러지는 몸은 큰 채로) 부활할 때 원래 크기입니다.
- 키가 커서 엄폐물(높이 160) 위로 몸이 드러나 숨기는 어렵습니다.
- 머리 위에 `GIANT` 칩 (3초 남으면 깜빡임), 내 캐릭터면 왼쪽 아래에 `GIANT  ATK x1.5  DMG TAKEN x0.5  8s`.
- 조절: 캐릭터의 `GiantScale`, `GiantDamageMultiplier`, `GiantDamageTakenMultiplier`, `GiantBaseDuration`, `GiantSecondsPerPip`, `GiantGrowTime`, 주사위 컴포넌트의 `GiantDieChance`.

### 머리 위 표시 (모든 캐릭터)

```
   5 ATK  3 MOVE        ← 방금 굴린 주사위 (1.6초)
       [GIANT]          ← 거인일 때
  [ATK+25][MOVE+15]     ← 받고 있는 버프 합계 (3초 남으면 깜빡임)
        42              ← 체력
  [CANNON][■■■■■■□□] [2] ← 무기(등급 색, 시작 무기면 안 보임) + 체력 바 (나 초록 / 아군 파랑 / 적 빨강) + 모아둔 주사위 개수
  ▮▮▮▮▮▮▮▮▮▮▯▯▯▯▯        ← 탄창 15칸, 재장전 중엔 진행 바 (내 캐릭터만)
  ‾‾‾‾‾‾‾‾‾‾            ← 다음 주사위까지 게이지
```

화면 왼쪽 아래에는 내 주사위 게이지(`DICE xN`)와 버프 목록이 남은 시간과 함께, 오른쪽 아래에는 지금 무기(`LEGEND CANNON  DMG x2.8  SHOT SPD x0.5`)와 남은 탄약(`12 / 15`, 재장전 중엔 `RELOADING`)이 표시됩니다. 탄창이 20발보다 크면 머리 위 탄창은 칸 대신 한 줄 바로 나옵니다.

## 처음 열기

1. Unreal Engine 5.8과 Visual Studio 2022(“C++를 사용한 게임 개발” 워크로드)를 설치합니다.
   다른 5.x 버전을 쓰면 `RRRandom.uproject`의 `EngineAssociation` 값을 그 버전으로 바꾸거나,
   `.uproject` 파일 우클릭 → *Switch Unreal Engine version* 을 씁니다.
2. `git lfs install` 을 한 번 실행합니다. 맵과 에셋(`.uasset`, `.umap`)은 Git LFS로 관리합니다.
3. `RRRandom.uproject` 를 더블클릭합니다. 모듈을 빌드할지 물으면 *예* 를 누릅니다.
   (또는 우클릭 → *Generate Visual Studio project files* 후 `RRRandom.sln` 에서 `Development Editor` 로 빌드)
4. 처음에는 엔진 기본 레벨(`Template_Default`)이 열립니다. 바로 플레이(Alt+P) 해볼 수 있습니다 (타이틀 화면은 `-game` 실행 때만 나옴).
   레벨에 과녁(`RRRandomDummy`)이 하나도 없으면 시작 위치 앞에 3개가 자동으로 세워집니다.
5. Rider로 열 때 "로드 실패"가 나면 .NET 10 SDK가 설치되어 있는지 확인합니다. UE 5.8의 UnrealBuildTool은 .NET 10이 필요합니다.

## 우리 맵 만들기

1. *File → New Level → Basic* 으로 새 레벨을 만들고 `Content/Maps/Main` 으로 저장합니다.
2. *Edit → Project Settings → Maps & Modes* 에서 *Editor Startup Map* 을 `Main` 으로 바꿉니다.
   *Game Default Map* 은 타이틀(`Entry`) 그대로 두고, `Config/DefaultGame.ini` 의 `ArenaMap` 을 `/Game/Maps/Main` 으로 바꿉니다.
   (바뀐 ini 파일을 같이 커밋해 주세요.)
3. 과녁은 *Place Actors* 에서 `RRRandomDummy` 를 찾아 원하는 곳에 끌어다 놓습니다.
   자동 배치는 게임 모드의 `bSpawnDummies` 를 켰을 때만 합니다(기본 꺼짐, 팀전 한가운데에 서 있게 되므로).
4. 팀 인원과 팀 시작 거리는 게임 모드의 `TeamSize`(기본 3), `TeamSpawnDistance` 로 바꿉니다.
   `bSplitPlayersAcrossTeams` 를 끄면 온라인 플레이어가 모두 파랑 팀에 들어가 봇과 싸웁니다.
   바닥이 없는 자리에는 아무도 세우지 않고 로그에 경고를 남깁니다.

## 온라인 플레이

전용 서버 없이, 플레이어 한 명이 호스트(리슨 서버)가 되고 나머지가 접속합니다.
체력, 탄약, 무기, 주사위, 쓰러짐, 점수, 봇은 모두 호스트가 계산하고 다른 플레이어에게 복제합니다.

| 키 | 동작 |
| --- | --- |
| 타이틀 NETWORK → HOST GAME, 또는 F1 | 게임 열기. 플레이 맵을 리슨 서버로 (다시) 불러옴 |
| 타이틀 NETWORK → JOIN GAME, 또는 F2 | 게임을 찾아 첫 번째에 참가 |
| F3 | 나가서 다시 혼자 플레이 |
| Esc | 나가서 타이틀로 |

화면 왼쪽 위에 연결 상태(`SOLO` / `HOSTING (LAN) 2 PLAYERS` / `ONLINE (EOS) ...`)와 마지막 진행 상황(노란 줄)이 나옵니다.
콘솔 명령 `RRHost`, `RRJoin`, `RRLeave`, `RRTitle` 도 같고, 실행 옵션 `-RRHost`, `-RRJoin` 을 붙이면 타이틀을 건너뛰고 시작하자마자 열기/참가합니다.

### 바로 테스트하기 (LAN, 설정 필요 없음)

EOS 키가 비어 있으면 엔진이 EOS 대신 Null 서브시스템을 쓰고, 게임은 LAN 브로드캐스트로 찾아 IP(포트 7777)로 접속합니다.
같은 PC 두 창이나 같은 공유기의 PC끼리 바로 됩니다. 실행 옵션 `-RRLan` 을 붙이면 EOS가 설정돼 있어도 LAN을 씁니다.

```powershell
$ue = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$proj = "C:\work\RRRandom\RRRandom.uproject"
& $ue $proj -game -windowed -ResX=960 -ResY=540 -WinX=0 -WinY=0 -RRHost     # 호스트
& $ue $proj -game -windowed -ResX=960 -ResY=540 -WinX=960 -WinY=0 -RRJoin   # 참가
```

다른 PC에서 접속할 때 Windows 방화벽이 UnrealEditor 허용을 물으면 허용합니다.
게임 로직만 볼 때는 에디터에서 *Play → Net Mode: Play As Listen Server*, *Number of Players: 2* 로도 됩니다 (세션 없이 바로 연결).

### EOS로 인터넷 플레이

EOS는 P2P 연결을 중계해 주므로 포트 포워딩이 필요 없습니다. 무료입니다.

1. [Epic Developer Portal](https://dev.epicgames.com/portal)에서 제품을 만들고 *Product Settings* 에서 Product ID, Sandbox ID, Deployment ID를 확인합니다.
2. *Product Settings → Clients* 에서 클라이언트를 만듭니다. Client Policy는 **Peer2Peer**. Client ID와 Client Secret을 받습니다.
3. *Epic Account Services* 에서 애플리케이션의 Permissions(Basic Profile, Online Presence, Friends)를 켜고, Linked Clients에 2번 클라이언트를 연결합니다.
4. `Config/DefaultEngine.ini` 의 `+Artifacts=(...)` 줄을 채웁니다. `ClientEncryptionKey` 는 64자리 16진수 아무 값이면 됩니다.
   ```powershell
   -join ((1..64) | ForEach-Object { '{0:X}' -f (Get-Random -Maximum 16) })
   ```
   Client Secret이 이 파일에 들어가므로 공개 저장소라면 이 줄은 커밋하지 마세요.
5. 포털에서 EOS SDK를 받아 `SDK/Tools` 의 Dev Auth Tool을 실행합니다. 포트 `6547` 로 열고 Epic 계정으로 로그인해 이름 `Player1` 로 credential을 추가합니다.
   같은 PC에서 두 번째 게임을 띄우려면 다른 Epic 계정을 `Player2` 로 추가하고, 그 게임에는 `-RRDevAuth=Player2` 를 붙입니다.
6. 한쪽에서 HOST GAME(F1), 다른 쪽에서 JOIN GAME(F2). 왼쪽 위에 `HOSTING (EOS)` / `ONLINE (EOS)` 가 보이면 성공입니다.
   실패하면 노란 상태 줄과 `Saved/Logs/RRRandom.log` 의 `LogEOS`, `LogOnline` 줄을 봅니다.

- 친구 PC도 각자 Dev Auth Tool로 로그인합니다. 브랜드 심사를 받기 전에는 조직(Organization) 멤버 계정만 로그인될 수 있으니, 안 되면 친구 계정을 조직에 추가합니다.
- 로그인 방식은 `Config/DefaultGame.ini` 의 `LoginType` 으로 바꿉니다 (`accountportal` 은 Epic 로그인 창). `-AUTH_TYPE=... -AUTH_LOGIN=... -AUTH_PASSWORD=...` 실행 옵션이 있으면 그것을 씁니다.

### 동작 방식과 제약

- 호스트는 혼자 할 때와 같은 게임을 돌리고, 플레이어가 들어오면 파랑, 빨강 순서로 봇 자리를 하나씩 대신합니다. 플레이어가 나가면 봇이 다시 그 자리를 채웁니다.
- 참가자는 이동을 언리얼 기본 캐릭터 이동 예측으로 하고, 연사 쿨다운만 자기 쪽에서 계산해 발사·재장전·주사위·오르기를 호스트에 요청(RPC)합니다. 탄약과 무기는 호스트 값이 돌아와 표시됩니다.
- 엄폐물 오르기는 캐릭터 이동 예측에 포함되지 않아서, 참가자가 오르는 동안과 끝난 뒤 0.5초는 호스트가 그 참가자의 위치를 그대로 믿습니다. 치트 방지가 약하므로 친구끼리 하는 것을 전제로 합니다.
- 호스트가 나가면 게임이 끝나고 참가자는 타이틀 화면으로 돌아갑니다 (`Disconnected: ...` 표시, 호스트 이전 없음).
- 과녁(`RRRandomDummy`)은 아직 동기화하지 않습니다 (기본 꺼짐).

## 코드 구조

캐릭터와 과녁은 엔진에 들어 있는 언리얼 마네킹(`/Engine/Tutorial/.../TutorialTPP`)과 Idle/Walk 애니메이션을 써서 별도 에셋이 필요 없습니다.
마네킹 자체 머티리얼은 색을 바꿀 수 없어서, 스켈레탈 메시에 쓸 수 있고 `DiffuseColor` 파라미터가 있는 `/Engine/TemplateResources/M_Template_Master` 를 입힙니다.

- `ARRRandomGameMode` — 호스트에만 있음. 기본 폰과 컨트롤러 지정. 레벨에 엄폐물이 없으면 `CoverLayout` 대로 세움. 팀마다 `TeamSize` 개 자리를 두고 플레이어가 먼저 차지(`SpawnDefaultPawnFor`), 나머지는 봇으로 채움. 플레이어가 나가면(`Logout`) 봇이 대신함. 쓰러뜨린 수로 팀 점수를 올림. `bSpawnDummies` 면 과녁도 세움
- `ARRRandomGameState` — 팀 점수. 모든 플레이어에게 복제됨
- `URRRandomSessionSubsystem` — 온라인 플레이와 화면 이동. EOS 로그인, 세션 만들기/찾기/참가/나가기, 리슨 서버로 맵 열기, 혼자 플레이(`PlaySolo`), 타이틀로(`ReturnToTitle`), 진행 취소(`CancelPending`). EOS를 못 쓰면 LAN. 게임 인스턴스에 붙어 있어서 맵이 바뀌어도 남음
- `ARRRandomTitleGameMode` / `ARRRandomTitlePlayerController` / `ARRRandomTitleHUD` — 타이틀 화면. 폰 없음. 컨트롤러가 메뉴 상태(메인/NETWORK, 선택된 버튼)와 키/클릭을 처리하고 세션 서브시스템을 부름. HUD가 캔버스에 버튼과 상태를 그리고 마우스가 올라간 버튼을 선택함
- `ARRRandomCharacter` — 플레이어와 봇이 같이 쓰는 탑뷰 마네킹 캐릭터. 팀, 체력/회복, 점프/엄폐물 오르기(`TryClimb`), 엄폐물 위에서만 가장자리로 걸어 내려가기, 15발 탄창/자동 연사/재장전(`StartReload`), `FireAt` 으로 투사체 발사, 쓰러짐(래그돌)과 시작 위치 부활. 체력/탄약/무기/생존 여부를 복제하고, 참가자는 `Server...` RPC로 발사/재장전/주사위/오르기를 요청
- `ARRRandomAIController` — 봇. 보이는 적을 우선으로 가장 가까운 적을 쫓아 자기 선호 거리를 유지하며 좌우로 움직임. 엄폐물에 가리지 않을 때만 반응 시간 뒤 짧게 끊어 쏘고, 재장전 중이거나 체력 45% 아래, 또는 맞고 있는데 체력 70% 아래면 적 반대편 엄폐물 뒤로 숨음. 적이 안 보이면 미리 재장전. 싸움이 시작되면 주사위를 굴림. 내비메시 없이 직접 이동하고 앞에 엄폐물이 있으면 옆으로 비켜 감
- `ARRRandomCover` — 엄폐물. 엔진 기본 큐브로 만든 블록이라 에셋이 필요 없음. 총알과 캐릭터를 막음. 피벗이 바닥 가운데라 `Size` 만 바꾸면 바닥에 선 채로 커짐. 호스트가 세운 엄폐물이 참가자에게도 같은 크기로 복제됨
- `ARRRandomPlayerController` — WASD 이동, 왼쪽 클릭 공격(누르고 있으면 자동 연사, 커서 아래 바닥/엄폐물 위 높이를 겨눔), R 재장전, Space 점프, E 주사위, F1/F2/F3 온라인 열기/참가/나가기, Esc 타이틀로. 입력 에셋이 지정되지 않으면 코드에서 만들어 씀
- `ARRRandomProjectile` — 직선으로 날아가 맞은 대상에 랜덤 데미지(8~15)를 줌. 같은 팀과 다른 투사체는 통과하고, 사거리만큼 날면 사라짐. 쓰러진 과녁 래그돌은 밀어냄. 무기 등급에 따라 빛(포인트 라이트), 총구/꼬리/맞은 곳 Niagara 이펙트를 붙임 (`TierLooks` 표). 호스트만 만들고 맞힘을 판정함. 참가자 화면의 총알은 충돌 없이 보이기만 하고, 맞은 위치가 복제되면(`bImpacted`) 이펙트를 냄. 내 총알은 노란색, 나머지는 팀 색
- `FRRWeapon` (`RRRandomWeapon.h`) — 무기 하나를 캐릭터 기본값에 곱하는 배율로 표현. `Roll` 이 무기 종류와 눈금으로 랜덤 무기를 만들고, 한 발 데미지로 등급을 정함
- `ARRRandomDummy` — 과녁 마네킹. 체력 100, 맞으면 빨갛게 번쩍이고 데미지 숫자가 뜸. 0이 되면 래그돌로 쓰러졌다가 3초 뒤 제자리에서 일어남
- `URRRandomDiceBuffComponent` — 시간에 따라 게이지를 채워 주사위를 쌓고, E로 굴리면 눈금만큼 시간제 버프를 줌. 같은 능력치 버프는 더해짐. 무기 주사위가 나오면 `OnWeaponDie` 로 알려서 캐릭터가 무기를 바꾸고, 거인 주사위가 나오면 `OnGiantDie` 로 알려서 캐릭터가 거인이 됨. 게이지, 주사위, 버프, 굴림 결과는 호스트가 계산해 복제함
- `ARRRandomHUD` — 캔버스에 단순하게 그림. 모든 캐릭터 머리 위에 무기, 체력, 주사위 개수/게이지, 버프, 굴림 결과, 데미지 숫자를 표시하고, 화면에는 팀 점수, 내 주사위 패널, 남은 탄약, 부활 카운트다운, 왼쪽 위 온라인 상태를 표시
- `URRRandomEventComponent` — 무해한 랜덤 효과(색, 크기, 점프, 속도, 회전)를 골라 적용. 원래 Space에 붙어 있었지만 Space가 점프가 되면서 지금은 입력에 연결돼 있지 않음 (`RollRandomEvent` 를 부르면 동작). 몸 색 머티리얼과 속도 배율은 여전히 이 컴포넌트를 거침
