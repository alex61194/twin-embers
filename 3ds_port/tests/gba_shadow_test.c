#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "gba_shadow.h"

static unsigned sInterrupts;
static void OnVBlank(void)
{
    assert(gGbaShadow.regs[0x006] == 160);
    assert((gGbaShadow.regs[0x004] & 1) != 0);
    ++sInterrupts;
}

int main(void)
{
    assert(sizeof(gGbaShadow.regs) == 0x400);
    assert(sizeof(gGbaShadow.vram) == 0x18000);
    assert(sizeof(gGbaShadow.pltt) == 0x400);
    assert(sizeof(gGbaShadow.oam) == 0x400);
    assert((uintptr_t)gGbaShadow.regs % 4 == 0);
    assert((uintptr_t)gGbaShadow.vram % 4 == 0);
    assert((uintptr_t)gGbaShadow.pltt % 4 == 0);
    assert((uintptr_t)gGbaShadow.oam % 4 == 0);
    memset(&gGbaShadow, 0xa5, sizeof(gGbaShadow));
    GbaShadow_Reset();
    const uint8_t *bytes = (const uint8_t *)&gGbaShadow;
    for (size_t i = 0; i < sizeof(gGbaShadow); ++i)
        if (i != 0x130 && i != 0x131 && i != 0x208)
            assert(bytes[i] == 0);
    assert(gGbaShadow.regs[0x130] == 0xff);
    assert(gGbaShadow.regs[0x131] == 0x03);
    assert(gGbaShadow.regs[0x208] == 1);

    gGbaShadow.regs[0x004] = 0x08; /* Preserve the IRQ-enable bit. */
    GbaShadow_BeginVBlank();
    assert(gGbaShadow.regs[0x004] == 0x09);
    assert(gGbaShadow.regs[0x006] == 160);
    assert(gGbaShadow.regs[0x202] == 1);
    GbaShadow_EndVBlank();
    assert(gGbaShadow.regs[0x004] == 0x08);
    assert(gGbaShadow.regs[0x006] == 0);
    assert(gGbaShadow.regs[0x202] == 0);

    GbaShadow_DispatchVBlank(OnVBlank);
    assert(sInterrupts == 0);
    gGbaShadow.regs[0x200] = 1;
    GbaShadow_DispatchVBlank(OnVBlank);
    assert(sInterrupts == 1);
    gGbaShadow.regs[0x208] = 0;
    GbaShadow_DispatchVBlank(OnVBlank);
    assert(sInterrupts == 1);
    assert(gGbaShadow.regs[0x006] == 0);
    assert((gGbaShadow.regs[0x004] & 1) == 0);

    GbaShadow_SetHeldKeys(0x001 | 0x010);
    assert(gGbaShadow.regs[0x130] == 0xee);
    assert(gGbaShadow.regs[0x131] == 0x03);
    return 0;
}
