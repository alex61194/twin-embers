"""Collect the exact-version third-party notices for the frozen Windows Builder.

Run with the same Python that ran PyInstaller, right after the build:

    python builder/package_notices.py --exe dist/TwinEmbersBuilder.exe --out dist/notices

Every file bundled in the executable is assigned to a component. An unknown file,
a bundled Python that differs from the running interpreter, or a license text that
lacks a required section fails the build, so a new dependency cannot ship without
its notice. License texts are copied from the build environment, never retyped.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.metadata
import re
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# Bundled name pattern -> component. Order matters: the first match wins.
RULES = [
    (r'(launch_gui|firered3ds_builder[\\/].+)', 'twin-embers'),
    (r'(pyiboot\d+_\w+|pyimod\d+_\w+|pyi_rth_\w+)', 'pyinstaller'),
    (r'(python3\d+\.dll|base_library\.zip|PYZ\.pyz|struct|select\.pyd|unicodedata\.pyd'
     r'|_(bz2|hashlib|queue|socket|tkinter|ctypes|overlapped|asyncio|multiprocessing|uuid|wmi)\.pyd)', 'python'),
    (r'lib(crypto|ssl)-3(-x64)?\.dll', 'openssl'),
    (r'(tcl86t\.dll|tk86t\.dll|(_tcl_data|_tk_data|tcl8)[\\/].+)', 'tcltk'),
    (r'(VCRUNTIME140(_1)?\.dll|MSVCP140\.dll|ucrtbase\.dll|api-ms-win-[\w-]+\.dll)', 'microsoft'),
]

COMPONENTS = {
    'twin-embers': 'Twin Embers Builder (alex61194, MIT; inherited ZallaxDev portions, MIT)',
    'pyinstaller': 'PyInstaller bootloader and runtime hooks (GPL-2.0-or-later with the bootloader exception)',
    'python': 'Python runtime and standard library (PSF License; bzip2 statically linked in _bz2.pyd; zlib in the Python DLL)',
    'openssl': 'OpenSSL (Apache License 2.0), used by _hashlib.pyd',
    'tcltk': 'Tcl/Tk (Tcl/Tk license)',
    'microsoft': 'Microsoft Visual C++ runtime and Universal CRT (Microsoft Distributable Code)',
}

# Sections the copied Python license file must contain for the components above.
PYTHON_LICENSE_SECTIONS = {
    'python': r'PYTHON SOFTWARE FOUNDATION LICENSE VERSION 2',
    'microsoft': r'Microsoft\s+Distributable\s+Code',
    'bzip2': r'bzip2',
    'openssl': r'Apache\s+License',
    'tcltk': r'Regents\s+of\s+the\s+University\s+of\s+California',
}

PROJECT_LICENSES = ['LICENSE-PORT.md', 'NOTICE.md', 'licenses/ZallaxDev-LICENSE-PORT.md', 'licenses/ZallaxDev-MIT.txt']


def classify(name: str) -> str:
    for pattern, component in RULES:
        if re.fullmatch(pattern, name, re.IGNORECASE):
            return component
    raise ValueError(f'Unreviewed file bundled in the executable: {name}')


def bundled_names(exe: Path) -> list[str]:
    from PyInstaller.archive.readers import CArchiveReader  # build-time dependency only
    return sorted(CArchiveReader(str(exe)).toc)


def versions() -> dict[str, str]:
    import ssl
    import tkinter
    import zlib
    found = {
        'Python': sys.version.split()[0],
        'PyInstaller': importlib.metadata.version('pyinstaller'),
        'Tcl/Tk': tkinter.Tcl().eval('info patchlevel'),
        'OpenSSL': ssl.OPENSSL_VERSION,
        'zlib': zlib.ZLIB_RUNTIME_VERSION,
    }
    return found


def copy_licenses(out: Path) -> list[str]:
    lic = out / 'licenses'
    lic.mkdir(parents=True, exist_ok=True)
    copied = []
    python_license = Path(sys.base_prefix) / 'LICENSE.txt'
    text = python_license.read_text(encoding='utf-8', errors='replace')
    for section, pattern in PYTHON_LICENSE_SECTIONS.items():
        if not re.search(pattern, text):
            raise ValueError(f'{python_license} lacks the {section} section')
    name = f'Python-{sys.version.split()[0]}-LICENSE.txt'
    shutil.copyfile(python_license, lic / name)
    copied.append(name)
    for sub in ('tcl8.6', 'tk8.6'):
        terms = Path(sys.base_prefix) / 'tcl' / sub / 'license.terms'
        if terms.is_file():
            name = f'{sub}-license.terms.txt'
            shutil.copyfile(terms, lic / name)
            copied.append(name)
    dist = importlib.metadata.distribution('pyinstaller')
    copying = [f for f in dist.files or [] if f.name == 'COPYING.txt']
    if not copying:
        raise ValueError('PyInstaller COPYING.txt not found in its distribution')
    name = f'PyInstaller-{dist.version}-COPYING.txt'
    shutil.copyfile(copying[0].locate(), lic / name)
    copied.append(name)
    for rel in PROJECT_LICENSES:
        name = 'TwinEmbers-' + Path(rel).name
        shutil.copyfile(ROOT / rel, lic / name)
        copied.append(name)
    return copied


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--exe', type=Path, required=True)
    ap.add_argument('--out', type=Path, required=True)
    args = ap.parse_args()

    names = bundled_names(args.exe)
    groups: dict[str, list[str]] = {}
    for name in names:
        groups.setdefault(classify(name), []).append(name)
    expected_dll = f'python{sys.version_info.major}{sys.version_info.minor}.dll'
    if not any(n.lower() == expected_dll for n in groups.get('python', [])):
        raise ValueError(f'The executable does not bundle {expected_dll}; run with the interpreter that built it')

    args.out.mkdir(parents=True, exist_ok=True)
    copied = copy_licenses(args.out)
    digest = hashlib.sha256(args.exe.read_bytes()).hexdigest()
    header = (ROOT / 'builder/Windows-GUI-THIRD-PARTY.txt').read_text(encoding='utf-8').rstrip()
    lines = [header, '', f'Executable: {args.exe.name}  SHA-256 {digest}', '', 'Exact versions in this build:']
    lines += [f'  {k}: {v}' for k, v in versions().items()]
    lines += ['', 'Bundled components (files per component):']
    lines += [f'  {COMPONENTS[c]}: {len(groups[c])}' for c in COMPONENTS if c in groups]
    lines += ['', 'License texts in licenses/:'] + [f'  {n}' for n in copied]
    lines += ['', 'Complete bundle inventory:'] + [f'  [{classify(n)}] {n}' for n in names]
    (args.out / 'THIRD-PARTY-NOTICES.txt').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    print(f'{len(names)} bundled files classified; {len(copied)} license texts; SHA-256 {digest}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
