"""Assembles the web Builder site (GitHub Pages) from web/ and the Builder package.

    python tools/build_web.py --out build/web [--version v0.3.0] [--date 2026-10-29]

The site is static: web/ as it is, the README's banner and captures (docs/media) in media/, the UniStore with this release's version and date filled in
and its icon sheet (twinembers.t3x, made here from docs/media/twin-embers-icon.png),
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
import struct
import subprocess
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WEB = ROOT / 'web'
PACKAGE = ROOT / 'builder' / 'firered3ds_builder'
PAGES = ('index.html', 'es.html', 'style.css', 'app.js', 'worker.js')
# The README's images the page shows (docs/media, reviewed there): the banner and the gallery's captures.
MEDIA = ROOT / 'docs' / 'media'
# The banner and every README capture (docs/media/NN-name.png); the page's gallery shows them all.
MEDIA_FILES = ('twin-embers-banner.png',) + tuple(sorted(p.name for p in MEDIA.glob('[0-9][0-9]-*.png')))
UNISTORE = 'twinembers.unistore'
# The UniStore's icon sheet: the game's 48x48 icon (docs/media) as a citro2d sprite sheet Universal-Updater loads.
ICON = MEDIA / 'twin-embers-icon.png'
SHEET = 'twinembers.t3x'
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
    if 'sheet' in info and (not isinstance(info['sheet'], str) or '/' in info['sheet'] or not info['sheet']
                            or not isinstance(info['sheetURL'], str) or not info['sheetURL'].startswith('https://')):
        raise ValueError('storeInfo.sheet is a bare file name and storeInfo.sheetURL an HTTPS address')
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
        for key in ('icon_index', 'sheet_index'):
            if key in meta and (type(meta[key]) is not int or meta[key] < 0):
                raise ValueError(f'info.{key} must be a non-negative integer')
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


def png_rgba(raw: bytes) -> tuple[int, int, list[tuple[int, int, int, int]]]:
    """Width, height and RGBA pixels (rows from the top) of an 8-bit RGB or RGBA, non-interlaced PNG."""
    if raw[:8] != b'\x89PNG\r\n\x1a\n':
        raise ValueError('not a PNG image')
    pos, idat, header = 8, b'', None
    while pos < len(raw):
        length, kind = struct.unpack_from('>I4s', raw, pos)
        body = raw[pos + 8:pos + 8 + length]
        if kind == b'IHDR':
            header = struct.unpack('>IIBBBBB', body)
        elif kind == b'IDAT':
            idat += body
        pos += 12 + length
    width, height, depth, color, _, _, interlace = header
    if depth != 8 or color not in (2, 6) or interlace:
        raise ValueError('only 8-bit RGB or RGBA, non-interlaced PNG images')
    bpp = 4 if color == 6 else 3
    data, stride, rows, prev = zlib.decompress(idat), width * bpp, [], bytearray(width * bpp)
    for y in range(height):
        kind, line = data[y * (stride + 1)], bytearray(data[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride):
            a, b, c = line[i - bpp] if i >= bpp else 0, prev[i], prev[i - bpp] if i >= bpp else 0
            if kind == 1:
                line[i] = (line[i] + a) & 255
            elif kind == 2:
                line[i] = (line[i] + b) & 255
            elif kind == 3:
                line[i] = (line[i] + (a + b) // 2) & 255
            elif kind == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append(line)
        prev = line
    pixels = [tuple(row[x * bpp:x * bpp + bpp]) + ((255,) if bpp == 3 else ()) for row in rows for x in range(width)]
    return width, height, pixels


def icon_sheet(raw: bytes) -> bytes:
    """A one-image citro2d sprite sheet (.t3x, as devkitPro's `tex3ds -f rgba8 -z none` writes it) of a PNG icon of at
    most 48x48: a 64x64 RGBA8 texture, the icon in its top-left corner, 8x8 tiles in rows from the top, each tile's
    pixels in Morton order, each pixel as A, B, G, R."""
    width, height, pixels = png_rgba(raw)
    if width > 48 or height > 48:
        raise ValueError('a Universal-Updater icon is at most 48x48')
    size = 64
    header = struct.pack('<HBBB', 1, 3 | 3 << 3, 0, 0)                     # one image; 64x64 (8 << 3); RGBA8; no mipmaps
    header += struct.pack('<6H', width, height, 0, 1024, width * 1024 // size, 1024 - height * 1024 // size)
    texture = bytearray()
    for tile in range(size // 8 * size // 8):
        tx, ty = tile % (size // 8), tile // (size // 8)
        for i in range(64):
            x = tx * 8 + sum(((i >> (2 * b)) & 1) << b for b in range(3))
            y = ty * 8 + sum(((i >> (2 * b + 1)) & 1) << b for b in range(3))
            r, g, b, a = pixels[y * width + x] if x < width and y < height else (0, 0, 0, 0)
            texture += bytes((a, b, g, r))
    return header + struct.pack('<I', len(texture) << 8) + bytes(texture)   # compression header: 0 (none), size


def commit_hour() -> int:
    """The last commit's hour, in the same time zone as commit_date(), for the store's revision."""
    try:
        return int(subprocess.run(['git', '-C', str(ROOT), 'log', '-1', '--format=%cd', '--date=format:%H'],
                                  check=True, capture_output=True, text=True).stdout)
    except (OSError, ValueError, subprocess.CalledProcessError):
        return datetime.datetime.now().hour


def commit_date() -> str:
    try:
        return subprocess.run(['git', '-C', str(ROOT), 'log', '-1', '--format=%cs'], check=True,
                              capture_output=True, text=True).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return datetime.date.today().isoformat()


def unistore(version: str, date: str, hour: int = 0) -> bytes:
    """The UniStore with this build's version; the revision is the date and hour (YYYYMMDDHH), so a later
    publication, on the same day too, makes Universal-Updater fetch the store and its icon sheet again."""
    doc = json.loads((WEB / UNISTORE).read_text(encoding='utf-8'))
    doc['storeInfo']['revision'] = int(date.replace('-', '')) * 100 + hour
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


def assemble(out: Path, version: str, date: str, hour: int = 0) -> list[str]:
    """Write the site to `out`, which must not exist or be empty. Returns the paths written."""
    if not re.fullmatch(r'\d{4}-\d{2}-\d{2}', date):
        raise ValueError('--date is YYYY-MM-DD')
    if out.exists() and any(out.iterdir()):
        raise ValueError(f'{out} is not empty; give a new folder')
    written: dict[str, bytes] = {}
    for name in PAGES:
        written[name] = (WEB / name).read_bytes()
    for name in MEDIA_FILES:
        written[f'media/{name}'] = (MEDIA / name).read_bytes()
    written[UNISTORE] = unistore(version, date, hour)
    written[SHEET] = icon_sheet(ICON.read_bytes())
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
    paths = assemble(args.out, args.version, args.date or commit_date(), 0 if args.date else commit_hour())
    print(f'Wrote {len(paths)} files to {args.out}')


if __name__ == '__main__':
    main()
