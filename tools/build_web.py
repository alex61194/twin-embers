"""Assembles the web Builder site (GitHub Pages) from web/ and the Builder package.

    python tools/build_web.py --out build/web [--version v0.3.0] [--date 2026-10-29]

The site is static: web/ as it is, the UniStore with this release's version and date filled in,
and the Builder modules and recipe the page runs with Pyodide, listed with their SHA-256 in
builder/files.json. No ROM, pack or game data is ever part of it. Serve the folder locally with
`python -m http.server -d build/web` to try it (crypto.subtle needs localhost or HTTPS).
"""
from __future__ import annotations

import argparse
import datetime
import hashlib
import json
import os
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WEB = ROOT / 'web'
PACKAGE = ROOT / 'builder' / 'firered3ds_builder'
PAGES = ('index.html', 'style.css', 'app.js', 'worker.js')
UNISTORE = 'twinembers.unistore'
# What firered3ds_builder.browser imports, directly or not. The GUI, CLI and Windows parts stay out.
MODULES = ('__init__.py', 'browser.py', 'build.py', 'rom.py', 'recipe.py', 'pak.py',
           'gen4_sprites.py', 'gen4_sources.py')

# Universal-Updater's own rules (source/store/store.cpp, storeEntry.cpp, script.cpp): what it
# needs to accept a store, and the fields each script action must have.
ACTIONS = {
    'copy': ('source', 'destination'), 'deleteFile': ('file',), 'downloadFile': ('file', 'output'),
    'downloadRelease': ('repo', 'file', 'output'), 'exit': (), 'extractFile': ('file', 'input', 'output'),
    'installCia': ('file',), 'mkdir': ('directory',), 'move': ('old', 'new'),
    'promptMessage': ('message',), 'promptMsg': ('message',), 'rmdir': ('directory',), 'skip': (),
}


def validate_unistore(doc: object) -> None:
    """Raise ValueError unless Universal-Updater 3.x would load and run this UniStore."""
    if not isinstance(doc, dict) or not isinstance(doc.get('storeInfo'), dict) \
            or not isinstance(doc.get('storeContent'), list):
        raise ValueError('a UniStore is an object with storeInfo and a storeContent list')
    info = doc['storeInfo']
    if type(info.get('version')) is not int or info['version'] not in (3, 4):
        raise ValueError('storeInfo.version must be the integer 3 or 4')
    if type(info.get('revision')) is not int or info['revision'] < 0:
        raise ValueError('storeInfo.revision must be a non-negative integer')
    for key in ('title', 'file', 'url'):
        if not isinstance(info.get(key), str) or not info[key]:
            raise ValueError(f'storeInfo.{key} must be a non-empty string')
    if '/' in info['file'] or not info['url'].startswith('https://'):
        raise ValueError('storeInfo.file is a bare file name and storeInfo.url an HTTPS address')
    if ('sheet' in info) != ('sheetURL' in info):
        raise ValueError('sheet and sheetURL go together')
    if not doc['storeContent']:
        raise ValueError('the store lists no entry')
    for entry in doc['storeContent']:
        meta = entry.get('info') if isinstance(entry, dict) else None
        if not isinstance(meta, dict) or not isinstance(meta.get('title'), str) or not meta['title']:
            raise ValueError('every entry needs info.title')
        for key in ('author', 'version', 'description', 'license', 'last_updated', 'releasenotes', 'wiki',
                    'preinstall_message', 'llm_generation'):
            if key in meta and not isinstance(meta[key], str):
                raise ValueError(f'info.{key} must be a string')
        for key in ('category', 'console'):
            if key in meta and not (isinstance(meta[key], list) and all(isinstance(v, str) for v in meta[key])):
                raise ValueError(f'info.{key} must be a list of strings')
        scripts = {name: value for name, value in entry.items() if name != 'info'}
        if not scripts:
            raise ValueError(f'{meta["title"]} has nothing to install')
        for name, script in scripts.items():
            if isinstance(script, dict):
                script = script.get('script')
            if not isinstance(script, list) or not script:
                raise ValueError(f'script {name!r} is not a list of actions')
            for action in script:
                kind = action.get('type') if isinstance(action, dict) else None
                if kind not in ACTIONS:
                    raise ValueError(f'script {name!r}: unknown action {kind!r}')
                for field in ACTIONS[kind]:
                    if not isinstance(action.get(field), str) or not action[field]:
                        raise ValueError(f'script {name!r}: {kind} needs {field}')
                if kind == 'downloadFile' and not action['file'].startswith('https://'):
                    raise ValueError(f'script {name!r}: downloads must use HTTPS')


def commit_date() -> str:
    try:
        return subprocess.run(['git', '-C', str(ROOT), 'log', '-1', '--format=%cs'], check=True,
                              capture_output=True, text=True).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return datetime.date.today().isoformat()


def unistore(version: str, date: str) -> bytes:
    """The UniStore with this build's version; the revision is the date, so a release on a later day
    makes Universal-Updater fetch the store again."""
    doc = json.loads((WEB / UNISTORE).read_text(encoding='utf-8'))
    doc['storeInfo']['revision'] = int(date.replace('-', ''))
    for entry in doc['storeContent']:
        entry['info']['version'] = version
        entry['info']['last_updated'] = date
    validate_unistore(doc)
    return (json.dumps(doc, indent=2, ensure_ascii=True) + '\n').encode('ascii')


def builder_files() -> list[tuple[str, bytes]]:
    files = [(f'firered3ds_builder/{name}', (PACKAGE / name).read_bytes()) for name in MODULES]
    for path in sorted((PACKAGE / 'recipes').rglob('*.json')):
        files.append((path.relative_to(PACKAGE.parent).as_posix(), path.read_bytes()))
    return files


def assemble(out: Path, version: str, date: str) -> list[str]:
    """Write the site to `out`, which must not exist or be empty. Returns the paths written."""
    if not re.fullmatch(r'\d{4}-\d{2}-\d{2}', date):
        raise ValueError('--date is YYYY-MM-DD')
    if out.exists() and any(out.iterdir()):
        raise ValueError(f'{out} is not empty; give a new folder')
    written: dict[str, bytes] = {}
    for name in PAGES:
        written[name] = (WEB / name).read_bytes()
    written[UNISTORE] = unistore(version, date)
    written['.nojekyll'] = b''
    files = builder_files()
    listing = {'version': version, 'files': [{'path': path, 'sha256': hashlib.sha256(data).hexdigest()}
                                             for path, data in files]}
    for path, data in files:
        written[f'builder/{path}'] = data
    written['builder/files.json'] = (json.dumps(listing, indent=1) + '\n').encode('ascii')
    for path, data in written.items():
        target = out / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    return sorted(written)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--version', default=os.environ.get('TWIN_EMBERS_VERSION') or 'latest')
    parser.add_argument('--date', default=None, help='release date, YYYY-MM-DD (default: last commit)')
    args = parser.parse_args()
    paths = assemble(args.out, args.version, args.date or commit_date())
    print(f'Wrote {len(paths)} files to {args.out}')


if __name__ == '__main__':
    main()
