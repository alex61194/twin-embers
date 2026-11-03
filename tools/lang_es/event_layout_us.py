#!/usr/bin/env python3
"""Dump the layout of the USA data/event_scripts.o: labels and pointer sites, no bytes.

    event_layout_us.py event_scripts.o out.json

{"size", "syms": [[offset, name, global]...], "relocs": [[offset, type, symbol, addend]...]}
is what gen_event_scripts_es.py reads next to the Spanish layout. Run on the
object as assembled, before tools/rex_objects.py turns it into NOBITS.
"""
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import rex  # noqa: E402

TYPES = {2: 'R_ARM_ABS32'}
STT_SECTION = 3


def layout(path):
    obj = rex.Obj.load(Path(path))
    sec = obj.section('script_data')
    if sec is None:
        raise SystemExit(f'{path}: no script_data section')
    data = obj.data(sec)
    syms = [[y.value, y.name, y.bind in (rex.STB_GLOBAL, rex.STB_WEAK)] for y in obj.symbols
            if y.shndx == sec.index and y.name and y.type != STT_SECTION and not y.name.startswith('$')]
    syms.sort(key=lambda r: r[0])
    relocs = []
    for r in obj.relocs(sec):
        if r.type not in TYPES:
            raise SystemExit(f'{path}: relocation type {r.type} at {r.offset}')
        y = obj.symbols[r.symbol]
        name = obj.sections[y.shndx].name if y.type == STT_SECTION else y.name
        addend = int.from_bytes(data[r.offset:r.offset + 4], 'little', signed=True)
        relocs.append([r.offset, TYPES[r.type], name, addend])
    return dict(size=sec.size, syms=syms, relocs=relocs)


if __name__ == '__main__':
    out = layout(sys.argv[1])
    Path(sys.argv[2]).write_text(json.dumps(out, separators=(',', ':')) + '\n')
    print(f'{sys.argv[2]}: {out["size"]} bytes, {len(out["syms"])} labels, {len(out["relocs"])} relocations')
