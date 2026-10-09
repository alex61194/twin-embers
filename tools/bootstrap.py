"""Build a fresh ignored workspace from pinned external upstream; never reset it."""
import argparse
import hashlib
import json
import shutil
import subprocess
import tempfile
import tomllib
from pathlib import Path
from apply_edits import apply

ROOT = Path(__file__).resolve().parents[1]


def git(path, *args):
    return subprocess.check_output(['git', '-C', str(path), *args], text=True).strip()


def inputs():
    files = [ROOT/'upstream.lock']
    for folder in ('tools', 'builder', '3ds_port', 'patches'):
        files += [p for p in (ROOT/folder).rglob('*') if p.is_file()
                  and not any(x in ('__pycache__', 'build', 'romfs') for x in p.relative_to(ROOT).parts)
                  and p.suffix not in ('.pyc', '.elf', '.o', '.3dsx', '.exe', '.pak')]
    return {p.relative_to(ROOT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in files}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--dir', type=Path, default=ROOT/'build/upstream')
    ap.add_argument('--verify-only', action='store_true')
    args = ap.parse_args()
    dest = args.dir.resolve()
    if (ROOT/'build').resolve() not in dest.parents:
        raise ValueError('Bootstrap must remain strictly inside build/')
    lock = tomllib.loads((ROOT/'upstream.lock').read_text())['pokefirered']
    fingerprint = inputs()
    marker = dest/'.twin-bootstrap.json'
    if dest.exists():
        state = json.loads(marker.read_text())
        if state['inputs'] != fingerprint or git(dest, 'rev-parse', 'HEAD') != lock['commit']:
            raise ValueError('Inputs changed; preserve this build and select a fresh --dir')
        if git(dest, 'remote', 'get-url', 'origin') != lock['repository']:
            raise ValueError('External source origin changed')
        for name, digest in state['files'].items():
            if not (dest/name).is_file() or hashlib.sha256((dest/name).read_bytes()).hexdigest() != digest:
                raise ValueError(f'Local source changed and was preserved: {name}')
        print('Verified existing pinned build:', dest)
        return
    if args.verify_only:
        raise ValueError('Bootstrap workspace does not exist')
    dest.parent.mkdir(parents=True, exist_ok=True)
    stage = Path(tempfile.mkdtemp(prefix='.twin-stage-', dir=dest.parent))
    try:
        git(stage, 'init', '-q')
        git(stage, 'config', 'core.autocrlf', 'false')
        git(stage, 'remote', 'add', 'origin', lock['repository'])
        git(stage, 'fetch', '--depth=1', 'origin', lock['commit'])
        git(stage, 'checkout', '-q', '--detach', lock['commit'])
        apply(stage, sorted((ROOT/'patches/pokefirered').glob('*.json')))
        for name in fingerprint:
            if name.startswith('patches/'):
                continue
            target = stage/name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT/name, target)
        names = set(git(stage, 'ls-files').splitlines()) | {n for n in fingerprint if not n.startswith('patches/')}
        state = {'inputs': fingerprint, 'files': {n: hashlib.sha256((stage/n).read_bytes()).hexdigest() for n in sorted(names)}}
        (stage/'.twin-bootstrap.json').write_text(json.dumps(state, indent=2)+'\n')
        if dest.exists():
            raise ValueError('Destination appeared during preparation; preserved')
        stage.rename(dest)
    except Exception:
        print('Incomplete workspace preserved:', stage)
        raise
    print('Prepared fresh pinned external workspace:', dest)


if __name__ == '__main__':
    main()
