/* CtrPal_DetectBlend recognises FireRed's BlendPalette fades exactly and
 * nothing else. */
#include <assert.h>
#include <stdint.h>

#include "3ds_palette_fade.h"

static uint16_t sBase[256], sCur[256];

static void Fade(unsigned coeff, uint16_t target)
{
    for (unsigned i = 0; i < 256; ++i)
        sCur[i] = CtrPal_Blend(sBase[i], coeff, target);
}

int main(void)
{
    uint32_t seed = 12345;
    for (unsigned i = 0; i < 256; ++i)
    {
        seed = seed * 1103515245u + 12345u;
        sBase[i] = (uint16_t)((seed >> 8) & 0x7fff);
    }
    unsigned coeff;
    uint16_t target;

    /* Unchanged palette: identity. */
    for (unsigned i = 0; i < 256; ++i) sCur[i] = sBase[i] | 0x8000;
    assert(CtrPal_DetectBlend(sBase, sCur, 256, &coeff, &target) && coeff == 0);

    /* Every step of the transition's gray flash and of a fade to black/white. */
    static const uint16_t targets[] = {11 | 11 << 5 | 11 << 10, 0, 0x7fff};
    for (unsigned k = 0; k < 3; ++k)
        for (unsigned c = 1; c <= 16; ++c)
        {
            Fade(c, targets[k]);
            assert(CtrPal_DetectBlend(sBase, sCur, 256, &coeff, &target));
            /* Whatever pair is reported reproduces the palette exactly. */
            for (unsigned i = 0; i < 256; ++i)
                assert(CtrPal_Blend(sBase[i], coeff, target) == sCur[i]);
            if (c < 16) assert(coeff == c && target == targets[k]);
        }

    /* A partial fade (one bank untouched) or a palette animation is not one. */
    Fade(8, 0);
    for (unsigned i = 16; i < 32; ++i) sCur[i] = sBase[i];
    assert(!CtrPal_DetectBlend(sBase, sCur, 256, &coeff, &target));
    for (unsigned i = 0; i < 256; ++i) sCur[i] = sBase[i];
    sCur[17] ^= 0x21;
    assert(!CtrPal_DetectBlend(sBase, sCur, 256, &coeff, &target));
    return 0;
}
