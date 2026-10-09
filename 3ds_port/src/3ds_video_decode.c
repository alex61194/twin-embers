#include "3ds_video.h"

uint32_t CtrVideo_TextMapOffset(unsigned x, unsigned y, unsigned size)
{
    unsigned width = (size & 1) ? 64 : 32;
    unsigned height = (size & 2) ? 64 : 32;
    x &= width - 1;
    y &= height - 1;
    return (((y / 32) * (width / 32) + x / 32) * 1024
            + (y & 31) * 32 + (x & 31)) * 2;
}

int CtrVideo_MosaicOrigin(int screen, unsigned size)
{
    if (size <= 1) return screen;
    int remainder = screen % (int)size;
    if (remainder < 0) remainder += size;
    return screen - remainder;
}

uint32_t CtrVideo_Texel(unsigned x, unsigned y, unsigned width)
{
    unsigned morton = (x & 1) | ((y & 1) << 1) | ((x & 2) << 1)
                    | ((y & 2) << 2) | ((x & 4) << 2) | ((y & 4) << 3);
    return ((y / 8) * (width / 8) + x / 8) * 64 + morton;
}

uint32_t CtrVideo_RGBA8(uint16_t color, bool opaque)
{
    unsigned r = color & 31, g = (color >> 5) & 31, b = (color >> 10) & 31;
    r = (r << 3) | (r >> 2);
    g = (g << 3) | (g >> 2);
    b = (b << 3) | (b >> 2);
    /* PICA texture memory is ABGR bytes (little endian uint32 = RRGGBBAA). */
    return (r << 24) | (g << 16) | (b << 8) | (opaque ? 255 : 0);
}

uint16_t CtrVideo_RGBA5551(uint16_t color)
{
    /* GBA BGR555 -> PICA RGBA5551, with no loss of source colour precision. */
    return ((color & 31) << 11) | ((color & 0x3e0) << 1)
         | ((color >> 9) & 0x3e) | 1;
}

unsigned CtrVideo_ObjTile(unsigned base, unsigned x, unsigned y,
                          unsigned width, bool color256, bool mapping1d)
{
    unsigned units = color256 ? 2 : 1;
    if (color256)
        base &= ~1u;
    return (base + y * (mapping1d ? width / 8 * units : 32) + x * units) & 1023;
}

bool CtrVideo_ObjOpaque(const uint8_t *objVram, unsigned base,
                        unsigned x, unsigned y, unsigned width,
                        bool color256, bool mapping1d)
{
    unsigned tile = CtrVideo_ObjTile(base, x / 8, y / 8, width,
                                      color256, mapping1d);
    unsigned offset = tile * 32 + (y & 7) * (color256 ? 8 : 4);
    if (color256)
        return objVram[offset + (x & 7)] != 0;
    unsigned packed = objVram[offset + ((x & 7) / 2)];
    return ((packed >> ((x & 1) * 4)) & 15) != 0;
}

int32_t CtrVideo_AffineReference(uint32_t value)
{
    value &= 0x0fffffff;
    return (value & 0x08000000) ? (int32_t)(value | 0xf0000000) : (int32_t)value;
}
