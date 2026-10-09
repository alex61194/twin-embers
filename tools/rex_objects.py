#!/usr/bin/env python3
"""Turn manifest units of one compiled game object into NOBITS storage.

For each unit listed in the REX manifest for this object the section keeps its
symbols, size and alignment, but loses its bytes and its relocations. What the
relocations said is saved next to the object (`<object>.rex.json`) so that
tools/rex_link.py can compute every pointer's final address after the link.

The game compiler never notices: sizeof, symbol names and addresses are those of
the original arrays. The bytes arrive from the data pack before AgbMain().
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import rex  # noqa: E402
from rex import tool  # noqa: E402

PREFIX = '.bss.ctr_rex.'


def unit_section_name(key: str, section: str) -> str:
    return PREFIX + key.replace('/', '.') + ('' if section.startswith('.') else '.') + section


def alias_name(key: str, section: str) -> str:
    return '__ctr_rex_' + hashlib.sha1(f'{key}:{section}'.encode()).hexdigest()[:20]


def target_of(obj: rex.Obj, key: str, reloc: rex.Reloc, data: bytes, renamed: dict[int, str],
              own: int, aliases: dict[str, str]):
    """Describe a pointer site's target so the post-link tool can resolve it.

    ['s', offset]        inside the unit that holds the pointer
    ['g', name, addend]  a global symbol; targets in other local sections are
                         reached through a generated global alias of that section,
                         which also keeps the section alive for the linker
                         (the relocation that used to do so is gone)."""
    sym = obj.symbols[reloc.symbol]
    width = rex.RELOC_WIDTH[reloc.type]
    raw = int.from_bytes(data[reloc.offset:reloc.offset + width], 'little')
    addend = raw - (1 << (8 * width)) if raw >> (8 * width - 1) else raw
    if sym.shndx == rex.SHN_UNDEF or sym.bind in (rex.STB_GLOBAL, rex.STB_WEAK):
        if not sym.name:
            raise ValueError(f'{obj.path}: unnamed global relocation target')
        return ['g', sym.name, addend, width]
    if sym.shndx >= len(obj.sections) or sym.shndx == rex.SHN_ABS:
        raise ValueError(f'{obj.path}: relocation against an absolute symbol {sym.name!r}')
    target = obj.sections[sym.shndx]
    if target.flags & (rex.SHF_MERGE | rex.SHF_STRINGS):
        raise ValueError(f'{obj.path}: pointer into merged section {target.name}')
    if width != 4:
        raise ValueError(f'{obj.path}: {width * 8}-bit relocation against a section')
    if target.index == own:
        return ['s', sym.value + addend]
    final = renamed.get(target.index, target.name)
    alias = aliases.setdefault(final, alias_name(key, final))
    return ['g', alias, sym.value + addend, 4]


def transform(path: Path, manifest: dict) -> list[dict]:
    key = rex.object_key(path)
    wanted = rex.units_by_object(manifest).get(key, {})
    obj = rex.Obj.load(path)
    chosen = []
    for sec in obj.sections:
        unit = wanted.get(sec.name)
        if unit is None or not rex.is_unit_candidate(sec):
            continue
        if sec.size != unit[2]:
            raise ValueError(f'{path}: {sec.name} is {sec.size} bytes, the manifest expects {unit[2]}: '
                             'regenerate the manifest (tools/rex_manifest.py)')
        chosen.append((sec, unit))
    renamed = {sec.index: unit_section_name(key, sec.name) for sec, _ in chosen}
    rows, aliases, command = [], {}, [tool('arm-none-eabi-objcopy')]
    for sec, (entry, offset, size) in chosen:
        data = obj.data(sec)
        sites = []
        for reloc in obj.relocs(sec):
            if reloc.type not in rex.RELOC_WIDTH:
                raise ValueError(f'{path}: relocation type {reloc.type} in {sec.name}')
            sites.append([reloc.offset, *target_of(obj, key, reloc, data, renamed, sec.index, aliases)])
        rows.append(dict(section=renamed[sec.index], original=sec.name, size=size, align=sec.align,
                         entry=entry, offset=offset, sites=sites))
        command += ['--rename-section', f'{sec.name}={renamed[sec.index]},alloc']
        for rel in obj.rel_sections(sec):
            command += ['--remove-section', rel.name]
    for section, name in sorted(aliases.items()):
        command += ['--add-symbol', f'{name}={section}:0,global']
    if chosen:
        temp = path.with_name(f'{path.name}.{os.getpid()}.rex-tmp')
        response = path.with_name(f'{path.name}.{os.getpid()}.rex-rsp')
        response.write_text('\n'.join(json.dumps(a) for a in command[1:] + [str(path), str(temp)]) + '\n')
        subprocess.run([command[0], '@' + str(response)], check=True)
        response.unlink()
        changed = rex.Obj.load(temp)
        for row in rows:
            sec = changed.section(row['section'])
            if sec is None or sec.type != rex.SHT_NOBITS or sec.size != row['size'] or sec.align != row['align']:
                raise ValueError(f'{path}: NOBITS conversion changed {row["original"]}')
        names = {sec.name: renamed[sec.index] for sec, _ in chosen}

        def shape(o, mapping):
            return [(s.name, s.value, s.size, s.bind, s.type,
                     mapping.get(o.sections[s.shndx].name, o.sections[s.shndx].name) if s.shndx < len(o.sections) else s.shndx)
                    for s in o.symbols]
        before = shape(obj, names)
        after = [r for r in shape(changed, {}) if not r[0].startswith('__ctr_rex_')]
        if after != before:
            raise ValueError(f'{path}: symbol table changed by the conversion')
        temp.replace(path)
    path.with_suffix('.rex.json').write_text(json.dumps(dict(key=key, units=rows), separators=(',', ':')) + '\n')
    return rows


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--manifest', type=Path, required=True)
    ap.add_argument('--object', type=Path, required=True)
    args = ap.parse_args()
    rows = transform(args.object.resolve(), rex.load_recipe(args.manifest))
    print(f'{args.object.name}: {len(rows)} data units now load from the pack')


if __name__ == '__main__':
    main()
