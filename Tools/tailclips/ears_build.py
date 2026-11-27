"""Ears: per-animal references and techniques (2026-11-25, the user: "이제 귀도 작업하고 싶다. 이건 일정하게 작업은 안 될것 같고
동물별로 자료 10가지, 기술 10가지 확인해보고 각각 장단점 리스트 뽑아줘").
Writes index.html beside this file. References: refs_a.json / refs_b.json (two web research passes, ten works for each
ear type, sources opened and checked by the researcher; "confidence" is theirs). Techniques: the ten below, judged for
each animal friend against what its ears are in the game now (now_<Name>.jpg, captured on the Studio map; none of the
six has an ear, horn or fin bone - checked in the engine with Tools/cinematic/ue_cine_bones.py).
The look is the earlier ear and tail page's (../eartail/build.py). Run with any Python 3.
"""
import html
import io
import json
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
e = html.escape
CSS = re.search(r"CSS = '''(.*?)'''", io.open(os.path.join(HERE, "..", "eartail", "build.py"), encoding="utf-8").read(), re.S).group(1)

# name, key in the JSON files, what its ears are now, bones now
ANIMALS = [
    ("Fox", "여우", "fox", "크고 곧게 선 뾰족한 귀. 머리 높이의 절반쯤이고 밑동이 넓습니다. 안쪽에 크림색 털, 끝은 어두운 색.",
     "뼈 49개(꼬리 8개 포함). 귀는 머리 뼈 하나에 통째로 묶여 있습니다."),
    ("RedFox", "레드 폭스", "fennec", "머리보다 큰 귀. 옆으로 넓게 벌어져 있고 안쪽은 분홍, 끝은 검습니다. 이마에 리본이 있어 귀와 가깝습니다.",
     "뼈 37개(꼬리 4개 포함). 귀는 머리 뼈에 묶여 있습니다."),
    ("Rabbit", "토끼", "rabbit", "긴 귀 두 개. 왼쪽은 곧게 서 있고 오른쪽은 중간에서 접혀 내려와 있습니다(접힌 귀가 이 캐릭터의 생김새).",
     "뼈 57개(손가락이 많음). 꼬리 뼈도 귀 뼈도 없습니다."),
    ("Squirrel", "다람쥐", "squirrel", "가늘고 뾰족한 귀에 끝 털 뭉치. 크기는 작습니다. 말아 올린 큰 꼬리가 머리 바로 뒤에 있어 귀와 닿을 만큼 가깝습니다.",
     "뼈 45개(꼬리 4개 포함). 귀는 머리 뼈에 묶여 있습니다."),
    ("Tiger", "호랑이", "tiger", "작고 둥근 귀. 검은 테에 안쪽이 흽니다. 둥근 머리의 양옆 위쪽에 붙어 있습니다.",
     "뼈 45개(꼬리 4개 포함). 귀는 머리 뼈에 묶여 있습니다."),
    ("Dragon", "용", "dragon", "귀 대신 뒤로 휜 뿔 두 개와 양옆의 뾰족한 지느러미가 있습니다. 털이 없고, 등에 가시가 줄지어 있습니다.",
     "뼈 41개(날개 4개, 꼬리 4개 포함). 뿔과 지느러미는 머리 뼈에 묶여 있습니다."),
]

GOOD, MID, BAD = "잘 맞음", "조건부", "안 맞음"
EPIC = "https://dev.epicgames.com/documentation/unreal-engine/"
# title, what it is, how it would be done here, sources [(label, url)], pros, cons, fit per animal {name: (grade, note)}
TECHS = [
    ("귀 뼈 + 스프링 체인",
     "귀마다 뼈를 1~4개 넣고, 지금 꼬리에 쓰는 뼈별 스프링을 귀에도 돌립니다. 머리가 움직이면 귀가 조금 늦게 따라오고 출렁입니다.",
     "Blender에서 귀 가운데 선을 따라 뼈를 놓고 표면을 이웃 뼈와 나눠 묶습니다(꼬리에서 만든 방식). 게임 코드는 꼬리 체인을 귀 두 개에 한 번씩 더 돌리면 됩니다.",
     [("Lil Gator Game의 흔들 뼈", "https://gamedeveloper.com/design/the-exaggerated-animations-and-tottering-wobble-bones-of-lil-gator-game"),
      ("스프링 수식 모음", "https://theorangeduck.com/page/spring-roll-call")],
     ["꼬리에서 만든 코드, 조절 명령, 녹화 도구를 그대로 씁니다.", "뼈 몇 개라 게임에서 가볍습니다.", "털은 몸의 가중치를 따라가므로 다시 만들면 같이 움직입니다.",
      "뼈만 늘어나므로 애니메이션을 다시 받을 필요가 없습니다."],
     ["동물마다 귀 뼈를 놓고 가중치를 잡는 작업이 필요합니다.", "귀 밑동과 머리 사이를 좁게 나누면 접힌 자국이 납니다(꼬리에서 겪은 문제).",
      "털 메시를 다시 만들어야 합니다.", "출렁임만으로는 감정이 읽히지 않습니다."],
     {"Fox": (GOOD, "귀가 크고 곧게 서 있어 끝이 늦게 따라오는 움직임이 잘 보입니다. 귀마다 뼈 2개."),
      "RedFox": (GOOD, "여섯 중 효과가 가장 큽니다. 귀마다 뼈 3개. 귀끼리, 귀와 리본이 겹치지 않게 한도를 둬야 합니다."),
      "Rabbit": (GOOD, "긴 귀라 꼬리와 거의 같은 방식이 됩니다. 귀마다 뼈 3~4개. 접힌 귀는 접힌 모양을 기준으로 흔들리게 합니다."),
      "Squirrel": (MID, "귀가 작고 가늘어 출렁임이 거의 안 보입니다. 넣는다면 귀마다 뼈 1개."),
      "Tiger": (MID, "작고 둥근 귀는 출렁일 길이가 없습니다. 뼈 1개로 돌리는 용도에 가깝습니다."),
      "Dragon": (MID, "뿔은 단단해야 해서 대상이 아닙니다. 옆 지느러미에만 뼈 1~2개.")}),
    ("귀 뼈 + 상태별 포즈",
     "서 있을 때 가끔 까딱, 조준하면 뒤로 눕힘, 맞으면 움찔, 달리면 뒤로 젖힘처럼 상황마다 귀의 목표 각도를 코드가 줍니다.",
     "방금 넣은 '아이들 꼬리'와 같은 틀입니다. 종별 표에 각도를 적고, 스프링이 그 자세를 향해 움직이게 합니다. 귀 뼈가 먼저 있어야 합니다.",
     [("동물 귀 자세가 전하는 감정 (VRST 2022 요약)", "https://www.itmedia.co.jp/news/article/2212/07/1221207041/"),
      ("붉은여우의 행동", "https://www.wildlifeonline.me.uk/animals/article/red-fox-behaviour-the-social-hierarchy"),
      ("고양이의 몸짓 언어", "https://kb.rspca.org.au/knowledge-base/what-does-my-cats-body-language-mean/")],
     ["감정과 상태가 귀로 읽힙니다. 위에서 내려다보는 시점에서도 보이는 부위입니다.", "숫자 몇 개로 조절하고 콘솔에서 바로 바꿔 볼 수 있습니다.",
      "스프링 체인 위에 그대로 얹힙니다."],
     ["동물마다, 상태마다 값을 정하고 확인해야 합니다.", "너무 자주, 크게 움직이면 산만합니다.", "게임 상태(조준, 피격, 승리 등)와 이어 주는 코드가 필요합니다."],
     {"Fox": (GOOD, "쫑긋 세우기, 뒤로 눕히기, 한쪽만 돌리기가 모두 잘 보입니다."),
      "RedFox": (GOOD, "귀가 얼굴보다 커서 귀 자세가 곧 표정이 됩니다. 각도는 작게 잡아야 과하지 않습니다."),
      "Rabbit": (GOOD, "두 귀를 따로 움직일 수 있습니다. 선 귀는 방향, 접힌 귀는 들썩임으로 나눠 쓸 수 있습니다."),
      "Squirrel": (GOOD, "작은 각도로 쫑긋과 눕힘만 줘도 충분합니다."),
      "Tiger": (GOOD, "귀를 납작하게 눕히는 것이 큰 고양이의 대표 신호입니다. 뼈 1개로 됩니다."),
      "Dragon": (GOOD, "지느러미를 펼치고 접는 것으로 같은 역할을 합니다.")}),
    ("엔진의 AnimDynamics, Trail·Spring 컨트롤러",
     "엔진에 들어 있는 가벼운 물리 노드로 귀 뼈를 흔듭니다.",
     "지금 몸 애니메이션은 C++ 코드가 직접 계산하고 애니메이션 블루프린트 그래프가 없습니다. 이 노드를 쓰려면 후처리용 애니메이션 블루프린트를 새로 만들어 붙여야 합니다.",
     [("AnimDynamics", EPIC + "animation-blueprint-animdynamics-in-unreal-engine"),
      ("Trail Controller", EPIC + "animation-blueprint-trail-controller-in-unreal-engine")],
     ["엔진이 제공하고 관리합니다.", "각도 한도와 충돌 평면이 들어 있습니다.", "에디터에서 눈으로 보며 조절할 수 있습니다."],
     ["지금 구조에 없는 애니메이션 블루프린트를 동물마다 추가해야 합니다.", "결과는 스프링 체인과 거의 같습니다.", "상태별 포즈를 주려면 어차피 코드와 이어야 합니다."],
     {n: (MID, "쓸 수는 있지만 꼬리 코드를 재사용하는 쪽보다 얻는 것이 적습니다.") for n in ("Fox", "RedFox", "Rabbit", "Squirrel", "Tiger", "Dragon")}),
    ("KawaiiPhysics 플러그인",
     "귀와 꼬리를 흔드는 데 널리 쓰이는 공개 플러그인입니다. 구와 캡슐 충돌로 귀가 머리나 서로를 뚫지 않게 할 수 있습니다.",
     "플러그인을 프로젝트에 넣고, 후처리 애니메이션 블루프린트에서 노드 하나로 귀 뼈를 지정합니다.",
     [("KawaiiPhysics", "https://github.com/pafuhana1213/KawaiiPhysics")],
     ["귀·꼬리 전용이라 설정이 쉽고 예제가 많습니다.", "충돌이 있어 큰 귀와 긴 귀에 유리합니다."],
     ["외부 플러그인에 기대게 됩니다. 엔진을 올릴 때 따라 올려야 합니다.", "역시 애니메이션 블루프린트가 필요합니다.", "꼬리 코드와 방식이 둘로 나뉩니다."],
     {"Fox": (MID, "충돌이 꼭 필요하지 않아 이점이 작습니다."),
      "RedFox": (MID, "큰 귀끼리, 귀와 리본이 겹치는 문제가 한도만으로 안 풀릴 때의 대안입니다."),
      "Rabbit": (MID, "긴 귀가 머리와 어깨를 뚫는 문제가 생기면 고려할 만합니다."),
      "Squirrel": (BAD, "귀가 작아 얻는 것이 없습니다."),
      "Tiger": (BAD, "귀가 작아 얻는 것이 없습니다."),
      "Dragon": (BAD, "지느러미는 한도만으로 충분합니다.")}),
    ("모프 타깃으로 귀 자세 만들기",
     "귀를 눕힌 모양, 접은 모양을 메시의 변형으로 미리 만들어 두고 코드로 섞습니다.",
     "Blender에서 귀 정점을 밑동 기준으로 돌리거나 접어 변형을 만듭니다. 털은 따로 된 메시(게임 12겹, 영상 36겹)라 같은 변형을 털에도 넣어야 합니다.",
     [("애니메이션 커브로 모프 구동", EPIC + "animation-curves-in-unreal-engine"),
      ("엔진 안에서 모프 조각하기", "https://80.lv/articles/you-can-create-sculpt-blend-shapes-directly-in-unreal-engine-5-6")],
     ["뼈대를 건드리지 않습니다.", "접힘, 말림, 납작해짐처럼 뼈 한두 개로 만들기 어려운 모양을 정확히 잡습니다."],
     ["몸과 털 양쪽에 같은 변형을 넣어야 해서 작업과 용량이 큽니다.", "출렁임이 없습니다.", "자세마다 변형을 하나씩 만들어야 합니다."],
     {"Fox": (BAD, "뼈로 돌리는 것으로 충분합니다."),
      "RedFox": (BAD, "귀가 커서 털까지 변형하면 용량이 큽니다."),
      "Rabbit": (MID, "접힌 귀를 폈다 접는 동작만은 변형이 더 깔끔합니다. 그 한 가지에만 쓸 만합니다."),
      "Squirrel": (BAD, "귀가 작아 뼈 1개로 충분합니다."),
      "Tiger": (MID, "둥근 귀가 납작해지는 모양은 변형이 더 자연스럽습니다. 다만 털 문제는 같습니다."),
      "Dragon": (MID, "털이 없어 부담이 가장 작습니다. 지느러미를 펼치는 모양에 쓸 수 있습니다.")}),
    ("재질에서 정점 밀기",
     "귀 끝 쪽 정점을 재질에서 흔듭니다. 뼈도 리그 작업도 없이 살랑임만 줍니다.",
     "귀 끝으로 갈수록 커지는 마스크를 정점 색이나 높이로 만들고, 몸 재질과 털 재질에 같은 식을 넣습니다. 털 재질에는 이미 바람과 끌림이 들어 있습니다.",
     [("월드 포지션 오프셋", EPIC + "world-position-offset-material-functions-in-unreal-engine")],
     ["리그와 에셋 작업이 거의 없습니다.", "아주 가볍습니다.", "털에도 같은 식으로 넣을 수 있습니다."],
     ["흔들림만 되고 자세는 못 만듭니다.", "크게 주면 젤리처럼 보이고 그림자와 어긋납니다.", "상태에 반응시키기 어렵습니다."],
     {"Fox": (MID, "귀 끝과 안쪽 털의 잔떨림을 더하는 보조로만."),
      "RedFox": (MID, "귀 안쪽 긴 털의 잔떨림 보조로만."),
      "Rabbit": (BAD, "긴 귀는 큰 움직임이 필요합니다."),
      "Squirrel": (GOOD, "귀 끝 털 뭉치의 떨림은 이것으로 충분합니다."),
      "Tiger": (MID, "귀 테두리 털의 잔떨림 정도."),
      "Dragon": (BAD, "딱딱한 부위라 어울리지 않습니다.")}),
    ("귀를 따로 만든 파츠로 붙이기",
     "주둥이 파츠처럼 귀를 따로 된 메시로 만들어 머리에 붙이고 통째로 돌리거나 다른 모양으로 갈아 낍니다.",
     "머리에서 귀를 잘라 내고 구멍을 막은 뒤, 귀를 머리 뼈 아래 뼈에 통째로 묶습니다.",
     [("여우 꼬리 5판 (이 프로젝트)", "https://claude.ai/artifact/MT1Vr8JV8x8RrJa7CcJ58z")],
     ["통째로 돌리기 쉽습니다.", "표정이나 변신에 따라 귀 모양을 갈아 낄 수 있습니다."],
     ["붙인 자리가 드러납니다. 여우 꼬리에서 같은 방식이 그 이유로 버려졌습니다.", "털이 있는 동물은 경계에서 털이 끊깁니다."],
     {"Fox": (BAD, "털 때문에 붙인 자리가 보입니다."),
      "RedFox": (BAD, "같은 이유. 구미호 변신 때 귀를 바꾸는 용도라면 다시 볼 만합니다."),
      "Rabbit": (BAD, "같은 이유."),
      "Squirrel": (BAD, "같은 이유."),
      "Tiger": (BAD, "같은 이유."),
      "Dragon": (MID, "털이 없고 딱딱한 부위라 경계가 덜 어색합니다. 뿔 모양을 바꿔야 할 일이 생기면 맞는 방식입니다.")}),
    ("시퀀서에서 귀 뼈에 키 넣기",
     "영상에서는 귀 뼈에 직접 키를 넣어 연기시킵니다. 기본 동작 위에 얹는 방식이라 몸 동작은 그대로 둡니다.",
     "귀 뼈가 생긴 뒤, 시네마틱을 짓는 스크립트에서 컷마다 귀 각도를 키로 넣습니다.",
     [("FK 컨트롤 릭", EPIC + "fk-control-rig-in-unreal-engine")],
     ["연기를 프레임 단위로 정할 수 있습니다.", "몇 번을 렌더해도 같은 결과가 나옵니다."],
     ["게임에는 쓰이지 않습니다.", "컷마다 손으로 정해야 합니다.", "귀 뼈가 먼저 있어야 합니다."],
     {"Fox": (GOOD, "여우 vs 호랑이 영상의 주인공입니다. 놀람, 겁, 기쁨 컷에 바로 쓰입니다."),
      "Tiger": (GOOD, "영상의 다른 주인공입니다. 으스댈 때와 놀랄 때의 귀."),
      "RedFox": (MID, "지금 영상에는 나오지 않습니다."),
      "Rabbit": (MID, "지금 영상에는 나오지 않습니다."),
      "Squirrel": (MID, "지금 영상에는 나오지 않습니다."),
      "Dragon": (MID, "지금 영상에는 나오지 않습니다.")}),
    ("스프링을 미리 계산해 클립에 굽기",
     "귀의 출렁임을 미리 계산해 애니메이션 클립에 키로 구워 넣습니다.",
     "Blender나 엔진 스크립트에서 클립마다 스프링을 돌려 귀 뼈 키를 만듭니다.",
     [("Jiggle Physics 애드온", "https://github.com/naelstrof/blender-jiggle-physics"),
      ("커브를 밀어서 따라오게 하기", "https://lesterbanks.com/2016/11/animating-tail-offset-curves/")],
     ["결과가 고정되어 영상 렌더에 안전합니다.", "게임 코드가 필요 없습니다."],
     ["클립마다 구워야 합니다. 여우만 해도 기본 49개에 모션 매칭 158개입니다.", "게임에서 방향 전환이나 피격에 반응하지 않습니다."],
     {"Fox": (MID, "영상용 클립에만 쓸 만합니다. 게임은 실시간 스프링이 낫습니다."),
      "Tiger": (MID, "영상용 클립에만."),
      "RedFox": (BAD, "게임에서만 쓰는 동물이라 실시간 쪽이 낫습니다."),
      "Rabbit": (BAD, "같은 이유."),
      "Squirrel": (BAD, "같은 이유."),
      "Dragon": (BAD, "같은 이유.")}),
    ("귀는 그대로 두고 머리 동작과 털로만",
     "귀는 단단한 채로 두고, 머리의 끄덕임과 기울임, 귀 쪽 털의 바람·끌림을 키워 표현합니다.",
     "털 재질의 바람·끌림 값을 귀 구역에서만 키우고, 서 있을 때의 머리 움직임을 조금 더 줍니다.",
     [("볼트: 크고 선 귀는 표현력을 위한 디자인", "https://claude.ai/artifact/FcLqEZhMNWKJNQiS6UrV1x")],
     ["에셋 작업이 없고 위험이 없습니다.", "지금 바로 됩니다."],
     ["귀 자체는 움직이지 않습니다.", "큰 귀에서는 뻣뻣함이 그대로 남습니다."],
     {"Fox": (BAD, "귀가 커서 뻣뻣함이 눈에 띕니다."),
      "RedFox": (BAD, "귀가 가장 커서 가장 눈에 띕니다."),
      "Rabbit": (BAD, "긴 귀가 막대처럼 보입니다."),
      "Squirrel": (GOOD, "귀가 작아 이 정도로도 어색하지 않습니다. 가장 나중에 해도 됩니다."),
      "Tiger": (GOOD, "귀가 작아 뒤로 미뤄도 티가 덜 납니다."),
      "Dragon": (GOOD, "뿔은 원래 움직이지 않는 부위입니다. 지느러미만 따로 생각하면 됩니다.")}),
]
assert len(TECHS) == 10

# what I would do for each, in order
PLAN = {
    "Fox": "귀마다 뼈 2개를 넣고 스프링 체인과 상태별 포즈를 얹습니다. 꼬리에서 만든 것을 그대로 옮기는 일이라 여기서 먼저 틀을 잡는 것이 맞습니다. 영상에서는 같은 뼈에 키를 넣습니다.",
    "RedFox": "여우에서 잡은 틀을 뼈 3개로 늘려 씁니다. 귀가 커서 효과가 가장 크지만, 귀끼리와 리본이 겹치지 않게 한도를 먼저 정해야 합니다.",
    "Rabbit": "귀마다 뼈 3~4개. 선 귀와 접힌 귀의 기준 자세가 달라 좌우 값을 따로 둡니다. 토끼는 꼬리 뼈도 없으니 같이 넣는 것이 좋습니다.",
    "Squirrel": "귀 끝 털 뭉치는 재질에서 떨게 하고, 귀는 뼈 1개로 쫑긋과 눕힘만 줍니다. 꼬리가 머리 뒤에 붙어 있어 귀를 크게 움직이면 겹칩니다.",
    "Tiger": "뼈 1개로 귀를 눕히고 세우는 포즈만 줍니다. 출렁임은 거의 필요 없습니다. 영상용으로는 같은 뼈에 키를 넣습니다.",
    "Dragon": "뿔은 그대로 둡니다. 옆 지느러미에 뼈 1~2개를 넣어 펼치고 접게 합니다. 털이 없어 작업이 가장 깔끔합니다.",
}


def load(name):
    path = os.path.join(HERE, name)
    return json.load(io.open(path, encoding="utf-8")) if os.path.exists(path) else {}


REFS = {}
REFS.update(load("refs_a.json"))
REFS.update(load("refs_b.json"))
NOTES = [REFS.pop("notes")] if "notes" in REFS else []


def pick(item, *keys):
    for k in keys:
        if item.get(k):
            return str(item[k])
    return ""


def host(url):
    m = re.match(r"https?://([^/]+)", url or "")
    return m.group(1).replace("www.", "") if m else ""


def ref_rows(items):
    out = []
    for i, it in enumerate(items, 1):
        title = pick(it, "title", "work")
        by = " · ".join(x for x in (pick(it, "studio", "developer"), pick(it, "year")) if x)
        who = pick(it, "character", "character_name")
        does = pick(it, "ears_do", "what_ears_do", "what", "ears", "description")
        made = pick(it, "how_made", "how_it_was_made", "made")
        src, vid = pick(it, "source_url", "source"), pick(it, "video_url", "video")
        conf = pick(it, "confidence")
        src_html = f'<a href="{e(src)}">{e(host(src))}</a>' if src.startswith("http") else '<span class="kind">출처 없음</span>'
        vid_html = f'<a class="vid" href="{e(vid)}"><span aria-hidden="true">▶</span> 영상</a>' if vid.startswith("http") else '<span class="kind">영상을 찾지 못했습니다</span>'
        out.append(
            f'        <tr><td class="num">{i}</td><td><b>{e(title)}</b><span class="by">{e(by)}</span><span class="by">{e(who)}</span></td>'
            f'<td>{e(does)}<span class="kind">만든 방법: {e(made or "밝혀진 것 없음")}</span></td>'
            f'<td><span class="pro">장점</span> {e(pick(it, "pros"))}<br><span class="con">단점</span> {e(pick(it, "cons"))}</td>'
            f'<td class="src">{src_html}{vid_html}<span class="kind">확신 {e(conf)}</span></td></tr>')
    return "\n".join(out)


def tech_rows(name):
    out = []
    for i, (title, _what, _how, _src, _pros, _cons, fit) in enumerate(TECHS, 1):
        grade, note = fit[name]
        cls = {GOOD: "good", MID: "mid", BAD: "bad"}[grade]
        out.append(f'        <tr><td class="num">{i}</td><td><b>{e(title)}</b></td><td><span class="grade {cls}">{grade}</span></td><td>{e(note)}</td></tr>')
    return "\n".join(out)


def animal_section(name, korean, key, ears, bones):
    refs = REFS.get(key, [])
    table = f'''    <div class="tablebox">
      <table>
        <thead><tr><th class="num">번호</th><th>작품</th><th>귀가 하는 일</th><th>우리에게 장점과 단점</th><th>출처·영상</th></tr></thead>
        <tbody>
{ref_rows(refs)}
        </tbody>
      </table>
    </div>''' if refs else '    <p class="lead small">참고 작품을 아직 넣지 못했습니다.</p>'
    return f'''  <section id="{name.lower()}">
    <h2>{korean}</h2>
    <img class="now" src="now_{name}.jpg" alt="{korean}의 지금 귀 모습. 비스듬한 앞, 옆, 뒤, 위에서 본 게임 화면">
    <div class="two">
      <div class="box"><h3>지금 귀</h3><p>{e(ears)}</p><p class="small lead">{e(bones)}</p></div>
      <div class="box plan"><h3>이 동물에 권하는 순서</h3><p>{e(PLAN[name])}</p></div>
    </div>
    <h3>참고 작품 {len(refs)}가지</h3>
{table}
    <h3>기술 10가지를 이 동물에 대 보면</h3>
    <div class="tablebox">
      <table>
        <thead><tr><th class="num">번호</th><th>기술</th><th>맞는 정도</th><th>이유</th></tr></thead>
        <tbody>
{tech_rows(name)}
        </tbody>
      </table>
    </div>
  </section>
'''


# What was built (2026-11-25, the user: "다람쥐는 작업에서 제외하고 나머지는 권장하는 순서로 작업해서 적용해줘"): each animal
# friend's game clip (ear_<Name>.mp4, rec_tail.ps1 take Ear_<Name>, cut by ../../ears/clip.sh) and its Blender bend sheet
# (bend_<Name>.jpg, blender_ear_pose.py), with what its ears got.
# name, Korean, what its ears got, the bend sheet's rows
BUILT = [
    ("Fox", "여우", "귀마다 뼈 2개(약 24cm). 스프링 3회/초, 던짐 1, 포즈 1(기준).",
     "위에서부터: 그대로 · 뒤로 45° · 앞으로 20° · 바깥으로 35° · 뒤로 30°와 바깥으로 30°"),
    ("RedFox", "레드 폭스", "귀마다 뼈 3개(약 36cm). 귀가 커서 스프링은 더 무르게(2.3회/초), 던짐 1.2, 포즈는 0.75로 작게.",
     "위에서부터: 그대로 · 뒤로 45° · 앞으로 20° · 바깥으로 35° · 뒤로 30°와 바깥으로 30°"),
    ("Rabbit", "토끼", "선 귀(왼쪽) 뼈 3개, 접힌 귀(오른쪽) 뼈 4개. 접힌 귀는 접힌 모양 그대로를 기준으로 움직입니다. 스프링 2회/초, 던짐 1.3, 포즈 0.9.",
     "위에서부터: 바깥으로 35° · 뒤로 45° · 뒤로 30°와 바깥으로 30° · 앞으로 25°와 바깥으로 20°. 선 귀 끝의 작은 뿔 같은 모양은 원래 모델에 있는 것입니다."),
    ("Tiger", "호랑이", "귀마다 뼈 1개. 출렁임은 작게(스프링 4회/초, 던짐 0.6), 뒤로 눕히는 포즈 위주(포즈 0.8).",
     "위에서부터: 그대로 · 뒤로 50° · 앞으로 25° · 바깥으로 40° · 뒤로 40°와 바깥으로 30°. 바깥으로 크게 벌리면 귀 앞쪽 밑동에 어두운 주름이 보입니다(게임에서는 그만큼 벌리지 않습니다)."),
    ("Dragon", "용", "뿔은 그대로, 머리 옆 지느러미에만 뼈 1개씩. 스프링 3.5회/초, 던짐 0.7, 포즈 0.8. 지느러미가 뒤를 향해 있어 '뒤로'는 머리에 붙게 접히는 쪽입니다.",
     "위에서부터: 뒤로 40° · 앞으로 30° · 바깥으로 40°. 처음에는 지느러미 위의 뿔 밑동까지 같이 움직여서, 지느러미 위쪽을 잘라 내고 다시 만들었습니다."),
]


def built_section():
    cards = []
    for name, korean, got, rows in BUILT:
        cards.append(f'''      <div class="box">
        <h3>{korean}</h3>
        <video src="ear2_{name}.mp4" controls muted loop playsinline preload="metadata" aria-label="{korean}의 귀, 게임 화면"></video>
        <p>{e(got)}</p>
        <details><summary>Blender에서 굽혀 본 모습</summary>
          <img class="now" src="bend_{name}.jpg" alt="{korean}의 귀 뼈를 굽혀 본 모습. 앞, 옆, 뒤">
          <p class="small lead">{e(rows)}</p>
        </details>
      </div>''')
    return f'''  <section id="built">
    <h2>적용 결과: 여우, 레드 폭스, 토끼, 호랑이, 용</h2>
    <p class="lead">권한 순서대로 다섯 동물에 귀 뼈를 넣고(다람쥐는 빼기로 함) 게임에 적용했습니다. 귀마다 뼈 몇 개가 스프링처럼 흔들리고, 상황에 따라 자세를 잡습니다. 출렁임이 약하다고 하셔서 머리 움직임에 따른 출렁임(던짐)을 0.45에서 2로 올려 다시 찍었습니다(같은 걸음에서 예전의 약 4배). 영상은 모두 같은 순서입니다.</p>
    <div class="box plan">
      <h3>영상 순서 (약 32초, 시간은 대략)</h3>
      <ul>
        <li><b>0~5초</b> 서 있기: 가끔 한쪽 귀가 까딱합니다(2.5~6초마다 아무 쪽이나).</li>
        <li><b>5~9초</b> 걸어오기, <b>9~13초</b> 달려서 돌아가기: 빠를수록 귀가 뒤로 젖혀지고(달릴 때 30°), 멈추면 앞으로 출렁입니다.</li>
        <li><b>13~14초</b> 점프 · <b>16~17초</b> 사격 4발: 조준하는 동안 귀가 앞으로 쫑긋(12°), 쏠 때마다 움찔.</li>
        <li><b>19초, 21초</b> 맞기(체력 15씩): 귀가 뒤로 납작하게(55°, 조금 바깥으로) 눕고 1초쯤 걸려 돌아옵니다.</li>
        <li><b>23~31초</b> 옆에서 보며 걷고 달리기 · <b>31초</b> 쓰러짐: 귀가 처지며 바깥으로 늘어집니다.</li>
      </ul>
    </div>
    <div class="two">
{chr(10).join(cards)}
    </div>
    <div class="box plan" id="rush">
      <h3>달리기와 탈것: 여우·레드 폭스·토끼 귀를 더 크게 뒤로</h3>
      <p>"뛰어갈 때 좀 더 드라마틱하게, 탈것 속도에서도 비슷하게"에 맞춰, 걷는 속도(초당 300)를 넘는 만큼 귀가 바람에 밀려 더 젖혀지게 했습니다. 질주(초당 약 510)에서 그 몫의 약 60%, 탈것(초당 1000 이상)에서는 거의 전부이고, 빠를수록 바람에 파르르 떱니다. 탈것에서는 운전하는 쪽의 속도를 씁니다(뒷자리에 타도 같음).</p>
      <ul>
        <li><b>더하는 각도</b>: 여우·레드 폭스 50°, 토끼 55°, 호랑이 15°, 용 20°. 여우로 보면 질주 27° → 약 59°, 오토바이 약 75~84°(거의 눕힌 정도).</li>
        <li><b>영상 순서</b>(약 29초): 옆에서 걷기 → 질주로 돌아가기 → 앞에서 질주 → 오토바이 타고 옆에서 왕복 → 앞에서 달려옴. 위 다섯 영상은 이 변경 전에 찍은 것이라 달리기 장면의 귀가 덜 젖혀져 있습니다.</li>
        <li><b>조절</b>: <code>rr.Ear.Rush 1.5</code> 더 크게, <code>rr.Ear.Rush 0.5</code> 작게(지금 1).</li>
      </ul>
      <div class="three">
        <video src="rush_Fox.mp4" controls muted loop playsinline preload="metadata" aria-label="여우, 달리기와 오토바이"></video>
        <video src="rush_RedFox.mp4" controls muted loop playsinline preload="metadata" aria-label="레드 폭스, 달리기와 오토바이"></video>
        <video src="rush_Rabbit.mp4" controls muted loop playsinline preload="metadata" aria-label="토끼, 달리기와 오토바이"></video>
      </div>
      <p class="small lead">왼쪽부터 여우, 레드 폭스, 토끼. 연습용 맵에는 원래 일반 탈것이 없어서, 촬영용 실행 옵션(<code>-RRStudioVehicles</code>)으로 오토바이를 세워 두고 찍었습니다.</p>
    </div>
    <div class="box plan" id="stiff">
      <h3>여우 귀를 단단하게 ("여우 귀만 너무 젤리 같은데")</h3>
      <p>여우 귀는 뼈 두 개가 굽힘을 나눠 갖고 끝 쪽이 더 많이 휘어서, 귀 전체가 휘청거리며 출렁였습니다. 여우 귀는 단단한 연골이라 휘지 않고 밑동에서 돌아야 맞습니다. 그래서 여우만 굽힘의 85%를 밑동 뼈가 맡게 하고(귀는 곧은 삼각형 그대로 기울어짐), 감쇠를 0.3에서 0.55로 올려 빨리 멈추게 했습니다. 스프링도 3에서 3.5회/초로 조금 단단하게. 같은 장면에서 프레임마다 흔들리는 양이 약 37% 줄었습니다. 다른 동물은 그대로입니다.</p>
      <div class="two">
        <video src="stiff_fox_1.mp4" controls muted loop playsinline preload="metadata" aria-label="여우 귀 전후 비교, 기본 동작"></video>
        <video src="stiff_fox_2.mp4" controls muted loop playsinline preload="metadata" aria-label="여우 귀 전후 비교, 달리기와 오토바이"></video>
      </div>
      <p class="small lead">두 영상 모두 왼쪽이 전, 오른쪽이 후입니다. 왼쪽 영상은 기본 동작(서기·걷기·질주·점프·사격·맞기), 오른쪽 영상은 달리기와 오토바이. 위 "달리기와 탈것"의 여우 영상도 바꾼 뒤의 것으로 교체했습니다. 콘솔 <code>rr.Ear.Rigid 0~1</code>(1이면 완전히 곧게), <code>rr.Ear.Damping</code>(높을수록 빨리 멈춤)으로 바로 바꿔 볼 수 있습니다.</p>
    </div>
    <div class="box">
      <h3>만든 방식과 조절</h3>
      <ul>
        <li><b>뼈</b>: Blender에서 귀의 가운데 선을 따라 뼈를 놓고(머리 뼈 아래), 귀 아래쪽 30%는 머리와 무게를 나눠 밑동이 부드럽게 굽게 했습니다. 털은 다시 만들지 않고 새 무게만 옮겨, 모양과 색은 전과 똑같습니다. 애니메이션과 모션 매칭은 건드리지 않았습니다.</li>
        <li><b>움직임</b>: 꼬리와 같은 뼈별 스프링입니다. 머리가 흔들리고 돌고 착지할 때 귀 끝이 늦게 따라오고, 그 위에 달리기·조준·사격·맞기·쓰러짐·까딱임 자세가 얹힙니다.</li>
        <li><b>콘솔로 바로 바꿔 보기</b>(다시 빌드할 필요 없음): <code>rr.Ear 0</code> 끄기 · <code>rr.Ear.Throw 3</code> 더 크게, <code>rr.Ear.Throw 1</code> 더 작게 출렁임(지금 2) · <code>rr.Ear.Pose 1.5</code> 자세를 더 크게 · <code>rr.Ear.Hz 2</code> 더 무르게 · <code>rr.Ear.Back 40</code> / <code>rr.Ear.Out 30</code> 그 자세로 고정해 보기(-999 = 해제).</li>
        <li><b>아직 안 한 것</b>: 영상(시네마틱)에서 귀에 키 넣기, 다람쥐, 귀끼리·리본과의 겹침 한도(레드 폭스에서 아직 겹치는 장면은 못 봤습니다).</li>
      </ul>
    </div>
  </section>
'''


# (2026-11-25, the user: "호랑이 꼬리도 여우 꼬리처럼 다시 작업해줘 ... 토끼 꼬리는 더 동글동글 하고 풍성하게"; asked: tiger by
# Tripo, rabbit in Blender)
TAILS = '''  <section id="tails">
    <h2>꼬리 다시: 호랑이는 곧은 꼬리로 새로 뽑고, 토끼는 둥글고 풍성하게</h2>
    <div class="two">
      <div class="box plan">
        <h3>호랑이: 여우와 같은 방식</h3>
        <ul>
          <li><b>그림</b>: 3D에 넣었던 호랑이 그림 세 장(정면·왼쪽·뒤)에서 꼬리만 몸 뒤로 똑바로 다시 그렸습니다(Nano Banana Pro, 무료 14장, 600 → 614/1000). 첫 옆모습은 꼬리가 짧아 "키의 2/3쯤, 손목 굵기"로 다시, 앞·뒤는 고친 옆모습을 참고로 넣어 맞췄습니다.</li>
          <li><b>3D</b>: Smart Mesh 멀티뷰 100 + 텍스처 30 = 130 크레딧(19,620 → 19,490). 20,279면.</li>
          <li><b>옛 뼈대 그대로</b>: 새 모델을 옛 호랑이 몸에 맞춰(평균 1.1cm) 무게를 옮기고 꼬리 뼈를 4개에서 8개로. 애니메이션·모션 매칭은 그대로입니다. 새 꼬리가 47cm·반지름 4.3cm로 작게 나와(옛 것은 말린 선으로 85cm, 6~8cm) 1.6배 길게, 끝 쪽 1.5배 굵게 → 73cm.</li>
          <li><b>얼굴</b>: 지난번 순서 그대로 — 아래 얼굴 모양을 지금 호랑이에 맞춰 감싸고, 눈·눈썹·코·입·흰 주둥이를 지금 호랑이에서 옮기고, 수염 8가닥, 주둥이 파츠, 귀 뼈, 털(꼬리 전체 포함).</li>
          <li><b>서 있을 때</b>: 네 가지를 찍어 보고, 뿌리는 낮게 두고 끝이 위로 말려 올라가는 것(컨셉 그림과 같은 모양)을 골랐습니다. 움직이면 다시 곧게 펴집니다.</li>
        </ul>
      </div>
      <div class="box plan">
        <h3>토끼: 크레딧 없이 Blender에서</h3>
        <ul>
          <li>솜꼬리 표면을 지름 약 18cm의 둥근 공 위로 옮겼습니다(예전 것은 끝이 뾰족한 달걀 모양, 약 13cm). 몸에 붙는 자리는 그대로 두고, 공 표면은 고르게 펴서 주름이 없게 했습니다.</li>
          <li>털은 꼬리 공에서만 2.3배 길게 다시 길렀습니다. 털 재질과 그 조절값(크림색 밝기 등)은 그대로입니다.</li>
          <li>텍스처·애니메이션·귀는 그대로입니다.</li>
        </ul>
      </div>
    </div>
    <h3>호랑이: 전(왼쪽)과 후(오른쪽), 같은 동작</h3>
    <div class="two">
      <video src="tiger_tail_1.mp4" controls muted loop playsinline preload="metadata" aria-label="호랑이 꼬리 전후, 옆에서"></video>
      <video src="tiger_tail_2.mp4" controls muted loop playsinline preload="metadata" aria-label="호랑이 꼬리 전후, 위 뒤에서"></video>
    </div>
    <p class="small lead">왼쪽 영상은 옆에서, 오른쪽 영상은 위 뒤에서. 걷기 → 질주 → 제자리 점프 → 달리며 점프 → 사격. 옛 꼬리는 처음부터 S자로 말린 채 통째로 흔들렸고, 새 꼬리는 몸 뒤로 곧게 나와 뼈 8개가 나눠 휩니다.</p>
    <div class="two">
      <figure><img class="now" src="tiger_model.jpg" alt="옛 호랑이(위)와 새 호랑이(아래)의 옆과 뒤, Blender"><figcaption class="small lead">위: 옛 모델, 아래: 새 모델(꼬리를 늘리고 굵힌 뒤).</figcaption></figure>
      <figure><img class="now" src="tiger_face.jpg" alt="지금 호랑이(위)와 새 호랑이(아래)의 얼굴 앞, 비스듬히, 옆"><figcaption class="small lead">얼굴: 위가 지금 게임의 호랑이, 아래가 새 모델에 옮긴 것.</figcaption></figure>
    </div>
    <div class="two">
      <figure><img class="now" src="tiger_idle.jpg" alt="서 있을 때 꼬리 자세 네 가지, 옆과 뒤"><figcaption class="small lead">서 있을 때 꼬리 자세 후보(왼쪽부터 곧게 · 여우와 같은 값 · <b>고른 것</b>: 끝이 위로 말림 · 옆으로 낮게 말림), 위는 옆, 아래는 뒤에서.</figcaption></figure>
      <figure><img class="now" src="tiger_inputs.jpg" alt="3D에 넣은 새 그림 세 장: 정면, 왼쪽, 뒤"><figcaption class="small lead">3D에 넣은 새 그림(정면은 꼬리가 몸 뒤에 숨어 안 보임, 뒤는 꼬리 끝이 정면으로 보임).</figcaption></figure>
    </div>
    <h3>토끼</h3>
    <div class="two">
      <video src="rabbit_tail.mp4" controls muted loop playsinline preload="metadata" aria-label="토끼의 새 꼬리, 게임"></video>
      <figure><img class="now" src="rabbit_tail.jpg" alt="토끼 꼬리 전(위)과 후(아래), 털 포함"><figcaption class="small lead">위: 예전 꼬리, 아래: 새 꼬리(털 포함, 옆 · 뒤 · 비스듬히).</figcaption></figure>
    </div>
    <div class="box plan" id="tigercat">
      <h3>호랑이 꼬리: 일정한 두께 + 고양이처럼 ("점점 두꺼워지는게 아니라 일정한 두께로 ... 고양이 꼬리처럼")</h3>
      <ul>
        <li><b>두께</b>: 꼬리 뼈를 따라 14토막으로 나눠 토막마다 반지름을 4.8cm로 맞췄습니다(전에는 뿌리 3.7cm에서 끝 쪽 6.6cm로 굵어짐, 이제 4.6~5.2cm). 몸에 붙는 첫 12%는 서서히 맞추고, 끝의 둥근 마무리는 그대로입니다. 얼굴·텍스처는 그대로이고 털만 다시 길렀습니다.</li>
        <li><b>고양이 같은 움직임</b>(호랑이만, 다른 동물은 그대로):
          <ul>
            <li>흔들림이 뿌리에서 끝으로 물결처럼 전해집니다(지연 2배). 굽힘은 끝 쪽이 더 많이 맡습니다(여우는 뿌리 쪽).</li>
            <li>더 넓게(1.4배), 더 느리게(0.7배) 흔들고, 끝은 더 무르게(초당 0.55회) 해서 채찍처럼 뒤따라옵니다.</li>
            <li>걸을 때는 꼬리를 들고(뿌리 22°) 끝을 갈고리처럼 말아 올립니다(55°). 질주하면 그 4분의 1만 들어 뒤로 눕습니다. 서 있을 때의 끝이 위로 말린 자세는 그대로입니다.</li>
          </ul></li>
      </ul>
      <div class="two">
        <video src="tiger_cat_1.mp4" controls muted loop playsinline preload="metadata" aria-label="호랑이 꼬리 전후, 옆에서"></video>
        <video src="tiger_cat_2.mp4" controls muted loop playsinline preload="metadata" aria-label="호랑이 꼬리 전후, 위 뒤에서"></video>
      </div>
      <p class="small lead">각 영상의 왼쪽 반이 전(끝이 굵고 여우처럼 움직임), 오른쪽 반이 후. 왼쪽 영상은 옆에서, 오른쪽 영상은 위 뒤에서: 걷기 → 질주 → 제자리 점프 → 달리며 점프 → 사격.</p>
      <h3>뿌리 굵게 ("여전히 뿌리가 너무 얇아")</h3>
      <p>원인: 꼬리가 몸에서 나오는 첫 고리(뿌리에서 6cm)는 꼬리 뼈에 34%만 붙어 있어서 두께 맞추기에서 빠졌고, 반바지에서 나오는 둘레는 몸(엉덩이) 쪽 면이라 손대지 않았습니다. 그래서 3.5cm 목에서 5cm로 넓어지는 병목 모양이었습니다. 고친 것: 꼬리에 조금이라도 붙은 면은 붙은 만큼 맞추고, 뿌리는 1.25배 굵게(6cm, 꼬리의 35%에 걸쳐 4.8cm로), 반바지에서 꼬리가 나오는 둘레도 뿌리 굵기까지 넓혀 몸에서 자연스럽게 퍼져 나오게 했습니다.</p>
      <div class="two">
        <video src="tiger_root.mp4" controls muted loop playsinline preload="metadata" aria-label="굵어진 호랑이 꼬리 뿌리, 게임"></video>
        <figure><img class="now" src="tiger_root.jpg" alt="호랑이 꼬리 뿌리, 전(위)과 후(아래)"><figcaption class="small lead">위: 전(뿌리가 가늘게 잘록), 아래: 후. 옆 · 뒤 · 비스듬히(털 포함, Blender).</figcaption></figure>
      </div>
      <figure><img class="now" src="tiger_even.jpg" alt="호랑이 꼬리 두께, 전(위)과 후(아래)"><figcaption class="small lead">위: 전(끝으로 갈수록 굵음), 아래: 후(일정한 두께).</figcaption></figure>
    </div>
    <div class="box plan" id="tigersplit">
      <h3>걸을 땐 묵직하게, 방향 틀 때만 격하게 ("걸을 땐 묵직하게, 방향 틀 때만 격하게 나눠줘")</h3>
      <p>몸이 도는 속도를 매 프레임 재서, 빨리 돌 때만 꼬리를 격한 값으로 바꿉니다.</p>
      <ul>
        <li><b>걸을 때·서 있을 때</b>: 감쇠 0.65(Stray식 묵직함), 체인 효과 0.5(살짝만 늦게 따라옴).</li>
        <li><b>방향 틀 때</b>: 몸이 초당 60°보다 빨리 돌기 시작하면 바뀌기 시작해, 초당 240°에서 완전히 격한 값(감쇠 0.35, 체인 1.7)이 됩니다. 도는 순간 바로 올라가고, 다 돈 뒤에는 약 1초에 걸쳐 묵직한 값으로 돌아와서 휙 넘어가는 동작이 끝까지 보입니다.</li>
        <li>게임 로그로 확인: 곧게 걸을 땐 0.65 / 0.5, 지그재그에서 꺾을 때마다 0.35 / 1.7로 갔다가 돌아옵니다.</li>
      </ul>
      <div class="two">
        <video src="tiger_split_1.mp4" controls muted loop playsinline preload="metadata" aria-label="호랑이 꼬리, 늘 격한 판과 나눈 판, 옆에서"></video>
        <video src="tiger_split_2.mp4" controls muted loop playsinline preload="metadata" aria-label="호랑이 꼬리, 늘 격한 판과 나눈 판, 위 뒤에서"></video>
      </div>
      <p class="small lead">각 영상의 왼쪽 반이 바로 전 판(늘 격함), 오른쪽 반이 나눈 판. 달리기 → 지그재그 넷 → U턴 → 다시 U턴 → 질주.</p>
      <figure><img class="now" src="tiger_split.jpg" alt="위 줄 늘 격한 판, 아래 줄 나눈 판: 곧게 걷기 셋, U턴 둘, 질주"><figcaption class="small lead">위 줄 바로 전 판, 아래 줄 나눈 판. 앞 셋은 곧게 걷기(아래가 고르게 묵직), 다음 둘은 U턴, 마지막은 질주.</figcaption></figure>
    </div>
    <div class="box plan" id="tigerchain2">
      <h3>호랑이 꼬리 체인 더 격하게 ("조금 더 격하게 반응했으면 해")</h3>
      <p>세 가지 세기를 게임에서 같은 움직임으로 찍어 비교했고, 가장 센 것을 골랐습니다.</p>
      <ul>
        <li><b>붙잡는 양</b>: 끝 마디가 원래 방향을 85% 붙잡던 것을 170%로 올렸습니다. 1을 넘으면 몸이 돈 것보다 더 크게 반대로 남았다가, 돌아오면서 반대편으로 휙 넘어갑니다.</li>
        <li><b>튕김</b>: 꼬리 스프링 감쇠를 0.65 → 0.35로 낮췄습니다. 이제 한 번에 멈추지 않고 한두 번 출렁이며 자리를 잡습니다.</li>
        <li><b>퍼짐</b>: 효과가 끝에만 몰리던 것(1.5)을 조금 더 가운데까지 퍼지게 했습니다(1.2).</li>
        <li>비교한 것: 0.85/0.65(바로 전 판), 1.3/0.45, 1.7/0.35. U턴, 지그재그, 질주 장면에서 꼬리가 꺾이거나 깨지는 곳은 없었습니다.</li>
        <li>감쇠를 낮췄기 때문에 걸을 때와 서 있을 때의 흔들림도 조금 더 살아 있습니다. Stray식 무게감은 그만큼 줄었습니다.</li>
      </ul>
      <div class="two">
        <video src="tiger_chain2_1.mp4" controls muted loop playsinline preload="metadata" aria-label="호랑이 꼬리, 바로 전 체인과 더 격한 체인, 옆에서"></video>
        <video src="tiger_chain2_2.mp4" controls muted loop playsinline preload="metadata" aria-label="호랑이 꼬리, 바로 전 체인과 더 격한 체인, 위 뒤에서"></video>
      </div>
      <p class="small lead">각 영상의 왼쪽 반이 바로 전 판(0.85/0.65), 오른쪽 반이 새 판(1.7/0.35, 기본값 빌드로 찍음). 달리기 → 지그재그 넷 → U턴 → 다시 U턴 → 질주.</p>
      <figure><img class="now" src="tiger_chain2.jpg" alt="새 판의 U턴 장면 여덟 컷"><figcaption class="small lead">새 판의 U턴: 꼬리가 크게 남았다가 휘어 넘어오며 따라옵니다.</figcaption></figure>
    </div>
    <div class="box plan" id="tigerchain">
      <h3>호랑이 꼬리 체인처럼 따라오게 ("꼬리가 너무 막대기 같은데 뛰거나 방향전환을 할 때 체인같은 느낌을 조금 더")</h3>
      <p>지금까지는 몸이 돌면 꼬리 전체가 한꺼번에 휘었다가 돌아와서 막대처럼 보였습니다. 이제 엉덩이가 돌거나 기울거나 오르내릴 때, 꼬리 마디마다 원래 가리키던 방향을 조금씩 붙잡게 했습니다. 뿌리는 거의 바로 따라가고, 끝으로 갈수록 더 오래 남아 있습니다.</p>
      <ul>
        <li><b>언제</b>: 방향을 틀 때, 달릴 때 걸음마다 엉덩이가 흔들리고 오르내릴 때, 멈추거나 출발할 때. 그 휨이 스프링을 따라 끝 쪽으로 한 박자 늦게 풀립니다.</li>
        <li><b>세기</b>: 끝 마디는 원래 방향의 85%를 붙잡습니다(뿌리 쪽은 적게, 끝 쪽일수록 많이). 탈것을 탈 때와 쓰러졌을 때는 꺼집니다.</li>
        <li>새 값은 동물마다 정합니다. 지금은 호랑이만 켰습니다. 무게감, 꼬불거림, 늘어뜨린 자세, 바닥 처리는 그대로입니다.</li>
      </ul>
      <div class="two">
        <video src="tiger_chain_1.mp4" controls muted loop playsinline preload="metadata" aria-label="호랑이 꼬리, 체인 효과 끔과 켬, 옆에서 지그재그와 U턴, 질주"></video>
        <video src="tiger_chain_2.mp4" controls muted loop playsinline preload="metadata" aria-label="호랑이 꼬리, 체인 효과 끔과 켬, 위 뒤에서 지그재그와 U턴, 질주"></video>
      </div>
      <p class="small lead">각 영상의 왼쪽 반이 끔(바로 전 판), 오른쪽 반이 켬. 둘 다 같은 움직임: 달리기 → 지그재그 넷 → U턴 → 다시 U턴 → 질주. 왼쪽은 옆에서, 오른쪽은 위 뒤에서.</p>
      <figure><img class="now" src="tiger_chain.jpg" alt="위에서 본 지그재그, 위 줄 끔, 아래 줄 켬"><figcaption class="small lead">위에서 본 지그재그와 U턴: 위 줄 끔(곧은 막대), 아래 줄 켬(끝이 늦게 따라와 휨).</figcaption></figure>
    </div>
    <div class="box plan" id="tigerrootfit">
      <h3>호랑이 꼬리 뿌리 굵기 맞추기 ("호랑이 꼬리 뿌리 부분이 또 전체적인 굵기하고 맞지 않는데 맞쳐줘")</h3>
      <p><b>원인</b>: 꼬리를 70%로 줄일 때 게임은 꼬리 첫 뼈의 크기를 줄입니다. 그런데 뿌리 쪽 고리들은 엉덩이 뼈에도 반쯤 붙어 있어서 덜 줄었고, 전에 뿌리를 굵게 해 둔 것(6cm)도 그대로 남았습니다. 게임 화면 기준 굵기가 뿌리부터 6.0 → 4.9 → 3.8 → 3.4cm로 나팔처럼 퍼져 있었습니다.</p>
      <ul>
        <li><b>고침</b>: 뿌리 쪽 고리마다 게임에서 얼마나 줄어드는지(꼬리 뼈에 붙은 비율만큼) 미리 셈해서, 모델에서는 그만큼 크게 두어 게임에서 같은 굵기가 나오게 했습니다. 새 스크립트 <code>blender_tail_game_fit.py</code>.</li>
        <li><b>결과</b>(게임 기준 반지름): 뿌리 3.7 → 3.7 → 3.6 → 3.4cm, 그 뒤 3.4cm로 일정. 뿌리가 아주 조금만 더 굵어 몸에 자연스럽게 붙습니다.</li>
        <li>털은 새 모델에서 다시 키웠고, 메시와 털을 언리얼에 다시 넣었습니다(뼈 위치 변화 0). 이전 에셋은 백업해 두었습니다.</li>
      </ul>
      <div class="two">
        <video src="tiger_rootfit_1.mp4" controls muted loop playsinline preload="metadata" aria-label="호랑이 꼬리 뿌리, 고치기 전과 후, 옆에서"></video>
        <video src="tiger_rootfit_2.mp4" controls muted loop playsinline preload="metadata" aria-label="호랑이 꼬리 뿌리, 고치기 전과 후, 위 뒤에서"></video>
      </div>
      <p class="small lead">각 영상의 왼쪽 반이 고치기 전, 오른쪽 반이 후. 왼쪽은 옆에서, 오른쪽은 위 뒤에서.</p>
      <figure><img class="now" src="tiger_rootfit.jpg" alt="호랑이 꼬리 뿌리 가까이, 위 줄 전, 아래 줄 후"><figcaption class="small lead">가까이: 위 줄 전(뿌리가 나팔처럼 굵음), 아래 줄 후(같은 굵기). 걷기 둘, 서 있기 하나.</figcaption></figure>
    </div>
    <div class="box plan" id="tigerhang">
      <h3>호랑이 꼬리 작게, 늘어뜨리고, 바닥 안 뚫게 ("30% 정도 두께와 길이를 줄여주고 … 바닥쪽으로 늘어뜨리고 끝만 살짝 … 바닥을 뚫고 안 보이는 부분은 없게")</h3>
      <ul>
        <li><b>크기</b>: 꼬리 길이와 두께를 함께 70%로. 꼬리 첫 뼈의 크기를 줄여서, 뒤 뼈들과 털도 같이 줄었습니다(모델 파일은 그대로). 엉덩이에 이어진 뿌리 부분은 몸 쪽에서 자연스럽게 가늘어집니다.</li>
        <li><b>늘어뜨리기</b>: 서 있을 때 뿌리부터 바닥 쪽으로 내리고(-40°) 끝만 들어 올립니다(48°). 옆으로도 조금 말립니다(25°). 걸을 때는 몸이 앞으로 기울어 꼬리가 들려 보여서 더 많이 내렸습니다(-60° → -95°, 끝 50°).</li>
        <li><b>바닥</b>: 꼬리 뿌리에서 아래로 바닥을 찾아, 꼬리 마디 하나라도 바닥에 너무 가까워지면 그 앞 마디를 딱 필요한 만큼 들어 올립니다. 그래서 바닥에 닿으면 바닥 위에 얹혀 눕고 끝만 떠 있습니다. 경사면도 바닥의 기울기를 따릅니다. 이 처리는 꼬리가 있는 다른 동물친구에게도 걸립니다.</li>
        <li>위아래 꼬불거림은 바닥을 치지 않게 반으로(14° → 7°), 옆 꼬불거림(9°)은 그대로.</li>
      </ul>
      <div class="two">
        <video src="tiger_hang_1.mp4" controls muted loop playsinline preload="metadata" aria-label="호랑이 꼬리, 바로 전 판과 작게 늘어뜨린 판, 옆에서"></video>
        <video src="tiger_hang_2.mp4" controls muted loop playsinline preload="metadata" aria-label="호랑이 꼬리, 바로 전 판과 작게 늘어뜨린 판, 위 뒤에서"></video>
      </div>
      <p class="small lead">각 영상의 왼쪽 반이 바로 전 판(꼬불거리는 판), 오른쪽 반이 새 판. 왼쪽은 옆에서, 오른쪽은 위 뒤에서.</p>
      <figure><img class="now" src="tiger_hang.jpg" alt="작아진 호랑이 꼬리가 바닥 위로 늘어진 모습, 걷기 셋과 서 있기"><figcaption class="small lead">가까이: 걷기 셋(늘어지고 끝만 들림), 서 있기(오른쪽 아래: 바닥 위에 얹히고 끝만 뜸).</figcaption></figure>
    </div>
    <div class="box plan" id="tigersquiggle">
      <h3>호랑이 꼬리 꼬불거리게 ("조금 더 꼬불거리는 모습도 보였으면 좋겠어. 지금은 너무 스틱 같아")</h3>
      <p>Stray식 판은 꼬리 전체가 한 덩어리로 휘어서 막대처럼 보였습니다. 그래서 꼬리 뼈 8개가 서로 반대로 조금씩 꺾이는 S자 물결을 넣고, 이 물결이 뿌리에서 끝으로 천천히 흘러가게 했습니다.</p>
      <ul>
        <li><b>S자 물결</b>: 뼈마다 위아래로 최대 14°, 옆으로 9°(끝 쪽이 더 크게). 꼬리 길이에 물결 하나 조금 넘게(1.15개) 들어가고, 초당 0.4번(뛰면 더 빨리) 끝 쪽으로 흘러갑니다. 옆 물결은 위아래보다 4분의 1박자 늦어 입체로 꼬불거립니다.</li>
        <li><b>언제</b>: 걸을 때 가장 크고, 서 있을 땐 0.6배, 질주 땐 줄고, 탈것·쓰러짐에선 없습니다.</li>
        <li>처음엔 9°/6°/물결 1개로 넣었는데 바로 전 판과 거의 구분이 안 돼서 키웠습니다.</li>
        <li>Stray식 무게감(낮게 늘어짐, 느리고 매끄러운 흔들림)은 그대로입니다. 호랑이만 바뀌었습니다.</li>
      </ul>
      <div class="two">
        <video src="tiger_squiggle_1.mp4" controls muted loop playsinline preload="metadata" aria-label="호랑이 꼬리, Stray식과 꼬불거리는 판, 옆에서"></video>
        <video src="tiger_squiggle_2.mp4" controls muted loop playsinline preload="metadata" aria-label="호랑이 꼬리, Stray식과 꼬불거리는 판, 위 뒤에서"></video>
      </div>
      <p class="small lead">각 영상의 왼쪽 반이 바로 전 판(Stray식), 오른쪽 반이 꼬불거리는 판. 왼쪽은 옆에서, 오른쪽은 위 뒤에서.</p>
      <figure><img class="now" src="tiger_squiggle.jpg" alt="꼬불거리는 호랑이 꼬리, 걷는 중 S자"><figcaption class="small lead">걷는 중: 뿌리는 낮게, 가운데가 한 번 꺾이고 끝이 올라가는 S자.</figcaption></figure>
    </div>
    <div class="box plan" id="tigerstray">
      <h3>호랑이 꼬리를 Stray 고양이처럼 ("Stray 고양이 꼬리처럼 호랑이 꼬리 움직임 개선해줘")</h3>
      <p>Stray 게임플레이 트레일러에서 고양이 꼬리를 보면, 걸을 때는 무겁고 낮게 뿌리에서 살짝 처졌다가 끝이 부드럽게 올라가고, 천천히 매끄럽게 흔들리며 튕김이 거의 없습니다. 멈춰서 무언가를 볼 때는 꼬리를 세우고 끝을 맙니다. 바로 전 판은 걸을 때 꼬리를 높이 들고 끝을 갈고리처럼 말았고, 넓고 채찍처럼 흔들려서 이 느낌과 달랐습니다.</p>
      <ul>
        <li><b>걸을 때</b>: 들어 올리던 것(뿌리 22°, 끝 55°)을 뿌리는 조금 처지고(-8°) 끝만 완만하게 올라가게(45°).</li>
        <li><b>흔들림</b>: 폭은 보통(1.4배 → 1배), 속도는 더 느리게(0.7배 → 0.55배). 새로 동물별 감쇠를 넣어 호랑이만 0.3 → 0.65로, 튕기지 않고 매끄럽게 멈춥니다. 끝은 덜 무르게(초당 0.55 → 0.75회).</li>
        <li><b>서 있을 때</b>: 조금 더 세우고(뿌리 18°) 끝을 맙니다(90°).</li>
        <li>물결처럼 전해지는 것, 끝 쪽이 더 휘는 것, 질주 때 낮게 눕는 것은 그대로입니다.</li>
      </ul>
      <div class="two">
        <video src="tiger_stray_1.mp4" controls muted loop playsinline preload="metadata" aria-label="호랑이 꼬리, 바로 전 판과 Stray식, 옆에서"></video>
        <video src="tiger_stray_2.mp4" controls muted loop playsinline preload="metadata" aria-label="호랑이 꼬리, 바로 전 판과 Stray식, 위 뒤에서"></video>
      </div>
      <p class="small lead">각 영상의 왼쪽 반이 바로 전 판(높이 든 갈고리), 오른쪽 반이 Stray식. 왼쪽은 옆에서, 오른쪽은 위 뒤에서.</p>
      <figure><img class="now" src="tiger_stray.jpg" alt="Stray식 호랑이 꼬리, 서기 걷기 질주 점프 사격"><figcaption class="small lead">Stray식: 서 있기 · 걷기 둘 · 질주 둘 · 점프 · 사격(옆), 위 뒤에서 달리기.</figcaption></figure>
    </div>
    <div class="box plan" id="rabbitbone">
      <h3>토끼 꼬리가 뛸 때 찌그러지던 것 ("토끼 뛸 때 꼬리가 이상하게 변형되는데 왜 그런지")</h3>
      <p><b>원인</b>: 토끼에는 꼬리 뼈가 없어서, 솜꼬리가 몸의 뼈 세 개(등 위쪽 53%, 등 25%, 엉덩이 22%)에 나눠 붙어 있었습니다. 달릴 때 등이 앞으로 숙여지고 비틀리면 공의 위쪽은 등을, 아래쪽은 엉덩이를 따라가서 공이 길쭉하게 늘어나고 기울었습니다. 예전 작은 꼬리에도 같은 문제가 있었는데, 공을 키우면서 잘 보이게 됐습니다.</p>
      <p><b>고친 것</b>: 공의 밑동에 꼬리 뼈 하나(엉덩이 아래)를 넣고 공 전체를 그 뼈에 붙였습니다(몸에 닿는 2cm만 몸과 나눔). 그래서 공은 모양을 그대로 유지한 채 통째로만 움직입니다. 흔들림은 꼬리 스프링이 맡는데, 토끼는 다른 동물의 30%만 움직이고 스프링을 단단하게(초당 2.5회) 해서 통통 살짝 흔들리게 했습니다. 털도 같은 뼈를 따라갑니다.</p>
      <div class="two">
        <video src="rabbit_tail_bone.mp4" controls muted loop playsinline preload="metadata" aria-label="토끼 꼬리 전후, 뒤에서 걷기 질주 점프"></video>
        <video src="rabbit_tail_side.mp4" controls muted loop playsinline preload="metadata" aria-label="토끼 꼬리, 옆에서 걷기 질주 점프"></video>
      </div>
      <p class="small lead">왼쪽: 뒤에서, 왼쪽 반이 전, 오른쪽 반이 후(걷기 → 질주 → 점프). 오른쪽: 바꾼 뒤를 옆에서.</p>
      <figure><img class="now" src="rabbit_bend.jpg" alt="등을 30도 숙였을 때 꼬리 모양, 전과 후"><figcaption class="small lead">Blender에서 등을 30° 숙여 본 것: 왼쪽 예전(길쭉해짐), 가운데 꼬리 뼈를 넣은 뒤(둥근 그대로), 오른쪽 꼬리 뼈로 12° 흔든 모습. 위는 옆, 아래는 뒤.</figcaption></figure>
    </div>
    <p class="small lead">남은 것: 호랑이 시네마틱(여우 vs 호랑이 영상)은 옛 호랑이 기준의 표정 주둥이 파츠 3개·영상용 털을 쓰고 있어, 새 모델로 다시 만들어야 합니다(여우도 같은 상태). 초상화는 예전 그림 그대로입니다.</p>
  </section>
'''


def tech_cards():
    out = []
    for i, (title, what, how, srcs, pros, cons, _fit) in enumerate(TECHS, 1):
        links = " · ".join(f'<a href="{e(u)}">{e(t)}</a>' for t, u in srcs)
        out.append(f'''      <div class="box">
        <h3><span class="tnum">{i}</span> {e(title)}</h3>
        <p>{e(what)}</p>
        <p class="small lead">우리 프로젝트에서는: {e(how)}</p>
        <div class="pc"><div><span class="pro">장점</span><ul>{"".join(f"<li>{e(p)}</li>" for p in pros)}</ul></div>
          <div><span class="con">단점</span><ul>{"".join(f"<li>{e(c)}</li>" for c in cons)}</ul></div></div>
        <p class="small lead">자료: {links}</p>
      </div>''')
    return "\n".join(out)


def matrix():
    head = "".join(f"<th>{k}</th>" for _n, k, *_ in ANIMALS)
    rows = []
    for i, (title, *_rest, fit) in enumerate(TECHS, 1):
        cells = "".join('<td><span class="grade %s">%s</span></td>' % ({GOOD: "good", MID: "mid", BAD: "bad"}[fit[n][0]], fit[n][0]) for n, *_ in ANIMALS)
        rows.append(f'        <tr><td class="num">{i}</td><td><b>{e(title)}</b></td>{cells}</tr>')
    return f'''    <div class="tablebox">
      <table>
        <thead><tr><th class="num">번호</th><th>기술</th>{head}</tr></thead>
        <tbody>
{chr(10).join(rows)}
        </tbody>
      </table>
    </div>'''


EXTRA = '''
.two { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 12px; }
.box { background: var(--sheet); border: 1px solid var(--line); border-radius: 8px; padding: 14px 16px; display: flex; flex-direction: column; gap: 8px; min-width: 0; }
.box.plan { border: 2px solid var(--accent); }
.box ul { margin: 4px 0 0; padding-left: 18px; font-size: 14px; }
.box li + li { margin-top: 3px; }
.pc { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 12px; }
.pro, .con { font-size: 12px; font-weight: 700; border-radius: 4px; padding: 1px 7px; }
.pro { color: var(--quick); background: var(--quick-soft); }
.con { color: #b0452f; background: #f8e6e0; }
.grade { font-size: 12px; font-weight: 700; border-radius: 4px; padding: 1px 8px; white-space: nowrap; }
.grade.good { color: var(--quick); background: var(--quick-soft); }
.grade.mid { color: var(--soft); background: var(--accent-soft); }
.grade.bad { color: #b0452f; background: #f8e6e0; }
.tnum { font-family: var(--num); color: var(--accent); background: var(--accent-soft); border-radius: 4px; padding: 0 8px; margin-right: 4px; }
img.now { width: 100%; height: auto; display: block; border-radius: 8px; border: 1px solid var(--line); }
.nav { display: flex; flex-wrap: wrap; gap: 6px; }
.nav a { font-size: 14px; font-weight: 700; text-decoration: none; color: var(--ink); background: var(--accent-soft); border: 1px solid var(--line); border-radius: 4px; padding: 3px 14px; }
.nav a:hover, .nav a:focus-visible { background: var(--accent); color: var(--sheet); outline: none; }
section { scroll-margin-top: 12px; }
video { width: 100%; height: auto; display: block; border-radius: 8px; border: 1px solid var(--line); background: #000; }
details summary { cursor: pointer; font-size: 14px; font-weight: 700; }
code { font-size: 13px; background: var(--accent-soft); border-radius: 4px; padding: 0 5px; }
figure { margin: 0; display: flex; flex-direction: column; gap: 6px; }
.three { display: grid; grid-template-columns: repeat(3, minmax(0, 1fr)); gap: 10px; }
@media (max-width: 820px) { .three { grid-template-columns: minmax(0, 1fr); } }
@media (prefers-color-scheme: dark) { :root:not([data-theme="light"]) .con, :root:not([data-theme="light"]) .grade.bad { color: #f0a48f; background: #3a221c; } }
:root[data-theme="dark"] .con, :root[data-theme="dark"] .grade.bad { color: #f0a48f; background: #3a221c; }
@media (max-width: 820px) { .two, .pc { grid-template-columns: minmax(0, 1fr); } }
'''

nav = "".join(f'<a href="#{n.lower()}">{k}</a>' for n, k, *_ in ANIMALS)
total = sum(len(REFS.get(key, [])) for _n, _k, key, *_ in ANIMALS)
notes = "".join(f'<p class="small lead">조사 메모: {e(n)}</p>' for n in NOTES)
page = f'''<!doctype html>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>동물별 귀 작업 자료</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link href="https://fonts.googleapis.com/css2?family=Barlow+Condensed:wght@600;700&family=Noto+Sans+KR:wght@400;500;700&display=swap" rel="stylesheet">
<style>{CSS}{EXTRA}</style>

<div class="wrap">
  <header>
    <div class="eyebrow">RRRANDOM · 게임 · 2026-11-25</div>
    <h1>동물별 귀 작업 자료</h1>
    <p class="lead">동물마다 귀 생김새가 달라서 따로 봤습니다. 동물 여섯마다 참고 작품을 모으고, 기술 10가지를 그 동물의 귀에 대 봤습니다. 먼저 결론과 기술 10가지를 적고, 그 아래에 동물별 자료가 있습니다. 참고 작품은 모두 {total}가지입니다.</p>
    <nav class="nav" aria-label="동물"><a href="#tails">꼬리: 호랑이·토끼</a><a href="#tigersplit">걸을 땐 묵직, 틀 땐 격하게</a><a href="#tigerchain2">호랑이 꼬리 체인 더 격하게</a><a href="#tigerchain">호랑이 꼬리 체인처럼</a><a href="#tigerrootfit">호랑이 꼬리 뿌리 맞추기</a><a href="#tigerhang">호랑이 꼬리 작게·늘어뜨리기</a><a href="#tigersquiggle">호랑이 꼬리 꼬불거리게</a><a href="#tigerstray">호랑이 꼬리 Stray처럼</a><a href="#tigercat">호랑이 꼬리 고양이처럼</a><a href="#rabbitbone">토끼 꼬리 찌그러짐</a><a href="#built">적용 결과</a><a href="#rush">달리기·탈것</a><a href="#stiff">여우 귀 단단하게</a><a href="#techs">기술 10가지</a>{nav}</nav>
  </header>

{TAILS}
{built_section()}
  <section>
    <h2>먼저 결론</h2>
    <div class="two">
      <div class="box plan">
        <h3>여섯 모두에 통하는 것</h3>
        <ul>
          <li>지금은 여섯 모두 귀(용은 뿔과 지느러미)에 뼈가 없고 머리와 한 덩어리로 움직입니다.</li>
          <li>게임에서는 "귀 뼈 + 스프링 체인"(1번)에 "상태별 포즈"(2번)를 얹는 것이 기본입니다. 꼬리에서 만든 코드와 도구를 그대로 씁니다.</li>
          <li>출렁임보다 포즈가 더 많이 읽힙니다. 귀가 작은 동물은 포즈만으로 충분합니다.</li>
          <li>영상에서는 같은 뼈에 키를 넣습니다(8번).</li>
        </ul>
      </div>
      <div class="box">
        <h3>동물마다 달라지는 것</h3>
        <ul>
          <li><b>뼈 개수</b>: 레드 폭스·토끼 3~4개, 여우 2개, 다람쥐·호랑이 1개, 용은 지느러미만 1~2개.</li>
          <li><b>기준 자세</b>: 토끼는 한쪽 귀가 접혀 있어 좌우 값이 다릅니다.</li>
          <li><b>겹침</b>: 레드 폭스는 귀끼리와 리본, 다람쥐는 머리 뒤의 꼬리.</li>
          <li><b>털</b>: 용만 털이 없어 변형이나 파츠 방식도 쓸 수 있습니다.</li>
          <li><b>순서</b>: 여우에서 틀을 잡고, 레드 폭스, 토끼, 호랑이, 용, 다람쥐 순을 권합니다.</li>
        </ul>
      </div>
    </div>
    <h3>기술과 동물을 한 표로</h3>
{matrix()}
  </section>

  <section id="techs">
    <h2>기술 10가지와 장단점</h2>
    <div class="two">
{tech_cards()}
    </div>
  </section>

{"".join(animal_section(*a) for a in ANIMALS)}
  <section>
    <h2>알아 둘 것</h2>
    <div class="box">
      <ul>
        <li>참고 작품은 웹 조사 두 갈래로 모았습니다. 조사한 쪽이 출처 페이지를 열어 확인했고, 표의 "확신"은 그 판단입니다. 영상 링크는 검색으로 찾은 것이라 재생까지 확인하지 못한 것이 있습니다.</li>
        <li>기술의 장단점과 동물별 판단은 지금 프로젝트의 구조(몸 애니메이션을 C++ 코드가 직접 계산, 털은 따로 된 메시, 여우 꼬리에서 겪은 일)에 비춰 제가 내린 것입니다. 실제로 해 보면 달라질 수 있습니다.</li>
        <li>귀 사진은 게임의 연습용 맵에서 찍었습니다. 뼈 정보는 엔진에서 다시 확인했습니다.</li>
        <li>2026-11-25 사용자 결정("다람쥐는 작업에서 제외하고 나머지는 권장하는 순서로 작업해서 적용해줘")에 따라 맨 위의 "적용 결과"를 만들었습니다. 그 아래 자료는 고를 때 쓴 그대로입니다.</li>
      </ul>
      {notes}
    </div>
  </section>
</div>
'''
io.open(os.path.join(HERE, "index.html"), "w", encoding="utf-8", newline="\n").write(page)
print("wrote index.html", len(page), "refs", {key: len(REFS.get(key, [])) for _n, _k, key, *_ in ANIMALS})
