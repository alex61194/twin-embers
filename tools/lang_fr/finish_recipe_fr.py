#!/usr/bin/env python3
"""Transforme la sortie de gen_recipe_fr.py en recette livrable pour le francais.

    finish_recipe_fr.py recipe-fr.json event_scripts_layout_fr.json

Ecrit builder/firered3ds_builder/recipes/firered-fc663907.json et ses parties (meme
decoupage que la recette USA, tools/split_recipe.py), la disposition des scripts
d'evenements en event_scripts-NN.json, celle des scripts de combat en
battle_scripts_1-00.json, et pending.json : les operations et unites non placees.
Que des offsets, des longueurs et des CRC32.

engine_units sont ceux de la USA (tables de saut cote code, identiques dans les deux
jeux). Tant qu'une entree n'a pas de CRC, l'engine_abi vaut 0 et le Builder refuse
la recette.
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
FR_SHA1 = 'fc663907256f06a3a09e2d6b967bc9af4919f111'

# Scripts de combat francais : 6 octets inseres a l'offset USA 7638 (fix Teeter
# Dance), et le pointeur que le jeu francais ajoute a 7640 (cible USA 7669).
# La cle s'appelle es_size parce que l'outil de generation (tools/lang_es/gen_event_scripts_es.py)
# est partage entre les builds localises ; le couple est identique a l'espagnol et a l'allemand.
BATTLE_LAYOUT = {"es_size": 12554, "inserts": [[7638, 6]], "extra_sites": [[7640, 7669]]}


def layout_parts(layout):
    """event_scripts-NN.json : les scalaires d'abord, puis pos et sites par morceaux < LIMIT."""
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
    fr = json.load(open(recipe_path))
    us = load_recipe(RECIPES / 'firered-41cb23d8.json')
    pending = dict(entries=[e['path'] for e in fr['entries'] if e['crc32'] is None],
                   units=[u[:5] for u in fr['units'] if len(u) > 5])
    units = [u[:5] for u in fr['units']]
    complete = not pending['entries']
    abi = engine_abi((e['path'], e['size'], e['crc32']) for e in fr['entries']) if complete else 0
    assert fr['rom_sha1'] == FR_SHA1, 'la recette ne vient pas de la ROM francaise'
    recipe = dict(schema=2, rom_sha1=fr['rom_sha1'], engine_abi=abi, entries=fr['entries'],
                  units=units, engine_units=us['engine_units'])
    index = RECIPES / 'firered-fc663907.json'
    split(recipe, index)
    folder = RECIPES / 'firered-fc663907'
    folder.mkdir(parents=True, exist_ok=True)
    for n, blob in enumerate(layout_parts(json.load(open(layout_path)))):
        (folder / f'event_scripts-{n:02d}.json').write_text(blob)
    (folder / 'battle_scripts_1-00.json').write_text(
        json.dumps(BATTLE_LAYOUT, separators=(',', ':')) + '\n')
    (folder / 'pending.json').write_text(
        json.dumps(pending, indent=0, separators=(',', ':')) + '\n')
    print(f'{index}: {len(fr["entries"])} entries, {len(units)} units; '
          f'{len(pending["entries"])} entries et {len(pending["units"])} units en attente; '
          f'engine ABI {abi:08x}')


if __name__ == '__main__':
    main(*sys.argv[1:3])
