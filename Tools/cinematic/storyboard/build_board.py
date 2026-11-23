"""The fox-vs-tiger cinematic's storyboard page (2026-11-23): writes page/index.html from the cut list below.
Panels are frames of the final render (page/img/c<cut>_<n>.jpg, made from render/final1080h). Run with any Python 3:
    python build_board.py
Cut times are frames at 24 fps, from RRRandom/Tools/cinematic/ue_cine_build.py (CUTS, the expression keys, the cues).
"""
import html
import io
import os

FPS = 24
cuts = [
    dict(n=1, name="추격", f0=0, f1=58, old="1A", cam="Cam1a_Chase", lens="24mm",
         panels=[(4, "여우가 달려오고 호랑이가 뒤쫓는다"), (18, "첫 장애물을 뛰어넘는다"), (46, "호랑이가 망치로 장애물을 날린다")],
         view="길 옆 조금 위에서, 여우 앞쪽을 같은 속도로 물러나며 따라간다.",
         act="여우가 고리 도로를 달려 첫 장애물(주사위 더미)을 뛰어넘는다(0.6초). 뒤따르던 호랑이가 망치를 머리 위로 들었다 내리쳐 장애물을 날린다(1.8초).",
         face=[("여우", "놀람"), ("호랑이", "접근")], fx="주사위 블록이 부서져 여우 쪽으로 튄다."),
    dict(n=2, name="둘째 장애물", f0=58, f1=120, old="1B", cam="Cam1b_Barricade", lens="24mm",
         panels=[(60, "여우가 둘째 장애물로 달려든다"), (84, "여우는 넘어 지나가고 호랑이가 망치를 든다"), (93, "장애물이 부서진다")],
         view="둘째 장애물 옆 낮은 자리. 거의 고정.",
         act="여우가 둘째 장애물을 넘어 화면 앞으로 빠져나간다(2.7초). 호랑이가 다시 망치를 내리쳐 부수고 달려 나온다(3.9초).",
         face=[("여우", "놀람"), ("호랑이", "접근")], fx="주사위 블록이 화면 쪽으로 튄다."),
    dict(n=3, name="막다른 골목", f0=120, f1=156, old="2", cam="Cam2_DeadEnd", lens="18mm",
         panels=[(132, "골목으로 뛰어든다"), (148, "벽 앞에서 미끄러지며 멈춘다"), (155, "뒤를 돌아본다")],
         view="골목 안쪽, 낮은 자리. 고정.",
         act="여우가 골목으로 뛰어들어 벽 앞에서 미끄러지며 멈추고(5.5초), 뒤를 돌아본다(6.2~6.5초).",
         face=[("여우", "놀람 → 걱정 (6.3초)")], fx="화면 밖에서 골목 입구 모퉁이가 부서진다(5.6초). 다음 컷의 잔해가 된다."),
    dict(n=4, name="호랑이 등장", f0=156, f1=204, old="3", cam="Cam3_TigerArrives", lens="30mm",
         panels=[(158, "골목 입구에 호랑이가 선다"), (182, "여우를 보며 걸어온다"), (203, "가까이 와서 멈춘다")],
         view="여우 등 뒤 낮은 자리에서 골목 입구 쪽. 역광과 긴 그림자. 여우는 화면 앞에 흐리게 걸친다. 고정.",
         act="호랑이가 잔해가 깔린 골목 입구에서 여우를 똑바로 보며 천천히 걸어 들어온다.",
         face=[("호랑이", "접근"), ("여우", "걱정 (등을 보임)")], fx="이 컷에만 골목 입구의 낮고 따뜻한 조명으로 호랑이 그림자를 골목 끝까지 드리우고, 호랑이 얼굴에 좁은 보조광을 준다."),
    dict(n=5, name="금색 주사위", f0=204, f1=240, old="4A", cam="Cam4_GoldDie", lens="18mm",
         panels=[(205, "머리 위로 금색 주사위가 튀어나온다"), (221, "주사위가 도는 동안 환호한다"), (236, "5 눈으로 멈춘다")],
         view="호랑이 쪽에서 여우 상반신과 머리 위 주사위. 아주 조금 물러난다.",
         act="여우 머리 위로 금색 궁극기 주사위가 튀어나와 1.4초 동안 돌다가 5 눈으로 멈추고 터진다(9.9초). 여우는 두 손을 들고 환호한다.",
         face=[("여우", "환희 → 결심 (9.9초)")], fx="주사위 반짝임과 빛기둥, 멈출 때 터지는 빛."),
    dict(n=6, name="호랑이 움찔", f0=240, f1=252, old="4B", cam="Cam4b_TigerStarts", lens="28mm",
         panels=[(242, "호랑이가 움찔한다"), (250, "고개를 젖히며 놀란다")],
         view="호랑이의 망치 쪽 앞에서 얼굴. 고정.",
         act="주사위가 터지자 호랑이가 움찔하며 고개를 젖힌다.",
         face=[("호랑이", "깜짝")], fx="없음."),
    dict(n=7, name="분신", f0=252, f1=300, old="5", cam="Cam5_Clones", lens="24mm",
         panels=[(253, "호랑이와 여우가 마주 선다"), (256, "연기가 터진다"), (290, "분신들이 호랑이를 에워싼다")],
         view="골목 입구 쪽 높은 자리에서 내려다본다. 고정.",
         act="연기가 터지며(10.6초) 여우 분신 다섯이 나타나, 진짜 여우까지 여섯이 호랑이를 둥글게 에워싼다. 호랑이는 좌우를 두리번거린다.",
         face=[("여우", "결심 → 웃음 (10.6초)"), ("호랑이", "깜짝 → 긴장 (10.6초)")], fx="분신 연기: 가운데 큰 것 하나, 자리마다 하나씩."),
    dict(n=8, name="포위", f0=300, f1=360, old="6", cam="Cam6_Surrounded", lens="18mm",
         panels=[(302, "여우들이 권총을 겨눈다"), (310, "호랑이가 망치를 떨어뜨린다"), (352, "방패 뒤로 웅크린다")],
         view="높은 곳에서 거의 수직으로 내려다본다. 고정.",
         act="여우 여섯이 일제히 권총을 겨눈다. 호랑이는 망치를 떨어뜨리고(12.5~12.8초) 방패 뒤로 웅크린다.",
         face=[("여우", "웃음"), ("호랑이", "긴장")], fx="없음."),
    dict(n=9, name="씩 웃는 여우", f0=360, f1=408, old="7", cam="Cam7_Grin", lens="40mm",
         panels=[(362, "웅크린 호랑이 너머로 여우 얼굴"), (383, "눈을 한 번 깜빡인다"), (400, "권총을 겨눈 채 씩 웃는다")],
         view="여우 턱 높이에서 살짝 올려다본다. 천천히 다가간다. 화면 앞에 호랑이 귀와 방패가 흐리게 걸친다.",
         act="여우가 권총을 겨눈 채 씩 웃는다. 눈을 한 번 깜빡인다(15.9초).",
         face=[("여우", "웃음 (눈 깜빡 15.9~16.05초)")], fx="이 컷에만 여우 얼굴용 조명을 쓴다."),
]


def sf(fr):
    return f"{fr // FPS}+{fr % FPS:02d}"


def sec(fr):
    return f"{fr / FPS:.1f}"


e = html.escape
segs = "\n".join(
    f'      <a class="seg" href="#cut-{c["n"]}" style="flex-grow:{c["f1"] - c["f0"]}" aria-label="컷 {c["n"]} {c["name"]}"><b>{c["n"]}</b></a>'
    for c in cuts)
ticks = "\n".join(f'      <span style="flex-grow:{c["f1"] - c["f0"]}">{sec(c["f0"])}</span>' for c in cuts)
rows = "\n".join(
    f'        <tr><td class="num"><a href="#cut-{c["n"]}">{c["n"]}</a></td><td>{e(c["name"])}</td>'
    f'<td class="num">{sec(c["f0"])}~{sec(c["f1"])}</td><td class="num">{sf(c["f1"] - c["f0"])}</td><td class="num">{c["lens"]}</td>'
    f'<td>{" · ".join(e(w + " " + x) for w, x in c["face"])}</td></tr>' for c in cuts)
blocks = []
for c in cuts:
    pan = "\n".join(
        f'          <figure><img src="img/c{c["n"]}_{i}.jpg" alt="컷 {c["n"]}-{i}: {e(cap)}" loading="lazy" width="960" height="540">'
        f'<figcaption><span class="tag">{c["n"]}-{i}</span><span class="cap">{e(cap)}</span><span class="tc">{fr / FPS:.2f}초</span></figcaption></figure>'
        for i, (fr, cap) in enumerate(c["panels"], 1))
    faces = "".join(f'<span class="who {"fox" if w == "여우" else "tiger"}">{w}</span> {e(x)}<br>' for w, x in c["face"])
    blocks.append(f'''    <article class="cut" id="cut-{c["n"]}">
      <div class="slate">
        <div class="no"><span>컷</span>{c["n"]}</div>
        <div class="title">{e(c["name"])}</div>
        <dl class="time">
          <div><dt>구간</dt><dd>{sec(c["f0"])}~{sec(c["f1"])}초</dd></div>
          <div><dt>길이</dt><dd>{sf(c["f1"] - c["f0"])}</dd></div>
          <div><dt>렌즈</dt><dd>{c["lens"]}</dd></div>
        </dl>
        <div class="old">이전 문서의 컷 {c["old"]} · {c["cam"]}</div>
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
        </dl>
      </div>
    </article>''')

CSS = '''
/* layout: a storyboard sheet. A running-time strip on top, then one row per cut: a slate column (number, time,
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
'''

n_panels = sum(len(c["panels"]) for c in cuts)
total = cuts[-1]["f1"]
page = f'''<title>여우 vs 호랑이 콘티</title>
<meta name="claude:viewport-fit-sides" content="cover">
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Barlow+Condensed:wght@600;700&family=Noto+Sans+KR:wght@400;500;700&display=swap">
<style>{CSS}</style>

<div class="wrap">
  <header>
    <div class="eyebrow">RRRANDOM · 시네마틱 콘티 · 2026-11-23</div>
    <h1>여우 vs 호랑이 콘티</h1>
    <p class="lead">지금 시네마틱(최종 영상 8판)을 컷 순서대로 정리했습니다. 컷은 1번부터 {len(cuts)}번까지이고, 그림에는 컷 번호와 순번을 붙였습니다(예: 3-2는 컷 3의 둘째 그림). 고칠 곳은 이 번호로 가리키시면 됩니다.</p>
    <ul class="facts">
      <li><b>{len(cuts)}</b>컷</li>
      <li><b>{n_panels}</b>그림</li>
      <li><b>{total // FPS}</b>초</li>
      <li><b>{total}</b>프레임 (초당 {FPS})</li>
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
    <p class="lead small">칸의 너비가 컷의 길이입니다. 아래 숫자는 컷이 시작하는 시각(초)이고, 번호를 누르면 그 컷으로 갑니다.</p>
    <video class="film" controls muted playsinline preload="metadata" src="FoxVsTiger_web_v8.mp4"></video>
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
    <p class="lead small">길이는 콘티 용지 표기대로 초+프레임입니다(2+10 = 2초 10프레임).</p>
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
        <li>그림은 최종 영상 8판의 프레임입니다. 그림 아래 오른쪽 숫자가 그 프레임의 시각입니다.</li>
        <li>음악과 효과음은 아직 없어서 소리 칸은 두지 않았습니다.</li>
        <li>이전 문서와 작업 기록은 추격을 1A·1B, 주사위를 4A·4B로 나눠 컷 1~7로 불렀습니다. 각 컷의 번호 칸 아래에 그 이름과 카메라 이름을 적어 두었습니다.</li>
        <li>표정 이름은 여우 놀람·걱정·결심·웃음·환희, 호랑이 접근·깜짝·긴장입니다.</li>
      </ul>
    </div>
  </section>
</div>
'''
out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "page", "index.html")
io.open(out, "w", encoding="utf-8", newline="\n").write(page)
print("BOARD", out, len(page), "chars,", len(cuts), "cuts,", n_panels, "panels")
