#ifndef FIRERED_3DS_FAST_CELLS_H
#define FIRERED_3DS_FAST_CELLS_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Layer-texture cells written straight to a vertex buffer (from ZallaxDev's Emerald 3DS FastCells). A cell is 4
 * vertices in a linear buffer and a layer's changed cells are one indexed draw: GPU_REPLACE from the tile atlas with
 * blend ONE/ZERO and no alpha test, so a tile overwrites its whole 8x8 square, transparent texels included, and an
 * empty cell is drawn transparent. That is the picture the Citro2D path makes with a clear and a quad per cell, at
 * a fraction of the CPU: Citro2D costs ~4.6 us a quad on an Old 3DS, and a new map or screen redraws 3000-4000.
 *
 * The caller sets the target, the view and the atlas through Citro2D (a quad of no size, then C2D_Flush) and then
 * calls CtrFastCells_Draw, which puts Citro2D's attribute layout, buffer and texture stage back as they were.
 * sdmc:/3ds/twinembers/fastcells.off keeps the Citro2D path (for comparing on hardware).
 */
bool CtrFastCells_Init(void);
/* The buffers are refilled from the start each frame (call after C3D_FrameBegin). */
void CtrFastCells_FrameBegin(void);
/* Whether `count` more cells fit this frame; false keeps the Citro2D path for them. */
bool CtrFastCells_Room(unsigned count);
/* cells[0..count) index the layer's cells (columns 32 or 64 wide); slot[cell] is its atlas slot or < 0 for empty,
 * entry[cell] its tilemap entry (bits 10/11 flip). The atlas is atlasSize texels square, 8x8 slots. */
void CtrFastCells_Draw(const uint16_t *cells, unsigned count, const int16_t *slot, const uint16_t *entry,
                       unsigned columns, unsigned atlasSize);

#endif
