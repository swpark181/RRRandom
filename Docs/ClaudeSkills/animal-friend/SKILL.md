---
name: animal-friend
description: Makes a new RRRandom animal friend (동물친구) character, or a new look for one, from concept to a clean textured Tripo 3D model ready for rigging — Tripo Studio concept images in the house style, picking and mixing concepts, eight-direction turnaround (the user's rule for every character), multiview 3D, and texture cleanup in Blender. Use whenever the user asks for a new character, character design concepts (시안), a new outfit/skin for an animal friend, a turnaround (8방향/4방향 컨셉아트), a Tripo character model, or to fix a character model's texture, in any wording, e.g. "여우 여자 캐릭터 시안 5개", "캐릭터 디자인 뽑아줘", "4방향 컨셉아트", "트라이포 3D 모델링까지", "벨트 마감 조잡한 부분 개선", "텍스처 경계 지저분한 부분 고쳐줘". Not for props, maps or vehicles (Tripo is used there too, but the rules here are for characters).
---

# 동물친구 만들기: 시안 → 8방향 → Tripo 3D → 텍스처 정리

RRRandom 의 플레이 캐릭터는 **동물친구**(영문 UI ANIMAL FRIEND)라고 부른다. 브롤러/brawler 라고 하지 않는다.
지금 있는 동물친구: 여우, 토끼, 다람쥐, 호랑이, 용, 레드 폭스(= 여우 여자 "붉은 사막여우", 2026-10-21 게임에 RedFox 로, 여우 키트; 나중에 구미호로 변신 예정).
답은 한국어로.

작업 폴더: 그림·모델은 `C:\work\char-pipeline` (`input\<이름>\`, `output\<이름>\`), 스크립트는 `C:\work\char-pipeline\scripts`,
게임은 `C:\work\RRRandom` (진행 기록 `HANDOFF.md`, 설명 `README.md`). 첫 사례 전체 기록: `input\fox_girl\`, `output\fox_girl\`.

## 0. 원칙 (사용자가 직접 정한 것)

- **시안은 처음부터 Tripo 로.** 손으로 그린 SVG/HTML 시안은 "프로젝트 컨셉에 맞는 걸로", "트라이포 활용해서"라며 버려졌다.
  기존 동물친구 그림을 참고 이미지로 넣어 같은 스타일로 뽑는다.
- **공통 룩:** 3D 카툰·치비 비율(큰 머리, 짧은 몸·다리), 편한 A 자세, 반팔 티셔츠 + 반바지 + 갈색 주머니 벨트.
  종마다 털·티셔츠·바지 색을 달리해서 다른 동물친구와 안 헷갈리게. 물체(소품)는 통통한 풍선 같은 카툰 스타일.
- 탑뷰·쿼터뷰 게임이라 **위에서 봐도 읽히는 표식**(귀, 머리 위 장식, 꼬리)이 있으면 좋다.
- 나중 변신(궁극기, 구미호 등)이 예정돼 있으면 기본형에 **작은 암시**만(이마 문양, 아이라인 등), 꼬리 수 같은 큰 변화는 변신형에.
- 결과는 늘 **한눈에 비교하는 시트**로 보여준다(아래 7). 고르는 건 사용자.
- **캐릭터는 8방향으로 뽑고 그걸로 진행한다**(2026-10-21 사용자 지시, 이전의 4방향 대신). 아래 3절.
- 크레딧을 쓰는 단계(3D 30~45, 텍스처 30)와 다운로드는 사용자가 그 단계를 요청했을 때만. 무료 이미지 생성은 자유롭게(단 "Free n/1000" 한도 안에서, 2절).

## 1. Tripo Studio 열기 (Claude in Chrome)

- Tripo API 키는 쓸 수 없다(환경 변수는 가짜 값). 웹 `https://studio.tripo3d.ai` 를 Chrome 으로 조작한다.
- Chrome 도구가 없으면 대화 중 `/chrome` 으로는 안 생긴다 → 사용자에게 `claude --chrome --continue` 로 다시 시작해 달라고.
- 도구는 한 번에 로드: tabs_context_mcp, navigate, computer, find, javascript_tool, file_upload, form_input, tabs_close_mcp.
- 이미지: 왼쪽 **Image** 탭(`/workspace/generate-image`). 3D: **Model** 탭. 텍스처: **Texture** 탭. 끝나면 탭 닫기.

## 2. 시안 뽑기 (Image, 무료)

- 모델 **Nano Banana Pro**, 비율 **1:1**, 장수 2(기본). 아래 "Free n/1000" 이 무료 횟수, 비용 0 표시 확인.
- 참고 이미지: `find("file input for reference image upload")` → `file_upload` 로 `C:\work\char-pipeline\output\animals\fox_front.png`
  (또는 그 종의 기존 정면 그림). 업로드는 클릭하지 말 것(네이티브 창이 열림).
- **모델 목록(2026-10-22):** GPT Image 2 / 2.5, Nano Banana Pro / 2 / (무료), Midjourney v8.2(API by Youchuan). 모두 무료 횟수로
  0 크레딧. 기본은 Nano Banana Pro — **참고 이미지를 받는** 모델이라 "이 그림을 그대로 두고 ○○만 고쳐"(같은 캐릭터 유지, 8방향,
  옷만·눈만 수정)가 된다. **Midjourney 를 고르면 참고 이미지 버튼이 사라져 글만** 받고, 한 번에 4장(무료 횟수 4, 1024 또는
  2048²)을 준다 → 새 방향 탐색(분위기 시안)용이지, 기존 캐릭터를 그대로 이어 그리거나 8방향을 맞추는 데는 못 쓴다.
- **무료 한도(사용자 확인, 2026-10-22):** 이미지 생성은 Generate 버튼의 "Free n/1000" 이 1000 이 될 때까지 무료. 한 장에 1
  (2장 = 2, 4K 도 같음). 2026-10-22 끝에 480/1000. 넉넉하지만 무한은 아니니 큐에 넣기 전 대략 몇 장 쓸지 생각하고,
  답에 남은 무료 횟수를 적는다. 1000 을 넘기면 크레딧(정가 40, 4K 60)이 빠질 수 있으니 그때는 사용자에게 먼저 묻는다.
- **프롬프트 칸 포커스는 JS 로**(`ta=document.querySelector('textarea'); ta.focus(); ta.select()`): 참고 이미지를 올리면 칸이
  줄어 좌표 클릭이 빗나가고, 타이핑이 허공에 간다.
- **페이지를 연 뒤 오른쪽 기록 목록이 다 뜰 때까지 기다린다**: "Create your first image!" 가 보이는 동안(기록이 아직 안 뜸)엔
  Generate 를 몇 번 눌러도 무시된다(2026-10-22). `document.body.innerText` 에 예전 프롬프트가 보일 때부터 넣으면 한 번에 들어갔다.
  안 뜨면 페이지를 다시 연다.
- **페이지를 새로 연 뒤 Generate 가 조용히 무시되는 또 다른 이유:** 그 페이지에서 아직 **실제 키 입력**이 한 번도 없으면 앱이
  프롬프트를 못 받는다(JS 로 넣은 값만으로는 클릭을 세 번 해도 무시됨, 2026-10-22). 해결: 프롬프트는 JS 로 넣고
  (`setter=Object.getOwnPropertyDescriptor(HTMLTextAreaElement.prototype,'value').set; setter.call(ta,txt);
  ta.dispatchEvent(new Event('input',{bubbles:true}))`), 프롬프트 칸을 한 번 **좌표로 클릭**(JS focus 만으로는 가끔 키가 딴 데로 감) → ctrl+End → `computer type " "` 로 공백 한 칸을 실제로 친 뒤
  Generate → 매번 한 번에 들어갔다(긴 프롬프트도 타이핑 없이 빠름). 그래도 Generate 뒤 6초 기다려 `document.body.innerText` 에
  그 프롬프트가 생겼는지, Free 숫자가 2 늘었는지 확인하고 다음 것을 넣는다(확인 없이 큐에 넣다 5개를 잃은 적 있음).
- 참고 이미지 지우기: 프롬프트 칸 조상에서 `.i-tripo\:close` 를 찾아 click 반복. `file_upload` 는 여러 장 한 번에(최대 3장 써 봄).
- **4K 토글**(Nano Banana Pro): 2026-10-22 기준 비용 0(무료 횟수 차감, 정가 60). 결과는 4096² jpeg(`upscaled_*.jpeg`,
  img 이름이 `image_N.png` 가 아님). 3D 입력에는 1K 로 충분하고, **UI 초상화용 정면만 4K** 로(얼굴이 확실히 선명).
- **기존 캐릭터 머리·표정만 바꾸는 재디자인**은 8방향을 새로 뽑지 말고 옛 8방향 그림을 하나씩 "편집할 그림 + 새 정면" 두 장
  참고로 넣고 "Change ONLY the head and the tail" 로 고친다 → 몸·옷(이미 마감 정리된 것)이 그대로 남는다(레드 폭스 v4).
  3/4 뷰 꼬리는 그래도 반대로 휠 수 있으니 확인.
- **3/4 뷰 꼬리가 반대로 휘었을 때:** 그 그림 + 꼬리가 맞게 나온 이웃 두 방향(앞-오른쪽이면 정면+오른쪽 옆, 뒤-왼쪽이면 뒤+왼쪽
  옆)을 참고 세 장으로 넣고 "Edit the <방향> image ... The other two reference images only show where her tail really is ...
  Change ONLY the TAIL" + 그 뷰에서 꼬리가 보일 자리(먼 쪽이면 "mostly hidden behind her body, only the white tip peeks out")
  + "Remove the tail that now sweeps out to the image LEFT" 처럼 틀린 쪽을 콕 집어 지우게 하면 한 번에 맞았다(레드 폭스 v4).
  꼬리가 그녀의 왼쪽으로 휘는 캐릭터라면: 앞-오른쪽 3/4 = 먼 쪽이라 거의 숨음, 뒤-왼쪽 3/4 = 카메라 쪽 가운데로 늘어짐.
- 프롬프트 첫머리(그대로 재사용):
  `Character design concept for a cute 3D game. Use the reference image as the style guide: exactly the same 3D cartoon
  render style, materials, soft studio lighting, chibi proportions (big head, short body, short legs) and relaxed A-pose.
  Full body, front view, centered, plain light grey background. No text, no logo, no weapon. Character: a NEW ...`
  뒤에 털 색·눈·머리 장식·꼬리, 그리고 `Outfit in the same game style: <색> short-sleeve T-shirt, <색> shorts, brown utility belt with small pouches.`
- "no clothes / naked" 같은 말은 쓰지 않는다 → "image does not meet guidelines" 오류(참고 이미지 탓이 아님).
- 여러 안은 **연달아 큐에 넣어도 된다**: 프롬프트 칸 클릭 → ctrl+a → 새 프롬프트 type → Generate. 참고 이미지는 남아 있다.
- 완료 확인은 JS 로: 프롬프트 텍스트가 든 요소의 조상에서 `naturalWidth >= 1000` 인 img 수.
- 섞기("3번하고 5번 섞어서"), 다듬기("조금 더 예쁘게")는 고른 그림을 **참고 이미지로 다시 넣고** 바꿀 점만 적는다:
  `Refine this exact character into a prettier final design. Keep her identity, colors, outfit, ...`
- **생성된 그림을 참고 이미지로 넣는 법** (다운로드 없이): 페이지 JS 에서 그 img 의 src 를 fetch → createImageBitmap →
  canvas → PNG Blob → `new File` → `DataTransfer` 로 `input[type=file]` 에 넣고 change 이벤트. 행 머리의 작은 썸네일
  (`input.png`, 참고 이미지)과 결과 그림(`image_0/1.png`)을 헷갈리지 말 것. 넣은 뒤 프롬프트 칸을 zoom 으로 확인하고,
  잘못 들어간 썸네일은 `.i-tripo\:close` 아이콘이 든 button 을 JS 로 click 해서 지운다.

## 3. 8방향 컨셉아트 (Image, 무료) — 사용자 규칙(2026-10-21): 캐릭터는 늘 8방향으로 뽑고 그걸로 진행

- 고른 정면 그림 = FRONT. 그 그림을 참고 이미지로 넣고 같은 캐릭터를 나머지 7방향으로(45°씩, 캐릭터가 시계 반대로 도는 순서):
  FRONT · FRONT-LEFT 3/4 · LEFT SIDE · BACK-LEFT 3/4 · BACK · BACK-RIGHT 3/4 · RIGHT SIDE · FRONT-RIGHT 3/4.
  (왼쪽/오른쪽은 **그림 기준**: LEFT SIDE = 얼굴이 그림 왼쪽 끝을 봄.) 프롬프트 틀:
  `Character turnaround reference, <방향 설명>: the exact same character as the reference image, same 3D cartoon render
  style, materials, colors, chibi proportions and relaxed A-pose ...`
  방향 설명: 뒤 `BACK VIEW (seen straight from behind, her face is NOT visible)`,
  옆 `LEFT SIDE VIEW (exact side profile, she faces the LEFT edge of the image, only one eye visible)` / RIGHT 도 같은 식,
  3/4 `FRONT-LEFT THREE-QUARTER VIEW (body turned 45 degrees, she faces toward the LEFT edge of the image, both eyes visible,
  the far eye partly hidden)`, 뒤 3/4 `BACK-LEFT THREE-QUARTER VIEW (turned 135 degrees away, we see her back and the left
  side of her face only as a cheek outline, eyes NOT visible)`. 장식·꼬리·귀 위치를 뷰마다 다시 적어 준다.
- 비대칭 디테일(한쪽 리본·방울, 이마 문양 방향, 주머니 위치)은 **좌우 뷰가 서로 거울이 되지 않게** 매번 "on her LEFT ear
  (image right side in this view)" 처럼 캐릭터 기준과 그림 기준을 함께 적는다. 옆·뒤 3/4 를 FRONT 하나만 참고로 뽑으면
  돌아가며 어긋나기 쉬우니, 막히면 이미 고른 이웃 뷰(예: LEFT SIDE)를 참고 이미지로 함께 넣는다.
- **참고 이미지는 여러 장 넣을 수 있다**(file input 이 multiple). 정면·옆·뒤를 한 번에 `file_upload` 하고 프롬프트에
  "the reference images show her from the front, from her left side and from the back" 라고 적으면 3/4 뷰가 잘 맞았다
  (여우 여자 8방향, 2026-10-21: 5방향 × 2장 모두 방향·방울 위치가 맞음). 이미 정면/옆/뒤가 있으면 나머지 5방향만 뽑는다.
  각 프롬프트에 "<방향 설명>: the exact same character ..." + 디테일 목록 전체를 반복(1,700자 안팎, 한도 10,000).
- 연달아 Generate 할 때 가끔 클릭이 안 먹어 한 방향이 빠진다(뒤-오른쪽이 빠졌었음) → 다 넣은 뒤 `document.body.innerText`
  에 각 방향 이름이 있는지 확인.
- **그림체 참고(다른 캐릭터)를 넣으면 꼬리 방향이 참고 그림을 따라간다**(2026-10-22: C-3 레드 폭스를 그림체 참고로 넣은 여우·호랑이
  시안 6장 모두 "HIS LEFT (image right)" 라고 적었는데 그림 왼쪽으로 휨). 고른 정면을 8방향 전에 꼬리만 고치거나, 그 방향을 받아들여
  8방향 프롬프트를 그 쪽으로 맞춘다.
- **꼬리 자세는 처음(정면)부터 하나로 정해서 모든 뷰 프롬프트에 같은 문장으로**: 예 "it grows from the center of his lower back
  just under the belt, sweeps down behind him and curves out toward HIS LEFT side, the white tip resting on the floor beside and
  slightly behind his LEFT foot" + 뷰마다 그림 기준 위치(뒤 = 그림 왼쪽, 왼쪽 옆 = 몸 뒤 그림 오른쪽, 오른쪽 옆 = 몸 뒤 그림 왼쪽에 일부).
  여우 v3(2026-10-21)는 이렇게 해서 3D 두 개 모두 꼬리 하나·컨셉과 같은 자세로 나왔다(레드 폭스는 뷰마다 달라 V 자로 갈라졌었음).
- **뒤-왼쪽 3/4 는 자꾸 뒤-오른쪽(얼굴이 오른쪽 위)으로 나온다**: 정면·옆·뒤 세 장을 참고로 두 번 실패 → 참고를 **왼쪽 옆 + 뒤 두 장만**
  넣고 "the camera angle exactly HALFWAY between the two reference images ... turn him 45 degrees further away ... his snout points toward
  the upper LEFT" 로 하니 맞았다.
- 탑뷰 게임이라 **TOP VIEW**(`camera looking straight down from directly above her head`)도 덤으로 한 장 뽑는다(8방향과 별도).
- 뒤에서 꼬리가 **엉덩이 가운데**에서 나오게 적는다(옆으로 치우치면 리깅 때 귀찮다).
- 각 2장 중 고른다. 원본(1024²) 받기: JS 로 결과 img 를 PNG 로 다시 싸서 `<a download>` 클릭 → `C:\Users\user\Downloads` →
  (결과 img 의 "행"은 `image_0/1.png` img 가 2개 이상 든 **가장 가까운 조상**으로 찾을 것. 프롬프트 글자가 든 조상까지 올라가면
  목록 전체를 잡아서 옛 행 그림까지 수십 장 받아진다. 한 번에 다 받는 JS 는 45초 제한에 걸리니 방향 1~2개씩.)
  `input\<이름>\concept\<이름>_front|front_left|left|back_left|back|back_right|right|front_right|top.png` 로 옮김.
- 보여줄 때는 8장을 한 줄(또는 2×4)로 돌아가는 순서대로 이어 붙인 턴어라운드 시트 + 탑뷰.
- 그 뒤 단계(옷 마감 정리, 3D, 텍스처 확인)도 8방향 기준: 3D 확인 캡처·렌더도 8방향으로 돌려 본다.

## 4. 3D 전에 옷 마감 정리 (Image, 무료) — 꼭 할 것

Tripo 3D 는 그림에 그려진 모양을 그대로 따른다. 울퉁불퉁하게 뭉친 벨트·애매한 티셔츠 밑단은 모델에서 "조잡한 마감"이 된다
(첫 모델에서 사용자가 지적). 3D 전에 8방향 그림을 **각각 자기 자신을 참고 이미지로** 넣고 옷 구조만 다듬는다
(적어도 3D 에 넣을 FRONT/LEFT/RIGHT/BACK; 3/4 뷰도 같은 문구로 맞춰 두면 확인용 기준이 된다):
`Edit this exact image. Keep EVERYTHING identical (...), and only clean up the outfit construction so it is easy to turn
into a clean 3D model: one simple, smooth, flat brown leather belt band of even width ..., a small rectangular gold buckle
..., just two neat, simple rounded-box pouches attached flat on the left and right hips ...; the T-shirt hem ends in a
clean, straight, even edge ...; neat even rolled cuffs ... crisp borders between shirt, belt and shorts.`
결과는 `concept\<이름>_v2_<방향>.png`(방향 이름은 3절과 같음) 로 받아 둔다.

## 5. 멀티뷰 3D (Model, 크레딧)

- Model 탭 → 입력 모드 두 번째 아이콘(멀티뷰: Front / Left / Right / Back 칸). `find("file inputs for Front, Left, Right, Back")`
  → Front=정면, **Left 칸=LEFT SIDE 그림**(그림 왼쪽을 보는 옆모습), Back=뒷모습, Right=RIGHT SIDE 그림.
  (여우 여자 때는 옆모습이 하나뿐이라 Right 를 비웠다. 8방향이면 넣되, 두 옆모습의 비대칭 디테일이 서로 맞는지 먼저 확인 —
  어긋나면 Right 는 비운다.) 3/4 뷰 4장은 Tripo 칸이 없으니 모델·텍스처를 같은 각도로 돌려 보는 확인 기준으로 쓴다.
- Geometry & Texture: **Ultra Mesh Quality 켜기**(45 크레딧, 끄면 30 — 켰을 때 벨트·밑단이 훨씬 깔끔했다), Texture 켬,
  2K, Remove Lighting 켬, PBR 켬, Topology **Triangle**, Polycount **20000**(다른 동물친구와 같음). H3.1.
- 약 2분. 크레딧 숫자 변화(예: 22,530 → 22,485)를 기록해 둔다. 결과는 Assets 맨 위.
- 확인은 뷰어를 드래그해 45°씩 돌려 보고 `zoom`(save_to_disk)으로 8방향 캡처 → 컨셉 8방향과 위아래로 붙인 비교 시트.
  드래그 각도는 창 크기에 따라 달라진다(1459px 폭에서 약 131px = 45°, 오른쪽으로 끌면 앞-오른쪽 방향으로 돎). 매 캡처를 보고
  각도를 맞출 것. `zoom` 의 `scale` 0.55 이상으로 저장해야 시트용 해상도(500px 안팎)가 나온다(0.3 이면 270px).
  2026-10-22(레드 폭스 v4, 1459×812 창, 왼쪽 패널·오른쪽 Assets 열림): **왼쪽으로 끌면** 정면 → 앞-왼쪽 → 왼쪽 → 뒤 순서로 돎.
  45° 에 140~180px 로 매번 달랐다(같은 거리도 20°~70°) → 한 번 끌고 zoom 으로 보고, 지나치면 반대로 30~90px 되돌린다.
  먼저 휠 아래로 2칸 줄여야 발끝까지 들어온다. 캡처 영역 (510,180)-(1000,670).
- 여러 종을 한꺼번에: 탭마다 하나씩 Generate — **동시 생성은 2개까지**(세 번째는 크레딧도 안 빠지고 조용히 무시됨, 하나 끝나면 다시).
  비교는 뷰어 드래그 대신 1차·재시도를 둘 다 FBX 로 받아(무료) `blender_texture_compare.py --views turn8` 로 정확한 8방향 렌더
  (`scripts\unpack_tripo.sh <zip> <dir> <이름>` 이 tripo_convert_<id> 이름을 정리). 2026-10-21 토끼·다람쥐·호랑이는 둘이 거의 같아 재시도로.
- 결과 아래 **Free Retry**(무료, 툴팁 "Retry generation for free ... current version can be found in model history")로 같은
  입력을 한 번 더 뽑아 비교할 수 있다. **3D 를 만들면 Free Retry 까지 돌려 두 개를 8방향으로 나란히 보여주고 사용자가 고르게 한다.**
  여우 여자 v3(2026-10-21): Claude 는 재시도 손의 작은 금색 조각 때문에 1차를 추천했지만 사용자는 **재시도가 제일 좋다**고 골랐다
  (전체 인상 > 작은 흠; 작은 흠은 나중 정리 단계에서 고치면 된다). 고른 쪽이 Tripo 화면의 현재 버전인지 확인
  (Retry 하면 이전 것은 그 작업의 model history 로 감).
- 꼬리 끝이 뒤에서 두 갈래 흰 뭉치로 갈라져 보이는 건 v2(오른쪽 칸 비움)에도 있던 것 — 정면(꼬리가 옆으로 삐져나옴)·옆(뒤로 뻗음)·
  뒤(아래로 늘어짐) 그림의 꼬리 모양이 서로 달라서로 보인다. 다음 캐릭터는 컨셉 단계에서 꼬리 자세를 8방향 모두 같게 적을 것.

## 6. 텍스처 (Texture 탭 + Blender)

### Tripo 에서 다시 칠하기
- Texture 탭 → 두 번째 아이콘(멀티뷰, 앞의 v2 그림이 그대로 들어 있음) → Remove Lighting 켬 → 해상도(8K 는 무료 체험 1회였고
  이미 썼다; 보통 30 크레딧) → Generate Texture. 뒤통수 얼룩 같은 건 이것만으로도 사라졌다.
- 내려받기: 아래 Export → 파일 이름은 **form_input** 으로(`find("File Name text input")`; 좌표 클릭 뒤 타이핑하면 뷰어 단축키로
  먹힘) → FBX / **Blender** / 텍스처 해상도 → Export → Downloads 의 zip → `input\<이름>\tripo\` 에 풀기
  (`<이름>.fbx`, `<이름>.fbm\<이름>_basecolor.jpg`, `_normal.png`).

### Blender 정리 (Blender 5.2: `C:\Program Files\Blender Foundation\Blender 5.2\blender.exe`, `-b --factory-startup --python <스크립트> -- <인자>`)
시스템에 python 이 없다 — numpy 는 Blender 안에서만. 이미지 합치기(시트)는 PowerShell `System.Drawing`.

1. `blender_texture_clean.py <basecolor in> <png out> 4096` — 팔레트(털·흰털·검정·분홍·셔츠·바지·벨트·금색)를 k-means 로 맞추고,
   옷 경계의 흐린 중간색을 가까운 쪽으로 붙이고(옷 경계만, 털 안의 음영은 그대로), 털·옷 얼룩을 고르게, 마지막에 살짝 샤픈.
   **종마다 `PALETTE` 첫 추정값을 그 캐릭터 색으로 바꿀 것**(실행하면 맞춘 색을 출력).
2. `blender_texture_regions.py <fbx> <clean png> <out png>` — 메시로 "어느 부위인지"를 정해서 다시 칠함:
   - 텍셀 색 → 부위(털/셔츠/벨트/바지), 삼각형별 비율 → 정점에 평균 → 메시 위에서 부드럽게 → 텍셀마다 보간해서 부위를 정하고,
     다른 부위 색이 섞인 텍셀은 그 부위의 주변 평균색으로. 분홍(주사위 프린트·볼터치·귀 안쪽) 둘레는 보호.
   - **아랫면:** 아래를 향한 면(주머니 바닥, 바지단 테두리)은 어떤 입력 그림에도 안 보여서 Tripo 가 아래 색(바지)으로 칠한다
     → 아랫면 덩어리는 테두리가 맞닿은 부위를 따른다(옷↔옷만, 몸 높이 15~50% 구간만, 테두리 55% 이상이 한 부위일 때).
   - **UV 섬 사이 빈칸**을 16텍셀 채움(안 하면 이음매에 옛 색이 비쳐 밝은 줄이 보임).
   - 옷 위의 가는 "금색" 선은 벨트↔바지 섞인 색 → 옷 색으로(버클은 꽉 찬 금색이라 유지).
   - 다시 칠하기는 **목 아래만**(`BODY_TOP`): 초록 눈동자가 민트 셔츠로 읽혀 빨갛게 칠해진 적 있다.
   - 꼬리: 꼬리 영역(`TAIL_BEHIND`, `TAIL_BELOW`)에서만 빨강↔흰 끝을 같은 방식으로(끝의 흰색이 분홍으로 분류되니 분홍=흰색 취급).
   - 털 위의 엉뚱한 어두운 낙서(오른팔 뒤 "97" 같은 글자)는 "주변이 거의 다 빨강이고 어두운 게 거의 없는" 어두운 텍셀로 찾아 지움.
     장갑선 자체는 Tripo 그대로 둔다(메시로 다시 그리면 계단 모양이 됨).
   - 상수는 Tripo FBX 기준(+Z 위, 앞이 -Y, 키 약 1m). 다른 체형이면 `TAIL_*`, `BODY_TOP`, `UNDER_BAND` 를 확인.
   - 최종본 이름: `input\<이름>\tripo\<이름>_basecolor_final.png`.
3. 확인 렌더: `blender_texture_compare.py <out png> <fbx> <텍스처1> [<텍스처2> ...] --views a,b,c --px 600`
   (텍스처마다 한 줄; 원본 vs 결과를 위아래로). 뷰: front, back, left, face, chest, sleeve, waist, pocket, hem, hem_back,
   belt_low, low, under, under_l, under_r, arm_r_close, arm_l_close, head_back. 오른팔 = 캐릭터 기준(-X).
   8방향: `--views turn8` (= front, front_left, left, back_left, back, back_right, right, front_right; 좌우는 컨셉아트처럼
   **그림 기준** 얼굴 방향). 컨셉 8방향 시트와 같은 순서라 위아래로 붙여 비교하기 좋다. 경로는 **절대 경로**로(Blender 는
   상대 경로 텍스처를 못 읽는다).
   고친 뒤엔 **얼굴(face)과 전신(front/back/left)을 꼭 다시 본다** — 한 곳을 고치다 다른 곳이 망가진 적이 여러 번.

### 여우 여자 v3 (2026-10-21) 에서 추가된 옵션 — 모델마다 결과를 보고 고를 것
v3 명령(그대로 재사용 가능):
`blender_texture_clean.py <8K basecolor.jpg> <clean.png> 4096 --fixed belt` →
`blender_texture_regions.py <fbx> <clean.png> <final.png> --no-tail --repaint-top 0.44 --tail-raw <8K basecolor.jpg>`.
- **Texture 탭 비용:** 2K/4K 20 크레딧, 8K 30 크레딧(무료 체험 끝난 뒤). 멀티뷰 칸이 비어 있으면 앞·왼·오·뒤를 직접 올린다
  (Right 칸 업로드가 가끔 안 먹음 → zoom 으로 4칸 다 찼는지 확인 후 다시 올리기). 내보내기는 무료.
- `--fixed belt`: 털이 진한 빨강이면 k-means 가 벨트 색을 털 쪽으로 끌어(186,80,46) 벨트가 빨개진다 → 벨트를 첫 추정값에 고정.
  실행 출력의 `CLEAN belt` 가 갈색(≈158,97,46)인지 꼭 확인.
- `--no-tail` + `--tail-raw <Tripo 원본>`: Tripo 꼬리가 깔끔하면 꼬리 패스를 끄고 꼬리 털은 원본 그대로(정리 단계가 분홍빛 흰 끝을
  바지 크림색으로 끌고, 그 뒤 다시 칠하기가 얼룩을 만듦). 엉덩이 뒤 `TAIL_FAR`(0.07 m)보다 뒤에 있는 삼각형은 부위와 상관없이 원본.
- `--repaint-top 0.44`: 목 둘레 삼각형이 크면 부위 필드가 계단처럼 칼라를 찢는다 → 칼라 위로는 다시 칠하지 않음.
- 새로 늘 도는 패스: 발·손의 얼룩(검정 속 주황·금색·빨강 점, 손끝의 작은 가시 모양 UV 섬까지 메시 이웃으로 찾음) → 검정;
  머리의 크림색(턱 밑·귀 안 흰 털이 바지 색으로 끌린 것) → 흰 털; 흰 털 많은 곳의 크림색은 빨강 아닌 흰색으로.
- 확인 뷰 추가: `hand_r`, `hand_l`, `hand_r_under`, `hand_l_under`(손 위치는 메시에서 잰 ±0.215H, 앞 0.1H, 높이 0.31H),
  `tail_back`, `tail_side`, `collar`, `chin_under`. 손 금색 조각은 아래에서 봐야 보인다.

### 리깅이 끝난 모델의 텍스처만 바꿀 때 (74차, 레드 폭스 눈)
Tripo Texture 탭 재생성은 메시 정점 위치는 그대로 두지만 **UV 를 새로 편다** → 베이스컬러 파일만 갈아 끼우면 안 됨.
새 FBX → `blender_tripo_to_obj.py` → `blender_merge_parts.py <기존 Mixamo SK> <기존 몸 OBJ> <새 OBJ> …`(위치로 맞춤, "N on a body
vertex" 가 몸 정점 수와 같아야 함) → 같은 꼬리·목 명령 → 언리얼 다시. Mixamo 업로드는 다시 안 해도 된다. 비교는 두 FBX 를
`blender_texture_compare.py` 로 같은 뷰에서.

### 3D 가 "컨셉보다 뭉개짐" / 모양이 이상함 (77차)
- **뭉개짐(털결이 흐림)** = 텍스처 문제 → 같은 메시에 Texture 탭 멀티뷰(컨셉 4장, 조명 제거, **4K 20 크레딧**)로 다시 칠하면 털결과 색이
  살아난다(다람쥐). 내보낼 땐 4K. 받은 베이스컬러 확장자가 `.jpg`/`.JPEG` 로 섞이니 대소문자 무시하고 찾을 것.
- **모양(목 털이 길게 늘어진 조각 등)** = 지오메트리 → 텍스처만 다시 칠하면 색만 바뀌고 조각 모양이 남고, Blender 이완
  (`blender_smooth_region.py`)도 울퉁불퉁 → **컨셉 그 부분만 Image 편집("Edit this exact image … change only one thing")해서 3D 를 다시**
  (Ultra, 4K 텍스처면 55 크레딧). 여우 목 털을 이렇게 고쳤다.

### 3D 텍스처의 작은 흠(반점·무늬) 고치기 — Tripo 에서 (사용자 지시, 72차)
Blender 로 텍스처를 직접 칠하면 털 결이 없는 평평한 색이 되어 "이상하다"고 했다. 대신: 그 흠이 보이는 멀티뷰 입력 그림만
Tripo Image 로 고침("Edit this exact image ... change only one thing: ...", 무료) → Texture 탭 멀티뷰에서 그 칸만 휴지통으로 비우고
새 그림 업로드 → Remove Lighting 켬, 2K → Generate Texture(10 크레딧) → Export. Texture → Edit → Magic Brush 는 지금 무늬를
따라 다시 그려서 흰 반점이 그대로 나왔다. 초상화(`blender_portrait.py`)는 Mixamo FBX 재질의 광택을 끄도록 고쳐 둠.

### 시행착오 (반복하지 말 것)
- 삼각형 단위로 부위를 정하면 소매 경계가 지그재그, 주사위 프린트가 깨짐 → 정점 필드 + 보간으로.
- 색만 보고 고치면 아랫면·가려진 면을 못 고침 → 메시(법선, 이웃)를 써야 함.
- 사용자 스크린샷의 이상한 곳은 같은 각도로 렌더해서 먼저 확인하고(`under_*` 등 뷰 추가), 고친 뒤 같은 각도로 전후 비교.

## 7. 보여주기

- 캡처/렌더를 PowerShell `System.Drawing` 으로 라벨 붙인 시트로 합쳐 `SendUserFile`(display render).
  한글 라벨은 `Malgun Gothic`. 화면 캡처 시안은 해상도가 낮다고 알려 주고 원본은 Tripo 기록에 있다고.
- **비교 페이지도 늘 만든다**(사용자 지시 2026-10-22 "앞으로 비교 페이지 까지 만들어서 보여줘"): 3D 1차/재시도, 시안, 텍스처 전후를
  보여줄 때 PNG 시트에 더해 Artifact 로 HTML 비교 페이지를 게시하고 링크를 준다. 틀: `C:\work\char-pipeline\output\rabbit_mjc\page\index.html`
  (방향 버튼 + 컨셉/A/B 세 장, A↔B 겹쳐 밀기, 8방향 표, 가까이 보기 4칸, 추천·파일 경로). 그림은 `img/` 에 JPG 로 잘라
  (`concept_<dir>.jpg`, `a_<dir>.jpg`, `b_<dir>.jpg`, `a_d_<부위>.jpg`) `root` + `files` 로 함께 게시. 3D 렌더는
  `blender_texture_compare.py --views turn8` 가로 한 줄을 정사각형 8칸으로 자르면 된다.
- **3D 비교 때는 `under`(아래에서 올려다봄)와 `tail_back` 도 렌더해 볼 것**: 토끼 A-3 재시도는 8방향 렌더로는 안 보이던 두 번째 꼬리
  솜뭉치가 아래에서만 보였다(컨셉 정면에서 꼬리가 옆으로 삐져나와 Tripo 가 따로 하나 더 만듦). 뷰 이름 `waist` 는 키 작은 치비에선 입 높이가 찍힌다.
- **한쪽에만 생긴 여분 덩어리(두 번째 꼬리 등) — 먼저 이것**: `blender_mirror_lower.py <tripo fbx> <out fbx> <자를 높이(OBJ 단위)> [+X|-X]`
  — 셔츠 높이(토끼 31)에서 몸을 정확히 자르고, 그 아래 망가진 쪽을 지운 뒤 깨끗한 쪽 아래를 거울 복사(UV 그대로, 텍스처 안 바꿈)하고
  자른 선을 서로의 점을 끼워 넣어 용접한다. 토끼 B 의 여분 꼬리를 이걸로 고쳤다(`input\rabbit_mjc\tailfix\rabbit_b_mir.fbx`).
  머리·귀·한쪽 장식(달 클립)은 자른 선 위라 그대로. `bmesh.ops.symmetrize` 는 자른 선 위 면까지 뜯어 놓아 못 쓴다.
  아랫몸이 완전 대칭이 되니 한쪽에만 있는 무늬·장식이 아래쪽에 있으면 쓰지 말 것.
- (그 전 시도, 사용자가 "없어진 부분이 이상하다"고 함) 덩어리만 떼기: `blender_remove_lump.py <tripo fbx> <basecolor> <out fbx> <out basecolor png>
  "<x0,x1,y0,y1,z0,z1>" [gap 1.2]` — 상자(OBJ 좌표, `blender_obj_grid.py` 로 읽음) 안에서 **반대쪽을 거울로 비춘 면과 1.2 이상 떨어진 정점**
  = 덩어리(좌우 대칭이라 주머니·바지·손은 짝이 있어 남음), 거기서 흰 털을 따라 두 겹 더 → 삭제 → 구멍을 촘촘히 메우고 이완 →
  메운 면은 아틀라스 빈칸에 UV 를 주고 반대쪽 표면에서 텍셀마다 칠함. 토끼 B 의 여분 꼬리(오른손 아래)를 이렇게 뗐다
  (`input\rabbit_mjc\tailfix\`). 색으로 고르기(어두운 쪽이 남아 날개처럼 튐), 평평한 뚜껑(각진 면), 반대쪽 면에 점 붙이기(가시),
  삼각형마다 한 색(얼룩)은 모두 안 됐다. 결과 확인은 `rab_sel` 식으로 그 엉덩이 둘레를 4각도 렌더.
- 답에는: 무엇을 했는지, 크레딧 변화, 남은 문제(정직하게), 파일 경로(절대 경로), 다음 단계 제안.

## 8. 다음 단계 (요청 시) — 게임에 넣기

`C:\work\RRRandom\HANDOFF.md` 23차(토끼·다람쥐·호랑이), 30·44차(드래곤), 51차(한복 호랑이) 그대로:
`blender_tripo_to_obj.py <fbx> <obj> <베이스컬러 파일명>` → Mixamo(사용자 계정, Chrome; **Mixamo 는 캐릭터를 하나만 보관** —
올리기 전에 지금 있는 것과 받아 둔 것 확인) → 2 Chain Fingers, 옆으로 큰 꼬리는 대칭 끄고 마커 → 17개 애니메이션
(SK, walk, fire, death, victory, aim_idle, run_f/b/l/r, dance1~4, defeat1~3) → `blender_animal_tail.py`(꼬리 뼈) →
`ue_import_animal.py` → **얼굴 = 컨셉 정면에서 잘라냄**(`concept_portraits.ps1` 에 한 줄 추가, 사용자 지시 73차; 3D 렌더 `blender_portrait.py` 는 안 씀) → `ue_import_ui_texture.py`(한 번에 한 장씩) → 코드 `RRSpecies`/키트/타이틀 →
탈것 탑승 확인(아래 "탈것 탑승 맞추기").
큰 포니테일·챙 같은 건 머리 뼈에 붙인다.
모든 종이 함께 쓰는 모션(뒤로 날아감 `A_<종>_FlyBack`, 달리기 `A_<종>_Run` = Mixamo "Rifle Run (Rifle Down)")은 새 종에도
`ue_import_shared_anim.py <fbx>|<이름>|<리그 엉덩이>|<새 종>=<엉덩이>` 로 넣어 줄 것(원본: `input\shared_anims\`, `input\tiger\mixamo\tiger_flyback.fbx`). 모션 확인은 `-RRMap=Studio` + `RRStudioCam`.

### 탈것 탑승 맞추기 — 새 동물친구마다 꼭 (71차, 2026-10-21)
탑승은 코드가 알아서 맞추지만 **뼈 이름과 종 표 값**에 기대므로 새 종을 넣으면 반드시 확인한다.
- **필요한 뼈**(Mixamo 이름, `mixamorig:` 없이): `Hips`, `LeftUpLeg`/`LeftLeg`/`LeftFoot`(+Right), `Spine`, `LeftArm`/`LeftForeArm`
  (+Right), 꼬리는 `Tail1..4`. 앉는 탈것은 매 프레임 `Hips` 뼈를 좌석 지점 위, 쿠션 + 서 있는 엉덩이 높이의 22% 에 놓고
  (`ARRRandomCharacter::GetRideOffset`), 운전 자세는 다리·척추·팔을 코드로 굽힌다(`URRRandomBodyAnimInstance::UpdateRidePose`,
  킥보드는 서서, 오토바이는 걸터앉아, 차는 앉아서 핸들; 뒷좌석은 다리만). 탑승 중엔 꼬리를 60° 든다(`RideTailLift`).
  `Hips` 가 없으면 발이 쿠션 높이에 놓여 붕 뜬다.
- **종 표**(`RRRandomSpecies.cpp` `Tunings`): `{ 이름, MeshScale, HipsHeight, RideFit }`. `HipsHeight` 는 리그 측정값
  (`blender_rig_measure.py`) 그대로 — 앉는 높이에도 쓰인다. `RideFit`(앞, 오른쪽, 위; cm)은 그 종만의 미세 보정으로 처음엔 0,
  캡처를 보고 필요할 때만(토끼: 긴 몸통이라 위 −6). **탈것 표(`RRRandomVehicle.cpp` `Handlings`)는 메시 실측값이니 종 때문에
  건드리지 말 것** — 종 보정은 `RideFit` 으로.
- **확인 캡처**(`C:\work\char-pipeline\scripts\`): `capture_game_seq.ps1`(게임 창 + `ue_game_seq.py` 순서 실행, 샷은
  `output\screens\ride\`) → `shot_sheet.ps1`(종×탈것 시트). 일반 탈것은 기본 맵 `-Extra "-RRIdleEnemies"`(적 봇이 궁극기로 화면을
  덮지 않게), 탈 때마다 `goto=-1530,0,0` 빈터로 옮겨 찍고 **내리기 전에 다른 곳으로 `goto`**(세워 둔 차가 다음 장면을 가림).
  같은 팀 봇이 먼저 타 버리면 탑승이 안 되니 `nearmesh=SM_Motorbike#1` 처럼 다른 대를 고른다. 스타카 3대는 `-Extra "-RRMap=Studio"`.
  카메라는 `RRStudioCam -90 380 20`(왼쪽 옆, 화면 왼쪽이 앞). 예시 순서·결과: `output\screens\ride\round71_*.png`.
- **볼 것**: 엉덩이가 쿠션에 앉았는지(차·스타카는 가슴부터 위가 보이는 정도), 손이 핸들·핸들바에 닿는지, 킥보드는 발이 발판 위인지,
  꼬리·날개·큰 귀가 차체를 뚫는지. 여우·드래곤·레드 폭스·토끼·다람쥐·호랑이 시트와 나란히 놓고 비교.
- **새 탈것**이면: `blender_vehicles.py` 로 만든 FBX 를 `blender_seat_profile.py <이름>=<fbx>=<좌석 y들>` 로 재서(등받이 앞의 낮고
  평평한 구간 = 쿠션) `Handlings` 에 `SeatHeight`/`RearSeatHeight`(쿠션 윗면), `DriverSeatX`/`RearSeatX`(등받이 바로 앞),
  `DriverSeatY`/`RearSeatY` 를 넣고, 서서 타는 것이면 `ARRRandomVehicle::IsStoodOn`, 자세는 `UpdateRidePose` 의 `RidePoses` 에 추가.

### Mixamo "Unknown error while generating motion" — 꼬리가 발 옆 바닥에 닿아 있으면 (2026-10-21 확인)
고장이 아니다: 꼬리가 바닥·발 옆에 놓인 메시만 실패(레드 폭스, 여우·호랑이 v3). **꼬리를 떼고 몸만 리깅**: 임시 꼬리 뼈 리그(아래
fit_rig 또는 blender_animal_tail) → `blender_strip_parts.py <그 fbx> <obj> <body obj> Tail` → Mixamo(몸) → `mixamo_export.js` →
`blender_merge_parts.py <SK> <body obj> <whole obj> <out> Hips,0.4,Spine` → `blender_animal_tail.py` → `input\ue_import_mixamo_v3.sh`.
**꼬리 뼈 리그가 아직 없으면(새 모델) 72차 방식이 더 짧다**: `blender_obj_grid.py <obj> <prefix>`(위·옆·뒤 정점 격자 그림)로
꼬리 중심선을 OBJ 좌표(Y 위, +Z 앞, +X 캐릭터 왼쪽, 키 100)로 읽고 `blender_strip_polyline.py <obj> <body obj> "<x,y,z;…>" <반경> 0`
(마지막 0 = z<0, 몸 뒤쪽만 잘라 바지·다리는 남김) → zip(obj·mtl·basecolor) 업로드. 꼬리 뼈 좌표는 같은 점을 (x, −z, y)/100 으로
(`blender_merge_parts.py` 가 MERGE bounds 를 찍어 주니 대응 확인). 리그 뒤 `blender_pose_check.py <rig fbx> <out png> "Tail1:X:-50,…"`
로 꼬리 들기·다리·팔·고개를 굽혀 쉬는 자세와 나란히 보고 찢어짐을 확인. 복슬 호랑이(HANDOFF 72차)가 예.
레드 폭스 v5(74차, 굵은 여우 꼬리): 자르기 반경 14(12 면 바닥 털끝 몇 개가 몸에 남음, `body_*.png` 격자에서 떠 있는 점으로 보임),
`blender_animal_tail.py … 0.15 1.6`(0.13/1.2 면 꼬리를 들 때 발 옆 털끝이 실처럼 늘어남 — pose_check 에서 보임).
**꼬리를 잘랐는데도 "Unknown error while generating motion" 이면 떠 있는 작은 조각부터 의심**(75차 C-3 레드 폭스: 자르고 남은
정점 3개짜리 조각 하나 때문에 실패 → 가장 큰 연결 덩어리만 남기고 안 쓰는 정점을 지운 OBJ 로 바로 성공). 업로드 전에 덩어리 수를 센다.
Mixamo 창 크기가 바뀌면 화면 좌표 클릭이 빗나가 Next 가 안 눌린다 → 스크린샷 좌표를 다시 보거나 Skeleton LOD 는 드롭다운 클릭 후
키보드(ArrowDown ×2, Enter)로(`form_input` 은 보이기만 바뀌고 반영 안 됨). 실패해도 화면은 조용히 마커 단계로 돌아가니
`/api/v1/characters/<id>/monitor` 로 결과를 본다.
털이 복슬한 느낌(75차): 카툰 후처리가 법선으로 잉크를 그려 노멀맵은 안 됨 → `ue_fur_rim.py`(Emissive 프레넬 털빛 + Masked 가장자리
잔털 컷, 빨강·흰 털 마스크; 가장자리 컷은 1 − N·V 를 0.5~0.95 구간으로 넓게 잡아야 보임 — Fresnel⁴ 로는 1~2픽셀뿐이라 안 보였음;
값은 `ue_set_scalar_defaults.py` 로 바꿔 같은 각도로 비교; 노이즈 칸이 픽셀보다 작으면 자글거림 → 220칸 + 화면 미분으로 멀면 끄기, 몸 안쪽은 반드시 불투명 1),
`FURRIM=1 ue_import_mixamo_v3.sh …` 로 다시 넣을 때도 유지. 실루엣까지 복슬하게 하려면 컨셉을 굵은 털 뭉치 그림체로(Tripo 가 모양으로 만듦).
Mixamo 에 남은 캐릭터를 바꾸기 전에 그 캐릭터의 애니메이션이 `input\<종>\mixamo\` 에 다 있는지 확인하고, 업로드 뒤
"Proceed with this new character?" 에서 Next 를 눌러야 바뀐다(`/api/v1/characters/primary` 로 이름 확인).
Mixamo 가 로그아웃돼 있으면 Adobe 로그인은 사용자가 직접(비밀번호 입력 금지) — 로그인 뒤 탭을 mixamo.com 으로 다시 열면 된다.
토끼·다람쥐(꼬리가 바닥에 안 닿음)는 그대로 올리면 된다. 한 번 실패했다고 "Mixamo 고장"으로 단정하지 말고 예전에 된 zip 과 비교할 것.
**마커 위치도 "Unknown error while generating motion" 을 낸다**(76차 토끼, 네 번 실패): 팔꿈치 마커가 팔 바깥 빈 곳, 턱 마커가 입 위에
있었음 → 마커 단계 화면을 `zoom` 으로 확대해 **팔꿈치는 팔뚝 위**, **턱은 얼굴 아래 끝(목·티셔츠 깃 바로 위)**, 손목은 손과 팔이 만나는 곳,
사타구니는 반바지 가운데, 무릎은 반바지 밑단과 발 사이에 두고 다시 하니 바로 성공. 메시 검사(세 면 모서리·넓이 0 면·떠 있는 조각)와
마커 확인을 둘 다 먼저 하고, 그래도 안 되면 예전 zip 으로 Mixamo 쪽인지 가른다(성공해도 마지막 Next 를 안 누르고 닫으면 남은 캐릭터는 그대로).
아래 Blender 기증 리그는 정말 Mixamo 가 안 될 때만(정강이 가중치·발목 위치 문제로 사용자가 "리깅이 잘못됐다"고 함).

**Mixamo 리그 뒤 꼭: `blender_head_weights.py <rig fbx> <out fbx>`** — 큰 머리 치비는 Mixamo 가 얼굴(Head)과 목둘레(Spine2)를
면 한 줄로 갈라 고개를 움직이면 턱 아래가 길게 늘어난다(사용자 "얼굴이 늘어남", 2026-10-21). 꼬리 뼈를 붙인 뒤, 언리얼 전에.

### 77차 메모 (영화 그림체 네 종)
- 다람쥐처럼 몸만 한 꼬리는 통째로 올리면 실패 → 꼬리를 떼고 몸만. 날개 달린 용은 꼬리만 떼면 Mixamo 가 날개를 팔로 잡아 팔 뼈가
  접힌다 → 날개도 떼고 몸만 올린 뒤 merge + Wing 체인.
- 호랑이(film B)는 마커·LOD·FBX 를 바꿔도 6번 모두 실패 → `blender_fit_rig.py`(기증 = 체형 비슷한 종의 Mixamo SK, joints 는 Right* 를
  적어야 MIRROR 가 Left* 를 만듦) 후 `blender_unarm_region.py <fbx> <out> <cx> <half> <zmax>`(허리 아래 팔 무게 조각 중 팔 두 덩어리만
  남김 — 주머니가 손을 따라 늘어남 해결), merge·꼬리 뒤 `blender_head_only.py <fbx> <out> <z>`(수염·볼털 찢어짐 해결) → 목 가중치.
  애니메이션은 `ue_import_rig_anims.py` 로 기증 종의 것.
- 업로드 직후 실패 원인 확인: `performance.clearResourceTimings()` 후 Next → `/rig` 요청의 캐릭터 id 로 `/monitor`.

### Mixamo 자동 리거가 정말 안 될 때 (최후 수단)
레드 폭스(여우 여자, HANDOFF 68차)를 이렇게 넣었다. 먼저 예전에 성공한 zip(`input\tiger\tiger_mixamo.zip`)으로 Mixamo 쪽 고장인지
확인(실패한 업로드는 Mixamo 의 남은 캐릭터를 안 바꾼다) → 그렇다면:
1. 관절 읽기: 새 OBJ 를 정면·옆·확대(팔, 다리, 목) 격자 렌더로 보고 `joints.txt` 작성 — `<뼈 이름(mixamorig: 빼고)> x y z`, 기증 리그의
   블렌더 좌표(m, -Y 앞, +X 캐릭터 왼쪽), `MIRROR <가운데 x>` 로 Right* 를 Left* 로. 몸 가운데 x 는 0 이 아닐 수 있다
   (`blender_tripo_to_obj.py` 가 발 근처 꼬리 끝까지 쳐서 가운데를 잡음). 예: `input\red_fox\rig\joints.txt`.
2. `blender_fit_rig.py <기증 SK fbx> <기증 obj> <새 obj> <joints.txt> <out fbx> <미리보기>` — 기증은 체형이 비슷한 종의 Mixamo
   With Skin(`input\tiger\mixamo\tiger_SK.fbx` + `input\tiger\mixamo_obj\tiger.obj`). 미리보기 시트(뼈 색칠 + 시험 자세)로 확인.
   **발목은 발등(부츠 모양 발 꼭대기)보다 낮게, 발 높이의 1/2~2/3 쯤**, 무릎은 반바지 밑단 근처, 손목은 주먹이 시작하는 곳 —
   발목을 발 꼭대기에 찍었더니 걸을 때 정강이가 접혔다(사용자 지적 2026-10-21). 스크립트가 팔다리 가중치를 관절 선을 따라
   다시 나눈다(기증 Mixamo 가중치는 정강이를 발 뼈에 줌). 리그 뒤 `scratchpad` 의 굽힘 시험처럼 무릎·발목·팔꿈치를 꺾어 렌더해 볼 것.
   **뼈는 옮기기만, 방향은 기증 그대로**(스크립트가 그렇게 함): 새 관절을 잇게 뼈를 다시 향하게 했더니 애니메이션이 기증 뼈 방향을
   덮어써서 머리가 18° 숙여지고 달리기가 이상했다(사용자 지적, 2026-10-21).
   리그 전에 **꼬리가 하나인지** 위에서 본 렌더로 확인 — Tripo 가 컨셉마다 다른 꼬리 자세를 둘 다 만들어 V 자로 갈라질 수 있다
   ("여우 꼬리는 하나여야"). 갈라졌으면 `blender_fold_lobe.py <obj> <out obj> <갈라지는 점 x,y> <가운데 각도> <옆 각도> <줄임>`.
3. 꼬리 `blender_animal_tail.py` — 털이 넓게 퍼진 꼬리는 반경·몸 비율을 넉넉히(레드 폭스 0.2 / 1.3; 작으면 털 끝이 다리에 남아 늘어남).
   기증 리그의 꼬리 가중치(호랑이는 왼허벅지)는 꼬리 뼈로 다시 칠해져야 함 — 꼬리를 들어 본 렌더로 확인.
4. 언리얼: 리그 FBX 를 `<dir>\<name>_SK.fbx` 로 두고 `ue_import_animal.py`(메시·머티리얼만, 애니메이션 없음 로그는 정상) →
   `ue_import_rig_anims.py <Name>|<엉덩이>|<기증 mixamo 폴더>|<기증 이름>|<기증 엉덩이>`(17개) → `ue_import_shared_anim.py` 로 Run·FlyBack.
5. 얼굴: 컨셉 정면을 `concept_portraits.ps1` 로 잘라서(73차부터 UI 얼굴은 컨셉아트; 3D 렌더 초상화는 쓰지 않음).
6. 코드: 스킬이 기존 종과 같으면 `RRSpecies::GetKitSpecies` 에 매핑 하나 추가(키트 표·전용 주사위·궁극기가 따라감). 타이틀 카드
   줄 폭(`CardRowWidth`)은 카드 수에 맞춰.
타이틀 자동 캡처(키 입력)는 포커스를 잃어 Enter 가 씹히기 쉽다 — 키마다 Alt 를 한 번 눌렀다 떼고 SetForegroundWindow.

작업이 끝나면 HANDOFF.md 에 차수 기록을 남기고, 진행 상황은 메모리(`fox-girl-animal-friend` 같은 project 메모)에.
