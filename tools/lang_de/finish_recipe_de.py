#!/usr/bin/env python3
"""Turn gen_recipe_de.py's output into the files the repository ships for German.

    finish_recipe_de.py recipe-de.json event_scripts_layout_de.json

Writes builder/firered3ds_builder/recipes/firered-18a3758c.json and its parts (the
same partitioned form as the USA recipe, tools/split_recipe.py), the German
event-script layout as event_scripts-NN.json in the same folder, battle_scripts_1-00.json,
and pending.json: the operations and units not placed yet. Only offsets, lengths and CRC32s.

engine_units are the USA ones (code-side jump tables and the like, the same in
both games). While any entry has no CRC the recipe's engine_abi is 0 and the
Builder refuses it.
"""
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools')); sys.path.insert(0, str(ROOT / 'builder'))
from split_recipe import split, LIMIT  # noqa: E402
from firered3ds_builder.recipe import load_recipe  # noqa: E402
from firered3ds_builder.pak import engine_abi  # noqa: E402

RECIPES = ROOT / 'builder/firered3ds_builder/recipes'


def layout_parts(layout):
    """event_scripts-NN.json: the scalars first, then pos and sites in pieces under LIMIT."""
    parts = [json.dumps(dict(es_start=layout['es_start'], es_size=layout['es_size']), separators=(',', ':'))]
    for key in ('pos', 'sites'):
        rows, cur = layout[key], []
        for row in rows:
            cur.append(row)
            if len(json.dumps({key: cur}, separators=(',', ':'))) > LIMIT - 64:
                cur.pop(); parts.append(json.dumps({key: cur}, separators=(',', ':'))); cur = [row]
        parts.append(json.dumps({key: cur}, separators=(',', ':')))
    return [p + '\n' for p in parts]


def main(recipe_path, layout_path):
    de = json.load(open(recipe_path))
    us = load_recipe(RECIPES / 'firered-41cb23d8.json')

    # European v1.0 battle scripts gained a 6-byte jumpifhasnohp at offset 7638
    for e in de['entries']:
        if e['path'] == 'gamedata/asm/battle_scripts_1.bin':
            e['size'] = 12554
            e['ops'] = [['C', 1943608, 12554]]
            e['crc32'] = 3334903444
    for u in de['units']:
        if u[0] == 'asm/battle_scripts_1.o':
            u[4] = 12554

    pending = dict(entries=[e['path'] for e in de['entries'] if e['crc32'] is None],
                   units=[u[:5] for u in de['units'] if len(u) > 5])
    units = [u[:5] for u in de['units']]
    complete = not pending['entries']
    abi = engine_abi((e['path'], e['size'], e['crc32']) for e in de['entries']) if complete else 0
    recipe = dict(schema=2, rom_sha1=de['rom_sha1'], engine_abi=abi, entries=de['entries'],
                  units=units, engine_units=us['engine_units'])
    index = RECIPES / 'firered-18a3758c.json'
    split(recipe, index)
    folder = RECIPES / 'firered-18a3758c'
    for n, blob in enumerate(layout_parts(json.load(open(layout_path)))):
        (folder / f'event_scripts-{n:02d}.json').write_text(blob)
    (folder / 'battle_scripts_1-00.json').write_text(
        json.dumps({'es_size': 12554, 'inserts': [[7638, 6]], 'extra_sites': [[7640, 7669]]}, separators=(',', ':')) + '\n'
    )
    (folder / 'pending.json').write_text(json.dumps(pending, indent=0, separators=(',', ':')) + '\n')
    print(f'{index}: {len(de["entries"])} entries, {len(units)} units; '
          f'{len(pending["entries"])} entries and {len(pending["units"])} units pending; engine ABI {abi:08x}')


if __name__ == '__main__':
    main(*sys.argv[1:3])
