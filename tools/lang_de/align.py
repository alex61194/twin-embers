"""Prueba de alineacion ROM USA (BPRE 1.0) -> ROM espanola (BPRS).
Solo lee las dos ROM localmente y escribe un informe sin bytes de la ROM."""
import json, sys, glob, os, collections, hashlib
from pathlib import Path

here = Path(__file__).resolve().parent
us = Path(sys.argv[1]).read_bytes(); es = Path(sys.argv[2]).read_bytes()
assert hashlib.sha1(us).hexdigest() == '41cb23d8dccc8ebd7c649cd8fbb58eeace6e2fdc'

def lz_len(off):
    size = int.from_bytes(us[off+1:off+4], 'little'); pos = off + 4; out = 0
    while out < size:
        flags = us[pos]; pos += 1
        for b in range(8):
            if out >= size: break
            if flags & (0x80 >> b):
                a = us[pos]; pos += 2; out += (a >> 4) + 3
            else:
                pos += 1; out += 1
    return pos - off

def src(op):
    k = op[0]
    if k == 'C': return op[1], op[2]
    if k == 'Z': return op[1], lz_len(op[1])
    if k == 'R': return op[1], op[2] * op[4]
    if k == 'B': return op[1], op[2] * op[3]
    return None

stats = collections.Counter(); bytes_by = collections.Counter()
rows = []; deltas = collections.Counter()
for f in sorted(glob.glob(str(here / 'recipe' / 'entries-*.json'))):
    for e in json.load(open(f))['entries']:
        for op in e['ops']:
            r = src(op)
            if not r: continue
            off, n = r
            blk = us[off:off+n]
            if es[off:off+n] == blk:
                cls, where = 'igual', off
            else:
                where = es.find(blk)
                if where < 0: cls = 'no_encontrado'
                elif es.find(blk, where + 1) >= 0: cls = 'ambiguo'
                else: cls = 'movido'
            stats[cls] += 1; bytes_by[cls] += n
            if cls == 'movido': deltas[where - off] += 1
            if cls == 'no_encontrado':
                same = sum(1 for i in range(n) if es[off+i:off+i+1] == blk[i:i+1])
                rows.append((e['path'], op[0], off, n, round(same / n, 2)))
            else:
                rows.append((e['path'], op[0], off, n, cls))
out = here / 'informe.txt'
with open(out, 'w', encoding='utf-8') as w:
    w.write('Operaciones por clase: %s\n' % dict(stats))
    w.write('Bytes por clase: %s\n' % dict(bytes_by))
    w.write('Desplazamientos mas comunes (movido): %s\n\n' % deltas.most_common(40))
    for r in rows:
        if r[4] not in ('igual',): w.write('%s\t%s\t0x%x\t%d\t%s\n' % r)
print(open(out, encoding='utf-8').read()[:3000])
