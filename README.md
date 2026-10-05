# RRRandom

무해한 랜덤 게임. 언리얼 엔진 5 C++ 프로젝트, 3D 탑뷰(쿼터뷰) 시점.

## 조작

| 입력 | 동작 |
| --- | --- |
| 마우스 왼쪽 클릭 | 클릭한 곳까지 걸어감 |
| 마우스 왼쪽 누르고 있기 | 커서를 따라 걸어감 |
| W A S D | 걸어다니기 |
| Space | 랜덤 이벤트 (색 바뀜, 크기 바뀜, 점프, 속도 변화, 빙글빙글) |

## 처음 열기

1. Unreal Engine 5.7과 Visual Studio 2022(“C++를 사용한 게임 개발” 워크로드)를 설치합니다.
   다른 5.x 버전을 쓰면 `RRRandom.uproject`의 `EngineAssociation` 값을 그 버전으로 바꾸거나,
   `.uproject` 파일 우클릭 → *Switch Unreal Engine version* 을 씁니다.
2. `git lfs install` 을 한 번 실행합니다. 맵과 에셋(`.uasset`, `.umap`)은 Git LFS로 관리합니다.
3. `RRRandom.uproject` 를 더블클릭합니다. 모듈을 빌드할지 물으면 *예* 를 누릅니다.
   (또는 우클릭 → *Generate Visual Studio project files* 후 `RRRandom.sln` 에서 `Development Editor` 로 빌드)
4. 처음에는 엔진 기본 레벨(`Template_Default`)이 열립니다. 바로 플레이(Alt+P) 해볼 수 있습니다.

## 우리 맵 만들기

1. *File → New Level → Basic* 으로 새 레벨을 만들고 `Content/Maps/Main` 으로 저장합니다.
2. *Edit → Project Settings → Maps & Modes* 에서 *Editor Startup Map* 과 *Game Default Map* 을 `Main` 으로 바꿉니다.
   (`Config/DefaultEngine.ini` 가 바뀝니다. 같이 커밋해 주세요.)

## 코드 구조

- `ARRRandomGameMode` — 기본 폰과 컨트롤러 지정
- `ARRRandomCharacter` — 탑뷰 카메라와 캐릭터. 엔진 기본 도형으로 그려서 별도 에셋이 필요 없음
- `ARRRandomPlayerController` — 클릭 이동, WASD, Space 입력. 입력 에셋이 지정되지 않으면 코드에서 만들어 씀
- `URRRandomEventComponent` — Space를 누를 때마다 무해한 랜덤 효과를 하나 골라 적용. `OnRandomEvent` 로 UI 연결 가능
