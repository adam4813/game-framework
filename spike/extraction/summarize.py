"""Median over the repeats of each row's median (us), with the low and high repeat. Usage: summarize.py [results.tsv]"""
import collections
import os
import statistics
import sys

path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), 'results.tsv')
rows = collections.defaultdict(list)
for line in open(path, encoding='utf-8'):
    f = line.rstrip('\n').split('\t')
    if len(f) != 9:
        continue
    _, scene, n, t, flags, step, med, p95, low = f
    rows[(step, scene, int(n), flags, int(t))].append((float(med), float(p95)))

steps = list(dict.fromkeys(k[0] for k in rows))
for step in steps:
    print('==', step)
    configs = sorted({(k[3], k[2], k[1]) for k in rows if k[0] == step}, reverse=False)
    for flags, n, scene in configs:
        cells = []
        for t in (0, 2, 4, 8, 16):
            v = rows.get((step, scene, n, flags, t))
            if not v:
                continue
            meds = [m for m, _ in v]
            cells.append(f't{t} {statistics.median(meds):.0f} [{min(meds):.0f}-{max(meds):.0f}] x{len(v)}')
        print(f'  {flags:13} {n:6} {scene:15} ' + ' | '.join(cells))
