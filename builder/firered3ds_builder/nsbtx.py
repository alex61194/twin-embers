"""A Nintendo DS texture archive (NSBTX, "BTX0"), the part HeartGold's follower Pokémon need.

HeartGold keeps each follower Pokémon's pictures as a TEX0 block of 4-bit paletted textures (format 3: 16 colours, 32x32 here)
and a PLTT block with the palettes (the normal one first, the shiny one second). decode() returns each texture as RGBA
rows with the colour-0 flag honoured (colour 0 transparent when the texture says so), for any of the palettes."""

from __future__ import annotations

import struct


class NsbtxError(ValueError):
    pass


FORMAT_16_COLOURS = 3


def _u16(data: bytes, at: int) -> int:
    return struct.unpack_from("<H", data, at)[0]


def _u32(data: bytes, at: int) -> int:
    return struct.unpack_from("<I", data, at)[0]


def _dictionary(tex: bytes, at: int, entry_size: int) -> list[tuple[str, bytes]]:
    """A Nitro name dictionary: a header (u8 0, u8 count, u16 size), the search tree ((count + 1) 4-byte nodes), the
    entries' own header (u16, u16, u32) and their `entry_size` bytes, then each entry's 16-byte name."""
    count = tex[at + 1]
    first = at + 8 + (count + 1) * 4 + 4
    entries = [tex[first + n * entry_size:first + (n + 1) * entry_size] for n in range(count)]
    names_at = first + count * entry_size
    names = [tex[names_at + n * 16:names_at + (n + 1) * 16].split(b"\0")[0].decode("latin-1") for n in range(count)]
    return list(zip(names, entries))


class Archive:
    """The textures (name, width, height, format, colour 0 transparent, offset) and palettes of one NSBTX."""

    def __init__(self, data: bytes):
        if data[:4] != b"BTX0":
            raise NsbtxError("not a texture archive")
        tex_at = None
        for block in range(_u16(data, 0x0E)):
            at = _u32(data, 0x10 + 4 * block)
            if data[at:at + 4] == b"TEX0":
                tex_at = at
        if tex_at is None:
            raise NsbtxError("no texture block")
        self.tex = tex = data[tex_at:]
        self.texture_data = _u32(tex, 0x14)
        self.palette_data = _u32(tex, 0x38)
        self.textures = []
        for name, entry in _dictionary(tex, _u16(tex, 0x0E), 8):
            offset, params, _ = struct.unpack("<HHI", entry)
            self.textures.append((name, 8 << ((params >> 4) & 7), 8 << ((params >> 7) & 7), (params >> 10) & 7,
                                  (params >> 13) & 1, offset << 3))
        self.palettes = [(name, _u16(entry, 0) << 3) for name, entry in _dictionary(tex, _u32(tex, 0x34), 4)]

    def decode(self, texture: int, palette: int = 0) -> tuple[int, int, list[tuple[int, int, int, int]]]:
        """(width, height, RGBA pixels row by row) of a 16-colour texture with the palette chosen."""
        _, width, height, fmt, transparent, offset = self.textures[texture]
        if fmt != FORMAT_16_COLOURS:
            raise NsbtxError("only 16-colour textures are read")
        base = self.palette_data + self.palettes[palette][1]
        colours = [_u16(self.tex, base + 2 * n) for n in range(16)]
        start = self.texture_data + offset
        if start + width * height // 2 > len(self.tex) or base + 32 > len(self.tex):
            raise NsbtxError("a texture or palette runs past the archive")
        pixels = []
        for n in range(width * height):
            byte = self.tex[start + n // 2]
            index = byte >> 4 if n & 1 else byte & 15
            c = colours[index]
            r, g, b = (c & 31) * 255 // 31, (c >> 5 & 31) * 255 // 31, (c >> 10 & 31) * 255 // 31
            pixels.append((r, g, b, 0 if transparent and index == 0 else 255))
        return width, height, pixels
