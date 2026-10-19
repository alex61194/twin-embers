#ifndef CTR_TYPE_CHART_H
#define CTR_TYPE_CHART_H

/*
 * The type matchup of a move shown on the battle's move cards. Reads FireRed's own gTypeEffectiveness
 * (battle_main.c): triples of attacker type, defender type and multiplier (0, 5, 10, 20), ending with
 * TYPE_ENDTABLE. The TYPE_FORESIGHT marker splits the table: what follows it (the Ghost immunities) applies
 * unless Foresight is in effect, which the card does not look at. Pure C, tested on the host.
 */
#define CTR_TYPE_FORESIGHT 0xFE
#define CTR_TYPE_ENDTABLE  0xFF

/* What a card says. CTR_FX_NONE is a neutral hit (nothing is drawn) and also "not known". */
enum { CTR_FX_NONE, CTR_FX_IMMUNE, CTR_FX_QUARTER, CTR_FX_HALF, CTR_FX_DOUBLE, CTR_FX_QUADRUPLE };

/* One attacking type against one defending type, in hundredths (0, 50, 100, 200). */
static inline unsigned CtrTypeChart_One(const unsigned char *chart, unsigned atk, unsigned def)
{
    for (unsigned i = 0; chart[i] != CTR_TYPE_ENDTABLE; i += 3)
    {
        if (chart[i] == CTR_TYPE_FORESIGHT)
            continue;
        if (chart[i] == atk && chart[i + 1] == def)
            return chart[i + 2] * 10u;
    }
    return 100;
}

/* Against a defender of one or two types (type2 equal to type1 counts once), as a CTR_FX_*. */
static inline int CtrTypeChart_Effect(const unsigned char *chart, unsigned atk, unsigned type1, unsigned type2)
{
    unsigned m = CtrTypeChart_One(chart, atk, type1);

    if (type2 != type1)
        m = m * CtrTypeChart_One(chart, atk, type2) / 100;
    return m == 0 ? CTR_FX_IMMUNE : m <= 25 ? CTR_FX_QUARTER : m < 100 ? CTR_FX_HALF
         : m >= 400 ? CTR_FX_QUADRUPLE : m > 100 ? CTR_FX_DOUBLE : CTR_FX_NONE;
}

#endif
