# usage: net_stats.py <log> [<log> ...]   (round 100)
# Sums up the "net smooth" lines of rr.Net.LogSmooth: for bodies that were moving (speed over 150), how far the drawn body
# and the raw capsule jumped beyond their own speed, per second and per frame.
import re
import sys

PAT = re.compile(r"net smooth (\S+): body worst (\d+), mean ([\d.]+), (\d+) jumps; capsule worst (\d+), mean ([\d.]+), (\d+) jumps; (\d+) frames, trouble ([\d.]+), speed (\d+)")
for path in sys.argv[1:]:
    rows = []
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = PAT.search(line)
            if m:
                rows.append([m.group(1)] + [float(x) for x in m.groups()[1:]])
    moving = [r for r in rows if r[9] > 150]
    if not moving:
        print(f"{path}: {len(rows)} lines, none moving")
        continue
    n = len(moving)
    frames = sum(r[7] for r in moving)
    worst_b = sorted(r[1] for r in moving)
    worst_c = sorted(r[4] for r in moving)
    print(f"{path}: {n} body-seconds moving ({len(rows)} in all), {frames / n:.0f} frames a second")
    print(f"  drawn body: worst jump in a second median {worst_b[n // 2]:.0f}, 90% {worst_b[int(n * 0.9)]:.0f}, max {worst_b[-1]:.0f};"
          f" mean per frame {sum(r[2] * r[7] for r in moving) / frames:.2f}; jumping frames {sum(r[3] for r in moving) / frames * 100:.1f}%")
    print(f"  raw capsule: worst jump in a second median {worst_c[n // 2]:.0f}, 90% {worst_c[int(n * 0.9)]:.0f}, max {worst_c[-1]:.0f};"
          f" mean per frame {sum(r[5] * r[7] for r in moving) / frames:.2f}; jumping frames {sum(r[6] for r in moving) / frames * 100:.1f}%")
    print(f"  trouble mean {sum(r[8] for r in moving) / n:.3f}")
