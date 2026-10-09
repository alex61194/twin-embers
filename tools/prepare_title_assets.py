#!/usr/bin/env python3
"""Generate only the local INCBIN inputs needed by FireRed's intro and title."""

from __future__ import annotations

import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCES = (ROOT / "src" / "intro.c", ROOT / "src" / "title_screen.c")
INCBIN = re.compile(r'INCBIN_U(?:8|16|32)\(\s*"([^"]+)"')


def main() -> None:
    assets = sorted({name for source in SOURCES
                     for name in INCBIN.findall(source.read_text(encoding="utf-8"))})
    if not assets or any(not name.startswith("graphics/") or ".." in Path(name).parts
                         for name in assets):
        raise ValueError("unexpected title asset path")

    subprocess.run(["make", "-C", "tools/gbagfx"], cwd=ROOT, check=True)
    subprocess.run(["make", "-j4", "SETUP_PREREQS=0", "NODEP=1", *assets],
                   cwd=ROOT, check=True)
    missing = [name for name in assets if not (ROOT / name).is_file()]
    if missing:
        raise FileNotFoundError("title assets missing after generation: " + ", ".join(missing))
    print(f"prepared {len(assets)} local intro/title assets")


if __name__ == "__main__":
    main()
