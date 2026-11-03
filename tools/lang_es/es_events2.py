"""Disposicion ES de event_scripts con desensamblado de scripts (solo offsets y tamanos)."""
import json, sys, bisect, collections
from pathlib import Path
here = Path(__file__).resolve().parent
sys.path.insert(0, str(here)); import scrdis
us = Path(sys.argv[1]).read_bytes(); es = Path(sys.argv[2]).read_bytes()
L = json.load(open(here / 'event_scripts_layout_us.json'))
base, size = L['us_start'], L['size']
B = us[base:base+size]
rel = sorted(set(r[0] for r in L['relocs'])); relset = set(rel); relmask = set(x + k for x in rel for k in range(4))
offs = sorted(set([0] + [s[0] for s in L['syms'] if s[0] < size] + [size]))
names = {}
for o, n in L['syms']: names.setdefault(o, n)
def has_rel(a, b):
    i = bisect.bisect_left(rel, a); return i < len(rel) and rel[i] < b
def is_text(a, b):
    if has_rel(a, b): return False
    z = B[a:b].rstrip(b'\x00'); return len(z) > 0 and z[-1] == 0xff and z.count(0xff) == 1
def is_move(a, b):
    if has_rel(a, b): return False
    z = B[a:b].rstrip(b'\x00'); return 'Movement' in names.get(a, '') and len(z) > 0 and z[-1] == 0xfe
E0 = base + 140   # inicio ES (primera prueba)
e = E0; pos = {}; sites = []; kinds = collections.Counter(); diffs = []
for a, b in zip(offs, offs[1:]):
    pos[a] = e - E0
    if is_text(a, b) or is_move(a, b):
        term = 0xff if is_text(a, b) else 0xfe
        n = es.find(bytes([term]), e) - e + 1
        kinds['texto' if term == 0xff else 'mov'] += 1
        pad = len(B[a:b]) - len(B[a:b].rstrip(b'\x00'))
        e += n
        if pad and b % 4 == 0:
            while (e - E0) % 4: e += 1
        continue
    sw = scrdis.sweep(B, a, end=b) if has_rel(a, b) and 'Script' in names.get(a, '') and 'MapScripts' not in names.get(a, '') else None
    if sw and sw[0] == b:
        # comandos USA y sus relocs reales
        cmds = []; p = a
        while p < b:
            c = scrdis.command(B, p); cmds.append((p, c[0], [x for x in c[1] if p + x in relset])); p += c[0]
        q = e; ok = True
        for (p, n, rs) in cmds:
            c = scrdis.command(es, q)
            if c is None or es[q] != B[p]: ok = False; break
            sites += [q - E0 + x for x in rs]; q += c[0]
        if ok and b < size and es[q] != B[b] and B[b - 1] not in (2, 3):
            # el ES puede llevar comandos extra al final del tramo (p. ej. un delay)
            q0 = q
            for _ in range(4):
                c = scrdis.command(es, q)
                if c is None: break
                q += c[0]
                if es[q] == B[b]: break
            if es[q] != B[b]: q = q0
        if ok:
            kinds['script' if q - e == b - a else 'script_distinto'] += 1
            e = q; continue
        # alinear secuencias de comandos (el ES puede tener comandos insertados o quitados)
        import difflib
        ec = []; q = e
        for _ in range(len(cmds) + 12):
            c = scrdis.command(es, q)
            if c is None: break
            ec.append((q, c[0], es[q])); q += c[0]
        sm = difflib.SequenceMatcher(None, [B[p] for p, _, _ in cmds], [x[2] for x in ec], autojunk=False)
        last = None; got = []
        for blk in sm.get_matching_blocks():
            for k in range(blk.size):
                (p, n, rs), (q2, n2, _) = cmds[blk.a + k], ec[blk.b + k]
                got += [q2 - E0 + x for x in rs]; last = blk.b + k
        if last is not None and sum(m.size for m in sm.get_matching_blocks()) >= 0.8 * len(cmds):
            sites += got
            # el ultimo comando USA debe estar emparejado
            mb = [m for m in sm.get_matching_blocks() if m.size]
            if mb and mb[-1].a + mb[-1].size == len(cmds):
                q2, n2, _ = ec[last]; kinds['script_alineado'] += 1
                diffs.append([names.get(a), a, b - a, 'alineado', q2 + n2 - e])
                e = q2 + n2; continue
        kinds['script_fallo'] += 1; diffs.append([names.get(a), a, b - a])
    # fijo
    kinds['fijo'] += 1
    d = sum(1 for k in range(a, b) if k not in relmask and B[k] != es[e + k - a])
    sites += [e - E0 + (x - a) for x in rel[bisect.bisect_left(rel, a):bisect.bisect_left(rel, b)]]
    if d: diffs.append([names.get(a), a, b - a, d])
    e += b - a
pos[size] = e - E0
print(dict(kinds)); print('tamano ES', pos[size], 'USA', size, 'diferencias', len(diffs))
# validacion: punteros internos
ok = ko = 0; badp = []
for s in sites:
    w = int.from_bytes(es[E0+s:E0+s+4], 'little')
    if not 0x08000000 <= w < 0x0a000000: ko += 1; badp.append(s); continue
    t = w - 0x08000000 - E0
    if 0 <= t < pos[size]:
        if t in set_pos if (set_pos := globals().get('set_pos')) else False: ok += 1
        else:
            globals()['set_pos'] = set(pos.values())
            if t in set_pos: ok += 1
            else: ko += 1; badp.append(s)
print('sitios', len(sites), 'punteros ES validos', ok, 'raros', ko)
print('primeras diferencias', diffs[:12]); print('fallos script', [d for d in diffs if len(d) == 3][:12]); print('raros', badp[:10])
json.dump({'es_start': E0, 'es_size': pos[size], 'pos': sorted(pos.items()), 'sites': sites, 'diffs': diffs},
          open(here / 'event_scripts_layout_es.json', 'w'))
