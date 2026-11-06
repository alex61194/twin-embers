"""Writes builder/firered3ds_builder/gen4_hd.py: the list of HD battle scene files the Builder may fetch.

    python tools/make_gen4_hd.py --dir EXTRA_DIR --repo OWNER/NAME --commit SHA

EXTRA_DIR is what tools/dev_hd_battlefields.py wrote (graphics/gen4/hdn/NNN.bin, graphics/gen4/hd/NNN.bin and
graphics/gen4/hd/map.bin); the same files, at the same paths, are the content of OWNER/NAME at SHA. The Builder checks
each file's size and SHA-256 against the list, so the list is only as good as the files it was made from.
"""
import argparse
import hashlib
from pathlib import Path

OUT = Path(__file__).resolve().parents[1] / 'builder' / 'firered3ds_builder' / 'gen4_hd.py'
HEAD = '''"""The HD battle scenes the Builder may fetch (tools/make_gen4_hd.py; do not edit).

Art by Bentley734 (HD Dynamic Battlefields), used with the author's permission; see docs/THIRD-PARTY-ART.md.
A row: (its number, "hd", its path in REPO at COMMIT and in the pack, size, SHA-256)."""

REPO = {repo!r}
COMMIT = {commit!r}
FILES = (
'''


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('--dir', type=Path, required=True)
    parser.add_argument('--repo', required=True)
    parser.add_argument('--commit', required=True)
    args = parser.parse_args()
    root = args.dir
    paths = sorted(file.relative_to(root).as_posix() for file in root.rglob('*') if file.is_file())
    kinds = {'graphics/gen4/hdn/': b'G4H1', 'graphics/gen4/hd/': None}
    rows = []
    for number, path in enumerate(paths):
        assert any(path.startswith(prefix) for prefix in kinds), path
        data = (root / path).read_bytes()
        rows.append((number, 'hd', path, len(data), hashlib.sha256(data).hexdigest()))
    assert sum(path.startswith('graphics/gen4/hdn/') for path in paths) > 0
    assert 'graphics/gen4/hd/map.bin' in paths
    text = HEAD.format(repo=args.repo, commit=args.commit)
    text += ''.join('    %r,\n' % (row,) for row in rows) + ')\n'
    OUT.write_text(text, encoding='utf-8', newline='\n')
    print('wrote', OUT, len(rows), 'files,', sum(row[3] for row in rows), 'bytes')


if __name__ == '__main__':
    main()
