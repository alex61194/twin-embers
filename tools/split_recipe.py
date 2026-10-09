#!/usr/bin/env python3
"""Write a flat schema-2 recipe as the partitioned form the repository ships.

The flat recipe comes from tools/rex_manifest.py. The parts are compact JSON lists,
each at most LIMIT bytes (reviewable and under the Builder's per-part cap), filled
greedily in recipe order, plus an index that pins every part by SHA-256.
Running it on an unchanged recipe reproduces the committed files byte for byte.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

LIMIT = 180000
KEYS = ('entries', 'units', 'engine_units')


def parts_of(key: str, rows: list) -> list[bytes]:
    head, tail = f'{{"{key}":['.encode(), b']}\n'
    parts, current = [], []
    size = len(head) + len(tail)
    for row in rows:
        blob = json.dumps(row, separators=(',', ':')).encode()
        if current and size + len(blob) + 1 > LIMIT:
            parts.append(head + b','.join(current) + tail)
            current, size = [], len(head) + len(tail)
        size += len(blob) + (1 if current else 0)
        current.append(blob)
    parts.append(head + b','.join(current) + tail)
    return parts


def split(recipe: dict, out_index: Path) -> list[Path]:
    stem = out_index.stem
    folder = out_index.parent / stem
    folder.mkdir(parents=True, exist_ok=True)
    index = {k: recipe[k] for k in ('schema', 'rom_sha1', 'engine_abi')}
    index['recipe_parts'] = []
    written = []
    for key in KEYS:
        for n, blob in enumerate(parts_of(key, recipe[key])):
            target = folder / f'{key}-{n:02d}.json'
            target.write_bytes(blob)
            written.append(target)
            index['recipe_parts'].append(dict(path=f'{stem}/{target.name}', sha256=hashlib.sha256(blob).hexdigest()))
    for old in sorted(folder.glob('*.json')):
        if old not in written:
            old.unlink()
    out_index.write_bytes((json.dumps(index, separators=(',', ':')) + '\n').encode())
    return written


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('recipe', type=Path, help='flat recipe from rex_manifest.py')
    ap.add_argument('index', type=Path, help='partitioned recipe index to write')
    args = ap.parse_args()
    files = split(json.loads(args.recipe.read_text()), args.index)
    print(f'{args.index}: {len(files)} parts')


if __name__ == '__main__':
    main()
