"""Segunda pasada: bloques no encontrados, probando desplazamientos y
tratando los punteros de ROM (0x08xxxxxx) como comodines."""
import json, sys, glob, collections, re
from pathlib import Path
here = Path(__file__).resolve().parent
us = Path(sys.argv[1]).read_bytes(); es = Path(sys.argv[2]).read_bytes()
rep = open(here / 'informe.txt', encoding='utf-8').read().splitlines()
deltas = [int(d) for d in re.findall(r'\((-?\d+), \d+\)', rep[2])] + [0]
def score(off, n, d):
    if off + d < 0 or off + d + n > len(es): return 0
    a = us[off:off+n]; b = es[off+d:off+d+n]; ok = 0; i = 0
    while i < n:
        if i % 4 == 0 and i + 4 <= n and a[i+3] in (8, 9) and b[i+3] in (8, 9):
            ok += 4; i += 4; continue
        ok += a[i] == b[i]; i += 1
    return ok / n
cls = collections.Counter(); byts = collections.Counter(); out = []
for line in rep[4:]:
    p = line.split('\t')
    if len(p) != 5 or p[4] in ('movido', 'ambiguo'): continue
    off = int(p[2], 16); n = int(p[3])
    best = max((score(off, n, d), d) for d in deltas)
    k = 'ptr_ok' if best[0] >= 0.98 else 'parcial' if best[0] >= 0.6 else 'distinto'
    cls[k] += 1; byts[k] += n
    out.append('%s\t0x%x\t%d\t%.3f\t%d\t%s' % (p[0], off, n, best[0], best[1], k))
with open(here / 'informe2.txt', 'w', encoding='utf-8') as w:
    w.write('%s\n%s\n' % (dict(cls), dict(byts))); w.write('\n'.join(out))
print(dict(cls), dict(byts))
for l in out:
    if not l.endswith('ptr_ok'): print(l)
