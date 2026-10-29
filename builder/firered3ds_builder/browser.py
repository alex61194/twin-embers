"""Entry points for the web Builder (web/), which runs this package in the player's browser with Pyodide.

The page does only what Python cannot do there: it reads the ROM the player picked and, when asked,
fetches the HeartGold sprite files into a folder. Everything else is the desktop Builder's own code,
so both write the same pack byte for byte. Nothing here touches the network or leaves the device.
"""
from __future__ import annotations

import hashlib
from pathlib import Path

from . import gen4_sources, gen4_sprites
from .build import build as build_pack
from .rom import load_rom


def gen4_downloads() -> list[list[str]]:
    """[URL, file name] of every listed HeartGold sprite file, for the page to fetch into one folder."""
    return [[gen4_sprites.file_url(row), gen4_sprites.file_name(row)] for row in gen4_sources.FILES]


def build(rom_path: str, output_path: str, gen4_folder: str | None = None) -> dict:
    """Verify the ROM and write the pack, as `firered3ds-builder build [--gen4-sprites]` does.

    `gen4_folder`: the folder the page fetched the sprite files into, or None to leave them out.
    Files there are used only if their size and SHA-256 match gen4_sources.py."""
    rom = load_rom(Path(rom_path))
    extra, sprites = [], None
    if gen4_folder is not None:
        extra, sprites = gen4_sprites.pack_files(
            gen4_sprites.Downloader(folder=Path(gen4_folder), get=None, threads=0))
    output = Path(output_path)
    result = build_pack(Path(rom_path), None, output, extra)
    return {
        "entries": result["entries"],
        "bytes": result["bytes"],
        "abi": result["abi"],
        "sha256": hashlib.sha256(output.read_bytes()).hexdigest(),
        "rom_sha1": rom.sha1,
        "gen4": sprites,
    }
