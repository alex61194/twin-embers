"""Writes builder/firered3ds_builder/gen4_followers.py: the HeartGold follower Pokémon the Builder may fetch.

    python tools/make_gen4_followers.py --checkout PATH_TO_POKEHEARTGOLD

PATH_TO_POKEHEARTGOLD is a checkout of pret/pokeheartgold (a sparse one of files/data/mmodel/mmodel,
include/constants/follow_mon_idx.h and include/constants/species.h is enough). The commit it is at is pinned, and every
file the Builder may fetch is listed with its size and SHA-256, so a changed or replaced file upstream is refused, never
converted. Nothing from the checkout is copied into this repository: only indexes, sizes and hashes.

HeartGold draws the Pokémon that walks behind you from one texture archive each (files/data/mmodel/mmodel/
mmodel_NNNNNNNN.NSBTX: eight 32x32 pictures, two a direction, in a normal and a shiny palette). The archives follow
include/constants/follow_mon_idx.h in order, the first (FOLLOWER_MON_BULBASAUR, 0) being mmodel 297. A species with a
female picture of its own has a second entry named like it with _F. The national dex 1-386 are listed; a species is
listed by its constant's name (SPECIES_IVYSAUR is FOLLOWER_MON_IVYSAUR; Unown's, UNOWN_A); other forms are not (each species has its normal one).
"""

import argparse
import hashlib
import re
import subprocess
from pathlib import Path

MMODEL = 'files/data/mmodel/mmodel/mmodel_{:08d}.NSBTX'
BASE = 297
SPECIES = 386
OUT = Path(__file__).resolve().parents[1] / 'builder' / 'firered3ds_builder' / 'gen4_followers.py'


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('--checkout', type=Path, required=True)
    args = parser.parse_args()
    root = args.checkout
    commit = subprocess.run(['git', '-C', str(root), 'rev-parse', 'HEAD'], check=True,
                            capture_output=True, text=True).stdout.strip()
    species = {}
    for name, number in re.findall(r'#define SPECIES_(\w+)\s+(\d+)\s*$',
                                   (root / 'include/constants/species.h').read_text(), re.M):
        if 0 < int(number) <= SPECIES:
            species[name] = int(number)
    follower = {}
    for name, number in re.findall(r'#define FOLLOWER_MON_(\w+)\s+(\d+)\s*$',
                                   (root / 'include/constants/follow_mon_idx.h').read_text(), re.M):
        if name != 'NONE':
            follower[name] = int(number)
    follower_of = {'UNOWN': 'UNOWN_A'}     # Unown's follower entries are its letters; A is the normal one
    rows, missing = [], []
    for name, dex in sorted(species.items(), key=lambda item: item[1]):
        name = follower_of.get(name, name)
        if name not in follower:
            missing.append(name)
            continue
        for body, key in (('male', name), ('female', name + '_F')):
            if body == 'female' and (key not in follower or key in species):
                continue
            index = follower[key]
            data = (root / MMODEL.format(BASE + index)).read_bytes()
            rows.append((dex, body, index, len(data), hashlib.sha256(data).hexdigest()))
    assert not missing, missing
    assert len({row[0] for row in rows}) == SPECIES, len({row[0] for row in rows})
    lines = ['"""HeartGold follower Pokémon the Builder may fetch (tools/make_gen4_followers.py; do not edit).',
             '',
             'A row: (national dex, "male"/"female", the entry\'s number in follow_mon_idx.h, size, SHA-256); its file is',
             'MMODEL_PATH.format(BASE + number) in the pinned commit."""',
             '',
             "REPO = 'pret/pokeheartgold'",
             "COMMIT = '%s'" % commit,
             "MMODEL_PATH = '%s'" % MMODEL,
             'BASE = %d' % BASE,
             'FILES = (']
    lines += ['    (%d, %r, %d, %d, %r),' % row for row in rows]
    lines.append(')')
    OUT.write_text('\n'.join(lines) + '\n', encoding='utf-8', newline='\n')
    print('wrote', OUT, len(rows), 'files,', sum(1 for row in rows if row[1] == 'female'), 'female')


if __name__ == '__main__':
    main()
