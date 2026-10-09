#ifndef FIRERED_3DS_VRAM_TRACK_H
#define FIRERED_3DS_VRAM_TRACK_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* What changed in the logical VRAM, a kilobyte at a time. Shared by the
 * compositor (3ds_video.c) and its host test.
 *
 * The game writes VRAM in many ways (CPU copies, DMA, decompression, plain
 * pointers), so instead of hooking them all, once per presented frame the
 * whole VRAM is compared with a copy of the last one. stamps[b] is the frame
 * token at which block b last differed. Anything verified against VRAM at
 * token t (a cached tile's bytes) still holds while its block's stamp is <= t,
 * so the compositor compares a tile's bytes only when its block changed.
 * Ported from ZallaxDev's DualScreen compositor (MIT), which measured the
 * per-tile memcmp it replaces at about a millisecond a frame.
 *
 * Tiles never straddle blocks: 4bpp tiles are 32-byte aligned, 8bpp tiles
 * 64-byte aligned, and a block is 1024 bytes. */
enum {
    CTR_VRAM_TRACK_BLOCK = 1024,
    CTR_VRAM_TRACK_WORDS = CTR_VRAM_TRACK_BLOCK / 4,
};

typedef uint32_t __attribute__((may_alias)) CtrVramWord;

/* Compares `blocks` blocks of `vram` (4-byte aligned) with `copy`, refreshes
 * the copy of each block that differs and stamps it with `token`. Returns the
 * number of blocks that changed. */
static inline unsigned CtrVramTrack_Update(const uint8_t *vram, uint32_t *copy,
                                           uint32_t *stamps, unsigned blocks, uint32_t token)
{
    unsigned changed = 0;

    for (unsigned b = 0; b < blocks; ++b)
    {
        const CtrVramWord *now = (const CtrVramWord *)(vram + b * CTR_VRAM_TRACK_BLOCK);
        const uint32_t *old = copy + b * CTR_VRAM_TRACK_WORDS;

        /* Eight words at a time, one branch for all eight. */
        for (unsigned i = 0; i < CTR_VRAM_TRACK_WORDS; i += 8)
        {
            if (((now[i] ^ old[i]) | (now[i + 1] ^ old[i + 1]) | (now[i + 2] ^ old[i + 2])
                 | (now[i + 3] ^ old[i + 3]) | (now[i + 4] ^ old[i + 4]) | (now[i + 5] ^ old[i + 5])
                 | (now[i + 6] ^ old[i + 6]) | (now[i + 7] ^ old[i + 7])) != 0)
            {
                memcpy(copy + b * CTR_VRAM_TRACK_WORDS, now, CTR_VRAM_TRACK_BLOCK);
                stamps[b] = token;
                ++changed;
                break;
            }
        }
    }
    return changed;
}

/* May the bytes at `address`, verified at token `checked`, have changed? */
static inline bool CtrVramTrack_MayDiffer(const uint32_t *stamps, unsigned address, uint32_t checked)
{
    return stamps[address / CTR_VRAM_TRACK_BLOCK] > checked;
}

#endif
