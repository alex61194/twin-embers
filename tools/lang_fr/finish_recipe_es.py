#!/usr/bin/env python3
"""Turn gen_recipe_es.py's output into the files the repository ships for Spanish.

    finish_recipe_es.py recipe-es.json event_scripts_layout_es.json

Writes builder/firered3ds_builder/recipes/firered-ab8f6bfe.json and its parts (the
same partitioned form as the USA recipe, tools/split_recipe.py), the Spanish
event-script layout as event_scripts-NN.json in the same folder, and pending.json:
the operations and units not placed yet. Only offsets, lengths and CRC32s.

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
    es = json.load(open(recipe_path))
    us = load_recipe(RECIPES / 'firered-41cb23d8.json')
    pending = dict(entries=[e['path'] for e in es['entries'] if e['crc32'] is None],
                   units=[u[:5] for u in es['units'] if len(u) > 5])
    units = [u[:5] for u in es['units']]
    complete = not pending['entries']
    abi = engine_abi((e['path'], e['size'], e['crc32']) for e in es['entries']) if complete else 0
    recipe = dict(schema=2, rom_sha1=es['rom_sha1'], engine_abi=abi, entries=es['entries'],
                  units=units, engine_units=us['engine_units'])
    index = RECIPES / 'firered-ab8f6bfe.json'
    split(recipe, index)
    folder = RECIPES / 'firered-ab8f6bfe'
    for n, blob in enumerate(layout_parts(json.load(open(layout_path)))):
        (folder / f'event_scripts-{n:02d}.json').write_text(blob)
    (folder / 'pending.json').write_text(json.dumps(pending, indent=0, separators=(',', ':')) + '\n')
    print(f'{index}: {len(es["entries"])} entries, {len(units)} units; '
          f'{len(pending["entries"])} entries and {len(pending["units"])} units pending; engine ABI {abi:08x}')


if __name__ == '__main__':
    main(*sys.argv[1:3])
