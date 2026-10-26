"""Write a reproducible build-identity header when the checkout identity changes."""
import argparse
from pathlib import Path
import subprocess

p = argparse.ArgumentParser()
p.add_argument('repository', type=Path)
p.add_argument('output', type=Path)
p.add_argument('--version', required=True, help='the release this checkout builds, e.g. 0.2.0 (3ds_port/Makefile)')
p.add_argument('--release', default='0', help='1: label the build with the version alone, without its commit')
a = p.parse_args()
root = a.repository.resolve()
commit = subprocess.check_output(['git', 'rev-parse', '--short=12', 'HEAD'], cwd=root, text=True).strip()
dirty = bool(subprocess.check_output(['git', 'status', '--porcelain', '--untracked-files=no'], cwd=root, text=True).strip())
# The title screen's label: "v0.2.0" for the release itself (RELEASE=1, or HEAD tagged v0.2.0),
# "v0.2.0-1a2b3c4" for any build in between, so a screenshot names the exact commit.
tags = subprocess.run(['git', 'tag', '--points-at', 'HEAD'], cwd=root, text=True, capture_output=True).stdout.split()
label = f'v{a.version}'
if a.release != '1' and label not in tags:
    label += f'-{commit[:7]}'
content = (f'#define CTR_BUILD_GIT "{commit}"\n#define CTR_BUILD_DIRTY {int(dirty)}\n'
           f'#define CTR_BUILD_LABEL "{label}"\n')
a.output.parent.mkdir(parents=True, exist_ok=True)
if not a.output.exists() or a.output.read_text() != content:
    a.output.write_text(content)
