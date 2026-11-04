"""Tercera pasada: bloques grandes 'distinto'. Busca el desplazamiento por
anclas de 16 bytes; para LZ77 compara el contenido descomprimido."""
import sys, collections
from pathlib import Path
here = Path(__file__).resolve().parent
us = Path(sys.argv[1]).read_bytes(); es = Path(sys.argv[2]).read_bytes()
def lz(src, off):
    if src[off] != 0x10: return None
    size = int.from_bytes(src[off+1:off+4], 'little'); out = bytearray(); pos = off + 4
    try:
        while len(out) < size:
            fl = src[pos]; pos += 1
            for b in range(8):
                if len(out) >= size: break
                if fl & (0x80 >> b):
                    a, c = src[pos], src[pos+1]; pos += 2
                    ln, d = (a >> 4) + 3, ((a & 15) << 8 | c) + 1
                    for _ in range(ln): out.append(out[-d])
                else: out.append(src[pos]); pos += 1
    except IndexError: return None
    return bytes(out)
def score(a, b):
    n = min(len(a), len(b)); ok = 0; i = 0
    while i < n:
        if i % 4 == 0 and i + 4 <= n and a[i+3] in (8, 9) and b[i+3] in (8, 9): ok += 4; i += 4; continue
        ok += a[i] == b[i]; i += 1
    return ok / max(len(a), 1)
lines = open(here / 'out2.txt', encoding='utf-8').read().splitlines()[1:]
res = collections.Counter(); rb = collections.Counter()
for l in lines:
    p = l.split('\t')
    if len(p) < 6 or p[5] == 'ptr_ok': continue
    path, off, n = p[0], int(p[1], 16), int(p[2])
    if path.endswith('.lz'):
        u = lz(us, off); size = int.from_bytes(us[off+1:off+4], 'little')
        hdr = bytes([0x10]) + size.to_bytes(3, 'little'); best = (0, None); i = es.find(hdr)
        while i >= 0:
            if i % 4 == 0:
                e = lz(es, i)
                if e and len(e) == len(u):
                    s = score(u, e)
                    if s > best[0]: best = (s, i - off)
                    if s == 1: break
            i = es.find(hdr, i + 1)
        kind = 'lz_igual' if best[0] == 1 else 'lz_traducido' if best[0] else 'lz_no'
    else:
        votes = collections.Counter()
        for k in range(48):
            q = off + (n * k) // 48
            w = us[q:q+16]
            if len(w) < 16: continue
            j = es.find(w)
            if j >= 0 and es.find(w, j + 1) < 0: votes[j - q] += 1
        if not votes: kind, best = 'texto?', (0, None)
        else:
            d = votes.most_common(1)[0][0]
            best = (score(us[off:off+n], es[off+d:off+d+n]), d)
            kind = 'ptr_ok' if best[0] >= .98 else 'parcial' if best[0] >= .6 else 'texto?'
    res[kind] += 1; rb[kind] += n
    print('%s\t0x%x\t%d\t%s\t%s\t%s' % (path, off, n, kind, '%.3f' % best[0], best[1]))
print(dict(res)); print(dict(rb))
