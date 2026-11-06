"""Writes builder/firered3ds_builder/gen4_looks.py: the player looks of Emerald Lens the Builder may fetch.

    python tools/make_gen4_looks.py --commit SHA

Reads github.com/ElvieBlooms/emerald_lens at SHA (needs internet): for each look listed in LOOKS below, its FireRed/LeafGreen
sheets (assets/sprites/<look>/gen3/frlg/<sheet>.png) with their size and SHA-256, and its meta.json (label, character, gender
and credit). The Builder checks each file against that list; a look that lacks a sheet keeps FireRed's own picture for it.
"""
import argparse
import hashlib
import json
import urllib.request
from pathlib import Path

REPO = 'ElvieBlooms/emerald_lens'
OUT = Path(__file__).resolve().parents[1] / 'builder' / 'firered3ds_builder' / 'gen4_looks.py'
# The looks offered, in the order the game lists them (the folder names of assets/sprites).
LOOKS = ('ash', 'ash_hgss', 'ash_hatless', 'ash_backwards', 'gary_kanto', 'silver_hgss', 'gold', 'gold_hgss',
         'brendan_oras', 'brendan_hgss', 'green', 'green_hgss', 'green_lou', 'kris', 'kris_blue', 'kris_hgss',
         'may_oras', 'may_hgss')
SHEETS = ('walk', 'bike', 'surf', 'fieldMove', 'fishing', 'vsSeekerBike', 'flyBird', 'back', 'front', 'intro', 'mapIcon')
HEAD = '''"""The player looks of Emerald Lens the Builder may fetch (tools/make_gen4_looks.py; do not edit).

By the artists named in each look's credit, shared in ElvieBlooms' Emerald Lens (https://github.com/ElvieBlooms/emerald_lens),
used with the author's permission; see docs/THIRD-PARTY-ART.md. LOOKS: (folder, label, character, gender, credit); a
row of FILES: (the look's number in LOOKS, "look", the sheet's path in REPO at COMMIT, size, SHA-256)."""

REPO = {repo!r}
COMMIT = {commit!r}
'''


def get(commit: str, path: str) -> bytes:
    url = f'https://raw.githubusercontent.com/{REPO}/{commit}/{path}'
    with urllib.request.urlopen(url, timeout=60) as response:
        return response.read()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('--commit', required=True)
    args = parser.parse_args()
    looks, files = [], []
    for number, folder in enumerate(LOOKS):
        meta = json.loads(get(args.commit, f'assets/sprites/{folder}/meta.json'))
        gender = meta.get('genderMode', 'boy')
        credit = '; '.join(meta.get('credit', []))
        looks.append((folder, meta.get('label', folder.upper()), meta.get('character', folder.upper()), gender, credit))
        for sheet in SHEETS:
            path = f'assets/sprites/{folder}/gen3/frlg/{sheet}.png'
            try:
                data = get(args.commit, path)
            except Exception:  # noqa: BLE001 - this look has no such sheet
                continue
            files.append((number, 'look', path, len(data), hashlib.sha256(data).hexdigest()))
        assert any(row[0] == number and row[2].endswith('/walk.png') for row in files), folder
    text = HEAD.format(repo=REPO, commit=args.commit)
    text += 'LOOKS = (\n' + ''.join('    %r,\n' % (look,) for look in looks) + ')\n'
    text += 'FILES = (\n' + ''.join('    %r,\n' % (row,) for row in files) + ')\n'
    OUT.write_text(text, encoding='utf-8', newline='\n')
    print('wrote', OUT, len(looks), 'looks,', len(files), 'files')


if __name__ == '__main__':
    main()
