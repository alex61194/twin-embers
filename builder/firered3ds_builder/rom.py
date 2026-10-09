"""Strict local-only identification; no padding, patching or network access."""
from dataclasses import dataclass
import hashlib
from pathlib import Path

SUPPORTED_SHA1 = '41cb23d8dccc8ebd7c649cd8fbb58eeace6e2fdc'
ROM_SIZE = 16 * 1024 * 1024


@dataclass(frozen=True)
class Rom:
    data: bytes
    sha1: str
    source: Path


def load_rom(path: Path) -> Rom:
    path = Path(path)
    if path.stat().st_size != ROM_SIZE:
        raise ValueError('Expected an exact 16 MiB FireRed ROM dump; trimmed files are unsupported')
    with path.open('rb') as file:
        data = file.read(ROM_SIZE + 1)
    digest = hashlib.sha1(data).hexdigest()
    if len(data) != ROM_SIZE or digest != SUPPORTED_SHA1:
        raise ValueError(f'Unsupported ROM. Expected SHA-1 {SUPPORTED_SHA1}; Rev 1 and other languages are unsupported')
    return Rom(data, digest, path)
