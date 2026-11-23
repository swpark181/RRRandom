"""The storyboard DRAFT of the re-cut fox-vs-tiger cinematic (2026-11-23, the user's new cut list; before the real
sequence is touched): writes page2/index.html. Panels are frames of the rough blocking LS_FvT_Board (board2/<take>,
RRRandom/Tools/cinematic/ue_board_build.py), cut 9's are the current film's (page/img/c9_*.jpg: "as before").
The top-view movement diagrams are drawn from board2/layout.json, which the build script writes.
Run with any Python 3:   python build_board2.py [take]      (default n6, the take with the tension pass: 13 cuts, 32 s; makes page2/img and the page)
"""
import html
import io
import json
import math
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TAKE = sys.argv[1] if len(sys.argv) > 1 else "n6"
FF = "C:/work/char-pipeline/tools/ffmpeg/ffmpeg.exe"
FPS = 24
L = json.load(io.open(os.path.join(HERE, "board2", "layout.json"), encoding="utf-8"))
T = L["times"]
LANE_X, ALLEY_Y = -4980.0, -3995.0
ROAD = (-5199.0, -4605.0)                       # the ring road's edges (X), read off the top view
ALLEY = (-4177.0, -3812.0)                      # the alley floor's edges (Y)
MOUTH_X = -5400.0
WALL_X = L["wall_x"]
TIGER, SIT, FOXRING, FOXWIDE = L["spots"]["tiger"], L["spots"]["sit"], L["spots"]["fox_ring"], L["spots"]["fox_wide"]
C = {k: v[0] for k, v in L["cuts"].items()}       # each cut's start, seconds
WIDE = L["wide"]                                  # the wide ring: length and width, cm

# (tension pass, 2026-11-23: the cuts between 3 and 6 follow the sketch page page3. A cut's "n" is its key in layout.json
# and its anchor; "label" is what the page prints: 3e is "3-끝")
NEAR = L["spots"]["near"]
cuts = [
    dict(n=1, name="추격: 얼굴에서 줌 아웃", lens="55→30mm", cam="Cam1_Chase", view="road",
         panels=[(3, "달리는 여우 얼굴 클로즈업"), (30, "불안하게 뒤를 한 번 돌아본다"), (80, "줌 아웃: 뒤에 쫓아오는 호랑이가 흐리게 보인다")],
         shot="여우 앞에서 같은 속도로 물러난다. 긴 렌즈로 얼굴을 크게 잡고 시작해, 돌아본 뒤에 줌 아웃하면서 조금 더 물러나고 여우 정면 쪽으로 돈다. 초점은 계속 여우.",
         act="여우가 고리 도로를 달린다. 0.5~2.0초에 오른쪽 어깨 너머로 뒤를 한 번 돌아보고 다시 앞을 본다(머리가 먼저 돌고 목과 가슴이 따라간다). 호랑이는 6m쯤 뒤에서 쫓아온다. 처음에는 여우 머리에 가려 있다가 줌 아웃하면서 드러난다.",
         face=[("여우", "놀람 → 걱정 (돌아보는 동안) → 놀람"), ("호랑이", "접근")], fx="배경과 호랑이는 초점 밖(조리개 1.4).",
         was="길 옆에서 따라가며 첫 장애물을 넘고 호랑이가 부수는 컷이었습니다. 장애물은 2컷으로 모았습니다."),
    dict(n=2, name="장애물: 짚고 올라 구르며 내려온다", lens="16→11.5mm", cam="Cam2_Obstacle", view="road",
         panels=[(90, "긴 장애물로 달려든다"), (98, "두 손으로 짚고 올라간다"), (116, "위를 달린다"), (140, "끝에서 뛰어내린다"),
                 (148, "구르며 착지한다. 뒤에 호랑이"), (158, "호랑이가 장애물을 부순다")],
         shot="길 옆에서 옆모습으로. 올라설 때까지 장애물 앞면 쪽에서 기다렸다가, 위를 달리는 여우를 따라 높이 올라 넘어가고, 끝에서는 넓게 벌려 장애물 전체와 호랑이까지 담는다.",
         act="높이 1m, 길이 4m의 평평한 단을 짚고 올라가(3.8~5.5초) 위를 달려 끝에서 뛰어내리고(5.6초), 바닥에서 한 바퀴 구른 뒤(5.9초~) 일어나 달린다. 뒤따라온 호랑이가 망치로 장애물을 부순다(6.4초).",
         face=[("여우", "놀람"), ("호랑이", "접근")], fx="장애물 블록이 부서져 날아간다.",
         was="낮은 주사위 더미를 뛰어넘는 컷이었습니다(가운데가 낮고 양옆이 높은 모양)."),
    dict(n=3, name="막다른 골목: 부딪히고 주저앉는다", lens="18mm", cam="Cam3_DeadEnd", view="alley",
         panels=[(200, "골목 끝으로 달려온다"), (206, "두 손으로 벽을 짚으며 부딪힌다"), (217, "튕겨 나와 돌아선다"),
                 (229, "등을 벽 쪽으로"), (238, "벽을 타고 미끄러져 내려간다"), (256, "바닥에 주저앉는다")],
         shot="골목 안 낮은 자리, 벽 쪽을 비스듬히. 고정.",
         act=f"여우가 골목 끝까지 달려와 두 손으로 벽을 짚으며 부딪힌다({T['hit']:.1f}초). 튕겨 나온 뒤 카메라 쪽으로 돌아서고({T['turn']:.1f}초~), 등을 벽에 대고 바닥까지 미끄러져 앉는다({T['sit']:.1f}초).",
         face=[("여우", "놀람")], fx=f"화면 밖에서 골목 입구 모퉁이가 부서진다({C['3'] + 0.6:.1f}초).",
         was="벽 앞에서 미끄러지며 멈추고 선 채로 돌아보는 컷이었습니다."),
    dict(n="3e", label="3-끝", new=True, name="갇혔다: 그림자가 먼저 닿는다", lens="18mm", cam="Cam3E_Top", view="alley",
         panels=[(268, "세 면이 막힌 구석의 여우"), (280, "입구에 선 호랑이의 그림자가 바닥을 타고 자란다"), (290, "그림자가 여우에게 닿는다")],
         shot="바로 위에서 내려다본다. 왼쪽이 막힌 벽, 오른쪽이 골목 입구. 고정.",
         act="호랑이가 골목 입구로 걸어 들어온다. 입구 쪽 낮은 빛 때문에 그의 긴 그림자가 바닥을 타고 여우 쪽으로 자란다.",
         face=[("여우", "놀람")], fx="골목 조명을 끄고, 길 쪽에서 골목 안으로 좁고 낮은 빛 하나. 이 컷 동안 빛을 낮춰서 그림자가 길어지게 했다.",
         was="없던 컷입니다."),
    dict(n=4, name="호랑이 등장: 그림자를 지나 얼굴이 보인다", lens="26mm", cam="Cam4_Tiger", view="alley",
         panels=[(300, "골목 입구, 역광 속 검은 형체"), (318, "어둠을 지나 걸어온다"), (332, "빛 속으로 들어서며 얼굴이 보인다")],
         shot="앉은 여우 옆 낮은 자리에서 골목 입구 쪽을 올려다본다(호랑이의 망치 쪽: 방패 쪽에서는 방패가 얼굴을 가림). 고정.",
         act="호랑이가 골목 입구에서 천천히 걸어 들어온다(초속 1.3m). 빛 웅덩이를 지나며 얼굴이 드러나지만 멈추지 않는다.",
         face=[("호랑이", "접근")], fx="골목 조명을 끄고 화면을 어둡게. 입구 뒤쪽의 낮은 조명으로 윤곽과 긴 그림자, 가운데에 빛 웅덩이 하나.",
         was="여우 어깨 너머로 역광과 긴 그림자를 보는 컷이었습니다. 수정안 초안에서는 3초였고 호랑이가 가운데에서 멈췄습니다."),
    dict(n="4-2", new=True, name="멈추지 않는다: 코앞까지", lens="20mm", cam="Cam42_Loom", view="alley",
         panels=[(350, "여우 머리 너머로 다가오는 호랑이"), (372, "가운데를 지나 계속 온다"), (392, "코앞에 선다")],
         shot="여우 뒤통수 너머, 조금 옆에서. 천천히 다가간다. 카메라가 막힌 벽 자리에 있어서 이 컷은 벽을 치우고 찍는다.",
         act=f"호랑이가 골목 가운데에서 서지 않고 여우 앞 1.4m까지 걸어온다({T['near']:.1f}초).",
         face=[("호랑이", "접근")], fx="막힌 벽 위쪽에 약한 푸른 조명을 하나 더해서 호랑이가 검은 덩어리로만 보이지 않게 했다.",
         was="없던 컷입니다. 수정안 초안에서는 호랑이가 4m 앞에서 멈췄습니다."),
    dict(n="4-3", new=True, name="망치를 든다 / 눈을 질끈 감는다", lens="12mm / 50mm", cam="Cam43_Up", view="alley",
         panels=[(402, "여우 눈높이에서 올려다본 호랑이. 망치가 올라간다"), (414, "천천히 든다"), (424, "여우의 눈"),
                 (438, "다 들었다. 반 박자 멈춘다"), (447, "떨리는 눈"), (454, "눈을 질끈 감는다")],
         shot="두 구도를 번갈아 보여 준다(1.0초, 0.6초, 0.4초, 0.5초로 짧아진다). 올려다보는 쪽은 여우 눈 자리에서 찍어서 그 두 컷에서는 여우를 숨겼다. 눈 쪽은 호랑이 망치 팔 옆 낮은 자리에서 긴 렌즈로.",
         act=f"호랑이가 1.5초에 걸쳐 망치를 들고({T['raise'][0]:.1f}~{T['raise'][1]:.1f}초) 꼭대기에서 멈춘다. 여우는 움츠러들다가({T['cower']:.1f}초~) 눈을 질끈 감는다({C['4-3'] + 2.12:.1f}초).",
         face=[("여우", f"놀람 → 눈 감음 ({C['4-3'] + 2.12:.1f}초)"), ("호랑이", "접근")], fx="없음. 소리를 넣게 되면 여기서 음악을 끊는다.",
         was="없던 컷입니다."),
    dict(n="5-1", new=True, name="뿅! 주사위가 대신 맞는다", lens="10.5mm", cam="Cam51_Bonk", view="alley",
         panels=[(460, "꼭대기에서 멈춘 망치. 여우는 움츠리고 있다"), (465, "내려치는 순간 여우 머리 위에 주사위가 나타난다"), (470, "뿅! 망치가 튕겨 나가고 주사위가 돌기 시작한다")],
         shot="골목 옆벽에 붙어서 옆모습으로. 골목 폭이 3.65m뿐이라 아주 넓은 렌즈를 썼다. 호랑이의 망치 쪽에서 찍어서 스케치와 좌우가 반대다(여우가 오른쪽).",
         act=f"호랑이가 망치를 내려치고({T['die']:.2f}초) 그 아래 여우 머리 위에 금색 주사위가 튀어나와 대신 맞는다({T['bonk']:.2f}초). 망치는 튕겨 올라가고 호랑이가 뒤로 젖혀진다.",
         face=[("여우", "눈 감음"), ("호랑이", f"접근 → 깜짝 ({T['bonk'] + 0.05:.1f}초)")], fx="맞는 자리에 작은 흰 연기. 주사위 반짝임이 시작된다.",
         was="수정안 초안의 5컷(겁에 질린 얼굴, 저절로 도는 주사위)을 바꿨습니다."),
    dict(n="5-2", new=True, name="1…? 5! 둘이 주사위를 본다", lens="12mm", cam="Cam52_Watch", view="alley",
         panels=[(486, "세차게 도는 주사위"), (506, "여우가 눈을 뜨고 올려다본다"), (524, "모서리로 서서 흔들린다"),
                 (533, "1 쪽으로 기운다. 호랑이 씩"), (543, "넘어간다"), (549, "5! 표정이 뒤바뀐다")],
         shot="같은 쪽 옆벽, 더 낮은 자리에서 올려다본다. 두 얼굴 사이 위에 주사위. 고정.",
         act=(f"맞은 주사위가 여우 머리 위에서 세차게 돌다가 느려진다. 여우가 1초에 걸쳐 고개를 들어 올려다본다({T['look']:.1f}초~). 주사위가 모서리로 서서 흔들리고({T['rock']:.1f}초) "
              f"1 쪽으로 기울었다가({T['one']:.1f}초) 5로 넘어간다({T['five']:.2f}초). 호랑이는 망치를 내리고 몸을 숙여 지켜본다."),
         face=[("여우", f"눈 감음 → 걱정 ({T['look'] + 0.3:.1f}초) → 환희 ({T['five'] + 0.06:.1f}초)"),
               ("호랑이", f"깜짝 → 긴장 ({T['bonk'] + 0.9:.1f}초) → 접근 ({T['one'] + 0.1:.1f}초) → 깜짝 ({T['five'] + 0.06:.1f}초)")],
         fx="주사위 반짝임과 빛기둥. 여우 얼굴에 금색 빛.",
         was="수정안 초안의 5컷(여우가 서서히 올려다보며 밝아진다)을 바꿨습니다. 올려다보며 밝아지는 것은 이 컷에 남겼습니다."),
    dict(n="5-3", new=True, name="터진다: 호랑이가 밀려난다", lens="18mm", cam="Cam53_Burst", view="alley",
         panels=[(556, "5에서 멈춘 주사위가 터진다"), (562, "빛에 밀려 호랑이가 뒤로 미끄러진다"), (584, "골목 가운데에서 멈춘다. 여우가 일어선다")],
         shot="막힌 벽 쪽 구석에서 골목 입구 쪽으로. 앞에 여우와 주사위, 멀리 밀려나는 호랑이. 스케치는 옆에서 넓게였는데, 골목이 좁아 옆에서는 둘을 한 화면에 담을 수 없어서 비스듬히 잡았다. 이 컷도 벽을 치우고 찍는다.",
         act=f"주사위가 터지고({T['land']:.2f}초) 호랑이가 방패를 든 채 골목 가운데까지 3.4m 밀려난다(~{T['tiger_back']:.1f}초). 여우는 몸을 숙였다가 일어선다({T['up']:.1f}초~).",
         face=[("여우", f"환희 → 결심 ({C['5-3'] + 0.3:.1f}초)"), ("호랑이", "깜짝")], fx="주사위가 터지는 빛과 고리.",
         was="없던 컷입니다. 밀려난 자리가 6컷에서 분신이 둘러싸는 자리입니다."),
    dict(n=6, name="분신: 넓게 둘러싼다", lens="17mm", cam="Cam6_Clones", view="alley",
         panels=[(592, "일어선 여우와 밀려난 호랑이"), (600, "연기와 함께 분신"), (640, "여우가 한 걸음 나서고, 여섯이 넓게 둘러싼다")],
         shot="여우 등 뒤 높은 곳에서 골목 길이 방향으로 내려다본다. 카메라가 막힌 벽 자리 너머에 있어서 벽을 치우고 찍는다. 고정.",
         act=f"연기가 터지며 분신 다섯이 나타나고({T['poof']:.2f}초) 여우가 한 걸음 나선다. 여섯이 골목 길이 방향의 긴 타원(길이 {WIDE[0] / 100:.1f}m, 폭 {WIDE[1] / 100:.1f}m)으로 호랑이를 둘러싼다. 호랑이는 좌우를 두리번거린다.",
         face=[("여우", f"결심 → 웃음 ({T['poof']:.1f}초)"), ("호랑이", f"깜짝 → 긴장 ({T['poof'] + 0.6:.1f}초)")],
         fx="분신 연기: 여우 자리에 큰 것 하나, 분신 자리마다 하나씩.",
         was="호랑이 얼굴만 0.5초 잡는 컷(6)과, 입구 쪽에서 내려다보는 분신 컷(7)이 따로 있었습니다. 7을 여기에 합쳤습니다. 일어서기와 호랑이 움찔은 5-3으로 옮겼습니다."),
    dict(n=8, name="포위: 한 걸음씩 좁힌다", lens="18mm", cam="Cam8_CloseIn", view="alley",
         panels=[(652, "넓게 선 여우들이 권총을 겨눈다"), (692, "다가오는 사이 호랑이가 망치를 놓친다"), (716, "좁은 원. 호랑이는 방패 뒤로 웅크린다")],
         shot="높은 곳에서 거의 수직으로 내려다본다(지금 영상과 같은 자리). 고정.",
         act=(f"여우 여섯이 권총을 겨눈 채 한 걸음 딛고 멈추기를 되풀이해({' / '.join(f'{a:.1f}' for a, _ in T['steps'])}초) 반지름 1.7m의 원까지 좁힌다. "
              f"여우와 맞은편 분신은 세 걸음, 벽 쪽 분신 넷은 두 걸음이고, 서로 조금씩 어긋나게 딛는다. 호랑이는 둘째 걸음에 망치를 놓치고({T['drop']:.1f}초) 방패 뒤로 웅크린다({T['crouch']:.1f}초)."),
         face=[("여우", "웃음"), ("호랑이", "긴장")], fx="없음.",
         was="처음부터 좁은 원으로 서서 겨누는 컷이었습니다."),
    dict(n=9, name="씩 웃는 여우", lens="40mm", cam="Cam9_Grin", view="alley", old=True,
         panels=[(1, "웅크린 호랑이 너머로 여우 얼굴"), (2, "눈을 한 번 깜빡인다"), (3, "권총을 겨눈 채 씩 웃는다")],
         shot="여우 턱 높이에서 살짝 올려다본다. 천천히 다가간다. 화면 앞에 호랑이 귀와 방패가 흐리게 걸친다.",
         act=f"여우가 권총을 겨눈 채 씩 웃는다. 눈을 한 번 깜빡인다({C['9'] + 0.9:.1f}초).",
         face=[("여우", "웃음 (눈 깜빡)")], fx="이 컷에만 여우 얼굴용 조명.",
         was="그대로입니다. 그림 세 장은 지금 영상(최종 8판)의 화면입니다."),
]
for c in cuts:
    c.setdefault("label", str(c["n"]))
for c in cuts:
    c["f0"], c["f1"] = (int(round(v * FPS)) for v in L["cuts"][str(c["n"])])

# ---------------------------------------------------------------------------------------------------------------- pictures
img_dir = os.path.join(HERE, "page2", "img")
os.makedirs(img_dir, exist_ok=True)
for c in cuts:
    for i, (fr, _) in enumerate(c["panels"], 1):
        dst = os.path.join(img_dir, f"c{c['n']}_{i}.jpg")
        if c.get("old"):
            shutil.copyfile(os.path.join(HERE, "page", "img", f"c9_{fr}.jpg"), dst)
        else:
            src = os.path.join(HERE, "board2", TAKE, f"fvt.{fr:04d}.jpeg")
            subprocess.run([FF, "-y", "-loglevel", "error", "-i", src, "-vf", "scale=960:540", "-q:v", "3", dst], check=True)


# ---------------------------------------------------------------------------------------------------------------- diagrams
def at(keys, t):
    """Linear position along [t, x, y] keys."""
    if t <= keys[0][0]:
        return keys[0][1], keys[0][2]
    for a, b in zip(keys, keys[1:]):
        if a[0] <= t <= b[0]:
            k = 0.0 if b[0] == a[0] else (t - a[0]) / (b[0] - a[0])
            return a[1] + (b[1] - a[1]) * k, a[2] + (b[2] - a[2]) * k
    return keys[-1][1], keys[-1][2]


def path_of(keys, t0, t1, n=40):
    pts = [at(keys, t0 + (t1 - 0.06 - t0) * i / n) for i in range(n + 1)]
    out = [pts[0]]
    for p in pts[1:]:
        if math.hypot(p[0] - out[-1][0], p[1] - out[-1][1]) > 4.0:
            out.append(p)
    return out


class Sketch:
    """A top view in world centimetres. Road views run left to right the way they run (-Y), alley views have the dead
    end on the left and the alley's mouth on the right (+X)."""

    def __init__(self, view):
        self.view, self.items, self.box = view, [], None

    def uv(self, x, y):
        return (-y, x) if self.view == "road" else (x, y)

    def grow(self, x, y, pad=0.0):
        u, v = self.uv(x, y)
        b = self.box
        self.box = [u - pad, v - pad, u + pad, v + pad] if b is None else [min(b[0], u - pad), min(b[1], v - pad), max(b[2], u + pad), max(b[3], v + pad)]

    def rect(self, x0, y0, x1, y1, cls, grow=False):
        (u0, v0), (u1, v1) = self.uv(x0, y0), self.uv(x1, y1)
        self.items.append(("rect", min(u0, u1), min(v0, v1), abs(u1 - u0), abs(v1 - v0), cls))
        if grow:
            self.grow(x0, y0)
            self.grow(x1, y1)

    def line(self, pts, cls, arrow=False):
        if len(pts) < 2:
            return
        for p in pts:
            self.grow(p[0], p[1], 40)
        self.items.append(("line", [self.uv(*p) for p in pts], cls, arrow))

    def who(self, x, y, letter, cls, r=34):
        self.grow(x, y, 70)
        self.items.append(("who", self.uv(x, y), letter, cls, r))

    def camera(self, x, y, tx, ty, mm, cls="cam", reach=230.0):
        self.grow(x, y, 60)
        d = math.atan2(ty - y, tx - x)
        half = math.atan(11.88 / mm)
        pts = [(x, y)] + [(x + reach * math.cos(d + s * half), y + reach * math.sin(d + s * half)) for s in (-1, 1)]
        for p in pts[1:]:
            self.grow(p[0], p[1], 10)
        self.items.append(("cam", [self.uv(*p) for p in pts], cls))

    def text(self, x, y, s, cls="lab", anchor="middle"):
        self.items.append(("text", self.uv(x, y), s, cls, anchor))

    def svg(self, label):
        b = self.box
        pad = 60.0
        u0, v0, w, h = b[0] - pad, b[1] - pad, b[2] - b[0] + 2 * pad, b[3] - b[1] + 2 * pad
        if w / h > 3.2:                              # not a ribbon: give it height
            v0, h = v0 - (w / 3.2 - h) / 2, w / 3.2
        if w / h < 1.5:
            u0, w = u0 - (h * 1.5 - w) / 2, h * 1.5
        fs = w / 30.0
        sw = fs / 5.5
        out = [f'<svg class="plan" viewBox="{u0:.0f} {v0:.0f} {w:.0f} {h:.0f}" role="img" aria-label="{html.escape(label)}" style="--fs:{fs:.1f}px;--sw:{sw:.1f}px">',
               f'<defs><marker id="ah{label[:3]}" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="5" markerHeight="5" orient="auto-start-reverse"><path d="M0 0L10 5L0 10z" fill="context-stroke"/></marker></defs>',
               f'<rect x="{u0:.0f}" y="{v0:.0f}" width="{w:.0f}" height="{h:.0f}" class="ground"/>']
        order = {"rect": 0, "cam": 1, "line": 2, "who": 3, "text": 4}
        for it in sorted(self.items, key=lambda i: order[i[0]]):
            if it[0] == "rect":
                out.append(f'<rect x="{it[1]:.0f}" y="{it[2]:.0f}" width="{it[3]:.0f}" height="{it[4]:.0f}" class="{it[5]}"/>')
            elif it[0] == "cam":
                p = it[1]
                out.append(f'<path d="M{p[0][0]:.0f} {p[0][1]:.0f}L{p[1][0]:.0f} {p[1][1]:.0f}L{p[2][0]:.0f} {p[2][1]:.0f}z" class="{it[2]}"/>')
                out.append(f'<circle cx="{p[0][0]:.0f}" cy="{p[0][1]:.0f}" r="{fs * 0.42:.0f}" class="{it[2]}dot"/>')
            elif it[0] == "line":
                d = "M" + "L".join(f"{u:.0f} {v:.0f}" for u, v in it[1])
                mark = f' marker-end="url(#ah{label[:3]})"' if it[3] else ""
                out.append(f'<path d="{d}" class="{it[2]}"{mark}/>')
            elif it[0] == "who":
                (u, v), letter, cls, r = it[1], it[2], it[3], it[4]
                r = max(r, fs * 0.8)
                out.append(f'<circle cx="{u:.0f}" cy="{v:.0f}" r="{r:.0f}" class="{cls}"/>')
                if letter:
                    out.append(f'<text x="{u:.0f}" y="{v + fs * 0.34:.0f}" class="in">{letter}</text>')
            else:
                (u, v), s, cls, anchor = it[1], it[2], it[3], it[4]
                out.append(f'<text x="{u:.0f}" y="{v:.0f}" class="{cls}" text-anchor="{anchor}">{html.escape(s)}</text>')
        out.append("</svg>")
        return "\n".join(out)


def cam_keys(name, t0, t1):
    return [k for k in L["cams"][name] if t0 - 1e-6 <= k[0] <= t1 + 1e-6]


def alley_set(s, wall=True):
    s.rect(MOUTH_X + 250, ALLEY[0] - 150, WALL_X - 100, ALLEY[0], "bldg")          # the two buildings
    s.rect(MOUTH_X + 250, ALLEY[1], WALL_X - 100, ALLEY[1] + 150, "bldg")
    s.rect(WALL_X - 100, ALLEY[0] - 150, WALL_X, ALLEY[1] + 150, "wall" if wall else "wallgone")
    s.rect(MOUTH_X + 250, ALLEY[0], WALL_X, ALLEY[1], "floor")
    s.grow(WALL_X - 100, ALLEY[0] - 60)
    s.grow(MOUTH_X + 150, ALLEY[1] + 60)
    s.text(WALL_X - 50, ALLEY[0] - 70, "막힌 벽" if wall else "벽 (이 컷만 치움)", anchor="start" if not wall else "middle")
    s.text(MOUTH_X + 150, ALLEY[0] - 70, "골목 입구 →", anchor="end")


def plan(c):
    n, (t0, t1) = c["n"], L["cuts"][str(c["n"])]
    s = Sketch(c["view"])
    ck = cam_keys(c["cam"], t0, t1)
    if c["view"] == "road":
        ob = L["obstacle"]
        fox, tig = path_of(L["fox"], t0, t1), path_of(L["tiger"], t0, t1)
        ys = [p[1] for p in fox + tig] + [k[2] for k in ck]
        s.rect(ROAD[0], max(ys) + 260, ROAD[1], min(ys) - 260, "road")
        if n == 2:
            s.rect(ob[0], ob[1], ob[2], ob[3], "block", True)
            s.text(LANE_X - 300, (ob[1] + ob[3]) / 2, "장애물 (높이 1m, 길이 4m)")
        s.line(tig, "ptiger", True)
        s.line(fox, "pfox", True)
        s.who(tig[0][0], tig[0][1], "호", "tiger")
        s.who(fox[0][0], fox[0][1], "여", "fox")
        s.line([(k[1], k[2]) for k in ck], "pcam", True)
        for k in (ck[0], ck[-1]):
            s.camera(k[1], k[2], k[4], k[5], k[7], "cam" if k is ck[-1] else "cam0")
    else:
        alley_set(s, wall=n not in (6, "4-2", "5-3"))
        ring = [(x1, y1) for _, _, x1, y1 in L["clones"]]
        wide = [(x0, y0) for x0, y0, _, _ in L["clones"]]
        if n == 3:
            fox = path_of(L["fox"], t0, T["sit"] + 0.2)
            s.line(fox, "pfox", True)
            s.who(fox[0][0], fox[0][1], "", "fox0", 20)
            s.who(SIT[0], SIT[1], "여", "fox")
        if n in (4, "3e", "4-2", "4-3", "5-1", "5-2", "5-3"):       # the fox on the floor, the tiger's way in the cut
            tig = path_of(L["tiger"], t0, t1)
            if n == 4:
                s.who(TIGER[0], TIGER[1], "", "pool", 120)
                s.text(TIGER[0], TIGER[1] + 165, "빛")
            if math.hypot(tig[-1][0] - tig[0][0], tig[-1][1] - tig[0][1]) > 30:
                s.line(tig, "ptiger", True)
            if n in ("5-1", "5-2", "5-3"):
                s.who(SIT[0] + 22, SIT[1], "", "die", 62)
                s.text(SIT[0] + 40, SIT[1] - 90, "머리 위 주사위", anchor="start")
            s.who(tig[0][0], tig[0][1], "호", "tiger")
            s.who(SIT[0], SIT[1], "여", "fox")
        if n == 5:
            s.who(SIT[0], SIT[1], "여", "fox")
            s.who(SIT[0], SIT[1], "", "die", 62)
            s.text(SIT[0] + 20, SIT[1] - 90, "머리 위 주사위", anchor="start")
            s.who(TIGER[0], TIGER[1], "호", "tiger")
        if n == 6:
            s.who(TIGER[0], TIGER[1], "호", "tiger")
            for p in wide:
                s.who(p[0], p[1], "분", "clone")
            s.line(path_of(L["fox"], t0, t1), "pfox", True)
            s.who(SIT[0], SIT[1], "", "fox0", 20)
            s.who(FOXWIDE[0], FOXWIDE[1], "여", "fox")
        if n == 8:
            s.who(TIGER[0], TIGER[1], "호", "tiger")
            for a, b in zip(wide + [(FOXWIDE[0], FOXWIDE[1])], ring + [(FOXRING[0], FOXRING[1])]):
                k = 34.0 / max(1.0, math.hypot(b[0] - a[0], b[1] - a[1]))
                s.line([a, (b[0] - (b[0] - a[0]) * k, b[1] - (b[1] - a[1]) * k)], "pfox", True)
                s.who(a[0], a[1], "", "fox0", 20)
            for p in ring:
                s.who(p[0], p[1], "분", "clone")
            s.who(FOXRING[0], FOXRING[1], "여", "fox")
            s.text(TIGER[0], ALLEY[1] + 95, "카메라: 바로 위에서 내려다봄")
        if n == 9:
            s.who(TIGER[0], TIGER[1], "호", "tiger")
            for p in ring:
                s.who(p[0], p[1], "분", "clone")
            s.who(FOXRING[0], FOXRING[1], "여", "fox")
        if n != 8:
            if len(ck) > 1 and math.hypot(ck[-1][1] - ck[0][1], ck[-1][2] - ck[0][2]) > 20:
                s.line([(k[1], k[2]) for k in ck], "pcam", True)
            reach = 330.0 if n == 6 else 230.0
            if abs(ck[0][7] - ck[-1][7]) > 1 or len(ck) > 1:
                s.camera(ck[0][1], ck[0][2], ck[0][4], ck[0][5], ck[0][7], "cam0", reach)
            s.camera(ck[-1][1], ck[-1][2], ck[-1][4], ck[-1][5], ck[-1][7], "cam", reach)
    return s.svg(f"컷 {n} 동선")


# ---------------------------------------------------------------------------------------------------------------- video
def film():
    """The rough blocking as a film, cut 9 from the current film (its frames 360-407), with the cut number in the corner."""
    out = os.path.join(HERE, "page2", "FoxVsTiger_board_draft.mp4")
    font = "C\\:/Windows/Fonts/arialbd.ttf"
    draw = ",".join(f"drawtext=fontfile='{font}':text='CUT {str(c['n']).upper()}':x=20:y=18:fontsize=26:fontcolor=white:box=1:boxcolor=black@0.45:boxborderw=8:"
                    f"enable='between(n,{c['f0']},{c['f1'] - 1})'" for c in cuts)
    n9 = cuts[-1]["f0"]
    subprocess.run([FF, "-y", "-loglevel", "error", "-framerate", "24", "-start_number", "0", "-i", os.path.join(HERE, "board2", TAKE, "fvt.%04d.jpeg"),
                    "-framerate", "24", "-start_number", "360", "-i", "C:/work/char-pipeline/output/cinematic_blocking/render/final1080h/fvt.%04d.jpeg",
                    "-filter_complex", f"[0]trim=end_frame={n9},setpts=PTS-STARTPTS[a];[1]scale=1280:720,setpts=PTS-STARTPTS[b];[a][b]concat=n=2:v=1,{draw}[v]",
                    "-map", "[v]", "-c:v", "libx264", "-pix_fmt", "yuv420p", "-crf", "27", "-preset", "slow", "-movflags", "+faststart", out], check=True)
    return out


# ---------------------------------------------------------------------------------------------------------------- page
def sf(fr):
    return f"{fr // FPS}+{fr % FPS:02d}"


def sec(fr):
    return f"{fr / FPS:.1f}"


e = html.escape
src = io.open(os.path.join(HERE, "build_board.py"), encoding="utf-8").read()
CSS = src[src.index("CSS = '''") + 9:src.index("'''\n\nn_panels")]
CSS += '''
/* the draft's own parts: a top-view plan beside each cut's notes, the "was" line, the change table, the questions */
:root { --road: #efe5cf; --floor: #d6ecc8; --bldg: #cdc6e4; --ground: #f7f6fa; --die: #e2a500; }
@media (prefers-color-scheme: dark) { :root:not([data-theme="light"]) { --road: #3b3628; --floor: #27381f; --bldg: #3b3558; --ground: #191822; --die: #f0c040; } }
:root[data-theme="dark"] { --road: #3b3628; --floor: #27381f; --bldg: #3b3558; --ground: #191822; --die: #f0c040; }
.lower { display: grid; grid-template-columns: minmax(0, 5fr) minmax(0, 6fr); gap: 14px 22px; align-items: start; }
.lower .notes { grid-template-columns: minmax(0, 1fr); }
.planbox { display: flex; flex-direction: column; gap: 6px; min-width: 0; }
.plan { width: 100%; height: auto; display: block; border: 1px solid var(--line); border-radius: 4px; font-family: var(--font); }
.plan .ground { fill: var(--ground); }
.plan .road { fill: var(--road); }
.plan .floor { fill: var(--floor); }
.plan .bldg { fill: var(--bldg); }
.plan .wall { fill: var(--soft); }
.plan .wallgone { fill: none; stroke: var(--soft); stroke-width: var(--sw); stroke-dasharray: 18 14; }
.plan .block { fill: var(--bldg); stroke: var(--soft); stroke-width: var(--sw); }
.plan .pfox, .plan .ptiger, .plan .pcam { fill: none; stroke-width: var(--sw); stroke-linejoin: round; stroke-linecap: round; }
.plan .pfox { stroke: var(--fox); }
.plan .ptiger { stroke: var(--tiger); stroke-dasharray: 26 16; }
.plan .pcam { stroke: var(--accent); stroke-dasharray: 6 14; }
.plan .fox { fill: var(--fox); }
.plan .fox0 { fill: var(--ground); stroke: var(--fox); stroke-width: var(--sw); }
.plan .clone { fill: var(--ground); stroke: var(--fox); stroke-width: var(--sw); }
.plan .tiger { fill: var(--tiger); }
.plan .pool { fill: var(--die); opacity: .35; }
.plan .die { fill: none; stroke: var(--die); stroke-width: var(--sw); stroke-dasharray: 10 8; }
.plan .cam { fill: var(--accent); fill-opacity: .2; stroke: var(--accent); stroke-width: calc(var(--sw) * .6); }
.plan .cam0 { fill: var(--accent); fill-opacity: .07; stroke: var(--accent); stroke-width: calc(var(--sw) * .5); stroke-dasharray: 8 8; }
.plan .camdot, .plan .cam0dot { fill: var(--accent); }
.plan .cam0dot { fill-opacity: .45; }
.plan text { font-size: var(--fs); fill: var(--soft); font-weight: 500; }
.plan text.in { fill: var(--sheet); font-weight: 700; text-anchor: middle; font-size: calc(var(--fs) * .95); }
.plan circle.clone + text.in { fill: var(--fox); }
.legend { display: flex; flex-wrap: wrap; gap: 4px 14px; font-size: 12px; color: var(--soft); margin: 0; padding: 0; list-style: none; }
.legend i { display: inline-block; width: 18px; height: 0; border-top: 3px solid; vertical-align: middle; margin-right: 5px; }
.legend .lf { border-color: var(--fox); } .legend .lt { border-color: var(--tiger); border-top-style: dashed; } .legend .lc { border-color: var(--accent); border-top-style: dotted; }
.was { font-size: 13px; color: var(--soft); border-top: 1px solid var(--line); padding-top: 10px; max-width: none; }
.was b { color: var(--ink); font-weight: 700; margin-right: 6px; }
.chip { display: inline-block; font-size: 11px; font-weight: 700; padding: 1px 7px; border-radius: 999px; background: var(--accent-soft); color: var(--accent); white-space: nowrap; }
.ask ol { margin: 0; padding-left: 20px; font-size: 14px; }
.ask li + li { margin-top: 8px; }
.ask b { font-weight: 700; }
.gone { background: var(--sheet); border: 1px dashed var(--line); border-radius: 8px; padding: 10px 14px; font-size: 14px; color: var(--soft); scroll-margin-top: 12px; }
.gone b { font-family: var(--num); font-size: 22px; color: var(--soft); margin-right: 8px; }
td.now { color: var(--soft); }
.no.long { font-size: 44px; }
.seg b { font-size: 18px; white-space: nowrap; }
@media (max-width: 720px) { .lower { grid-template-columns: minmax(0, 1fr); } }
'''

segs = "\n".join(
    f'      <a class="seg" href="#cut-{c["n"]}" style="flex-grow:{c["f1"] - c["f0"]}" aria-label="컷 {c["label"]} {e(c["name"])}"><b>{c["label"]}</b></a>' for c in cuts)
ticks = "\n".join(f'      <span style="flex-grow:{c["f1"] - c["f0"]}">{sec(c["f0"])}</span>' for c in cuts)
NOW = {1: "길 옆에서 따라가며 첫 장애물을 넘고, 호랑이가 부순다", 2: "둘째 장애물(낮은 주사위 더미)을 뛰어넘는다", 3: "미끄러지며 멈추고 돌아본다",
       4: "여우 어깨 너머, 역광과 긴 그림자", "5-1": "주사위가 나오자 바로 환호한다", 6: "호랑이 얼굴만 0.5초",
       8: "좁은 원으로 서서 겨누고, 호랑이가 망치를 떨어뜨린다", 9: "씩 웃는 여우"}
NEW = {1: "얼굴 클로즈업 → 뒤를 한 번 돌아봄 → 줌 아웃하며 뒤에 흐린 호랑이", 2: "길고 높이가 일정한 장애물을 짚고 올라가 구르며 내려온다",
       3: "벽에 부딪히며 멈추고, 돌아서며 주저앉는다", 4: "어두운 골목, 그림자를 지나 얼굴이 드러난다. 멈추지 않는다",
       6: "분신으로 넓게 둘러싼다 (기존 7 합침)",
       8: "넓게 선 여우들이 한 걸음씩 다가오고, 그 사이 망치를 놓친다", 9: "그대로"}
rows = []
for c in cuts:
    rows.append(f'        <tr><td class="num"><a href="#cut-{c["n"]}">{c["label"]}</a></td><td class="now">{e(NOW.get(c["n"], "없음 (새 컷)"))}</td><td>{e(NEW.get(c["n"], c["name"]))}</td>'
                f'<td class="num">{sec(c["f0"])}~{sec(c["f1"])}</td><td class="num">{sf(c["f1"] - c["f0"])}</td></tr>')
    if c["n"] == 6:
        rows.append('        <tr><td class="num">7</td><td class="now">입구 쪽 위에서 내려다보는 분신 컷</td><td>6에 합쳐서 삭제</td><td class="num">-</td><td class="num">-</td></tr>')
rows = "\n".join(rows)

LEGEND = ('<ul class="legend"><li><i class="lf"></i>여우가 움직이는 길</li><li><i class="lt"></i>호랑이가 움직이는 길</li>'
          '<li><i class="lc"></i>카메라 (부채꼴이 보는 쪽, 흐린 것은 컷 시작 때)</li></ul>')
blocks = []
for c in cuts:
    pan = "\n".join(
        f'          <figure><img src="img/c{c["n"]}_{i}.jpg" alt="컷 {c["label"]}-{i}: {e(cap)}" loading="lazy" width="960" height="540">'
        f'<figcaption><span class="tag">{c["label"]}-{i}</span><span class="cap">{e(cap)}</span>'
        f'<span class="tc">{"지금 영상" if c.get("old") else f"{fr / FPS:.2f}초"}</span></figcaption></figure>'
        for i, (fr, cap) in enumerate(c["panels"], 1))
    faces = "".join(f'<span class="who {"fox" if w == "여우" else "tiger"}">{w}</span> {e(x)}<br>' for w, x in c["face"])
    blocks.append(f'''    <article class="cut" id="cut-{c["n"]}">
      <div class="slate">
        <div class="no{" long" if len(c["label"]) > 1 else ""}"><span>컷</span>{c["label"]}</div>
        <div class="title">{e(c["name"])}</div>
        <dl class="time">
          <div><dt>구간</dt><dd>{sec(c["f0"])}~{sec(c["f1"])}초</dd></div>
          <div><dt>길이</dt><dd>{sf(c["f1"] - c["f0"])}</dd></div>
          <div><dt>렌즈</dt><dd>{c["lens"]}</dd></div>
        </dl>
        <div class="old">{"지금 영상의 화면" if c.get("old") else ("배치 초안 (새 컷)" if c.get("new") else "배치 초안 (동작 넣음)")}</div>
      </div>
      <div class="body">
        <div class="panels">
{pan}
        </div>
        <div class="lower">
          <div class="planbox">
{plan(c)}
            {LEGEND}
          </div>
          <dl class="notes">
            <div><dt>화면</dt><dd>{e(c["shot"])}</dd></div>
            <div><dt>동작</dt><dd>{e(c["act"])}</dd></div>
            <div><dt>표정</dt><dd>{faces}</dd></div>
            <div><dt>효과</dt><dd>{e(c["fx"])}</dd></div>
          </dl>
        </div>
        <p class="was"><b>지금 영상</b>{e(c["was"])}</p>
      </div>
    </article>''')
    if c["n"] == 6:
        blocks.append('    <div class="gone" id="cut-7"><b>7</b>기존 7컷(분신)은 6컷에 합쳐서 없앴습니다. 뒤 컷의 번호 8, 9는 그대로 둡니다.</div>')

n_panels = sum(len(c["panels"]) for c in cuts)
total = cuts[-1]["f1"]
film()
page = f'''<title>여우 vs 호랑이 콘티 수정안</title>
<meta name="claude:viewport-fit-sides" content="cover">
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Barlow+Condensed:wght@600;700&family=Noto+Sans+KR:wght@400;500;700&display=swap">
<style>{CSS}</style>

<div class="wrap">
  <header>
    <div class="eyebrow">RRRANDOM · 시네마틱 콘티 · 수정안 초안 · 2026-11-23</div>
    <h1>여우 vs 호랑이 콘티 수정안</h1>
    <p class="lead">말씀하신 컷별 수정 내용을 게임 캐릭터로 배치한 초안입니다. 실제 시퀀스는 건드리지 않았고, 따로 만든 연습용 맵에서 찍었습니다. 이번 판에서는 3컷과 6컷 사이를 <a href="https://claude.ai/artifact/6bC5aPXToCrmSUTHLhNHgT">구도 스케치</a>대로 바꿨습니다: 호랑이가 멈추지 않고 코앞까지 와서 망치를 들고, 내려치는 순간 금색 주사위가 대신 맞고, 둘이 주사위를 지켜본 뒤 터지는 빛에 호랑이가 밀려납니다.</p>
    <ul class="facts">
      <li><b>{len(cuts)}</b>컷 (새 컷 7개, 7번 없음)</li>
      <li><b>{n_panels}</b>그림</li>
      <li><b>{total // FPS}</b>초 (지금은 17초)</li>
      <li><a href="https://claude.ai/artifact/UqbffPLvmd1iaaBUh5vYmC">지금 영상의 콘티</a></li>
    </ul>
  </header>

  <section>
    <h2>시간 흐름</h2>
    <div class="strip">
{segs}
    </div>
    <div class="ticks" aria-hidden="true">
{ticks}
    </div>
    <p class="lead small">칸의 너비가 컷의 길이입니다. 번호를 누르면 그 컷으로 갑니다. 아래는 초안을 이어 붙인 영상입니다(9컷은 지금 영상의 화면).</p>
    <video class="film" controls muted playsinline preload="metadata" src="FoxVsTiger_board_draft.mp4"></video>
  </section>

  <section>
    <h2>무엇이 바뀌나</h2>
    <div class="tablebox">
      <table>
        <thead><tr><th class="num">컷</th><th>지금 영상</th><th>수정안</th><th class="num">구간(초)</th><th class="num">길이</th></tr></thead>
        <tbody>
{rows}
        </tbody>
      </table>
    </div>
    <p class="lead small">길이는 초+프레임입니다(3+12 = 3초 12프레임). 새 컷이 들어가면서 25초에서 32초로 늘었습니다. 컷 길이는 넉넉히 잡은 값입니다.</p>
  </section>

  <section class="ask">
    <h2>확인 부탁드립니다</h2>
    <div class="list">
      <ol>
        <li><b>옆에서 보는 구도의 좌우 (5-1, 5-2).</b> 스케치는 여우가 왼쪽이었는데 초안은 오른쪽입니다. 스케치대로 찍으면 호랑이의 방패가 몸과 망치를 가려서, 망치 쪽에서 찍었습니다.</li>
        <li><b>벽을 치우고 찍는 컷 (4-2, 5-3, 6).</b> 카메라가 막힌 벽 자리에 들어가야 해서 그 컷에서만 벽을 숨겼습니다.</li>
        <li><b>망치 높이.</b> 게임의 망치 휘두르기를 그대로 써서, 망치가 머리 위로 높이 솟지 않고 어깨 뒤로 젖혀졌다가 넘어옵니다. 스케치처럼 머리 위에서 빛을 가리게 하려면 드는 동작을 따로 만들어야 합니다.</li>
        <li><b>한쪽 눈만 뜬 표정.</b> 아직 없어서 눈 감음에서 걱정으로 바로 넘어갑니다.</li>
        <li><b>길이.</b> 초안은 {total // FPS}초입니다. 2컷(4초), 5-2(3초), 6·8컷(2.5초, 3초)을 줄이면 30초 안팎이 됩니다.</li>
        <li><b>앞서 여쭌 것.</b> 2컷 장애물 모양(높이 1m, 길이 4m의 단), 호랑이가 장애물을 부수는 것, 6컷 타원({WIDE[0] / 100:.1f}m × {WIDE[1] / 100:.1f}m)은 그대로 두었습니다.</li>
      </ol>
    </div>
  </section>


  <section>
    <h2>컷별 콘티</h2>
    <div class="cuts">
{chr(10).join(blocks)}
    </div>
  </section>

  <section id="motions">
    <h2>동작을 어떻게 만들었나</h2>
    <p class="lead small">새로 찍거나 사 온 동작은 없습니다. 게임에 있는 여우 동작과 모션 매칭 샘플의 동작을 자르고 잇고, 필요한 뼈만 시간에 따라 돌려서 만들었습니다.</p>
    <div class="tablebox">
      <table>
        <thead><tr><th>동작</th><th>재료</th><th>만든 방법</th></tr></thead>
        <tbody>
        <tr><td>뛰며 뒤돌아보기 (1컷)</td><td>게임의 달리기</td><td>달리기 세 바퀴 위에 가슴·목·머리 회전을 얹었습니다(모두 70도). 머리가 먼저 돌고 목과 가슴이 따라가며, 돌아올 때는 가슴이 먼저 돌아옵니다.</td></tr>
        <tr><td>벽에 부딪히기 (3컷)</td><td>겁먹고 서 있는 동작</td><td>부딪히는 순간 가슴과 머리가 앞으로 접혔다가 0.2초 뒤 뒤로 튕기고, 두 팔은 벽을 짚는 높이로 들었다가 내립니다.</td></tr>
        <tr><td>돌아서며 주저앉기 (3컷)</td><td>샘플의 "벤치에 앉기"</td><td>돌기 시작하는 곳부터 썼습니다(1.5배속). 원래는 무릎 위로 깊이 숙이며 앉는데, 벽을 타고 내려가는 느낌이 나도록 허리를 세웠습니다. 의자 높이에서 바닥까지는 여우를 내리면서 바닥에 앉은 자세로 넘깁니다.</td></tr>
        <tr><td>앉아서 떨기, 올려다보기 (3~5컷)</td><td>겁먹고 서 있는 동작</td><td>엉덩이와 다리는 앞으로 뻗은 채 고정하고 상체만 조금씩 흔들리게 했습니다. 올려다보기는 같은 동작에 목과 머리를 든 것으로 1초에 걸쳐 넘어갑니다.</td></tr>
        <tr><td>일어서기 (6컷)</td><td>샘플의 "벤치에서 일어서기"</td><td>몸을 앞으로 숙였다가 밀고 일어나 작게 한 발 나갑니다(1.3배속). 처음 0.4초 동안 바닥에서 의자 높이로 올립니다.</td></tr>
        <tr><td>한 걸음 딛고 멈추기 (6, 8컷)</td><td>모션 매칭의 걷기 시작 + 걷기 멈춤</td><td>오른발이 나가는 데까지와 왼발이 따라와 서는 데부터를 이었습니다. 한 걸음 57cm이고, 여우는 그 동작이 가는 만큼만 움직입니다. 8컷은 그 다리 위에 권총을 겨누는 상체를 얹었습니다.</td></tr>
        <tr><td>호랑이 망치 들기, 내려치기, 튕기기 (4-3, 5-1)</td><td>게임의 망치 휘두르기</td><td>그 동작은 상체만 쓰는 동작이라 다리는 게임의 서 있는 동작에서 가져왔습니다. 같은 동작을 느리게 틀어 들고, 꼭대기에서 멈추고, 주사위에 닿는 데까지만 내려친 뒤 거꾸로 돌려 튕겨 나가게 했습니다.</td></tr>
        <tr><td>호랑이 지켜보기, 밀려나기 (5-2, 5-3)</td><td>게임의 서 있는 동작, 움찔, 방패 막기</td><td>지켜보기는 서 있는 동작에 몸을 숙이고 고개를 들게 했습니다. 밀려나기는 움찔한 뒤 방패를 든 채로, 위치만 뒤로 미끄러지게 키를 넣었습니다.</td></tr>
        <tr><td>여우 움츠리기 (4-3 ~ 5-1)</td><td>앉아서 떠는 동작</td><td>등을 앞으로 말고 고개를 숙이게 했습니다. 주사위가 돌기 시작하면 올려다보는 동작으로 1초에 걸쳐 넘어갑니다.</td></tr>
        <tr><td>주사위 (5-1 ~ 5-3)</td><td>키</td><td>튀어나오기, 맞고 내려앉았다 떠오르기, 세차게 돌다 느려지기, 모서리로 서서 흔들리기, 1 쪽으로 기울기, 5로 넘어가기를 프레임마다 키로 넣었습니다.</td></tr>
        </tbody>
      </table>
    </div>
    <div class="list">
      <ul>
        <li>새 컷에서 거친 곳: 여우 눈 클로즈업은 고개를 숙인 얼굴을 옆 아래에서 잡아 화면이 기울어 보입니다. 5-1은 아주 넓은 렌즈라 가장자리가 늘어납니다. 망치와 주사위가 닿는 자리는 눈으로 맞춘 것입니다. 밀려나는 호랑이는 발이 미끄러지기만 합니다.</li>
        <li>아직 거친 곳: 손이 벽에 정확히 닿는지는 맞추지 않았습니다. 의자 높이에서 바닥으로 넘어가는 0.45초는 두 동작을 섞은 것입니다. 벽 쪽 분신 넷은 호랑이를 보며 비스듬히 걸어서 발이 조금 미끄러집니다(위에서 내려다보는 컷이라 잘 보이지 않습니다).</li>
        <li>샘플에 바닥에 주저앉는 동작은 없었습니다(벤치 높이 앉기뿐). 이 결과가 모자라면 다음 순서는 Mixamo, 그다음은 직접 찍은 영상입니다. UE 5.8의 MetaHuman Animator가 웹캠 한 대로 전신 동작을 받을 수 있어, 찍게 되면 엔진 안에서 할 수 있습니다.</li>
      </ul>
    </div>
  </section>

  <section>
    <h2>알아 둘 것</h2>
    <div class="list">
      <ul>
        <li>그림 아래 오른쪽 숫자는 초안 영상에서 그 그림의 시각입니다. 동선 그림은 위에서 내려다본 배치입니다(여 = 여우, 호 = 호랑이, 분 = 분신).</li>
        <li>2컷의 올라가기와 구르기는 모션 매칭 샘플의 동작을 여우에 옮긴 것입니다(달려와 1m 짚고 오르기, 착지하며 구르기). 손이 장애물 모서리에 정확히 닿지는 않습니다.</li>
        <li>2컷 카메라는 길가에 있는 큰 분홍 주사위를 피하려고 중간에 위로 넘어갑니다. 그 0.4초는 화면이 크게 흔들립니다.</li>
        <li>9컷은 바뀌지 않아서 지금 영상의 화면을 그대로 넣었습니다.</li>
        <li>조명과 표정 시점은 대강입니다. 음악과 효과음은 아직 없습니다.</li>
      </ul>
    </div>
  </section>
</div>
'''
out = os.path.join(HERE, "page2", "index.html")
io.open(out, "w", encoding="utf-8", newline="\n").write(page)
print("BOARD2", out, len(page), "chars,", len(cuts), "cuts,", n_panels, "panels")
