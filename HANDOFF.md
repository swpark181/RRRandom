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

## 7차 — 거인화

- 아직 커밋 안 됨 (6차와 함께).
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

- 아직 커밋 안 됨 (5차는 `82f6d28` 로 커밋됨).
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
| 주사위 | 5초마다 1개, 버프 15초, 눈 1개당 +5% | `URRRandomDiceBuffComponent` |
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

위에서 아래로: 굴림 결과(`5 ATK  3 MOVE`, 1.6초) → 버프 칩(능력치별 합계, 3초 남으면 깜빡임) → 체력 숫자
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
- 버프는 시간제(15초). 무한히 강해지는 것을 막기 위함.
- 봇은 내비메시 없이 `AddMovementInput` 으로 직접 이동. 앞 120 거리를 구체로 스윕해서 엄폐물이 있으면 35/70/105/140도씩 꺾어 비켜 감 (지난번에 비켜 간 쪽을 먼저 시도). 엄폐물이 복잡해지면(ㄷ자 벽 등) 갇힐 수 있으니 그땐 내비메시 필요.
- 시야/엄폐 판정은 모두 `WorldStatic` 오브젝트 대상 스윕 (총알 높이 = 캡슐 중심 +20, 반지름 12). 엄폐물은 `BlockAll` 이라 `WorldStatic`. 바닥도 `WorldStatic` 이지만 수평 스윕이라 안 걸림.
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
