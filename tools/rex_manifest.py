#!/usr/bin/env python3
"""Derive the public FireRed reconstruction recipe from reference builds.

DEVELOPMENT TOOL. It needs two private reference artifacts that never ship:

  * a vanilla pret build (agbcc) of the pinned commit: ROM, ELF and linker map.
    The ROM has the supported SHA-1 41cb23d8...;
  * an *untransformed* 3DS build tree (objects as compiled, before rex_objects).

For every candidate unit (a data section of a game object) it proves that the 3DS
bytes equal the reference ROM bytes everywhere except at pointer words, and then
records only structure: ROM offsets, sizes and CRC-32s. The output contains no
game bytes. Units that cannot be proven (different record layout, ambiguous name,
unknown relocation) are written to the exclusion report, never silently dropped.

Also covers the 6301 INCBIN graphics ranges: each payload file must equal the ROM
bytes at the INCBIN symbol's ROM address.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'builder'))
import audit_clean_release as audit  # noqa: E402
import rex  # noqa: E402
from firered3ds_builder.pak import engine_abi  # noqa: E402
from firered3ds_builder.recipe import execute  # noqa: E402
from firered3ds_builder.rom import SUPPORTED_SHA1  # noqa: E402

ROM_BASE = 0x08000000


class RomRef:
    """The vanilla reference build: symbols by (object, name) and global name."""

    def __init__(self, rom: bytes, elf_path: Path, map_path: Path):
        self.rom = rom
        elf = audit.Elf(elf_path)
        self.globals: dict[str, list[tuple[int, int]]] = defaultdict(list)
        self.locals: dict[tuple[str, str], list[tuple[int, int]]] = defaultdict(list)
        current = ''
        for s in elf.symbols:
            if s['type'] == 4:
                current = s['name']
                continue
            if s['type'] != 1 or not s['name'] or s['shndx'] in (0, 0xfff1, 0xfff2) or s['addr'] < ROM_BASE:
                continue
            row = (s['addr'], s['size'])
            if s['bind'] == 1:
                self.globals[s['name']].append(row)
            else:
                self.locals[(current, s['name'])].append(row)
        self.suffixed: dict[tuple[str, str], list[tuple[int, int]]] = defaultdict(list)
        for (stem, name), rows in self.locals.items():
            m = re.fullmatch(r'(.+)\.\d+', name)
            if m:
                self.suffixed[(stem, m.group(1))].extend(rows)
        self.contribs = defaultdict(list)
        for c in audit.parse_map(map_path.read_text(errors='replace')):
            if c['addr'] >= ROM_BASE and c['size']:
                self.contribs[c['obj']].append(c)

    def symbol(self, objkey: str, name: str):
        """(rom address, size) or a reason string."""
        stem = objkey.split('/', 1)[1]
        rows = self.globals.get(name) or self.locals.get((stem, name))
        if not rows:
            # Function-local statics: gcc calls them `name.N`, agbcc `name.M`.
            m = re.fullmatch(r'(.+)\.\d+', name)
            rows = self.suffixed.get((stem, m.group(1))) if m else None
        if not rows:
            return 'no reference symbol'
        if len(rows) > 1:
            return 'ambiguous reference symbol'
        return rows[0]

    def section(self, objkey: str, section: str):
        rows = [c for c in self.contribs.get(rex.rom_object_path(objkey), []) if c['input'] == section]
        if len(rows) != 1:
            return 'no unique reference section'
        return rows[0]['addr'], rows[0]['size']


def masked(data: bytes, sites) -> bytes:
    out = bytearray(data)
    for off, width in sites:
        out[off:off + width] = bytes(width)
    return bytes(out)


def strided_ops(mine: bytes, sites, rom: bytes, addr: int, rsize: int):
    """Match a 3DS array of S-byte records against 4-byte-padded ROM records.

    agbcc pads every struct to a multiple of 4 bytes, modern GCC does not, so an
    array of 6-byte structs is 8-byte records in the ROM. Returns the "R" op."""
    n = len(mine)
    for s in range(n, 1, -1):
        if n % s:
            continue
        r = (s + 3) & ~3
        count = n // s
        if r == s or count * r != rsize:
            continue
        op = ['R', addr - ROM_BASE, r, s, count]
        if masked(execute([op], rom, n), sites) == masked(mine, sites):
            return [op]
    return None


# Bit-field structs whose bit positions differ between the compilers. agbcc packs
# adjacent fields contiguously; the 3DS compiler never lets one straddle its 16-bit
# storage unit. struct BgTemplate is 14 bits of u16 fields followed by `u16 baseTile:10`:
# bit 14 in the ROM, bit 16 here. Candidates are tried whole; the match is a byte proof.
BITFIELD_LAYOUTS = [(4, [[0, 0, 14], [14, 16, 10]])]


def bitfield_ops(mine: bytes, sites, rom: bytes, addr: int, rsize: int):
    if sites or len(mine) != rsize:
        return None
    for stride, ranges in BITFIELD_LAYOUTS:
        if len(mine) % stride:
            continue
        op = ['B', addr - ROM_BASE, stride, len(mine) // stride, ranges]
        if execute([op], rom, len(mine)) == mine:
            return [op]
    return None


def padded_ops(mine: bytes, sites, rom: bytes, addr: int, rsize: int):
    """The ROM table without a trailing zero sentinel that this build adds (BUGFIX)."""
    if rsize >= len(mine) or any(mine[rsize:]):
        return None
    if masked(mine[:rsize], [s for s in sites if s[0] < rsize]) != masked(rom[addr - ROM_BASE:addr - ROM_BASE + rsize], sites):
        return None
    return [['C', addr - ROM_BASE, rsize], ['F', 0, len(mine) - rsize]]


# Const tables with no ROM symbol of their own that the generic whole-ROM search
# (>= 8 bytes) cannot judge: a function-local string, two colour-index triples. Each
# is named explicitly, so no size threshold decides what leaves the executable, and
# coincidental matches (a one-byte port variable, a compiler switch table) never do.
OBJECT_LOCAL_UNITS = {
    ('c/field_player_avatar.o', '.rodata.dot.0'),
    ('c/fame_checker.o', '.rodata.sTextColor_Green'),
    ('c/oak_speech.o', '.rodata.sTextColor_White'),
}


def find_in_object(ref: 'RomRef', key: str, section: str, mine: bytes, sites):
    """A unique pointer-masked occurrence inside the same object's own ROM data.

    Only for the units named in OBJECT_LOCAL_UNITS. The object's own contribution
    bounds the search and the result is accepted only when exactly one place matches;
    the bytes of that place are then equal to this build's table by construction."""
    if (key, section) not in OBJECT_LOCAL_UNITS:
        return None
    pattern = pattern_of(mine, sites)
    hits = []
    for c in ref.contribs.get(rex.rom_object_path(key), []):
        if not c['input'].startswith(('.rodata', '.data')):
            continue
        base = c['addr'] - ROM_BASE
        blob = ref.rom[base:base + c['size']]
        hits += [base + m.start() for m in re.finditer(b'(?=' + pattern + b')', blob, re.DOTALL)]
    return [['C', hits[0], len(mine)]] if len(hits) == 1 else None


def pattern_of(mine: bytes, sites) -> bytes:
    pieces, at = [], 0
    for off, width in sorted(sites):
        pieces.append(re.escape(mine[at:off]))
        pieces.append(b'.' * width)
        at = off + width
    pieces.append(re.escape(mine[at:]))
    return b''.join(pieces)


def find_in_rom(mine: bytes, sites, rom: bytes):
    """A pointer-masked occurrence of an unpaired table anywhere in the ROM.

    Names cannot pair some function-local statics, but their bytes are enough:
    copying any identical ROM range reproduces the table exactly."""
    if len(mine) < 8:
        return None
    pieces, at = [], 0
    for off, width in sorted(sites):
        pieces.append(re.escape(mine[at:off]))
        pieces.append(b'.' * width)
        at = off + width
    pieces.append(re.escape(mine[at:]))
    found = re.search(b''.join(pieces), rom, re.DOTALL)
    return [['C', found.start(), len(mine)]] if found else None


def analyse(tree: Path, ref: RomRef):
    """-> (units, exclusions). A unit is (objkey, section, ops, size)."""
    units, excluded = [], []
    build = tree / '3ds_port/build'
    for path in sorted([*build.glob('game/**/*.o'), *build.glob('game_data/*.o'), *build.glob('game_song/*.o')]):
        key = rex.object_key(path)
        obj = rex.Obj.load(path)
        functions = {s.name for s in obj.symbols if s.type == 2}
        for sec in obj.sections:
            if not rex.is_unit_candidate(sec):
                continue
            relocs = obj.relocs(sec)
            bad = sorted({r.type for r in relocs if r.type not in rex.RELOC_WIDTH})
            if bad:
                excluded.append((key, sec.name, sec.size, f'relocation types {bad}'))
                continue
            merged = [r for r in relocs if (t := obj.symbols[r.symbol]).bind == rex.STB_LOCAL
                      and t.shndx < len(obj.sections) and obj.sections[t.shndx].flags & (rex.SHF_MERGE | rex.SHF_STRINGS)]
            if merged:
                excluded.append((key, sec.name, sec.size, 'content differs from reference (points into a port string literal)'))
                continue
            sym = rex.unit_symbol(obj, sec) if re.match(r'\.(rodata|data)\.', sec.name) else None
            if key.startswith('c/') and sec.name in ('.rodata', '.data'):
                mine = obj.data(sec)
                ops = find_in_rom(mine, [(r.offset, rex.RELOC_WIDTH[r.type]) for r in relocs], ref.rom)
                if ops is None:
                    excluded.append((key, sec.name, sec.size, 'compiler constants'))
                else:
                    units.append((key, sec.name, ops, sec.size))
                continue
            if sec.name.startswith(('.rodata.', '.data.')):
                if sym is None:
                    why = ('compiler jump table' if sec.name.split('.', 2)[2] in functions
                           else 'unnamed data section')
                    excluded.append((key, sec.name, sec.size, why))
                    continue
                found = ref.symbol(key, sym.name)
            else:
                found = ref.section(key, sec.name)
            mine = obj.data(sec)
            sites = [(r.offset, rex.RELOC_WIDTH[r.type]) for r in relocs]
            ops, why = None, None
            if isinstance(found, str):
                why = found
            else:
                addr, rsize = found
                theirs = ref.rom[addr - ROM_BASE:addr - ROM_BASE + sec.size]
                if rsize >= sec.size and masked(mine, sites) == masked(theirs, sites):
                    tail = ref.rom[addr - ROM_BASE + sec.size:addr - ROM_BASE + rsize]
                    if not any(tail):
                        ops = [['C', addr - ROM_BASE, sec.size]]
                    else:
                        why = 'reference has extra non-zero bytes'
                elif rsize != sec.size:
                    ops = strided_ops(mine, sites, ref.rom, addr, rsize) or padded_ops(mine, sites, ref.rom, addr, rsize)
                if ops is None:
                    ops = bitfield_ops(mine, sites, ref.rom, addr, rsize)
                if ops is None and why is None:
                    why = 'content differs from reference'
            if ops is None:
                ops = find_in_rom(mine, sites, ref.rom) or find_in_object(ref, key, sec.name, mine, sites)
            if ops is None:
                excluded.append((key, sec.name, sec.size, why))
                continue
            units.append((key, sec.name, ops, sec.size))
    return units, excluded


def graphics_entries(tree: Path, ref: RomRef):
    """Entries for the INCBIN files the executable already externalises."""
    # Every INCBIN range of every object, not only those the current link keeps:
    # the recipe must not change when the linker's garbage collection does.
    rows = [row for manifest in sorted((tree / '3ds_port/build/game').rglob('*.assets.json'))
            for row in json.loads(manifest.read_text())]
    entries, missing = {}, []
    for row in rows:
        path = row['path']
        data = (tree / path).read_bytes()
        key = 'c/' + row['source'].removesuffix('.incbin.i') + '.o'
        found = ref.symbol(key, row['symbol'])
        if isinstance(found, str):
            missing.append((path, found))
            continue
        offset = row['offset']
        if row.get('prefix'):
            # SpindaSpot {u8 x, y; u16 image[16]}: 34 bytes here, 36 (padded) in the ROM.
            offset = (offset - 2) // 34 * 36 + 2
        addr = found[0] + offset
        theirs = ref.rom[addr - ROM_BASE:addr - ROM_BASE + len(data)]
        if theirs != data:
            missing.append((path, 'differs from reference ROM bytes'))
            continue
        if path not in entries:
            entries[path] = dict(path=path, size=len(data), crc32=rex.crc(data), ops=[['C', addr - ROM_BASE, len(data)]])
    return entries, missing


def group_of(key: str) -> str:
    kind, name = key.split('/', 1)
    return 'songs' if kind == 'song' else f"{kind}/{name.removesuffix('.o')}"


def build_recipe(tree: Path, ref: RomRef, with_graphics: bool = True):
    units, excluded = analyse(tree, ref)
    groups: dict[str, list] = defaultdict(list)
    for u in units:
        groups[group_of(u[0])].append(u)
    entries, unit_rows = [], []
    for name in sorted(groups):
        ops, offset = [], 0
        for key, section, uops, size in groups[name]:
            for op in uops:
                if op[0] == 'C' and ops and ops[-1][0] == 'C' and ops[-1][1] + ops[-1][2] == op[1]:
                    ops[-1] = ['C', ops[-1][1], ops[-1][2] + op[2]]
                else:
                    ops.append(op)
            unit_rows.append([key, section, len(entries), offset, size])
            offset += size
        data = execute(ops, ref.rom, offset)
        entries.append(dict(path=rex.region_path(name), size=offset, crc32=rex.crc(data), ops=ops))
    missing = []
    if with_graphics:
        graphics, missing = graphics_entries(tree, ref)
        entries.extend(graphics[p] for p in sorted(graphics))
    abi = engine_abi((e['path'], e['size'], e['crc32']) for e in entries)
    engine = []
    for key, section, size, why in excluded:
        reason = {'compiler jump table': 'jump-table', 'compiler constants': 'compiler-constants',
                  'content differs from reference': 'port-adapted'}.get(why, 'port-authored')
        if '.CSWTCH.' in section:
            reason = 'jump-table'
        engine.append([key, section, size, reason])
    recipe = dict(schema=2, rom_sha1=SUPPORTED_SHA1, engine_abi=abi, entries=entries, units=unit_rows,
                  engine_units=sorted(engine))
    return recipe, excluded, missing


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--tree', type=Path, required=True, help='untransformed 3DS build tree')
    ap.add_argument('--rom', type=Path, required=True)
    ap.add_argument('--rom-elf', type=Path, required=True)
    ap.add_argument('--rom-map', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    ap.add_argument('--report', type=Path)
    ap.add_argument('--no-graphics', action='store_true')
    args = ap.parse_args()
    ref = RomRef(args.rom.read_bytes(), args.rom_elf, args.rom_map)
    recipe, excluded, missing = build_recipe(args.tree.resolve(), ref, not args.no_graphics)
    args.output.write_text(json.dumps(recipe, separators=(',', ':')) + '\n')
    total = sum(u[4] for u in recipe['units'])
    print(f"{len(recipe['units'])} units ({total:,} bytes), {len(recipe['entries'])} entries, ABI {recipe['engine_abi']:08x}")
    reasons = defaultdict(lambda: [0, 0])
    for key, section, size, why in excluded:
        reasons[why.split(' (')[0]][0] += 1
        reasons[why.split(' (')[0]][1] += size
    for why, (n, size) in sorted(reasons.items(), key=lambda kv: -kv[1][1]):
        print(f'  excluded: {why}: {n} units, {size:,} bytes')
    if missing:
        print(f'  graphics not in the ROM: {len(missing)}')
    if args.report:
        args.report.write_text(json.dumps(dict(excluded=excluded, graphics_not_in_rom=missing), indent=1) + '\n')


if __name__ == '__main__':
    main()
