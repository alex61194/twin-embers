#ifndef CTR_BOTTOM_H
#define CTR_BOTTOM_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Bottom screen: native 320x240 touch companion to the TOP gameplay.
 * TOP remains authoritative; this is only a view/controller over the
 * existing FireRed state (FIELD/home + Party for this milestone).
 *
 * Drawing is CPU-only into a canvas matching the bottom framebuffer
 * layout (RGB565, column-major, each column bottom-to-top) and copied
 * only when the snapshot changes. No GPU, no VRAM, no audio changes.
 * Input uses the existing CtrInput touch state; HID is never polled here.
 */
#define CTR_BOTTOM_WIDTH 320
#define CTR_BOTTOM_HEIGHT 240

/* Native side (libctru): copy the rectangle [x0, x1) x [y0, y1) (screen
 * coordinates, y down) of the canvas to the screen, nothing else. */
void CtrBottom_BlitRect(const uint16_t *canvas, int x0, int y0, int x1, int y1);

/* Game side. Init once before AgbMain; Frame once per frame after input is
 * scanned and before the game reads its keys. */
void CtrBottom_Init(void);
void CtrBottom_Frame(void);
/* Keys the bottom screen presses on the player's behalf this frame. */
uint16_t CtrBottom_InjectedKeys(void);
/* Filter physical keys before combining them with the bottom UI's injected keys. */
uint16_t CtrBottom_FilterGameKeys(uint16_t held);
/* False when disabled or resources missing: TOP fallback stays visible. */
bool CtrBottom_IsEnabled(void);
/* The overworld turbo (3ds_fast_forward.h): Applies is true while the stable field is the screen's owner (no menu,
 * PokeTouch screen, route, battle, load or fade); CanCycle also needs nothing said or scripted. */
bool CtrBottom_TurboApplies(void);
bool CtrBottom_TurboCanCycle(void);

#endif
