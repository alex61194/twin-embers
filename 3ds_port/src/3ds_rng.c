/* Original port entropy folding, alex61194 with AI assistance; MIT. */
#include "3ds_rng.h"

static uint32_t Mix(uint32_t x)
{
    x ^= x >> 16;
    x *= UINT32_C(0x7feb352d);
    x ^= x >> 15;
    x *= UINT32_C(0x846ca68b);
    return x ^ (x >> 16);
}

uint16_t CtrRng_Fold(uint64_t random, uint64_t ticks, uint64_t milliseconds,
                     uint32_t sample)
{
    uint32_t x = Mix((uint32_t)random);
    x = Mix(x ^ (uint32_t)(random >> 32));
    x = Mix(x ^ (uint32_t)ticks);
    x = Mix(x ^ (uint32_t)(ticks >> 32));
    x = Mix(x ^ (uint32_t)milliseconds);
    x = Mix(x ^ (uint32_t)(milliseconds >> 32));
    return (uint16_t)Mix(x ^ sample);
}
