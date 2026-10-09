#!/usr/bin/env python3
"""Generate FireRed sound binaries and song assembly from pret sources locally."""

from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
import os
import shlex
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SOUND = ROOT / "sound"
TOOLS = ROOT / "tools"


def build_tool(name: str) -> Path:
    make = shutil.which("make")
    if make is None:
        raise RuntimeError("make is required to build the sound converters")
    subprocess.run([make, "-C", str(TOOLS / name)], check=True,
                   stdout=subprocess.DEVNULL)
    executable = TOOLS / name / (name + (".exe" if os.name == "nt" else ""))
    if not executable.is_file():
        raise RuntimeError(f"sound converter missing: {executable}")
    return executable


def convert_wav(tool: Path, source: Path) -> bool:
    target = source.with_suffix(".bin")
    if target.exists() and target.stat().st_mtime_ns >= source.stat().st_mtime_ns:
        return False
    command = [str(tool), "-b"]
    if "cries" in source.parts:
        command.extend(["-c", "-l", "1", "--no-pad"])
    command.extend([str(source), str(target)])
    subprocess.run(command, check=True, stdout=subprocess.DEVNULL)
    return True


def convert_midi(tool: Path, source: Path, options: list[str]) -> bool:
    target = source.with_suffix(".s")
    if target.exists() and target.stat().st_mtime_ns >= source.stat().st_mtime_ns:
        return False
    subprocess.run([str(tool), str(source), str(target), *options], check=True,
                   stdout=subprocess.DEVNULL)
    return True


def main() -> None:
    wav_tool = build_tool("wav2agb")
    midi_tool = build_tool("mid2agb")
    jobs = []
    absent_midi = 0
    with ThreadPoolExecutor(max_workers=6) as executor:
        for source in sorted((SOUND / "direct_sound_samples").rglob("*.wav")):
            jobs.append(executor.submit(convert_wav, wav_tool, source))
        config = SOUND / "songs" / "midi" / "midi.cfg"
        for line in config.read_text(encoding="utf-8").splitlines():
            words = shlex.split(line, comments=True)
            if not words:
                continue
            source = config.parent / words[0].removesuffix(":")
            if not source.is_file():
                absent_midi += 1
                continue
            jobs.append(executor.submit(convert_midi, midi_tool, source, words[1:]))
        changed = 0
        for job in as_completed(jobs):
            changed += bool(job.result())
    print(f"sound assets: {len(jobs)} checked, {changed} generated locally; "
          f"{absent_midi} optional MIDI entries absent upstream")


if __name__ == "__main__":
    main()
