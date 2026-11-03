"""Spanish recipe: map every USA recipe operation to the Spanish ROM (BPRS rev 0).

Run on the machine that holds both ROMs:
    python gen_recipe_es.py USA.gba ES.gba recipe_dir es_sizes2.json event_scripts_layout_es.json out_dir

recipe_dir holds the USA recipe parts (entries-*.json, units-*.json). The output
is a recipe with Spanish offsets, sizes and CRC32s, plus a report. Like the USA
recipe it holds no ROM bytes, only positions, lengths and checksums.

How each operation is placed:
  * text symbols (one 0xFF-terminated string): position and length found by
    es_sizes.py / es_sizes2.py (pointers followed from the USA ROM);
  * event_scripts.bin: the layout from es_events2.py;
  * everything else: the shift given by 16-byte anchors near the operation,
    checked by comparing the bytes with ROM pointers masked out; if that
    fails, a unique search of the whole block;
  * LZ77 data (Z operations and raw .lz copies): an LZ header with the same
    decompressed size at the shifted position; a localized graphic keeps its
    size once decompressed, the compressed length is read from the Spanish ROM.
Operations that cannot be placed are listed in the report and keep their USA
position marked 'unplaced', so the recipe is refused until they are solved.
"""
import bisect
import collections
import glob
import hashlib
import json
import sys
import zlib
from pathlib import Path

US_SHA1 = '41cb23d8dccc8ebd7c649cd8fbb58eeace6e2fdc'
# English plural suffixes the Spanish game never appends (its code has no such call):
# filled with 0xFF they are empty strings, so the English code appends nothing.
EMPTY_IN_SPANISH = {
    (0x3a72a0, 2),   # scrcmd.c sText_S
    (0x3a72a2, 4),   # scrcmd.c sText_IES
    (0x46d94c, 4),   # pokemon_jump.c sPluralTxt
    # data/mystery_event_msg.s: e-Reader Mystery Event scripts, laid out differently in the
    # Spanish ROM and unreachable from the game's menus; 0xFF everywhere ends any script at once
    (0x488e28, 2877),
    (0x488c70, 50),  # mystery_event_msg.c gText_MysteryGiftBerry, shown only by those scripts
}
# Layout bytes the Spanish ROM has no copy of (its code uses other constants): these
# sections stay in the engine as pret compiles them, out of the recipe.
KEEP_IN_ENGINE = {
    (0x3cd924, 2),   # trainer_card.c sTrainerCardIdXPositions
    (0x3cd926, 2),   # trainer_card.c sTrainerCardIdYPositions
}
# teachy_tv.c sBg1EndGraphic: the English closing banner is an 8x2 tilemap, the Spanish
# one 4x2 (its code passes width 4). Its two rows go into the English 8x2 rectangle,
# each followed by four blank tiles.
SPANISH_LENGTH = {(0x479590, 32): 16}


def teachy_end_ops(t):
    return [['C', t, 8], ['F', 0, 8], ['C', t + 8, 8], ['F', 0, 8]]
ES_SHA1 = 'ab8f6bfe0ccdaf41188cd015c8c74c314d02296a'


def lz_span(rom, off):
    """(decompressed size, compressed length) of an LZ77 stream, or None."""
    if off + 4 > len(rom) or rom[off] != 0x10:
        return None
    size = int.from_bytes(rom[off + 1:off + 4], 'little')
    pos, out = off + 4, 0
    try:
        while out < size:
            flags = rom[pos]; pos += 1
            for b in range(8):
                if out >= size:
                    break
                if flags & (0x80 >> b):
                    a, c = rom[pos], rom[pos + 1]; pos += 2
                    d = ((a & 15) << 8 | c) + 1
                    if d > out:
                        return None
                    out += (a >> 4) + 3
                else:
                    pos += 1; out += 1
    except IndexError:
        return None
    return size, pos - off


def lz_data(rom, off):
    size = int.from_bytes(rom[off + 1:off + 4], 'little')
    out, pos = bytearray(), off + 4
    while len(out) < size:
        flags = rom[pos]; pos += 1
        for b in range(8):
            if len(out) >= size:
                break
            if flags & (0x80 >> b):
                a, c = rom[pos], rom[pos + 1]; pos += 2
                n, d = (a >> 4) + 3, ((a & 15) << 8 | c) + 1
                for _ in range(n):
                    out.append(out[-d])
            else:
                out.append(rom[pos]); pos += 1
    return bytes(out[:size])


def same_but_pointers(a, b):
    """Share of equal bytes, counting a ROM pointer (xx xx xx 08/09) in both as equal."""
    n = len(a)
    if n != len(b) or n == 0:
        return 0.0
    ok = bytearray(n)
    for i in range(n):
        if a[i] == b[i]:
            ok[i] = 1
        if i >= 3 and a[i] in (8, 9) and b[i] in (8, 9):
            ok[i - 3:i + 1] = b'\x01\x01\x01\x01'
    return sum(ok) / n


def main():
    us_path, es_path, recipe_dir, sizes_path, events_path, out_dir = map(Path, sys.argv[1:7])
    us, es = us_path.read_bytes(), es_path.read_bytes()
    assert hashlib.sha1(us).hexdigest() == US_SHA1, 'not the USA 1.0 ROM'
    assert hashlib.sha1(es).hexdigest() == ES_SHA1, 'not the Spanish rev 0 ROM'

    # 16-byte anchors: USA position -> shift into the Spanish ROM
    idx = {}
    for p in range(0, len(es) - 16, 4):
        w = es[p:p + 16]
        if w.count(w[0]) != 16:
            idx[w] = -1 if w in idx else p
    apos, ad = [], []
    for p in range(0, len(us) - 16, 4):
        w = us[p:p + 16]
        if w.count(w[0]) == 16:
            continue
        q = idx.get(w, -1)
        if q >= 0 and (not ad or ad[-1] != q - p or p - apos[-1] >= 256):
            apos.append(p); ad.append(q - p)
    del idx

    def shifts(p, n):
        """Candidate shifts for USA block [p, p+n): nearest anchors around it, most common first."""
        i = bisect.bisect_left(apos, p)
        votes = collections.Counter()
        for j in range(max(0, i - 3), min(len(apos), i + 3)):
            votes[ad[j]] += 1
        k = bisect.bisect_left(apos, p + n)
        for j in range(max(0, i), min(len(apos), k)):
            votes[ad[j]] += 2
        return [d for d, _ in votes.most_common()]

    entries = [e for f in sorted(glob.glob(str(recipe_dir / 'entries-*.json'))) for e in json.load(open(f))['entries']]
    units = [u for f in sorted(glob.glob(str(recipe_dir / 'units-*.json'))) for u in json.load(open(f))['units']]

    path_of = {op[1]: e['path'] for e in entries for op in e['ops'] if op[0] == 'C'}

    # text strings found by es_sizes: USA (offset, size) -> Spanish (offset, size)
    text, text_src = {}, {}      # how each string was found: 'ptr' (pointers) or 'run' (contiguous)
    for r in json.load(open(sizes_path))['rows']:
        uo, un, eo, en = r[5], r[4], r[6], r[7]
        if eo is None or en is None or r[0] == 'asm/event_scripts.o':
            continue
        if us[uo:uo + un].count(0xff) != 1 or us[uo + un - 1] != 0xff:
            continue            # a table that happens to end in 0xFF, not a string
        if es[eo:eo + en].count(0xff) != 1 or es[eo + en - 1] != 0xff:
            continue
        text[(uo, un)] = (eo, en)
        text_src[(uo, un)] = 'ptr' if r[8] > 0 else 'run'
    base_text, base_src = dict(text), dict(text_src)
    ev = json.load(open(events_path))

    # More strings, by pointers: every single-string C op not found yet.
    def is_string(rom, o, n):
        b = rom[o:o + n]
        return n > 0 and b.count(0xff) == 1 and b[-1] == 0xff

    def strlen(o):
        e = es.find(b'\xff', o)
        return e - o + 1 if e >= 0 else None

    wanted = {}
    for e in entries:
        if e['path'] == 'gamedata/asm/event_scripts.bin':
            continue
        for op in e['ops']:
            if op[0] == 'C' and is_string(us, op[1], op[2]) \
                    and es[op[1]:op[1] + op[2]] != us[op[1]:op[1] + op[2]]:
                wanted[op[1]] = op[2]
    sites = collections.defaultdict(list)
    for p in range(0, len(us) - 3, 4):
        if us[p + 3] == 8:
            v = int.from_bytes(us[p:p + 4], 'little') - 0x08000000
            if v in wanted:
                sites[v].append(p)
    def find_strings(site_to_es, strict=True):
        """Spanish position of strings by the pointers to them, then by contiguous runs."""
        for o, n in wanted.items():
            votes = collections.Counter()
            for p in sites.get(o, ()):
                for q in [site_to_es(p)]:
                    if q is not None and 0 <= q <= len(es) - 4 and es[q + 3] == 8:
                        t = int.from_bytes(es[q:q + 4], 'little') - 0x08000000
                        if 0 < t < len(es) and (es[t - 1] == 0xff or not strict):
                            votes[t] += 1
            if not votes and not strict:
                for p in sites.get(o, ()):
                    t = pool_target(p)
                    if t is not None and 0 < t < len(es) and es[t - 1] == 0xff:
                        votes[t] += 1
            if votes:
                t, v = votes.most_common(1)[0]
                if v * 2 > sum(votes.values()) and strlen(t) and (o, n, t) not in banned:
                    text[(o, n)] = (t, strlen(t)); text_src[(o, n)] = 'ptr'
        # contiguous runs: a string that follows another in the USA ROM follows it in the Spanish ROM
        runs = sorted((o, n) for e in entries for op in e['ops'] if op[0] == 'C'
                      for o, n in [(op[1], op[2])] if is_string(us, o, n))
        for _ in range(64):
            grew = False
            for (ao, an), (bo, bn) in zip(runs, runs[1:]):
                if ao + an != bo:
                    continue
                if (ao, an) in text and (bo, bn) not in text:
                    t = sum(text[(ao, an)]); m = strlen(t)
                    if m and (bo, bn, t) not in banned:
                        text[(bo, bn)] = (t, m); text_src[(bo, bn)] = 'run'; grew = True
                elif (bo, bn) in text and (ao, an) not in text:
                    t = text[(bo, bn)][0]
                    if t > 0 and es[t - 1] == 0xff:
                        j = t - 2
                        while j >= 0 and es[j] != 0xff:
                            j -= 1
                        if (ao, an, j + 1) not in banned:
                            text[(ao, an)] = (j + 1, t - j - 1); text_src[(ao, an)] = 'run'; grew = True
            if not grew:
                break


    loose_hits = set()

    def place_block(off, n, loose=False):
        blk = us[off:off + n]
        for d in shifts(off, n):
            q = off + d
            if 0 <= q and q + n <= len(es):
                cand = es[q:q + n]
                if cand == blk or same_but_pointers(blk, cand) >= 0.98:
                    return q
        q = es.find(blk)
        if q >= 0 and es.find(blk, q + 1) < 0:
            return q
        if loose:
            # a fixed-size record with localized names inside: same shift as its neighbours
            for d in shifts(off, n)[:1]:
                q = off + d
                if 0 <= q and q + n <= len(es) and same_but_pointers(blk, es[q:q + n]) >= 0.5:
                    loose_hits.add((off, n))
                    return q
        return None

    def place_lz(off):
        span = lz_span(us, off)
        if span is None:
            return None
        size, _ = span
        us_out = lz_data(us, off)
        best = None
        for d in shifts(off, span[1]):
            q = off + d
            s = lz_span(es, q)
            if s and s[0] == size:
                if lz_data(es, q) == us_out:
                    return q, s, 'same'
                best = best or (q, s, 'localized')
        if best:
            return best
        hdr = us[off:off + 4]
        q = es.find(hdr)
        hits = []
        while q >= 0 and len(hits) < 64:
            if q % 4 == 0:
                s = lz_span(es, q)
                if s and s[0] == size:
                    if lz_data(es, q) == us_out:
                        return q, s, 'same'
                    hits.append((q, s))
            q = es.find(hdr, q + 1)
        if len(hits) == 1:
            return hits[0][0], hits[0][1], 'localized?'
        return None

    def build():
        stats, report, placed, lost = collections.Counter(), [], [], []
        trace.clear()
        new_entries, posmaps = [], []
        for ei, e in enumerate(entries):
            ops, pmap, upos, epos, bad = [], [], 0, 0, False
            for op in e['ops']:
                k = op[0]
                if k == 'F':
                    ops.append(list(op)); pmap.append((upos, op[2], epos, op[2]))
                    upos += op[2]; epos += op[2]; continue
                if k == 'C':
                    off, n = op[1], op[2]
                    if (off, n) in KEEP_IN_ENGINE:
                        pmap.append((upos, n, epos, 0)); stats['kept-in-engine'] += 1
                        upos += n; continue
                    if (off, n) in SPANISH_LENGTH and (off, n) in pinned:
                        ops.extend(teachy_end_ops(pinned[(off, n)][0])); pmap.append((upos, n, epos, n))
                        stats['rearranged'] += 1
                        upos += n; epos += n; continue
                    if (off, n) in EMPTY_IN_SPANISH:
                        ops.append(['F', 0xff, n]); pmap.append((upos, n, epos, n))
                        stats['empty-in-spanish'] += 1
                        upos += n; epos += n; continue
                    if e['path'] == 'gamedata/asm/event_scripts.bin':
                        q, m, how = ev['es_start'], ev['es_size'], 'event-scripts'
                    elif (off, n) in text:
                        (q, m), how = text[(off, n)], 'text-' + text_src[(off, n)]
                    elif (off, n) in pinned:
                        (q, m), how = pinned[(off, n)], 'followed'
                        report.append(f'FOLLOWED\t{e["path"]}\tC\t0x{off:x}\t{n}\t{m}')
                    elif e['path'].endswith('.lz') and lz_span(us, off) and lz_span(us, off)[1] <= n < lz_span(us, off)[1] + 4:
                        got = place_lz(off)
                        if got:
                            q, (_, clen), how = got
                            pad = n - lz_span(us, off)[1]
                            m = clen + ((-clen) % 4 if pad else 0)
                            how = 'lz-' + how
                        else:
                            q = None
                    else:
                        q = place_block(off, n, loose=not is_string(us, off, n)); m = n
                        if (off, n, q) in banned:
                            q = None
                        how = 'same' if q is not None and es[q:q + n] == us[off:off + n] else \
                            'loose' if (off, n) in loose_hits else 'pointers'
                        if how == 'loose':
                            lost.append((off, n))
                            report.append(f'LOOSE\t{e["path"]}\tC\t0x{off:x}\t{n}')
                        if q is None and us[off + n - 1:off + n] == b'\xff':
                            how = 'text-unplaced'
                    if q is None:
                        stats['unplaced'] += 1; bad = True; lost.append((off, n))
                        report.append(f'UNPLACED\t{e["path"]}\tC\t0x{off:x}\t{n}')
                        ops.append(['C', off, n]); pmap.append((upos, n, None, n))
                        upos += n; epos += n; continue
                    stats[how] += 1
                    trace.append((off, n, q, m, how, e['path']))
                    ops.append(['C', q, m]); pmap.append((upos, n, epos, m))
                    if m == n and how in ('same', 'pointers', 'loose'):
                        placed.append((off, n, q))
                    upos += n; epos += m
                elif k == 'Z':
                    got = place_lz(op[1])
                    us_size = lz_span(us, op[1])[0]
                    if got is None:
                        stats['unplaced'] += 1; bad = True
                        report.append(f'UNPLACED\t{e["path"]}\tZ\t0x{op[1]:x}\t{us_size}')
                        ops.append(list(op)); pmap.append((upos, us_size, None, us_size))
                    else:
                        stats['lz-' + got[2]] += 1
                        if got[2] != 'same':
                            report.append(f'LOCALIZED\t{e["path"]}\tZ\t0x{op[1]:x}\t{us_size}')
                        ops.append(['Z', got[0]]); pmap.append((upos, us_size, epos, us_size))
                    upos += us_size; epos += us_size
                elif k == 'R':
                    off, src, dst, count = op[1:]
                    n = src * (count - 1) + dst
                    q = place_block(off, n)
                    if q is None:
                        stats['unplaced'] += 1; bad = True
                        report.append(f'UNPLACED\t{e["path"]}\tR\t0x{off:x}\t{n}')
                        q = off
                    else:
                        stats['records'] += 1
                        placed.append((off, n, q))
                    ops.append(['R', q, src, dst, count]); pmap.append((upos, dst * count, epos, dst * count))
                    upos += dst * count; epos += dst * count
                elif k == 'B':
                    off, stride, count = op[1], op[2], op[3]
                    n = stride * count
                    q = place_block(off, n)
                    if q is None:
                        stats['unplaced'] += 1; bad = True
                        report.append(f'UNPLACED\t{e["path"]}\tB\t0x{off:x}\t{n}')
                        q = off
                    else:
                        stats['records'] += 1
                        placed.append((off, n, q))
                    ops.append(['B', q, stride, count, op[4]]); pmap.append((upos, n, epos, n))
                    upos += n; epos += n
                else:
                    raise SystemExit(f'unknown op {k}')
            assert upos == e['size'], (e['path'], upos, e['size'])
            ne = dict(path=e['path'], size=epos, ops=ops)
            if bad:
                ne['crc32'] = None
            new_entries.append(ne); posmaps.append(pmap)
        return stats, report, new_entries, posmaps, placed, lost

    def anchor_site(p):
        return p + shifts(p, 4)[0] if shifts(p, 4) else None

    pinned, trace, banned = {}, [], set()
    placed, pstart, nb, nstart, resized_lz = [], [], [], [], []

    def placed_site(p):
        """A pointer site inside a block already placed moves with that block."""
        i = bisect.bisect_right(pstart, p) - 1
        if i >= 0 and p + 4 <= placed[i][0] + placed[i][1]:
            return placed[i][2] + p - placed[i][0]
        return anchor_site(p)

    def pool_target(p):
        """Spanish pointer at a literal-pool site whose surrounding code is the same in both
        games (at least two of the four words beside it agree), trying each nearby shift."""
        for d in shifts(p, 4):
            q = p + d
            if 8 <= q <= len(es) - 12 and es[q + 3] in (8, 9) \
                    and sum(us[p + k:p + k + 4] == es[q + k:q + k + 4] for k in (-8, -4, 4, 8)) >= 2:
                return int.from_bytes(es[q:q + 4], 'little') - 0x08000000
        return None

    def locate():
        """Strings by pointers and runs, then tables by the pointers to them, avoiding banned spots."""
        text.clear(); text.update({k: v for k, v in base_text.items() if (*k, v[0]) not in banned})
        text_src.clear(); text_src.update(base_src)
        pinned.clear()
        find_strings(anchor_site)
        *_, first, lost = build()
        placed[:] = sorted(first); pstart[:] = [x[0] for x in placed]
        nb[:] = sorted((t[0], t[1], t[2], t[3]) for t in trace)
        nstart[:] = [t[0] for t in nb]

        find_strings(placed_site)

        # Tables that moved and changed (records with localized fields, lists sorted by Spanish
        # word): follow the pointers to them from blocks already placed. The length stays the
        # USA one unless the next block in the USA ROM, contiguous, starts elsewhere in Spain.
        lost = {o: n for o, n in lost if not is_string(us, o, n)}
        for _round in range(8):
            before = len(pinned) + len(text)
            lsites = collections.defaultdict(list)
            for p in range(0, len(us) - 3, 4):
                if us[p + 3] == 8:
                    v = int.from_bytes(us[p:p + 4], 'little') - 0x08000000
                    if v in lost:
                        lsites[v].append(p)
            for o, n in lost.items():
                votes = collections.Counter()
                for p in lsites.get(o, ()):
                    i = bisect.bisect_right(pstart, p) - 1
                    if i >= 0 and p + 4 <= placed[i][0] + placed[i][1]:
                        q = placed[i][2] + p - placed[i][0]
                        if es[q + 3] == 8:
                            votes[int.from_bytes(es[q:q + 4], 'little') - 0x08000000] += 1
                m = n
                if not votes:
                    # only pointed to from code: the literal pool moves with the code around it.
                    # The block there must look like the USA one, have the same shape (as many
                    # 0xFF ends: a table of fixed-width names), be an LZ stream of the same
                    # decompressed size, or be named by several code sites at once.
                    code = collections.Counter()
                    for p in lsites.get(o, ()):
                        q = anchor_site(p)
                        if q is not None and 0 <= q <= len(es) - 4 and es[q + 3] in (8, 9):
                            code[int.from_bytes(es[q:q + 4], 'little') - 0x08000000] += 1
                    for t, v in code.most_common(1):
                        if not 0 <= t <= len(es) - n:
                            continue
                        ff = us[o:o + n].count(0xff)
                        lz_u = lz_span(us, o)
                        if lz_u and lz_u[1] <= n < lz_u[1] + 4:
                            lz_e = lz_span(es, t)
                            if lz_e and lz_e[0] == lz_u[0]:
                                votes[t] = v
                                m = lz_e[1] + ((-lz_e[1]) % 4 if n > lz_u[1] else 0)
                        elif same_but_pointers(us[o:o + n], es[t:t + n]) >= 0.75 \
                                or (ff >= 3 and es[t:t + n].count(0xff) in range(ff - ff // 50 - 1, ff + ff // 50 + 2)) \
                                or v >= 3:
                            votes[t] = v
                if not votes:
                    for p in lsites.get(o, ()):
                        t = pool_target(p)
                        if t is not None and 0 <= t <= len(es) - n:
                            votes[t] += 1
                if not votes:
                    # sandwiched: the blocks just before and after it moved by the same amount
                    # and the gap between them is as long in Spain, so it sits in that gap
                    i = bisect.bisect_right(nstart, o)
                    if 0 < i < len(nb):
                        a, b = nb[i - 1], nb[i]
                        t = a[2] + a[3] + (o - a[0] - a[1])
                        if a[0] + a[1] <= o and o + n <= b[0] and b[2] - (t + n) == b[0] - (o + n):
                            votes[t] += 1
                if not votes and n <= 8:
                    # a few bytes of layout (positions, ids): the same bytes near where the neighbours went
                    i = bisect.bisect_right(nstart, o)
                    if 0 < i:
                        a = nb[i - 1]
                        c = a[2] + a[3] + (o - a[0] - a[1])
                        for d in sorted(range(-256, 257), key=abs):
                            if 0 <= c + d <= len(es) - n and es[c + d:c + d + n] == us[o:o + n]:
                                votes[c + d] += 1
                                break
                if not votes:
                    # next to a placed neighbour, with its shift
                    i = bisect.bisect_right(pstart, o)
                    for j in (i - 1, i):
                        if 0 <= j < len(placed):
                            t = o + placed[j][2] - placed[j][0]
                            if 0 <= t <= len(es) - n and same_but_pointers(us[o:o + n], es[t:t + n]) >= 0.9:
                                votes[t] += 1
                                break
                if votes:
                    t, v = votes.most_common(1)[0]
                    lz_u = lz_span(us, o)
                    if lz_u and lz_u[1] <= n < lz_u[1] + 4:
                        # an LZ stream, however it was found: the Spanish one must unpack to the
                        # same size, and its own compressed length is the one copied
                        # (a localized picture may unpack bigger or smaller: the game decompresses
                        # it to VRAM or through its 16 KiB buffer, so up to 16 KiB is accepted)
                        lz_e = lz_span(es, t)
                        if not lz_e or (lz_e[0] != lz_u[0] and not 0 < lz_e[0] <= 0x4000):
                            continue
                        if lz_e[0] != lz_u[0]:
                            resized_lz.append((o, lz_u[0], lz_e[0]))
                        m = lz_e[1] + ((-lz_e[1]) % 4 if n > lz_u[1] else 0)
                    if v * 2 > sum(votes.values()) and 0 <= t < len(es) and (o, n, t) not in banned:
                        pinned[(o, n)] = (t, SPANISH_LENGTH.get((o, n), m))
            for (o, n), (t, m) in pinned.items():
                if m == n and (o, n, t) not in placed:
                    placed.append((o, n, t))
            placed.sort()
            pstart[:] = [x[0] for x in placed]
            nb[:] = sorted(set(nb) | {(o, n, t, m) for (o, n), (t, m) in list(pinned.items()) + list(text.items())})
            nstart[:] = [t[0] for t in nb]
            find_strings(placed_site, strict=False)
            if len(pinned) + len(text) == before:
                break
        starts_es = {}
        for (o, n), (t, m) in list(text.items()) + list(pinned.items()):
            starts_es[o] = t
        for o, n, q in placed:
            starts_es[o] = q
        for (o, n), (t, m) in list(pinned.items()):
            nxt = starts_es.get(o + n)
            if path_of.get(o) == 'gamedata/c/easy_chat.bin' and m == n and nxt is not None and nxt != t + n and 0 < nxt - t < 2 * n:
                pinned[(o, n)] = (t, nxt - t)

    # A weak placement (string found by pointer or run, table followed by pointer, loose
    # record) that lands on bytes a strong placement owns, or on another weak one, is wrong:
    # drop it and build again.
    rank = {'same': 3, 'pointers': 3, 'records': 3, 'lz-same': 3, 'event-scripts': 3,
            'text-ptr': 2, 'followed': 2, 'lz-localized': 2, 'text-run': 1, 'lz-localized?': 1, 'loose': 1}
    for _round in range(6):
        locate()
        stats, report, new_entries, posmaps, *_ = build()
        tr = sorted(trace, key=lambda t: t[2])
        drop = set()
        for i, a in enumerate(tr):
            for b in tr[i + 1:]:
                if b[2] >= a[2] + a[3]:
                    break
                if (a[2], a[3]) == (b[2], b[3]) or (a[0] < b[0] + b[1] and b[0] < a[0] + a[1]):
                    continue        # the USA recipe copies these bytes twice too
                ra, rb = rank.get(a[4], 1), rank.get(b[4], 1)
                if ra == rb and ra < 3 and a[1] != b[1]:
                    # two equally good guesses: the bigger block is the one many pointers agree on
                    ra, rb = (ra + 0.5, rb) if a[1] > b[1] else (ra, rb + 0.5)
                if ra < 3 and ra <= rb:
                    drop.add((a[0], a[1], a[2]))
                if rb < 3 and rb <= ra:
                    drop.add((b[0], b[1], b[2]))
        if not drop:
            break
        banned.update(drop)
        print('dropped', len(drop), 'doubtful placements')

    # CRCs from the Spanish ROM, with the Builder's own interpreter
    here = Path(__file__).resolve().parent
    for base in (here.parents[1] / 'builder', here / 'builder'):
        sys.path.insert(0, str(base))
    try:
        from firered3ds_builder.recipe import execute
    except ImportError:
        execute = None
    for ne in new_entries:
        if 'crc32' in ne:
            continue
        if execute is None:
            raise SystemExit('run from the twin-embers checkout (builder/ is needed for the CRCs)')
        out = execute(ne['ops'], es, ne['size'])
        assert len(out) == ne['size'], ne['path']
        ne['crc32'] = zlib.crc32(out)

    # units: USA position in the entry -> Spanish position and size
    new_units, ustats = [], collections.Counter()
    for key, sec, ei, off, size in units:
        pm = posmaps[ei]
        starts = [p[0] for p in pm]
        i = bisect.bisect_right(starts, off) - 1
        if i < 0:
            ustats['outside'] += 1; continue
        a = pm[i]
        if a[2] is None:
            ustats['unplaced'] += 1; new_units.append([key, sec, ei, off, size, 'unplaced']); continue
        if a[3] == 0 and off == a[0] and size == a[1]:
            ustats['kept-in-engine'] += 1; continue
        if off == a[0]:
            end, j, esz = off + size, i, 0
            while j < len(pm) and pm[j][0] < end:
                esz += pm[j][3]; j += 1
            if j and pm[j - 1][0] + pm[j - 1][1] == end:
                new_units.append([key, sec, ei, a[2], esz]); ustats['mapped'] += 1
                if esz != size:
                    ustats['resized'] += 1
                continue
        if a[1] == a[3] and off + size <= a[0] + a[1]:
            new_units.append([key, sec, ei, a[2] + off - a[0], size]); ustats['inside'] += 1; continue
        ustats['split'] += 1; new_units.append([key, sec, ei, off, size, 'split'])
        report.append(f'UNIT-SPLIT\t{key}\t{sec}\t{off}\t{size}')

    out_dir.mkdir(parents=True, exist_ok=True)
    json.dump(dict(schema=2, rom_sha1=ES_SHA1, entries=new_entries, units=new_units),
              open(out_dir / 'recipe-es.json', 'w'), separators=(',', ':'))
    # placements that land on the same Spanish bytes as another one of a different length
    trace.sort(key=lambda t: t[2])
    for a, b in zip(trace, trace[1:]):
        if b[2] < a[2] + a[3] and (a[2], a[3]) != (b[2], b[3]):
            report.append(f'OVERLAP\t{a[5]}\t{a[4]}\t0x{a[0]:x}\t{a[1]}\t{b[5]}\t{b[4]}\t0x{b[0]:x}\t{b[1]}')
    for o, a, b in sorted(set(resized_lz)):
        report.append(f'LZ-RESIZED\t{path_of.get(o)}\t0x{o:x}\t{a}\t{b}')
    json.dump(trace, open(out_dir / 'placements.json', 'w'))
    with open(out_dir / 'recipe-es-report.txt', 'w', encoding='utf-8') as w:
        w.write(f'ops: {dict(stats)}\nunits: {dict(ustats)}\n')
        w.write('\n'.join(report) + '\n')
    print('ops:', dict(stats)); print('units:', dict(ustats))
    print('entries without CRC:', sum(1 for e in new_entries if e['crc32'] is None), 'of', len(new_entries))


if __name__ == '__main__':
    main()
