"""Writes builder/firered3ds_builder/gen4_sources.py: the HeartGold battle sprites the Builder may fetch.

    python tools/make_gen4_sources.py --checkout PATH_TO_POKEHEARTGOLD

PATH_TO_POKEHEARTGOLD is a checkout of pret/pokeheartgold (a sparse one of
files/poketool/pokegra/pokegra is enough). The commit it is at is pinned, and
every file the Builder may fetch is listed with its size and SHA-256, so a
changed or replaced file upstream is refused, never converted. Nothing from the
checkout is copied into this repository: only paths, sizes and hashes.

For national dex 1-386: male/front.png (its palette is the normal one),
male/back.png (its palette is the shiny one), and the female pair where it
differs from the male one. A female-only species has only its female pair.

Unown's letters B to ? (otherpoke/unown, FireRed's order: 1-25 B to Z, 26 !,
27 ?); its A is the species' own picture.

And the menu icons, keyed as the port reads them: 0 the egg, 1-386 the
national dex, 387-413 Unown's letters B to ? (HeartGold's own numbering,
pokemon_icon_idx.c: dex + 7, the egg 501, Unown's forms 507 on).
"""

import argparse
import hashlib
import subprocess
from pathlib import Path

REPO = 'pret/pokeheartgold'
DIR = 'files/poketool/pokegra/pokegra/{:04d}/{}/{}.png'
ICON = 'files/poketool/icongra/poke_icon/poke_icon_{:08d}.png'
UNOWN = 'files/poketool/pokegra/otherpoke/unown/{}/{}.png'
UNOWN_NAMES = [chr(ord('a') + i) for i in range(26)] + ['exclamation_mark', 'question_mark']
UNOWN_LETTERS = 27
SPECIES = 386
OUT = Path(__file__).resolve().parents[1] / 'builder' / 'firered3ds_builder' / 'gen4_sources.py'


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('--checkout', type=Path, required=True)
    args = parser.parse_args()
    root = args.checkout
    commit = subprocess.run(['git', '-C', str(root), 'rev-parse', 'HEAD'], check=True,
                            capture_output=True, text=True).stdout.strip()
    rows = []
    for dex in range(1, SPECIES + 1):
        pair = {}
        for body in ('male', 'female'):
            files = [root / DIR.format(dex, body, side) for side in ('front', 'back')]
            pair[body] = tuple(f.read_bytes() if f.is_file() else b'' for f in files)
        # A female-only species keeps its sprites in female/ and empty files in male/.
        bodies = [b for b in ('male', 'female') if all(pair[b])]
        if len(bodies) == 2 and pair['female'] == pair['male']:
            bodies = ['male']
        if not bodies:
            raise SystemExit(f'no sprites for national dex {dex}')
        for body in bodies:
            for side, data in zip(('front', 'back'), pair[body]):
                rows.append((dex, body, side, data))
    for letter in range(1, 28):
        for side in ('front', 'back'):
            rows.append((letter, 'unown', side, (root / UNOWN.format(UNOWN_NAMES[letter], side)).read_bytes()))
    icons = [(0, 501)] + [(dex, dex + 7) for dex in range(1, SPECIES + 1)]
    icons += [(SPECIES + 1 + letter, 507 + letter) for letter in range(UNOWN_LETTERS)]
    for key, index in icons:
        data = (root / ICON.format(index)).read_bytes()
        rows.append((key, 'icon', index, data))
    lines = ['"""HeartGold battle sprites and menu icons the Builder may fetch (tools/make_gen4_sources.py; do not edit).',
             '',
             'A battle sprite: (national dex, "male"/"female", "front"/"back", size, SHA-256), its path in the',
             'pinned commit PATH.format(dex, body, side). An icon: (key, "icon", HeartGold\'s icon number, size,',
             'SHA-256), its path ICON_PATH.format(number); key 0 is the egg, 1-386 the national dex, 387-413',
             'Unown\'s letters B to ?. An Unown letter: (1-27, "unown", side, size, SHA-256), its path',
             'UNOWN_PATH.format(UNOWN_NAMES[letter], side)."""',
             '',
             f'REPO = {REPO!r}',
             f'COMMIT = {commit!r}',
             f'PATH = {DIR!r}',
             f'ICON_PATH = {ICON!r}',
             f'UNOWN_PATH = {UNOWN!r}',
             f'UNOWN_NAMES = {tuple(UNOWN_NAMES)!r}',
             'FILES = (']
    lines += [f'    ({dex}, {body!r}, {side!r}, {len(data)}, {hashlib.sha256(data).hexdigest()!r}),'
              for dex, body, side, data in rows]
    lines.append(')')
    OUT.write_text('\n'.join(lines) + '\n', encoding='utf-8', newline='\n')
    print(f'{OUT}: {len(rows)} files at {commit}')


if __name__ == '__main__':
    main()
