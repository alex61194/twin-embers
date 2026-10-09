#!/usr/bin/env python3
"""Reproduce Azahar's Load3DSXFile() parser to find the first rejection.

Reference: azahar-emu/Azahar src/core/loader/3dsx.cpp, Load3DSXFile().
This tool reads a .3dsx exactly the way Azahar does (same order, same
sizes, same relocation walk with 512-entry chunks, same skip/patch loop
with the same pos<end_pos quirk) and reports the FIRST condition that
makes Azahar return ERROR_READ:

  1. truncated header / relocation-header / segment / relocation-stream read;
  2. aligned-size integer overflow;
  3. sub_type (= stored word >> 28) != 0 in an absolute (table 0) relocation;
  4. sub_type not in {0, 1} in a relative (table 1) relocation.

Exit 0: the whole file simulates cleanly (Azahar would proceed to process
creation). Exit 1: prints the first failing segment/table/entry/word
address/original value/sub_type/reason.

Usage: validate_3dsx.py [--dump] file.3dsx [file.3dsx ...]
  --dump prints the header/segment/relocation inventory without simulating.
"""

from __future__ import annotations

import struct
import sys

RELOCBUFSIZE = 512
NUM_SEGMENTS = 3
BASE_ADDR = 0x00100000  # Memory::PROCESS_IMAGE_VADDR, as in Azahar

HEADER_FMT = struct.Struct("<IHHIIIIIIIII")
HEADER_SIZE = HEADER_FMT.size  # 44
RELOC_FMT = struct.Struct("<HH")


class Truncated(Exception):
    pass


class Reader:
    def __init__(self, data: bytes):
        self.data = data
        self.size = len(data)

    def read(self, offset: int, count: int, what: str) -> bytes:
        if offset < 0 or count < 0 or offset + count > self.size:
            raise Truncated(
                f"truncated {what}: need [{offset:#x}..{offset + count:#x}), "
                f"EOF at {self.size:#x}")
        return self.data[offset:offset + count]


def align4k(n: int) -> int:
    return (n + 0xFFF) & ~0xFFF


def main(argv: list[str]) -> int:
    dump_only = "--dump" in argv
    paths = [a for a in argv if not a.startswith("--")]
    if not paths:
        print("usage: validate_3dsx.py [--dump] file.3dsx [...]", file=sys.stderr)
        return 2
    rc = 0
    for path in paths:
        if not run_one(path, dump_only):
            rc = 1
    return rc


def run_one(path: str, dump_only: bool) -> bool:
    with open(path, "rb") as f:
        data = f.read()
    rd = Reader(data)
    print(f"== {path} ({len(data)} bytes) ==")

    try:
        hdr_raw = rd.read(0, HEADER_SIZE, "file header")
    except Truncated as e:
        print(f"  REJECT: {e}")
        return False
    (magic, header_size, reloc_hdr_size, format_ver, flags, code_size,
     rodata_size, data_size, bss_size, smdh_offset, smdh_size,
     fs_offset) = HEADER_FMT.unpack(hdr_raw)
    print(f"  magic=0x{magic:08X} ({'3DSX ok' if magic == 0x58534433 else 'BAD MAGIC'}) "
          f"header_size={header_size} reloc_hdr_size={reloc_hdr_size} "
          f"format_ver={format_ver} flags={flags}")
    print(f"  code={code_size:#x} rodata={rodata_size:#x} data={data_size:#x} "
          f"bss={bss_size:#x}")
    # Bytes past header_size are relocation headers, not smdh/fs fields:
    # only interpret them when an extended header is present.
    has_ext = header_size >= 44
    if has_ext:
        print(f"  smdh_offset={smdh_offset:#x} smdh_size={smdh_size:#x} "
              f"fs_offset={fs_offset:#x}")
    else:
        print(f"  no extended header (fields past {header_size:#x} "
              f"are relocation counts, not smdh/fs)")
        smdh_offset, smdh_size, fs_offset = 0, 0, 0
    if magic != 0x58534433:
        print("  REJECT: bad 3DSX magic")
        return False
    if bss_size > data_size:
        print(f"  REJECT: bss ({bss_size:#x}) larger than data ({data_size:#x}); "
              f"segment read underflows")
        return False

    seg_sizes = [align4k(code_size), align4k(rodata_size), align4k(data_size)]
    names = ["code", "rodata", "data"]
    for i, n in enumerate(names):
        raw = (code_size, rodata_size, data_size)[i]
        if seg_sizes[i] < raw:
            print(f"  REJECT: aligned-size overflow in {n} segment")
            return False
        print(f"  seg[{i}] {n}: file={raw:#x} aligned={seg_sizes[i]:#x}")
    offsets = [seg_sizes[0], seg_sizes[0] + seg_sizes[1]]
    total = sum(seg_sizes)
    print(f"  offsets=[{offsets[0]:#x},{offsets[1]:#x}] image_total={total:#x}")

    n_tables = reloc_hdr_size // 4
    print(f"  n_reloc_tables={n_tables}")
    if n_tables == 0:
        print("  REJECT: no relocation tables (reloc_hdr_size=0)")
        return False

    # File layout: header_size, then 3 x n_tables u32, then segments.
    cursor = header_size
    rel_counts: list[list[int]] = []
    try:
        for seg in range(NUM_SEGMENTS):
            counts = []
            for _ in range(n_tables):
                (count,) = struct.unpack("<I", rd.read(cursor, 4, f"reloc header seg{seg}"))
                cursor += 4
                counts.append(count)
            rel_counts.append(counts)
            print(f"  reloc_hdr[{names[seg]}]: "
                  + " ".join(f"t{t}={c}" for t, c in enumerate(counts)))
    except Truncated as e:
        print(f"  REJECT: {e}")
        return False

    seg_file = []
    for i, n in enumerate(names):
        raw = (code_size, rodata_size, data_size)[i]
        if i == 2:
            raw -= bss_size
        seg_file.append((cursor, raw))
        cursor += raw
    for i, n in enumerate(names):
        print(f"  file[{n}]: [{seg_file[i][0]:#x}..{seg_file[i][0] + seg_file[i][1]:#x})")
    print(f"  reloc_streams_start={cursor:#x} EOF={len(data):#x}")
    if smdh_size:
        print(f"  smdh range=[{smdh_offset:#x}..{smdh_offset + smdh_size:#x}) "
              f"{'OK' if smdh_offset + smdh_size <= len(data) else 'PAST EOF'}")
    if fs_offset:
        print(f"  romfs range=[{fs_offset:#x}..{len(data):#x}) size={(len(data) - fs_offset):#x}")

    if dump_only:
        # Stream sizes without walking entries.
        cursor2 = cursor
        for seg in range(NUM_SEGMENTS):
            for t in range(n_tables):
                stream = rel_counts[seg][t] * 4
                print(f"  stream[{names[seg]}][t{t}]: count={rel_counts[seg][t]} "
                      f"bytes={stream:#x} range=[{cursor2:#x}..{cursor2 + stream:#x}) "
                      f"{'OK' if cursor2 + stream <= len(data) else 'PAST EOF'}")
                cursor2 += stream
        return True

    # Simulate Azahar's relocation walk exactly.
    image = bytearray(total)
    seg_base = [0, seg_sizes[0], seg_sizes[0] + seg_sizes[1]]
    try:
        for i, n in enumerate(names):
            off, raw = seg_file[i]
            image[seg_base[i]:seg_base[i] + raw] = rd.read(off, raw, f"{n} segment")
        # BSS clear (loader zeroes data_size-bss .. data_size of segment 2).
        start = seg_base[2] + data_size - bss_size
        image[start:start + bss_size] = b"\x00" * bss_size
    except Truncated as e:
        print(f"  REJECT: {e}")
        return False

    def translate(addr: int) -> int:
        addr &= ~0xF0000000
        if addr < offsets[0]:
            return BASE_ADDR + addr
        if addr < offsets[1]:
            return BASE_ADDR + seg_sizes[0] + addr - offsets[0]
        return BASE_ADDR + seg_sizes[0] + seg_sizes[1] + addr - offsets[1]

    try:
        for seg in range(NUM_SEGMENTS):
            for table in range(n_tables):
                n_relocs = rel_counts[seg][table]
                if table >= 2:
                    # Unknown table: Azahar skips it (Seek past it).
                    skip_bytes = n_relocs * 4
                    rd.read(cursor, skip_bytes, f"unknown table {table} seg{seg}")
                    cursor += skip_bytes
                    continue
                pos_word = 0
                end_words = seg_sizes[seg] // 4
                stream_start = cursor
                while n_relocs:
                    remaining = min(RELOCBUFSIZE, n_relocs)
                    n_relocs -= remaining
                    chunk = rd.read(cursor, remaining * 4,
                                    f"reloc stream seg{seg} t{table}")
                    cursor += remaining * 4
                    for k in range(remaining):
                        if not pos_word < end_words:
                            break  # Azahar quirk: rest of chunk ignored
                        skip, patch = RELOC_FMT.unpack_from(chunk, k * 4)
                        pos_word += skip
                        num_patches = patch
                        while 0 < num_patches and pos_word < end_words:
                            at = seg_base[seg] + pos_word * 4
                            in_addr = BASE_ADDR + at
                            orig = struct.unpack_from("<I", image, at)[0]
                            sub = orig >> 28
                            addr = translate(orig)
                            if table == 0:
                                if sub != 0:
                                    print(f"  REJECT seg={names[seg]} table=absolute "
                                          f"stream_off={stream_start:#x} entry={k} "
                                          f"word_addr={BASE_ADDR + at:#x} "
                                          f"orig=0x{orig:08X} sub_type={sub} "
                                          f"reason=sub_type!=0 in absolute relocation")
                                    return False
                            else:
                                if sub not in (0, 1):
                                    print(f"  REJECT seg={names[seg]} table=relative "
                                          f"stream_off={stream_start:#x} entry={k} "
                                          f"word_addr={BASE_ADDR + at:#x} "
                                          f"orig=0x{orig:08X} sub_type={sub} "
                                          f"reason=sub_type not in {{0,1}} in relative relocation")
                                    return False
                            pos_word += 1
                            num_patches -= 1
                    stream_start += remaining * 4
    except Truncated as e:
        print(f"  REJECT: {e}")
        return False

    print(f"  OK: all relocation streams simulate cleanly "
          f"(reloc_streams_end={cursor:#x} EOF={len(data):#x})")
    return True


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
