#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "3ds_dma.h"
#include "gba_shadow.h"

enum { ENABLE = 0x8000, FIXED_SRC = 0x0100, WORD = 0x0400,
       REPEAT = 0x0200, DEST_RELOAD = 0x0060,
       VBLANK = 0x1000, HBLANK = 0x2000 };

int main(void)
{
    uint32_t source[4] = {0x11223344, 0x55667788, 0x99aabbcc, 0xddeeff00};
    uint32_t dest[4] = {0};
    GbaShadow_Reset();
    CtrDma_Reset();
    CtrDma_Set(3, source, dest, ((ENABLE | WORD) << 16) | 4);
    assert(memcmp(source, dest, sizeof(source)) == 0);
    assert((gGbaShadow.regs[0x0df] & 0x80) == 0);

    memset(dest, 0, sizeof(dest));
    CtrDma_Set(3, &source[1], dest, ((ENABLE | WORD | FIXED_SRC) << 16) | 4);
    for (unsigned i = 0; i < 4; ++i) assert(dest[i] == source[1]);

    memset(dest, 0, sizeof(dest));
    CtrDma_Set(2, source, dest, ((ENABLE | WORD | VBLANK) << 16) | 2);
    assert(dest[0] == 0);
    CtrDma_RunVBlank();
    assert(dest[0] == source[0] && dest[1] == source[1] && dest[2] == 0);
    memset(dest, 0, sizeof(dest));
    CtrDma_RunVBlank();
    assert(dest[0] == 0);

    CtrDma_Set(2, source, dest, ((ENABLE | WORD | VBLANK) << 16) | 2);
    CtrDma_Stop(2);
    CtrDma_RunVBlank();
    assert(dest[0] == 0);

    /* TransferPlttBuffer shape: 16-bit inc/inc immediate copy of a full
     * 1 KiB palette bank. Regression test: game palette DMA reached the
     * bridge registers but never executed before DmaSet was routed here. */
    {
        static uint16_t palSrc[512];
        static uint16_t palDest[512];
        for (unsigned i = 0; i < 512; ++i)
            palSrc[i] = (uint16_t)(i * 7u + 3u);
        memset(palDest, 0, sizeof(palDest));
        CtrDma_Set(3, palSrc, palDest, ((ENABLE) << 16) | 512);
        assert(memcmp(palSrc, palDest, sizeof(palSrc)) == 0);
        assert((gGbaShadow.regs[0x0df] & 0x80) == 0);
    }
    /* Title BLDY: VBlank writes line 0, repeated HBlank DMA starts at line 1.
     * Reading the compositor snapshot must not advance or mutate DMA state. */
    {
        uint16_t brightness[4] = {7, 12, 3, 0};
        uint16_t bldy = brightness[0], lines[4] = {0};
        uint32_t control = ((ENABLE | HBLANK | REPEAT | DEST_RELOAD) << 16) | 1;
        CtrDma_Set(0, brightness + 1, &bldy, control);
        assert(CtrDma_ReadHBlank16(0, &bldy, lines, 4));
        assert(memcmp(lines, brightness, sizeof(lines)) == 0);
        assert(bldy == 7);
        assert(CtrDma_ReadHBlank16(0, &bldy, lines, 4));
        assert(memcmp(lines, brightness, sizeof(lines)) == 0);
        CtrDma_Stop(0);
        assert(!CtrDma_ReadHBlank16(0, &bldy, lines, 4));
        CtrDma_Set(0, brightness + 1, &bldy, control | ((uint32_t)WORD << 16));
        assert(!CtrDma_ReadHBlank16(0, &bldy, lines, 4));
    }
    return 0;
}
