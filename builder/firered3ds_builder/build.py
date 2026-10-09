"""Verify own ROM, reconstruct in memory, validate then atomically write pack."""
import json
from pathlib import Path
from .rom import load_rom
from .recipe import load_recipe, reconstruct
from .pak import write_pak, PakReader

RECIPES = Path(__file__).resolve().parent / 'recipes'


def default_recipe(rom_sha1: str) -> Path:
    """The recipe shipped with the Builder for this exact ROM."""
    for path in sorted(RECIPES.glob('*.json')):
        with path.open('rb') as file:
            head = file.read(256)
        if f'"rom_sha1":"{rom_sha1}"' in head.decode('ascii', 'ignore').replace(' ', ''):
            return path
    raise ValueError(f'No recipe for ROM SHA-1 {rom_sha1}')


def build(rom_path: Path, recipe_path: Path | None, output: Path) -> dict:
    rom = load_rom(rom_path)
    recipe_path = Path(recipe_path) if recipe_path else default_recipe(rom.sha1)
    if output.resolve() in (rom_path.resolve(), recipe_path.resolve()):
        raise ValueError('Pack output must not overwrite ROM or recipe')
    recipe = load_recipe(recipe_path)
    files = reconstruct(recipe, rom.data)
    staging = output.with_name(output.name + '.building')
    if staging.exists() or staging.with_name(staging.name + '.tmp').exists():
        raise ValueError('Pack staging path already exists; preserve and inspect it')
    try:
        result = write_pak(staging, files, recipe['engine_abi'], bytes.fromhex(rom.sha1))
        with PakReader(staging, recipe['engine_abi'], bytes.fromhex(rom.sha1)) as reader:
            reader.verify()
        staging.replace(output)
        return result
    finally:
        staging.unlink(missing_ok=True)
        staging.with_name(staging.name + '.tmp').unlink(missing_ok=True)
