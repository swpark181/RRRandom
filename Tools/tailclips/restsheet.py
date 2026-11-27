# restsheet.py <tag> <out.jpg> <crop> <t1> <t2> ...   big crops of single frames, 3 a row (times in game seconds)
import os, re, subprocess, sys, datetime
HERE = r"C:\work\char-pipeline\output\video\tail"
FF = r"C:\work\char-pipeline\tools\ffmpeg\ffmpeg.exe"
def clock(t):
    h, m, s, ms = re.split(r"[.:]", t)
    return int(h) * 3600 + int(m) * 60 + int(s) + int(ms) / 1000
tag, out, crop = sys.argv[1:4]
lines = open(os.path.join(HERE, "clips", tag + ".txt"), encoding="utf-8-sig").read().splitlines()
began = clock(lines[0].split()[1])
t0 = None
for line in lines[1:]:
    m = re.match(r"\[\d+\.\d+\.\d+-(\d+\.\d+\.\d+:\d+)\]\[\s*\d+\]LogPython: GAMESEQ (\d+\.\d+) frame \d+ ", line)
    if m:
        t0 = clock(m.group(1)) - began - float(m.group(2)); break
times = [float(v) for v in sys.argv[4:]]
ins, parts = [], []
for i, t in enumerate(times):
    ins += ["-ss", f"{t0 + t:.3f}", "-i", os.path.join(HERE, "clips", tag + ".mp4")]
    parts.append(f"[{i}]crop={crop},scale=600:600[s{i}]")
layout = "|".join(f"{(i % 3) * 600}_{(i // 3) * 600}" for i in range(len(times)))
subprocess.run([FF, "-y", "-loglevel", "error"] + ins + ["-filter_complex", ";".join(parts) + ";" + "".join(f"[s{i}]" for i in range(len(times))) + f"xstack=inputs={len(times)}:layout={layout}:fill=black", "-frames:v", "1", "-q:v", "3", out], check=True)
