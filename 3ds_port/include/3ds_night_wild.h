#ifndef CTR_NIGHT_WILD_H
#define CTR_NIGHT_WILD_H

#include <stdbool.h>

/*
 * Night encounters: from 21:00 to 05:00 on the 3DS clock (the night of 3ds_day_night.h), some of a map's wild slots
 * give another species, always and at the slot's own level (wild_encounter.c calls this before the LeafGreen half,
 * 3ds_leafgreen.h). The day birds and bugs give way to night ones: Pidgey to Hoothoot, Caterpie to Spinarak, Spearow
 * to Murkrow, more Clefairy in Mt. Moon, Haunter in the Pokemon Tower, Gastly in the Mansion. Johto species wait for the
 * National Pokedex, as in the Sevii Islands; before it a rule gives its Kanto species, or nothing. One rule is the other
 * way round: in Six Island's Pattern Bush, Ledyba by day and Spinarak by night. The table (src/3ds_night_wild.c) is
 * port-authored; FireRed's encounter tables still come from the pack. Pure C over the map and species constants,
 * tested on the host.
 */

/* The encounter areas, in wild_encounter.c's order (as CTR_LG_AREA_*). */
enum { CTR_WILD_LAND, CTR_WILD_WATER, CTR_WILD_ROCKS, CTR_WILD_FISHING };
/* When a rule applies. */
enum { CTR_WILD_NIGHT, CTR_WILD_DAY };

#define CTR_NIGHT_WILD_START (21u * 3600u)   /* seconds of the day */
#define CTR_NIGHT_WILD_END   (5u * 3600u)

/* Whether a second of the day (0-86399) is night for the encounters. */
static inline bool CtrNightWild_IsNight(unsigned second)
{
    return second >= CTR_NIGHT_WILD_START || second < CTR_NIGHT_WILD_END;
}

/*
 * The species a slot gives at this time of day: `map` is a MAP_* constant (group << 8 | number), `area` a
 * CTR_WILD_*, `slot` the slot index and `species` FireRed's species there. Returns `species` itself when no rule
 * applies.
 */
unsigned CtrNightWild_Species(unsigned map, unsigned area, unsigned slot, unsigned species, bool night, bool national);

#ifndef CTR_HOST_TEST
/* The 3DS clock's second of the day. */
unsigned CtrNightWild_Second(void);
#endif

#endif
