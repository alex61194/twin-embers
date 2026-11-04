"""Strict local-only identification; no padding, patching or network access."""
from dataclasses import dataclass
import hashlib
from pathlib import Path

SUPPORTED_SHA1 = '41cb23d8dccc8ebd7c649cd8fbb58eeace6e2fdc'
SPANISH_SHA1 = 'ab8f6bfe0ccdaf41188cd015c8c74c314d02296a'
GERMAN_SHA1 = '18a3758ceeef2c77b315144be2c3910d6f1f69fe'
# Every ROM a recipe exists for: SHA-1 -> the language of the .3dsx its pack is for.
SUPPORTED_ROMS = {
    SUPPORTED_SHA1: 'en',   # FireRed (USA, Europe) 1.0
    SPANISH_SHA1: 'es',     # Edicion Rojo Fuego (Spain) 1.0
    GERMAN_SHA1: 'de',      # Feuerrote Edition (Germany) 1.0
}
ROM_SIZE = 16 * 1024 * 1024


@dataclass(frozen=True)
class Rom:
    data: bytes
    sha1: str
    source: Path

    @property
    def language(self) -> str:
        return SUPPORTED_ROMS[self.sha1]


def load_rom(path: Path) -> Rom:
    path = Path(path)
    if path.stat().st_size != ROM_SIZE:
        raise ValueError('Expected an exact 16 MiB FireRed ROM dump; trimmed files are unsupported')
    with path.open('rb') as file:
        data = file.read(ROM_SIZE + 1)
    digest = hashlib.sha1(data).hexdigest()
    if len(data) != ROM_SIZE or digest not in SUPPORTED_ROMS:
        raise ValueError(f'Unsupported ROM. Expected FireRed USA 1.0 (SHA-1 {SUPPORTED_SHA1}), Rojo Fuego Spain 1.0 '
                         f'(SHA-1 {SPANISH_SHA1}), or Feuerrot Germany 1.0 (SHA-1 {GERMAN_SHA1}); '
                         'Rev 1 and other languages are unsupported')
    return Rom(data, digest, path)
