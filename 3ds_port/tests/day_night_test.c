/* CtrDayNight_Tint: the plain day stays untouched, no hour is darker than
 * the night, and the colour moves smoothly through the day without a jump.
 * The palette side: shading, how lit the windows are, what is glass, and the
 * lights table (3ds_day_night_lights.h) well formed. */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include "3ds_day_night.h"
#include "3ds_day_night_lights.h"

static int Channel(uint32_t tint, unsigned shift) { return (int)((tint >> shift) & 255); }

static uint16_t Bgr(unsigned r, unsigned g, unsigned b) { return (uint16_t)((r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10)); }

/* Every entry: a tile of the character block, a map palette (0-12), the
 * primary tileset's own tiles and palettes in General, and no repeats. */
static void Table(const uint16_t *table, unsigned count, int general)
{
    assert(count > 0);
    for (unsigned i = 0; i < count; ++i)
    {
        unsigned tile = table[i] & 1023, bank = table[i] >> 12;
        assert(bank < 13 && (table[i] & 0x0c00) == 0);
        if (general) assert(tile < 640 && bank < 7);
        for (unsigned j = 0; j < i; ++j) assert(table[j] != table[i]);
    }
}
#define TABLE(t, general) Table(t, sizeof(t) / sizeof(t[0]), general)

int main(void)
{
    /* Day 09:00-17:30 is exactly white, so the compositor converts nothing. */
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

    /* Shading: white leaves every colour as it is, the night darkens, black stays black. */
    for (uint32_t c = 0; c < 0x8000u; ++c)
    {
        uint16_t shaded = CtrDayNight_Shade((uint16_t)c, night);
        assert(CtrDayNight_Shade((uint16_t)c, 0xffffffu) == c);
        for (unsigned shift = 0; shift < 15; shift += 5)
            assert(((shaded >> shift) & 31) <= ((c >> shift) & 31));
    }
    assert(CtrDayNight_Shade(0x7fff, night) == ((11u) | (13u << 5) | (20u << 10)));
    assert(CtrDayNight_Shade(0, night) == 0);
    /* The windows: dark by day, fully lit at night, partly at dusk and less at dawn. */
    assert(CtrDayNight_Lights(0xffffffu) == 0 && CtrDayNight_Lights(night) == 256);
    unsigned dusk = CtrDayNight_Lights(CtrDayNight_Tint(68400u)), dawn = CtrDayNight_Lights(CtrDayNight_Tint(23400u));
    assert(dawn > 0 && dawn < dusk && dusk < 256);
    /* Glass: the house windows' light blues (General, palette 3, colours 10-13), not grass, wall, sill or shadow. */
    assert(CtrDayNight_Glass(Bgr(156, 213, 255)) && CtrDayNight_Glass(Bgr(115, 189, 246))
           && CtrDayNight_Glass(Bgr(98, 164, 222)) && CtrDayNight_Glass(Bgr(74, 131, 197)));
    assert(!CtrDayNight_Glass(Bgr(49, 98, 164)) && !CtrDayNight_Glass(Bgr(230, 222, 164))
           && !CtrDayNight_Glass(Bgr(246, 246, 255)) && !CtrDayNight_Glass(Bgr(115, 205, 164)));
    /* Lit glass: at night it is the lamplight, by day the plain colour. */
    assert(CtrDayNight_Lit(Bgr(156, 213, 255), night, 256) == CTR_DAY_NIGHT_LAMP);
    assert(CtrDayNight_Lit(Bgr(156, 213, 255), 0xffffffu, 0) == Bgr(156, 213, 255));

    TABLE(kCtrLights_General, 1);
    TABLE(kCtrLights_PalletTown, 0);
    TABLE(kCtrLights_ViridianCity, 0);
    TABLE(kCtrLights_PewterCity, 0);
    TABLE(kCtrLights_VermilionCity, 0);
    TABLE(kCtrLights_LavenderTown, 0);
    TABLE(kCtrLights_CeladonCity, 0);
    TABLE(kCtrLights_FuchsiaCity, 0);
    TABLE(kCtrLights_SaffronCity, 0);
    TABLE(kCtrLights_IndigoPlateau, 0);
    TABLE(kCtrLights_SeviiIslands123, 0);
    /* The Poke Mart's roof shares the windows' colours: none of its tiles is listed. */
    static const unsigned martRoof[] = {192, 193, 194, 195, 208, 209, 210, 224, 240, 241, 242, 243, 256, 257, 258, 259, 273, 275};
    for (unsigned i = 0; i < sizeof(martRoof) / sizeof(martRoof[0]); ++i)
        for (unsigned j = 0; j < sizeof(kCtrLights_General) / sizeof(kCtrLights_General[0]); ++j)
            assert((kCtrLights_General[j] & 1023) != martRoof[i]);
    return 0;
}
