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
"""

import argparse
import hashlib
import subprocess
from pathlib import Path

REPO = 'pret/pokeheartgold'
DIR = 'files/poketool/pokegra/pokegra/{:04d}/{}/{}.png'
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
    lines = ['"""HeartGold battle sprites the Builder may fetch (tools/make_gen4_sources.py; do not edit).',
             '',
             'Each file: (national dex, "male"/"female", "front"/"back", size, SHA-256). Its path in the',
             'pinned commit is PATH.format(dex, body, side)."""',
             '',
             f'REPO = {REPO!r}',
             f'COMMIT = {commit!r}',
             f'PATH = {DIR!r}',
             'FILES = (']
    lines += [f'    ({dex}, {body!r}, {side!r}, {len(data)}, {hashlib.sha256(data).hexdigest()!r}),'
              for dex, body, side, data in rows]
    lines.append(')')
    OUT.write_text('\n'.join(lines) + '\n', encoding='utf-8', newline='\n')
    print(f'{OUT}: {len(rows)} files at {commit}')


if __name__ == '__main__':
    main()
