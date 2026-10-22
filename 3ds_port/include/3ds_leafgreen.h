#ifndef CTR_LEAFGREEN_H
#define CTR_LEAFGREEN_H

/*
 * LeafGreen's version exclusives in FireRed's own wild encounters (wild_encounter.c). The two games share their
 * encounter tables slot for slot; where LeafGreen puts a different species in a slot, it is almost always the
 * counterpart of FireRed's (Ekans/Sandshrew, Oddish/Bellsprout, Psyduck/Slowpoke...). A slot holding one of those
 * FireRed species gives its LeafGreen counterpart half of the time, at the slot's own level, so both games' species
 * can be caught in the same places. Two LeafGreen slots are not a counterpart of FireRed's and are named by place:
 * Magmar on the Mt. Ember exterior (FireRed's Spearow slots 8 and 10, levels 38 and 40) and Mantine in the deep water
 * of Tanoby Ruins and Trainer Tower (FireRed's Tentacool slot 2). Species that differ only in how often they appear
 * (Krabby/Horsea, the Nidoran, Kakuna/Metapod, Koffing/Grimer, Ledyba/Spinarak) are left as FireRed has them.
 * Pure C over the species constants (src/3ds_leafgreen.c), tested on the host.
 */

/* Where a named LeafGreen slot can be. The caller (wild_encounter.c) turns the current map into one of these. */
enum { CTR_LG_PLACE_OTHER, CTR_LG_PLACE_MT_EMBER_EXTERIOR, CTR_LG_PLACE_SEVII_DEEP_WATER };

/* The encounter areas, in wild_encounter.c's order. */
enum { CTR_LG_AREA_LAND, CTR_LG_AREA_WATER, CTR_LG_AREA_ROCKS, CTR_LG_AREA_FISHING };

/* LeafGreen's counterpart of a FireRed-only wild species, or the species itself when there is none. */
unsigned CtrLeafGreen_Counterpart(unsigned species);

/*
 * The LeafGreen alternative of one slot. On entry species, minLevel and maxLevel hold FireRed's slot; returns 1 and
 * rewrites them when LeafGreen has another species there, 0 (untouched) when the slot is the same in both games.
 */
int CtrLeafGreen_Slot(int place, unsigned area, unsigned slot, unsigned *species, unsigned *minLevel, unsigned *maxLevel);

#endif
