"""Tiger / cat tail motion references (2026-11-25, the user: "호랑이 꼬리 관련해서 영상자료 찾아줘. 고양이 꼬리도 괜찮아. 10가지
정도 찾아줘. 영상도 바로 볼 수 있게 옆에 띄워주고.").

Reads videos.json beside this file (a web research pass; every video's YouTube oEmbed was fetched, so it exists and may
be embedded) and writes:
  player.html  the list on the left, the chosen video playing on the right (YouTube embed). Open it from a local web
               server (serve.sh: http://127.0.0.1:8790/player.html) - YouTube refuses embeds on file:// pages.
  links.html   the same list with links only, for the published page (a published page may not embed other sites).
Run with any Python 3.
"""
import html
import io
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
e = html.escape
data = json.load(io.open(os.path.join(HERE, "videos.json"), encoding="utf-8"))
videos = data["videos"]
# (2026-11-25, the user: "게임이나 3d 영상 기준으로 찾아줘. 실제 영상은 그대로 쓰기 힘들어." - videos.json now holds game and 3D
# references only; the first live-action list is kept as videos_live.json)
KIND = {"game": "게임", "3d film": "3D 영화·시리즈", "3d rig/demo": "3D 리그·데모", "real tiger": "실제 호랑이",
        "real cat": "실제 고양이", "animation": "애니메이션 참고"}
ORDER = ["game", "3d film", "3d rig/demo", "real tiger", "real cat", "animation"]
videos.sort(key=lambda v: ORDER.index(v["kind"]) if v["kind"] in ORDER else 9)

CSS = """
:root { --bg: #f6f3ee; --sheet: #fffdf9; --ink: #2b2118; --soft: #6e5d4c; --line: #e4dbcf; --accent: #d9772b;
  --accent-soft: #fbe9d8; --sel: #fff1e2; --body: "Noto Sans KR", system-ui, sans-serif; --head: "Gowun Dodum", "Noto Sans KR", sans-serif; }
@media (prefers-color-scheme: dark) { :root:not([data-theme="light"]) { --bg: #1b1713; --sheet: #241e18; --ink: #f3e9dc;
  --soft: #bba893; --line: #3a3027; --accent: #f0954a; --accent-soft: #3a2a1c; --sel: #33261a; color-scheme: dark } }
:root[data-theme="dark"] { --bg: #1b1713; --sheet: #241e18; --ink: #f3e9dc; --soft: #bba893; --line: #3a3027;
  --accent: #f0954a; --accent-soft: #3a2a1c; --sel: #33261a; color-scheme: dark }
* { box-sizing: border-box; }
body { margin: 0; background: var(--bg); color: var(--ink); font: 15px/1.55 var(--body); }
header { padding-block: 18px 10px; padding-left: max(16px, env(safe-area-inset-left, 0px)); padding-right: max(16px, env(safe-area-inset-right, 0px)); }
h1 { font: 700 24px/1.25 var(--head); margin: 0 0 4px; text-wrap: balance; }
.lead { color: var(--soft); margin: 0; max-width: 70ch; }
.kind { font-size: 12px; font-weight: 700; letter-spacing: .04em; color: var(--accent); background: var(--accent-soft);
  border-radius: 4px; padding: 1px 7px; }
.item { display: flex; flex-direction: column; gap: 4px; padding: 12px 14px; border: 1px solid var(--line);
  border-radius: 8px; background: var(--sheet); text-align: left; color: var(--ink); font: inherit; min-width: 0; }
.item b { font-size: 15px; }
.by { font-size: 12px; color: var(--soft); }
.what { font-size: 14px; }
.use { font-size: 13px; color: var(--soft); }
a { color: var(--accent); }
"""


def item_body(i, v):
    start = int(v.get("start") or 0)
    return (f'<span><span class="kind">{e(KIND.get(v["kind"], v["kind"]))}</span> <span class="by">{i}. {e(v.get("channel", ""))}'
            f'{" · " + str(start // 60) + ":" + format(start % 60, "02d") + "부터" if start else ""} · 확신 {e(v.get("confidence", ""))}</span></span>'
            f'<b>{e(v["title"])}</b><span class="what">{e(v.get("what", ""))}</span><span class="use">우리 호랑이에: {e(v.get("use", ""))}</span>')


# ---- the local player ----
items = []
for i, v in enumerate(videos, 1):
    items.append(f'<button class="item" type="button" data-id="{e(v["id"])}" data-start="{int(v.get("start") or 0)}">{item_body(i, v)}</button>')
player = f"""<!doctype html>
<meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>호랑이 꼬리 영상 자료</title>
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Gowun+Dodum&family=Noto+Sans+KR:wght@400;700&display=swap">
<style>{CSS}
main {{ display: grid; grid-template-columns: minmax(300px, 420px) minmax(0, 1fr); gap: 16px; padding-block: 0 20px;
  padding-left: max(16px, env(safe-area-inset-left, 0px)); padding-right: max(16px, env(safe-area-inset-right, 0px)); }}
.list {{ display: flex; flex-direction: column; gap: 8px; max-height: calc(100vh - 110px); overflow-y: auto; padding-right: 4px; }}
.item {{ cursor: pointer; }}
.item:hover, .item:focus-visible {{ border-color: var(--accent); outline: none; }}
.item[aria-pressed="true"] {{ background: var(--sel); border-color: var(--accent); }}
.stage {{ position: sticky; top: 12px; align-self: start; display: flex; flex-direction: column; gap: 10px; min-width: 0; }}
.frame {{ width: 100%; aspect-ratio: 16 / 9; border-radius: 10px; overflow: hidden; background: #000; }}
.frame iframe {{ width: 100%; height: 100%; border: 0; display: block; }}
#now {{ font-weight: 700; }}
@media (max-width: 820px) {{ main {{ grid-template-columns: minmax(0, 1fr); }} .stage {{ position: static; order: -1; }} .list {{ max-height: none; }} }}
</style>
<header><h1>호랑이 꼬리 영상 자료</h1>
<p class="lead">게임과 3D 애니메이션 속 호랑이·고양이 꼬리 {len(videos)}편. 왼쪽에서 고르면 오른쪽에서 바로 재생됩니다(꼬리가 잘 보이는 시점부터).</p></header>
<main>
  <div class="list" role="list">{''.join(items)}</div>
  <div class="stage"><div class="frame"><iframe id="player" title="선택한 영상" allow="autoplay; encrypted-media; picture-in-picture; fullscreen" allowfullscreen></iframe></div>
    <div id="now"></div><a id="out" href="#" target="_blank" rel="noopener">YouTube에서 열기</a></div>
</main>
<script>
const items = [...document.querySelectorAll('.item')];
function play(btn, auto) {{
  items.forEach(b => b.setAttribute('aria-pressed', b === btn ? 'true' : 'false'));
  const id = btn.dataset.id, s = btn.dataset.start;
  document.getElementById('player').src = 'https://www.youtube-nocookie.com/embed/' + id + '?rel=0&start=' + s + (auto ? '&autoplay=1&mute=1' : '');
  document.getElementById('now').textContent = btn.querySelector('b').textContent;
  document.getElementById('out').href = 'https://www.youtube.com/watch?v=' + id + (s > 0 ? '&t=' + s + 's' : '');
}}
items.forEach(b => b.addEventListener('click', () => play(b, true)));
if (items.length) play(items[0], false);
</script>
"""
io.open(os.path.join(HERE, "player.html"), "w", encoding="utf-8", newline="\n").write(player)

# ---- the published list (links only) ----
cards = []
for i, v in enumerate(videos, 1):
    start = int(v.get("start") or 0)
    url = f'https://www.youtube.com/watch?v={v["id"]}' + (f"&t={start}s" if start else "")
    cards.append(f'<div class="item">{item_body(i, v)}<a href="{e(url)}" target="_blank" rel="noopener">YouTube에서 보기</a></div>')
notes = f'<p class="lead">{e(data.get("notes", ""))}</p>' if data.get("notes") else ""
links = f"""<title>호랑이 꼬리 영상 자료</title>
<meta name="claude:viewport-fit-sides" content="cover">
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Gowun+Dodum&family=Noto+Sans+KR:wght@400;700&display=swap">
<style>{CSS}
main {{ display: grid; grid-template-columns: repeat(auto-fill, minmax(min(100%, 340px), 1fr)); gap: 10px; padding-block: 4px 24px;
  padding-left: max(16px, env(safe-area-inset-left, 0px)); padding-right: max(16px, env(safe-area-inset-right, 0px)); }}
footer {{ padding-block: 0 24px; padding-left: max(16px, env(safe-area-inset-left, 0px)); padding-right: max(16px, env(safe-area-inset-right, 0px)); }}
</style>
<header><h1>호랑이 꼬리 영상 자료</h1>
<p class="lead">게임과 3D 애니메이션 속 호랑이·고양이 꼬리 {len(videos)}편. 이 페이지에는 다른 사이트 영상을 넣을 수 없어 링크로 둡니다(꼬리가 잘 보이는 시점부터 열립니다). 목록 옆에서 바로 재생되는 판은 작업 PC의 로컬 페이지로 따로 띄웠습니다.</p></header>
<main>{''.join(cards)}</main>
<footer>{notes}</footer>
"""
io.open(os.path.join(HERE, "links.html"), "w", encoding="utf-8", newline="\n").write(links)
print("wrote player.html and links.html,", len(videos), "videos")
