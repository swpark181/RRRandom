"""Ear and tail acting: the reference page (2026-11-24, the user: "귀나 꼬리로 귀여움이 느껴지는 게임이나 애니메이션 자료 30개
하고 구현 관련 기술자료 30개 찾아서 지금 적절한 적용 기술 3순위 까지 골라줘. 그리고 적용 시간대비 효과가 큰 것도 3가지").
Writes index.html beside this file. The 60 entries were gathered by four web research passes on 2026-11-24 (games,
animation, Unreal techniques, general craft); "kind" says what the linked page is. Run with any Python 3.
"""
import html
import io
import os

from videos import VIDEOS   # (the user: "각 자료에 영상 자료를 찾아올 수 있으면 영상도 옆에 볼수있게 붙여줘")

e = html.escape
MAKER, PRESS, DOCS, FAN = "제작진 발언", "기사·리뷰", "공식 문서", "위키·팬 자료"

GAMES = [
    ("우마무스메 프리티 더비", "Cygames · 2021", "귀와 꼬리를 미묘한 감정을 내는 고유 부위로 씁니다. 풀이 죽으면 귀가 처지고 편할 때는 팔랑거립니다. 꼬리는 뿌리만 키를 주고 그 뒤는 시뮬레이션인데, 놀라서 꼬리가 곧게 서는 장면은 시뮬레이션을 끄고 손으로 움직였습니다.", "https://game.watch.impress.co.jp/docs/kikaku/1366170.html", MAKER),
    ("파이널 판타지 XIV", "Square Enix · 2013", "미코테는 다른 종족이 큰 몸짓으로 하는 감정 표현을 귀와 꼬리 움직임으로 합니다. 어느 감정 표현이 무엇을 하는지는 확인하지 못했습니다.", "https://trialsoffantasy.com/emotes/", FAN),
    ("몬스터 헌터 라이즈", "Capcom · 2021", "가루크를 만들 때 팀이 꼬리와 귀가 어떻게 흔들리는지를 거듭 논의했습니다.", "https://www.siliconera.com/monster-hunter-rise-interview-shares-canyne-design-details/", MAKER),
    ("Stray", "BlueTwelve Studio · 2022", "고양이를 애니메이터 한 명이 모션 캡처 없이 전부 키프레임으로 만들었고, 프로듀서는 꼬리·귀·수염·눈의 미묘한 움직임을 핵심으로 꼽습니다.", "https://techradar.com/features/stray-exclusive-how-the-devs-made-the-most-relatable-cat-game-of-all-time", MAKER),
    ("오리와 도깨비불", "Moon Studios · 2020", "귀와 꼬리를 별도의 물리 층으로 돌려서, 몸이 움직인 뒤에 따라오게 합니다.", "https://www.orithegame.com/rockpapershotgun-how-animation-powers-ori-and-the-will-of-the-wisps/", PRESS),
    ("Little Kitty, Big City", "Double Dagger Studio · 2024", "옆을 볼 때 귀가 먼저 움직입니다(참고 영상을 프레임 단위로 확인). 꼬리는 웃음을 위해 일부러 비현실적으로 꺾습니다.", "https://gamedeveloper.com/art/deep-dive-how-little-kitty-big-city-rejects-realism-to-achieve-authenticity", MAKER),
    ("더 라스트 가디언", "Japan Studio / genDESIGN · 2016", "토리코의 귀가 천장이나 높은 물체에 닿으면 고양이처럼 움찔합니다. 귀여움을 노리기보다 동물다운 표현을 목표로 했습니다.", "https://en.wikipedia.org/wiki/The_Last_Guardian", FAN),
    ("닌텐독스 + 캣츠", "Nintendo · 2011", "개는 꼬리를 힘차게, 고양이는 천천히 흔듭니다. 개 모델로 만든 고양이 시제품이 개처럼 꼬리를 흔들어서 애니메이션을 거의 따로 만들었습니다.", "https://www.nintendo.com/en-gb/Iwata-Asks/Iwata-Asks-Nintendo-3DS/Vol-4-nintendogs-cats/2-Adding-Kittens-Doubled-the-Work/2-Adding-Kittens-Doubled-the-Work-204778.html", MAKER),
    ("Night in the Woods", "Infinite Fall · 2017", "2D 고양이 주인공 메이의 귀가 가끔 고양이답게 움찔합니다.", "https://www.rpgfan.com/review/night-in-the-woods/", PRESS),
    ("VRChat (PhysBones)", "VRChat Inc. · 2017", "아바타의 귀와 꼬리에 물리로 2차 동작을 줍니다. 설정에 따라 다른 사람이 잡고 늘릴 수도 있습니다.", "https://creators.vrchat.com/avatars/avatar-dynamics/physbones", DOCS),
    ("소닉 더 헤지혹 2 (테일즈)", "Sega · 1992", "두 꼬리를 프로펠러처럼 돌려 날고 소닉을 나릅니다. 꼬리가 둘인 것은 디자인을 더 강하게 하려는 선택이었습니다.", "https://en.wikipedia.org/wiki/Tails_(Sonic_the_Hedgehog)", FAN),
    ("클로노아", "Namco · 1997", "큰 귀를 퍼덕여 날 수 있느냐는 질문에서 '누르고 있으면 떠 있는 점프'가 나왔습니다. 귀 애니메이션 자체는 출처에 없습니다.", "https://www.siliconera.com/klonoa-now-and-then-hideo-yoshizawa-returns-to-phantomile/", MAKER),
    ("라쳇 & 클랭크", "Insomniac Games · 2002~", "기분이 나쁘거나 체력이 낮으면 귀가 처진다고 합니다. 팬 위키의 내용이고 공식 자료에서는 확인하지 못했습니다.", "https://allthetropes.org/wiki/Expressive_Ears", FAN),
    ("Rain World", "Videocult · 2017", "꼬리와 팔다리는 물리 몸체 위에 절차적으로 그리는 층이라, 늘어져 끌리는 꼬리가 나옵니다.", "https://unity.com/blog/exploring-procedural-design-rain-world", PRESS),
    ("페이블 II", "Lionhead Studios · 2008", "개가 꼬리를 흔들며 공을 쫓고, 위협받으면 웅크려 떱니다. 귀는 언급이 없습니다.", "https://www.engadget.com/2007-03-08-fable-2-mans-best-friend.html", PRESS),
]
FILMS = [
    ("주토피아", "Walt Disney Animation · 2016", "주디의 귀는 한 번에 하나씩 움직이고 몸보다 먼저 갑니다. 소리 쪽으로 귀가 먼저 돌고 그다음에 몸이 돕니다.", "https://insidethemagic.net/?p=82746", MAKER),
    ("메이의 새빨간 비밀", "Pixar · 2022", "레서판다의 귀와 꼬리 움직임이 메이의 연기를 더 풍부하게 한다는 평입니다.", "https://inverse.com/entertainment/pixar-turning-red-review", PRESS),
    ("드래곤 길들이기 (투슬리스)", "Universal/DreamWorks 실사 · 2025", "현장 인형 머리의 귀판으로 놀람·호기심·두려움·분노를 표현했습니다.", "https://beforesandafters.com/2025/10/07/a-foam-head-performance/", MAKER),
    ("레이디와 트램프", "Disney 실사, Framestore · 2019", "개의 연기에서 귀를 어깨처럼 썼습니다. 꼬리도 뿌리와 엉덩이까지 같이 움직여야 따로 놀지 않았습니다.", "https://beforesandafters.com/2020/06/23/we-used-the-ears-like-shoulders-framestores-tips-for-animating-character-tics", MAKER),
    ("숀 더 쉽 무비", "Aardman · 2015", "대사 없이 표정이 눈, 눈꺼풀, 귀에서 나옵니다.", "https://www.annecyfestival.com/about/archives/2015/2015-meetings/2015-conference-summaries/summary:100000393398", MAKER),
    ("개들의 섬", "Wes Anderson 스톱모션 · 2018", "인형 제작자가 자기 개가 눈·귀·입으로 감정을 드러내는 모습을 관찰해 인형을 만들었습니다.", "https://www.filmindependent.org/blog/detail-oriented-the-stop-motion-master-who-brought-wes-andersons-isle-of-dogs-to-life/", MAKER),
    ("피터 래빗 2", "Sony, Animal Logic · 2021", "움직이는 사이사이에 작은 귀 움찔을 넣었습니다. 지켜보는 장면에서는 늘리고 말할 때는 줄였습니다.", "https://cgw.com/Publications/CGW/2021/July-August-September-2021/Rabbit-on-the-Run.aspx", MAKER),
    ("릴로 & 스티치", "Disney 실사, ILM · 2025", "스티치가 소리 나는 쪽으로 귀만 돌립니다. 아트 디렉터가 키우는 토끼의 귀에서 가져왔습니다.", "https://www.ilm.com/inside-the-ilm-art-department-lilo-stitch/", MAKER),
    ("배드 가이즈", "DreamWorks Animation · 2022", "칭찬을 받으면 울프의 꼬리가 저절로 흔들려 속마음이 드러납니다.", "https://filmthreat.com/reviews/the-bad-guys/", PRESS),
    ("늑대와 향신료 (2024)", "Passione · 2024", "호로가 화나면 꼬리털을 세우고, 귀를 까딱이며 작은 춤을 춥니다.", "https://www.animenewsnetwork.com/review/spice-and-wolf-merchant-meets-the-wise-wolf/episode-5/.210404", PRESS),
    ("볼트", "Walt Disney Animation · 2008", "크고 곧게 선 귀는 표현력을 높이고 얼굴을 가리지 않게 한 디자인입니다.", "https://en.wikipedia.org/wiki/Bolt_(Disney_character)", FAN),
    ("만달로리안 (그로구)", "Lucasfilm, Legacy Effects · 2019~", "인형 조종자 한 명이 얼굴 표정과 귀를 함께 움직입니다. 구체적인 귀 포즈는 출처에 없습니다.", "https://www.sideshow.com/blog/star-wars-puppets-and-puppeteers", PRESS),
    ("케모노 프렌즈", "Yaoyorozu · 2017", "서벌이 꼬리를 좌우로 흔드는 리듬은 그림 콘티로 잡을 수 없어서 영상 콘티로 잡았습니다.", "https://pastebin.com/5wF6ZvYu", MAKER),
    ("세상에서 제일 잘 돌봐주는 선코 씨", "Doga Kobo · 2019", "크고 폭신한 꼬리가 작품의 핵심 매력이고, 오프닝과 엔딩이 그 폭신함을 강조합니다.", "https://www.animenewsnetwork.com/review/the-helpful-fox-senko-san/episodes-1-2/.145847", PRESS),
    ("명탐정 피카츄", "Legendary, MPC·Framestore · 2019", "실루엣이 꼬리로 결정됩니다. 꼬리가 너무 헐렁하게 움직이면 산만해져서 조였습니다.", "https://www.cartoonbrew.com/interviews/detective-pikachu-vfx-supervisor-erik-nordby-on-the-huge-challenge-of-translating-pokemon-into-cg-174100.html", MAKER),
]
UNREAL = [
    ("FK 컨트롤 릭 (시퀀서)", "Epic 문서", "뼈마다 FK 컨트롤이 자동으로 생기는 트랙. 애디티브로 바꾸면 기본 동작 위에 꼬리 뼈만 키를 얹고, 끝나면 애니메이션 시퀀스로 구울 수 있습니다.", "https://dev.epicgames.com/documentation/unreal-engine/fk-control-rig-in-unreal-engine", DOCS),
    ("레이어드 컨트롤 릭 트랙", "Epic 문서", "애니메이션 시퀀스 위에 작은 꼬리·귀용 릭을 층으로 얹고 가중치를 키로 줍니다. 역방향 솔브 그래프가 필요합니다.", "https://dev.epicgames.com/documentation/unreal-engine/animating-with-control-rig-in-unreal-engine", DOCS),
    ("시퀀서 애니메이션 믹서", "Epic 개발자 커뮤니티 · UE 5.8", "시퀀서 안에서 애니메이션을 섞고 층으로 쌓는 트랙. 실험 단계 플러그인이고, 애디티브·마스크 층은 확인하지 못했습니다.", "https://forums.unrealengine.com/t/tutorial-animation-mixer-in-sequencer-intro/2731468", DOCS),
    ("컨트롤 릭으로 동적 체인 만들기", "Epic 개발자 커뮤니티 · UE 5.1+", "뒤늦게 따라오는 뼈 체인을 컨트롤 릭에서 만드는 법(용 리그 예제). 꼬리 뼈 4개에 그대로 쓸 수 있습니다.", "https://forums.unrealengine.com/t/tutorial-how-to-create-a-dynamic-chain-using-control-rig/1212267", DOCS),
    ("컨트롤 릭 스플라인 리깅", "Epic 문서", "컨트롤 몇 개로 꼬리 전체의 곡선을 잡습니다(말기, 휘두르기). 포즈용이고 시뮬레이션은 아닙니다.", "https://dev.epicgames.com/documentation/unreal-engine/control-rig-spline-rigging-in-unreal-engine", DOCS),
    ("Verlet + CCDIK 2차 동작", "80.lv (Epic의 Stéphane Biava 영상 소개) · 2025", "컨트롤 릭의 Verlet 노드로 목표를 흔들고 CCDIK 체인이 따라가게 해서 귀·꼬리 오버랩을 만듭니다.", "https://80.lv/articles/tutorial-creating-dynamic-control-rig-animations-in-unreal-engine-5", PRESS),
    ("컨트롤 릭 피직스", "Epic 개발자 커뮤니티 · 2025", "릭 안에서 물리 시뮬레이션과 충돌을 돌립니다. 실험 단계이고 튜토리얼 링크가 깨졌다는 보고가 있습니다.", "https://forums.unrealengine.com/t/tutorial-control-rig-physics/2668326", DOCS),
    ("AnimDynamics", "Epic 문서", "애니메이션 블루프린트의 가벼운 물리 노드. 꼬리는 체인 하나, 귀는 뼈마다 하나. 충돌은 없고 실시간용입니다.", "https://dev.epicgames.com/documentation/unreal-engine/animation-blueprint-animdynamics-in-unreal-engine", DOCS),
    ("Trail Controller / Spring Controller", "Epic 문서", "체인이 부모의 움직임을 뒤늦게 따라가게 합니다. 지난 프레임에 의존해서 프레임 시간에 따라 결과가 달라집니다.", "https://dev.epicgames.com/documentation/unreal-engine/animation-blueprint-trail-controller-in-unreal-engine", DOCS),
    ("Rigid Body (RBAN)", "Epic 문서", "피직스 애셋으로 애님 그래프 안에서 시뮬레이션합니다. 꼬리가 다리와 바닥에 부딪히게 할 수 있습니다.", "https://dev.epicgames.com/documentation/unreal-engine/animation-blueprint-rigid-body-in-unreal-engine", DOCS),
    ("KawaiiPhysics", "pafuhana1213 · GitHub · UE 5.3~5.8", "유사 물리 노드 하나로 꼬리와 귀를 흔듭니다. 시퀀서용 샘플이 있지만 렌더 큐에서의 재현성은 언급이 없습니다.", "https://github.com/pafuhana1213/KawaiiPhysics", DOCS),
    ("스켈레톤 편집 도구", "Epic 문서", "엔진 안에서 뼈를 추가하고 스킨을 묶고 가중치를 칠합니다. 머리 뼈 아래에 귀 뼈를 넣을 수 있습니다.", "https://dev.epicgames.com/documentation/unreal-engine/skeleton-editing-in-unreal-engine", DOCS),
    ("SkeletonModifier / SkinWeightModifier", "Epic 파이썬 API · UE 5.8 · 실험 단계", "뼈 추가와 정점 가중치 지정을 스크립트로 합니다. 자동 바인딩은 없어서 정점마다 가중치를 줘야 합니다.", "https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/SkeletonModifier", DOCS),
    ("엔진 안에서 모프 조각하기", "80.lv · UE 5.6 · 실험 단계", "스켈레탈 메시 에디터에서 모프 타깃을 만들고 조각합니다. 귀 처짐·접힘 모양을 블렌더 왕복 없이 만들 수 있습니다.", "https://80.lv/articles/you-can-create-sculpt-blend-shapes-directly-in-unreal-engine-5-6", PRESS),
    ("애니메이션 커브로 모프 구동", "Epic 문서", "모프 타깃과 이름이 같은 커브를 클립에 넣으면 그 값으로 모프가 움직입니다.", "https://dev.epicgames.com/documentation/unreal-engine/animation-curves-in-unreal-engine", DOCS),
    ("월드 포지션 오프셋 머티리얼 함수", "Epic 문서", "마스크를 준 정점을 재질에서 흔듭니다. 뼈 없이 귀 끝을 팔랑이게 할 수 있지만 음영은 따라오지 않습니다.", "https://dev.epicgames.com/documentation/unreal-engine/world-position-offset-material-functions-in-unreal-engine", DOCS),
    ("무비 렌더 큐 화질 설정", "Epic 문서", "시간 샘플마다 엔진이 한 틱씩 돌기 때문에, 시뮬레이션으로 움직이는 꼬리·귀가 뷰포트와 렌더에서 달라지는 이유를 설명합니다.", "https://dev.epicgames.com/documentation/unreal-engine/cinematic-rendering-image-quality-settings-in-unreal-engine", DOCS),
    ("렌더 큐에서 물리가 튀는 문제", "Epic 포럼 · 2020~2024", "시간 샘플을 올리면 천·물리·그룸이 떤다는 현장 보고. 결론은 '움직임을 구워서 내보내라'입니다. 뼈 체인 노드에 대한 보고는 아닙니다.", "https://forums.unrealengine.com/t/movie-render-queue-physics-go-crazy/469767", FAN),
]
CRAFT = [
    ("관절의 연속 꺾임", "Alexander Williams, Animation Apprentice · 2019", "움직임이 뿌리에서 시작해 관절마다 조금씩 늦게 끝으로 전달됩니다. 꼬리 뿌리를 먼저 돌리고 뒤 관절이 차례로 따라오게 합니다.", "https://animationapprentice.blogspot.com/2019/04/what-is-successive-breaking-of-joints.html", PRESS),
    ("커브를 밀어서 따라오게 하기", "Lester Banks (Paul Creamer의 팁) · 2016", "뿌리의 커브를 체인 아래로 복사하고 관절마다 시간을 조금씩 뒤로 밉니다. 꼬리와 귀 모두에 쓰는 가장 싼 방법입니다.", "https://lesterbanks.com/2016/11/animating-tail-offset-curves/", PRESS),
    ("동물 귀 자세가 전하는 감정", "Shijo 외, VRST 2022 (ITmedia 요약)", "귀를 뒤로 젖히면 놀람, 앞으로 숙이거나 천천히 바깥으로 돌리면 슬픔·두려움, 빠르게 되풀이하면 기쁨으로 읽습니다. 빠를수록 흥분도가 높게 읽힙니다.", "https://www.itmedia.co.jp/news/article/2212/07/1221207041/", PRESS),
    ("개의 몸짓 언어", "Amy L. Pike 수의사, dvm360 · 2018", "귀 앞은 경계, 옆은 걱정, 뒤로 바짝은 달래기. 꼬리는 등보다 높으면 흥분, 내리면 두려움, 말아 넣으면 큰 불안. 여우 포즈 표로 그대로 쓸 수 있습니다.", "https://www.dvm360.com/view/canine-body-language-basics", PRESS),
    ("고양이의 몸짓 언어", "RSPCA Australia · 2023", "편할 때 귀는 자연스럽고 꼬리는 서서 끝이 휘고, 불안하면 귀가 옆으로 돌고 꼬리를 말며, 화나면 귀를 납작하게 합니다. 호랑이 포즈 표가 됩니다.", "https://kb.rspca.org.au/knowledge-base/what-does-my-cats-body-language-mean/", PRESS),
    ("붉은여우의 행동", "Wildlife Online · 2023", "우위일 때 귀를 세우고 꼬리를 수평 위로, 복종할 때 귀를 눕히고 꼬리를 말며 반길 때는 꼬리를 세차게 흔듭니다.", "https://www.wildlifeonline.me.uk/animals/article/red-fox-behaviour-the-social-hierarchy", PRESS),
    ("Lil Gator Game의 흔들 뼈", "Game Developer · 2022", "뼈마다 부모에서 자식 순으로 흔들림을 계산하는 자체 물리. 머리가 몸의 방향을 늦게 따라가게 해서 과장된 귀여움을 냅니다.", "https://gamedeveloper.com/design/the-exaggerated-animations-and-tottering-wobble-bones-of-lil-gator-game", MAKER),
    ("기존 스킨을 지키며 뼈 추가하기", "Blender Artists 포럼 · 2024", "뼈를 넣고 빈 그룹으로만 묶은 뒤, 다른 정점 그룹을 잠그고 새 뼈만 칠합니다. 이미 스킨이 된 메시에 귀 뼈를 넣는 절차입니다.", "https://blenderartists.org/t/rigify-add-new-bone-to-rig-and-keep-original-skinning/1520654", FAN),
    ("Jiggle Physics 애드온", "naelstrof · GitHub", "뼈에 Verlet 흔들림을 주는 블렌더 애드온. 렌더 전에 반드시 구워야 하고, 용의 꼬리와 귀 예시가 있습니다.", "https://github.com/naelstrof/blender-jiggle-physics", DOCS),
    ("스프링 수식 모음", "theorangeduck.com · 2021", "프레임 속도와 무관하게 정확한 스프링·댐퍼 수식. 살짝 덜 감쇠된 스프링으로 관절을 목표 자세로 끌면 넘쳤다가 가라앉는 움직임이 나옵니다.", "https://theorangeduck.com/page/spring-roll-call", PRESS),
    ("VRM 스프링 본 규격", "VRM Consortium · GitHub", "관절마다 관성, 원래 방향으로 당기는 힘, 중력, 길이 제한을 차례로 적용합니다. 뻣뻣함을 낮추고 저항을 올리면 묵직한 꼬리, 뻣뻣함을 올리면 쫑긋한 귀가 됩니다.", "https://github.com/vrm-c/vrm-specification/blob/master/specification/VRMC_springBone-1.0/README.md", DOCS),
    ("오리와 눈먼 숲 애니메이션 (GDC 2015 정리)", "James Benson 강연의 제3자 노트 · 2018", "꼬리는 진행 방향을 보여 주는 장치이고, 회전할 때 몇 프레임 동안 길이의 세 배까지 늘립니다. 뼈 스케일로 하는 스미어입니다.", "https://zyzyz.github.io/en/2018/01/GDC2015-Animating-Ori", FAN),
]
assert len(GAMES) == 15 and len(FILMS) == 15 and len(UNREAL) == 18 and len(CRAFT) == 12


def rows(items, start):
    out = []
    for i, (title, by, note, url, kind) in enumerate(items, start):
        host = url.split("/")[2].replace("www.", "")
        vids = "".join(f'<a class="vid" href="{e(v)}"><span aria-hidden="true">▶</span> {e(who)}</a><span class="kind">{e(what)}</span>'
                       for v, who, what in VIDEOS.get(i, [])) or '<span class="kind">영상을 찾지 못했습니다</span>'
        out.append(f'        <tr><td class="num">{i}</td><td><b>{e(title)}</b><span class="by">{e(by)}</span></td><td>{e(note)}</td>'
                   f'<td class="src"><a href="{e(url)}">{e(host)}</a><span class="kind">{kind}</span></td><td class="video">{vids}</td></tr>')
    return "\n".join(out)


def table(items, start, first):
    return f'''    <div class="tablebox">
      <table>
        <thead><tr><th class="num">번호</th><th>{first}</th><th>귀·꼬리에 대해 알 수 있는 것</th><th>출처</th><th>영상</th></tr></thead>
        <tbody>
{rows(items, start)}
        </tbody>
      </table>
    </div>'''


CSS = '''
/* layout: a reference sheet. The picks first (three techniques in order, three quick wins), then the two lists of
   thirty as numbered tables whose numbers the picks cite. Same look as the storyboard pages. */
:root {
  --bg: #f2f1f6; --sheet: #ffffff; --ink: #1c1a29; --soft: #5d5a70; --line: #d6d3e2; --accent: #5b3fd0; --accent-soft: #e9e4fb; --quick: #1f7a4d; --quick-soft: #e2f3ea;
  --font: "Noto Sans KR", "Apple SD Gothic Neo", "Malgun Gothic", system-ui, sans-serif;
  --num: "Barlow Condensed", "Arial Narrow", "Noto Sans KR", sans-serif;
}
@media (prefers-color-scheme: dark) {
  :root:not([data-theme="light"]) { --bg: #15141d; --sheet: #1e1c2a; --ink: #ecebf4; --soft: #a3a0b8; --line: #353247; --accent: #a892ff; --accent-soft: #2c2650; --quick: #6fd3a0; --quick-soft: #1c3329; color-scheme: dark; }
}
:root[data-theme="dark"] { --bg: #15141d; --sheet: #1e1c2a; --ink: #ecebf4; --soft: #a3a0b8; --line: #353247; --accent: #a892ff; --accent-soft: #2c2650; --quick: #6fd3a0; --quick-soft: #1c3329; color-scheme: dark; }
body { background: var(--bg); color: var(--ink); font-family: var(--font); font-size: 15px; line-height: 1.65;
  padding-block: 28px 64px; padding-left: max(16px, env(safe-area-inset-left, 0px)); padding-right: max(16px, env(safe-area-inset-right, 0px)); }
.wrap { max-width: 1120px; margin: 0 auto; display: flex; flex-direction: column; gap: 28px; }
header { display: flex; flex-direction: column; gap: 10px; }
.eyebrow { font-size: 12px; font-weight: 700; letter-spacing: .08em; color: var(--accent); }
h1 { font-size: clamp(24px, 4vw, 34px); font-weight: 700; line-height: 1.25; margin: 0; text-wrap: balance; }
h2 { font-size: 18px; font-weight: 700; margin: 0; }
h3 { font-size: 16px; font-weight: 700; margin: 0; line-height: 1.4; }
p { margin: 0; max-width: 68ch; }
.lead { color: var(--soft); }
.small { font-size: 13px; }
section { display: flex; flex-direction: column; gap: 12px; }
.picks { display: grid; grid-template-columns: repeat(3, minmax(0, 1fr)); gap: 12px; }
.pick { background: var(--sheet); border: 1px solid var(--line); border-radius: 8px; padding: 14px 16px; display: flex; flex-direction: column; gap: 8px; min-width: 0; }
.rank { font-family: var(--num); font-size: 15px; font-weight: 700; letter-spacing: .04em; color: var(--accent); background: var(--accent-soft); border-radius: 4px; padding: 1px 9px; align-self: flex-start; }
.quick .rank { color: var(--quick); background: var(--quick-soft); }
.pick p { font-size: 14px; }
.pick dl { margin: 0; display: flex; flex-direction: column; gap: 4px; font-size: 13px; }
.pick dt { font-weight: 700; color: var(--soft); font-size: 12px; }
.pick dd { margin: 0; }
.cite { font-variant-numeric: tabular-nums; color: var(--soft); }
.tablebox { overflow-x: auto; background: var(--sheet); border: 1px solid var(--line); border-radius: 8px; }
table { border-collapse: collapse; width: 100%; min-width: 860px; font-size: 14px; }
th, td { text-align: left; padding: 8px 12px; border-bottom: 1px solid var(--line); vertical-align: top; }
tr:last-child td { border-bottom: 0; }
th { font-size: 12px; color: var(--soft); font-weight: 700; white-space: nowrap; }
td.num, th.num { font-family: var(--num); font-size: 17px; font-weight: 700; color: var(--accent); font-variant-numeric: tabular-nums; white-space: nowrap; }
th.num { font-family: var(--font); font-size: 12px; color: var(--soft); }
td:nth-child(2) { width: 17%; min-width: 130px; }
.by { display: block; font-size: 12px; color: var(--soft); }
td.src { width: 15%; min-width: 130px; font-size: 13px; overflow-wrap: anywhere; }
td.video { width: 27%; min-width: 210px; font-size: 13px; }
td.video .kind + .vid { margin-top: 8px; }
.vid { display: inline-block; color: var(--accent); font-weight: 700; text-decoration: none; background: var(--accent-soft); border-radius: 4px; padding: 1px 8px; }
.vid:hover { text-decoration: underline; }
td.src a { color: var(--accent); }
.kind { display: block; font-size: 11px; color: var(--soft); }
.list { background: var(--sheet); border: 1px solid var(--line); border-radius: 8px; padding: 12px 18px; }
.list ul { margin: 0; padding-left: 18px; font-size: 14px; }
.list li + li { margin-top: 4px; }
a:focus-visible { outline: 2px solid var(--accent); outline-offset: 2px; }
@media (max-width: 820px) { .picks { grid-template-columns: minmax(0, 1fr); } }
'''

page = f'''<title>귀와 꼬리 연기 자료</title>
<meta name="claude:viewport-fit-sides" content="cover">
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Barlow+Condensed:wght@600;700&family=Noto+Sans+KR:wght@400;500;700&display=swap">
<style>{CSS}</style>

<div class="wrap">
  <header>
    <div class="eyebrow">RRRANDOM · 시네마틱 · 2026-11-24</div>
    <h1>귀와 꼬리 연기 자료</h1>
    <p class="lead">귀나 꼬리로 귀여움과 감정을 전하는 작품 30개와 구현 기술 자료 30개를 모으고, 지금 여우 vs 호랑이 시네마틱에 맞는 기술을 골랐습니다. 고른 것을 먼저 적고, 근거가 된 자료는 아래 표의 번호로 가리켰습니다(작품 1~30, 기술 31~60). 자료마다 볼 수 있는 영상을 오른쪽 칸에 붙였고, 누르면 새 탭에서 열립니다.</p>
  </header>

  <section>
    <h2>지금 적용할 기술 3순위</h2>
    <p class="lead small">전제: 여우와 호랑이 리그에는 꼬리 뼈가 4개씩 있고 귀 뼈는 없습니다. 동작 클립은 스크립트로 만들고, 영상은 프레임마다 여러 번 샘플링하는 오프라인 렌더로 뽑습니다.</p>
    <div class="picks">
      <div class="pick">
        <span class="rank">1순위</span>
        <h3>뼈에 키를 넣어 굽는 연기</h3>
        <p>꼬리(나중에는 귀) 뼈에 감정 포즈를 키로 넣고, 뿌리에서 끝으로 조금씩 늦게 따라가게 만든 뒤 클립에 구워 넣습니다.</p>
        <dl>
          <dt>왜</dt><dd>꼬리는 오늘 시험에서 이미 됐습니다. 구운 키는 몇 번을 렌더해도 같은 결과가 나옵니다. 우마무스메도 감정이 걸린 장면은 시뮬레이션을 끄고 손으로 움직였습니다.</dd>
          <dt>근거</dt><dd class="cite">1, 19, 47, 48, 49, 50</dd>
        </dl>
      </div>
      <div class="pick">
        <span class="rank">2순위</span>
        <h3>귀 뼈 추가</h3>
        <p>시네마틱 전용 몸·털 메시에 귀 뼈를 좌우 하나씩 넣습니다. 그 뒤로는 1순위와 같은 방식으로 귀를 연기시킵니다.</p>
        <dl>
          <dt>왜</dt><dd>귀 정점이 전부 머리 뼈 하나에 묶여 있어서 스크립트로 귀만 골라 가중치를 줄 수 있습니다. 털은 몸의 가중치를 따라가므로 다시 만들면 같이 움직입니다. 게임 메시는 건드리지 않습니다.</dd>
          <dt>근거</dt><dd class="cite">16, 19, 42, 43, 51, 56</dd>
        </dl>
      </div>
      <div class="pick">
        <span class="rank">3순위</span>
        <h3>스프링을 계산해서 굽는 자동 오버랩</h3>
        <p>달리기, 벽에 부딪히기, 밀려나기처럼 몸이 크게 움직이는 곳은 스프링 수식으로 꼬리와 귀의 출렁임을 계산해 키로 굽습니다.</p>
        <dl>
          <dt>왜</dt><dd>실시간 물리 노드는 이 렌더 방식에서 프레임마다 결과가 흔들릴 수 있고, 지금 캐릭터에는 그런 노드를 돌릴 애니메이션 블루프린트도 없습니다. 같은 수식을 클립을 만들 때 계산하면 그 문제가 없습니다.</dd>
          <dt>근거</dt><dd class="cite">5, 47, 48, 57, 58, 59</dd>
        </dl>
      </div>
    </div>
    <p class="small lead">고르지 않은 것: 모프 타깃(44, 45: 몸과 털 양쪽에 모양을 만들어야 함), 재질에서 정점 밀기(46: 음영이 따라오지 않음), 실시간 물리 노드(37~41: 렌더할 때마다 결과가 달라질 수 있음), 시퀀서에서 손으로 키 잡기(31~36: 시퀀스를 스크립트가 다시 만들 때 손작업이 지워짐).</p>
  </section>

  <section>
    <h2>들인 시간에 비해 효과가 큰 것 3가지</h2>
    <div class="picks">
      <div class="pick quick">
        <span class="rank">가장 빠름</span>
        <h3>꼬리 감정 포즈 다섯 군데</h3>
        <p>새 에셋 없이 바로 됩니다. 움츠릴 때 꼬리를 다리 쪽으로 말고, 망치가 주사위에 맞는 순간과 5가 나오는 순간에 꼬리를 곧게 세우고, 포위한 분신들은 꼬리를 서로 어긋나게 흔들고, 호랑이는 다가올 때 꼬리 끝을 휘두르다가 포위되면 내립니다.</p>
        <dl><dt>근거</dt><dd class="cite">1, 24, 25, 52, 53, 54</dd></dl>
      </div>
      <div class="pick quick">
        <span class="rank">클로즈업 효과</span>
        <h3>귀 포즈 네 가지만</h3>
        <p>귀 뼈를 넣은 뒤에는 포즈 네 개로 충분합니다. 놀라면 뒤로 젖히고, 눈을 감을 때 눕히고, 5가 나오면 쫑긋 세우고, 뒤돌아볼 때는 한쪽 귀가 머리보다 먼저 돕니다. 얼굴 클로즈업(컷 1, 4-3, 9)에서 귀가 화면의 큰 부분을 차지해서 효과가 큽니다.</p>
        <dl><dt>근거</dt><dd class="cite">6, 16, 19, 22, 23, 51</dd></dl>
      </div>
      <div class="pick quick">
        <span class="rank">한 번에 전체</span>
        <h3>커브를 밀어서 꼬리 따라오게 하기</h3>
        <p>뿌리의 움직임을 관절마다 몇 프레임씩 늦춰 복사하는 것만으로 달리기, 벽 충돌, 주저앉기 전부에 꼬리의 따라옴이 생깁니다. 클립 생성기에 몇 줄 넣으면 모든 동작에 적용됩니다.</p>
        <dl><dt>근거</dt><dd class="cite">5, 49, 50, 60</dd></dl>
      </div>
    </div>
  </section>

  <section>
    <h2>작품 자료 30</h2>
    <h3>게임 (1~15)</h3>
{table(GAMES, 1, "게임")}
    <h3>애니메이션·영화 (16~30)</h3>
{table(FILMS, 16, "작품")}
  </section>

  <section>
    <h2>구현 기술 자료 30</h2>
    <h3>언리얼 엔진 (31~48)</h3>
{table(UNREAL, 31, "기술")}
    <h3>애니메이션·리깅 일반 (49~60)</h3>
{table(CRAFT, 49, "기술")}
  </section>

  <section>
    <h2>알아 둘 것</h2>
    <div class="list">
      <ul>
        <li>자료는 웹 조사 네 갈래로 72건을 모은 뒤, 페이지를 열지 못했거나 귀·꼬리 내용이 출처에 없는 것을 빼고 60건을 남겼습니다.</li>
        <li>출처 칸 아래의 표시는 링크된 글의 종류입니다. "위키·팬 자료"는 제작진이 직접 말한 것이 아니라서 참고 수준으로 보셔야 합니다.</li>
        <li>설명은 링크된 글에 실제로 적힌 범위까지만 썼습니다. 글에 없던 내용은 "확인하지 못했습니다"라고 적었습니다.</li>
        <li>언리얼 문서는 버전 표시 없이 최신판으로 열리는 주소입니다. 실험 단계라고 적힌 기능은 그대로 표시했습니다.</li>
        <li>영상은 60건 가운데 59건에 붙였습니다(파이썬으로 뼈를 추가하는 항목만 없음). 이 페이지에서는 바깥 영상을 안에 넣어 재생할 수 없어서 링크로 달았습니다.</li>
        <li>영상은 제목, 채널, 설명, 챕터 목록을 보고 골랐고 실제로 재생해 보지는 못했습니다. 예고편은 그 캐릭터가 나온다는 것까지만 확인된 것이고, 설명한 귀·꼬리 동작이 그 영상에 나오는지는 보셔야 압니다. 시각이 적힌 것은 챕터나 자막에 있던 시각입니다.</li>
        <li>귀 뼈 추가는 아직 만들어 보지 않았습니다. 꼬리는 시험 클립으로 움직이는 것까지 확인했습니다.</li>
      </ul>
    </div>
  </section>
</div>
'''
out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "index.html")
io.open(out, "w", encoding="utf-8", newline="\n").write(page)
print("EARTAIL", out, len(page), "chars")
