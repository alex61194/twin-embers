#!/usr/bin/env python3
"""Byte-equivalence of the committed recipe against a known-good private build.

Development check (needs a ROM and an untransformed build tree, both private):

  1. rebuild every pack entry from the ROM with the Builder code;
  2. every REX unit: the bytes the pack gives the engine equal the bytes the
     compiler put in the object, everywhere except pointer words;
  3. every graphics entry equals the INCBIN payload file byte for byte.

End users never run this; it is how the recipe is proven, not how it is used.
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'builder'))
import rex  # noqa: E402
from firered3ds_builder.recipe import reconstruct, load_recipe  # noqa: E402
from firered3ds_builder.rom import load_rom  # noqa: E402


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--tree', type=Path, required=True, help='build tree compiled before REX (embedded link)')
    ap.add_argument('--rom', type=Path, required=True)
    ap.add_argument('--recipe', type=Path, default=Path(__file__).resolve().parents[1] / 'builder/firered3ds_builder/recipes/firered-41cb23d8.json')
    args = ap.parse_args()
    recipe = load_recipe(args.recipe)
    files = dict(reconstruct(recipe, load_rom(args.rom).data))
    entries = recipe['entries']
    build = args.tree / '3ds_port/build'
    checked = bad = 0
    objects: dict[str, rex.Obj] = {}
    for key, section, entry, offset, size in recipe['units']:
        kind, name = key.split('/', 1)
        directory = {'c': 'game', 'asm': 'game_data', 'song': 'game_song'}[kind]
        path = build / directory / name
        if key not in objects:
            objects[key] = rex.Obj.load(path)
        obj = objects[key]
        sec = obj.section(section)
        if sec is None:
            continue  # the compiler dropped it; the engine never asks for it
        mine = bytearray(obj.data(sec))
        theirs = bytearray(files[entries[entry]['path']][offset:offset + size])
        for reloc in obj.relocs(sec):
            width = rex.RELOC_WIDTH[reloc.type]
            mine[reloc.offset:reloc.offset + width] = bytes(width)
            theirs[reloc.offset:reloc.offset + width] = bytes(width)
        checked += 1
        if mine != theirs:
            bad += 1
            print('MISMATCH', key, section)
    graphics = 0
    for entry in entries:
        if entry['path'].startswith(rex.REGION_PREFIX):
            continue
        local = args.tree / entry['path']
        graphics += 1
        if not local.is_file() or local.read_bytes() != files[entry['path']]:
            bad += 1
            print('MISMATCH graphics', entry['path'])
    print(f'{checked} REX units and {graphics} graphics entries compared with the private build: '
          f'{bad} differences; {len(files)} pack entries reconstructed')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
