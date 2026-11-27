"""Tail chain clips (2026-11-24): cuts a recording (clips/<tag>.mp4 + .txt from rec_tail.ps1) on the game's events.
    python cut_tail.py times <tag>                      prints where each case starts in the clip (seconds)
    python cut_tail.py sheet <tag> <out.jpg> <game seconds> ...   frames at those game times, cropped round the middle
    python cut_tail.py pair <name> [slow]               clips/<name>_off + <name>_on -> out/<name>.mp4: the four cases
                                                        one after another, before on the left and after on the right
    python cut_tail.py grid <name> [slow]               the same from two cameras: the side view above, the view from
                                                        above and behind below (takes recorded with both passes)
The four cases are parts of the recorded sequence, in game seconds (see SEQ in record_all.ps1).
"""
import datetime
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
FF = "C:/work/char-pipeline/tools/ffmpeg/ffmpeg.exe"
FONT = "C\\:/Windows/Fonts/malgunbd.ttf"
CASES = [("걷기", 4.8, 10.6), ("뛰기", 10.8, 16.3), ("점프", 16.3, 22.3), ("사격", 22.3, 29.6)]
CROP = "crop=1280:720:320:230"       # the animal friend stands a little below the middle of the 1920x1080 picture
TOP_CROP = "crop=1280:720:280:240"   # ...and from above and behind (the second pass, TOP_LATER seconds after the first)
TOP_LATER = 27.0


def clock(text):
    h, m, s, ms = (int(v) for v in re.split(r"[.:]", text))
    return h * 3600 + m * 60 + s + ms / 1000.0


def anchor(tag):
    """Seconds into the clip at which the game's clock read 0."""
    lines = open(os.path.join(HERE, "clips", tag + ".txt"), encoding="utf-8-sig").read().splitlines()
    began = clock(lines[0].split()[1])
    for line in lines[1:]:
        m = re.match(r"\[\d+\.\d+\.\d+-(\d+\.\d+\.\d+:\d+)\]\[\s*\d+\]LogPython: GAMESEQ (\d+\.\d+) frame \d+ ", line)
        if m:
            return clock(m.group(1)) - began - float(m.group(2))
    raise SystemExit(f"{tag}: no command in its log")


def run(args):
    subprocess.run([FF, "-y", "-loglevel", "error"] + args, check=True)


mode = sys.argv[1]
if mode == "times":
    t0 = anchor(sys.argv[2])
    print(sys.argv[2], "game 0 at", round(t0, 2), [(n, round(t0 + a, 2), round(t0 + b, 2)) for n, a, b in CASES])
elif mode == "sheet":
    tag, out = sys.argv[2], sys.argv[3]
    t0 = anchor(tag)
    times = [float(v) for v in sys.argv[4:]]
    ins, parts = [], []
    for i, t in enumerate(times):
        ins += ["-ss", f"{t0 + t:.3f}", "-i", os.path.join(HERE, "clips", tag + ".mp4")]
        parts.append(f"[{i}]{CROP},scale=480:270,drawtext=fontfile='{FONT}':text='{t:.2f}':x=6:y=6:fontsize=18:fontcolor=white:box=1:boxcolor=black@0.5[s{i}]")
    cols = 4
    layout = "|".join(f"{(i % cols) * 480}_{(i // cols) * 270}" for i in range(len(times)))
    run(ins + ["-filter_complex", ";".join(parts) + ";" + "".join(f"[s{i}]" for i in range(len(times))) + f"xstack=inputs={len(times)}:layout={layout}:fill=black",
               "-frames:v", "1", "-q:v", "3", out])
elif mode == "grid":
    name = sys.argv[2]
    slow = float(sys.argv[3]) if len(sys.argv) > 3 else 1.0
    os.makedirs(os.path.join(HERE, "out"), exist_ok=True)
    offs = {side: anchor(f"{name}_{side}") for side in ("off", "on")}
    pieces = []
    for k, (label, a, b) in enumerate(CASES):
        piece = os.path.join(HERE, "out", f"_{name}_{k}.mp4")
        tag = lambda text, x, y: f"drawtext=fontfile='{FONT}':text='{text}':x={x}:y={y}:fontsize=22:fontcolor=white:box=1:boxcolor=black@0.55:boxborderw=6"   # noqa: E731
        ins = []
        for later in (0.0, TOP_LATER):
            for side in ("off", "on"):
                ins += ["-ss", f"{offs[side] + a + later:.3f}", "-t", f"{b - a:.3f}", "-i", os.path.join(HERE, "clips", f"{name}_{side}.mp4")]
        run(ins + ["-filter_complex",
                   f"[0]{CROP},scale=640:360[a];[1]{CROP},scale=640:360[b];[2]{TOP_CROP},scale=640:360[c];[3]{TOP_CROP},scale=640:360[d];"
                   f"[a][b][c][d]xstack=inputs=4:layout=0_0|640_0|0_360|640_360,"
                   f"{tag('전 · ' + label + ' · 옆', 10, 10)},{tag('후 · ' + label + ' · 옆', 650, 10)},{tag('전 · ' + label + ' · 위', 10, 370)},{tag('후 · ' + label + ' · 위', 650, 370)},"
                   f"setpts={slow}*PTS,fps=30,format=yuv420p",
                   "-c:v", "libx264", "-crf", "23", "-an", piece])
        pieces.append(piece)
    listing = os.path.join(HERE, "out", f"_{name}.txt")
    open(listing, "w", encoding="utf-8").write("".join(f"file '{p}'\n" for p in pieces))
    out = os.path.join(HERE, "out", f"{name}.mp4")
    run(["-f", "concat", "-safe", "0", "-i", listing, "-c", "copy", "-movflags", "+faststart", out])
    for p in pieces + [listing]:
        os.remove(p)
    print(out, os.path.getsize(out))
elif mode == "pair":
    name = sys.argv[2]
    slow = float(sys.argv[3]) if len(sys.argv) > 3 else 1.0
    os.makedirs(os.path.join(HERE, "out"), exist_ok=True)
    offs = {side: anchor(f"{name}_{side}") for side in ("off", "on")}
    pieces = []
    for k, (label, a, b) in enumerate(CASES):
        piece = os.path.join(HERE, "out", f"_{name}_{k}.mp4")
        tag = lambda text, x: f"drawtext=fontfile='{FONT}':text='{text}':x={x}:y=14:fontsize=28:fontcolor=white:box=1:boxcolor=black@0.55:boxborderw=8"   # noqa: E731
        run(["-ss", f"{offs['off'] + a:.3f}", "-t", f"{b - a:.3f}", "-i", os.path.join(HERE, "clips", f"{name}_off.mp4"),
             "-ss", f"{offs['on'] + a:.3f}", "-t", f"{b - a:.3f}", "-i", os.path.join(HERE, "clips", f"{name}_on.mp4"),
             "-filter_complex", f"[0]{CROP},scale=960:540[a];[1]{CROP},scale=960:540[b];[a][b]hstack,{tag('전 · ' + label, 14)},{tag('후 · ' + label, 974)},"
                                f"setpts={slow}*PTS,fps=30,format=yuv420p",
             "-c:v", "libx264", "-crf", "23", "-an", piece])
        pieces.append(piece)
    listing = os.path.join(HERE, "out", f"_{name}.txt")
    open(listing, "w", encoding="utf-8").write("".join(f"file '{p}'\n" for p in pieces))
    out = os.path.join(HERE, "out", f"{name}.mp4")
    run(["-f", "concat", "-safe", "0", "-i", listing, "-c", "copy", "-movflags", "+faststart", out])
    for p in pieces + [listing]:
        os.remove(p)
    print(out, os.path.getsize(out))
