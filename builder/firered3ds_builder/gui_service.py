"""Local-only GUI workflow: verify, build, verify, and safely publish a data pack.

The ROM is read locally by the existing builder. This module never talks to a
network, never changes game data and never writes to the ROM. It deliberately
contains no Tk dependency so the security-critical workflow is host-testable.
"""
from __future__ import annotations

import errno
import hashlib
import os
from pathlib import Path
import shutil
import tempfile
from typing import Callable

from .build import build
from .pak import PakReader
from .rom import load_rom, SUPPORTED_SHA1

Stage = Callable[[str, str], None]


class BuildCancelled(Exception):
    """The user cancelled before a pack was published."""


def _stage(callback: Stage | None, step: str, message: str) -> None:
    if callback is not None:
        callback(step, message)


def _cancel(event: object | None) -> None:
    if event is not None and event.is_set():
        raise BuildCancelled("Cancelled. No new pack was installed.")


def describe_error(exc: BaseException) -> str:
    """Plain-language text for common file errors; other messages are kept as they are."""
    if isinstance(exc, OSError) and exc.errno == errno.ENOSPC:
        return ("The destination drive is full. Free at least 32 MiB on the SD card "
                "and try again. Existing files were not changed.")
    if isinstance(exc, PermissionError) or (isinstance(exc, OSError) and exc.errno == errno.EROFS):
        return ("Windows could not write to that folder. If it is an SD card, check that "
                "its lock switch is not set to LOCK, then try again. Existing files were "
                "not changed.")
    if isinstance(exc, FileNotFoundError) and exc.filename:
        return f"File or folder not found: {exc.filename}"
    return str(exc)


def verify_own_rom(path: str | Path) -> str:
    """Check exact size/hash without changing the original ROM."""
    return load_rom(Path(path)).sha1


def _check_target(rom: Path, target: Path, overwrite: bool) -> None:
    if target.name in ("", ".", "..") or target.suffix.lower() != ".pak":
        raise ValueError("Choose an output filename ending in .pak.")
    if target.is_symlink():
        raise ValueError("Refusing to write through a symbolic-link output.")
    if rom.resolve() == target.resolve():
        raise ValueError("The output must not be the source ROM.")
    if target.exists():
        if not target.is_file():
            raise ValueError("The output path is not a regular file.")
        if not overwrite:
            raise FileExistsError(f"Existing pack will not be overwritten: {target}")


def build_private_pack(
    rom_path: str | Path,
    output_path: str | Path,
    *,
    overwrite: bool = False,
    create_directories: bool = False,
    on_progress: Stage | None = None,
    cancel: object | None = None,
) -> dict:
    """Build with the original engine; publish only a fully verified pack.

    Overwrite requires explicit permission from the GUI's confirmation dialog.
    Non-overwrite publication uses exclusive creation, never a replace race.
    Temporary data stays in the destination's filesystem and is cleaned up.
    Cancel requests take effect at each stage boundary; an ongoing builder
    reconstruction cannot be interrupted in the middle of a ROM operation.
    """
    rom_path = Path(rom_path).expanduser()
    output_path = Path(output_path).expanduser()
    _cancel(cancel)
    _stage(on_progress, "rom", "Verifying your local ROM...")
    rom = load_rom(rom_path)
    if rom.sha1 != SUPPORTED_SHA1:
        raise ValueError("Unsupported ROM revision or language.")
    _check_target(rom_path, output_path, overwrite)
    _cancel(cancel)

    if create_directories:
        output_path.parent.mkdir(parents=True, exist_ok=True)
    if not output_path.parent.is_dir():
        raise ValueError("The destination folder does not exist.")
    if shutil.disk_usage(output_path.parent).free < 32 * 1024 * 1024:
        raise OSError("At least 32 MiB of free space is required on the destination.")

    with tempfile.TemporaryDirectory(prefix=".fire3ds-", dir=output_path.parent) as tmp:
        candidate = Path(tmp) / "twinembers.pak"
        _cancel(cancel)
        _stage(on_progress, "build", "Reconstructing the pack from your ROM...")
        result = build(rom_path, None, candidate)
        _cancel(cancel)
        _stage(on_progress, "verify", "Checking all pack records and checksums...")
        with PakReader(candidate, result["abi"], bytes.fromhex(rom.sha1)) as reader:
            count = reader.verify()
        if count != result["entries"]:
            raise ValueError("Pack verification count does not match the build.")
        _cancel(cancel)
        _check_target(rom_path, output_path, overwrite)
        digest = hashlib.sha256(candidate.read_bytes()).hexdigest()
        candidate_size = candidate.stat().st_size
        _stage(on_progress, "publish", "Saving the verified pack locally...")
        _cancel(cancel)
        # Keep a durable same-volume backup when replacing a user's pack.
        # Do NOT delete the backup if a later rollback itself fails.
        backup_path = None
        installed = False
        keep_backup = False
        try:
            if output_path.exists():
                # The GUI explicitly confirmed overwrite before starting.
                # Preserve the previous bytes until destination verification.
                with tempfile.NamedTemporaryFile(
                    mode="wb", prefix=".fire3ds-previous-", suffix=".pak",
                    dir=output_path.parent, delete=False
                ) as saved:
                    backup_path = Path(saved.name)
                    with output_path.open("rb") as previous:
                        shutil.copyfileobj(previous, saved, 1024 * 1024)
                    saved.flush()
                    os.fsync(saved.fileno())
                if (hashlib.sha256(backup_path.read_bytes()).digest()
                        != hashlib.sha256(output_path.read_bytes()).digest()):
                    raise OSError("The existing pack changed during backup; refusing replacement.")
                os.replace(candidate, output_path)
                installed = True
            else:
                # Exclusive create: never replace a file created by a second
                # process while reconstruction was running.
                created = False
                try:
                    with output_path.open("xb") as destination:
                        created = True
                        with candidate.open("rb") as source:
                            shutil.copyfileobj(source, destination, 1024 * 1024)
                        destination.flush()
                        os.fsync(destination.fileno())
                    installed = True
                except BaseException:
                    if created:
                        output_path.unlink(missing_ok=True)
                    raise

            # Verify the exact bytes written to removable/local destination.
            if (output_path.stat().st_size != candidate_size or
                    hashlib.sha256(output_path.read_bytes()).hexdigest() != digest):
                raise OSError("The destination copy did not match the verified pack.")
            _stage(on_progress, "done", "Pack built and verified successfully.")
        except BaseException:
            if installed and backup_path is not None:
                try:
                    os.replace(backup_path, output_path)
                except OSError as restore_error:
                    keep_backup = True
                    raise OSError(
                        f"Automatic rollback failed. Your previous pack is "
                        f"preserved at {backup_path}; restore it manually."
                    ) from restore_error
            elif installed and backup_path is None:
                output_path.unlink(missing_ok=True)
            raise
        finally:
            if backup_path is not None and not keep_backup:
                backup_path.unlink(missing_ok=True)
        return {
            "path": str(output_path),
            "sha256": digest,
            "abi": result["abi"],
            "entries": count,
            "bytes": output_path.stat().st_size,
            "rom_sha1": rom.sha1,
        }
