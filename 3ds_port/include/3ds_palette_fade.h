#ifndef FIRERED_3DS_PALETTE_FADE_H
#define FIRERED_3DS_PALETTE_FADE_H

#include <stdbool.h>
#include <stdint.h>

/*
 * FireRed fades a whole screen with BlendPalette (src/blend_palette.c):
 * every channel moves v + (((t - v) * coeff) >> 4) towards one colour t. When
 * the background palette is exactly that blend of the palette the field
 * layers were composed with, the compositor can keep those layers and tint
 * them on the GPU instead of re-uploading every tile and redrawing every
 * cell on each step of the fade. Shared with the host test.
 */
static inline unsigned CtrPal_BlendChannel(unsigned v, unsigned coeff, unsigned t)
{
    return (unsigned)((int)v + ((((int)t - (int)v) * (int)coeff) >> 4));
}

static inline uint16_t CtrPal_Blend(uint16_t color, unsigned coeff, uint16_t target)
{
    uint16_t out = 0;
    for (unsigned shift = 0; shift < 15; shift += 5)
        out |= (uint16_t)(CtrPal_BlendChannel((color >> shift) & 31, coeff, (target >> shift) & 31) << shift);
    return out;
}

/* True when every cur[i] == CtrPal_Blend(base[i], *coeff, *target) (bit 15
 * ignored, as on hardware). *coeff is 0 when cur equals base. Of the pairs
 * that fit, the smallest coefficient is reported. */
static inline bool CtrPal_DetectBlend(const uint16_t *base, const uint16_t *cur, unsigned count,
                                      unsigned *coeff, uint16_t *target)
{
    bool same = true;
    for (unsigned i = 0; i < count && same; ++i)
        same = ((base[i] ^ cur[i]) & 0x7fff) == 0;
    if (same)
    {
        *coeff = 0;
        *target = 0;
        return true;
    }
    for (unsigned c = 1; c <= 16; ++c)
    {
        uint16_t found = 0;
        unsigned shift;
        for (shift = 0; shift < 15; shift += 5)
        {
            unsigned t;
            for (t = 0; t < 32; ++t)
            {
                unsigned i;
                for (i = 0; i < count; ++i)
                    if (CtrPal_BlendChannel((base[i] >> shift) & 31, c, t) != ((cur[i] >> shift) & 31u))
                        break;
                if (i == count) break;
            }
            if (t == 32) break;
            found |= (uint16_t)(t << shift);
        }
        if (shift >= 15)
        {
            *coeff = c;
            *target = found;
            return true;
        }
    }
    return false;
}

/* How far a run of palette entries (the background banks a battle scene lives in) has been blended towards one colour
 * while the rest of the palette has not: 0 when they are unchanged or are not one blend, else the coefficient (1-16) and
 * the colour in *target. FireRed's battle moves blend only the scene's banks (AnimTask_BlendBattleAnimPal with
 * F_PAL_BG), which the whole-palette test above never sees. */
static inline unsigned CtrPal_RunFade(const uint16_t *unfaded, const uint16_t *faded, unsigned first, unsigned count,
                                      uint16_t *target)
{
    unsigned coeff = 0;

    *target = 0;
    if (!CtrPal_DetectBlend(unfaded + first, faded + first, count, &coeff, target))
    {
        *target = 0;
        return 0;
    }
    return coeff;
}

#endif
