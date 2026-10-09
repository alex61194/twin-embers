#!/usr/bin/env python3
"""Reject GBA sound/IWRAM literals in the ARM11 m4a text section."""

import argparse
import subprocess
import tempfile
from pathlib import Path


BAD = {
    0x03007FF0: "SOUND_INFO_PTR",
    0x04000006: "REG_VCOUNT",
    0x04000060: "REG_SOUND1CNT_L",
    0x040000BC: "REG_DMA1",
}


def text_bytes(objcopy: str, path: Path) -> bytes:
    with tempfile.TemporaryDirectory() as directory:
        target = Path(directory) / "text.bin"
        subprocess.run(
            [objcopy, "-O", "binary", "--only-section=.text", str(path), str(target)],
            check=True,
        )
        return target.read_bytes()


def check_literals(label: str, data: bytes) -> None:
    for value, name in BAD.items():
        literal = value.to_bytes(4, "little")
        for offset in range(0, len(data) - 3, 4):
            if data[offset:offset + 4] == literal:
                raise SystemExit(f"{label}: physical GBA {name} at .text+0x{offset:x}")


def symbol_value(readelf: str, elf: Path, name: str) -> int:
    symbols = subprocess.check_output([readelf, "-s", "-W", str(elf)], text=True)
    matches = [line.split() for line in symbols.splitlines()
               if line.split() and line.split()[-1] == name]
    if len(matches) != 1:
        raise SystemExit(f"{elf}: expected one {name} symbol, got {len(matches)}")
    return int(matches[0][1], 16)


def text_address(readelf: str, elf: Path) -> int:
    sections = subprocess.check_output([readelf, "-S", "-W", str(elf)], text=True)
    for line in sections.splitlines():
        fields = line.split()
        if ".text" in fields:
            index = fields.index(".text")
            if fields[index + 1] == "PROGBITS" and "X" in fields[index + 6]:
                return int(fields[index + 2], 16)
    raise SystemExit(f"{elf}: executable .text section missing")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--object", type=Path, required=True)
    parser.add_argument("--elf", type=Path)
    parser.add_argument("--objcopy", required=True)
    parser.add_argument("--readelf", required=True)
    args = parser.parse_args()

    object_text = text_bytes(args.objcopy, args.object)
    check_literals(str(args.object), object_text)
    guard = symbol_value(args.readelf, args.object, "chk_adr_r2") & ~1
    if object_text[guard:guard + 2] != b"\x70\x47":
        raise SystemExit(f"{args.object}: chk_adr_r2 must be a Thumb bx lr passthrough")
    relocations = subprocess.check_output(
        [args.readelf, "-r", "-W", str(args.object)], text=True
    )
    for symbol, minimum in (("gGbaSoundInfoPtr", 4), ("gGbaShadow", 4)):
        count = sum(symbol in line and "R_ARM_ABS32" in line
                    for line in relocations.splitlines())
        if count < minimum:
            raise SystemExit(f"{args.object}: expected {minimum} {symbol} relocations, got {count}")
    if not any("R_ARM_ABS32" in line and line.split()[-1] == "SoundMainRAM"
               for line in relocations.splitlines()):
        raise SystemExit(f"{args.object}: SoundMain mixer literal must relocate to SoundMainRAM")
    if any("SoundMainRAM_Buffer" in line for line in relocations.splitlines()):
        raise SystemExit(f"{args.object}: ARM11 mixer still references non-executable buffer")
    if args.elf:
        elf_text = text_bytes(args.objcopy, args.elf)
        check_literals(str(args.elf), elf_text)
        base = text_address(args.readelf, args.elf)
        literal = symbol_value(args.readelf, args.elf, "lt_SoundMainRAM_Buffer")
        mixer = symbol_value(args.readelf, args.elf, "SoundMainRAM")
        offset = literal - base
        if not 0 <= offset <= len(elf_text) - 4:
            raise SystemExit(f"{args.elf}: mixer literal outside .text")
        target = int.from_bytes(elf_text[offset:offset + 4], "little")
        if target != mixer or target & 1 == 0 or not base <= target & ~1 < base + len(elf_text):
            raise SystemExit(f"{args.elf}: mixer branches to 0x{target:08x}, expected Thumb .text 0x{mixer:08x}")
    print("PASS ARM11 m4a: address guard passthrough; mixer in .text; no physical GBA sound literals")


if __name__ == "__main__":
    main()
