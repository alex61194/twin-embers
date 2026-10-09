#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "gba_shadow.h"

void RegisterRamReset(uint32_t flags);
void CpuSet(const void *src, void *dest, uint32_t control);
void CpuFastSet(const void *src, void *dest, uint32_t control);
void LZ77UnCompWram(const void *src, void *dest);
void LZ77UnCompVram(const void *src, void *dest);

int main(void)
{
    uint32_t source[16], dest[16];
    for (unsigned i = 0; i < 16; ++i)
        source[i] = 0x11220000u + i;
    memset(dest, 0xcc, sizeof(dest));
    CpuSet(source, dest, 0x04000000 | 3);
    assert(memcmp(source, dest, 3 * sizeof(uint32_t)) == 0);
    assert(dest[3] == 0xcccccccc);

    memset(dest, 0, sizeof(dest));
    CpuSet(&source[2], dest, 0x01000000 | 0x04000000 | 4);
    for (unsigned i = 0; i < 4; ++i)
        assert(dest[i] == source[2]);
    assert(dest[4] == 0);

    uint16_t halfword = 0xa55a;
    memset(dest, 0, sizeof(dest));
    CpuSet(&halfword, dest, 0x01000000 | 3);
    const uint16_t *halves = (const uint16_t *)dest;
    assert(halves[0] == halfword && halves[1] == halfword && halves[2] == halfword);
    assert(halves[3] == 0);

    memset(dest, 0xcc, sizeof(dest));
    CpuFastSet(source, dest, 9);
    assert(memcmp(source, dest, sizeof(source)) == 0);
    memset(dest, 0, sizeof(dest));
    CpuFastSet(&source[1], dest, 0x01000000 | 1);
    for (unsigned i = 0; i < 8; ++i)
        assert(dest[i] == source[1]);
    assert(dest[8] == 0);

    GbaShadow_Reset();
    memset(gGbaShadow.vram, 0xa5, sizeof(gGbaShadow.vram));
    memset(gGbaShadow.pltt, 0xa5, sizeof(gGbaShadow.pltt));
    memset(gGbaShadow.oam, 0xa5, sizeof(gGbaShadow.oam));
    gGbaShadow.regs[0] = 0xa5;
    GbaShadow_SetHeldKeys(1);
    RegisterRamReset(0x08 | 0x10);
    assert(gGbaShadow.vram[0] == 0 && gGbaShadow.vram[sizeof(gGbaShadow.vram) - 1] == 0);
    assert(gGbaShadow.oam[0] == 0 && gGbaShadow.pltt[0] == 0xa5);
    assert(gGbaShadow.regs[0] == 0xa5);
    RegisterRamReset(0x80);
    assert(gGbaShadow.regs[0] == 0 && gGbaShadow.regs[0x208] == 0);
    assert(gGbaShadow.regs[0x130] == 0xfe && gGbaShadow.regs[0x131] == 3);
    /* Both BGs must sample an untransformed tilemap after BIOS reset.
     * Non-register resets must preserve an explicitly programmed collapse. */
    const uint8_t affineDefaults[16] = {0, 1, 0, 0, 0, 0, 0, 1};
    assert(memcmp(gGbaShadow.regs + 0x20, affineDefaults, 16) == 0);
    assert(memcmp(gGbaShadow.regs + 0x30, affineDefaults, 16) == 0);
    memset(gGbaShadow.regs + 0x20, 0, 32);
    RegisterRamReset(0x08 | 0x10);
    for (unsigned i = 0x20; i < 0x40; ++i)
        assert(gGbaShadow.regs[i] == 0);

    /* "ABABABAB": two literals followed by an overlapping six-byte copy. */
    const uint8_t compressed[] = {0x10, 8, 0, 0, 0x20, 'A', 'B', 0x30, 0x01};
    uint8_t decoded[9] = {0};
    LZ77UnCompWram(compressed, decoded);
    assert(memcmp(decoded, "ABABABAB", 8) == 0);
    assert(decoded[8] == 0);
    memset(decoded, 0, sizeof(decoded));
    LZ77UnCompVram(compressed, decoded);
    assert(memcmp(decoded, "ABABABAB", 8) == 0);
    return 0;
}
