#ifndef FIRERED_3DS_GBA_SHADOW_H
#define FIRERED_3DS_GBA_SHADOW_H

#include <stdint.h>

enum {
    GBA_REG_BYTES = 0x400,
    GBA_VRAM_BYTES = 0x18000,
    GBA_PLTT_BYTES = 0x400,
    GBA_OAM_BYTES = 0x400,
};

/* Zallax's portable backend uses these same GBA memory sizes. Keep the banks
 * separate so a future renderer can snapshot each without touching 3DS MMIO. */
typedef struct {
    _Alignas(4) uint8_t regs[GBA_REG_BYTES];
    _Alignas(4) uint8_t vram[GBA_VRAM_BYTES];
    _Alignas(4) uint8_t pltt[GBA_PLTT_BYTES];
    _Alignas(4) uint8_t oam[GBA_OAM_BYTES];
} GbaShadowMemory;

extern GbaShadowMemory gGbaShadow;
struct SoundInfo;
extern struct SoundInfo *gGbaSoundInfoPtr;
extern uint16_t gGbaIntrCheck;
extern void *gGbaIntrVector;
void GbaShadow_Reset(void);
void GbaShadow_BeginVBlank(void);
void GbaShadow_EndVBlank(void);
/* Invoke the game's VBlank handler only when its IME and IE bits permit it. */
void GbaShadow_DispatchVBlank(void (*handler)(void));
/* FireRed programs the VCount match at line 150 for its audio sync tick
 * (m4aSoundVSync). Dispatch it once per frame, before VBlank, mirroring the
 * GBA scanline order 150 -> 160. */
void GbaShadow_BeginVCount(void);
void GbaShadow_EndVCount(void);
void GbaShadow_DispatchVCount(void (*handler)(void));
/* GBA button bits are active high here; KEYINPUT is stored active low. */
void GbaShadow_SetHeldKeys(uint16_t held);

#endif
