"""Tamanos de los textos de la ROM espanola (BPRS) por simbolo de la receta USA.
Solo escribe tamanos y posiciones; nunca bytes de la ROM.
Metodo: punteros. Para cada simbolo de texto, busca en la ROM USA las palabras que
apuntan a el, lleva ese sitio a la ROM espanola con el mapa de desplazamientos
(anclas de 16 bytes) y lee alli el puntero espanol. El tamano es hasta el 0xFF."""
import json, sys, glob, bisect, collections, hashlib
from pathlib import Path
here = Path(__file__).resolve().parent
us = Path(sys.argv[1]).read_bytes(); es = Path(sys.argv[2]).read_bytes()
assert hashlib.sha1(us).hexdigest() == '41cb23d8dccc8ebd7c649cd8fbb58eeace6e2fdc'
N = len(us)
# 1. mapa de desplazamientos
idx = {}
for p in range(0, len(es) - 16, 4):
    w = es[p:p+16]
    if w.count(w[0]) == 16: continue
    idx[w] = -1 if w in idx else p
anch_pos, anch_d = [], []
for p in range(0, N - 16, 32):
    w = us[p:p+16]
    if w.count(w[0]) == 16: continue
    q = idx.get(w, -1)
    if q >= 0: anch_pos.append(p); anch_d.append(q - p)
del idx
def delta(p):
    i = bisect.bisect_right(anch_pos, p) - 1
    if i < 0: return None
    j = i + 1
    if j < len(anch_pos) and anch_d[j] != anch_d[i] and anch_pos[j] - p < p - anch_pos[i]:
        return anch_d[j]
    return anch_d[i]
# 2. unidades de la receta
E = [e for f in sorted(glob.glob(str(here / 'recipe' / 'entries-*.json'))) for e in json.load(open(f))['entries']]
U = [u for f in sorted(glob.glob(str(here / 'recipe' / 'units-*.json'))) for u in json.load(open(f))['units']]
def us_off(entry, off):
    pos = 0
    for op in entry['ops']:
        if op[0] == 'C':
            if pos <= off < pos + op[2]: return op[1] + off - pos
            pos += op[2]
        elif op[0] == 'F': pos += op[2]
        else: return None
    return None
targets = {}
for u in U:
    obj, sym, ei, off, size = u
    if ei >= len(E): continue
    o = us_off(E[ei], off)
    if o is None or us[o:o+size] == es[o:o+size]: continue
    # solo cadenas: terminan en 0xFF
    if size == 0 or us[o + size - 1] != 0xff: continue
    targets[o] = u
# 3. sitios de puntero en la ROM USA
sites = collections.defaultdict(list)
for p in range(0, N - 3, 4):
    if us[p+3] == 8:
        v = int.from_bytes(us[p:p+4], 'little') - 0x08000000
        if v in targets: sites[v].append(p)
rows, stats = [], collections.Counter()
for o, u in sorted(targets.items()):
    votes = collections.Counter()
    for p in sites.get(o, []):
        d = delta(p)
        if d is None: continue
        q = p + d
        if 0 <= q <= len(es) - 4 and es[q+3] == 8:
            votes[int.from_bytes(es[q:q+4], 'little') - 0x08000000] += 1
    if not votes:
        stats['sin_puntero'] += 1; rows.append(u + [o, None, None, 0]); continue
    t, v = votes.most_common(1)[0]
    end = es.find(b'\xff', t)
    n = end - t + 1 if end >= 0 else None
    stats['ok' if v == sum(votes.values()) else 'dudoso'] += 1
    rows.append(u + [o, t, n, v])
json.dump({'stats': dict(stats), 'rows': rows}, open(here / 'es_sizes.json', 'w'))
print(len(anch_pos), 'anclas;', len(targets), 'simbolos de texto;', dict(stats))
by = collections.Counter(); miss = collections.Counter()
for r in rows:
    by[r[0]] += 1
    if r[6] is None: miss[r[0]] += 1
print('sin puntero por objeto:', miss.most_common(25))
print('total por objeto:', by.most_common(25))
