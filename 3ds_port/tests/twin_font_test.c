#include <assert.h>
#include <stdio.h>
#include "twin_font.h"
int main(void)
{
    uint8_t src[64]={0};
    uint32_t rows[16];
    src[1]=0x40; /* top-left tile: first pixel foreground */
    src[0]=0x02; /* top-left tile: eighth pixel shadow */
    src[17]=0x80; /* top-right tile: ninth pixel shadow */
    src[33]=0x04; /* lower-left tile: third pixel foreground */
    TwinGlyphDecode(src,16,rows);
    assert(rows[0]==(1u | (2u<<14) | (2u<<16)));
    assert(rows[8]==(1u<<4));
    for (unsigned y=1;y<16;++y) if (y!=8) assert(rows[y]==0);
    TwinGlyphDecode(src,6,rows);
    assert(rows[0]==1);
    assert(rows[8]==(1u<<4));
    TwinGlyphDecode(src,0,rows);
    for (unsigned y=0;y<16;++y) assert(rows[y]==0);
    puts("PASS pack-backed font tile order, rows and width bounds");
    return 0;
}
