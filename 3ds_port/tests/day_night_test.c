/* CtrDayNight_Tint: the plain day stays untouched, no hour is darker than
 * the night, and the colour moves smoothly through the day without a jump. */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include "3ds_day_night.h"

static int Channel(uint32_t tint, unsigned shift) { return (int)((tint >> shift) & 255); }

int main(void)
{
    /* Day 09:00-17:30 is exactly white, so the compositor draws nothing. */
    for (uint32_t s = 32400u; s <= 63000u; s += 60u)
        assert(CtrDayNight_Tint(s) == 0xffffffu);
    /* Night 21:00-05:00 is the night key. */
    uint32_t night = CtrDayNight_Tint(0);
    assert(night == ((92u << 16) | (107u << 8) | 168u));
    assert(CtrDayNight_Tint(75600u) == night && CtrDayNight_Tint(18000u) == night && CtrDayNight_Tint(86399u) == night);
    /* Dawn and dusk land on their keys. */
    assert(CtrDayNight_Tint(23400u) == ((255u << 16) | (214u << 8) | 219u));
    assert(CtrDayNight_Tint(68400u) == ((255u << 16) | (184u << 8) | 153u));
    /* A second never moves a channel by more than one step; the day wraps. */
    uint32_t prev = CtrDayNight_Tint(86399u);
    for (uint32_t s = 0; s < CTR_DAY_NIGHT_DAY_SECONDS; ++s)
    {
        uint32_t t = CtrDayNight_Tint(s);
        int sum = 0, nightSum = 0;
        for (unsigned shift = 0; shift < 24; shift += 8)
        {
            assert(abs(Channel(t, shift) - Channel(prev, shift)) <= 1);
            sum += Channel(t, shift);
            nightSum += Channel(night, shift);
        }
        assert(sum >= nightSum);
        prev = t;
    }
    assert(CtrDayNight_Tint(CTR_DAY_NIGHT_DAY_SECONDS + 100u) == CtrDayNight_Tint(100u));
    return 0;
}
