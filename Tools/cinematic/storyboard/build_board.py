"""The fox-vs-tiger cinematic's storyboard page: writes page/index.html and page/img from the cut list below.
2026-11-24: the re-cut film (final v10: 13 cuts, 32.75 s; v9 before the polish pass). Panels are frames of the final render
(cinematic_blocking/render/final1080j); cut times come from final/layout.json, written by
RRRandom/Tools/cinematic/ue_cine_build.py. The page of the film before it (v8, 9 cuts) is build_board_v8.py.
Run with any Python 3:    python build_board.py [take]
Cut labels are the user's: 1, 2, 3, 3-끝, 4, 4-2, 4-3, 5-1, 5-2, 5-3, 6, 8, 9 (the old cut 7 went into 6).
"""
import html
import io
import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TAKE = sys.argv[1] if len(sys.argv) > 1 else "final1080j"
FRAMES = os.path.join("C:/work/char-pipeline/output/cinematic_blocking/render", TAKE)
FF = "C:/work/char-pipeline/tools/ffmpeg/ffmpeg.exe"
FILM = "FoxVsTiger_web_v10.mp4"
FPS = 24
L = json.load(io.open(os.path.join(HERE, "final", "layout.json"), encoding="utf-8"))
T = L["times"]
C = {k: v[0] for k, v in L["cuts"].items()}
WIDE = L["wide"]

cuts = [
    dict(n=1, name="추격: 얼굴에서 줌 아웃", lens="55→30mm", cam="Cam1_Chase",
         panels=[(14, "달리는 여우 얼굴 클로즈업"), (30, "불안하게 뒤를 한 번 돌아본다"), (80, "줌 아웃: 뒤에 쫓아오는 호랑이가 흐리게 보인다")],
         view="여우 앞에서 같은 속도로 물러난다. 긴 렌즈로 얼굴을 크게 잡고 시작해, 돌아본 뒤에 줌 아웃하면서 조금 더 물러난다. 초점은 계속 여우.",
         act="여우가 고리 도로를 달린다. 0.5~2.0초에 오른쪽 어깨 너머로 뒤를 한 번 돌아보고 다시 앞을 본다. 호랑이는 6m쯤 뒤에서 쫓아온다.",
         face=[("여우", "놀람 → 걱정 (돌아보는 동안) → 놀람"), ("호랑이", "접근")], fx="배경과 호랑이는 초점 밖(조리개 1.4).",
         was="길 옆에서 따라가며 첫 장애물을 넘고 호랑이가 부수는 컷."),
    dict(n=2, name="장애물: 짚고 올라 구르며 내려온다", lens="16→11.5mm", cam="Cam2_Obstacle",
         panels=[(90, "긴 장애물로 달려든다"), (98, "두 손으로 짚고 올라간다"), (118, "위를 달린다"), (140, "끝에서 뛰어내린다"),
                 (150, "구르며 착지한다. 뒤에 호랑이"), (174, "호랑이가 장애물을 부순다")],
         view="길 옆 낮은 자리에서 옆모습으로. 여우가 올라서면 같은 속도로 나란히 따라가고, 끝에서는 넓게 벌려 장애물 전체와 호랑이까지 담는다.",
         act=f"높이 1m, 길이 4m의 평평한 단을 짚고 올라가({T['mantle'][0]:.1f}~{T['mantle'][1]:.1f}초) 위를 달려 끝에서 뛰어내리고({T['off']:.1f}초), 바닥에서 한 바퀴 구른 뒤({T['down']:.1f}초~) 일어나 달린다. 뒤따라온 호랑이가 망치로 장애물을 부순다({T['smash']:.1f}초).",
         face=[("여우", "놀람"), ("호랑이", "접근")], fx="장애물 블록이 부서져 화면 쪽으로 날아온다.",
         was="낮은 주사위 더미 둘을 뛰어넘는 컷."),
    dict(n=3, name="막다른 골목: 부딪히고 주저앉는다", lens="18mm", cam="Cam3_DeadEnd",
         panels=[(200, "골목 끝으로 달려온다"), (206, "두 손으로 벽을 짚으며 부딪힌다"), (217, "튕겨 나와 돌아선다"),
                 (229, "등을 벽 쪽으로"), (238, "벽을 타고 미끄러져 내려간다"), (256, "바닥에 주저앉는다")],
         view="골목 안 낮은 자리, 벽 쪽을 비스듬히. 고정.",
         act=f"여우가 골목 끝까지 달려와 두 손으로 벽을 짚으며 부딪힌다({T['hit']:.1f}초). 튕겨 나온 뒤 카메라 쪽으로 돌아서고({T['turn']:.1f}초~), 등을 벽에 대고 바닥까지 미끄러져 앉는다({T['sit']:.1f}초).",
         face=[("여우", "놀람")], fx=f"화면 밖에서 골목 입구 모퉁이가 부서진다({C['3'] + 0.6:.1f}초).",
         was="벽 앞에서 미끄러지며 멈추고 선 채로 돌아보는 컷."),
    dict(n="3e", label="3-끝", name="갇혔다: 그림자가 먼저 닿는다", lens="18mm", cam="Cam3E_Top",
         panels=[(268, "세 면이 막힌 구석의 여우"), (280, "입구에 선 호랑이의 그림자가 바닥을 타고 자란다"), (292, "그림자가 여우에게 닿는다")],
         view="바로 위에서 내려다본다. 왼쪽이 막힌 벽, 오른쪽이 골목 입구. 고정.",
         act="호랑이가 골목 입구로 걸어 들어온다. 입구 쪽 낮은 빛 때문에 그의 긴 그림자가 바닥을 타고 여우 쪽으로 자란다.",
         face=[("여우", "놀람")], fx="골목 조명을 끄고, 길 쪽에서 골목 안으로 좁고 낮은 빛 하나. 이 컷 동안 빛을 낮춰서 그림자가 길어진다.",
         was="없던 컷."),
    dict(n=4, name="호랑이 등장: 그림자를 지나 얼굴이 보인다", lens="26mm", cam="Cam4_Tiger",
         panels=[(296, "골목 입구, 역광 속의 호랑이"), (318, "빛 웅덩이를 지나 걸어온다"), (338, "멈추지 않고 다가온다")],
         view="앉은 여우 옆 낮은 자리에서 골목 입구 쪽을 올려다본다(호랑이의 망치 쪽). 고정.",
         act="호랑이가 골목 입구에서 천천히 걸어 들어온다(초속 1.3m). 빛 웅덩이를 지나며 얼굴이 드러나지만 멈추지 않는다.",
         face=[("호랑이", "접근")], fx="골목 조명을 끄고 어둡게. 입구 뒤쪽의 낮은 조명으로 윤곽과 긴 그림자, 가운데에 빛 웅덩이 하나, 호랑이 가슴과 얼굴에 좁은 보조광.",
         was="여우 어깨 너머로 역광과 긴 그림자를 보는 컷. 호랑이가 골목 가운데에서 멈췄다."),
    dict(n="4-2", name="멈추지 않는다: 코앞까지", lens="20mm", cam="Cam42_Loom",
         panels=[(350, "여우 머리 너머로 다가오는 호랑이"), (372, "가운데를 지나 계속 온다"), (392, "코앞에 선다")],
         view="여우 뒤통수 너머, 조금 옆에서. 천천히 다가간다. 카메라가 막힌 벽 자리에 있어서 이 컷은 벽을 치우고 찍는다.",
         act=f"호랑이가 골목 가운데에서 서지 않고 여우 앞 1.4m까지 걸어온다({T['near']:.1f}초).",
         face=[("호랑이", "접근")], fx="막힌 벽 위쪽에 약한 푸른 조명 하나.",
         was="없던 컷."),
    dict(n="4-3", name="망치를 든다 / 눈을 질끈 감는다", lens="12 / 42mm", cam="Cam43_Up · Cam43_Eyes",
         panels=[(402, "여우 눈높이에서 올려다본 호랑이. 망치가 올라간다"), (410, "천천히 든다"), (424, "여우의 얼굴"),
                 (440, "머리 위로 다 들었다. 반 박자 멈춘다"), (445, "움츠러든다"), (452, "눈을 질끈 감는다")],
         view="두 구도를 번갈아 보여 준다(1.0초, 0.6초, 0.4초, 0.5초로 짧아진다). 올려다보는 쪽은 여우 눈 자리에서 찍어서 그때는 여우를 숨기고, 얼굴 쪽은 호랑이가 선 자리 뒤 낮은 곳에서 정면으로 찍어서 그때는 호랑이를 숨긴다.",
         act=f"호랑이가 1.5초에 걸쳐 망치를 머리 위로 곧게 들고({T['raise'][0]:.1f}~{T['raise'][1]:.1f}초) 꼭대기에서 멈춘다. 여우는 움츠러들다가({T['cower']:.1f}초~) 눈을 질끈 감는다({C['4-3'] + 2.12:.1f}초).",
         face=[("여우", f"놀람 → 눈 감음 ({C['4-3'] + 2.12:.1f}초)"), ("호랑이", "접근")], fx="없음. 소리를 넣게 되면 여기서 음악을 끊는다.",
         was="없던 컷."),
    dict(n="5-1", name="뿅! 주사위가 대신 맞는다", lens="10.5mm", cam="Cam51_Bonk",
         panels=[(458, "머리 위에서 멈춘 망치. 여우는 움츠리고 있다"), (465, "내려치는 순간 여우 머리 위에 주사위가 나타난다"), (475, "뿅! 망치가 튕겨 나가고 주사위가 돌기 시작한다")],
         view="골목 옆벽에 붙어서 옆모습으로. 골목 폭이 3.65m뿐이라 아주 넓은 렌즈를 썼다. 호랑이의 망치 쪽에서 찍어서 스케치와 좌우가 반대다(여우가 오른쪽).",
         act=f"호랑이가 망치를 내려치고({T['die']:.2f}초) 그 아래 여우 머리 위에 금색 주사위가 튀어나와 대신 맞는다({T['bonk']:.2f}초). 망치는 튕겨 올라가고 호랑이가 뒤로 젖혀진다.",
         face=[("여우", "눈 감음"), ("호랑이", f"접근 → 깜짝 ({T['bonk'] + 0.05:.1f}초)")], fx="맞는 자리에 작은 흰 연기. 주사위 반짝임이 시작된다.",
         was="여우가 서서 환호하는 동안 주사위가 저절로 돌던 컷(5)."),
    dict(n="5-2", name="1…? 5! 둘이 주사위를 본다", lens="12mm", cam="Cam52_Watch",
         panels=[(486, "세차게 도는 주사위"), (506, "여우가 눈을 뜨고 올려다본다"), (524, "모서리로 서서 흔들린다"),
                 (533, "1 쪽으로 기운다. 호랑이 씩"), (543, "넘어간다"), (549, "5! 표정이 뒤바뀐다")],
         view="같은 쪽 옆벽, 더 낮은 자리에서 올려다본다. 두 얼굴 사이 위에 주사위. 고정.",
         act=(f"맞은 주사위가 여우 머리 위에서 세차게 돌다가 느려진다. 여우가 1초에 걸쳐 고개를 들어 올려다본다({T['look']:.1f}초~). 주사위가 모서리로 서서 흔들리고({T['rock']:.1f}초) "
              f"1 쪽으로 기울었다가({T['one']:.1f}초) 5로 넘어간다({T['five']:.2f}초). 호랑이는 망치를 내리고 몸을 숙여 지켜본다."),
         face=[("여우", f"눈 감음 → 걱정 ({T['look'] + 0.3:.1f}초) → 환희 ({T['five'] + 0.06:.1f}초)"),
               ("호랑이", f"깜짝 → 긴장 ({T['bonk'] + 0.9:.1f}초) → 접근 ({T['one'] + 0.1:.1f}초) → 깜짝 ({T['five'] + 0.06:.1f}초)")],
         fx="주사위 반짝임과 빛기둥. 여우 얼굴에 금색 빛.",
         was="없던 컷."),
    dict(n="5-3", name="터진다: 호랑이가 밀려난다", lens="18mm", cam="Cam53_Burst",
         panels=[(556, "5에서 멈춘 주사위가 터진다"), (564, "빛에 밀려 호랑이가 뒤로 미끄러진다"), (584, "골목 가운데에서 멈춘다. 여우가 일어선다")],
         view="막힌 벽 쪽 구석에서 골목 입구 쪽으로. 앞에 여우와 주사위, 멀리 밀려나는 호랑이. 이 컷도 벽을 치우고 찍는다.",
         act=f"주사위가 터지고({T['land']:.2f}초) 호랑이가 방패를 든 채 골목 가운데까지 2.9m 밀려난다(~{T['tiger_back']:.1f}초). 여우는 일어선다({T['up']:.1f}초~).",
         face=[("여우", f"환희 → 결심 ({C['5-3'] + 0.3:.1f}초)"), ("호랑이", "깜짝")], fx="주사위가 터지는 빛과 고리. 밀려나는 호랑이 발밑에 먼지.",
         was="호랑이 얼굴만 0.5초 잡던 컷(6)."),
    dict(n=6, name="분신: 넓게 둘러싼다", lens="17mm", cam="Cam6_Clones",
         panels=[(592, "일어선 여우와 밀려난 호랑이"), (600, "연기와 함께 분신"), (640, "여우가 한 걸음 나서고, 여섯이 넓게 둘러싼다")],
         view="여우 등 뒤 높은 곳에서 골목 길이 방향으로 내려다본다. 카메라가 막힌 벽 자리 너머에 있어서 벽을 치우고 찍는다. 고정.",
         act=f"연기가 터지며 분신 다섯이 나타나고({T['poof']:.2f}초) 여우가 한 걸음 나선다. 여섯이 골목 길이 방향의 긴 타원(길이 {WIDE[0] / 100:.1f}m, 폭 {WIDE[1] / 100:.1f}m)으로 호랑이를 둘러싼다. 호랑이는 좌우를 두리번거린다.",
         face=[("여우", f"결심 → 웃음 ({T['poof']:.1f}초)"), ("호랑이", f"깜짝 → 긴장 ({T['poof'] + 0.6:.1f}초)")],
         fx="분신 연기: 여우 자리에 큰 것 하나, 분신 자리마다 하나씩.",
         was="입구 쪽에서 내려다보는 분신 컷(7). 처음부터 좁은 원이었다."),
    dict(n=8, name="포위: 한 걸음씩 좁힌다", lens="18mm", cam="Cam8_CloseIn",
         panels=[(652, "넓게 선 여우들이 권총을 겨눈다"), (692, "다가오는 사이 호랑이가 망치를 놓친다"), (716, "좁은 원. 호랑이는 방패 뒤로 웅크린다")],
         view="높은 곳에서 거의 수직으로 내려다본다. 고정.",
         act=(f"여우 여섯이 권총을 겨눈 채 한 걸음 딛고 멈추기를 되풀이해({' / '.join(f'{a:.1f}' for a, _ in T['steps'])}초) 반지름 1.7m의 원까지 좁힌다. "
              f"호랑이는 둘째 걸음에 망치를 놓치고({T['drop']:.1f}초) 방패 뒤로 웅크린다({T['crouch']:.1f}초)."),
         face=[("여우", "웃음"), ("호랑이", "긴장")], fx="없음.",
         was="처음부터 좁은 원으로 서서 겨누는 컷."),
    dict(n=9, name="씩 웃는 여우", lens="40mm", cam="Cam9_Grin",
         panels=[(722, "웅크린 호랑이 너머로 여우 얼굴"), (743, "눈을 한 번 깜빡인다"), (764, "권총을 겨눈 채 씩 웃는다")],
         view="여우 턱 높이에서 살짝 올려다본다. 천천히 다가간다. 화면 앞에 호랑이 귀와 방패가 흐리게 걸친다.",
         act=f"여우가 권총을 겨눈 채 씩 웃는다. 눈을 한 번 깜빡인다({C['9'] + 0.9:.1f}초).",
         face=[("여우", "웃음 (눈 깜빡)")], fx="이 컷에만 여우 얼굴용 조명.",
         was="같은 컷."),
]
for c in cuts:
    c.setdefault("label", str(c["n"]))
    c["f0"], c["f1"] = (int(round(v * FPS)) for v in L["cuts"][str(c["n"])])

img_dir = os.path.join(HERE, "page", "img")
os.makedirs(img_dir, exist_ok=True)
for name in os.listdir(img_dir):
    os.remove(os.path.join(img_dir, name))
ext = os.path.splitext(os.listdir(FRAMES)[0])[1]
for c in cuts:
    for i, (fr, _) in enumerate(c["panels"], 1):
        subprocess.run([FF, "-y", "-loglevel", "error", "-i", os.path.join(FRAMES, f"fvt.{fr:04d}{ext}"), "-vf", "scale=960:540", "-q:v", "3",
                        os.path.join(img_dir, f"c{c['n']}_{i}.jpg")], check=True)


def sf(fr):
    return f"{fr // FPS}+{fr % FPS:02d}"


def sec(fr):
    s = f"{fr / FPS:.2f}".rstrip("0").rstrip(".")
    return s if "." in s else s + ".0"


e = html.escape
segs = "\n".join(
    f'      <a class="seg" href="#cut-{c["n"]}" style="flex-grow:{c["f1"] - c["f0"]}" aria-label="컷 {c["label"]} {e(c["name"])}"><b>{c["label"]}</b></a>'
    for c in cuts)
rows = "\n".join(
    f'        <tr><td class="num"><a href="#cut-{c["n"]}">{c["label"]}</a></td><td>{e(c["name"])}</td>'
    f'<td class="num">{sec(c["f0"])}~{sec(c["f1"])}</td><td class="num">{sf(c["f1"] - c["f0"])}</td><td class="num">{c["lens"]}</td>'
    f'<td>{" · ".join(e(w + " " + x) for w, x in c["face"])}</td></tr>' for c in cuts)
blocks = []
for c in cuts:
    pan = "\n".join(
        f'          <figure><img src="img/c{c["n"]}_{i}.jpg" alt="컷 {c["label"]}의 {i}번 그림: {e(cap)}" loading="lazy" width="960" height="540">'
        f'<figcaption><span class="tag">{c["label"]}·{i}</span><span class="cap">{e(cap)}</span><span class="tc">{fr / FPS:.2f}초</span></figcaption></figure>'
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
        <div class="old">{c["cam"]}</div>
      </div>
      <div class="body">
        <div class="panels">
{pan}
        </div>
        <dl class="notes">
          <div><dt>화면</dt><dd>{e(c["view"])}</dd></div>
          <div><dt>동작</dt><dd>{e(c["act"])}</dd></div>
          <div><dt>표정</dt><dd>{faces}</dd></div>
          <div><dt>효과</dt><dd>{e(c["fx"])}</dd></div>
          <div class="was"><dt>8판</dt><dd>{e(c["was"])}</dd></div>
        </dl>
      </div>
    </article>''')

CSS = '''/* layout: a storyboard sheet. A running-time strip on top, then one row per cut: a slate column (number, time,
   lens) beside its numbered panels and the four note lines a paper sheet has (picture / action / face / effects). */
:root {
  --bg: #f2f1f6;
  --sheet: #ffffff;
  --ink: #1c1a29;
  --soft: #5d5a70;
  --line: #d6d3e2;
  --accent: #5b3fd0;
  --accent-soft: #e9e4fb;
  --fox: #c8500f;
  --tiger: #8a5a00;
  --font: "Noto Sans KR", "Apple SD Gothic Neo", "Malgun Gothic", system-ui, sans-serif;
  --num: "Barlow Condensed", "Arial Narrow", "Noto Sans KR", sans-serif;
}
@media (prefers-color-scheme: dark) {
  :root:not([data-theme="light"]) {
    --bg: #15141d; --sheet: #1e1c2a; --ink: #ecebf4; --soft: #a3a0b8; --line: #353247; --accent: #a892ff; --accent-soft: #2c2650; --fox: #ff9254; --tiger: #e6b04a;
    color-scheme: dark;
  }
}
:root[data-theme="dark"] {
  --bg: #15141d; --sheet: #1e1c2a; --ink: #ecebf4; --soft: #a3a0b8; --line: #353247; --accent: #a892ff; --accent-soft: #2c2650; --fox: #ff9254; --tiger: #e6b04a;
  color-scheme: dark;
}
body { background: var(--bg); color: var(--ink); font-family: var(--font); font-size: 15px; line-height: 1.65;
  padding-block: 28px 64px; padding-left: max(16px, env(safe-area-inset-left, 0px)); padding-right: max(16px, env(safe-area-inset-right, 0px)); }
.wrap { max-width: 1120px; margin: 0 auto; display: flex; flex-direction: column; gap: 28px; }
header { display: flex; flex-direction: column; gap: 10px; }
.eyebrow { font-size: 12px; font-weight: 700; letter-spacing: .08em; color: var(--accent); }
h1 { font-size: clamp(24px, 4vw, 34px); font-weight: 700; line-height: 1.25; margin: 0; text-wrap: balance; }
h2 { font-size: 18px; font-weight: 700; margin: 0; }
p { margin: 0; max-width: 68ch; }
.lead { color: var(--soft); }
.small { font-size: 13px; }
.facts { display: flex; flex-wrap: wrap; gap: 6px 22px; margin: 0; padding: 0; list-style: none; font-size: 14px; }
.facts b { font-family: var(--num); font-size: 20px; font-weight: 700; margin-right: 4px; font-variant-numeric: tabular-nums; }

section { display: flex; flex-direction: column; gap: 12px; }
.strip { display: flex; gap: 3px; }
.seg { flex: 1 1 0; min-width: 1.5em; height: 44px; display: flex; align-items: center; justify-content: center; background: var(--accent-soft); color: var(--ink);
  text-decoration: none; border-radius: 4px; border: 1px solid var(--line); }
.seg b { font-family: var(--num); font-size: 22px; font-weight: 700; }
.seg:hover, .seg:focus-visible { background: var(--accent); color: var(--sheet); outline: none; }
.ticks { display: flex; gap: 3px; font-size: 11px; color: var(--soft); font-variant-numeric: tabular-nums; }
.ticks span { flex: 1 1 0; min-width: 1.5em; white-space: nowrap; }
.film { width: 100%; max-width: 960px; border-radius: 8px; display: block; background: #000; }

.tablebox { overflow-x: auto; background: var(--sheet); border: 1px solid var(--line); border-radius: 8px; }
table { border-collapse: collapse; width: 100%; min-width: 560px; font-size: 14px; }
th, td { text-align: left; padding: 7px 12px; border-bottom: 1px solid var(--line); vertical-align: top; }
tr:last-child td { border-bottom: 0; }
th { font-size: 12px; color: var(--soft); font-weight: 700; }
td.num, th.num { font-variant-numeric: tabular-nums; white-space: nowrap; }
td a { color: var(--accent); font-weight: 700; }

.cuts { display: flex; flex-direction: column; gap: 18px; }
.cut { display: grid; grid-template-columns: 150px minmax(0, 1fr); background: var(--sheet); border: 1px solid var(--line); border-radius: 8px; overflow: hidden;
  scroll-margin-top: 12px; }
.slate { padding: 16px 14px; border-right: 1px solid var(--line); display: flex; flex-direction: column; gap: 8px; min-width: 0; }
.no { font-family: var(--num); font-size: 64px; font-weight: 700; line-height: .9; color: var(--accent); display: flex; align-items: baseline; gap: 6px; }
.no span { font-family: var(--font); font-size: 13px; font-weight: 700; color: var(--soft); }
.title { font-size: 16px; font-weight: 700; line-height: 1.35; }
.time { margin: 0; display: flex; flex-direction: column; gap: 2px; font-size: 13px; }
.time div { display: flex; justify-content: space-between; gap: 8px; }
.time dt { color: var(--soft); }
.time dd { margin: 0; font-variant-numeric: tabular-nums; font-weight: 500; white-space: nowrap; }
.old { font-size: 11px; color: var(--soft); overflow-wrap: anywhere; margin-top: auto; }
.body { padding: 14px; display: flex; flex-direction: column; gap: 14px; min-width: 0; }
.panels { display: grid; grid-template-columns: repeat(3, minmax(0, 1fr)); gap: 10px; }
figure { margin: 0; display: flex; flex-direction: column; gap: 5px; min-width: 0; }
figure img { width: 100%; height: auto; display: block; border: 1px solid var(--line); border-radius: 4px; background: var(--bg); }
figcaption { display: grid; grid-template-columns: auto minmax(0, 1fr) auto; gap: 8px; align-items: baseline; font-size: 13px; line-height: 1.4; }
.tag { font-family: var(--num); font-size: 17px; font-weight: 700; color: var(--accent); font-variant-numeric: tabular-nums; }
.tc { font-size: 11px; color: var(--soft); font-variant-numeric: tabular-nums; white-space: nowrap; }
.notes { margin: 0; display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 8px 22px; font-size: 14px; }
.notes div { display: grid; grid-template-columns: 2.6em minmax(0, 1fr); gap: 10px; }
.notes dt { font-size: 12px; font-weight: 700; color: var(--soft); padding-top: 2px; }
.notes dd { margin: 0; }
.who { font-size: 12px; font-weight: 700; }
.who.fox { color: var(--fox); }
.who.tiger { color: var(--tiger); }
.list { background: var(--sheet); border: 1px solid var(--line); border-radius: 8px; padding: 12px 18px; }
.list ul { margin: 0; padding-left: 18px; font-size: 14px; }
.list li + li { margin-top: 4px; }
a:focus-visible { outline: 2px solid var(--accent); outline-offset: 2px; }
@media (max-width: 720px) {
  .cut { grid-template-columns: minmax(0, 1fr); }
  .slate { border-right: 0; border-bottom: 1px solid var(--line); flex-direction: row; flex-wrap: wrap; align-items: center; gap: 6px 16px; }
  .no { font-size: 44px; }
  .time { flex-direction: row; flex-wrap: wrap; gap: 2px 14px; }
  .old { width: 100%; margin-top: 0; }
  .panels { grid-template-columns: minmax(0, 1fr); }
  .notes { grid-template-columns: minmax(0, 1fr); }
}
.no.long { font-size: 44px; }
.seg b { font-size: clamp(13px, 1.7vw, 22px); white-space: nowrap; }
.notes .was { color: var(--soft); }
.changes { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 12px; }
.changes .list h3 { margin: 0 0 6px; font-size: 14px; }
@media (max-width: 720px) { .changes { grid-template-columns: minmax(0, 1fr); } .no.long { font-size: 36px; } }
'''

n_panels = sum(len(c["panels"]) for c in cuts)
total = cuts[-1]["f1"]
page = f'''<title>여우 vs 호랑이 콘티</title>
<meta name="claude:viewport-fit-sides" content="cover">
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Barlow+Condensed:wght@600;700&family=Noto+Sans+KR:wght@400;500;700&display=swap">
<style>{CSS}</style>

<div class="wrap">
  <header>
    <div class="eyebrow">RRRANDOM · 시네마틱 콘티 · 2026-11-24</div>
    <h1>여우 vs 호랑이 콘티</h1>
    <p class="lead">손본 콘티대로 실제 시퀀스를 다시 만든 영상(최종 10판)을 컷 순서대로 정리했습니다. 컷 번호는 초안 때 그대로이고(7번은 6번에 합쳐서 없습니다), 그림에는 컷 번호와 순번을 붙였습니다(예: 4-3·2는 컷 4-3의 둘째 그림). 고칠 곳은 이 번호로 가리키시면 됩니다.</p>
    <ul class="facts">
      <li><b>{len(cuts)}</b>컷</li>
      <li><b>{n_panels}</b>그림</li>
      <li><b>{total // FPS}</b>초</li>
      <li><b>{total}</b>프레임 (초당 {FPS})</li>
    </ul>
  </header>

  <section>
    <h2>영상과 시간 흐름</h2>
    <video class="film" controls muted playsinline preload="metadata" src="{FILM}"></video>
    <div class="strip">
{segs}
    </div>
    <p class="lead small">칸의 너비가 컷의 길이입니다. 번호를 누르면 그 컷으로 갑니다. 소리는 아직 없습니다.</p>
  </section>

  <section>
    <h2>초안에서 달라진 것</h2>
    <div class="changes">
      <div class="list">
        <h3>고친 것</h3>
        <ul>
          <li><b>컷 2</b>: 여우가 장애물 위를 달릴 때 화면이 휙 돌며 흐려지던 것을 없앴습니다. 카메라가 낮게 나란히 따라갑니다.</li>
          <li><b>컷 4</b>: 빛을 지난 뒤 방패가 검은 덩어리로 보이던 것을 좁은 보조광으로 살렸습니다.</li>
          <li><b>컷 4-3</b>: 망치를 어깨 뒤가 아니라 머리 위로 곧게 듭니다. 여우 쪽은 한쪽 눈만 기울어 보이던 구도를 정면 얼굴로 바꿨습니다.</li>
          <li><b>컷 5-1</b>: 머리 위로 든 망치가 내려와 주사위에 맞습니다.</li>
          <li><b>컷 5-3</b>: 밀려나는 호랑이 발밑에 먼지를 넣었습니다.</li>
          <li><b>컷 6</b>: 분신이 연기보다 먼저 보이던 것을 연기 속에서 나타나게 했습니다.</li>
          <li><b>컷 9</b>: 시작할 때 분신의 권총이 렌즈를 가리던 것을 고쳤습니다. 이제 이 컷도 새로 찍은 화면입니다.</li>
        </ul>
      </div>
      <div class="list">
        <h3>그대로 둔 것 (확인 부탁드립니다)</h3>
        <ul>
          <li>장애물은 길이 4m, 높이 1m의 단 하나이고, 호랑이가 부숩니다.</li>
          <li>컷 5-1과 5-2는 스케치와 좌우가 반대입니다(호랑이의 망치 쪽에서 찍어야 방패에 가리지 않습니다).</li>
          <li>컷 4-2, 5-3, 6은 카메라 자리 때문에 막힌 벽을 치우고 찍습니다.</li>
          <li>주사위는 망치에 맞아서 돌고, 1로 기울었다가 5로 넘어갑니다.</li>
          <li>한쪽 눈만 뜨는 새 표정은 만들지 않았습니다. 지금 있는 눈 감은 표정을 씁니다.</li>
          <li>길이는 32초입니다(8판은 17초).</li>
        </ul>
      </div>
    </div>
  </section>

  <section>
    <h2>컷 목록</h2>
    <div class="tablebox">
      <table>
        <thead><tr><th class="num">컷</th><th>내용</th><th class="num">구간(초)</th><th class="num">길이</th><th class="num">렌즈</th><th>표정</th></tr></thead>
        <tbody>
{rows}
        </tbody>
      </table>
    </div>
    <p class="lead small">길이는 콘티 용지 표기대로 초+프레임입니다(2+06 = 2초 6프레임).</p>
  </section>

  <section>
    <h2>컷별 콘티</h2>
    <div class="cuts">
{chr(10).join(blocks)}
    </div>
  </section>

  <section>
    <h2>알아 둘 것</h2>
    <div class="list">
      <ul>
        <li>그림은 최종 영상 10판(1080p)의 프레임입니다. 9판에서 페이드, 털, 표정, 동작 이음매를 손본 판이고, 끝에 페이드 0.75초가 붙었습니다. 그림 아래 오른쪽 숫자가 그 프레임의 시각입니다.</li>
        <li>각 컷의 "8판" 줄은 이전 영상(17초, 9컷)에서 그 자리에 있던 컷입니다.</li>
        <li>음악과 효과음은 아직 없어서 소리 칸은 두지 않았습니다.</li>
        <li>표정 이름은 여우 놀람·걱정·결심·웃음·환희·눈 감음, 호랑이 접근·깜짝·긴장입니다.</li>
        <li>움직임은 프레임을 띄엄띄엄 뽑아 확인했습니다. 영상을 재생해서 보시면 제가 놓친 어색한 곳이 있을 수 있습니다.</li>
      </ul>
    </div>
  </section>
</div>
'''
out = os.path.join(HERE, "page", "index.html")
io.open(out, "w", encoding="utf-8", newline="\n").write(page)
io.open(os.path.join(HERE, "page", "preview.html"), "w", encoding="utf-8", newline="\n").write('<!doctype html><meta charset="utf-8">' + page)
print("BOARD", out, len(page), "chars,", len(cuts), "cuts,", n_panels, "panels")
