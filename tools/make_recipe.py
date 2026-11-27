#!/usr/bin/env python3
"""Write a schema-1 edit recipe from two trees: the build tree before an edit and the same tree after it.

    make_recipe.py BEFORE_TREE AFTER_TREE OUT.json relative/path [relative/path ...]

Lines that are unchanged become spans of the BEFORE file; an added line that already exists in the BEFORE file is a
`line` reference to it (so the recipe never repeats upstream text it can point at); every other added line is a `port`
line. The result is checked by applying it to a copy of BEFORE_TREE (tools/apply_edits.py) and comparing to AFTER_TREE.
"""
import difflib
import hashlib
import json
import shutil
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from apply_edits import apply  # noqa: E402

TECH = '96f95e015af9f5f38e4a8aefca7cab29a78b030d'


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def row_for(before, after, name):
    b = (before / name)
    a = (after / name)
    braw = b.read_bytes() if b.exists() else None
    araw = a.read_bytes() if a.exists() else None
    row = {'path': name, 'before_sha256': sha(braw) if braw is not None else None,
           'after_sha256': sha(araw) if araw is not None else None, 'remove': araw is None, 'lines': []}
    if araw is None:
        return row
    bl = [] if braw is None else braw.decode('utf-8').splitlines(keepends=True)
    al = araw.decode('utf-8').splitlines(keepends=True)
    index = {}
    for i, t in enumerate(bl):
        index.setdefault(t, i)
    out = row['lines']
    for tag, i1, i2, j1, j2 in difflib.SequenceMatcher(None, bl, al, autojunk=False).get_opcodes():
        if tag == 'equal':
            out.append({'span': [i1, i2 - i1]} if i2 - i1 > 1 else {'line': i1})
        elif tag in ('replace', 'insert'):
            for t in al[j1:j2]:
                if not t.endswith('\n'):
                    t += '\n'
                out.append({'line': index[t]} if t in index else {'port': t})
    # merge neighbouring single-line references into spans where they are consecutive
    merged = []
    for ins in out:
        if merged and 'line' in ins and 'span' in merged[-1] and merged[-1]['span'][0] + merged[-1]['span'][1] == ins['line']:
            merged[-1]['span'][1] += 1
        else:
            merged.append(ins)
    row['lines'] = merged
    return row


def main():
    before, after, out = Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3])
    names = sys.argv[4:]
    doc = {'schema': 1, 'technical_reference': TECH, 'files': [row_for(before, after, n) for n in names]}
    text = json.dumps(doc, separators=(',', ':')) + '\n'
    out.write_text(text, encoding='utf-8')
    with tempfile.TemporaryDirectory() as tmp:
        for n in names:
            if (before / n).exists():
                (Path(tmp) / n).parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(before / n, Path(tmp) / n)
        apply(tmp, [out])
        for n in names:
            got = Path(tmp) / n
            want = after / n
            assert (got.exists() == want.exists()) and (not want.exists() or got.read_bytes() == want.read_bytes()), n
    print(f'{out} ok: {len(names)} file(s)')


if __name__ == '__main__':
    main()
