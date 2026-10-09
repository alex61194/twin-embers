/* Original port entropy adapter, alex61194 with AI assistance; MIT. */
#ifndef CTR_RNG_H
#define CTR_RNG_H
#include <stdint.h>

/* Fold platform entropy into FireRed's original 16-bit seed domain. Zero is
 * valid, as are collisions: neither Trainer IDs nor seeds are unique IDs. */
uint16_t CtrRng_Fold(uint64_t random, uint64_t ticks, uint64_t milliseconds,
                     uint32_t sample);
/* Called only at FireRed's existing title/player-naming sampling boundaries. */
uint16_t CtrRng_GetSeed(void);
#endif
