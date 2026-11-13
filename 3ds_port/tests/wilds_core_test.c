#define _POSIX_C_SOURCE 199309L
/* The rules of the visible wild Pokemon (3ds_wilds_core.c), run against a small fake world: a tile grid, FireRed's 16 object
 * events, a lead Pokemon. The game's glue (recipes 0249 and 0250) does what this test's fake world does, so what is checked here is
 * the logic the game runs. Ends with host micro-benchmarks of the pure logic (a host's time, not a console's). */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "3ds_wilds_core.h"

static int failures, checks;
#define CHECK(c, ...) do { ++checks; if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); failures++; } } while (0)

/* The fake world --------------------------------------------------------------------------------------------------------- */

#define W 80
#define H 60
typedef struct World
{
    uint8_t tile[H][W];             /* CTRW_TILE_NONE, LAND or WATER */
    int objActive[CTRW_OBJECTS];    /* 1: the object event is in use */
    int objX[CTRW_OBJECTS], objY[CTRW_OBJECTS];
    int npcWanted, npcMade, npcFailed;
    int px, py;
    int scans;                      /* how many times the free object events were counted */
} World;

static int FreeObjects(void *ctx)
{
    World *w = ctx;
    int n = 0;

    ++w->scans;
    for (int i = 0; i < CTRW_OBJECTS; ++i)
        n += !w->objActive[i];
    return n;
}

static int AtObject(const World *w, int x, int y)
{
    for (int i = 0; i < CTRW_OBJECTS; ++i)
        if (w->objActive[i] && w->objX[i] == x && w->objY[i] == y)
            return 1;
    return 0;
}

static int TileFn(void *ctx, int x, int y)
{
    World *w = ctx;

    if (x < 0 || y < 0 || x >= W || y >= H || AtObject(w, x, y))
        return CTRW_TILE_NONE;
    return w->tile[y][x];
}

/* The engine making an NPC (scrolling in): it takes a free object event, or fails and the NPC is missing. */
static int MakeObject(World *w, int x, int y)
{
    for (int i = 0; i < CTRW_OBJECTS; ++i)
        if (!w->objActive[i])
        {
            w->objActive[i] = 1;
            w->objX[i] = x;
            w->objY[i] = y;
            return i;
        }
    return -1;
}

static void MakeNpcs(World *w, int n)
{
    for (int i = 0; i < n; ++i)
    {
        w->npcWanted++;
        if (MakeObject(w, 1 + i, 1) >= 0)
            w->npcMade++;
        else
            w->npcFailed++;
    }
}

static void WorldInit(World *w, int tile)
{
    memset(w, 0, sizeof(*w));
    memset(w->tile, tile, sizeof(w->tile));
    w->px = W / 2;
    w->py = H / 2;
    MakeObject(w, w->px, w->py);        /* the player */
}

static CtrWildRing Ring(const World *w)
{
    CtrWildRing r = {w->px, w->py, 8, 6, w->px - 11, w->px + 12, w->py, w->py + 10};

    return r;
}

static int sRollFails;      /* the fake game species roll fails (a land table with no Pokemon that has a picture) */

/* The glue's spawn, as the game does it: a due attempt picks a cell, then a Pokemon, then makes the object event. */
static int Spawn(CtrWildsCore *c, World *w, int consoleCap, int level, uint32_t power, int roamer)
{
    CtrWildRing ring = Ring(w);
    int x, y, kind, slot, obj;
    CtrWildMon mon;

    (void)consoleCap;
    if (!CtrWildsCore_SpawnDue(c))
        return 0;
    slot = CtrWildsCore_FreeSlot(c);
    if (slot == CTRW_NONE || !CtrWildsCore_PickCell(c, &ring, TileFn, w, c->mapLand, c->mapWater, 0, &x, &y, &kind))
    {
        CtrWildsCore_NoteFail(c, CTRW_FAIL_NOCELL);
        CtrWildsCore_SpawnDone(c, 0);
        return 0;
    }
    if (sRollFails)
    {
        CtrWildsCore_NoteFail(c, CTRW_FAIL_ROLL);
        CtrWildsCore_SpawnDone(c, 0);
        return 0;
    }
    memset(&mon, 0, sizeof(mon));
    mon.species = 16;
    mon.level = (uint8_t)level;
    mon.water = kind == CTRW_TILE_WATER;
    mon.roamer = (uint8_t)roamer;
    mon.power = power;
    mon.personality = CtrWildsCore_Personality(c, 7);
    obj = MakeObject(w, x, y);
    if (obj < 0)
    {
        CtrWildsCore_NoteFail(c, CTRW_FAIL_NOOBJ);
        CtrWildsCore_SpawnDone(c, 0);
        return 0;
    }
    CtrWildsCore_Commit(c, slot, &mon, 0, 0, 1);
    CtrWildsCore_SetObject(c, slot, obj);
    CtrWildsCore_SpawnDone(c, 1);
    return 1;
}

static void Shed(CtrWildsCore *c, World *w, int n)
{
    while (n-- > 0)
    {
        int slot = CtrWildsCore_FarthestSlot(c);

        if (slot == CTRW_NONE)
            return;
        if (c->wild[slot].obj != CTRW_NONE)
            w->objActive[c->wild[slot].obj] = 0;
        CtrWildsCore_Forget(c, slot);
    }
}

/* One frame of the glue: the frame setting, the periodic check, the spawn. */
static void Frame(CtrWildsCore *c, World *w, int density, int consoleCap, int blocked)
{
    CtrWildEnv env = {density, c->aggro, consoleCap, blocked, c->leadPower};
    int shed;

    if (CtrWildsCore_Frame(c, &env))
    {
        Shed(c, w, c->active);
        return;
    }
    if (!c->operative)
        return;
    shed = CtrWildsCore_Tick(c, density, consoleCap, FreeObjects, w);
    if (shed > 0)
        Shed(c, w, shed);
    Spawn(c, w, consoleCap, 10, 100, 0);
}

static void Run(CtrWildsCore *c, World *w, int density, int consoleCap, int frames)
{
    for (int i = 0; i < frames; ++i)
        Frame(c, w, density, consoleCap, 0);
}

static void MapOn(CtrWildsCore *c, uint16_t map, int land, int water)
{
    CtrWildsCore_MapChanged(c, map);
    c->mapLand = (uint8_t)land;
    c->mapWater = (uint8_t)water;
}

/* Tests ------------------------------------------------------------------------------------------------------------------ */

static void TestSettings(void)
{
    CHECK(CtrWildsCore_DensityCap(0) == 0 && CtrWildsCore_DensityCap(1) == 2 && CtrWildsCore_DensityCap(2) == 4
          && CtrWildsCore_DensityCap(3) == 6 && CtrWildsCore_DensityCap(4) == 0, "density caps");
    CHECK(CtrWildsCore_AggroPercent(0) == 0 && CtrWildsCore_AggroPercent(1) == 10 && CtrWildsCore_AggroPercent(4) == 40
          && CtrWildsCore_AggroPercent(5) == 0 && CtrWildsCore_AggroPercent(-1) == 0, "aggression percents");
    /* An old file's visiblewilds=1 (on) is NORMAL; with the new keys it is FEW; the rest is clamped. */
    CHECK(CtrWildsCore_MigrateDensity(1, 0, 0) == 2, "old on becomes NORMAL");
    CHECK(CtrWildsCore_MigrateDensity(1, 1, 0) == 1 && CtrWildsCore_MigrateDensity(1, 0, 1) == 1, "FEW with the new keys");
    CHECK(CtrWildsCore_MigrateDensity(0, 0, 0) == 0 && CtrWildsCore_MigrateDensity(2, 0, 0) == 2 && CtrWildsCore_MigrateDensity(3, 0, 0) == 3,
          "other values kept");
    CHECK(CtrWildsCore_MigrateDensity(4, 1, 1) == 0 && CtrWildsCore_MigrateDensity(-3, 1, 1) == 0 && CtrWildsCore_MigrateDensity(99, 0, 0) == 0,
          "unknown values are OFF");
    CHECK(CtrWildsCore_ClampAggro(4) == 4 && CtrWildsCore_ClampAggro(5) == 0 && CtrWildsCore_ClampAggro(-1) == 0, "aggression clamp");
}

static void TestBudget(void)
{
    /* density and console cap */
    CHECK(CtrWildsCore_Target(0, 6, 16, 0) == 0, "OFF: none");
    CHECK(CtrWildsCore_Target(1, 6, 16, 0) == 2 && CtrWildsCore_Target(2, 6, 16, 0) == 4 && CtrWildsCore_Target(3, 6, 16, 0) == 6, "New 3DS densities");
    CHECK(CtrWildsCore_Target(3, 2, 16, 0) == 2 && CtrWildsCore_Target(2, 2, 16, 0) == 2 && CtrWildsCore_Target(1, 2, 16, 0) == 2, "Old 3DS: at most 2 for every density");
    CHECK(CtrWildsCore_Target(3, 99, 16, 0) == CTRW_SLOTS, "never above the table");
    /* the free object events: 4 stay free */
    CHECK(CtrWildsCore_Target(3, 6, 10, 0) == 6, "10 free: room for 6");
    CHECK(CtrWildsCore_Target(3, 6, 8, 0) == 4, "8 free: room for 4");
    CHECK(CtrWildsCore_Target(3, 6, 5, 0) == 1, "5 free: room for 1");
    CHECK(CtrWildsCore_Target(3, 6, 4, 0) == 0, "4 free: none (the reserve)");
    CHECK(CtrWildsCore_Target(3, 6, 0, 0) == 0, "none free: none");
    CHECK(CtrWildsCore_Target(3, 6, 4, 3) == 3, "wilds already out keep their places while the reserve holds");
    CHECK(CtrWildsCore_ShedCount(4, 3) == 0 && CtrWildsCore_ShedCount(3, 3) == 1 && CtrWildsCore_ShedCount(0, 3) == 3 && CtrWildsCore_ShedCount(0, 1) == 1
          && CtrWildsCore_ShedCount(2, 0) == 0, "shedding brings the reserve back, no more than are out");
}

static void TestObjectLimits(void)
{
    CtrWildsCore c;
    World w;

    /* a map with 12 NPCs (and the player): 3 free, below the reserve: nothing appears and the original encounters stay */
    CtrWildsCore_Init(&c, 1);
    WorldInit(&w, CTRW_TILE_LAND);
    MakeNpcs(&w, 12);
    MapOn(&c, 1, 1, 0);
    Run(&c, &w, 3, 6, 2000);
    CHECK(c.active == 0, "almost full: no wild appears");
    CHECK(FreeObjects(&w) == 3, "almost full: the free object events are untouched");
    CHECK(!CtrWildsCore_SuppressStep(&c, CTRW_TILE_LAND), "almost full: the step encounters stay");

    /* 10 NPCs: 5 free, room for one wild, and the free ones never fall under 4 */
    CtrWildsCore_Init(&c, 2);
    WorldInit(&w, CTRW_TILE_LAND);
    MakeNpcs(&w, 10);
    MapOn(&c, 1, 1, 0);
    for (int i = 0; i < 6000; ++i)
    {
        Frame(&c, &w, 3, 6, 0);
        CHECK(FreeObjects(&w) >= CTRW_RESERVE, "free object events %d under the reserve", FreeObjects(&w));
    }
    CHECK(c.active == 1, "5 free: exactly one wild, got %d", c.active);
    CHECK(w.npcFailed == 0, "no NPC was displaced");

    /* the engine scrolls NPCs in, 1 then 1: they find free object events while a wild is out; the wilds give way to what the engine needs */
    CtrWildsCore_Init(&c, 3);
    WorldInit(&w, CTRW_TILE_LAND);
    MapOn(&c, 1, 1, 0);
    Run(&c, &w, 3, 6, 3000);
    CHECK(c.active == 6, "an empty map: six wilds, got %d", c.active);
    CHECK(FreeObjects(&w) == CTRW_OBJECTS - 1 - 6, "six wilds and the player use seven");
    MakeNpcs(&w, 4);                                    /* four NPCs scroll in: 9 free before, 5 after */
    CHECK(w.npcFailed == 0, "four NPCs still find a place");
    Run(&c, &w, 3, 6, CTRW_TICK + 2);
    CHECK(FreeObjects(&w) >= CTRW_RESERVE, "after a check the reserve is back: %d free", FreeObjects(&w));
    CHECK(c.active == 6 && FreeObjects(&w) == 5, "5 free is above the reserve: nothing is shed");
    MakeNpcs(&w, 5);                                    /* an extreme scroll: the engine may find the table short, a wild never takes the last 4 */
    Run(&c, &w, 3, 6, CTRW_TICK + 2);
    CHECK(FreeObjects(&w) >= CTRW_RESERVE || c.active == 0, "the reserve is restored by shedding wilds");
    /* a shed leaves its object event free and the table consistent */
    {
        int used = 0;

        for (int i = 0; i < CTRW_SLOTS; ++i)
            used += c.wild[i].state != CTRW_S_FREE;
        CHECK(used == c.active, "the table's count matches its slots");
    }

    /* the cap on an Old 3DS holds on an empty map */
    CtrWildsCore_Init(&c, 4);
    WorldInit(&w, CTRW_TILE_LAND);
    MapOn(&c, 1, 1, 0);
    Run(&c, &w, 3, 2, 4000);
    CHECK(c.active == 2, "Old 3DS: two wilds at most, got %d", c.active);
    /* lowering the density sheds the extra wilds */
    CtrWildsCore_Init(&c, 5);
    WorldInit(&w, CTRW_TILE_LAND);
    MapOn(&c, 1, 1, 0);
    Run(&c, &w, 3, 6, 4000);
    CHECK(c.active == 6, "six out");
    Run(&c, &w, 1, 6, CTRW_TICK * 4);
    CHECK(c.active == 2, "FEW: down to two, got %d", c.active);
    Run(&c, &w, 0, 6, 2);
    CHECK(c.active == 0, "OFF: all removed");
    CHECK(FreeObjects(&w) == CTRW_OBJECTS - 1, "OFF: every object event is back");
}

static void TestSpawn(void)
{
    CtrWildsCore c;
    World w;
    int first = -1, appeared = 0;

    CtrWildsCore_Init(&c, 11);
    WorldInit(&w, CTRW_TILE_LAND);
    MapOn(&c, 5, 1, 0);
    for (int i = 0; i < 6000; ++i)
    {
        int before = c.active;

        Frame(&c, &w, 2, 6, 0);
        if (c.active > before)
        {
            ++appeared;
            if (first < 0)
                first = i;
        }
    }
    CHECK(c.active == 4, "NORMAL on a New 3DS: four wilds, got %d", c.active);
    CHECK(first >= CTRW_FIRST_MIN && first < CTRW_FIRST_MIN + CTRW_FIRST_SPAN, "the first wild comes after 24 to 36 frames (frame %d)", first);
    CHECK(appeared == 4, "each appeared once (%d)", appeared);
    /* the wilds stand in the ring outside the screen */
    for (int i = 0; i < CTRW_SLOTS; ++i)
        if (c.wild[i].state != CTRW_S_FREE)
        {
            int ox = w.objX[c.wild[i].obj], oy = w.objY[c.wild[i].obj];

            CHECK(ox < w.px - 8 || ox > w.px + 8 || oy > w.py + 6, "wild %d spawned inside the screen (%d,%d)", i, ox, oy);
            CHECK(ox >= w.px - 11 && ox <= w.px + 12 && oy >= w.py && oy <= w.py + 10, "wild %d spawned outside the object event view", i);
        }
    /* the rate: spawns come 46 to 151 frames apart (a failed attempt tries again sooner): 45 to 90 while fewer than half of the target
     * are out, 90 to 150 after (the gap is the wait plus the frame of the attempt) */
    {
        CtrWildsCore c2;
        World w2;
        int last = -1, minGap = 100000, maxGap = 0, n = 0;

        CtrWildsCore_Init(&c2, 12);
        WorldInit(&w2, CTRW_TILE_LAND);
        MapOn(&c2, 5, 1, 0);
        for (int i = 0; i < 20000; ++i)
        {
            int before = c2.active;

            Frame(&c2, &w2, 3, 6, 0);
            if (c2.active > before)
            {
                if (last >= 0)
                {
                    minGap = i - last < minGap ? i - last : minGap;
                    maxGap = i - last > maxGap ? i - last : maxGap;
                }
                last = i;
                ++n;
            }
        }
        CHECK(n == 6 && minGap >= CTRW_FILL_MIN + 1 && maxGap <= CTRW_TOPUP_MIN + CTRW_TOPUP_SPAN, "spawn rate: %d wilds, gaps %d..%d", n, minGap, maxGap);
    }
    /* below the target only: at the target no attempt is made */
    {
        int scans = w.scans;

        Run(&c, &w, 2, 6, 600);
        CHECK(c.active == 4, "stays at the target");
        CHECK(w.scans - scans <= 600 / CTRW_TICK + 1, "object events are counted once per tick at most (%d scans in 600 frames)", w.scans - scans);
    }
    /* the whole table is reset by a map change */
    {
        int removed = CtrWildsCore_MapChanged(&c, 6);

        CHECK(removed == 4 && c.active == 0, "map change: the table is cleared (%d)", removed);
        for (int i = 0; i < CTRW_SLOTS; ++i)
            CHECK(c.wild[i].state == CTRW_S_FREE && c.wild[i].obj == CTRW_NONE, "slot %d is empty with no object id left", i);
        CHECK(CtrWildsCore_MapChanged(&c, 6) == -1, "the same map again is no change");
        CHECK(!c.operative && !c.spawnable && c.mapLand == 0, "the new map has to be probed again");
    }
}

static void TestMapsAndWater(void)
{
    CtrWildsCore c;
    World w;
    int land = 0, water = 0;

    /* acceptance by kind */
    CtrWildsCore_Init(&c, 21);
    CHECK(CtrWildsCore_AcceptKind(&c, CTRW_TILE_LAND, 1, 0, 0), "land on a land map");
    CHECK(!CtrWildsCore_AcceptKind(&c, CTRW_TILE_LAND, 0, 1, 0), "no land table: no land wild");
    CHECK(!CtrWildsCore_AcceptKind(&c, CTRW_TILE_LAND, 1, 1, 1), "surfing: no land wild");
    CHECK(CtrWildsCore_AcceptKind(&c, CTRW_TILE_WATER, 0, 1, 0) && CtrWildsCore_AcceptKind(&c, CTRW_TILE_WATER, 1, 1, 1), "water: only map, or surfing");
    CHECK(!CtrWildsCore_AcceptKind(&c, CTRW_TILE_WATER, 1, 0, 1), "no water table: no water wild");
    CHECK(!CtrWildsCore_AcceptKind(&c, CTRW_TILE_NONE, 1, 1, 0), "nothing is not a kind");
    {
        int yes = 0;

        for (int i = 0; i < 3000; ++i)
            yes += CtrWildsCore_AcceptKind(&c, CTRW_TILE_WATER, 1, 1, 0);
        CHECK(yes > 800 && yes < 1200, "on foot with land too, a third of the water cells (%d of 3000)", yes);
    }
    /* a map that is half grass, half water: both kinds appear, water ones on water cells only */
    CtrWildsCore_Init(&c, 22);
    WorldInit(&w, CTRW_TILE_LAND);
    for (int y = 0; y < H; ++y)
        for (int x = W / 2; x < W; ++x)
            w.tile[y][x] = CTRW_TILE_WATER;
    MapOn(&c, 7, 1, 1);
    for (int round = 0; round < 40; ++round)
    {
        Run(&c, &w, 3, 6, 600);
        for (int i = 0; i < CTRW_SLOTS; ++i)
            if (c.wild[i].state != CTRW_S_FREE)
            {
                int tile = w.tile[w.objY[c.wild[i].obj]][w.objX[c.wild[i].obj]];

                CHECK((c.wild[i].water ? CTRW_TILE_WATER : CTRW_TILE_LAND) == tile, "a wild's kind follows its tile");
                if (c.wild[i].water)
                    ++water;
                else
                    ++land;
            }
        MapOn(&c, (uint16_t)(100 + round), 1, 1);
        for (int o = 1; o < CTRW_OBJECTS; ++o)
            w.objActive[o] = 0;
    }
    CHECK(land > 0 && water > 0, "both land and water wilds appeared (%d, %d)", land, water);
    /* a land-only map never makes a water wild; a water-only map never a land one */
    CtrWildsCore_Init(&c, 23);
    WorldInit(&w, CTRW_TILE_WATER);
    MapOn(&c, 8, 0, 1);
    Run(&c, &w, 2, 6, 3000);
    CHECK(c.active == 4, "a water map fills with water wilds");
    for (int i = 0; i < CTRW_SLOTS; ++i)
        CHECK(c.wild[i].state == CTRW_S_FREE || c.wild[i].water, "water wild");
    CtrWildsCore_Init(&c, 24);
    WorldInit(&w, CTRW_TILE_WATER);
    MapOn(&c, 8, 1, 0);
    Run(&c, &w, 2, 6, 3000);
    CHECK(c.active == 0, "a land table on water cells makes nothing");
}

static void TestSuppression(void)
{
    CtrWildsCore c;
    World w;
    CtrWildEnv env = {2, 2, 6, 0, 0};

    CtrWildsCore_Init(&c, 31);
    /* off: never */
    MapOn(&c, 9, 1, 1);
    env.density = 0;
    CtrWildsCore_Frame(&c, &env);
    CHECK(!CtrWildsCore_SuppressStep(&c, CTRW_TILE_LAND), "option off: the step encounters are the game's");
    /* on, the map is supported, but the first tick has not run: spawnable is not known yet, so nothing is hidden */
    env.density = 2;
    CtrWildsCore_Frame(&c, &env);
    CHECK(c.operative && !CtrWildsCore_SuppressStep(&c, CTRW_TILE_LAND), "before the first check the encounters stay");
    WorldInit(&w, CTRW_TILE_LAND);
    CtrWildsCore_Tick(&c, 2, 6, FreeObjects, &w);
    CHECK(CtrWildsCore_SuppressStep(&c, CTRW_TILE_LAND) && CtrWildsCore_SuppressStep(&c, CTRW_TILE_WATER), "operative and room: the step encounters give way");
    /* each kind only where the map has it */
    MapOn(&c, 10, 1, 0);
    CtrWildsCore_Frame(&c, &env);
    c.spawnable = 1;
    CHECK(CtrWildsCore_SuppressStep(&c, CTRW_TILE_LAND) && !CtrWildsCore_SuppressStep(&c, CTRW_TILE_WATER), "land only: surfing keeps its encounters");
    MapOn(&c, 11, 0, 1);
    CtrWildsCore_Frame(&c, &env);
    c.spawnable = 1;
    CHECK(!CtrWildsCore_SuppressStep(&c, CTRW_TILE_LAND) && CtrWildsCore_SuppressStep(&c, CTRW_TILE_WATER), "water only");
    /* a map with no table or no picture (the probe found none): not operative */
    MapOn(&c, 12, 0, 0);
    CtrWildsCore_Frame(&c, &env);
    c.spawnable = 1;
    CHECK(!c.operative && !CtrWildsCore_SuppressStep(&c, CTRW_TILE_LAND), "no table with a picture: the game's encounters");
    /* blocked (Safari, Randomlocke, Union Room, quest log, encounters disabled) */
    MapOn(&c, 13, 1, 1);
    env.blocked = 1;
    CHECK(!CtrWildsCore_Frame(&c, &env) && !c.operative, "blocked: not operative");
    c.spawnable = 1;
    CHECK(!CtrWildsCore_SuppressStep(&c, CTRW_TILE_LAND), "blocked: the game's encounters");
    env.blocked = 0;
    /* the free object events run out: no wild can appear, so the original encounters come back */
    WorldInit(&w, CTRW_TILE_LAND);
    MakeNpcs(&w, 13);
    MapOn(&c, 14, 1, 0);
    CtrWildsCore_Frame(&c, &env);
    CtrWildsCore_Tick(&c, 2, 6, FreeObjects, &w);
    CHECK(c.operative && !c.spawnable && !CtrWildsCore_SuppressStep(&c, CTRW_TILE_LAND), "no room for a wild: the game's encounters");
    /* a wild that is already out keeps them hidden, even when the budget is spent */
    WorldInit(&w, CTRW_TILE_LAND);
    MapOn(&c, 15, 1, 0);
    Run(&c, &w, 1, 6, 1500);
    CHECK(c.active == 2 && CtrWildsCore_SuppressStep(&c, CTRW_TILE_LAND), "two wilds out: hidden");
    /* Randomlocke and the Safari Zone are 'blocked'; the Unown guard is the species check */
    CHECK(!CtrWildsCore_SpeciesOk(201, 1, 1) && CtrWildsCore_SpeciesOk(16, 1, 0) && !CtrWildsCore_SpeciesOk(16, 0, 0) && !CtrWildsCore_SpeciesOk(0, 1, 0),
          "species guard: Unown, no picture and nothing are not seen");
    /* the option off with nothing out is the early out */
    CtrWildsCore_Init(&c, 32);
    CHECK(CtrWildsCore_Idle(&c, 0), "OFF and nothing out: idle");
    CHECK(!CtrWildsCore_Idle(&c, 1), "ON: not idle");
    MapOn(&c, 16, 1, 0);
    WorldInit(&w, CTRW_TILE_LAND);
    Run(&c, &w, 1, 6, 1500);
    CHECK(!CtrWildsCore_Idle(&c, 0), "OFF with wilds out: they have to be removed");
}

/* The glue's Idle path: the option is off and nothing is out, so it returns at once (no Frame). */
static int GlueIdle(CtrWildsCore *c, int density)
{
    return CtrWildsCore_Idle(c, density) || CtrWildsCore_Unsupported(c);
}

static void TestIdleReleases(void)
{
    CtrWildsCore c;
    World w;

    /* The audit's sequence: NORMAL on a route, no wild out yet (operative, spawnable), then OFF: the original encounters are back
     * at once, without a Frame that would clear the flags. */
    CtrWildsCore_Init(&c, 61);
    WorldInit(&w, CTRW_TILE_LAND);
    MapOn(&c, 20, 1, 0);
    Frame(&c, &w, 2, 6, 0);
    CHECK(c.active == 0 && c.operative && c.spawnable, "A1 setup: nothing out, operative, spawnable");
    CHECK(CtrWildsCore_SuppressStep(&c, CTRW_TILE_LAND), "A1 setup: the step encounters give way");
    CHECK(GlueIdle(&c, 0), "A1: OFF with nothing out takes the idle path");
    CHECK(!CtrWildsCore_SuppressStep(&c, CTRW_TILE_LAND) && !CtrWildsCore_SuppressStep(&c, CTRW_TILE_WATER), "A1: OFF: the game's encounters at once");
    CHECK(!c.operative && !c.spawnable, "A1: operative and spawnable cleared by the idle path");
    /* ... and turning it back on works again */
    Run(&c, &w, 2, 6, 40);
    CHECK(c.operative && CtrWildsCore_SuppressStep(&c, CTRW_TILE_LAND), "A1: back ON: it hides them again");
    /* water too */
    CtrWildsCore_Init(&c, 62);
    MapOn(&c, 21, 1, 1);
    Frame(&c, &w, 2, 6, 0);
    CHECK(GlueIdle(&c, 0) && !CtrWildsCore_SuppressStep(&c, CTRW_TILE_WATER), "A1: water as well");
    /* with a wild out the idle path is not taken: the Frame removes it first */
    CtrWildsCore_Init(&c, 63);
    WorldInit(&w, CTRW_TILE_LAND);
    MapOn(&c, 22, 1, 0);
    Run(&c, &w, 1, 6, 1500);
    CHECK(c.active > 0 && !GlueIdle(&c, 0), "A1: wilds out: no idle path");
    Frame(&c, &w, 0, 6, 0);
    CHECK(c.active == 0 && !c.operative && !CtrWildsCore_SuppressStep(&c, CTRW_TILE_LAND), "A1: OFF with wilds out: removed, the game's encounters");
    /* a map that was probed with nothing to show: the unsupported path releases too */
    CtrWildsCore_Init(&c, 64);
    MapOn(&c, 23, 1, 0);
    c.probed = 1;
    c.operative = c.spawnable = 1;
    c.mapLand = 0;
    CHECK(GlueIdle(&c, 2) && !c.operative && !c.spawnable && !CtrWildsCore_SuppressStep(&c, CTRW_TILE_LAND), "A1: unsupported map: released");
    /* the failure count goes with it */
    CtrWildsCore_Init(&c, 65);
    MapOn(&c, 24, 1, 0);
    c.fails = 4;
    CHECK(CtrWildsCore_Idle(&c, 0) && c.fails == 0, "A1: idle forgets the failures");
}

/* A world with land only in a small patch around the player: the ring outside the screen has no valid cell. */
static void WorldPatch(World *w, int half)
{
    WorldInit(w, CTRW_TILE_NONE);
    for (int y = w->py - half; y <= w->py + half; ++y)
        for (int x = w->px - half; x <= w->px + half; ++x)
            w->tile[y][x] = CTRW_TILE_LAND;
}

static void WorldAllLand(World *w)
{
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            w->tile[y][x] = CTRW_TILE_LAND;
}

static int FramesToStall(CtrWildsCore *c, World *w, int limit)
{
    for (int i = 0; i < limit; ++i)
    {
        Frame(c, w, 2, 6, 0);
        if (CtrWildsCore_Stalled(c))
            return i;
    }
    return -1;
}

static void TestStall(void)
{
    CtrWildsCore c;
    World w;
    int n, supp = 0, frames;

    /* a map smaller than the screen: nowhere to appear. Suppressed while it still tries, then the game's encounters come back. */
    CtrWildsCore_Init(&c, 71);
    WorldPatch(&w, 3);
    MapOn(&c, 30, 1, 0);
    for (int i = 0; i < 100; ++i)
        Frame(&c, &w, 2, 6, 0);
    CHECK(c.active == 0 && c.operative && c.spawnable && c.fails < CTRW_STALL_FAILS && CtrWildsCore_SuppressStep(&c, CTRW_TILE_LAND),
          "M1: the first failures still suppress (%d)", c.fails);
    n = FramesToStall(&c, &w, 20000);
    CHECK(n > 0 && n < 20000, "M1: it stalls after %d more frames", n);
    CHECK(c.fails == CTRW_STALL_FAILS && !CtrWildsCore_SuppressStep(&c, CTRW_TILE_LAND) && !CtrWildsCore_SuppressStep(&c, CTRW_TILE_WATER),
          "M1: stalled: the game's encounters are back");
    CHECK(c.operative && c.spawnable, "M1: the system is still operative (nothing switched off)");
    /* a long time stalled: still not suppressing, still trying (the counter saturates, never wraps) */
    for (int i = 0; i < 100000; ++i)
    {
        Frame(&c, &w, 2, 6, 0);
        supp += CtrWildsCore_SuppressStep(&c, CTRW_TILE_LAND);
    }
    CHECK(supp == 0 && c.fails >= CTRW_STALL_FAILS && CtrWildsCore_Stalled(&c), "M1: 100000 frames of failures: never suppresses again (fails %d)", c.fails);
    /* the player walks to a place with room: the next attempt succeeds and it hides the encounters again */
    WorldAllLand(&w);
    frames = 0;
    while (c.active == 0 && frames < 2000)
    {
        Frame(&c, &w, 2, 6, 0);
        ++frames;
    }
    CHECK(c.active > 0 && c.fails == 0 && !CtrWildsCore_Stalled(&c), "M1: recovered by itself in %d frames", frames);
    CHECK(frames <= 330 && CtrWildsCore_SuppressStep(&c, CTRW_TILE_LAND), "M1: the retry came within 300 frames and the step encounters give way again");
    /* the wild goes: the count is 0, so it hides the encounters while it tries again */
    Shed(&c, &w, c.active);
    CHECK(c.active == 0 && c.fails == 0 && CtrWildsCore_SuppressStep(&c, CTRW_TILE_LAND), "M1: a success resets the count");
    /* failures below the threshold that end in a success never release */
    CtrWildsCore_Init(&c, 72);
    WorldPatch(&w, 3);
    MapOn(&c, 31, 1, 0);
    for (int i = 0; i < 3000 && c.fails < 3; ++i)
        Frame(&c, &w, 2, 6, 0);
    CHECK(c.fails >= 3 && c.fails < CTRW_STALL_FAILS, "M1 setup: a few failures (%d)", c.fails);
    WorldAllLand(&w);
    for (int i = 0; i < 400; ++i)
    {
        Frame(&c, &w, 2, 6, 0);
        supp += !CtrWildsCore_SuppressStep(&c, CTRW_TILE_LAND) && c.spawnable;
    }
    CHECK(c.active > 0 && supp == 0, "M1: a short run of failures then a success: always hidden (%d)", supp);
    /* a new map and the option going off both reset the count */
    CtrWildsCore_Init(&c, 73);
    WorldPatch(&w, 3);
    MapOn(&c, 32, 1, 0);
    CHECK(FramesToStall(&c, &w, 20000) > 0, "M1: stalls");
    MapOn(&c, 33, 1, 0);
    CHECK(c.fails == 0 && !CtrWildsCore_Stalled(&c), "M1: a new map starts over");
    CHECK(FramesToStall(&c, &w, 20000) > 0, "M1: stalls again");
    CHECK(GlueIdle(&c, 0) && c.fails == 0, "M1: the option off resets it");
    /* a narrow route, 1 tile wide: where it goes on below the screen a wild appears on it; a dead end inside the screen has no room */
    CtrWildsCore_Init(&c, 74);
    WorldInit(&w, CTRW_TILE_NONE);
    for (int y = 0; y < H; ++y)
        w.tile[y][w.px] = CTRW_TILE_LAND;
    MapOn(&c, 34, 1, 0);
    for (int i = 0; i < 3000; ++i)
        Frame(&c, &w, 2, 6, 0);
    CHECK(c.active > 0 && !CtrWildsCore_Stalled(&c), "M1: a long narrow route: a wild appears on it");
    CtrWildsCore_Init(&c, 78);
    WorldInit(&w, CTRW_TILE_NONE);
    for (int y = 0; y <= w.py + 3; ++y)
        w.tile[y][w.px] = CTRW_TILE_LAND;
    MapOn(&c, 38, 1, 0);
    CHECK(FramesToStall(&c, &w, 20000) > 0, "M1: a narrow dead end inside the screen: stalls");
    /* the player in a corner of a map */
    CtrWildsCore_Init(&c, 75);
    WorldInit(&w, CTRW_TILE_LAND);
    w.px = 1;
    w.py = 1;
    MapOn(&c, 35, 1, 0);
    for (int i = 0; i < 3000; ++i)
        Frame(&c, &w, 2, 6, 0);
    CHECK(c.active > 0 || CtrWildsCore_Stalled(&c), "M1: a corner of a map: a wild or a stall, never a lock");
    /* a land table on water cells */
    CtrWildsCore_Init(&c, 76);
    WorldInit(&w, CTRW_TILE_WATER);
    MapOn(&c, 36, 1, 0);
    CHECK(FramesToStall(&c, &w, 20000) > 0, "M1: a land table on water cells: stalls");
    /* any failed attempt counts (no cell, no species, no object event) */
    CtrWildsCore_Init(&c, 77);
    WorldInit(&w, CTRW_TILE_LAND);
    MapOn(&c, 37, 1, 0);
    Frame(&c, &w, 2, 6, 0);
    for (int i = 0; i < CTRW_STALL_FAILS; ++i)
        CtrWildsCore_SpawnDone(&c, 0);
    CHECK(CtrWildsCore_Stalled(&c) && !CtrWildsCore_SuppressStep(&c, CTRW_TILE_LAND), "M1: failures of any kind count");
    c.active = 1;
    CHECK(!CtrWildsCore_Stalled(&c), "M1: a wild out is not a stall");
    c.active = 0;
    /* the retry goes on at a lower rate while stalled */
    CtrWildsCore_SpawnDone(&c, 0);
    CHECK(c.spawnTimer >= CTRW_STALLED_MIN && c.spawnTimer < CTRW_STALLED_MIN + CTRW_STALLED_SPAN, "M1: stalled retries wait 60 to 90 frames (%d)",
          c.spawnTimer);
}

/* The saved game: wilds the table does not own. */
static void TestOrphans(void)
{
    CtrWildsCore c;
    const int marked = 1;

    CtrWildsCore_Init(&c, 81);
    /* after a load the table is empty: every marked wild object is an orphan; the others are not */
    for (int s = 0; s < CTRW_SLOTS; ++s)
        CHECK(CtrWildsCore_IsOrphan(&c, 2 + s, CTRW_LOCALID + s, marked), "M2: marked wild %d with an empty table is an orphan", s);
    CHECK(!CtrWildsCore_IsOrphan(&c, 3, CTRW_LOCALID, 0), "M2: the Berry Blender's local id 0xF0 without the mark is not touched");
    for (int id = 0; id < 0xF0; ++id)
        if (CtrWildsCore_IsOrphan(&c, 5, id, marked) || CtrWildsCore_IsOrphan(&c, 5, id, 0))
        {
            CHECK(0, "M2: a map's local id %d is not a wild", id);
            break;
        }
    CHECK(!CtrWildsCore_IsOrphan(&c, 4, 0xF7, marked) && !CtrWildsCore_IsOrphan(&c, 4, 0xF7, 0), "M2: the companion (0xF7) is never touched");
    CHECK(!CtrWildsCore_IsOrphan(&c, 4, 0xF6, marked) && !CtrWildsCore_IsOrphan(&c, 4, 0xFF, marked), "M2: ids beside the wilds' are not touched");
    for (int id = 0xEC; id <= 0xF0; ++id)
        CHECK(!CtrWildsCore_IsOrphan(&c, 7, id, 0), "M2: the Berry Blender's local id %d unmarked is not touched", id);
    /* a wild the table owns (same slot, same object event index) is kept; one at another index, or in a free slot, is an orphan */
    CtrWildsCore_MapChanged(&c, 5);
    {
        CtrWildMon mon = {16, 5, 0, 0, 12345, 100};

        CtrWildsCore_Commit(&c, 2, &mon, 0, 0, 1);
        CtrWildsCore_SetObject(&c, 2, 9);
    }
    CHECK(!CtrWildsCore_IsOrphan(&c, 9, CTRW_LOCALID + 2, marked), "M2: the table's own wild is kept");
    CHECK(CtrWildsCore_IsOrphan(&c, 4, CTRW_LOCALID + 2, marked), "M2: the same local id at another object event is an orphan");
    CHECK(CtrWildsCore_IsOrphan(&c, 9, CTRW_LOCALID + 3, marked), "M2: a free slot's local id is an orphan");
    CHECK(!CtrWildsCore_IsOrphan(&c, 9, CTRW_LOCALID + 2, 0), "M2: unmarked is never touched, even at the table's own slot");
    /* a sweep as the glue does it over the object events: NPCs, the companion, a Berry Blender object and wilds (one of them kept) */
    {
        struct { int id, mark; } obj[10] = {{0, 0}, {1, 0}, {2, 0}, {3, 0}, {0xF7, 1}, {0xF0, 0}, {0xF1, 1}, {0xEC, 0}, {9, 0}, {0xF0 + 2, 1}};
        int swept = 0, sweptAt = -1, kept = 0;

        for (int i = 0; i < 10; ++i)
        {
            if (CtrWildsCore_IsOrphan(&c, i, obj[i].id, obj[i].mark))
                ++swept, sweptAt = i;
            else if (i == 9)
                ++kept;
        }
        CHECK(swept == 1 && sweptAt == 6 && kept == 1, "M2: only the orphan at 6 is swept (%d at %d, %d)", swept, sweptAt, kept);
    }
    /* the table's own wild sits at index 9 here, so the one at 8 (same slot) is an orphan too: a stale table never owns by slot alone */
    CHECK(CtrWildsCore_IsOrphan(&c, 8, CTRW_LOCALID + 2, marked), "M2: ownership is by slot and index together");
    CtrWildsCore_Clear(&c);
    CHECK(CtrWildsCore_IsOrphan(&c, 9, CTRW_LOCALID + 2, marked), "M2: a cleared table owns nothing");
}

static void TestRepel(void)
{
    CHECK(!CtrWildsCore_RepelBlocks(0, 3, 50), "no Repel: nothing blocked");
    CHECK(CtrWildsCore_RepelBlocks(1, 9, 10), "Repel: a level under the lead's is blocked");
    CHECK(!CtrWildsCore_RepelBlocks(1, 10, 10) && !CtrWildsCore_RepelBlocks(1, 11, 10), "Repel: the lead's level and above come through, as the game");
    CHECK(CtrWildsCore_RepelBlocks(1, 99, 0), "Repel with no Pokemon able to fight: blocked, as the game");
    CHECK(!CtrWildsCore_RepelBlocks(0, 3, 0), "no Repel and no lead: not blocked");
}

static void SenseAt(CtrWildSense *s, int dx, int dy, int fast, int busy)
{
    s->dx = dx;
    s->dy = dy;
    s->fast = (uint8_t)fast;
    s->sameElev = 1;
    s->busy = (uint8_t)busy;
}

static int Make(CtrWildsCore *c, int slot, int temper, uint32_t power)
{
    CtrWildMon mon;

    memset(&mon, 0, sizeof(mon));
    mon.species = 19;
    mon.level = 5;
    mon.power = power;
    CtrWildsCore_Commit(c, slot, &mon, 0, 0, 1);
    c->wild[slot].temper = (uint8_t)temper;
    c->wild[slot].cool = 0;
    c->wild[slot].timer = 0;
    c->wild[slot].state = CTRW_S_IDLE;
    return slot;
}

static void TestAi(void)
{
    CtrWildsCore c;
    CtrWildSense s;
    CtrWildAct a;

    /* idle -> wander -> idle: it turns, walks one to three steps and rests */
    CtrWildsCore_Init(&c, 41);
    c.leadPower = 1000;
    Make(&c, 0, CTRW_TEMPER_CALM, 1000);
    {
        int turns = 0, steps = 0, sawWander = 0, sawIdleAfter = 0;

        for (int i = 0; i < 6000; ++i)
        {
            SenseAt(&s, 30, 30, 0, 0);
            CtrWildsCore_Think(&c, 0, &s, &a);
            if (a.kind == CTRW_A_TURN)
            {
                ++turns;
                CHECK(a.dir[0] >= 1 && a.dir[0] <= 4, "turn direction");
            }
            if (a.kind == CTRW_A_STEP)
            {
                ++steps;
                CHECK(a.n == 2 && a.dir[0] >= 1 && a.dir[0] <= 4 && a.dir[1] >= 1 && a.dir[1] <= 4 && a.dir[0] != a.dir[1], "wander directions");
            }
            if (c.wild[0].state == CTRW_S_WANDER)
                sawWander = 1;
            if (sawWander && c.wild[0].state == CTRW_S_IDLE)
                sawIdleAfter = 1;
            CHECK(a.kind != CTRW_A_ALERT && a.kind != CTRW_A_CHARGE && a.kind != CTRW_A_LEAVE, "a far calm wild only idles and wanders");
        }
        CHECK(turns > 10 && steps > 10 && sawWander && sawIdleAfter, "idle and wander alternate (%d turns, %d steps)", turns, steps);
    }
    /* a busy wild does not start steps */
    CtrWildsCore_Init(&c, 42);
    Make(&c, 0, CTRW_TEMPER_CALM, 1000);
    for (int i = 0; i < 2000; ++i)
    {
        SenseAt(&s, 30, 30, 0, 1);
        CtrWildsCore_Think(&c, 0, &s, &a);
        CHECK(a.kind == CTRW_A_NONE, "busy: no action");
    }
    /* a failed step ends the walk */
    CtrWildsCore_Init(&c, 43);
    Make(&c, 0, CTRW_TEMPER_CALM, 1000);
    c.wild[0].state = CTRW_S_WANDER;
    c.wild[0].steps = 3;
    CtrWildsCore_StepResult(&c, 0, 0);
    CHECK(c.wild[0].steps == 0, "a blocked step ends the walk");

    /* an aggressive wild notices at 4 tiles, not 5: '!' then the chase */
    CtrWildsCore_Init(&c, 44);
    c.leadPower = 1000;
    Make(&c, 0, CTRW_TEMPER_AGGRESSIVE, 1000);
    SenseAt(&s, 5, 0, 0, 0);
    for (int i = 0; i < 100; ++i)
    {
        CtrWildsCore_Think(&c, 0, &s, &a);
        CHECK(a.kind != CTRW_A_ALERT, "five tiles: not noticed");
    }
    SenseAt(&s, 4, 3, 0, 0);
    CtrWildsCore_Think(&c, 0, &s, &a);
    CHECK(a.kind == CTRW_A_ALERT && c.wild[0].state == CTRW_S_ALERT && c.wild[0].after == CTRW_S_CHASE, "four tiles: '!'");
    for (int i = 0; i < 30; ++i)
    {
        CtrWildsCore_Think(&c, 0, &s, &a);
        CHECK(a.kind == CTRW_A_NONE && c.wild[0].state == CTRW_S_ALERT, "the alert pauses it");
    }
    CtrWildsCore_Think(&c, 0, &s, &a);
    CHECK(c.wild[0].state == CTRW_S_CHASE || a.kind == CTRW_A_STEP, "then it chases");
    CHECK(a.kind == CTRW_A_STEP && a.dir[0] == CTRW_DIR_EAST && a.dir[1] == CTRW_DIR_SOUTH && !a.fast, "it steps toward the player, the longer axis first, at walking pace");
    /* the chase goes to 6 tiles and ends at 7 */
    {
        int steps = 0;

        SenseAt(&s, 6, 0, 0, 0);
        for (int i = 0; i < 40 && c.wild[0].state == CTRW_S_CHASE; ++i)
        {
            CtrWildsCore_Think(&c, 0, &s, &a);
            steps += a.kind == CTRW_A_STEP;
        }
        CHECK(steps <= 11, "the chase tires out after its steps (%d)", steps);
    }
    CtrWildsCore_Init(&c, 45);
    c.leadPower = 1000;
    Make(&c, 0, CTRW_TEMPER_AGGRESSIVE, 1000);
    c.wild[0].state = CTRW_S_CHASE;
    c.wild[0].steps = 12;
    SenseAt(&s, 6, 0, 0, 0);
    CtrWildsCore_Think(&c, 0, &s, &a);
    CHECK(a.kind == CTRW_A_STEP && c.wild[0].state == CTRW_S_CHASE, "six tiles: still chasing");
    SenseAt(&s, 7, 0, 0, 0);
    CtrWildsCore_Think(&c, 0, &s, &a);
    CHECK(a.kind == CTRW_A_NONE && c.wild[0].state == CTRW_S_IDLE && c.wild[0].cool == 600, "seven tiles: it loses interest and rests");
    /* adjacent: the charge (the encounter) */
    c.wild[0].state = CTRW_S_CHASE;
    c.wild[0].steps = 5;
    SenseAt(&s, 0, 1, 0, 0);
    CtrWildsCore_Think(&c, 0, &s, &a);
    CHECK(a.kind == CTRW_A_CHARGE && c.wild[0].state == CTRW_S_ENCOUNTER, "next to the player: the encounter");
    CtrWildsCore_Calm(&c, 0, 900);
    CHECK(c.wild[0].state == CTRW_S_IDLE && c.wild[0].cool == 900, "a refused encounter calms it");
    /* an encounter nobody picked up expires */
    c.wild[0].state = CTRW_S_ENCOUNTER;
    c.wild[0].timer = 3;
    for (int i = 0; i < 5; ++i)
        CtrWildsCore_Think(&c, 0, &s, &a);
    CHECK(c.wild[0].state == CTRW_S_IDLE, "an unclaimed encounter settles");
    /* a different height is never noticed */
    CtrWildsCore_Init(&c, 46);
    c.leadPower = 1000;
    Make(&c, 0, CTRW_TEMPER_AGGRESSIVE, 1000);
    SenseAt(&s, 2, 0, 0, 0);
    s.sameElev = 0;
    for (int i = 0; i < 50; ++i)
    {
        CtrWildsCore_Think(&c, 0, &s, &a);
        CHECK(a.kind != CTRW_A_ALERT, "another height: not noticed");
    }

    /* aggressive but much weaker than the lead: it is alarmed, then it runs instead of chasing */
    CtrWildsCore_Init(&c, 47);
    c.leadPower = 10000;
    Make(&c, 0, CTRW_TEMPER_AGGRESSIVE, 1000);
    SenseAt(&s, 3, 0, 0, 0);
    CtrWildsCore_Think(&c, 0, &s, &a);
    CHECK(a.kind == CTRW_A_ALERT && c.wild[0].after == CTRW_S_FLEE, "weaker: '!' then flight");
    for (int i = 0; i < 30; ++i)
        CtrWildsCore_Think(&c, 0, &s, &a);
    CtrWildsCore_Think(&c, 0, &s, &a);
    CHECK(c.wild[0].state == CTRW_S_FLEE, "it flees");
    CHECK(a.kind == CTRW_A_STEP && a.fast && a.dir[0] == CTRW_DIR_WEST, "away from the player, fast");
    /* a calm weak wild: runs only sometimes, from 2 tiles (4 when the player runs); a strong or equal one never */
    {
        int fled = 0, calmFleeFast = 0, alerts = 0, strongReact = 0;

        for (int seed = 0; seed < 400; ++seed)
        {
            CtrWildsCore_Init(&c, (uint32_t)(1000 + seed * 7919));
            c.leadPower = 10000;
            Make(&c, 0, CTRW_TEMPER_CALM, 4000);            /* 40% of the lead's: chance 10 + (70 - 40) = 40% */
            SenseAt(&s, 2, 0, 0, 0);
            CtrWildsCore_Think(&c, 0, &s, &a);
            alerts += a.kind == CTRW_A_ALERT;
            fled += c.wild[0].state == CTRW_S_FLEE;
            CtrWildsCore_Init(&c, (uint32_t)(5000 + seed * 104729));
            c.leadPower = 10000;
            Make(&c, 0, CTRW_TEMPER_CALM, 4000);
            SenseAt(&s, 4, 0, 1, 0);
            CtrWildsCore_Think(&c, 0, &s, &a);
            calmFleeFast += c.wild[0].state == CTRW_S_FLEE;
            CtrWildsCore_Init(&c, (uint32_t)(9000 + seed * 31));
            c.leadPower = 1000;
            Make(&c, 0, CTRW_TEMPER_CALM, 5000);            /* stronger: ignores */
            SenseAt(&s, 1, 0, 1, 0);
            CtrWildsCore_Think(&c, 0, &s, &a);
            strongReact += c.wild[0].state >= CTRW_S_ALERT;
        }
        CHECK(alerts == 0, "a calm wild never shows '!'");
        CHECK(fled > 100 && fled < 220, "a calm weak wild flees about 40%% (%d of 400)", fled);
        CHECK(calmFleeFast > 100, "running widens the range to 4 (%d)", calmFleeFast);
        CHECK(strongReact == 0, "a calm stronger wild ignores the player");
    }
    /* the flight ends, the wild rests and may leave when it is far */
    CtrWildsCore_Init(&c, 48);
    c.leadPower = 10000;
    Make(&c, 0, CTRW_TEMPER_CALM, 1000);
    c.wild[0].state = CTRW_S_FLEE;
    c.wild[0].steps = 2;
    SenseAt(&s, 1, 0, 0, 0);
    {
        int steps = 0;

        for (int i = 0; i < 10; ++i)
        {
            CtrWildsCore_Think(&c, 0, &s, &a);
            steps += a.kind == CTRW_A_STEP;
        }
        CHECK(steps == 2 && c.wild[0].state == CTRW_S_IDLE && c.wild[0].cool > 500, "a flight of two steps ends in a rest (%d)", steps);
    }
    {
        int left = 0;

        for (int seed = 0; seed < 400; ++seed)
        {
            CtrWildsCore_Init(&c, (uint32_t)(77 + seed * 13));
            Make(&c, 0, CTRW_TEMPER_CALM, 1000);
            c.wild[0].state = CTRW_S_FLEE;
            c.wild[0].steps = 0;
            SenseAt(&s, 7, 0, 0, 0);
            CtrWildsCore_Think(&c, 0, &s, &a);
            left += a.kind == CTRW_A_LEAVE;
        }
        CHECK(left > 80 && left < 160, "a far runner leaves with a 30%% chance (%d of 400)", left);
    }
    /* the temperament comes from the setting */
    for (int aggro = 0; aggro < CTRW_AGGROS; ++aggro)
    {
        int n = 0, total = 6000;

        CtrWildsCore_Init(&c, (uint32_t)(300 + aggro));
        c.aggro = (uint8_t)aggro;
        for (int i = 0; i < total; ++i)
        {
            CtrWildMon mon = {16, 5, 0, 0, 0, 100};

            n += CtrWildsCore_Commit(&c, 0, &mon, 0, 0, 1) == CTRW_TEMPER_AGGRESSIVE;
            CtrWildsCore_Forget(&c, 0);
        }
        CHECK(n * 100 >= total * CtrWildsCore_AggroPercent(aggro) * 9 / 10 - 50 && n * 100 <= total * CtrWildsCore_AggroPercent(aggro) * 11 / 10 + 50,
              "aggression %d: %d of %d are aggressive", aggro, n, total);
    }
}

typedef struct Battle
{
    uint16_t species;
    uint8_t level, nature, gender, shiny, roamer;
    uint32_t personality;
} Battle;

static Battle Launch(const CtrWild *w, int genderRatio, uint32_t otId)
{
    Battle b;

    b.species = w->species;
    b.level = w->level;
    b.personality = w->personality;
    b.nature = (uint8_t)CtrWildsCore_Nature(w->personality);
    b.gender = (uint8_t)CtrWildsCore_Gender(genderRatio, w->personality);
    b.shiny = (uint8_t)CtrWildsCore_Shiny(otId, w->personality);
    b.roamer = w->roamer;
    return b;
}

static void TestData(void)
{
    CtrWildsCore c;
    int shinies = 0;

    CtrWildsCore_Init(&c, 51);
    /* a personality with the nature that is asked, for each of the 25 */
    for (int nature = 0; nature < 25; ++nature)
        for (int i = 0; i < 200; ++i)
            CHECK(CtrWildsCore_Nature(CtrWildsCore_Personality(&c, nature)) == nature, "nature %d", nature);
    /* the edge: a personality near the top of 32 bits does not wrap */
    c.rng = 0;
    {
        uint32_t p;

        for (int i = 0; i < 100000; ++i)
        {
            p = CtrWildsCore_Personality(&c, 24);
            if (CtrWildsCore_Nature(p) != 24)
            {
                CHECK(0, "wrapped personality");
                break;
            }
        }
    }
    /* the gender rule is the game's */
    CHECK(CtrWildsCore_Gender(0, 0x12345678) == 0 && CtrWildsCore_Gender(254, 5) == 254 && CtrWildsCore_Gender(255, 77) == 255, "fixed genders");
    CHECK(CtrWildsCore_Gender(31, 0x1E) == 254 && CtrWildsCore_Gender(31, 0x1F) == 0 && CtrWildsCore_Gender(127, 0x7E) == 254 && CtrWildsCore_Gender(127, 0x7F) == 0,
          "ratio against the low byte");
    CHECK(CtrWildsCore_Shiny(0x0000ABCD, 0x0000ABCD) && CtrWildsCore_Shiny(0x12340000, 0x12340007) && !CtrWildsCore_Shiny(0x12340000, 0x12340008), "shiny: the sum under 8");
    /* what is rolled is what the battle gets: species, level, personality, nature, gender, shininess, roamer */
    for (int i = 0; i < 2000; ++i)
    {
        CtrWildMon mon = {(uint16_t)(1 + i % 386), (uint8_t)(2 + i % 60), (uint8_t)(i & 1), (uint8_t)(i % 7 == 0),
                          CtrWildsCore_Personality(&c, i % 25), 123};
        uint32_t otId = 0xA1B2C3D4u ^ (uint32_t)i;
        int ratio = (i % 4) * 85 + (i % 3), gender = CtrWildsCore_Gender(ratio, mon.personality), shiny = CtrWildsCore_Shiny(otId, mon.personality);
        Battle b;

        c.aggro = 0;
        CtrWildsCore_Forget(&c, 3);
        CtrWildsCore_Commit(&c, 3, &mon, gender, shiny, 1024 + i % 386);
        b = Launch(&c.wild[3], ratio, otId);
        CHECK(b.species == mon.species && b.level == mon.level && b.personality == mon.personality && b.nature == i % 25 && b.gender == gender
              && b.shiny == shiny && b.roamer == mon.roamer && c.wild[3].gender == gender && c.wild[3].shiny == shiny, "round trip of Pokemon %d", i);
        shinies += shiny;
        CtrWildsCore_Forget(&c, 3);
    }
    CHECK(shinies <= 5, "shininess keeps the game's odds (%d of 2000)", shinies);
    /* strength */
    CHECK(CtrWildsCore_Power(10, 300) == 3000, "power");
    c.leadPower = 0;
    Make(&c, 1, CTRW_TEMPER_CALM, 100);
    CHECK(CtrWildsCore_Judge(&c, 1) == 0, "no lead: unknown");
    c.leadPower = 200;
    CHECK(CtrWildsCore_Judge(&c, 1) == 50, "half the lead's power");
}

static void TestRoamer(void)
{
    CtrWildsCore c;
    int yes = 0;

    CtrWildsCore_Init(&c, 61);
    CHECK(!CtrWildsCore_RollRoamer(&c, 0, 0), "no roamer on this map: none");
    CHECK(!CtrWildsCore_RollRoamer(&c, 1, 1), "the roamer is already out: not another");
    for (int i = 0; i < 4000; ++i)
        yes += CtrWildsCore_RollRoamer(&c, 1, 0);
    CHECK(yes > 850 && yes < 1150, "the roamer shows with the game's 1 in 4 (%d of 4000)", yes);
    /* a roamer wild keeps its flag into the battle */
    {
        CtrWildMon mon = {243, 50, 0, 1, 0xCAFEBABE, 100};

        CtrWildsCore_Commit(&c, 0, &mon, 0, 0, 1);
        CHECK(Launch(&c.wild[0], 31, 1).roamer == 1 && c.wild[0].roamer == 1, "a roamer's flag survives");
    }
}

static void TestIds(void)
{
    CtrWildsCore c;
    World w;

    /* the table keeps the object event of each wild; forgetting and shedding leave none behind */
    CtrWildsCore_Init(&c, 71);
    WorldInit(&w, CTRW_TILE_LAND);
    MapOn(&c, 1, 1, 0);
    Run(&c, &w, 3, 6, 3000);
    for (int i = 0; i < CTRW_SLOTS; ++i)
        if (c.wild[i].state != CTRW_S_FREE)
        {
            CHECK(c.wild[i].obj < CTRW_OBJECTS && w.objActive[c.wild[i].obj], "slot %d points at a live object event", i);
            for (int j = i + 1; j < CTRW_SLOTS; ++j)
                CHECK(c.wild[j].state == CTRW_S_FREE || c.wild[j].obj != c.wild[i].obj, "two wilds share an object event");
        }
    Shed(&c, &w, 99);
    CHECK(c.active == 0 && FreeObjects(&w) == CTRW_OBJECTS - 1, "shed all: the table and the object events are clean");
    CtrWildsCore_Forget(&c, 2);
    CtrWildsCore_Forget(&c, 99);
    CHECK(c.active == 0, "forgetting an empty slot changes nothing");
    CHECK(CtrWildsCore_FarthestSlot(&c) == CTRW_NONE, "no wild: no farthest");
    /* the farthest is shed first */
    {
        CtrWildMon mon = {16, 5, 0, 0, 1, 100};

        CtrWildsCore_Commit(&c, 0, &mon, 0, 0, 1);
        CtrWildsCore_Commit(&c, 1, &mon, 0, 0, 1);
        c.wild[0].dist = 3;
        c.wild[1].dist = 9;
        CHECK(CtrWildsCore_FarthestSlot(&c) == 1, "the farthest wild goes first");
    }
}

/* The rhythm of the attempts ------------------------------------------------------------------------------------------------ */

#define SEEDS 4000

/* Every value over many seeds is inside the range and both ends are reached (so the range is exact, not just contained). */
typedef struct Range { int seen, min, max; } Range;

static void RangeAdd(Range *r, int v)
{
    if (!r->seen || v < r->min)
        r->min = v;
    if (!r->seen || v > r->max)
        r->max = v;
    r->seen = 1;
}

static void CheckRange(const Range *r, int lo, int hi, const char *what)
{
    CHECK(r->seen && r->min == lo && r->max == hi, "%s: %d to %d expected, got %d to %d", what, lo, hi, r->min, r->max);
}

static void TestRhythm(void)
{
    Range first = {0}, fill = {0}, topup = {0}, fail = {0}, stalled = {0}, removed = {0};

    for (int seed = 1; seed <= SEEDS; ++seed)
    {
        CtrWildsCore c;

        CtrWildsCore_Init(&c, (uint32_t)seed);
        CtrWildsCore_MapChanged(&c, 9);
        RangeAdd(&first, c.spawnTimer);
        /* a success with the target at 6: 2 out (active * 2 < target) fills, 3 or more completes */
        c.target = 6;
        c.active = 2;
        CtrWildsCore_SpawnDone(&c, 1);
        RangeAdd(&fill, c.spawnTimer);
        c.active = 3;
        CtrWildsCore_SpawnDone(&c, 1);
        RangeAdd(&topup, c.spawnTimer);
        /* a failure with nothing out: 20 to 40 while the count is below CTRW_STALL_FAILS, then 60 to 90 */
        c.active = 0;
        c.fails = 0;
        CtrWildsCore_SpawnDone(&c, 0);
        RangeAdd(&fail, c.spawnTimer);
        c.fails = CTRW_STALL_FAILS - 2;
        CtrWildsCore_SpawnDone(&c, 0);         /* the 5th in a row: not stalled yet */
        CHECK(!CtrWildsCore_Stalled(&c) && c.spawnTimer >= CTRW_FAIL_MIN && c.spawnTimer < CTRW_FAIL_MIN + CTRW_FAIL_SPAN,
              "the 5th failure is still a quick retry (%d)", c.spawnTimer);
        CtrWildsCore_SpawnDone(&c, 0);         /* the 6th: stalled */
        CHECK(CtrWildsCore_Stalled(&c), "the 6th failure in a row stalls");
        RangeAdd(&stalled, c.spawnTimer);
        /* a wild went away while the wait was long: cut to 60..119 */
        c.spawnTimer = 150;
        CtrWildsCore_Removed(&c);
        RangeAdd(&removed, c.spawnTimer);
    }
    CheckRange(&first, 24, 36, "first wild after a new map");
    CheckRange(&fill, 45, 90, "success, below half of the target");
    CheckRange(&topup, 90, 150, "success, half of the target or more");
    CheckRange(&fail, 20, 40, "failure");
    CheckRange(&stalled, 60, 90, "failure while stalled");
    CheckRange(&removed, 60, 119, "wait after a wild went away");
    /* the limit of 'below half' for the caps of both consoles (active counted after the wild was made) */
    {
        CtrWildsCore c;
        int lo = 1000, hi = 0;
        static const struct { int target, active, fill; } rows[] = {
            {2, 1, 0}, {4, 1, 1}, {4, 2, 0}, {6, 1, 1}, {6, 2, 1}, {6, 3, 0}, {6, 4, 0}, {6, 6, 0}, {1, 1, 0}, {3, 1, 1}, {3, 2, 0},
        };

        for (unsigned i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i)
            for (int seed = 1; seed <= 500; ++seed)
            {
                CtrWildsCore_Init(&c, (uint32_t)seed);
                c.target = (uint8_t)rows[i].target;
                c.active = (uint8_t)rows[i].active;
                CtrWildsCore_SpawnDone(&c, 1);
                if (rows[i].fill)
                    CHECK(c.spawnTimer >= 45 && c.spawnTimer <= 90, "target %d, %d out: fill wait (%d)", rows[i].target, rows[i].active, c.spawnTimer);
                else
                    CHECK(c.spawnTimer >= 90 && c.spawnTimer <= 150, "target %d, %d out: complete wait (%d)", rows[i].target, rows[i].active, c.spawnTimer);
                lo = c.spawnTimer < lo ? c.spawnTimer : lo;
                hi = c.spawnTimer > hi ? c.spawnTimer : hi;
            }
        CHECK(lo >= 45 && hi <= 150, "no wait outside 45..150 after a success (%d..%d)", lo, hi);
    }
    /* the new map's wait comes from the table's own RNG: it repeats with the seed (the game's generator takes no part) */
    {
        CtrWildsCore a, b;

        CtrWildsCore_Init(&a, 5);
        CtrWildsCore_Init(&b, 5);
        CtrWildsCore_MapChanged(&a, 1);
        CtrWildsCore_MapChanged(&b, 1);
        CHECK(a.spawnTimer == b.spawnTimer && a.rng == b.rng, "the first wait is deterministic from the table's seed");
        CtrWildsCore_MapChanged(&a, 1);
        CHECK(a.rng == b.rng, "the same map again draws nothing");
    }
    /* at the target the wait does not run down */
    {
        CtrWildsCore c;
        int before;

        CtrWildsCore_Init(&c, 3);
        CtrWildsCore_MapChanged(&c, 1);
        c.operative = 1;
        c.target = 2;
        c.active = 2;
        before = c.spawnTimer;
        for (int i = 0; i < 100; ++i)
            CtrWildsCore_SpawnDue(&c);
        CHECK(c.spawnTimer == before, "at the target the wait does not run down (%d)", c.spawnTimer);
    }
}

/* Taking a wild away brings the next one forward. */
static void TestRemoved(void)
{
    CtrWildsCore c;
    World w;
    int worst = 0, rangeOk = 1;

    CtrWildsCore_Init(&c, 1);
    CtrWildsCore_MapChanged(&c, 2);
    /* longer waits are cut, shorter ones are never made longer */
    for (int t = 0; t <= 400; ++t)
    {
        uint32_t rngBefore;

        c.spawnTimer = (uint16_t)t;
        rngBefore = c.rng;
        CtrWildsCore_Removed(&c);
        if (t <= 119)
            rangeOk &= c.spawnTimer == t && c.rng == rngBefore;
        else
            rangeOk &= c.spawnTimer >= 60 && c.spawnTimer <= 119;
    }
    CHECK(rangeOk, "Removed: 0..119 stay as they are (no draw), longer ones become 60..119");
    /* idempotent: several wilds away at once cut the wait once */
    CtrWildsCore_Init(&c, 2);
    c.spawnTimer = 400;
    for (int i = 0; i < 3; ++i)
        Make(&c, i, 0, 100);
    CtrWildsCore_Forget(&c, 0);
    {
        uint16_t t1 = c.spawnTimer;
        uint32_t rng1 = c.rng;

        CtrWildsCore_Forget(&c, 1);
        CtrWildsCore_Forget(&c, 2);
        CHECK(t1 >= 60 && t1 <= 119 && c.spawnTimer == t1 && c.rng == rng1 && c.active == 0, "Forget in a row: the wait is cut once (%d, %d)", t1, c.spawnTimer);
    }
    c.spawnTimer = 400;
    CtrWildsCore_Forget(&c, 0);             /* an empty slot: no wild went away */
    CtrWildsCore_Forget(&c, -1);
    CtrWildsCore_Forget(&c, CTRW_SLOTS);
    CHECK(c.spawnTimer == 400, "Forget of an empty or out-of-range slot leaves the wait alone");
    /* in play: a full table, one goes away, the next is out within 120 frames */
    for (int seed = 1; seed <= 200; ++seed)
    {
        int frames = 0;

        CtrWildsCore_Init(&c, (uint32_t)seed);
        WorldInit(&w, CTRW_TILE_LAND);
        MapOn(&c, 6, 1, 0);
        for (int i = 0; i < 3000 && c.active < 4; ++i)
            Frame(&c, &w, 2, 6, 0);
        CHECK(c.active == 4, "seed %d: the table filled", seed);
        Shed(&c, &w, 1);
        CHECK(c.active == 3 && c.spawnTimer <= 119, "seed %d: the wait is at most 119 after a removal (%d)", seed, c.spawnTimer);
        while (c.active < 4 && frames < 1000)
        {
            Frame(&c, &w, 2, 6, 0);
            ++frames;
        }
        worst = frames > worst ? frames : worst;
    }
    CHECK(worst <= CTRW_REMOVED_MIN + CTRW_REMOVED_SPAN + 1, "a removed wild is replaced within 120 frames (worst %d)", worst);
}

/* Quick retries must not delay the game's own encounters: the stall is reached within about 6 x 40 frames of the first failure. */
static void TestFastRetry(void)
{
    int worstStall = 0, worstBack = 0;

    for (int seed = 1; seed <= 300; ++seed)
    {
        CtrWildsCore c;
        World w;
        int firstFail = -1, released = -1, back = -1, i;

        CtrWildsCore_Init(&c, (uint32_t)seed);
        WorldPatch(&w, 3);                  /* nowhere to appear */
        MapOn(&c, 40, 1, 0);
        for (i = 0; i < 3000 && released < 0; ++i)
        {
            Frame(&c, &w, 2, 6, 0);
            if (firstFail < 0 && c.fails > 0)
                firstFail = i;
            if (c.fails > 0 && c.fails < CTRW_STALL_FAILS)
                CHECK(CtrWildsCore_SuppressStep(&c, CTRW_TILE_LAND), "seed %d: still suppressing before the 6th failure", seed);
            if (!CtrWildsCore_SuppressStep(&c, CTRW_TILE_LAND))
                released = i;
        }
        CHECK(firstFail >= 0 && released >= 0 && c.fails >= CTRW_STALL_FAILS, "seed %d: it stalled", seed);
        CHECK(released - firstFail <= CTRW_STALL_FAILS * (CTRW_FAIL_MIN + CTRW_FAIL_SPAN - 1), "seed %d: released %d frames after the first failure", seed,
              released - firstFail);
        worstStall = released - firstFail > worstStall ? released - firstFail : worstStall;
        /* the retries go on at 60..90; room appears and the first success hides the step encounters again at once */
        WorldAllLand(&w);
        for (i = 0; i < 400 && back < 0; ++i)
        {
            Frame(&c, &w, 2, 6, 0);
            if (c.active > 0)
                back = i;
        }
        CHECK(back >= 0 && back <= CTRW_STALLED_MIN + CTRW_STALLED_SPAN, "seed %d: recovered in %d frames", seed, back);
        CHECK(c.fails == 0 && !CtrWildsCore_Stalled(&c) && CtrWildsCore_SuppressStep(&c, CTRW_TILE_LAND), "seed %d: the first success suppresses again", seed);
        worstBack = back > worstBack ? back : worstBack;
        /* a new map leaves the stall too */
        WorldPatch(&w, 3);
        MapOn(&c, 41, 1, 0);
        CHECK(c.fails == 0 && !CtrWildsCore_Stalled(&c), "seed %d: a new map clears the failures", seed);
    }
    CHECK(worstStall <= 240, "the stall is at most 240 frames after the first failure (worst %d)", worstStall);
    printf("  fast retries: released at most %d frames after the first failure; recovery at most %d frames after room appears\n", worstStall, worstBack);
}

/* The ring is drawn in proportion to its zones ------------------------------------------------------------------------------ */

static int sPickCalls;

static int CountAll(void *ctx, int x, int y)
{
    (void)ctx;
    (void)x;
    (void)y;
    ++sPickCalls;
    return CTRW_TILE_LAND;
}

static int CountNone(void *ctx, int x, int y)
{
    (void)ctx;
    (void)x;
    (void)y;
    ++sPickCalls;
    return CTRW_TILE_NONE;
}

static int InZone(const CtrWildRing *r, int z, int x, int y)
{
    if (z == 0)
        return x >= r->left && x <= r->px - r->halfW - 1 && y >= r->top && y <= r->bottom;
    if (z == 1)
        return x >= r->px + r->halfW + 1 && x <= r->right && y >= r->top && y <= r->bottom;
    return x >= r->left && x <= r->right && y >= r->py + r->halfH + 1 && y <= r->bottom;
}

static int Groups(const CtrWildRing *r, int x, int y)
{
    return InZone(r, 0, x, y) | InZone(r, 1, x, y) << 1 | InZone(r, 2, x, y) << 2;
}

/* 200000 picks over a tile function that takes any cell. Each cell weighs the number of zones that hold it (the bottom corners are in
 * two), so the share of a group of cells (the same zones) is its weight over the total. Each group is within 2% of its share. */
static void CheckRing(const CtrWildRing *r, const char *name)
{
    enum { N = 200000, GROUPS = 8 };
    CtrWildsCore c;
    long cells[GROUPS] = {0}, seen[GROUPS] = {0}, total = 0;
    int bad = 0, outside = 0, x, y, kind;

    CtrWildsCore_Init(&c, 99);
    for (int cy = r->top - 2; cy <= r->bottom + 2; ++cy)
        for (int cx = r->left - 2; cx <= r->right + 2; ++cx)
        {
            int g = Groups(r, cx, cy);

            ++cells[g];
            total += (g & 1) + ((g >> 1) & 1) + ((g >> 2) & 1);
        }
    cells[0] = 0;
    for (int i = 0; i < N; ++i)
    {
        int g;

        if (!CtrWildsCore_PickCell(&c, r, CountAll, NULL, 1, 0, 0, &x, &y, &kind))
        {
            ++bad;
            continue;
        }
        g = Groups(r, x, y);
        if (g == 0)
        {
            ++bad;
            continue;
        }
        ++seen[g];
        outside += !(x < r->px - r->halfW || x > r->px + r->halfW || y > r->py + r->halfH);
    }
    CHECK(bad == 0 && outside == 0 && total > 0, "%s: every pick is in the ring, outside the screen (%d bad, %d inside the screen)", name, bad, outside);
    for (int g = 1; g < GROUPS; ++g)
    {
        int mult = (g & 1) + ((g >> 1) & 1) + ((g >> 2) & 1);
        double expect = (double)cells[g] * mult / (double)total, got = (double)seen[g] / N;

        CHECK(got > expect - 0.02 && got < expect + 0.02, "%s: group %d holds %.3f of the picks, expected %.3f", name, g, got, expect);
    }
}

static void TestPick(void)
{
    World w;
    CtrWildRing r;
    CtrWildsCore c;
    int x = 0, y = 0, kind = 0;

    WorldInit(&w, CTRW_TILE_LAND);
    r = Ring(&w);                           /* the left zone is 3x11 cells, the right one 4x11, the bottom one 24x4 */
    CheckRing(&r, "the usual ring");
    r = Ring(&w);
    r.bottom = r.py + r.halfH;              /* no bottom zone: only the sides, 3 columns to 4 */
    CheckRing(&r, "no bottom zone");
    {
        long left = 0, right = 0;

        CtrWildsCore_Init(&c, 7);
        for (int i = 0; i < 100000; ++i)
        {
            int ok = CtrWildsCore_PickCell(&c, &r, CountAll, NULL, 1, 0, 0, &x, &y, &kind);

            CHECK(ok, "no bottom zone: a pick");
            left += x < r.px;
            right += x > r.px;
        }
        CHECK(left > 100000 * 3 / 7.0 - 1500 && left < 100000 * 3 / 7.0 + 1500 && left + right == 100000, "weights: left 3 : right 4 (%ld : %ld)", left, right);
    }
    r = Ring(&w);
    r.left = r.px - r.halfW;                /* no left zone */
    CheckRing(&r, "no left zone");
    r = Ring(&w);
    r.right = r.px + r.halfW;               /* no right zone */
    CheckRing(&r, "no right zone");
    r = Ring(&w);
    r.left = r.px - r.halfW;
    r.right = r.px + r.halfW;               /* only the bottom */
    CheckRing(&r, "only the bottom zone");
    r = Ring(&w);
    r.left = r.px;
    r.right = r.px;                         /* a view narrower than the screen: only the bottom, one column */
    CheckRing(&r, "a one-column view");
    /* an empty ring: no cell, and no tile asked for */
    r = Ring(&w);
    r.left = r.px + 50;
    r.right = r.px + 5;
    sPickCalls = 0;
    CtrWildsCore_Init(&c, 8);
    CHECK(!CtrWildsCore_PickCell(&c, &r, CountAll, NULL, 1, 0, 0, &x, &y, &kind) && sPickCalls == 0, "an empty ring: nothing, no lookup");
    r = Ring(&w);
    r.bottom = r.py + 3;
    r.top = r.bottom + 1;
    sPickCalls = 0;
    CHECK(!CtrWildsCore_PickCell(&c, &r, CountAll, NULL, 1, 0, 0, &x, &y, &kind) && sPickCalls == 0, "no rows in the view: nothing, no lookup");
    memset(&r, 0, sizeof(r));
    sPickCalls = 0;
    CHECK(!CtrWildsCore_PickCell(&c, &r, CountAll, NULL, 1, 0, 0, &x, &y, &kind) && sPickCalls == 0, "an all-zero ring: nothing, no lookup");
    /* at most CTRW_PICK_TRIES lookups, and exactly that many when nothing is accepted */
    r = Ring(&w);
    CHECK(CTRW_PICK_TRIES == 24, "24 tries");
    for (int i = 0; i < 100; ++i)
    {
        sPickCalls = 0;
        CHECK(!CtrWildsCore_PickCell(&c, &r, CountNone, NULL, 1, 0, 0, &x, &y, &kind) && sPickCalls == CTRW_PICK_TRIES, "no cell accepted: %d lookups", sPickCalls);
    }
    sPickCalls = 0;
    CHECK(CtrWildsCore_PickCell(&c, &r, CountAll, NULL, 1, 0, 0, &x, &y, &kind) && sPickCalls == 1 && kind == CTRW_TILE_LAND, "the first cell taken: one lookup");
    /* water cells are never taken by a land-only table, and the lookups stay at 24 */
    sPickCalls = 0;
    CHECK(!CtrWildsCore_PickCell(&c, &r, CountAll, NULL, 0, 1, 0, &x, &y, &kind) && sPickCalls == CTRW_PICK_TRIES, "land cells with a water-only table: 24 lookups, none taken");
    /* huge or odd rings: no overflow, no division by zero, and the cell is in the ring */
    r = Ring(&w);
    r.left = -1000000000;
    r.right = 1000000000;
    r.top = -1000000000;
    r.bottom = 1000000000;
    for (int i = 0; i < 1000; ++i)
    {
        int ok = CtrWildsCore_PickCell(&c, &r, CountAll, NULL, 1, 0, 0, &x, &y, &kind);

        CHECK(ok && x >= r.left && x <= r.right && y >= r.top && y <= r.bottom, "a huge ring: a cell inside it (%d,%d)", x, y);
    }
    r = Ring(&w);
    r.px = 2147483000;
    r.left = 2147482900;
    r.right = 2147483600;
    r.top = 2147482000;
    r.bottom = 2147483600;
    for (int i = 0; i < 100; ++i)
        CtrWildsCore_PickCell(&c, &r, CountAll, NULL, 1, 0, 0, &x, &y, &kind);
    CHECK(1, "a ring near INT_MAX runs");
}

/* The diagnostics ------------------------------------------------------------------------------------------------------------ */

/* A table with the diagnostics on (the glue turns them on when port.log is on). */
static void InitOn(CtrWildsCore *c, uint32_t seed)
{
    CtrWildsCore_Init(c, seed);
    CtrWildsCore_SetStats(c, 1);
}

static void TestStats(void)
{
    CtrWildsCore c;
    World w;
    char line[160], small[16];
    int due = 0, firstStat = 0, fourth = 0;
    CtrWildsStats zero;

    memset(&zero, 0, sizeof(zero));
    /* a normal run: the first wild comes at the counted frame, the target is reached at the 4th, every attempt is told apart */
    InitOn(&c, 21);
    WorldInit(&w, CTRW_TILE_LAND);
    MapOn(&c, 0x0102, 1, 0);
    CHECK(memcmp(&c.stats, &zero, sizeof(zero)) == 0, "stats start at zero");
    for (int i = 0; i < 6000; ++i)
    {
        int before = c.active;

        Frame(&c, &w, 2, 6, 0);
        if (c.active > before && c.active == 1)
            firstStat = (int)c.stats.frames;
        if (c.active > before && c.active == 4)
            fourth = (int)c.stats.frames;
        due += CtrWildsCore_StatsDue(&c);
    }
    CHECK(c.stats.ok == 4 && c.stats.attempts == c.stats.ok + c.stats.fail[CTRW_FAIL_NOCELL] + c.stats.fail[CTRW_FAIL_ROLL] + c.stats.fail[CTRW_FAIL_NOOBJ],
          "attempts add up (%u, %u ok)", c.stats.attempts, c.stats.ok);
    CHECK(c.stats.frames == 6000 && c.stats.firstFrames == (uint32_t)firstStat && c.stats.fillFrames == (uint32_t)fourth && firstStat > 0 && fourth >= firstStat,
          "first wild at %u frames, target at %u (%d, %d)", c.stats.firstFrames, c.stats.fillFrames, firstStat, fourth);
    CHECK(c.stats.firstFrames >= CTRW_FIRST_MIN + 1 && c.stats.firstFrames <= CTRW_FIRST_MIN + CTRW_FIRST_SPAN, "first=%u is 25..37 frames", c.stats.firstFrames);
    CHECK(due == 4, "a summary at the first attempt and then every 1800 frames (%d in 6000)", due);
    CtrWildsCore_StatsLine(&c, line, sizeof(line));
    printf("  stats line: %s\n", line);
    CHECK(strncmp(line, "wilds: map=0102 att=", 20) == 0 && strstr(line, " ok=4 ") && strstr(line, " stalls=0 t=6000"), "the line: %s", line);
    /* failures by reason */
    InitOn(&c, 22);
    CtrWildsCore_MapChanged(&c, 1);
    CtrWildsCore_NoteFail(&c, CTRW_FAIL_NOCELL);
    CtrWildsCore_SpawnDone(&c, 0);
    CtrWildsCore_NoteFail(&c, CTRW_FAIL_ROLL);
    CtrWildsCore_NoteFail(&c, CTRW_FAIL_ROLL);
    CtrWildsCore_SpawnDone(&c, 0);
    CtrWildsCore_NoteFail(&c, CTRW_FAIL_NOOBJ);
    CtrWildsCore_NoteFail(&c, 77);          /* an unknown reason is ignored */
    CtrWildsCore_NoteFail(&c, -1);
    CtrWildsCore_SpawnDone(&c, 0);
    CHECK(c.stats.attempts == 3 && c.stats.ok == 0 && c.stats.fail[0] == 1 && c.stats.fail[1] == 2 && c.stats.fail[2] == 1 && c.stats.stalls == 0, "reasons are counted apart");
    CtrWildsCore_SpawnDone(&c, 0);
    CtrWildsCore_SpawnDone(&c, 0);
    CtrWildsCore_SpawnDone(&c, 0);
    CHECK(CtrWildsCore_Stalled(&c) && c.stats.stalls == 1, "the stall is counted once (%u)", c.stats.stalls);
    CtrWildsCore_SpawnDone(&c, 0);
    CHECK(c.stats.stalls == 1, "...not again while stalled");
    CtrWildsCore_StatsLine(&c, line, sizeof(line));
    CHECK(strcmp(line, "wilds: map=0001 att=7 ok=0 nocell=1 roll=2 noobj=1 first=0 fill=0 stalls=1 t=0") == 0, "exact line: %s", line);
    /* a new map starts over; the same map does not */
    CtrWildsCore_MapChanged(&c, 1);
    CHECK(c.stats.attempts == 7, "the same map keeps the counters");
    CtrWildsCore_MapChanged(&c, 2);
    CHECK(memcmp(&c.stats, &zero, sizeof(zero)) == 0, "a new map resets them");
    CHECK(!CtrWildsCore_StatsDue(&c), "no summary without attempts");
    /* a short buffer is always terminated */
    CtrWildsCore_StatsLine(&c, small, sizeof(small));
    CHECK(strlen(small) == 15 && strncmp(small, "wilds: map=0002", 15) == 0, "truncated to the buffer: '%s'", small);
    small[0] = 'x';
    CHECK(CtrWildsCore_StatsLine(&c, small, 1) == 0 && small[0] == 0, "a one-byte buffer holds the terminator");
    small[0] = 'x';
    CHECK(CtrWildsCore_StatsLine(&c, small, 0) == 0 && small[0] == 'x', "a zero-size buffer is not touched");
    /* the system off or with nothing to show counts nothing: no frame, no attempt */
    InitOn(&c, 23);
    WorldInit(&w, CTRW_TILE_LAND);
    MapOn(&c, 3, 1, 0);
    for (int i = 0; i < 5000; ++i)
        Frame(&c, &w, 0, 6, 0);             /* WILD POKEMON off: not operative */
    CHECK(memcmp(&c.stats, &zero, sizeof(zero)) == 0 && c.active == 0, "the option off: not one count");
    for (int i = 0; i < 5000; ++i)
        Frame(&c, &w, 2, 6, 1);             /* blocked (Safari, Randomlocke...) */
    CHECK(memcmp(&c.stats, &zero, sizeof(zero)) == 0, "blocked: not one count");
    for (int i = 0; i < 5000; ++i)
        Frame(&c, &w, 2, 0, 0);             /* a console cap of 0 */
    CHECK(memcmp(&c.stats, &zero, sizeof(zero)) == 0, "no cap: not one count");
    for (int i = 0; i < 5000; ++i)
        GlueIdle(&c, 0);                    /* the glue's early out */
    CHECK(memcmp(&c.stats, &zero, sizeof(zero)) == 0, "Idle: not one count");
    InitOn(&c, 24);
    MapOn(&c, 4, 0, 0);
    c.probed = 1;                           /* a map that cannot show a wild: Unsupported is the glue's early out */
    for (int i = 0; i < 5000; ++i)
        GlueIdle(&c, 2);
    CHECK(memcmp(&c.stats, &zero, sizeof(zero)) == 0, "Unsupported: not one count");
    /* target 0 is not a fill */
    InitOn(&c, 25);
    CtrWildsCore_MapChanged(&c, 5);
    c.operative = 1;
    c.target = 0;
    for (int i = 0; i < 100; ++i)
        CtrWildsCore_SpawnDue(&c);
    CHECK(c.stats.frames == 100 && c.stats.fillFrames == 0 && !c.stats.gotFill, "target 0 is not a fill");
    /* a map with no room: only no-cell failures, one stall, no first wild */
    InitOn(&c, 26);
    WorldPatch(&w, 3);
    MapOn(&c, 6, 1, 0);
    for (int i = 0; i < 3000; ++i)
        Frame(&c, &w, 2, 6, 0);
    CHECK(c.stats.ok == 0 && c.stats.stalls == 1 && c.stats.fail[CTRW_FAIL_NOCELL] == c.stats.attempts && c.stats.firstFrames == 0 && c.stats.fillFrames == 0,
          "a map with no room: only no-cell failures, one stall (%u attempts)", c.stats.attempts);
    /* the species roll failing is told apart */
    sRollFails = 1;
    InitOn(&c, 27);
    WorldInit(&w, CTRW_TILE_LAND);
    MapOn(&c, 7, 1, 0);
    for (int i = 0; i < 1000; ++i)
        Frame(&c, &w, 2, 6, 0);
    sRollFails = 0;
    CHECK(c.stats.attempts > 0 && c.stats.fail[CTRW_FAIL_ROLL] == c.stats.attempts && c.stats.fail[CTRW_FAIL_NOCELL] == 0, "roll failures are told apart");
    /* no free object event: told apart too */
    InitOn(&c, 28);
    WorldInit(&w, CTRW_TILE_LAND);
    MapOn(&c, 8, 1, 0);
    for (int i = 0; i < CTRW_OBJECTS; ++i)
        w.objActive[i] = 1;
    w.objActive[0] = 1;
    c.operative = 1;
    c.target = 2;
    c.spawnTimer = 0;
    CtrWildsCore_SpawnDue(&c);
    Spawn(&c, &w, 6, 10, 100, 0);
    CHECK(c.stats.fail[CTRW_FAIL_NOOBJ] == 1 && c.stats.attempts == 1, "no object event is told apart (%u, %u)", c.stats.fail[CTRW_FAIL_NOOBJ], c.stats.attempts);
}

/* The counters follow port.log ---------------------------------------------------------------------------------------------- */

/* The glue's logging, as the game does it (CtrWild_LogStats, CtrWild_NewMap, CtrWild_Refresh), with the formatter and the log call counted. */
typedef struct Driver
{
    int logOn;                  /* what CtrWilds_LogOn() answers */
    int formatCalls, logCalls;  /* CtrWildsCore_StatsLine calls, CtrWilds_Log calls */
    char last[160];
    uint64_t hash;              /* of the table's state after each frame: the spawn behaviour */
    int spawns;                 /* wilds made */
} Driver;

static void DriverLogStats(CtrWildsCore *c, Driver *d)
{
    char line[112];

    if (!c->statsOn)
        return;
    ++d->formatCalls;
    CtrWildsCore_StatsLine(c, line, sizeof(line));
    ++d->logCalls;
    snprintf(d->last, sizeof(d->last), "%s", line);
}

static void HashMix(uint64_t *h, uint64_t v)
{
    *h = (*h ^ v) * 1099511628211ull;
}

/* One frame of CtrWilds_Update in the glue's order: a new map (the summary of the map left, MapChanged, SetStats), the frame setting, the
 * 30-frame pass (SetStats, shed, the periodic summary) and the spawn. */
static void DriverFrame(CtrWildsCore *c, World *w, Driver *d, uint16_t map, int density, int cap)
{
    CtrWildEnv env = {density, c->aggro, cap, 0, c->leadPower};
    int shed, before;

    if (!(c->mapKnown && c->map == map))
    {
        if (c->statsOn && c->stats.attempts != 0)
            DriverLogStats(c, d);
        CtrWildsCore_MapChanged(c, map);
        CtrWildsCore_SetStats(c, d->logOn);
        c->mapLand = 1;
        c->mapWater = 0;
        c->probed = 1;
    }
    if (CtrWildsCore_Frame(c, &env))
        Shed(c, w, c->active);
    else if (c->operative)
    {
        shed = CtrWildsCore_Tick(c, density, cap, FreeObjects, w);
        if (shed >= 0)
        {
            CtrWildsCore_SetStats(c, d->logOn);
            if (shed > 0)
                Shed(c, w, shed);
            if (CtrWildsCore_StatsDue(c))
                DriverLogStats(c, d);
        }
        before = c->active;
        Spawn(c, w, cap, 10, 100, 0);
        d->spawns += c->active > before;
    }
    HashMix(&d->hash, c->active);
    HashMix(&d->hash, c->spawnTimer);
    HashMix(&d->hash, c->rng);
    HashMix(&d->hash, c->fails);
    HashMix(&d->hash, c->target);
    for (int i = 0; i < CTRW_SLOTS; ++i)
    {
        HashMix(&d->hash, c->wild[i].state);
        HashMix(&d->hash, c->wild[i].personality);
        HashMix(&d->hash, c->wild[i].obj);
    }
}

static int StatsZero(const CtrWildsCore *c)
{
    CtrWildsStats zero;

    memset(&zero, 0, sizeof(zero));
    return memcmp(&c->stats, &zero, sizeof(zero)) == 0;
}

/* The whole run: three maps of 2000 frames, with the log state given per frame by the pattern (0 off, 1 on, 2 changing every 700 frames). */
static void DriverRun(int pattern, Driver *d)
{
    CtrWildsCore c;
    World w;

    memset(d, 0, sizeof(*d));
    d->hash = 1469598103934665603ull;
    CtrWildsCore_Init(&c, 4242);
    WorldInit(&w, CTRW_TILE_LAND);
    for (int f = 0; f < 6000; ++f)
    {
        uint16_t map = (uint16_t)(0x0100 + f / 2000);

        d->logOn = pattern == 1 || (pattern == 2 && (f / 700) % 2 == 1);
        DriverFrame(&c, &w, d, map, 2, 6);
        if (pattern == 0)
            CHECK(c.statsOn == 0 && StatsZero(&c), "log off: frame %d: no counter moved", f);
    }
}

static void TestStatsSwitch(void)
{
    Driver off, on, tog;
    CtrWildsCore c;
    char buf[64];

    /* the switch itself */
    CtrWildsCore_Init(&c, 1);
    CHECK(c.statsOn == 0 && StatsZero(&c), "a new table: counters off, zero");
    CtrWildsCore_SetStats(&c, 5);
    CHECK(c.statsOn == 1, "any non-zero value means on");
    c.stats.attempts = 3;
    c.stats.frames = 99;
    CtrWildsCore_SetStats(&c, 1);
    CHECK(c.stats.attempts == 3 && c.stats.frames == 99, "on again: nothing is cleared");
    CtrWildsCore_MapChanged(&c, 7);
    CHECK(c.statsOn == 1 && StatsZero(&c), "a new map clears the counters and leaves the switch on");
    c.stats.attempts = 3;
    CtrWildsCore_SetStats(&c, 0);
    CHECK(c.statsOn == 0 && StatsZero(&c), "off: nothing is left behind");
    CtrWildsCore_SetStats(&c, 0);
    CHECK(c.statsOn == 0 && StatsZero(&c), "off again: no change");
    /* off: the core's own guards (the glue does not even call the formatter) */
    c.operative = 1;
    c.target = 4;
    for (int i = 0; i < 100; ++i)
        CtrWildsCore_SpawnDue(&c);
    CtrWildsCore_NoteFail(&c, CTRW_FAIL_NOCELL);
    CtrWildsCore_SpawnDone(&c, 1);
    CtrWildsCore_SpawnDone(&c, 0);
    CHECK(StatsZero(&c), "off: SpawnDue, NoteFail and SpawnDone count nothing");
    CHECK(c.spawnTimer >= CTRW_FAIL_MIN, "...but the wait is set as always (%d)", c.spawnTimer);
    CHECK(c.fails == 1, "...and the failures in a row are still counted (they decide the stall)");
    strcpy(buf, "unchanged");
    CHECK(CtrWildsCore_StatsLine(&c, buf, sizeof(buf)) == 0 && buf[0] == 0, "off: the line is empty");
    CHECK(!CtrWildsCore_StatsDue(&c), "off: no summary is ever due");
    c.stats.attempts = 5;       /* even with a stale counter */
    c.stats.nextLog = 0;
    CHECK(!CtrWildsCore_StatsDue(&c), "off: no summary is due even with attempts");

    /* port.log off for the whole run: not one counter moves, the formatter and the log are never called */
    DriverRun(0, &off);
    CHECK(off.formatCalls == 0 && off.logCalls == 0, "log off: %d formatter calls and %d log calls, none expected", off.formatCalls, off.logCalls);
    CHECK(off.spawns > 0, "log off: the wilds still appear (%d)", off.spawns);

    /* port.log on: a summary when a map is left and every 1800 operative frames */
    DriverRun(1, &on);
    CHECK(on.formatCalls == on.logCalls && on.logCalls >= 6 && on.logCalls <= 12, "log on: summaries written (%d)", on.logCalls);
    CHECK(strncmp(on.last, "wilds: map=0102 att=", 20) == 0, "the last line is of the map being played: %s", on.last);

    /* the log's state changes mid-run: off, on, off, on */
    DriverRun(2, &tog);
    CHECK(tog.logCalls > 0 && tog.formatCalls == tog.logCalls, "toggled: some summaries (%d)", tog.logCalls);
    /* the spawn behaviour is the same in all three: the same state after each frame (active, wait, rng, wilds), so the same spawns */
    CHECK(off.hash == on.hash && on.hash == tog.hash, "the same frame-by-frame state with the log off, on and toggled");
    CHECK(off.spawns == on.spawns && on.spawns == tog.spawns, "the same spawns (%d, %d, %d)", off.spawns, on.spawns, tog.spawns);

    /* the transitions leave nothing half done: on at the pass, from zero; off at the pass, zero */
    {
        World w;
        Driver d;
        int f;

        memset(&d, 0, sizeof(d));
        CtrWildsCore_Init(&c, 77);
        WorldInit(&w, CTRW_TILE_LAND);
        d.logOn = 1;
        for (f = 0; f < 500; ++f)
            DriverFrame(&c, &w, &d, 0x0200, 2, 6);
        CHECK(c.statsOn == 1 && c.stats.attempts > 0 && c.stats.frames == 500, "on: counting from the map's first frame (%u frames)", (unsigned)c.stats.frames);
        CHECK(c.stats.attempts == c.stats.ok + c.stats.fail[0] + c.stats.fail[1] + c.stats.fail[2], "on: attempts add up");
        d.logOn = 0;
        for (f = 0; f < CTRW_TICK; ++f)
            DriverFrame(&c, &w, &d, 0x0200, 2, 6);
        CHECK(c.statsOn == 0 && StatsZero(&c), "off: at the next pass the counters are zero");
        for (f = 0; f < 200; ++f)
        {
            DriverFrame(&c, &w, &d, 0x0200, 2, 6);
            CHECK(StatsZero(&c), "off: still zero after %d frames", f);
        }
        d.logOn = 1;
        for (f = 0; f < CTRW_TICK; ++f)
            DriverFrame(&c, &w, &d, 0x0200, 2, 6);
        CHECK(c.statsOn == 1 && c.stats.frames > 0 && c.stats.frames <= (uint32_t)CTRW_TICK, "on again: counting from zero at the pass (%u frames)", (unsigned)c.stats.frames);
        for (f = 0; f < 3000; ++f)
            DriverFrame(&c, &w, &d, 0x0200, 2, 6);
        CHECK(c.stats.attempts == c.stats.ok + c.stats.fail[0] + c.stats.fail[1] + c.stats.fail[2], "on again: attempts add up (%u)", (unsigned)c.stats.attempts);
        CHECK(c.stats.firstFrames <= c.stats.frames && c.stats.fillFrames <= c.stats.frames && c.stats.nextLog <= c.stats.frames + CTRW_STATS_PERIOD,
              "on again: the frame marks are not ahead of the count");
    }
}

static void TestGlueSupport(void)
{
    CtrWildsCore c;
    CtrWildMon mon = {16, 5, 0, 0, 1, 100};

    /* the game's own table starts zeroed: its generator still works and slots are empty */
    memset(&gCtrWilds, 0, sizeof(gCtrWilds));
    CHECK(CtrWildsCore_Rand(&gCtrWilds) != CtrWildsCore_Rand(&gCtrWilds), "a zeroed table draws numbers");
    CHECK(gCtrWilds.active == 0 && gCtrWilds.wild[0].state == CTRW_S_FREE, "a zeroed table is empty");
    /* a map nothing can be shown on, once probed, has nothing to do; before the probe it is not known */
    CtrWildsCore_Init(&c, 91);
    CtrWildsCore_MapChanged(&c, 1);
    CHECK(!CtrWildsCore_Unsupported(&c), "not probed: not known to be empty");
    c.probed = 1;
    CHECK(CtrWildsCore_Unsupported(&c), "probed with no land and no water: unsupported");
    c.mapLand = 1;
    CHECK(!CtrWildsCore_Unsupported(&c), "land: supported");
    c.mapLand = 0;
    CtrWildsCore_Commit(&c, 0, &mon, 0, 0, 1);
    CHECK(!CtrWildsCore_Unsupported(&c), "a wild is out: it has to be removed first");
    CtrWildsCore_MapChanged(&c, 2);
    CHECK(c.probed == 0 && !CtrWildsCore_Unsupported(&c), "a new map is probed again");
    /* the roamer already out */
    CHECK(!CtrWildsCore_HasRoamer(&c), "no roamer out");
    mon.roamer = 1;
    CtrWildsCore_Commit(&c, 2, &mon, 0, 0, 1);
    CHECK(CtrWildsCore_HasRoamer(&c), "a roamer is out");
    CHECK(!CtrWildsCore_RollRoamer(&c, 1, CtrWildsCore_HasRoamer(&c)), "so no second one");
    /* a wild that has been out long, rests and is far goes; one that is seen, or busy, does not */
    c.wild[2].state = CTRW_S_IDLE;
    c.wild[2].age = 1500 + 600 + 1;
    c.wild[2].dist = 12;
    CHECK(!CtrWildsCore_Stale(&c, 2, 12) && CtrWildsCore_Stale(&c, 2, 9), "retired only beyond the screen");
    c.wild[2].age = 1500 + 600;
    CHECK(!CtrWildsCore_Stale(&c, 2, 9), "not before its time (1500 + 300 per slot)");
    c.wild[2].age = 5000;
    c.wild[2].state = CTRW_S_CHASE;
    CHECK(!CtrWildsCore_Stale(&c, 2, 9), "a chase is never retired");
    c.wild[2].state = CTRW_S_WANDER;
    CHECK(CtrWildsCore_Stale(&c, 2, 9) && !CtrWildsCore_Stale(&c, 9, 9) && !CtrWildsCore_Stale(&c, -1, 9), "wandering is; bad slots are not");
}

/* Micro-benchmarks: the pure logic's cost per frame on this host (not a console's). */
static double Now(void)
{
    struct timespec t;

    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1e9 + t.tv_nsec;
}

static int FakeFree(void *ctx)
{
    (void)ctx;
    return 12;
}

static int AlwaysLand(void *ctx, int x, int y)
{
    (void)ctx;
    (void)x;
    (void)y;
    return CTRW_TILE_LAND;
}

/* SpawnDue without any counter, and the same with the byte test, both in this file (same translation unit, same call), so that the
 * difference between them is the byte test and the increment and nothing else. */
__attribute__((noinline)) static int OldSpawnDue(CtrWildsCore *c)
{
    if (!c->operative || c->active >= c->target)
        return 0;
    if (c->spawnTimer != 0)
    {
        --c->spawnTimer;
        return 0;
    }
    return 1;
}

__attribute__((noinline)) static int CopySpawnDue(CtrWildsCore *c)
{
    if (!c->operative)
        return 0;
    if (c->statsOn)
        ++c->stats.frames;
    if (c->active >= c->target)
        return 0;
    if (c->spawnTimer != 0)
    {
        --c->spawnTimer;
        return 0;
    }
    return 1;
}

static void Bench(void)
{
    CtrWildsCore c;
    CtrWildSense s;
    CtrWildAct a;
    volatile uint32_t sink = 0;
    double t0, t1;
    const int N = 20000000;

    /* option OFF, nothing out: the early out */
    CtrWildsCore_Init(&c, 81);
    t0 = Now();
    for (int i = 0; i < N; ++i)
        sink += (uint32_t)CtrWildsCore_Idle(&c, i & 0);
    t1 = Now();
    printf("bench: option OFF, nothing out (early out): %.2f ns per frame\n", (t1 - t0) / N);
    /* option ON: six wilds thinking + the periodic check + the spawn due test */
    CtrWildsCore_Init(&c, 82);
    c.leadPower = 5000;
    c.mapLand = 1;
    c.target = 6;
    for (int i = 0; i < 6; ++i)
        Make(&c, i, i & 1, 5000);
    SenseAt(&s, 9, 7, 0, 0);
    t0 = Now();
    for (int i = 0; i < N / 10; ++i)
    {
        CtrWildEnv env = {3, 2, 6, 0, 5000};

        CtrWildsCore_Frame(&c, &env);
        CtrWildsCore_Tick(&c, 3, 6, FakeFree, NULL);
        CtrWildsCore_SpawnDue(&c);
        for (int k = 0; k < 6; ++k)
            CtrWildsCore_Think(&c, k, &s, &a);
        sink += (uint32_t)a.kind;
    }
    t1 = Now();
    printf("bench: option ON, six wilds out, at target (frame + tick + 6 thinks): %.1f ns per frame\n", (t1 - t0) / (N / 10));
    /* a spawn attempt: ten ring cells and a personality (done at most every 60 to 240 frames) */
    {
        CtrWildRing ring = {40, 30, 8, 6, 29, 52, 30, 40};
        int x, y, kind, ok = 0;

        CtrWildsCore_Init(&c, 83);
        t0 = Now();
        for (int i = 0; i < N / 10; ++i)
        {
            ok += CtrWildsCore_PickCell(&c, &ring, AlwaysLand, NULL, 1, 0, 0, &x, &y, &kind);
            sink += CtrWildsCore_Personality(&c, i % 25);
        }
        t1 = Now();
        printf("bench: one spawn attempt (cell + personality): %.1f ns each\n", (t1 - t0) / (N / 10));
        (void)ok;
    }
    /* the diagnostics' only per-frame cost: one byte tested (and, with port.log on, one increment) in SpawnDue (operative, below the target,
     * the wait running down). Two sets, in one file so that only the counter differs: copies of the function without and with the test,
     * and the core's own function with the counters off and on. */
    {
        const int M = 200000000;
        double tRef, tCopyOff, tCopyOn, tOff, tOn;

        CtrWildsCore_Init(&c, 84);
        c.operative = 1;
        c.target = 4;
        t0 = Now();
        for (int i = 0; i < M; ++i)
        {
            c.spawnTimer = 0xFFFF;
            sink += (uint32_t)OldSpawnDue(&c);
        }
        tRef = (Now() - t0) / M;
        for (int on = 0; on < 2; ++on)
        {
            CtrWildsCore_SetStats(&c, on);
            t0 = Now();
            for (int i = 0; i < M; ++i)
            {
                c.spawnTimer = 0xFFFF;
                sink += (uint32_t)CopySpawnDue(&c);
            }
            if (on)
                tCopyOn = (Now() - t0) / M;
            else
                tCopyOff = (Now() - t0) / M;
            t0 = Now();
            for (int i = 0; i < M; ++i)
            {
                c.spawnTimer = 0xFFFF;
                sink += (uint32_t)CtrWildsCore_SpawnDue(&c);
            }
            if (on)
                tOn = (Now() - t0) / M;
            else
                tOff = (Now() - t0) / M;
        }
        printf("bench: SpawnDue per frame, same file: no counter / byte tested, off / tested and incremented, on: %.2f / %.2f / %.2f ns\n", tRef, tCopyOff, tCopyOn);
        printf("bench: SpawnDue per frame, the core's own function, counters off / on: %.2f / %.2f ns (a host, as a call; not a console)\n", tOff, tOn);
        /* the same with the option off: the early out, and nothing at all reaches the counter */
        CtrWildsCore_Init(&c, 85);
        t0 = Now();
        for (int i = 0; i < M; ++i)
            sink += (uint32_t)(CtrWildsCore_Idle(&c, 0) || CtrWildsCore_Unsupported(&c));
        printf("bench: option OFF (Idle || Unsupported) with the counters present: %.2f ns per frame; frames counted: %u\n", (Now() - t0) / M,
               (unsigned)c.stats.frames);
    }
    (void)sink;
}

int main(void)
{
    TestSettings();
    TestBudget();
    TestObjectLimits();
    TestSpawn();
    TestMapsAndWater();
    TestSuppression();
    TestIdleReleases();
    TestStall();
    TestOrphans();
    TestRepel();
    TestAi();
    TestData();
    TestRoamer();
    TestIds();
    TestGlueSupport();
    TestRhythm();
    TestRemoved();
    TestFastRetry();
    TestPick();
    TestStats();
    TestStatsSwitch();
    printf("wilds_core_test: %d checks, %d failures\n", checks, failures);
    if (failures == 0)
        Bench();
    return failures != 0;
}
