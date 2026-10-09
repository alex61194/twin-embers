#!/usr/bin/env python3
"""Compare every live pack asset with the untouched embedded object payload."""
import argparse
import json
from pathlib import Path
import sys
import zlib
from asset_bundle import Elf

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'builder'))
from firered3ds_builder.pak import PakReader


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--tree', type=Path, required=True)
    ap.add_argument('--baseline', type=Path, required=True)
    args = ap.parse_args()
    port = args.tree / '3ds_port'
    rows = json.loads((port / 'build/graphics-assets.json').read_text())
    linked = Elf(port / 'build/firered_game.elf')
    cache = {}
    with PakReader(port / 'build/twinembers.pak') as pak:
        count = pak.verify()
        for row in rows:
            name = row['source'].removesuffix('.incbin.i') + '.o'
            if name not in cache:
                cache[name] = Elf(args.baseline / '3ds_port/build/game' / name)
            elf = cache[name]
            section = elf.sections[elf.names.index('.rodata.' + row['symbol'])]
            original = elf.data(section)[row['offset']:row['offset'] + row['size']]
            actual = pak.read(row['path'])
            if (actual != original or len(actual) != row['size'] or zlib.crc32(actual) != row['crc']
                or section[8] != row['alignment']
                or not any(sym[0] + row['offset'] == row['address']
                           for sym in linked.symbol_rows.get(row['symbol'], []))):
                raise ValueError('Original bytes/alignment/interior symbol mismatch: ' + row['path'])
    print(f'PASS: {len(rows)} live ranges match original embedded bytes/CRC/alignment/symbol offsets; {count} pack entries verified')


if __name__ == '__main__':
    main()
