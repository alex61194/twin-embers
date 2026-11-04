"""The graphics the localized game changes: path -> (size, CRC32) from the localized recipe.

The localized build (TWIN_LANG=es or de) links against the English decomp's files, so those whose localized version has another
size are sized like the localized one before the game compiles (prepare_game_sources.py), and the asset index
(asset_bundle.py) records the localized CRC, which the pack's file has to match.
"""
import json
import os
from pathlib import Path

RECIPES = {
    'es': Path(__file__).resolve().parents[1] / 'builder/firered3ds_builder/recipes/firered-ab8f6bfe',
    'de': Path(__file__).resolve().parents[1] / 'builder/firered3ds_builder/recipes/firered-18a3758c',
}


def spanish():
    return os.environ.get('TWIN_LANG') in RECIPES


def files():
    lang = os.environ.get('TWIN_LANG')
    recipe_dir = RECIPES.get(lang)
    if not recipe_dir:
        return {}
    out = {}
    for part in sorted(recipe_dir.glob('entries-*.json')):
        for entry in json.loads(part.read_text())['entries']:
            if entry['path'].startswith(('graphics/', 'data/')):
                out[entry['path']] = (entry['size'], entry['crc32'])
    return out
