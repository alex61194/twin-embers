#!/usr/bin/env python3
"""Generate pinned FireRed headers and local C INCBIN inputs for ARM11.

Only the ignored upstream build tree is modified. Graphics are generated from
pret's source files; nothing generated here is committed or distributed.
"""

from __future__ import annotations

import re
import os
import subprocess
from pathlib import Path

import es_assets

ROOT = Path(__file__).resolve().parents[1]
ASSET = re.compile(
    r'"((?:graphics|data)/[^"\n]+\.(?:1bpp|4bpp|8bpp|gbapal|lz|fwlatfont|hwlatfont|fwjpnfont|bin))"'
)


def run(*args: str) -> None:
    subprocess.run(["make", *args], cwd=ROOT, check=True)


def main() -> None:
    sources = sorted((ROOT / "src").rglob("*.c")) + sorted((ROOT / "src").rglob("*.h"))
    assets = sorted({name for source in sources
                     for name in ASSET.findall(source.read_text(encoding="utf-8"))})
    if not assets or any(
        not name.startswith(("graphics/", "data/"))
        or ".." in Path(name).parts or " " in name
        for name in assets
    ):
        raise ValueError("unexpected FireRed INCBIN path")
    map_headers = [str(path.parent.relative_to(ROOT) / name).replace("\\", "/")
                   for path in sorted((ROOT / "data" / "maps").glob("*/map.json"))
                   for name in ("header.inc", "events.inc", "connections.inc")]
    if not map_headers:
        raise ValueError("map sources are missing")

    for tool in ("mapjson", "jsonproc", "gbagfx"):
        run("-C", f"tools/{tool}")
    jobs = str(max(1, int(os.environ.get("FIRERED_BUILD_JOBS", "4"))))
    run("-j" + jobs, "generated")

    # A generated prerequisite file avoids the Windows command-line length
    # limit while preserving upstream's individual graphics conversion rules.
    rule = ROOT / "build" / "3ds_port" / "all_assets.mk"
    rule.parent.mkdir(parents=True, exist_ok=True)
    rule.write_text("all-3ds-assets: \\\n" + " \\\n".join(assets + map_headers) + "\n", encoding="utf-8")
    run("-j" + jobs, "SETUP_PREREQS=0", "NODEP=1", "-f", "Makefile", "-f",
        str(rule.relative_to(ROOT)), "all-3ds-assets")

    missing = [name for name in assets + map_headers if not (ROOT / name).is_file()]
    if missing:
        raise FileNotFoundError(f"{len(missing)} generated assets missing; first: {missing[:5]}")
    if es_assets.spanish():
        # The Spanish game's pictures have their own sizes; the compiler needs the Spanish one (the bytes come from the pack).
        spanish = es_assets.files()
        resized = 0
        for name in assets:
            size = (spanish[name][0] + 3) // 4 * 4 if name in spanish else 0   # INCBIN_U16/U32 want whole words
            if name in spanish and spanish[name][0] != (ROOT / name).stat().st_size:
                (ROOT / name).write_bytes(bytes(size))
                resized += 1
        print(f"sized {resized} assets like the Spanish game's")
    print(f"prepared {len(assets)} FireRed C assets and {len(map_headers)} map includes")


if __name__ == "__main__":
    main()
