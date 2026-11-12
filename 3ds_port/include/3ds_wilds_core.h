/* VISIBLE WILD POKEMON, the pure part (3ds_wilds_core.c): the rules of the wild Pokemon that are seen walking in the overworld, with
 * no dependency on the game, so the host tests run them. The game's side (event_object_movement.c and wild_encounter.c, recipes
 * 0249 and 0250) is thin glue: it reads the map and the object events, calls this module and does what it answers.
 *
 * What is decided here: how many wilds may be out (the density, the console's cap and the object events that are free), where one
 * appears (a ring just outside the screen), what its temperament is, the state machine of each (idle, wander, alert, chase, flee,
 * encounter), when a Repel keeps it off, whether the normal step encounters give way to it, and the personality of the Pokemon the
 * battle will start with. Design ideas of overworld wild Pokemon after PR #83 by mcl5500/Sh3ld0r (MIT); the code is our own.
 *
 * Object events: FireRed has CTRW_OBJECTS of them. A wild takes one only while CTRW_RESERVE stay free, so the map's NPCs and objects
 * are never displaced; when fewer are free than that, wilds are shed (CtrWildsCore_Shed). The table keeps each wild's object event
 * index, so nothing scans the 16 object events per frame. */
#ifndef CTR_WILDS_CORE_H
#define CTR_WILDS_CORE_H

#include <stdint.h>

#define CTRW_SLOTS 6            /* the most wild Pokemon at once (New 3DS, density MANY) */
#define CTRW_OBJECTS 16         /* FireRed's OBJECT_EVENTS_COUNT */
#define CTRW_RESERVE 4          /* object events that stay free, always */
#define CTRW_NONE 0xFF          /* no object event / no slot */
#define CTRW_TICK 30            /* frames between two checks of the free object events */
#define CTRW_DENSITIES 4        /* OFF, FEW, NORMAL, MANY */
#define CTRW_AGGROS 5           /* OFF, RARE, LOW, MODERATE, HIGH */

/* The directions are the game's (constants/global.h). */
#define CTRW_DIR_NONE 0
#define CTRW_DIR_SOUTH 1
#define CTRW_DIR_NORTH 2
#define CTRW_DIR_WEST 3
#define CTRW_DIR_EAST 4

enum { CTRW_S_FREE = 0, CTRW_S_IDLE, CTRW_S_WANDER, CTRW_S_ALERT, CTRW_S_CHASE, CTRW_S_FLEE, CTRW_S_ENCOUNTER };
enum { CTRW_TEMPER_CALM = 0, CTRW_TEMPER_AGGRESSIVE = 1 };
enum { CTRW_TILE_NONE = 0, CTRW_TILE_LAND = 1, CTRW_TILE_WATER = 2 };
/* What a wild asks the game to do this frame. */
enum { CTRW_A_NONE = 0, CTRW_A_TURN, CTRW_A_STEP, CTRW_A_ALERT, CTRW_A_CHARGE, CTRW_A_LEAVE };

#define CTRW_NOTICE_AGGRESSIVE 4    /* an aggressive wild notices the player this close (tiles) */
#define CTRW_NOTICE_CALM 2          /* a calm one, only to run from a stronger Pokemon (4 when the player runs or bikes) */
#define CTRW_CHASE_RANGE 6          /* a chase goes on up to this far */

typedef struct CtrWild
{
    uint8_t state;              /* CTRW_S_*; FREE: the slot is empty */
    uint8_t obj;                /* its object event index, CTRW_NONE until the game made it */
    uint8_t temper;
    uint8_t after;              /* what the alert turns into: CTRW_S_CHASE or CTRW_S_FLEE */
    uint8_t water, roamer, shiny, gender;
    uint8_t level;
    uint8_t steps;              /* steps left of a walk, a flight or a chase */
    uint8_t dist;               /* its last distance to the player (tiles, the longer axis) */
    uint16_t species;
    uint16_t gfx;               /* the picture the game draws it with */
    uint16_t timer;             /* frames to the next decision */
    uint16_t cool;              /* frames before it may notice the player again */
    uint16_t age;               /* frames since it appeared (stops at 0xFFFF) */
    uint32_t personality;       /* the personality its battle Pokemon is made with: nature, gender and shininess are in it */
    uint32_t power;             /* level x the six base stats, for the strength comparison */
} CtrWild;

typedef struct CtrWildsCore
{
    CtrWild wild[CTRW_SLOTS];
    uint32_t rng;
    uint16_t spawnTimer;
    uint16_t tick;
    uint16_t map;               /* the map the table was made on */
    uint8_t mapKnown;
    uint8_t active;             /* wilds in the table */
    uint8_t mapLand, mapWater;  /* the map has a land / water table with a Pokemon that has a picture */
    uint8_t operative;          /* the system works on this map right now: it may hide the step encounters */
    uint8_t spawnable;          /* ... and a wild is out or one could appear (free object events, a cap) */
    uint8_t target;             /* how many wilds the budget allows */
    uint8_t aggro;              /* the AGGRESSIVE setting, 0 to 4 */
    uint32_t leadPower;         /* the lead Pokemon's level x base stats */
} CtrWildsCore;

/* What the option and the game say this frame. */
typedef struct CtrWildEnv
{
    int density;                /* 0 OFF to 3 MANY */
    int aggro;                  /* 0 OFF to 4 HIGH */
    int consoleCap;             /* 2 on an Old 3DS, 6 on a New 3DS */
    int blocked;                /* Safari Zone, Randomlocke, Union Room, quest log, encounters disabled */
    uint32_t leadPower;         /* the lead Pokemon's level x base stats (0: none) */
} CtrWildEnv;

/* One Pokemon, rolled once: what is seen is what the battle starts with. */
typedef struct CtrWildMon
{
    uint16_t species;
    uint8_t level;
    uint8_t water, roamer;
    uint32_t personality;
    uint32_t power;
} CtrWildMon;

/* What the game tells a wild's mind each frame. dx and dy: the player's tile minus the wild's. */
typedef struct CtrWildSense
{
    int dx, dy;
    uint8_t fast;               /* the player runs or bikes */
    uint8_t sameElev;
    uint8_t busy;               /* the wild is mid-step: it may not start another */
} CtrWildSense;

typedef struct CtrWildAct
{
    uint8_t kind;               /* CTRW_A_* */
    uint8_t dir[2];             /* a step: the direction to try, then the other one */
    uint8_t n;                  /* how many of dir[] are meant */
    uint8_t fast;               /* a step at the fast pace (a flight) */
} CtrWildAct;

/* The screen and the object event view around the player, in tiles (a ring just outside the screen is where a wild appears). */
typedef struct CtrWildRing
{
    int px, py;                 /* the player */
    int halfW, halfH;           /* the screen's half size, with a tile for the partial one */
    int left, right, top, bottom;   /* the object event view */
} CtrWildRing;

/* The game's answer for a cell: CTRW_TILE_NONE when nobody may stand there (not grass or water, a wall, an object, other height). */
typedef int (*CtrWildTileFn)(void *ctx, int x, int y);
/* How many object events are free right now (a scan, so it is asked rarely). */
typedef int (*CtrWildFreeFn)(void *ctx);

/* Settings (static inline: 3ds_settings.c reads them without linking this module). */
static inline int CtrWildsCore_ClampDensity(int v)
{
    return v >= 0 && v < CTRW_DENSITIES ? v : 0;
}

static inline int CtrWildsCore_ClampAggro(int v)
{
    return v >= 0 && v < CTRW_AGGROS ? v : 0;
}

/* settings.cfg: the density read from `visiblewilds`. The first version wrote visiblewilds=1 for "on" and had neither wildsil nor
 * wildaggro. The new one writes both with it whenever the density is not OFF, so a 1 that comes alone is the old "on", which is
 * NORMAL, and a 1 with them is FEW. Anything out of range is OFF. */
static inline int CtrWildsCore_MigrateDensity(int raw, int haveSil, int haveAggro)
{
    if (raw == 1 && !haveSil && !haveAggro)
        return 2;
    return CtrWildsCore_ClampDensity(raw);
}

int CtrWildsCore_DensityCap(int density);                       /* 0, 2, 4, 6 */
int CtrWildsCore_AggroPercent(int aggro);                       /* 0, 10, 20, 30, 40 */

/* The table and its random numbers (its own, never the game's Random()). */
void CtrWildsCore_Init(CtrWildsCore *c, uint32_t seed);
uint16_t CtrWildsCore_Rand(CtrWildsCore *c);
uint32_t CtrWildsCore_Seed(CtrWildsCore *c);                    /* an unrelated 32-bit seed, for the game's generator while it picks */
void CtrWildsCore_Stir(CtrWildsCore *c, uint32_t salt);
/* A new map: the table is cleared (the object events went with the map). Returns how many wilds were in it, or -1 for the same map. */
int CtrWildsCore_MapChanged(CtrWildsCore *c, uint16_t map);
void CtrWildsCore_Clear(CtrWildsCore *c);
void CtrWildsCore_Forget(CtrWildsCore *c, int slot);

/* Every frame, before anything else: TRUE when the option is off and nothing is out (nothing to do at all). */
int CtrWildsCore_Idle(const CtrWildsCore *c, int density);
/* The frame's setting: sets whether the system works here. Returns TRUE when every wild must be removed (it stopped working). */
int CtrWildsCore_Frame(CtrWildsCore *c, const CtrWildEnv *env);

/* The budget. */
int CtrWildsCore_Target(int density, int consoleCap, int freeObjects, int active);
int CtrWildsCore_ShedCount(int freeObjects, int active);       /* wilds to remove to bring the reserve back */
int CtrWildsCore_FarthestSlot(const CtrWildsCore *c);          /* the slot to shed first, or CTRW_NONE */
/* The periodic check: asks for the free object events at most once per CTRW_TICK frames and returns how many wilds to shed (>= 0).
 * It also fills c->target and c->spawnable. Returns -1 when it is not time yet. */
int CtrWildsCore_Tick(CtrWildsCore *c, int density, int consoleCap, CtrWildFreeFn freeFn, void *ctx);

/* Whether the normal step encounter of a kind (CTRW_TILE_LAND or WATER) gives way: only where the system works and a wild is out or
 * can appear, so a zone that cannot show wilds keeps the game's encounters. */
int CtrWildsCore_SuppressStep(const CtrWildsCore *c, int tileKind);

/* Spawning. */
int CtrWildsCore_SpawnDue(CtrWildsCore *c);                    /* TRUE when it is time for an attempt and one is wanted */
int CtrWildsCore_FreeSlot(const CtrWildsCore *c);              /* an empty slot below the target, or CTRW_NONE */
int CtrWildsCore_AcceptKind(CtrWildsCore *c, int tile, int hasLand, int hasWater, int surfing);
/* A cell in the ring just outside the screen (left, right or below it) that the game accepts: TRUE with its x, y and kind. */
int CtrWildsCore_PickCell(CtrWildsCore *c, const CtrWildRing *ring, CtrWildTileFn tileFn, void *ctx, int hasLand, int hasWater,
                          int surfing, int *x, int *y, int *kind);
void CtrWildsCore_SpawnDone(CtrWildsCore *c, int success);
int CtrWildsCore_Commit(CtrWildsCore *c, int slot, const CtrWildMon *mon, int gender, int shiny, int gfx);
void CtrWildsCore_SetObject(CtrWildsCore *c, int slot, int obj);

/* The Pokemon. */
uint32_t CtrWildsCore_Personality(CtrWildsCore *c, int nature);     /* a personality (from the table's generator) with that nature */
int CtrWildsCore_Nature(uint32_t personality);                      /* personality % 25, as the game */
int CtrWildsCore_Gender(int genderRatio, uint32_t personality);     /* 0 male, 254 female, 255 none: the game's rule */
int CtrWildsCore_Shiny(uint32_t otId, uint32_t personality);
uint32_t CtrWildsCore_Power(int level, int baseStatSum);
int CtrWildsCore_RollRoamer(CtrWildsCore *c, int roamerHere, int roamerOut);    /* 1 in 4, as the game's own roamer roll */
/* A species may be seen: it has a picture, is not an Unown (their letter depends on the chamber) and is a Pokemon. */
int CtrWildsCore_SpeciesOk(int species, int hasPicture, int isUnown);
/* A Repel keeps a wild off when it is weaker than the lead Pokemon (the game's IsWildLevelAllowedByRepel). leadLevel 0: none fit. */
int CtrWildsCore_RepelBlocks(int repelActive, int wildLevel, int leadLevel);

/* The mind. */
void CtrWildsCore_Think(CtrWildsCore *c, int slot, const CtrWildSense *sense, CtrWildAct *act);
void CtrWildsCore_StepResult(CtrWildsCore *c, int slot, int ok);   /* the game's answer to a CTRW_A_STEP */
void CtrWildsCore_Calm(CtrWildsCore *c, int slot, int cool);       /* the encounter did not happen (Repel, disabled): it settles down */
int CtrWildsCore_Judge(const CtrWildsCore *c, int slot);           /* wild power in percent of the lead's, or 0 when unknown */

#endif
