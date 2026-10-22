/* LeafGreen's version exclusives in FireRed's wild encounters (see include/3ds_leafgreen.h). */
#include "constants/species.h"
#include "3ds_leafgreen.h"

unsigned CtrLeafGreen_Counterpart(unsigned species)
{
    switch (species)
    {
    case SPECIES_EKANS:      return SPECIES_SANDSHREW;
    case SPECIES_ARBOK:      return SPECIES_SANDSLASH;
    case SPECIES_ODDISH:     return SPECIES_BELLSPROUT;
    case SPECIES_GLOOM:      return SPECIES_WEEPINBELL;
    case SPECIES_PSYDUCK:    return SPECIES_SLOWPOKE;
    case SPECIES_GOLDUCK:    return SPECIES_SLOWBRO;
    case SPECIES_GROWLITHE:  return SPECIES_VULPIX;
    case SPECIES_SHELLDER:   return SPECIES_STARYU;
    case SPECIES_SCYTHER:    return SPECIES_PINSIR;
    case SPECIES_SEADRA:     return SPECIES_KINGLER;
    case SPECIES_WEEZING:    return SPECIES_MUK;
    case SPECIES_WOOPER:     return SPECIES_MARILL;
    case SPECIES_MURKROW:    return SPECIES_MISDREAVUS;
    case SPECIES_QWILFISH:   return SPECIES_REMORAID;
    case SPECIES_DELIBIRD:   return SPECIES_SNEASEL;
    default:                 return species;
    }
}

int CtrLeafGreen_Slot(int place, unsigned area, unsigned slot,
                      unsigned *species, unsigned *minLevel, unsigned *maxLevel)
{
    unsigned other;

    if (place == CTR_LG_PLACE_MT_EMBER_EXTERIOR && area == CTR_LG_AREA_LAND
     && (slot == 8 || slot == 10) && *species == SPECIES_SPEAROW)
    {
        *species = SPECIES_MAGMAR;
        *minLevel = *maxLevel = slot == 8 ? 38 : 40;
        return 1;
    }
    if (place == CTR_LG_PLACE_SEVII_DEEP_WATER && area == CTR_LG_AREA_WATER
     && slot == 2 && *species == SPECIES_TENTACOOL)
    {
        *species = SPECIES_MANTINE;
        return 1;
    }
    other = CtrLeafGreen_Counterpart(*species);
    if (other == *species)
        return 0;
    *species = other;
    return 1;
}
