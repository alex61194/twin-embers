#!/usr/bin/env python3
"""Inventory file-backed FireRed assets without converting unrelated tables.

Raw source references include inactive version/language alternatives. Active
preprocessed declarations and linked live assets are reported separately.
"""
import argparse
import json
from pathlib import Path
import re

PREFIXES = {
    'fonts-common-ui': ('graphics/fonts/', 'graphics/text_window/'),
    'pokemon-battle': ('graphics/pokemon/', 'graphics/battle_anims/', 'graphics/battle_interface/', 'graphics/trainers/'),
    'overworld-tilesets': ('graphics/object_events/', 'graphics/field_effects/', 'data/tilesets/'),
    'intro-title': ('graphics/title_screen/', 'graphics/intro/'),
}
CALL = re.compile(r'INCBIN_([US](?:8|16|32))\s*\(([^)]*)\)|\.incbin\s+("[^"\n]+")')


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--tree', type=Path, required=True)
    ap.add_argument('--out', type=Path, required=True)
    args = ap.parse_args()
    rows = []
    for directory in ('src', 'data', 'include'):
        for source in sorted((args.tree / directory).rglob('*')):
            if source.suffix not in ('.c', '.h', '.s', '.inc'):
                continue
            text = source.read_text(encoding='utf-8')
            for call in CALL.finditer(text):
                for argument in (call[2] or call[3]).split(','):
                    path = ''.join(re.findall(r'"([^"\n]*)"', argument))
                    if not path.startswith(('graphics/', 'data/')):
                        continue
                    category = next((g for g, prefixes in PREFIXES.items() if path.startswith(prefixes)), 'remaining')
                    file = args.tree / path
                    rows.append(dict(source=source.relative_to(args.tree).as_posix(),
                        line=text.count('\n', 0, call.start()) + 1, path=path,
                        type=call[1] or 'asm', category=category,
                        size=file.stat().st_size if file.is_file() else None))
    summary = {}
    for category in sorted({r['category'] for r in rows}):
        group = [r for r in rows if r['category'] == category]
        unique = {r['path']: r['size'] for r in group}
        summary[category] = dict(references=len(group), unique_paths=len(unique),
            available_bytes=sum(n for n in unique.values() if n is not None),
            missing_paths=sum(n is None for n in unique.values()))
    result = dict(note='Raw references include inactive variants; sizes preserve original compression.',
                  summary=summary, assets=rows)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(summary, indent=2))


if __name__ == '__main__':
    main()
