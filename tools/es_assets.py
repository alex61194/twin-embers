"""The graphics the Spanish game changes: path -> (size, CRC32) from the Spanish recipe.

The Spanish build (TWIN_LANG=es) links against the English decomp's files, so those whose Spanish version has another
size are sized like the Spanish one before the game compiles (prepare_game_sources.py), and the asset index
(asset_bundle.py) records the Spanish CRC, which the pack's file has to match.
"""
import json
import os
from pathlib import Path

RECIPE = Path(__file__).resolve().parents[1] / 'builder/firered3ds_builder/recipes/firered-ab8f6bfe'


def spanish():
    return os.environ.get('TWIN_LANG') == 'es'


def files():
    out = {}
    for part in sorted(RECIPE.glob('entries-*.json')):
        for entry in json.loads(part.read_text())['entries']:
            if entry['path'].startswith(('graphics/', 'data/')):
                out[entry['path']] = (entry['size'], entry['crc32'])
    return out
