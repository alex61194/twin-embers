#ifndef TWIN_FONT_H
#define TWIN_FONT_H
#include <stdint.h>
/* Decode tiled 16x16, two-bit rows, with swapped byte pairs. No glyph artwork.
 * Each tile has 8 rows; its high byte encodes the left four pixels. */
static inline void TwinGlyphDecode(const uint8_t *src, unsigned width, uint32_t out[16])
{
    if (width > 16) width = 16;
    for (unsigned y=0; y<16; ++y) {
        out[y]=0;
        for (unsigned x=0; x<width; ++x) {
            unsigned tile=(y/8)*2+x/8, row=y%8;
            unsigned bits=(unsigned)src[tile*16+row*2+1]<<8 | src[tile*16+row*2];
            unsigned value=(bits >> ((7-x%8)*2)) & 3;
            if (value==1 || value==2) out[y] |= value << (x*2);
        }
    }
}
#endif
