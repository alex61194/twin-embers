/* CtrDayNight_Tint: the plain day stays untouched, no hour is darker than
 * the night, and the colour moves smoothly through the day without a jump.
 * The palette side: shading, how lit the windows are, what is glass, and the
 * lights table (3ds_day_night_lights.h) well formed, and the windows' own
 * hours. */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include "3ds_day_night.h"
#include "3ds_day_night_lights.h"

static unsigned Luma(uint16_t c) { return (c & 31) * 77 + ((c >> 5) & 31) * 150 + ((c >> 10) & 31) * 29; }

/* How many of n windows are lit at a second of the day. */
static unsigned LitCount(uint32_t second, unsigned n)
{
    unsigned lit = 0;
    for (unsigned i = 0; i < n; ++i)
        lit += CtrDayNight_WindowOn(CtrDayNight_WindowHash((int)(i % 97) * 2, (int)(i / 97) * 2, 3u << 8 | 1u), second);
    return lit;
}

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
        assert(bank < 13);
        if (general) assert(tile < 640 && bank < 7);
        /* Only General's buildings are open all night, and not its houses. */
        if (table[i] & CTR_LIGHT_PUBLIC) assert(general && tile != 557 && tile != 558);
        for (unsigned j = 0; j < i; ++j)
        {
            assert((table[j] & 0xf3ffu) != (table[i] & 0xf3ffu));
            if (table[j] & CTR_LIGHT_MASKED) ++j;
        }
        /* A masked entry's glass: some colour indices, never the transparent 0. */
        if (table[i] & CTR_LIGHT_MASKED)
        {
            assert(i + 1 < count && table[i + 1] && !(table[i + 1] & 1u));
            ++i;
        }
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

    /* Shading: white leaves every colour as it is, no hour brightens a colour
     * (by its luma; the night's grade may give a yellow a little blue), black
     * stays black, and the night is the moonlit grade. */
    for (uint32_t c = 0; c < 0x8000u; ++c)
    {
        assert(CtrDayNight_Shade((uint16_t)c, 0xffffffu) == c);
        assert(Luma(CtrDayNight_Shade((uint16_t)c, night)) <= Luma((uint16_t)c));
        assert(Luma(CtrDayNight_Shade((uint16_t)c, CtrDayNight_Tint(68400u))) <= Luma((uint16_t)c));
        assert(CtrDayNight_Shade((uint16_t)c, night) == CtrDayNight_Moon((uint16_t)c));
    }
    assert(CtrDayNight_Shade(0x7fff, night) == ((10u) | (13u << 5) | (24u << 10)));
    assert(CtrDayNight_Shade(0, night) == 0);
    /* Dusk stays warm: white keeps more red than blue. */
    {
        uint16_t w = CtrDayNight_Shade(0x7fff, CtrDayNight_Tint(68400u));
        assert((w & 31) > ((w >> 10) & 31));
    }
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

    /* The windows' own hours: none lit at noon or 17:00, all at 21:00 (on by
     * 20:00, the first off at 21:45), fewer and fewer through the night, a
     * few all night and a few early risers. */
    {
        const unsigned n = 9700;
        unsigned at[] = {LitCount(43200u, n), LitCount(61200u, n), LitCount(75600u, n), LitCount(81000u, n),
                         LitCount(84600u, n), LitCount(3600u, n), LitCount(10800u, n), LitCount(21600u, n)};
        assert(at[0] == 0 && at[1] == 0 && at[2] == n);
        assert(at[3] < n && at[4] < at[3] && at[5] < at[4] && at[6] <= at[5]);
        assert(at[3] > n / 2 && at[6] > n / 20 && at[6] < n / 5);
        assert(at[7] > at[6]);
        /* 18:30 to 20:00: they come on one by one. */
        assert(LitCount(66600u, n) < LitCount(68400u, n) && LitCount(68400u, n) < LitCount(72000u, n));
        /* The hours are a window's own: the same place, the same answer; another place, another hash. */
        assert(CtrDayNight_WindowHash(10, 20, 5) == CtrDayNight_WindowHash(10, 20, 5));
        assert(CtrDayNight_WindowHash(10, 20, 5) != CtrDayNight_WindowHash(12, 20, 5)
               && CtrDayNight_WindowHash(10, 20, 5) != CtrDayNight_WindowHash(10, 20, 6));
    }

    TABLE(kCtrLights_General, 1);
    /* The Pokemon Center's windows are lit all night, the houses' are not. */
    assert(kCtrLights_General[0] == (384 | CTR_LIGHT_PUBLIC | 2 << 12));
    TABLE(kCtrLights_PalletTown, 0);
    TABLE(kCtrLights_ViridianCity, 0);
    TABLE(kCtrLights_PewterCity, 0);
    TABLE(kCtrLights_VermilionCity, 0);
    TABLE(kCtrLights_LavenderTown, 0);
    TABLE(kCtrLights_CeladonCity, 0);
    TABLE(kCtrLights_FuchsiaCity, 0);
    TABLE(kCtrLights_SaffronCity, 0);
    TABLE(kCtrLights_IndigoPlateau, 0);
    TABLE(kCtrLights_CinnabarIsland, 0);
    TABLE(kCtrLights_SeviiIslands123, 0);
    TABLE(kCtrLights_SeviiIslands45, 0);
    TABLE(kCtrLights_SeviiIslands67, 0);
    /* The Pokemon Tower's dark panes are lit by their index, with the houses' hours. */
    assert(kCtrLights_LavenderTown[19] == (722 | CTR_LIGHT_MASKED | 9 << 12) && kCtrLights_LavenderTown[20] == 1u << 6);
    /* The Poke Mart's roof shares the windows' colours: none of its tiles is listed. */
    static const unsigned martRoof[] = {192, 193, 194, 195, 208, 209, 210, 224, 240, 241, 242, 243, 256, 257, 258, 259, 273, 275};
    for (unsigned i = 0; i < sizeof(martRoof) / sizeof(martRoof[0]); ++i)
        for (unsigned j = 0; j < sizeof(kCtrLights_General) / sizeof(kCtrLights_General[0]); ++j)
            assert((kCtrLights_General[j] & 1023) != martRoof[i]);
    return 0;
}
