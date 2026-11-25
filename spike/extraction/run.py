"""Runs the whole matrix and writes raw rows to results.tsv (spike, ticket 09)."""
import itertools
import os
import subprocess
import sys

here = os.path.dirname(os.path.abspath(__file__))
exe = os.path.join(here, '..', '..', 'build', 'desktop-test', 'spike', 'extraction', 'Release', 'extraction_bench.exe')
out = open(os.path.join(here, 'results.tsv'), 'w', encoding='utf-8', newline='\n')
scenes = ['flat_inline', 'flat_shared', 'mixed', 'groups_same', 'groups_diff', 'groups_inherit', 'chains']
configs = []
for s, n in itertools.product(scenes, (5200, 20000)):
    for t in (0, 4):
        configs.append((s, n, t, []))
for s, n in itertools.product(('mixed', 'flat_shared'), (5200, 20000)):
    for t in (2, 8, 16):
        configs.append((s, n, t, []))
for s, n, t in itertools.product(('mixed', 'groups_diff'), (5200, 20000), (0, 4)):
    configs.append((s, n, t, ['--cold', '--iters', '60']))
for n, t in itertools.product((5200, 20000), (0, 4)):
    configs.append(('mixed', n, t, ['--physics']))

# Repeat the whole matrix rather than each row, so a slow spell hits different rows.
for rep in range(3):
    for s, n, t, extra in configs:
        cmd = [exe, '--scene', s, '--entities', str(n), '--threads', str(t)] + extra
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode != 0:
            print('FAILED', rep, s, n, t, extra, r.returncode, r.stderr[-300:], flush=True)
            continue
        for line in r.stdout.splitlines():
            out.write(f'{rep}\t{line}\n')
        out.flush()
    print('repeat', rep, 'done', flush=True)
sys.exit(0)
