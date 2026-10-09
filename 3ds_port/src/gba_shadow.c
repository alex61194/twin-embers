#include <stddef.h>
#include <string.h>
#include "gba_shadow.h"

_Static_assert(offsetof(GbaShadowMemory, regs) == 0, "register bank offset");
_Static_assert(offsetof(GbaShadowMemory, vram) == GBA_REG_BYTES, "VRAM bank offset");
_Static_assert(offsetof(GbaShadowMemory, pltt) == GBA_REG_BYTES + GBA_VRAM_BYTES, "palette bank offset");
_Static_assert(offsetof(GbaShadowMemory, oam) == GBA_REG_BYTES + GBA_VRAM_BYTES + GBA_PLTT_BYTES, "OAM bank offset");
_Static_assert(sizeof(GbaShadowMemory) == GBA_REG_BYTES + GBA_VRAM_BYTES + GBA_PLTT_BYTES + GBA_OAM_BYTES, "shadow bank size");

GbaShadowMemory gGbaShadow;
struct SoundInfo *gGbaSoundInfoPtr;
uint16_t gGbaIntrCheck;
void *gGbaIntrVector;

enum {
    REG_DISPSTAT_OFFSET = 0x004,
    REG_VCOUNT_OFFSET = 0x006,
    REG_KEYINPUT_OFFSET = 0x130,
    REG_IF_OFFSET = 0x202,
    REG_IE_OFFSET = 0x200,
    REG_IME_OFFSET = 0x208,
    GBA_KEY_MASK = 0x03ff,
    VBLANK_BIT = 1,
    VCOUNT_BIT = 4,
    VCOUNT_LINE = 150,
};

static uint16_t Read16(unsigned offset)
{
    return (uint16_t)gGbaShadow.regs[offset]
         | (uint16_t)gGbaShadow.regs[offset + 1] << 8;
}

static void Write16(unsigned offset, uint16_t value)
{
    gGbaShadow.regs[offset] = (uint8_t)value;
    gGbaShadow.regs[offset + 1] = (uint8_t)(value >> 8);
}

void GbaShadow_Reset(void)
{
    memset(&gGbaShadow, 0, sizeof(gGbaShadow));
    gGbaSoundInfoPtr = NULL;
    gGbaIntrCheck = 0;
    gGbaIntrVector = NULL;
    Write16(REG_KEYINPUT_OFFSET, GBA_KEY_MASK);
    Write16(REG_IME_OFFSET, 1);
}

void GbaShadow_BeginVBlank(void)
{
    Write16(REG_VCOUNT_OFFSET, 160);
    Write16(REG_DISPSTAT_OFFSET, Read16(REG_DISPSTAT_OFFSET) | VBLANK_BIT);
    Write16(REG_IF_OFFSET, Read16(REG_IF_OFFSET) | VBLANK_BIT);
}

void GbaShadow_EndVBlank(void)
{
    Write16(REG_DISPSTAT_OFFSET, Read16(REG_DISPSTAT_OFFSET) & ~VBLANK_BIT);
    Write16(REG_IF_OFFSET, Read16(REG_IF_OFFSET) & ~VBLANK_BIT);
    Write16(REG_VCOUNT_OFFSET, 0);
}

void GbaShadow_DispatchVBlank(void (*handler)(void))
{
    GbaShadow_BeginVBlank();
    if (handler != NULL && (Read16(REG_IME_OFFSET) & 1) != 0
        && (Read16(REG_IE_OFFSET) & VBLANK_BIT) != 0)
        handler();
    GbaShadow_EndVBlank();
}

void GbaShadow_BeginVCount(void)
{
    Write16(REG_VCOUNT_OFFSET, VCOUNT_LINE);
    Write16(REG_DISPSTAT_OFFSET, Read16(REG_DISPSTAT_OFFSET) | VCOUNT_BIT);
    Write16(REG_IF_OFFSET, Read16(REG_IF_OFFSET) | VCOUNT_BIT);
}

void GbaShadow_EndVCount(void)
{
    Write16(REG_DISPSTAT_OFFSET, Read16(REG_DISPSTAT_OFFSET) & (uint16_t)~VCOUNT_BIT);
    Write16(REG_IF_OFFSET, Read16(REG_IF_OFFSET) & (uint16_t)~VCOUNT_BIT);
}

void GbaShadow_DispatchVCount(void (*handler)(void))
{
    GbaShadow_BeginVCount();
    if (handler != NULL && (Read16(REG_IME_OFFSET) & 1) != 0
        && (Read16(REG_IE_OFFSET) & VCOUNT_BIT) != 0)
        handler();
    GbaShadow_EndVCount();
}

void GbaShadow_SetHeldKeys(uint16_t held)
{
    Write16(REG_KEYINPUT_OFFSET, (uint16_t)(~held) & GBA_KEY_MASK);
}
