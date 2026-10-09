#!/usr/bin/env python3
"""Drop vestigial R_ARM_THM_CALL relocations so 3dsxtool can scan the ELF.

Background: 3dsxtool aborts on ANY relocation in an SHF_ALLOC section whose
target address is not 4-byte aligned, before looking at the relocation type
(3dstools/src/3dsxtool.cpp, ScanRelocSection). FireRed's hand-written Thumb
sound engine (src/m4a_1.s) leaves R_ARM_THM_CALL branch relocations at
2-byte offsets. In the linked ET_EXEC these branches are already statically
resolved (PC-relative, no load-time patch needed) and 3dsxtool's type switch
ignores CALL relocations entirely -- but its blanket alignment check fires
first, so the 3dsx can never be produced.

This tool copies input.elf to output.elf, removing ONLY the unaligned
.rel.text entries that provably satisfy ALL of these conditions (otherwise
it aborts without writing anything):

  1. The entry is R_ARM_THM_CALL in the .rel.text section (site in .text)
     at an address 3dsxtool rejects (r_offset & 3 != 0).
  2. The target symbol is defined in the final ELF (not SHN_UNDEF).
  3. The target address lies inside the PT_LOAD executable image.
  4. The target symbol is STT_FUNC in the .text section.
  5. The Thumb branch at the site already encodes a displacement that
     lands exactly on the target symbol: old-form pre-Thumb-2 BL
     (11110:imm11 / 11111:imm11) for Thumb targets, old-form BLX
     (11110:H:imm10 / 11101:imm11, emitted by ld --use-blx from
     3dsx.specs for ARM targets) for ARM targets.
  6. R_ARM_THM_CALL is PC-relative by ABI definition: it needs no
     modification when the whole image changes base address.
  7. The unaligned R_ARM_THM_CALL set is exactly EXPECTED_UNALIGNED
     (default 14) and the total R_ARM_THM_CALL count in .rel.text is
     exactly EXPECTED_TOTAL (default 26, the rest 4-byte aligned and
     harmless to 3dsxtool). Any deviation aborts the build instead of
     silently hiding relocations.

Everything else (.text bytes, .rodata, .data, symbols, addresses, every
other relocation) is left byte-identical. The .rel.text section is compacted
in place and its sh_size updated; the vacated tail bytes are zeroed so the
output stays deterministic. No relocation types are faked and no offsets
are altered to fool 3dsxtool: the output ELF is semantically honest.

Second verified step: dead interworking-thunk labels. 3dsxtool marks word
sym+8 for every `__*_from_arm` symbol in the symtab (3dstools
ScanRelocations tail). ld appends these 8-byte `ldr pc,[pc,#-4]` + literal
stubs for ARM<->Thumb calls at the end of .text; when two land back to
back, thunk[i]+8 is thunk[i+1]'s INSTRUCTION word (0xE51FF004), which
Azahar rejects (sub_type 0xE) and which must never be slid. Each neutered
label provably satisfies: `__*_from_arm` name, STT_FUNC in .text, 8 bytes
holding exactly that veneer shape, and zero references from any SHT_REL
section in the file. Only the debug labels go (st_name/st_info cleared in
place); code bytes are untouched. Expected count defaults to 2; any
deviation aborts without writing output.
"""

from __future__ import annotations

import struct
import sys

R_ARM_THM_CALL = 10
EXPECTED_UNALIGNED = 14
EXPECTED_TOTAL = 26
EXPECTED_THUNKS = 2
THUNK_INSN = 0xE51FF004  # ldr pc, [pc, #-4]: ld ARM<->Thumb veneer head

SHT_REL = 9
SHT_SYMTAB = 2
STT_FUNC = 2
SHN_UNDEF = 0
PT_LOAD = 1


def u16(b: bytes, o: int) -> int:
    return struct.unpack_from("<H", b, o)[0]


def u32(b: bytes, o: int) -> int:
    return struct.unpack_from("<I", b, o)[0]


def fail(msg: str) -> int:
    print(f"filter_3dsx_relocs: ABORT: {msg}", file=sys.stderr)
    return 1


def main(argv: list[str]) -> int:
    expected_unaligned = EXPECTED_UNALIGNED
    expected_total = EXPECTED_TOTAL
    expected_thunks = EXPECTED_THUNKS
    args = list(argv)
    while len(args) >= 3 and args[0].startswith("--expect-"):
        opt, args = args[0], args[1:]
        if opt == "--expect-unaligned":
            expected_unaligned = int(args[0])
            args = args[1:]
        elif opt == "--expect-total":
            expected_total = int(args[0])
            args = args[1:]
        elif opt == "--expect-thunks":
            expected_thunks = int(args[0])
            args = args[1:]
        else:
            print(f"unknown option {opt}", file=sys.stderr)
            return 2
    if len(args) != 2:
        print("usage: filter_3dsx_relocs.py [--expect-unaligned N] "
              "[--expect-total N] [--expect-thunks N] input.elf output.elf",
              file=sys.stderr)
        return 2
    src_path, dst_path = args

    data = bytearray(open(src_path, "rb").read())

    if data[0:4] != b"\x7fELF" or data[4] != 1 or data[5] != 1:
        return fail("not a 32-bit little-endian ELF")
    e_phoff = u32(data, 0x1C)
    e_shoff = u32(data, 0x20)
    e_phentsize = u16(data, 0x2A)
    e_phnum = u16(data, 0x2C)
    e_shentsize = u16(data, 0x2E)
    e_shnum = u16(data, 0x30)
    e_shstrndx = u16(data, 0x32)
    if e_shentsize != 40:
        return fail(f"unexpected section header size {e_shentsize}")

    def sh(i: int) -> tuple[int, ...]:
        o = e_shoff + i * 40
        return struct.unpack_from("<IIIIIIIIII", data, o)

    shstr = sh(e_shstrndx)
    strtab = data[shstr[4]:shstr[4] + shstr[5]]

    def name(off: int) -> str:
        end = strtab.index(b"\x00", off)
        return strtab[off:end].decode("ascii")

    sections = {}
    for i in range(e_shnum):
        s = sh(i)
        sections[name(s[0])] = (i, s)

    for need in (".text", ".rel.text", ".symtab"):
        if need not in sections:
            return fail(f"missing section {need}")
    text_idx, text = sections[".text"]
    _, rel = sections[".rel.text"]
    _, symtab = sections[".symtab"]
    if rel[1] != SHT_REL:
        return fail(".rel.text is not SHT_REL")
    if rel[7] != text_idx:  # sh_info: target section index
        return fail(".rel.text does not target .text")
    if symtab[1] != SHT_SYMTAB:
        return fail(".symtab is not SHT_SYMTAB")
    str_sec = sh(symtab[6])  # sh_link: associated string table
    sym_names = data[str_sec[4]:str_sec[4] + str_sec[5]]

    def sym_name(off: int) -> str:
        end = sym_names.index(b"\x00", off)
        return sym_names[off:end].decode("ascii", errors="replace")

    # Executable image range from PT_LOAD segments.
    lo, hi = None, None
    for i in range(e_phnum):
        o = e_phoff + i * e_phentsize
        p_type, p_vaddr, p_memsz = u32(data, o), u32(data, o + 8), u32(data, o + 20)
        if p_type == PT_LOAD and p_memsz:
            lo = p_vaddr if lo is None else min(lo, p_vaddr)
            hi = p_vaddr + p_memsz if hi is None else max(hi, p_vaddr + p_memsz)
    if lo is None:
        return fail("no PT_LOAD segments")

    text_addr, text_off, text_size = text[3], text[4], text[5]

    def sym(i: int) -> tuple[int, int, int, int, int]:
        o = symtab[4] + i * 16
        st_name, st_value, st_size, st_info, st_other, st_shndx = \
            struct.unpack_from("<IIIBBH", data, o)
        return st_name, st_value, st_size, st_info, st_shndx

    rel_off, rel_size = rel[4], rel[5]
    if rel_size % 8:
        return fail(f".rel.text size {rel_size} is not a multiple of 8")
    n_rel = rel_size // 8
    entries = []
    for i in range(n_rel):
        o = rel_off + i * 8
        r_offset, r_info = u32(data, o), u32(data, o + 4)
        entries.append((r_offset, r_info >> 8, r_info & 0xFF))

    thm = [(off, sym) for off, sym, typ in entries if typ == R_ARM_THM_CALL]
    blocked = [(off, sym) for off, sym in thm if off & 3]
    if len(thm) != expected_total:
        return fail(f"found {len(thm)} total R_ARM_THM_CALL entries, "
                    f"expected exactly {expected_total}; refusing to filter")
    if len(blocked) != expected_unaligned:
        return fail(f"found {len(blocked)} unaligned R_ARM_THM_CALL entries, "
                    f"expected exactly {expected_unaligned}; "
                    f"refusing to filter")

    removed = []
    for r_offset, sym_idx in blocked:
        # (1) site inside .text
        if not (text_addr <= r_offset < text_addr + text_size):
            return fail(f"reloc at 0x{r_offset:08x} is outside .text")
        st_name, st_value, _st_size, st_info, st_shndx = sym(sym_idx)
        sname = sym_name(st_name)
        # (2) defined symbol
        if st_shndx == SHN_UNDEF:
            return fail(f"reloc at 0x{r_offset:08x} targets "
                        f"undefined symbol '{sname}'")
        # (3) target inside the executable image
        if not (lo <= (st_value & ~1) < hi):
            return fail(f"target '{sname}' at 0x{st_value:08x} is "
                        f"outside the PT_LOAD image")
        # (4) function symbol in .text
        if (st_info & 0xF) != STT_FUNC or st_shndx != text_idx:
            return fail(f"target '{sname}' is not STT_FUNC in .text "
                        f"(type={st_info & 0xF}, sect={st_shndx})")
        # (5) Thumb branch at the site encodes the resolved displacement.
        # Old-form (pre-Thumb-2, correct for -mcpu=mpcore) BL:
        # hw1=11110:imm11, hw2=11111:imm11. Old-form BLX (what ld
        # --use-blx from 3dsx.specs emits for Thumb->ARM calls):
        # hw1=11110:H:imm10, hw2=11101:imm11, target=Align(PC,4)+disp.
        fo = text_off + (r_offset - text_addr)
        hw1, hw2 = u16(data, fo), u16(data, fo + 2)
        expect = st_value & ~1
        if (hw1 >> 11) == 0b11110 and (hw2 >> 11) == 0b11111:
            disp = ((hw1 & 0x7FF) << 12) | ((hw2 & 0x7FF) << 1)
            if disp & 0x400000:  # sign-extend 23 bits
                disp -= 0x800000
            got = (r_offset + 4 + disp) & 0xFFFFFFFF
            if (st_value & 1) == 0:
                return fail(f"site 0x{r_offset:08x} is BL but target "
                            f"'{sname}' is ARM (expected BLX)")
        elif (hw1 >> 11) == 0b11110 and (hw2 >> 11) == 0b11101:
            h = (hw1 & 0x800) >> 11  # BLX H bit selects target bit 1
            disp = (h << 23) | ((hw1 & 0x7FF) << 12) | ((hw2 & 0x7FF) << 1)
            if disp & 0x800000:  # sign-extend 24 bits
                disp -= 0x1000000
            got = (((r_offset + 4) & ~3) + disp) & 0xFFFFFFFF
            if st_value & 1:
                return fail(f"site 0x{r_offset:08x} is BLX but target "
                            f"'{sname}' is Thumb (expected BL)")
        else:
            return fail(f"site 0x{r_offset:08x} is not an old-form Thumb "
                        f"BL/BLX (halfwords 0x{hw1:04x} 0x{hw2:04x})")
        if got != expect:
            return fail(f"site 0x{r_offset:08x} branch lands at 0x{got:08x}, "
                        f"expected target '{sname}' at 0x{expect:08x}")
        # (6) R_ARM_THM_CALL is PC-relative by ABI definition: nothing to
        # patch at load; 3dsxtool's own switch ignores CALL relocations.
        removed.append((r_offset, sname, r_offset & 3))

    blocked_set = set(off for off, _ in blocked)
    kept = [(off, (s << 8) | t) for off, s, t in entries
            if not (t == R_ARM_THM_CALL and off in blocked_set)]
    out = bytearray(data)
    for i, (off, info) in enumerate(kept):
        struct.pack_into("<II", out, rel_off + i * 8, off, info)
    tail_start = rel_off + len(kept) * 8
    tail_end = rel_off + rel_size
    for i in range(tail_start, tail_end):
        out[i] = 0
    struct.pack_into("<I", out, e_shoff + sections[".rel.text"][0] * 40 + 20,
                     len(kept) * 8)

    print(f"filter_3dsx_relocs: removed {len(removed)} verified unaligned "
          f"R_ARM_THM_CALL relocs, kept {len(kept)} entries in .rel.text "
          f"({len(thm) - len(blocked)} aligned THM_CALL left untouched)")
    for off, sname, mod in removed:
        print(f"  - 0x{off:08x} (mod4={mod}) -> {sname}")

    # Second step: neuter dead interworking-thunk labels (see docstring).
    symtab_off, symtab_size = symtab[4], symtab[5]
    n_syms = symtab_size // 16
    thunk_idx = []
    for i in range(n_syms):
        o = symtab_off + i * 16
        st_name = u32(out, o)
        if st_name >= len(sym_names):
            continue
        if sym_names[st_name:st_name + 1] == b"\x00":
            continue
        try:
            end = sym_names.index(b"\x00", st_name)
        except ValueError:
            continue
        sname = sym_names[st_name:end].decode("ascii", errors="replace")
        if len(sname) >= 9 and sname[0] == "_" and sname[1] == "_" \
                and sname.endswith("_from_arm"):
            thunk_idx.append((i, sname))
    if len(thunk_idx) != expected_thunks:
        return fail(f"found {len(thunk_idx)} *_from_arm labels, expected "
                    f"exactly {expected_thunks}; refusing to filter")
    referenced: set[int] = set()
    for i in range(e_shnum):
        s = sh(i)
        if s[1] != SHT_REL:
            continue
        for j in range(s[5] // 8):
            referenced.add(u32(out, s[4] + j * 8 + 4) >> 8)
    neutered = []
    for i, sname in thunk_idx:
        o = symtab_off + i * 16
        st_value, st_size = u32(out, o + 4), u32(out, o + 8)
        st_info = out[o + 12]
        st_shndx = u16(out, o + 14)
        if st_shndx != text_idx:
            return fail(f"thunk '{sname}' is not in .text (sect {st_shndx})")
        if (st_info & 0xF) != STT_FUNC:
            return fail(f"thunk '{sname}' is not STT_FUNC")
        if st_size != 8:
            return fail(f"thunk '{sname}' size is {st_size}, expected 8")
        fo = text_off + (st_value - text_addr)
        if not (text_off <= fo and fo + 8 <= text_off + text_size):
            return fail(f"thunk '{sname}' at 0x{st_value:08x} is outside .text")
        w0, w1 = u32(out, fo), u32(out, fo + 4)
        if w0 != THUNK_INSN:
            return fail(f"thunk '{sname}' head is 0x{w0:08X}, "
                        f"expected veneer 0x{THUNK_INSN:08X}")
        if i in referenced:
            return fail(f"thunk '{sname}' is referenced by a relocation; "
                        f"it is not dead")
        out[o:o + 4] = b"\x00\x00\x00\x00"  # st_name = ""
        out[o + 12] = 0  # NOTYPE: no longer a function label
        neutered.append((sname, st_value, w1))
    print(f"filter_3dsx_relocs: neutered {len(neutered)} dead thunk labels")
    for sname, addr, lit in neutered:
        print(f"  - {sname} at 0x{addr:08x} (literal 0x{lit:08x})")

    with open(dst_path, "wb") as f:
        f.write(out)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
