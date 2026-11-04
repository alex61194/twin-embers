"""Desensamblador minimo de scripts de eventos de FireRed: longitud y punteros de cada comando."""
import json
from pathlib import Path
_T = json.load(open(Path(__file__).resolve().parent / 'script_ops.json'))
OPS = {int(k): v for k, v in _T.items()}   # op -> [size, [pointer offsets]]
TB = {0: [6, 10], 1: [6, 10, 14], 2: [6, 10, 14], 3: [6], 4: [6, 10, 14], 5: [6, 10],
      6: [6, 10, 14, 18], 7: [6, 10, 14], 8: [6, 10, 14, 18], 9: [6, 10]}
def command(b, p):
    """(tamano, [offsets de puntero]) del comando en b[p], o None si no se conoce."""
    op = b[p]
    if op == 0x5c:
        t = b[p + 1]
        if t not in TB: return None
        ptrs = TB[t]; return ptrs[-1] + 4, ptrs
    v = OPS.get(op)
    return None if v is None else (v[0], v[1])
def sweep(b, a, n=None, end=None):
    """Recorre comandos desde a: n comandos, o hasta end. Devuelve (fin, sitios, cuenta) o None."""
    p, sites, k = a, [], 0
    while (n is None or k < n) and (end is None or p < end):
        c = command(b, p)
        if c is None: return None
        sites += [p + x for x in c[1]]; p += c[0]; k += 1
    return p, sites, k
