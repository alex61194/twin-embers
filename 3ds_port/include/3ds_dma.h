#ifndef FIRERED_3DS_DMA_H
#define FIRERED_3DS_DMA_H

#include <stdint.h>
#include <stdbool.h>

void CtrDma_Reset(void);
void CtrDma_Set(unsigned channel, const void *src, void *dest, uint32_t control);
void CtrDma_Stop(unsigned channel);
void CtrDma_RunVBlank(void);
/* Snapshot a repeated 16-bit, one-value HBlank DMA for the compositor.
 * out[0] is the value written for the first scanline in VBlank. */
bool CtrDma_ReadHBlank16(unsigned channel, const void *dest,
                         uint16_t *out, unsigned lines);

#endif
