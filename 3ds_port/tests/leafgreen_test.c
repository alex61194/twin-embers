#include <stdio.h>
#include "constants/species.h"
#include "3ds_leafgreen.h"

static int failures;

static void expect(int cond, const char *what)
{
    if (!cond)
    {
        printf("FAIL %s\n", what);
        failures++;
    }
}

static void slot(int place, unsigned area, unsigned index, unsigned species, unsigned lo, unsigned hi,
                 int changes, unsigned want, unsigned wantLo, unsigned wantHi, const char *what)
{
    unsigned s = species, a = lo, b = hi;
    int got = CtrLeafGreen_Slot(place, area, index, &s, &a, &b);

    expect(got == changes && s == want && a == wantLo && b == wantHi, what);
}

int main(void)
{
    slot(CTR_LG_PLACE_OTHER, CTR_LG_AREA_LAND, 3, SPECIES_EKANS, 6, 8, 1, SPECIES_SANDSHREW, 6, 8, "Ekans -> Sandshrew, same level");
    slot(CTR_LG_PLACE_OTHER, CTR_LG_AREA_FISHING, 5, SPECIES_PSYDUCK, 15, 25, 1, SPECIES_SLOWPOKE, 15, 25, "fishing Psyduck -> Slowpoke");
    slot(CTR_LG_PLACE_OTHER, CTR_LG_AREA_LAND, 0, SPECIES_PIDGEY, 2, 5, 0, SPECIES_PIDGEY, 2, 5, "shared species untouched");
    slot(CTR_LG_PLACE_OTHER, CTR_LG_AREA_LAND, 0, SPECIES_KRABBY, 2, 5, 0, SPECIES_KRABBY, 2, 5, "both-version species untouched");
    slot(CTR_LG_PLACE_MT_EMBER_EXTERIOR, CTR_LG_AREA_LAND, 8, SPECIES_SPEAROW, 30, 30, 1, SPECIES_MAGMAR, 38, 38, "Mt. Ember slot 8 -> Magmar 38");
    slot(CTR_LG_PLACE_MT_EMBER_EXTERIOR, CTR_LG_AREA_LAND, 10, SPECIES_SPEAROW, 30, 30, 1, SPECIES_MAGMAR, 40, 40, "Mt. Ember slot 10 -> Magmar 40");
    slot(CTR_LG_PLACE_MT_EMBER_EXTERIOR, CTR_LG_AREA_LAND, 3, SPECIES_SPEAROW, 32, 32, 0, SPECIES_SPEAROW, 32, 32, "other Mt. Ember Spearow stays");
    slot(CTR_LG_PLACE_OTHER, CTR_LG_AREA_LAND, 8, SPECIES_SPEAROW, 30, 30, 0, SPECIES_SPEAROW, 30, 30, "Spearow elsewhere stays");
    slot(CTR_LG_PLACE_SEVII_DEEP_WATER, CTR_LG_AREA_WATER, 2, SPECIES_TENTACOOL, 35, 40, 1, SPECIES_MANTINE, 35, 40, "Tanoby water slot 2 -> Mantine");
    slot(CTR_LG_PLACE_SEVII_DEEP_WATER, CTR_LG_AREA_WATER, 1, SPECIES_TENTACOOL, 20, 35, 0, SPECIES_TENTACOOL, 20, 35, "other Tentacool stays");
    slot(CTR_LG_PLACE_OTHER, CTR_LG_AREA_WATER, 2, SPECIES_TENTACOOL, 35, 40, 0, SPECIES_TENTACOOL, 35, 40, "Tentacool elsewhere stays");
    expect(CtrLeafGreen_Counterpart(SPECIES_DELIBIRD) == SPECIES_SNEASEL, "Delibird -> Sneasel");
    expect(CtrLeafGreen_Counterpart(SPECIES_SANDSHREW) == SPECIES_SANDSHREW, "LeafGreen species map to themselves");
    if (failures)
        return 1;
    puts("leafgreen_test: OK");
    return 0;
}
