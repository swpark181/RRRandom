import re, sys, collections
# Round 99: bot stall statistics from a bot match log (rr.Bot.LogStalls 1)
text = open(sys.argv[1], encoding='utf-8', errors='ignore').read()
stalls = re.findall(r'bot stall: (\S+) \((\w+)\) at V\(([^)]*)\) for [\d.]+ s, wants V\(([^)]*)\), speed (\d+) \(max (\d+)\), (on ground|in the air), route (\d+)/(\d+) \(([^)]*)\) next V\(([^)]*)\), blocked by (\S+)', text)
def vec(s):
    d = dict(p.split('=') for p in s.split(', ') if '=' in p)
    return float(d.get('X', 0)), float(d.get('Y', 0)), float(d.get('Z', 0))
modes = collections.Counter(); air = 0; high = 0; blocked = collections.Counter(); away = 0
for name, mode, at, wants, speed, mx, where, ri, rn, src, nxt, by in stalls:
    modes[mode] += 1
    air += where == 'in the air'
    a = vec(at); n = vec(nxt); w = vec(wants)
    high += a[2] > 130
    blocked[by] += 1
    dx, dy = n[0] - a[0], n[1] - a[1]
    l = (dx * dx + dy * dy) ** 0.5 or 1
    if mode == 'route' and (dx * w[0] + dy * w[1]) / l < 0.3:
        away += 1
matches = text.count('RRRandom: match over')
secs = re.findall(r'\[(\d+)\.(\d+)\.(\d+)-(\d+)\.(\d+)\.(\d+):', text)
print(f'matches {matches}, stalls {len(stalls)}, unsticks {text.count("bot unstick")}, modes {dict(modes)}, in the air {air}, feet high (z>130) {high}, route but walking away from next {away}, blocked {dict(blocked)}')
walk = {}
for name, w, s, p in re.findall(r'bot walk: (\S+) walked ([\d.]+) s, stopped ([\d.]+) s, pushing at a wall ([\d.]+) s', text):
    walk[name] = (float(w), float(s), float(p))
if walk:
    W = sum(v[0] for v in walk.values()); S = sum(v[1] for v in walk.values()); P = sum(v[2] for v in walk.values())
    print(f'walked {W:.0f} s, stopped {S:.0f} s ({100*S/W:.1f}%), pushing at a wall {P:.0f} s ({100*P/W:.1f}%)')
hitches = [int(x) for x in re.findall(r'hitch, a frame of (\d+) ms', text)]
print(f'hitches over the limit: {len(hitches)}, worst {sorted(hitches)[-5:]}')
