#include <assert.h>
#include <stdint.h>

#include "gba/types.h"
#include "gba/defines.h"
#include "gba/io_reg.h"

int main(void)
{
#ifdef CTR_GBA_STAGE
    assert(DISPLAY_WIDTH == 240 && DISPLAY_HEIGHT == 160);
#else
    assert(DISPLAY_WIDTH == 400 && DISPLAY_HEIGHT == 240);
    assert(FIELD_VIEW_OFFSET_X == 80 && FIELD_VIEW_OFFSET_Y == 32);
#endif
    GbaShadow_Reset();

    REG_DISPCNT = 0x1234;
    assert(gGbaShadow.regs[0] == 0x34);
    assert(gGbaShadow.regs[1] == 0x12);
    *(vu16 *)BG_PLTT = 0x7fff;
    *(vu16 *)OBJ_PLTT = 0x4210;
    assert(gGbaShadow.pltt[0] == 0xff);
    assert(gGbaShadow.pltt[1] == 0x7f);
    assert(gGbaShadow.pltt[0x200] == 0x10);
    assert(gGbaShadow.pltt[0x201] == 0x42);
    *(vu16 *)BG_CHAR_ADDR(1) = 0xabcd;
    assert(gGbaShadow.vram[0x4000] == 0xcd);
    assert(gGbaShadow.vram[0x4001] == 0xab);
    ((vu16 *)OAM)[0] = 0x9876;
    assert(gGbaShadow.oam[0] == 0x76);
    assert(gGbaShadow.oam[1] == 0x98);

    SOUND_INFO_PTR = (struct SoundInfo *)(uintptr_t)0x1234;
    INTR_CHECK = 7;
    INTR_VECTOR = (void *)(uintptr_t)0x5678;
    assert(gGbaSoundInfoPtr == (struct SoundInfo *)(uintptr_t)0x1234);
    assert(gGbaIntrCheck == 7);
    assert(gGbaIntrVector == (void *)(uintptr_t)0x5678);
    GbaShadow_Reset();
    assert(SOUND_INFO_PTR == 0);
    assert(INTR_CHECK == 0);
    assert(INTR_VECTOR == 0);

    GbaShadow_BeginVBlank();
    assert(REG_VCOUNT == 160);
    assert(REG_DISPSTAT & DISPSTAT_VBLANK);
    assert(REG_IF & INTR_FLAG_VBLANK);
    GbaShadow_EndVBlank();
    assert(REG_VCOUNT == 0);
    assert((REG_DISPSTAT & DISPSTAT_VBLANK) == 0);
    assert((REG_IF & INTR_FLAG_VBLANK) == 0);
    return 0;
}
