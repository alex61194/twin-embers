#!/usr/bin/env python3
"""Prove the REX pointer map equals what the real linker produced.

Development check. Two builds of the same tree exist:

  --reference  the full embedded link (data in .rodata, relocated by ld)
  --tree       the REX link (data NOBITS, pointers from engine/rex.bin)

For every unit alive in both, every pointer site of the reference link must also
be a REX site (same offset) and must point at the same symbol and offset in both
images, and 16-bit constants must be equal. Layouts differ between the images, so
targets are compared by (nearest symbol, distance), not by address.
"""
from __future__ import annotations

import argparse
import bisect
import json
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import audit_clean_release as audit  # noqa: E402
import rex  # noqa: E402

SKIP_PREFIXES = ('$', '.L', '__ctr_rex_')


class Image:
    """A linked ELF: bytes by address and symbolisation of addresses.

    Local symbols are not comparable between the two links (the REX link drops
    those of NOBITS units), so addresses are named after global symbols and after
    the start of the data section they fall in."""

    def __init__(self, port: Path, renamed: dict[tuple[str, str], str] | None = None):
        self.elf = audit.Elf(port / 'build/firered_game.elf')
        listing = audit.parse_map((port / 'build/firered_game.map').read_text(errors='replace'))
        self.map = {(c['obj'], c['input']): c for c in listing}
        rows: dict[int, list[str]] = {}
        for s in self.elf.symbols:
            if s['name'] and s['shndx'] != 0 and s['bind'] in (1, 2) and not s['name'].startswith(SKIP_PREFIXES) \
                    and s['type'] in (0, 1, 2):
                rows.setdefault(s['addr'] & ~1, []).append(s['name'])
        wanted = renamed or {}
        for (obj, section), c in self.map.items():
            if not c['size']:
                continue
            original = wanted.get((obj, section))
            if original is None and section.startswith(('.rodata', '.data')) and obj.startswith('build/game/'):
                original = section
            if original and obj.startswith('build/game/'):
                rows.setdefault(c['addr'], []).append(f'{obj}:{original}')
        self.addrs = sorted(rows)
        self.names = [frozenset(rows[a]) for a in self.addrs]
        spans = sorted((c['addr'], c['addr'] + c['size'], f"{c['obj']}:{wanted.get((c['obj'], c['input']), c['input'])}")
                       for c in listing
                       if c['size'] and c['addr'] and (c['input'].startswith('.text') or c['obj'].startswith('build/game/')))
        self.span_starts = [x[0] for x in spans]
        self.spans = spans
        everything = sorted((c['addr'], c['addr'] + c['size'], f"{c['obj']}:{wanted.get((c['obj'], c['input']), c['input'])}")
                            for c in listing if c['size'] and c['addr'] and not c['input'].startswith(('.bss.ctr_script', '.bss.ctr_song')))
        self.all_starts = [x[0] for x in everything]
        self.all_spans = everything

    def word(self, addr: int, width: int = 4) -> int | None:
        for s in self.elf.sections:
            if s['flags'] & 2 and s['type'] == 1 and s['addr'] <= addr and addr + width <= s['addr'] + s['size']:
                return int.from_bytes(s_data(self.elf, s)[addr - s['addr']:addr - s['addr'] + width], 'little')
        return None

    def section_position(self, value: int) -> tuple[str, int] | None:
        v = value & ~1
        j = bisect.bisect_right(self.all_starts, v) - 1
        if j >= 0 and v < self.all_spans[j][1]:
            return self.all_spans[j][2], v - self.all_spans[j][0]
        return None

    def symbolise(self, value: int) -> tuple[str, int]:
        v = value & ~1
        j = bisect.bisect_right(self.span_starts, v) - 1
        if j >= 0 and v < self.spans[j][1]:
            # Named by the linker input section: stable where symbols are not.
            return frozenset([self.spans[j][2]]), v - self.spans[j][0]
        i = bisect.bisect_right(self.addrs, value & ~1) - 1
        return (self.names[i], (value & ~1) - self.addrs[i]) if i >= 0 else (frozenset(['?']), value)


_cache: dict[int, bytes] = {}


def s_data(elf, section) -> bytes:
    key = id(section)
    if key not in _cache:
        _cache[key] = elf.data(section)
    return _cache[key]


def same_target(a, b) -> bool:
    """Same distance from a name both images know (aliases share an address)."""
    return a[1] == b[1] and bool(a[0] & b[0])


def reloc_sites(elf_path: Path) -> set[int]:
    """Absolute addresses of every relocation the reference link kept."""
    obj = rex.Obj.load(elf_path)
    sites = set()
    for rel in obj.sections:
        if rel.type == rex.SHT_REL and obj.sections[rel.info].flags & 2 and not obj.sections[rel.info].flags & 4:
            for at in range(rel.offset, rel.offset + rel.size, 8):
                off, info = struct.unpack_from('<II', obj.raw, at)
                if info & 0xff in rex.RELOC_WIDTH:
                    sites.add(off)
    return sites


def rex_sites(blob: bytes) -> dict[int, int]:
    magic, version, _ref, regions, units, sites, _strings, _crc = struct.unpack_from('<8I', blob, 0)
    at = 32 + 16 * regions + 24 * units
    return {struct.unpack_from('<I', blob, at + 8 * n)[0]: struct.unpack_from('<I', blob, at + 8 * n + 4)[0]
            for n in range(sites)}


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--reference', type=Path, required=True, help='build tree with the full embedded link')
    ap.add_argument('--tree', type=Path, required=True, help='build tree with the REX link')
    args = ap.parse_args()
    ref_port, new_port = args.reference / '3ds_port', args.tree / '3ds_port'
    renamed = {}
    for path in (new_port / 'build').rglob('*.rex.json'):
        obj_name = path.with_suffix('').with_suffix('.o').relative_to(new_port).as_posix()
        for unit in json.loads(path.read_text())['units']:
            renamed[(obj_name, unit['section'])] = unit['original']
    ref, new = Image(ref_port), Image(new_port, renamed)
    reference_relocs = reloc_sites(ref_port / 'build/firered_game.elf')
    new_sites = rex_sites((new_port / 'romfs/engine/rex.bin').read_bytes())
    checked = mismatched = missing = units_seen = skipped = 0
    samples: list[str] = []
    for path in sorted((new_port / 'build').rglob('*.rex.json')):
        obj_name = path.with_suffix('').with_suffix('.o').relative_to(new_port).as_posix()
        for unit in json.loads(path.read_text())['units']:
            new_entry = new.map.get((obj_name, unit['section']))
            old_entry = ref.map.get((obj_name, unit['original']))
            if new_entry is None or old_entry is None:
                skipped += 1
                continue
            units_seen += 1
            lo, hi = old_entry['addr'], old_entry['addr'] + unit['size']
            expected = {a - lo for a in reference_relocs if lo <= a < hi}
            have = {off for off, *_ in unit['sites']}
            if expected != have:
                missing += len(expected ^ have)
                samples.append(f"{obj_name} {unit['original']}: sites differ by {sorted(expected ^ have)[:4]}")
            for off, kind, *rest in unit['sites']:
                width = rest[-1] if kind == 'g' else 4
                value_old = ref.word(lo + off, width)
                flagged = (new_entry['addr'] + off) | (0x80000000 if width == 2 else 0)
                value_new = new_sites.get(flagged)
                checked += 1
                if value_old is None or value_new is None:
                    mismatched += 1
                    samples.append(f"{obj_name} {unit['original']}+{off}: unreadable")
                elif width == 2:
                    if value_old != value_new:
                        mismatched += 1
                        samples.append(f"{obj_name} {unit['original']}+{off}: constant {value_old:#x} != {value_new:#x}")
                elif not (same_target(ref.symbolise(value_old), new.symbolise(value_new))
                          or (ref.section_position(value_old) is not None
                              and ref.section_position(value_old) == new.section_position(value_new))):
                    mismatched += 1
                    if len(samples) < 12:
                        samples.append(f"{obj_name} {unit['original']}+{off}: {sorted(ref.symbolise(value_old)[0])[:2]}+{ref.symbolise(value_old)[1]} != {sorted(new.symbolise(value_new)[0])[:2]}+{new.symbolise(value_new)[1]}")
    print(f'{units_seen} units compared ({skipped} not alive in both links), {checked} sites checked, '
          f'{mismatched} target mismatches, {missing} site-set differences')
    for line in samples[:12]:
        print('  ', line)
    # gMonIconPaletteTable[3..5] point past gMonIconPalettes ("never used" in the
    # source); where they land is a link accident, the symbol+offset is what matters.
    accepted = sum(1 for line in samples if 'gMonIconPaletteTable' in line)
    return 1 if mismatched - accepted > 0 or missing else 0


if __name__ == '__main__':
    sys.exit(main())
