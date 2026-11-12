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
    CHECK(first >= 60 && first < 60 + CTRW_TICK + 2, "the first wild comes after about a second (frame %d)", first);
    CHECK(appeared == 4, "each appeared once (%d)", appeared);
    /* the wilds stand in the ring outside the screen */
    for (int i = 0; i < CTRW_SLOTS; ++i)
        if (c.wild[i].state != CTRW_S_FREE)
        {
            int ox = w.objX[c.wild[i].obj], oy = w.objY[c.wild[i].obj];

            CHECK(ox < w.px - 8 || ox > w.px + 8 || oy > w.py + 6, "wild %d spawned inside the screen (%d,%d)", i, ox, oy);
            CHECK(ox >= w.px - 11 && ox <= w.px + 12 && oy >= w.py && oy <= w.py + 10, "wild %d spawned outside the object event view", i);
        }
    /* the rate: spawns come 90 to 239 frames apart (a failed attempt tries again sooner) */
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
        CHECK(n == 6 && minGap >= 90 && maxGap <= 240, "spawn rate: %d wilds, gaps %d..%d", n, minGap, maxGap);
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
    TestRepel();
    TestAi();
    TestData();
    TestRoamer();
    TestIds();
    TestGlueSupport();
    printf("wilds_core_test: %d checks, %d failures\n", checks, failures);
    if (failures == 0)
        Bench();
    return failures != 0;
}
