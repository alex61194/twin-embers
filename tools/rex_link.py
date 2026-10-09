#!/usr/bin/env python3
"""After the link: write romfs/engine/rex.bin, the engine's map of ROM-derived data.

For every unit that survived garbage collection the file records where the pack
bytes go (link-time address) and, for each pointer word inside it, the final
address that word must hold. It describes the executable only: it carries no
game bytes and changes with every code change, which is why it lives in the
RomFS and the pack does not depend on it.

  header   magic "F3RX", version, reference address (AgbMain), counts, CRC-32
  region   path offset, size, CRC-32, live unit count   (one per recipe region)
  unit     region, source offset, destination, size, first site, site count
  site     address of the pointer word, address it must hold; bit 31 of the address
           marks a 16-bit site (an absolute constant such as a special's number)
"""
from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import audit_clean_release as audit  # noqa: E402
import rex  # noqa: E402

MAGIC = 0x58523346  # "F3RX" little endian
VERSION = 1
HEADER = struct.Struct('<8I')
REGION = struct.Struct('<4I')
UNIT = struct.Struct('<6I')
SITE = struct.Struct('<2I')


def build(port: Path, recipe: dict, elf_path: Path, map_path: Path):
    elf = audit.Elf(elf_path)
    globals_: dict[str, int] = {}
    for s in elf.symbols:
        if s['bind'] in (1, 2) and s['name'] and s['shndx'] != 0:
            if s['name'] in globals_ and globals_[s['name']] != s['addr']:
                raise ValueError(f"duplicate global symbol {s['name']}")
            globals_[s['name']] = s['addr']
    if 'AgbMain' not in globals_:
        raise ValueError('AgbMain missing from the link')
    sections: dict[tuple[str, str], int] = {}
    for c in audit.parse_map(map_path.read_text(errors='replace')):
        sections.setdefault((c['obj'], c['input']), c['addr'])

    regions = [(i, e) for i, e in enumerate(recipe['entries']) if e['path'].startswith(rex.REGION_PREFIX)]
    region_index = {entry: n for n, (entry, _) in enumerate(regions)}
    units, sites, live = [], [], [0] * len(regions)
    missing_globals = set()
    for path in sorted((port / 'build').rglob('*.rex.json')):
        data = json.loads(path.read_text())
        obj_name = path.with_suffix('').with_suffix('.o').relative_to(port).as_posix()
        for u in data['units']:
            dst = sections.get((obj_name, u['section']))
            if dst is None:
                continue  # linker garbage-collected it
            first = len(sites)
            for off, kind, *rest in u['sites']:
                width = 4
                if kind == 'g':
                    name, addend, width = rest
                    if name not in globals_:
                        missing_globals.add(name)
                        continue
                    target = (globals_[name] + addend) & ((1 << (8 * width)) - 1)
                else:
                    target = dst + rest[0]
                sites.append(((dst + off) | (0x80000000 if width == 2 else 0), target & 0xffffffff))
            region = region_index[u['entry']]
            live[region] += 1
            units.append((region, u['offset'], dst, u['size'], first, len(sites) - first))
    if missing_globals:
        raise ValueError('pointer targets missing from the link: ' + ', '.join(sorted(missing_globals)[:10]))
    units.sort(key=lambda t: (t[0], t[1]))
    return regions, units, sites, live, globals_['AgbMain']


def roots(port: Path) -> list[str]:
    """Global names the link must keep alive: every pointer target of every unit.

    The relocations that used to reference them were removed with the bytes."""
    names = set()
    for path in (port / 'build').rglob('*.rex.json'):
        for unit in json.loads(path.read_text())['units']:
            names.update(site[2] for site in unit['sites'] if site[1] == 'g')
    return sorted(names)


def pack(regions, units, sites, live, reference) -> bytes:
    strings = bytearray()
    region_rows = bytearray()
    for n, (_, entry) in enumerate(regions):
        region_rows += REGION.pack(len(strings), entry['size'], entry['crc32'], live[n])
        strings += entry['path'].encode() + b'\0'
    unit_rows = b''.join(UNIT.pack(*u) for u in units)
    site_rows = b''.join(SITE.pack(*s) for s in sites)
    body = bytes(region_rows) + unit_rows + site_rows + bytes(strings)
    header = HEADER.pack(MAGIC, VERSION, reference, len(regions), len(units), len(sites), len(strings), rex.crc(body))
    return header + body


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--port-dir', type=Path, default=Path('.'))
    ap.add_argument('--recipe', type=Path)
    ap.add_argument('--elf', type=Path)
    ap.add_argument('--map', type=Path)
    ap.add_argument('--output', type=Path)
    ap.add_argument('--roots', type=Path, help='write the linker GC roots (before the link) and exit')
    args = ap.parse_args()
    port = args.port_dir.resolve()
    if args.roots:
        names = roots(port)
        args.roots.write_text(''.join(f'-Wl,--undefined={n}\n' for n in names))
        print(f'rex: {len(names)} link roots')
        return
    if not args.recipe:
        ap.error('--recipe is required')
    recipe = rex.load_recipe(args.recipe)
    regions, units, sites, live, ref = build(port, recipe, args.elf or port / 'build/firered_game.elf',
                                             args.map or port / 'build/firered_game.map')
    blob = pack(regions, units, sites, live, ref)
    out = args.output or port / 'romfs/engine/rex.bin'
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(blob)
    print(f'rex: {len(units)} live units, {len(sites)} pointer sites, '
          f'{sum(1 for n in live if n)}/{len(regions)} regions in use, {len(blob)} bytes')


if __name__ == '__main__':
    main()
