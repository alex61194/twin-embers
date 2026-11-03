"""Segunda pasada de es_sizes: propaga posiciones por tramos contiguos.
Si en la ROM USA el simbolo B empieza justo donde acaba A, en la espanola
B empieza tras el 0xFF de A (y al reves hacia atras)."""
import json, sys, collections
from pathlib import Path
here = Path(__file__).resolve().parent
es = Path(sys.argv[2]).read_bytes()
d = json.load(open(here / 'es_sizes.json'))
rows = sorted(d['rows'], key=lambda r: r[5])   # por offset USA
def strlen(t):
    e = es.find(b'\xff', t); return e - t + 1
changed = True; passes = 0
while changed and passes < 50:
    changed = False; passes += 1
    for i in range(len(rows) - 1):
        a, b = rows[i], rows[i + 1]
        if a[5] + a[4] != b[5]: continue
        if a[6] is not None and b[6] is None:
            b[6] = a[6] + a[7]; b[7] = strlen(b[6]); b[8] = -1; changed = True
        elif b[6] is not None and a[6] is None and b[6] > 0 and es[b[6] - 1] == 0xff:
            j = b[6] - 2
            while j >= 0 and es[j] != 0xff: j -= 1
            a[6] = j + 1; a[7] = b[6] - a[6]; a[8] = -2; changed = True
# comprobacion: tramo contiguo en USA debe ser contiguo en ES
bad = 0
for i in range(len(rows) - 1):
    a, b = rows[i], rows[i + 1]
    if a[5] + a[4] == b[5] and a[6] is not None and b[6] is not None and a[8] > 0 and b[8] > 0 and a[6] + a[7] != b[6]:
        bad += 1
st = collections.Counter('sin' if r[6] is None else 'puntero' if r[8] > 0 else 'tramo' for r in rows)
print(passes, dict(st), 'tramos rotos:', bad)
miss = collections.Counter(r[0] for r in rows if r[6] is None); print(miss.most_common(10))
grow = sum((r[7] or 0) - r[4] for r in rows); print('bytes ES - USA en estos simbolos:', grow)
json.dump({'rows': rows}, open(here / 'es_sizes2.json', 'w'))
