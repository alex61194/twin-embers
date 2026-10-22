#include <stdio.h>
#include "constants/maps.h"
#include "constants/species.h"
#include "3ds_night_wild.h"

static int failures;

static void expect(unsigned got, unsigned want, const char *what)
{
    if (got != want)
    {
        printf("FAIL %s: %u, expected %u\n", what, got, want);
        failures++;
    }
}

int main(void)
{
    expect(CtrNightWild_IsNight(21 * 3600), 1, "21:00 is night");
    expect(CtrNightWild_IsNight(4 * 3600 + 3599), 1, "04:59 is night");
    expect(CtrNightWild_IsNight(5 * 3600), 0, "05:00 is day");
    expect(CtrNightWild_IsNight(20 * 3600 + 3599), 0, "20:59 is day");

    expect(CtrNightWild_Species(MAP_ROUTE1, CTR_WILD_LAND, 0, SPECIES_PIDGEY, true, true), SPECIES_HOOTHOOT, "Route 1 night, National");
    expect(CtrNightWild_Species(MAP_ROUTE1, CTR_WILD_LAND, 0, SPECIES_PIDGEY, true, false), SPECIES_ZUBAT, "Route 1 night, Kanto");
    expect(CtrNightWild_Species(MAP_ROUTE1, CTR_WILD_LAND, 0, SPECIES_PIDGEY, false, true), SPECIES_PIDGEY, "Route 1 by day");
    expect(CtrNightWild_Species(MAP_ROUTE1, CTR_WILD_LAND, 1, SPECIES_RATTATA, true, true), SPECIES_RATTATA, "Rattata stays");
    expect(CtrNightWild_Species(MAP_ROUTE2, CTR_WILD_LAND, 0, SPECIES_PIDGEY, true, true), SPECIES_HOOTHOOT, "Route 2");
    expect(CtrNightWild_Species(MAP_ROUTE7, CTR_WILD_LAND, 9, SPECIES_GROWLITHE, true, false), SPECIES_GROWLITHE, "Kanto rule with no species");
    expect(CtrNightWild_Species(MAP_ROUTE7, CTR_WILD_LAND, 9, SPECIES_GROWLITHE, true, true), SPECIES_HOUNDOUR, "Houndour with National");
    expect(CtrNightWild_Species(MAP_MT_MOON_1F, CTR_WILD_LAND, 2, SPECIES_GEODUDE, true, false), SPECIES_CLEFAIRY, "Mt. Moon slot 2");
    expect(CtrNightWild_Species(MAP_MT_MOON_1F, CTR_WILD_LAND, 5, SPECIES_GEODUDE, true, false), SPECIES_GEODUDE, "other Geodude stays");
    expect(CtrNightWild_Species(MAP_ROUTE1, CTR_WILD_WATER, 0, SPECIES_PIDGEY, true, true), SPECIES_PIDGEY, "area must match");
    expect(CtrNightWild_Species(MAP_SIX_ISLAND_PATTERN_BUSH, CTR_WILD_LAND, 0, SPECIES_SPINARAK, false, true), SPECIES_LEDYBA, "Pattern Bush by day");
    expect(CtrNightWild_Species(MAP_SIX_ISLAND_PATTERN_BUSH, CTR_WILD_LAND, 0, SPECIES_SPINARAK, true, true), SPECIES_SPINARAK, "Pattern Bush Spinarak at night");
    expect(CtrNightWild_Species(MAP_SIX_ISLAND_PATTERN_BUSH, CTR_WILD_LAND, 6, SPECIES_LEDYBA, true, true), SPECIES_SPINARAK, "Pattern Bush Ledyba at night");
    expect(CtrNightWild_Species(MAP_FIVE_ISLAND, CTR_WILD_FISHING, 7, SPECIES_GYARADOS, true, true), SPECIES_CHINCHOU, "Super Rod Chinchou");
    expect(CtrNightWild_Species(MAP_FIVE_ISLAND, CTR_WILD_FISHING, 8, SPECIES_SEADRA, true, true), SPECIES_LANTURN, "Super Rod Lanturn");
    expect(CtrNightWild_Species(MAP_FIVE_ISLAND_LOST_CAVE_ROOM14, CTR_WILD_LAND, 2, SPECIES_ZUBAT, true, true), SPECIES_MISDREAVUS, "Lost Cave");
    if (failures)
        return 1;
    puts("night_wild_test: OK");
    return 0;
}
