import re, sys, statistics
# Round 99: how long ore lay before a bot picked it up (Meteor Rush bot matches)
t = open(sys.argv[1], encoding='utf-8', errors='ignore').read()
ages = [float(a) for a in re.findall(r'picks up a \w+; carries \d+; it was ([\d.]+) s old', t)]
rounds = re.findall(r'ROUND \d+ \(METEOR RUSH\) goes to team (\d) at (\d+) : (\d+); rounds [\d\s-]+; played ([\d.]+) s', t)
lands = len(re.findall(r'a meteor lands', t))
print(f'pickups {len(ages)}, median age {statistics.median(ages):.1f} s, mean {statistics.mean(ages):.1f} s, over 10 s: {sum(a > 10 for a in ages)} ({100 * sum(a > 10 for a in ages) / len(ages):.0f}%), over 20 s: {sum(a > 20 for a in ages)}; meteors landed {lands}; rounds {len(rounds)}, mean length {statistics.mean(float(r[3]) for r in rounds):.0f} s, scores {[(r[1], r[2]) for r in rounds]}')
