/* The move cards' type matchup (3ds_type_chart.h) on a table laid out like FireRed's gTypeEffectiveness. */
#include <stdio.h>

#include "3ds_type_chart.h"

enum { NORMAL = 0, FIGHTING = 1, FLYING = 2, GROUND = 4, ROCK = 5, GHOST = 7, FIRE = 10, WATER = 11, GRASS = 12,
       ELECTRIC = 13, ICE = 15, DRAGON = 16, STEEL = 8 };

static const unsigned char kChart[] = {
    FIRE, GRASS, 20, FIRE, ICE, 20, FIRE, WATER, 5, FIRE, ROCK, 5, FIRE, FIRE, 5,
    WATER, FIRE, 20, WATER, GROUND, 20, WATER, ROCK, 20, WATER, GRASS, 5,
    ELECTRIC, WATER, 20, ELECTRIC, FLYING, 20, ELECTRIC, GROUND, 0,
    GROUND, FLYING, 0, GROUND, FIRE, 20, GROUND, GRASS, 5,
    ICE, DRAGON, 20, ICE, GRASS, 20, ICE, GROUND, 20, ICE, FLYING, 20,
    CTR_TYPE_FORESIGHT, CTR_TYPE_FORESIGHT, 0,
    NORMAL, GHOST, 0, FIGHTING, GHOST, 0, NORMAL, ROCK, 5, FIGHTING, NORMAL, 20,
    CTR_TYPE_ENDTABLE, CTR_TYPE_ENDTABLE, 0,
};

static int failures;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); failures++; } } while (0)
#define EFFECT(atk, t1, t2) CtrTypeChart_Effect(kChart, atk, t1, t2)

int main(void)
{
    CHECK(EFFECT(FIRE, GRASS, GRASS) == CTR_FX_DOUBLE, "fire on grass");
    CHECK(EFFECT(FIRE, WATER, WATER) == CTR_FX_HALF, "fire on water");
    CHECK(EFFECT(WATER, FIRE, FIRE) == CTR_FX_DOUBLE, "a single type counts once");
    CHECK(EFFECT(FIRE, GRASS, ICE) == CTR_FX_QUADRUPLE, "fire on grass and ice");
    CHECK(EFFECT(FIRE, WATER, ROCK) == CTR_FX_QUARTER, "fire on water and rock");
    CHECK(EFFECT(FIRE, GRASS, WATER) == CTR_FX_NONE, "x2 and x1/2 make a neutral hit");
    CHECK(EFFECT(ELECTRIC, WATER, GROUND) == CTR_FX_IMMUNE, "one immune type decides");
    CHECK(EFFECT(GROUND, FLYING, GROUND) == CTR_FX_IMMUNE, "ground on flying");
    CHECK(EFFECT(NORMAL, STEEL, STEEL) == CTR_FX_NONE, "a pair the table lacks is neutral");
    CHECK(EFFECT(NORMAL, GHOST, GHOST) == CTR_FX_IMMUNE, "the entries after the Foresight marker apply");
    CHECK(EFFECT(FIGHTING, GHOST, NORMAL) == CTR_FX_IMMUNE, "fighting on ghost normal");
    CHECK(EFFECT(FIGHTING, NORMAL, ROCK) == CTR_FX_DOUBLE, "fighting on normal rock (rock is not in the table: neutral)");
    CHECK(EFFECT(NORMAL, ROCK, ROCK) == CTR_FX_HALF, "normal on rock");
    CHECK(EFFECT(WATER, GRASS, GROUND) == CTR_FX_NONE, "x1/2 and x2 cancel");
    CHECK(EFFECT(ICE, DRAGON, FLYING) == CTR_FX_QUADRUPLE, "ice on dragon flying");

    if (failures) { printf("%d failures\n", failures); return 1; }
    puts("PASS type chart");
    return 0;
}
