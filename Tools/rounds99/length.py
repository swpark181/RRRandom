import re, sys, statistics as st, collections
# Round 99: how long the main mode's rounds and matches run in bot matches, and how much a player gets to do in one.
# Game time from the log's frame counter (it wraps at 1000; the bot matches run at a fixed 30 frames a second).
FPS = 30.0
rounds = []   # dicts
matches = []
for path in sys.argv[1:]:
    total = 0; prev = None; cur = None; match_rounds = []
    for line in open(path, encoding='utf-8', errors='ignore'):
        m = re.match(r'\[[\d.:-]+\]\[\s*(\d+)\]LogTemp: RRRandom: (.*)', line)
        if not m:
            continue
        f = int(m.group(1))
        if prev is not None:
            total += (f - prev) % 1000
        prev = f
        t = total / FPS
        s = m.group(2)
        r = re.match(r'ROUND (\d+) starts: the rule die shows \d+, ([A-Z ]+) \(', s)
        if r:
            cur = dict(rule=r.group(2).strip(), start=t, kos=[], cards=0, abilities=0, waits=[], rolls=0, players=set(), file=path)
            continue
        if s.startswith('match over'):
            if match_rounds:
                matches.append(match_rounds)
            match_rounds = []
            continue
        if cur is None:
            continue
        if 'knocked out by' in s:
            cur['kos'].append(t - cur['start'])
        elif re.search(r'keeps the \w+ card', s):
            cur['cards'] += 1
        elif re.search(r'\) uses [A-Z]', s):
            cur['abilities'] += 1
        w = re.search(r'waits ([\d.]+) s to get back up', s)
        if w:
            cur['waits'].append(float(w.group(1)))
        e = re.match(r'ROUND \d+ \(([A-Z ]+)\) goes to team (\d) at (\d+) : (\d+); rounds (\d+) - (\d+); played ([\d.]+) s', s)
        if e:
            cur.update(length=float(e.group(7)), score=(int(e.group(3)), int(e.group(4))))
            rounds.append(cur); match_rounds.append(cur); cur = None
def q(xs, p):
    xs = sorted(xs); return xs[min(len(xs) - 1, int(p * len(xs)))]
print(f'{len(rounds)} rounds in {len(matches)} matches (6 bots, 3 a side)')
for rule in sorted({r["rule"] for r in rounds}):
    rs = [r for r in rounds if r['rule'] == rule]
    L = [r['length'] for r in rs]
    first = [r['kos'][0] - 6.2 for r in rs if r['kos']]   # after the rule die and countdown (6.2 s)
    kos = [len(r['kos']) for r in rs]
    cards = [r['cards'] / 6 for r in rs]; abil = [r['abilities'] / 6 for r in rs]
    dead = [sum(r['waits']) / (6 * r['length']) for r in rs]
    margin = [abs(r['score'][0] - r['score'][1]) for r in rs]
    print(f'{rule}: n={len(rs)}, length median {st.median(L):.0f} s (min {min(L):.0f}, 25% {q(L, .25):.0f}, 75% {q(L, .75):.0f}, max {max(L):.0f}); '
          f'first KO after {st.median(first):.0f} s; KOs/round {st.mean(kos):.1f}; per player: cards {st.mean(cards):.1f}, abilities {st.mean(abil):.1f}, '
          f'knocked out {st.mean(kos) / 6:.1f} times, down {100 * st.mean(dead):.0f}% of the round; score margin median {st.median(margin):.0f}')
ML = [sum(r['length'] for r in m) + 6.2 * len(m) + 5 * len(m) for m in matches]
print(f'matches: rounds per match {st.mean(len(m) for m in matches):.2f} (2 rounds: {sum(len(m) == 2 for m in matches)}, 3 rounds: {sum(len(m) == 3 for m in matches)}); '
      f'match length with intros median {st.median(ML) / 60:.1f} min (min {min(ML) / 60:.1f}, max {max(ML) / 60:.1f}); play time only median {st.median(sum(r["length"] for r in m) for m in matches) / 60:.1f} min')
