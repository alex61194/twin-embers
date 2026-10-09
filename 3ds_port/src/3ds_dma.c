/* FireRed GBA DMA bridge: immediate and VBlank transfers in virtual RAM.
 * The title's repeated HBlank BLDY transfer is sampled by the compositor. */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "3ds_dma.h"
#include "gba_shadow.h"
#include "3ds_perf.h"
#include "3ds_assets.h"

enum {
    CHANNELS = 4,
    DMA0_OFFSET = 0x0b0,
    DMA_STRIDE = 12,
    DMA_ENABLE = 0x8000,
    DMA_REPEAT = 0x0200,
    DMA_32BIT = 0x0400,
    DMA_START_MASK = 0x3000,
    DMA_START_NOW = 0x0000,
    DMA_START_VBLANK = 0x1000,
    DMA_START_HBLANK = 0x2000,
};

typedef struct {
    const uint8_t *src;
    uint8_t *dest;
    uint8_t *reloadDest;
    uint32_t count;
    uint16_t control;
} DmaChannel;

static DmaChannel sChannels[CHANNELS];

static void StoreRegister(unsigned offset, uint32_t value)
{
    memcpy(gGbaShadow.regs + offset, &value, sizeof(value));
}

static void StoreControl(unsigned channel)
{
    unsigned offset = DMA0_OFFSET + DMA_STRIDE * channel + 8;
    uint32_t value = (sChannels[channel].count & 0xffff)
                   | ((uint32_t)sChannels[channel].control << 16);
    StoreRegister(offset, value);
}

void CtrDma_Reset(void)
{
    memset(sChannels, 0, sizeof(sChannels));
}

static void Transfer(unsigned channel)
{
    DmaChannel *dma = &sChannels[channel];
    unsigned width = (dma->control & DMA_32BIT) ? 4 : 2;
    unsigned sourceMode = (dma->control >> 7) & 3;
    unsigned destMode = (dma->control >> 5) & 3;
    if (sourceMode == 3)
        return; /* Prohibited on GBA; never read an invented source address. */
    const void *start = sourceMode == 1
        ? dma->src - (dma->count - 1) * width : dma->src;
    CTR_ASSET_READ(start, sourceMode == 2 ? width : (size_t)dma->count * width);

    uint64_t perfStart = CtrPerf_Begin();
    CTR_PERF_COUNT(PERF_DMA_BYTES, dma->count * width);
    CTR_PERF_TRANSFER(dma->dest, dma->count * width);

    for (uint32_t i = 0; i < dma->count; ++i)
    {
        uint32_t value = 0;
        memcpy(&value, dma->src, width);
        memcpy(dma->dest, &value, width);
        if (sourceMode == 0) dma->src += width;
        if (sourceMode == 1) dma->src -= width;
        if (destMode == 0 || destMode == 3) dma->dest += width;
        if (destMode == 1) dma->dest -= width;
    }
    if (destMode == 3)
        dma->dest = dma->reloadDest;
    if (!(dma->control & DMA_REPEAT) || (dma->control & DMA_START_MASK) == DMA_START_NOW)
        dma->control &= ~DMA_ENABLE;
    StoreControl(channel);
    CtrPerf_End(PERF_DMA, perfStart);
}

void CtrDma_Set(unsigned channel, const void *src, void *dest, uint32_t control)
{
    if (channel >= CHANNELS)
        return;
    DmaChannel *dma = &sChannels[channel];
    dma->src = src;
    dma->dest = dest;
    dma->reloadDest = dest;
    dma->control = (uint16_t)(control >> 16);
    dma->count = control & 0xffff;
    if (dma->count == 0)
        dma->count = channel == 3 ? 0x10000 : 0x4000;
    unsigned offset = DMA0_OFFSET + DMA_STRIDE * channel;
    StoreRegister(offset, (uint32_t)(uintptr_t)src);
    StoreRegister(offset + 4, (uint32_t)(uintptr_t)dest);
    StoreControl(channel);
    if ((dma->control & DMA_ENABLE) && (dma->control & DMA_START_MASK) == DMA_START_NOW)
        Transfer(channel);
}

void CtrDma_Stop(unsigned channel)
{
    if (channel >= CHANNELS)
        return;
    sChannels[channel].control &= ~DMA_ENABLE;
    StoreControl(channel);
}

void CtrDma_RunVBlank(void)
{
    for (unsigned channel = 0; channel < CHANNELS; ++channel)
        if ((sChannels[channel].control & (DMA_ENABLE | DMA_START_MASK)) ==
            (DMA_ENABLE | DMA_START_VBLANK))
            Transfer(channel);
}

bool CtrDma_ReadHBlank16(unsigned channel, const void *dest,
                         uint16_t *out, unsigned lines)
{
    if (channel >= CHANNELS || !dest || !out || !lines)
        return false;
    const DmaChannel *dma = &sChannels[channel];
    if (dma->dest != dest || !dma->src || dma->count != 1
     || (dma->control & (DMA_ENABLE | DMA_START_MASK | DMA_REPEAT | DMA_32BIT))
        != (DMA_ENABLE | DMA_START_HBLANK | DMA_REPEAT)
     || ((dma->control >> 7) & 3) != 0 /* incrementing source */
     || ((dma->control >> 5) & 3) != 3) /* reload destination */
        return false;
    /* FireRed supplies the source starting at entry 1. Its VBlank handler
     * writes entry 0 directly before arming DMA. HBlank after line y writes
     * entry y+1, so that value belongs to the next displayed line. */
    memcpy(out, dest, sizeof(*out));
    for (unsigned y = 1; y < lines; ++y)
        memcpy(out + y, dma->src + (y - 1) * sizeof(*out), sizeof(*out));
    return true;
}
