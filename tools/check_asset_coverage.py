#!/usr/bin/env python3
"""Fail if a selected live INCBIN array remains in a file-backed ELF section."""
import argparse
import json
from pathlib import Path
import re
import sys
from asset_bundle import ARRAY, SPOT, INCBIN, GROUPS, Elf, declarations


def selected(path):
    return any(path.startswith(prefixes) for prefixes in GROUPS.values())


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--tree', type=Path, required=True)
    args = ap.parse_args()
    port = args.tree / '3ds_port'
    text = (port / 'build/firered_game.map').read_text(errors='replace').split('Linker script and memory map', 1)[1]
    live = {(section, obj.replace('\\', '/')) for section, size, obj in re.findall(
        r'^\s+(\.\S+)\s+0x[0-9a-f]+\s+0x([0-9a-f]+)\s+(\S+\.o)\s*$', text, re.M)}
    checked = emitted = eliminated = discarded = 0
    issues = []
    for pre in sorted((port / 'build/game').rglob('*.incbin.i')):
        source = pre.read_text(encoding='utf-8')
        stem = pre.relative_to(port / 'build/game').as_posix().removesuffix('.incbin.i')
        obj_name = 'build/game/' + stem + '.o'
        obj = port / obj_name
        elf = None
        pure = [(match, list(declarations(match[0]))) for pattern in (ARRAY, SPOT) for match in pattern.finditer(source)]
        spans = [(m.start(), m.end()) for m, decl in pure if decl]
        for call in INCBIN.finditer(source):
            paths = [''.join(re.findall(r'"([^"\n]*)"', arg)) for arg in call[1].split(',')]
            if any(selected(p) for p in paths) and not any(a <= call.start() < b for a, b in spans):
                issues.append(dict(source=stem, problem='unclassified INCBIN declaration', paths=paths))
        for match, decls in pure:
            for symbol, paths in decls:
                if not any(selected(p) for p in paths):
                    continue
                checked += 1
                if elf is None: elf = Elf(obj)
                candidates = [(name, value) for name, value in elf.symbols.items()
                              if name == symbol or name.startswith(symbol + '.')]
                if not candidates:
                    eliminated += 1
                    continue
                for name, value in candidates:
                    ndx = value[3]
                    if ndx >= len(elf.sections):
                        issues.append(dict(source=stem, symbol=name, problem='unexpected symbol section'))
                        continue
                    section = elf.names[ndx]
                    if (section, obj_name) not in live:
                        discarded += 1
                    elif section.startswith('.bss.ctr_asset.') and elf.sections[ndx][1] == 8:
                        emitted += 1
                    else:
                        issues.append(dict(source=stem, symbol=name, section=section, problem='live file-backed asset remains embedded'))
    print(json.dumps(dict(selected_declarations=checked, live_external_arrays=emitted,
        compiler_eliminated=eliminated, linker_discarded=discarded, issues=issues), indent=2))
    if issues: sys.exit('Asset coverage is incomplete; preserve the previous checkpoint')


if __name__ == '__main__':
    main()
