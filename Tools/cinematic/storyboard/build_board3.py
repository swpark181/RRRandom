# -*- coding: utf-8 -*-
"""Proposal page (2026-11-23, the user: "지금 영상에서 긴장감을 올리면서 금색 주사위가 나왔을 때 좀 더 극적이고 유머러스한
상황으로 ... 콘티로 일단 손봐야 할 부분 아주 간단하게 구도만 잡아서 설명해줘. 페이지로."): which parts of the re-cut draft
(page2, 25 s) to rework, as hand-drawn composition sketches (inline SVG, not game renders) with a few lines each.
Writes page3/index.html (and page3/preview.html, the same with a charset line, for looking at it locally).
Run with any Python 3:   python build_board3.py
"""
import html
import io
import math
import os
import shutil

HERE = os.path.dirname(os.path.abspath(__file__))
e = html.escape
FOX, FOX_D, CREAM, EAR_IN, DARK = "#f07a1c", "#a8500f", "#fbeedd", "#f7d3ae", "#2b1a12"
TIG, TIG_D, STRIPE = "#f2a21f", "#8a560c", "#2b1a12"
RED, YEL, GOLD, GOLD_D = "#e0403a", "#f2c230", "#f6cf55", "#b98510"
WALL, WALL_L, FLOOR, NIGHT = "#3d3862", "#544d84", "#3f5a46", "#1d1a2c"
W, H = 480, 270


def poly(pts, fill, extra=""):
    return f'<polygon points="{" ".join(f"{x:.1f},{y:.1f}" for x, y in pts)}" fill="{fill}" {extra}/>'


def eye(kind, ex, ey, s, look=(0.0, 0.0), side=1):
    """One eye. side: -1 the left one on the picture, 1 the right one (for the shapes that mirror)."""
    w = max(1.2, s * 0.32)
    if kind == "wide":
        return (f'<circle cx="{ex:.1f}" cy="{ey:.1f}" r="{s * 1.15:.1f}" fill="#fff" stroke="{DARK}" stroke-width="{w * 0.5:.1f}"/>'
                f'<circle cx="{ex + look[0] * s * 0.55:.1f}" cy="{ey + look[1] * s * 0.55:.1f}" r="{s * 0.5:.1f}" fill="{DARK}"/>')
    if kind == "shut":      # squeezed shut: > <
        return (f'<path d="M{ex - side * s:.1f},{ey - s * 0.75:.1f} L{ex + side * s * 0.7:.1f},{ey:.1f} L{ex - side * s:.1f},{ey + s * 0.75:.1f}" '
                f'fill="none" stroke="{DARK}" stroke-width="{w:.1f}" stroke-linecap="round" stroke-linejoin="round"/>')
    if kind == "joy":
        return (f'<path d="M{ex - s:.1f},{ey + s * 0.4:.1f} Q{ex:.1f},{ey - s * 1.2:.1f} {ex + s:.1f},{ey + s * 0.4:.1f}" fill="none" stroke="{DARK}" '
                f'stroke-width="{w:.1f}" stroke-linecap="round"/>')
    if kind == "smug":      # half-closed
        return (f'<path d="M{ex - s * 1.15:.1f},{ey:.1f} A{s * 1.15:.1f} {s * 0.85:.1f} 0 0 0 {ex + s * 1.15:.1f},{ey:.1f} Z" fill="#fff"/>'
                f'<circle cx="{ex + look[0] * s * 0.5:.1f}" cy="{ey + s * 0.3:.1f}" r="{s * 0.38:.1f}" fill="{DARK}"/>'
                f'<path d="M{ex - s * 1.3:.1f},{ey:.1f} L{ex + s * 1.3:.1f},{ey:.1f}" stroke="{DARK}" stroke-width="{w:.1f}" stroke-linecap="round"/>')
    return ""


def brows(kind, x, y, r, s):
    if not kind:
        return ""
    up = -1 if kind == "worried" else 1          # worried: the inner ends go up; set: they come down
    out = ""
    for side in (-1, 1):
        ex = x + side * r * 0.42
        out += (f'<path d="M{ex + side * s * 1.3:.1f},{y - r * 0.08 - s * (1.95 - up * 0.35):.1f} L{ex - side * s * 0.9:.1f},{y - r * 0.08 - s * (1.95 + up * 0.35):.1f}" '
                f'stroke="{DARK}" stroke-width="{max(1.4, s * 0.38):.1f}" stroke-linecap="round"/>')
    return out


def fox_head(x, y, r, eyes="wide", look=(0.0, 0.0), brow="worried", mouth=None, body=FOX, back=False):
    s = r * 0.2
    out = ""
    for side in (-1, 1):
        out += poly([(x + side * r * 0.82, y - r * 0.25), (x + side * r * 1.02, y - r * 1.5), (x + side * r * 0.18, y - r * 0.72)], body, f'stroke="{DARK}" stroke-width="{r * 0.04:.1f}" stroke-linejoin="round"')
        if not back:
            out += poly([(x + side * r * 0.74, y - r * 0.5), (x + side * r * 0.9, y - r * 1.22), (x + side * r * 0.4, y - r * 0.75)], EAR_IN)
    out += f'<ellipse cx="{x:.1f}" cy="{y:.1f}" rx="{r:.1f}" ry="{r * 0.84:.1f}" fill="{body}" stroke="{DARK}" stroke-width="{r * 0.04:.1f}"/>'
    if back:
        return out
    out += f'<ellipse cx="{x:.1f}" cy="{y + r * 0.45:.1f}" rx="{r * 0.66:.1f}" ry="{r * 0.36:.1f}" fill="{CREAM}"/>'
    out += f'<ellipse cx="{x + look[0] * r * 0.06:.1f}" cy="{y + r * 0.3:.1f}" rx="{r * 0.12:.1f}" ry="{r * 0.085:.1f}" fill="{DARK}"/>'
    kinds = ("shut", "wide") if eyes == "one" else (eyes, eyes)
    for side, kind in zip((-1, 1), kinds):
        out += eye(kind, x + side * r * 0.42, y - r * 0.08, s, look, side)
    out += brows(brow, x, y, r, s)
    if mouth == "grin":
        out += f'<path d="M{x - r * 0.34:.1f},{y + r * 0.48:.1f} Q{x:.1f},{y + r * 0.82:.1f} {x + r * 0.38:.1f},{y + r * 0.42:.1f}" fill="none" stroke="{DARK}" stroke-width="{r * 0.06:.1f}" stroke-linecap="round"/>'
    elif mouth == "wavy":
        out += (f'<path d="M{x - r * 0.3:.1f},{y + r * 0.56:.1f} l{r * 0.12:.1f},{-r * 0.07:.1f} l{r * 0.12:.1f},{r * 0.07:.1f} l{r * 0.12:.1f},{-r * 0.07:.1f} l{r * 0.12:.1f},{r * 0.07:.1f} l{r * 0.12:.1f},{-r * 0.07:.1f}" '
                f'fill="none" stroke="{DARK}" stroke-width="{r * 0.05:.1f}" stroke-linecap="round" stroke-linejoin="round"/>')
    elif mouth == "o":
        out += f'<ellipse cx="{x:.1f}" cy="{y + r * 0.58:.1f}" rx="{r * 0.1:.1f}" ry="{r * 0.13:.1f}" fill="{DARK}"/>'
    return out


def tiger_head(x, y, r, eyes="smug", look=(0.0, 0.0), brow="set", mouth=None, body=TIG, face=True):
    s = r * 0.19
    out = ""
    for side in (-1, 1):
        out += f'<circle cx="{x + side * r * 0.72:.1f}" cy="{y - r * 0.78:.1f}" r="{r * 0.3:.1f}" fill="{body}" stroke="{DARK}" stroke-width="{r * 0.04:.1f}"/>'
        if face:
            out += f'<circle cx="{x + side * r * 0.72:.1f}" cy="{y - r * 0.76:.1f}" r="{r * 0.15:.1f}" fill="{CREAM}"/>'
    out += f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{r:.1f}" fill="{body}" stroke="{DARK}" stroke-width="{r * 0.04:.1f}"/>'
    if not face:
        return out
    for dx in (-0.26, 0.0, 0.26):                  # stripes on the brow, and two on each cheek
        out += poly([(x + (dx - 0.09) * r, y - r * 0.97), (x + (dx + 0.09) * r, y - r * 0.97), (x + dx * r, y - r * 0.58)], STRIPE)
    for side in (-1, 1):
        for dy in (0.08, 0.34):
            out += poly([(x + side * r * 0.98, y + (dy - 0.08) * r), (x + side * r * 0.98, y + (dy + 0.08) * r), (x + side * r * 0.62, y + dy * r)], STRIPE)
    out += f'<ellipse cx="{x:.1f}" cy="{y + r * 0.46:.1f}" rx="{r * 0.5:.1f}" ry="{r * 0.34:.1f}" fill="{CREAM}"/>'
    out += poly([(x - r * 0.12 + look[0] * r * 0.05, y + r * 0.26), (x + r * 0.12 + look[0] * r * 0.05, y + r * 0.26), (x + look[0] * r * 0.05, y + r * 0.4)], "#d8667a")
    for side in (-1, 1):
        out += eye(eyes, x + side * r * 0.4, y - r * 0.1, s, look, side)
    out += brows(brow, x, y - r * 0.02, r * 0.95, s)
    if mouth == "grin":
        out += f'<path d="M{x - r * 0.3:.1f},{y + r * 0.52:.1f} Q{x:.1f},{y + r * 0.84:.1f} {x + r * 0.36:.1f},{y + r * 0.46:.1f}" fill="none" stroke="{DARK}" stroke-width="{r * 0.06:.1f}" stroke-linecap="round"/>'
    elif mouth == "o":
        out += f'<ellipse cx="{x:.1f}" cy="{y + r * 0.62:.1f}" rx="{r * 0.13:.1f}" ry="{r * 0.16:.1f}" fill="{DARK}"/>'
    return out


def hammer(x, y, rot, s=1.0, dim=False):
    """The squeaky hammer: (x, y) is the grip, the head 62 away along the handle (rot 0 = head up)."""
    red, yel = ("#7c2622", "#8a6e1c") if dim else (RED, YEL)
    return (f'<g transform="translate({x:.1f},{y:.1f}) rotate({rot:.1f}) scale({s:.2f})">'
            f'<rect x="-3.5" y="-62" width="7" height="66" rx="3" fill="{yel}" stroke="{DARK}" stroke-width="1"/>'
            f'<rect x="-26" y="-84" width="52" height="30" rx="8" fill="{red}" stroke="{DARK}" stroke-width="1.2"/>'
            f'<rect x="-26" y="-84" width="9" height="30" rx="4" fill="{yel}" opacity=".85"/><rect x="17" y="-84" width="9" height="30" rx="4" fill="{yel}" opacity=".85"/></g>')


def shield(x, y, r, dim=False):
    red, yel = ("#7c2622", "#8a6e1c") if dim else (RED, YEL)
    return (f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{r:.1f}" fill="{red}" stroke="{DARK}" stroke-width="1.2"/><circle cx="{x:.1f}" cy="{y:.1f}" r="{r * 0.62:.1f}" fill="none" stroke="{yel}" stroke-width="{r * 0.14:.1f}"/>'
            f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{r * 0.22:.1f}" fill="{yel}"/>')


PIPS = {1: [(0, 0)], 5: [(0, 0), (-1, -1), (1, -1), (-1, 1), (1, 1)]}


def die(x, y, s, rot=0.0, face=5, glow=1.0, key="g"):
    out = f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{s * (1.5 + glow):.1f}" fill="url(#glow-{key})"/>' if glow else ""
    out += f'<g transform="translate({x:.1f},{y:.1f}) rotate({rot:.1f})"><rect x="{-s / 2:.1f}" y="{-s / 2:.1f}" width="{s:.1f}" height="{s:.1f}" rx="{s * 0.2:.1f}" fill="{GOLD}" stroke="{GOLD_D}" stroke-width="{max(1.2, s * 0.05):.1f}"/>'
    for px, py in PIPS[face]:
        out += f'<circle cx="{px * s * 0.26:.1f}" cy="{py * s * 0.26:.1f}" r="{s * 0.085:.1f}" fill="#6b4a05"/>'
    return out + "</g>"


def arrow(pts, key, color="#fff", width=2.6, dash=""):
    d = "M" + " L".join(f"{x:.1f},{y:.1f}" for x, y in pts) if isinstance(pts, list) else pts
    return (f'<path d="{d}" fill="none" stroke="{color}" stroke-width="{width}" stroke-linecap="round" stroke-linejoin="round" '
            f'{f"stroke-dasharray={chr(34)}{dash}{chr(34)}" if dash else ""} marker-end="url(#ah-{key})"/>')


def label(x, y, s, size=13, anchor="start", color="#fff"):
    return (f'<text x="{x:.1f}" y="{y:.1f}" font-size="{size}" font-weight="700" text-anchor="{anchor}" fill="{color}" stroke="#14121e" stroke-width="3.2" '
            f'paint-order="stroke" stroke-linejoin="round">{e(s)}</text>')


def blocks(x0, y0, x1, y1, size=42, fill=WALL, line="#2a2646"):
    """A wall of town blocks, face on."""
    out = f'<rect x="{x0}" y="{y0}" width="{x1 - x0}" height="{y1 - y0}" fill="{fill}"/>'
    y, row = y0, 0
    while y < y1:
        out += f'<path d="M{x0},{y} H{x1}" stroke="{line}" stroke-width="1.5"/>'
        x = x0 + (size / 2 if row % 2 else 0)
        while x < x1:
            out += f'<path d="M{x:.0f},{y} V{min(y1, y + size)}" stroke="{line}" stroke-width="1.5"/>'
            x += size
        y += size
        row += 1
    return out


def svg(key, body, alt):
    return (f'<svg class="sk" viewBox="0 0 {W} {H}" role="img" aria-label="{e(alt)}"><defs>'
            f'<marker id="ah-{key}" viewBox="0 0 10 10" refX="7" refY="5" markerWidth="5.5" markerHeight="5.5" orient="auto-start-reverse"><path d="M0,0 L10,5 L0,10 Z" fill="context-stroke"/></marker>'
            f'<radialGradient id="glow-{key}"><stop offset="0" stop-color="#ffe9a0" stop-opacity=".95"/><stop offset=".45" stop-color="#ffd257" stop-opacity=".4"/><stop offset="1" stop-color="#ffd257" stop-opacity="0"/></radialGradient>'
            f'<linearGradient id="back-{key}" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#f1c27a"/><stop offset="1" stop-color="#8c5f4e"/></linearGradient>'
            f'<clipPath id="clip-{key}"><rect width="{W}" height="{H}"/></clipPath></defs><g clip-path="url(#clip-{key})">{body}</g></svg>')


# ------------------------------------------------------------------------------------------------------------ the sketches
def sk_top():
    """3-end: straight down. The fox boxed in on three sides; the tiger's shadow comes in from the open side."""
    b = f'<rect width="{W}" height="{H}" fill="{FLOOR}"/>'
    for i in range(1, 12):
        b += f'<path d="M{i * 42},0 V{H}" stroke="#2f4636" stroke-width="1.2"/>'
    for j in range(1, 7):
        b += f'<path d="M0,{j * 42} H{W}" stroke="#2f4636" stroke-width="1.2"/>'
    b += poly([(W, 48), (W, 222), (150, 158), (150, 112)], "#ffcf8a", 'opacity=".2"')                 # light from the mouth
    b += blocks(0, 0, W, 48, 48, WALL_L) + blocks(0, 222, W, H, 48, WALL_L) + blocks(0, 48, 56, 222, 44, WALL_L)
    # the shadow: a long body, a round head with ears, the hammer on its shoulder
    b += '<g fill="#0d0b18" opacity=".72">' + poly([(W, 116), (W, 154), (166, 150), (166, 120)], "#0d0b18")
    b += '<circle cx="150" cy="135" r="24"/><circle cx="140" cy="111" r="9"/><circle cx="140" cy="159" r="9"/><rect x="196" y="96" width="34" height="20" rx="6"/></g>'
    # the tiger at the mouth, from above
    b += f'<circle cx="452" cy="135" r="21" fill="{TIG}" stroke="{DARK}" stroke-width="1.2"/><circle cx="441" cy="116" r="7" fill="{TIG}" stroke="{DARK}"/><circle cx="441" cy="154" r="7" fill="{TIG}" stroke="{DARK}"/>'
    b += "".join(poly([(452 + d, 118), (458 + d, 118), (455 + d, 135)], STRIPE) for d in (-9, 0, 9))
    # the fox against the dead end, from above
    b += f'<ellipse cx="72" cy="168" rx="16" ry="7" fill="{FOX}" transform="rotate(35 72 168)"/><circle cx="62" cy="176" r="6" fill="{CREAM}"/>'
    b += f'<circle cx="84" cy="135" r="15" fill="{FOX}" stroke="{DARK}" stroke-width="1.2"/>' + poly([(90, 122), (104, 116), (98, 130)], FOX_D) + poly([(90, 148), (104, 154), (98, 140)], FOX_D)
    b += arrow([(290, 186), (150, 186)], "top") + label(290, 205, "그림자가 먼저 닿는다", 13, "end")
    b += label(70, 100, "여우", 12, "middle", "#ffd0a8") + label(452, 100, "호랑이", 12, "middle", "#ffe0a0") + label(14, 250, "바로 위에서 · 고정", 13)
    return svg("top", b, "바로 위에서 내려다본 골목. 세 면이 막힌 구석에 작은 여우, 열린 쪽에서 호랑이의 긴 그림자가 여우 쪽으로 뻗는다")


def sk_loom():
    """4-2: over the fox's shoulder, low. The tiger does not stop: he fills the frame, his shadow covers the fox."""
    b = f'<rect width="{W}" height="{H}" fill="{NIGHT}"/><rect x="150" y="34" width="180" height="196" fill="url(#back-loom)"/>'
    b += poly([(0, 0), (150, 34), (150, 230), (0, H)], WALL) + poly([(W, 0), (330, 34), (330, 230), (W, H)], WALL)
    for t in (0.25, 0.5, 0.75):
        b += f'<path d="M0,{H * t:.0f} L150,{34 + 196 * t:.0f} M{W},{H * t:.0f} L330,{34 + 196 * t:.0f}" stroke="#2a2646" stroke-width="1.5"/>'
    b += poly([(0, H), (150, 230), (330, 230), (W, H)], FLOOR)
    b += poly([(196, 238), (300, 238), (410, H), (60, H)], "#0d0b18", 'opacity=".7"')                       # his shadow, towards the lens
    b += f'<ellipse cx="248" cy="176" rx="64" ry="74" fill="#241c2c" stroke="{TIG}" stroke-width="2"/>'
    b += hammer(296, 150, 28, 1.05, True) + f'<path d="M286,150 L302,128" stroke="#241c2c" stroke-width="20" stroke-linecap="round"/>'
    b += shield(190, 184, 36, True)
    b += tiger_head(248, 86, 46, "smug", (-0.6, 0.6), "set", "grin", "#b9781a")
    b += f'<circle cx="248" cy="86" r="46" fill="none" stroke="{TIG}" stroke-width="2.2"/>'
    # the fox's head and shoulder in the foreground, out of focus, in his shadow
    b += '<g opacity=".93">' + fox_head(92, 252, 74, body="#7c3d0e", back=True) + "</g>"
    b += arrow([(372, 150), (372, 214)], "loom") + label(384, 176, "멈추지 않고", 13) + label(384, 194, "코앞까지", 13)
    b += label(14, 22, "여우 어깨 너머 · 낮게 · 천천히 다가감", 13)
    return svg("loom", b, "여우 뒤통수 너머로 낮게 본 골목. 역광 속 호랑이가 화면을 채우며 다가오고 그림자가 여우 쪽으로 깔린다")


def sk_raise():
    """4-3a: the fox's view, straight up. The hammer goes up slowly and covers the light."""
    b = f'<rect width="{W}" height="{H}" fill="#55466a"/><circle cx="240" cy="44" r="120" fill="url(#glow-raise)"/>'
    b += poly([(0, 0), (176, 0), (104, H), (0, H)], WALL) + poly([(W, 0), (304, 0), (376, H), (W, H)], WALL)
    for t in (0.3, 0.6):
        b += f'<path d="M0,{H * t:.0f} L{176 - 72 * t:.0f},{H * t:.0f} M{W},{H * t:.0f} L{304 + 72 * t:.0f},{H * t:.0f}" stroke="#2a2646" stroke-width="1.5"/>'
    b += f'<ellipse cx="240" cy="262" rx="158" ry="104" fill="#241c2c" stroke="{TIG}" stroke-width="2"/>'       # the body, foreshortened
    b += f'<path d="M172,188 L214,84 M308,188 L266,84" stroke="#241c2c" stroke-width="26" stroke-linecap="round"/>'
    b += f'<path d="M172,188 L214,84 M308,188 L266,84" stroke="{TIG}" stroke-width="1.6" stroke-linecap="round" fill="none" opacity=".7"/>'
    b += f'<rect x="236" y="62" width="8" height="52" rx="3" fill="#8a6e1c"/><rect x="176" y="18" width="128" height="52" rx="14" fill="#8e2a26" stroke="{DARK}" stroke-width="1.4"/>'
    b += f'<rect x="176" y="18" width="20" height="52" rx="8" fill="#8a6e1c"/><rect x="284" y="18" width="20" height="52" rx="8" fill="#8a6e1c"/>'
    b += tiger_head(240, 148, 40, "smug", (0, 0.9), "set", "grin", "#b9781a")
    b += arrow([(410, 150), (410, 54)], "raise") + label(398, 112, "천천히", 13, "end") + label(398, 130, "든다", 13, "end")
    b += label(14, 256, "여우 눈높이에서 올려다봄 · 망치가 빛을 가린다", 13)
    return svg("raise", b, "여우가 올려다본 호랑이. 두 팔로 망치를 머리 위로 들어 올려 뒤의 빛을 가린다")


def sk_eyes():
    """4-3b: the fox's eyes, very close; the shadow comes over the face; the eyes squeeze shut."""
    b = blocks(0, 0, W, H, 60, WALL)
    b += fox_head(240, 176, 236, "shut", brow="worried", mouth="wavy")
    b += poly([(150, 0), (W, 0), (W, H), (330, H)], "#0d0b18", 'opacity=".42"')                             # the shadow, with the hammer's corner
    b += '<rect x="96" y="-10" width="120" height="58" rx="14" fill="#0d0b18" opacity=".42" transform="rotate(-12 156 20)"/>'
    b += f'<path d="M396,118 q-9,15 0,24 q9,-9 0,-24 Z" fill="#bfe6ff" stroke="{DARK}" stroke-width="1"/>'
    b += label(14, 256, "눈만 크게 · 그림자가 얼굴을 덮는다 · 눈을 질끈 감는다", 13)
    return svg("eyes", b, "화면 가득한 여우 얼굴. 망치 그림자가 얼굴을 덮고 여우가 눈을 질끈 감는다")


def sk_bonk():
    """5-1: side on, the three in one line: hammer - die - fox."""
    b = blocks(0, 0, W, 214, 46, WALL) + blocks(0, 0, 44, 214, 46, WALL_L) + f'<rect x="0" y="214" width="{W}" height="56" fill="{FLOOR}"/>'
    # the tiger, leaning in
    b += f'<ellipse cx="356" cy="170" rx="58" ry="60" fill="{TIG}" stroke="{DARK}" stroke-width="1.4"/><ellipse cx="338" cy="182" rx="30" ry="36" fill="{CREAM}"/>'
    b += shield(404, 178, 30)
    b += tiger_head(318, 92, 42, "smug", (-0.9, 0.5), "set", "grin")
    b += f'<path d="M318,150 L246,128" stroke="{TIG}" stroke-width="20" stroke-linecap="round"/>'
    b += '<path d="M330,14 C250,2 190,30 170,74" fill="none" stroke="#fff" stroke-width="2.2" stroke-dasharray="7 7" opacity=".85"/>'
    b += hammer(246, 128, -74, 1.0)
    # the die pops up right over the fox's head, under the blow
    b += die(118, 104, 40, 10, 5, 0.9, "bonk")
    b += poly([(150 + 26 * math.cos(math.radians(a)) * (1.0 if i % 2 else 0.45), 96 + 26 * math.sin(math.radians(a)) * (1.0 if i % 2 else 0.45)) for i, a in enumerate(range(0, 360, 30))], "#fff", f'stroke="{DARK}" stroke-width="1"')
    b += label(150, 62, "뿅!", 26, "middle", "#fff3b0")
    # the fox, curled up small, eyes shut, hands over its head
    b += f'<ellipse cx="104" cy="204" rx="26" ry="22" fill="#f4f0ea" stroke="{DARK}" stroke-width="1.2"/>'
    b += fox_head(106, 168, 27, "shut", brow="worried", mouth="wavy")
    b += f'<path d="M84,188 L78,152 M128,188 L134,152" stroke="{FOX}" stroke-width="9" stroke-linecap="round"/>'
    b += label(14, 256, "옆에서 · 망치 – 주사위 – 여우가 한 줄", 13)
    return svg("bonk", b, "옆에서 본 장면. 내려치는 망치가 웅크린 여우 머리 위에 막 나타난 금색 주사위를 때린다")


def sk_watch():
    """5-2: the die between the two faces, both looking only at it; it rocks between 1 and 5."""
    b = blocks(0, 0, W, H, 54, "#343056")
    b += f'<circle cx="240" cy="92" r="150" fill="url(#glow-watch)"/>'
    b += tiger_head(388, 150, 76, "wide", (-0.9, -0.7), "worried", "o")
    b += fox_head(96, 190, 70, "one", (0.9, -0.8), "worried", "o")
    # the die standing on its edge: the 1 face and the 5 face on either side
    b += poly([(240, 30), (240, 150), (176, 128), (176, 52)], "#dba824", f'stroke="{GOLD_D}" stroke-width="2" stroke-linejoin="round"')
    b += poly([(240, 30), (240, 150), (304, 128), (304, 52)], GOLD, f'stroke="{GOLD_D}" stroke-width="2" stroke-linejoin="round"')
    b += '<circle cx="208" cy="90" r="7" fill="#6b4a05"/>'
    b += "".join(f'<circle cx="{x}" cy="{y}" r="6" fill="#6b4a05"/>' for x, y in ((272, 90), (256, 62), (289, 68), (256, 118), (289, 112)))
    b += arrow("M196,22 Q240,0 284,22", "watch") + '<path d="M284,22 Q240,0 196,22" fill="none" stroke="#fff" stroke-width="2.6" marker-end="url(#ah-watch)"/>'
    b += label(164, 58, "1…?", 17, "end", "#fff3b0") + label(316, 58, "…5!", 17, "start", "#fff3b0")
    b += label(14, 256, "둘 사이 위에 주사위 · 둘 다 주사위만 본다", 13)
    return svg("watch", b, "가운데 위에 모서리로 선 금색 주사위. 왼쪽 아래 여우는 한쪽 눈만 뜨고, 오른쪽 호랑이는 눈이 커져서 둘 다 주사위를 올려다본다")


def sk_burst():
    """5-3: side on, wide. The 5 bursts; the tiger is pushed back to the middle of the alley."""
    b = blocks(0, 0, W, 214, 46, WALL) + blocks(0, 0, 44, 214, 46, WALL_L) + f'<rect x="0" y="214" width="{W}" height="56" fill="{FLOOR}"/>'
    b += f'<circle cx="108" cy="78" r="130" fill="url(#glow-burst)"/>'
    for a in range(-80, 200, 20):
        r0, r1 = 40, 40 + (58 if a % 40 else 84)
        b += f'<path d="M{108 + r0 * math.cos(math.radians(a)):.1f},{78 + r0 * math.sin(math.radians(a)):.1f} L{108 + r1 * math.cos(math.radians(a)):.1f},{78 + r1 * math.sin(math.radians(a)):.1f}" stroke="#ffe9a0" stroke-width="3" stroke-linecap="round"/>'
    b += die(108, 78, 46, 0, 5, 0, "burst")
    b += f'<ellipse cx="100" cy="196" rx="24" ry="28" fill="#f4f0ea" stroke="{DARK}" stroke-width="1.2"/>' + fox_head(102, 152, 27, "wide", (0.8, 0.0), "set", "grin")
    # the tiger skidding back on his heels, shield up
    b += '<g transform="rotate(14 372 200)">' + f'<ellipse cx="372" cy="166" rx="50" ry="56" fill="{TIG}" stroke="{DARK}" stroke-width="1.4"/><ellipse cx="358" cy="176" rx="26" ry="32" fill="{CREAM}"/></g>'
    b += tiger_head(392, 86, 38, "wide", (-0.9, 0.0), "worried", "o") + shield(318, 160, 32) + hammer(430, 150, 150, 0.8)
    for y in (232, 242, 252):
        b += f'<path d="M252,{y} H{318 - (y - 232) * 2}" stroke="#fff" stroke-width="2.4" stroke-linecap="round" opacity=".8"/>'
    b += '<circle cx="330" cy="236" r="9" fill="#fff" opacity=".55"/><circle cx="346" cy="244" r="6" fill="#fff" opacity=".55"/>'
    b += arrow([(226, 190), (292, 190)], "burst") + label(196, 176, "가운데까지 밀려난다", 13)
    b += label(14, 256, "옆에서 넓게 · 터지는 빛, 밀려나는 호랑이", 13)
    return svg("burst", b, "옆에서 넓게 본 장면. 여우 머리 위 주사위가 5로 멈춰 터지고, 그 빛에 호랑이가 방패를 든 채 뒤로 미끄러진다")


# ------------------------------------------------------------------------------------------------------------ the beats
# (tag, where it goes, title, seconds, sketch, composition, what happens, why, the draft's picture it replaces or follows)
BEATS = [
    ("3-끝", "3컷 끝에 추가", "갇혔다", 1.2, sk_top(),
     "바로 위에서 내려다본다. 세 면이 벽인 구석에 여우가 작게 있고, 열린 쪽에서 호랑이의 긴 그림자가 화면 안으로 자라 들어온다.",
     "주저앉은 여우 다음에 바로 붙입니다. 호랑이는 아직 보이지 않고 그림자만 먼저 여우에게 닿습니다.",
     "도망칠 곳이 없다는 것을 한 장으로 보여 줍니다. 호랑이 얼굴보다 그림자가 먼저 오면 4컷의 등장이 더 무겁습니다.", None),
    ("4-2", "4컷 뒤에 추가", "멈추지 않는다", 2.0, sk_loom(),
     "여우 뒤통수 너머, 바닥 가까이에서. 역광 속 호랑이가 화면을 채울 때까지 걸어오고 카메라도 천천히 다가간다.",
     "지금은 호랑이가 골목 가운데(여우에게서 4m)에서 멈춥니다. 그 자리를 지나 여우 코앞까지 오게 합니다. 그의 그림자가 여우를 덮습니다.",
     "거리가 긴장입니다. 4m 떨어져 서 있으면 위협이 멈춘 것처럼 보입니다.", "c4_3.jpg"),
    ("4-3", "4컷 뒤에 추가", "망치를 든다", 2.0, sk_raise(),
     "여우 눈높이에서 올려다본다. 호랑이가 망치를 머리 위로 천천히 들어 뒤의 빛을 가린다.",
     "아래의 눈 클로즈업과 번갈아 보여 줍니다. 컷 길이를 1초, 0.6초, 0.4초로 줄여 갑니다. 망치가 꼭대기에 닿으면 반 박자 멈춥니다.",
     "긴장이 가장 높은 곳입니다. 소리를 넣게 되면 여기서 음악을 끊고 조용하게 둡니다.", None),
    ("4-3", "위와 번갈아", "눈을 질끈 감는다", 0.0, sk_eyes(),
     "여우 눈만 화면 가득. 망치 그림자가 얼굴 위로 넘어온다.",
     "놀란 눈이 떨리다가, 망치가 꼭대기에 닿을 때 질끈 감습니다.",
     "관객이 여우 자리에서 맞을 준비를 하게 됩니다. 표정은 지금 있는 것(눈 감은 걱정)으로 됩니다.", "c5_1.jpg"),
    ("5-1", "5컷 앞부분을 바꿈", "뿅!", 1.0, sk_bonk(),
     "옆에서. 망치, 주사위, 웅크린 여우가 한 줄에 놓인다.",
     "내려치는 바로 그 순간 여우 머리 위에 금색 주사위가 나타나 망치에 대신 맞습니다. 뿅망치 소리와 함께 망치가 튕겨 나가고, 맞은 주사위가 제자리에서 세차게 돌기 시작합니다.",
     "가장 조용한 순간에 장난감 소리가 나서 웃음이 됩니다. 호랑이가 여우의 궁극기 주사위를 직접 굴려 준 셈입니다.", "c5_2.jpg"),
    ("5-2", "5컷 가운데를 바꿈", "1…? 5!", 3.0, sk_watch(),
     "둘의 얼굴 사이, 위쪽에 주사위. 둘 다 주사위만 올려다본다. 금빛이 두 얼굴을 비춘다.",
     "맞을 줄 알았던 여우가 한쪽 눈을 뜨고 서서히 올려다봅니다. 주사위가 느려지다 모서리로 서서 1 쪽으로 기웁니다(호랑이 씩, 여우 울상). 그러다 5로 넘어갑니다(여우 환희, 호랑이 깜짝).",
     "주사위 게임이라서 되는 개그입니다. 분신 수가 주사위 눈이라 1과 5의 차이를 둘 다 압니다. 말씀하신 '서서히 올려다보며 밝아진다'는 여기에 남깁니다.", "c5_2.jpg"),
    ("5-3", "5컷 끝을 바꿈", "터진다", 1.5, sk_burst(),
     "옆에서 넓게. 왼쪽에 터지는 주사위와 일어서는 여우, 오른쪽에 밀려나는 호랑이.",
     "5에서 멈춘 주사위가 터지고, 그 빛에 호랑이가 방패를 든 채 골목 가운데까지 미끄러져 물러납니다. 여우는 웃으며 일어섭니다.",
     "밀려난 자리가 지금 6컷에서 분신이 둘러싸는 자리라서, 6컷부터는 그대로 이어집니다. 6컷의 '움찔'은 여기서 이미 하므로 6컷에서는 두리번거리기만 합니다.", "c5_3.jpg"),
]
NOW_CAP = {"c4_3.jpg": "지금 4컷: 4m 앞에서 멈춘다", "c5_1.jpg": "지금 5컷: 겁에 질린 얼굴", "c5_2.jpg": "지금 5컷: 주사위가 저절로 돈다", "c5_3.jpg": "지금 5컷: 5로 멈추고 터진다"}

out_dir = os.path.join(HERE, "page3")
os.makedirs(os.path.join(out_dir, "img"), exist_ok=True)
for name in NOW_CAP:
    shutil.copyfile(os.path.join(HERE, "page2", "img", name), os.path.join(out_dir, "img", name))

src = io.open(os.path.join(HERE, "build_board.py"), encoding="utf-8").read()
CSS = src[src.index("CSS = '''") + 9:src.index("'''\n\nn_panels")]
CSS += '''
/* the proposal's own parts: hand-drawn composition sketches (fixed picture colours), the reworked strip, the flow */
.sk { width: 100%; height: auto; display: block; border: 1px solid var(--line); border-radius: 4px; font-family: var(--font); background: #1d1a2c; }
.seg.keep { background: var(--sheet); color: var(--soft); }
.seg.new { background: var(--accent); color: var(--sheet); border-color: var(--accent); }
.seg.new:hover, .seg.new:focus-visible { background: var(--ink); }
.seg b { font-size: 18px; white-space: nowrap; }
.key { display: flex; flex-wrap: wrap; gap: 4px 16px; font-size: 12px; color: var(--soft); margin: 0; padding: 0; list-style: none; }
.key i { display: inline-block; width: 12px; height: 12px; border-radius: 3px; border: 1px solid var(--line); vertical-align: -1px; margin-right: 5px; }
.key .kn { background: var(--accent); border-color: var(--accent); } .key .kk { background: var(--sheet); }
.flow { margin: 0; padding-left: 22px; font-size: 15px; }
.flow li + li { margin-top: 6px; }
.flow b { font-weight: 700; }
.beat { display: grid; grid-template-columns: 150px minmax(0, 1fr); background: var(--sheet); border: 1px solid var(--line); border-radius: 8px; overflow: hidden; scroll-margin-top: 12px; }
.beat .no { font-size: 46px; }
.beat .time dt { white-space: nowrap; }
.beat .time dd { white-space: normal; text-align: right; }
.pair { display: grid; grid-template-columns: minmax(0, 11fr) minmax(0, 9fr); gap: 14px 20px; align-items: start; }
.pair .notes { grid-template-columns: minmax(0, 1fr); }
.nowpic { display: grid; grid-template-columns: 132px minmax(0, 1fr); gap: 10px; align-items: center; font-size: 12px; color: var(--soft); border-top: 1px solid var(--line); padding-top: 10px; }
.nowpic img { width: 100%; height: auto; display: block; border: 1px solid var(--line); border-radius: 4px; }
.ask ol { margin: 0; padding-left: 20px; font-size: 14px; }
.ask li + li { margin-top: 8px; }
.ask b { font-weight: 700; }
td.now { color: var(--soft); }
@media (max-width: 720px) {
  .beat { grid-template-columns: minmax(0, 1fr); }
  .pair { grid-template-columns: minmax(0, 1fr); }
}
'''

# the running order: the draft's cuts with the reworked part in place (seconds; "new" = added or changed)
ORDER = [("1", 3.5, 0, ""), ("2", 4.0, 0, ""), ("3", 3.5, 0, ""), ("3-끝", 1.2, 1, "b-1"), ("4", 2.0, 0, ""), ("4-2", 2.0, 1, "b-2"), ("4-3", 2.0, 1, "b-3"),
         ("5-1", 1.0, 1, "b-5"), ("5-2", 3.0, 1, "b-6"), ("5-3", 1.5, 1, "b-7"), ("6", 3.0, 0, ""), ("8", 3.0, 0, ""), ("9", 2.0, 0, "")]
total = sum(o[1] for o in ORDER)
segs = "\n".join((f'      <a class="seg new" href="#{o[3]}" style="flex-grow:{o[1]}"><b>{o[0]}</b></a>' if o[2] else
                  f'      <span class="seg keep" style="flex-grow:{o[1]}"><b>{o[0]}</b></span>') for o in ORDER)

cards = []
for i, (tag, where, title, secs, sketch, comp, what, why, now) in enumerate(BEATS, 1):
    nowpic = (f'\n        <div class="nowpic"><img src="img/{now}" alt="{e(NOW_CAP[now])}" loading="lazy" width="960" height="540"><span>{e(NOW_CAP[now])}</span></div>' if now else "")
    cards.append(f'''    <article class="beat" id="b-{i}">
      <div class="slate">
        <div class="no">{tag}</div>
        <div class="title">{e(title)}</div>
        <dl class="time">
          <div><dt>자리</dt><dd>{e(where)}</dd></div>
          <div><dt>길이</dt><dd>{f"약 {secs:g}초" if secs else "위에 포함"}</dd></div>
        </dl>
      </div>
      <div class="body">
        <div class="pair">
          {sketch}
          <dl class="notes">
            <div><dt>구도</dt><dd>{e(comp)}</dd></div>
            <div><dt>내용</dt><dd>{e(what)}</dd></div>
            <div><dt>이유</dt><dd>{e(why)}</dd></div>
          </dl>
        </div>{nowpic}
      </div>
    </article>''')

page = f'''<title>여우 vs 호랑이 콘티: 긴장과 주사위 장면 손볼 곳</title>
<meta name="claude:viewport-fit-sides" content="cover">
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Barlow+Condensed:wght@600;700&family=Noto+Sans+KR:wght@400;500;700&display=swap">
<style>{CSS}</style>

<div class="wrap">
  <header>
    <div class="eyebrow">RRRANDOM · 시네마틱 콘티 · 손볼 곳 제안 · 2026-11-23</div>
    <h1>긴장과 주사위 장면, 손볼 곳</h1>
    <p class="lead">수정안 초안(25초)에서 긴장을 더 올리고, 금색 주사위가 나오는 순간을 더 극적이고 웃기게 만들려면 어디를 손봐야 하는지 구도만 잡은 제안입니다. 그림은 구도를 보여 주는 스케치이고 게임 화면이 아닙니다.</p>
    <ul class="facts">
      <li>손볼 곳 <b>3</b>군데 (3컷 끝, 4컷, 5컷)</li>
      <li>스케치 <b>{len(BEATS)}</b>장</li>
      <li>길이 <b>25</b>초 → 약 <b>{total:.0f}</b>초</li>
      <li><a href="https://claude.ai/artifact/B9MXkSnKKFxAcNMjusBwmX">수정안 초안 콘티</a></li>
    </ul>
  </header>

  <section>
    <h2>한 줄기로 보면</h2>
    <div class="list">
      <ol class="flow">
        <li><b>호랑이가 멈추지 않는다.</b> 골목 가운데에서 서지 않고 여우 코앞까지 온다. 그림자가 먼저 여우를 덮는다.</li>
        <li><b>망치를 천천히 든다.</b> 여우는 눈을 질끈 감는다. 여기가 가장 조용하고 가장 긴장된 곳.</li>
        <li><b>뿅!</b> 내려치는 순간 여우 머리 위에 금색 주사위가 나타나 대신 맞는다. 호랑이가 여우의 궁극기 주사위를 굴려 준 셈이 된다.</li>
        <li><b>둘이 같이 주사위를 본다.</b> 1 쪽으로 기울다가 5로 넘어간다. 두 얼굴의 표정이 뒤바뀐다.</li>
        <li><b>터진다.</b> 그 빛에 호랑이가 골목 가운데로 밀려나고, 지금의 6컷(분신 포위)으로 그대로 이어진다.</li>
      </ol>
    </div>
  </section>

  <section>
    <h2>어디가 바뀌나</h2>
    <div class="strip">
{segs}
    </div>
    <ul class="key"><li><i class="kn"></i>새로 넣거나 바꾸는 곳 (누르면 그 그림으로)</li><li><i class="kk"></i>그대로 두는 컷</li></ul>
    <p class="lead small">칸의 너비는 어림한 길이입니다. 4컷의 등장(그림자를 지나 얼굴이 보인다)은 3초에서 2초로 줄이고 그 뒤에 4-2, 4-3을 붙입니다. 5컷 3초는 5-1, 5-2, 5-3으로 바뀝니다.</p>
  </section>

  <section>
    <h2>손볼 곳</h2>
    <div class="cuts">
{chr(10).join(cards)}
    </div>
  </section>

  <section>
    <h2>그대로 두는 곳</h2>
    <div class="tablebox">
      <table>
        <thead><tr><th class="num">컷</th><th>내용</th><th>손대는 것</th></tr></thead>
        <tbody>
        <tr><td class="num">1</td><td>얼굴 클로즈업, 뒤돌아보기, 줌 아웃</td><td class="now">없음</td></tr>
        <tr><td class="num">2</td><td>장애물을 짚고 올라 구르며 내려온다</td><td>구도는 그대로. 호랑이 망치가 여우 꼬리 바로 뒤에 떨어지도록 시점만 당기면 추격이 더 급해집니다.</td></tr>
        <tr><td class="num">3</td><td>벽에 부딪히고 돌아서며 주저앉는다</td><td class="now">없음 (끝에 3-끝을 붙임)</td></tr>
        <tr><td class="num">6</td><td>일어서서 분신으로 넓게 둘러싼다</td><td>호랑이 움찔은 5-3으로 옮기고, 여기서는 두리번거리기만 합니다.</td></tr>
        <tr><td class="num">8</td><td>한 걸음씩 좁힌다, 망치를 놓친다</td><td class="now">없음</td></tr>
        <tr><td class="num">9</td><td>씩 웃는 여우</td><td class="now">없음</td></tr>
        </tbody>
      </table>
    </div>
  </section>

  <section class="ask">
    <h2>정해 주실 것</h2>
    <div class="list">
      <ol>
        <li><b>망치가 주사위를 때리는 설정.</b> 게임에서는 궁극기 주사위가 머리 위에서 저절로 돕니다. 여기서는 망치에 맞아서 돌기 시작합니다. 게임과 다르게 가도 되는지요.</li>
        <li><b>5컷 지시가 달라지는 점.</b> 말씀하신 "주사위를 서서히 올려다보며 표정이 밝아진다"는 5-2에 남겼지만, 그 앞에 눈을 감고 움츠리는 구간이 생기고 밝아지는 순간이 5가 나올 때로 늦춰집니다.</li>
        <li><b>1과 5 사이에서 흔들리는 장면.</b> 넣으면 3초쯤 듭니다. 빼면 주사위가 바로 5로 멈추고 터집니다.</li>
        <li><b>한쪽 눈만 뜬 표정.</b> 지금 표정 그림에 없어서 새로 그려야 합니다. 안 그리면 눈 감음에서 걱정으로 바로 넘깁니다.</li>
        <li><b>길이.</b> 약 {total:.0f}초가 됩니다. 2컷, 6컷, 8컷을 조금씩 줄이면 30초 안팎으로 맞출 수 있습니다.</li>
      </ol>
    </div>
  </section>

  <section>
    <h2>만들 때 필요한 것</h2>
    <div class="list">
      <ul>
        <li>호랑이: 내려치기는 게임의 망치 휘두르기(머리 위에서 내려침)를 씁니다. 천천히 들기, 망치가 튕겨 나가는 팔, 뒤로 밀려나기는 있는 동작에 뼈 회전을 얹어 새로 만들어야 합니다.</li>
        <li>여우: 움츠리기는 지금의 앉은 동작에 팔을 올려 만듭니다. 표정은 눈 감은 걱정, 놀람, 환희, 결심이 이미 있습니다.</li>
        <li>주사위: 지금도 키로 움직이므로 세차게 돌기, 모서리로 서서 흔들리기, 5로 멈추기는 키만 더 넣으면 됩니다.</li>
        <li>조명: 그림자가 여우를 덮는 것은 4컷의 입구 뒤 낮은 조명을 그대로 씁니다.</li>
      </ul>
    </div>
  </section>
</div>
'''
io.open(os.path.join(out_dir, "index.html"), "w", encoding="utf-8", newline="\n").write(page)
io.open(os.path.join(out_dir, "preview.html"), "w", encoding="utf-8", newline="\n").write('<meta charset="utf-8">\n' + page)
print(f"BOARD3 {os.path.join(out_dir, 'index.html')} {len(page)} chars, {len(BEATS)} sketches, about {total:.1f} s")
