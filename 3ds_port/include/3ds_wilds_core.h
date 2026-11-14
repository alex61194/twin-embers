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
#define CTRW_LOCALID 0xF0       /* the local id of slot 0 (CTR_WILD_LOCALID); slot n is CTRW_LOCALID + n */
#define CTRW_MARK 0xD7          /* the mark a wild's object event carries (trainerRange_berryTreeId, unused by it): the save keeps it */
#define CTRW_STALL_FAILS 6      /* spawn attempts that fail in a row, with nothing out, before the game's step encounters come back */
#define CTRW_NONE 0xFF          /* no object event / no slot */
#define CTRW_TICK 30            /* frames between two checks of the free object events */
#define CTRW_DENSITIES 4        /* OFF, FEW, NORMAL, MANY */
#define CTRW_AGGROS 5           /* OFF, RARE, LOW, MODERATE, HIGH */

/* The spawn rhythm, in frames (60 per second). Each is a minimum and the number of values from it: the wait is MIN + Rand() % SPAN.
 * That is the configured wait; the REAL frames equal it: SpawnDue counts operative frames below the target and answers on the frame the
 * wait reaches 0, so a wait of T is T frames between two attempts (the frames with locked controls or a blocked system do not count),
 * and the first wait is T frames from the map change, the change frame being frame 1. */
#define CTRW_FIRST_MIN 24       /* the first wild after a new map: 24 to 36 frames (0.4 to 0.6 s) of operative time */
#define CTRW_FIRST_SPAN 13
#define CTRW_FILL_MIN 45        /* after a success while fewer than half of the target are out (active * 2 < target): 45 to 90 */
#define CTRW_FILL_SPAN 46
#define CTRW_TOPUP_MIN 90       /* after a success with half of the target or more out: 90 to 150 (it only counts while active < target) */
#define CTRW_TOPUP_SPAN 61
#define CTRW_FAIL_MIN 20        /* after an attempt that found nowhere (or nothing to show): 20 to 40 */
#define CTRW_FAIL_SPAN 21
#define CTRW_STALLED_MIN 60     /* the same while stalled (CTRW_STALL_FAILS fails in a row, nothing out): 60 to 90, so a map with no room costs little */
#define CTRW_STALLED_SPAN 31
#define CTRW_REMOVED_MIN 60     /* after a wild went away: the wait is cut to 60 to 119 when it was longer (never made longer) */
#define CTRW_REMOVED_SPAN 60
#define CTRW_PICK_TRIES 24      /* cells a spawn attempt may try (one tile lookup each) */
#define CTRW_STATS_PERIOD 1800  /* operative frames (30 s) between two summary lines in one map */
#define CTRW_ZONE_MAX 4096    /* the longest side of a ring zone that PickCell uses (a guard, the game's are 30 at most) */

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
    uint8_t last;               /* the direction of the last wander step (0: none): the next one does not walk back over it */
    uint8_t dist;               /* its last distance to the player (tiles, the longer axis) */
    uint16_t species;
    uint16_t gfx;               /* the picture the game draws it with */
    uint16_t timer;             /* frames to the next decision */
    uint16_t cool;              /* frames before it may notice the player again */
    uint16_t age;               /* frames since it appeared (stops at 0xFFFF) */
    uint32_t personality;       /* the personality its battle Pokemon is made with: nature, gender and shininess are in it */
    uint32_t power;             /* level x the six base stats, for the strength comparison */
} CtrWild;

/* Why a spawn attempt failed (CtrWildsCore_NoteFail). */
enum { CTRW_FAIL_NOCELL = 0, CTRW_FAIL_ROLL, CTRW_FAIL_NOOBJ, CTRW_FAIL_REASONS };

/* Light diagnostics of the map the table is on (reset by every new map): plain counters, nothing is written per frame. They work only
 * while the table's statsOn is set (the game's glue sets it from "port.log is on", CtrWildsCore_SetStats): with the log off not one
 * counter moves and no line is formatted. frames counts only the frames the spawn logic runs (the system is operative there), so
 * with the option off, nothing is counted. 0 in firstFrames or fillFrames: not reached yet. They count from the moment counting
 * began: on a new map that is the map's first frame; if the log came on later, from then (SetStats starts from zero, never from a
 * half state). */
typedef struct CtrWildsStats
{
    uint32_t frames;            /* operative frames since the map changed */
    uint32_t nextLog;           /* frames at which the next periodic line is due */
    uint32_t firstFrames;       /* frames up to the first wild */
    uint32_t fillFrames;        /* frames up to the first time active >= target (target > 0) */
    uint32_t attempts, ok;
    uint32_t fail[CTRW_FAIL_REASONS];
    uint32_t stalls;            /* times the system became stalled */
    uint8_t gotFirst, gotFill;
} CtrWildsStats;

typedef struct CtrWildsCore
{
    CtrWild wild[CTRW_SLOTS];
    CtrWildsStats stats;
    uint32_t rng;
    uint16_t spawnTimer;
    uint16_t tick;
    uint16_t map;               /* the map the table was made on */
    uint8_t mapKnown;
    uint8_t probed;             /* mapLand and mapWater have been worked out for this map */
    uint8_t active;             /* wilds in the table */
    uint8_t mapLand, mapWater;  /* the map has a land / water table with a Pokemon that has a picture */
    uint8_t operative;          /* the system works on this map right now: it may hide the step encounters */
    uint8_t spawnable;          /* ... and a wild is out or one could appear (free object events, a cap) */
    uint8_t target;             /* how many wilds the budget allows */
    uint8_t fails;              /* spawn attempts that found nowhere to appear, in a row (0 at a success, a new map, off) */
    uint8_t aggro;              /* the AGGRESSIVE setting, 0 to 4 */
    uint8_t statsOn;            /* the diagnostics count (CtrWildsCore_SetStats); 0 at the start and while port.log is off */
    uint8_t charge;             /* slot + 1 of the chaser that stands next to the player and waits for the field input; 0: none */
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

/* The one table the game's glue uses. */
extern CtrWildsCore gCtrWilds;

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
/* Switches the diagnostics on or off (the glue asks the log's state every CTRW_TICK frames and on a new map, never per frame). A change
 * either way starts from zero: the counters are cleared (on: counting begins now; off: nothing is left behind). The same state does
 * nothing. It never touches the spawn logic, the timers or the table's random numbers. */
void CtrWildsCore_SetStats(CtrWildsCore *c, int on);
void CtrWildsCore_Forget(CtrWildsCore *c, int slot);            /* a wild is gone: the next one is brought forward (CtrWildsCore_Removed) */
/* A wild went away: a longer wait for the next one is cut to CTRW_REMOVED_MIN..+SPAN-1 frames. Does nothing when the wait is already
 * that short (so calling it for several wilds in a row draws no more than once, and never makes a wait longer). */
void CtrWildsCore_Removed(CtrWildsCore *c);

/* Every frame, before anything else: TRUE when the option is off and nothing is out (nothing to do at all). It also stops the system
 * working (operative, spawnable, fails), so the game's step encounters are back at once, with no Frame needed. */
int CtrWildsCore_Idle(CtrWildsCore *c, int density);
/* The frame's setting: sets whether the system works here. Returns TRUE when every wild must be removed (it stopped working). */
int CtrWildsCore_Frame(CtrWildsCore *c, const CtrWildEnv *env);

/* TRUE when this map was probed and neither a land nor a water table of it can show a Pokemon, and no wild is out: nothing to do. */
int CtrWildsCore_Unsupported(CtrWildsCore *c);      /* ... and, like Idle, it stops the system working then */

/* An object event of the saved game that a wild left behind: it carries the mark and a wild's local id but the table does not own it
 * (the table is empty after a load, or kept the state of before it). index: its place in the object events. marked: it is the Boy,
 * has no movement type and carries CTRW_MARK. Anything else, a map's NPC included, is never an orphan. */
int CtrWildsCore_IsOrphan(const CtrWildsCore *c, int index, int localId, int marked);

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
/* TRUE when nothing is out and CTRW_STALL_FAILS attempts in a row found nowhere to appear (a map too small, a narrow route, the player
 * at its edge): the game's step encounters come back, and the system goes on trying with its timer until one succeeds (it then
 * hides them again), the map changes or the option goes off. Nothing is switched off for good. */
int CtrWildsCore_Stalled(const CtrWildsCore *c);

/* Spawning. */
/* TRUE when it is time for an attempt and one is wanted. A call counts one operative frame below the target: the wait goes down by one and
 * the answer is TRUE on the call that brings it to 0 (a wait of T: the T-th call; already 0: this one). */
int CtrWildsCore_SpawnDue(CtrWildsCore *c);
int CtrWildsCore_FreeSlot(const CtrWildsCore *c);              /* an empty slot below the target, or CTRW_NONE */
int CtrWildsCore_AcceptKind(CtrWildsCore *c, int tile, int hasLand, int hasWater, int surfing);
/* A cell in the ring just outside the screen (left, right or below it) that the game accepts: TRUE with its x, y and kind. The cell is
 * drawn uniformly over the three zones (each weighs its number of cells; empty zones weigh 0, an empty ring gives FALSE) and at most
 * CTRW_PICK_TRIES cells are looked at (one tile lookup each). */
int CtrWildsCore_PickCell(CtrWildsCore *c, const CtrWildRing *ring, CtrWildTileFn tileFn, void *ctx, int hasLand, int hasWater,
                          int surfing, int *x, int *y, int *kind);
/* Before SpawnDone(c, 0): why the attempt failed, for the diagnostics (CTRW_FAIL_*). */
void CtrWildsCore_NoteFail(CtrWildsCore *c, int reason);
/* An attempt ended (after the wild was committed, when it succeeded): sets the wait to the next one (CTRW_*_MIN above). */
void CtrWildsCore_SpawnDone(CtrWildsCore *c, int success);
/* Diagnostics: TRUE (and the next one is scheduled) once in CTRW_STATS_PERIOD operative frames while attempts were made in this map.
 * Always FALSE while statsOn is 0. */
int CtrWildsCore_StatsDue(CtrWildsCore *c);
/* One short line for port.log, into buf (always terminated): "wilds: map=0102 att=9 ok=6 nocell=2 roll=1 noobj=0 first=31 fill=420
 * stalls=0 t=900". first and fill are frames, 0 when not reached. Returns the length written. While statsOn is 0 it
 * writes nothing (buf is "" ) and returns 0: the glue does not call it then, this is the second guard. */
int CtrWildsCore_StatsLine(const CtrWildsCore *c, char *buf, unsigned cap);
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

int CtrWildsCore_HasRoamer(const CtrWildsCore *c);                 /* a roamer is already out */
/* A wild that has been out a long while (1500 frames plus 300 per slot), is resting and is out of the screen's sight is retired. */
int CtrWildsCore_Stale(const CtrWildsCore *c, int slot, int screenDist);

/* The mind. */
void CtrWildsCore_Think(CtrWildsCore *c, int slot, const CtrWildSense *sense, CtrWildAct *act);
void CtrWildsCore_StepResult(CtrWildsCore *c, int slot, int ok);   /* the game's answer to a CTRW_A_STEP */
void CtrWildsCore_StepTaken(CtrWildsCore *c, int slot, int dir);   /* ... and the direction it walked (CTRW_DIR_*), after an ok one */
void CtrWildsCore_Calm(CtrWildsCore *c, int slot, int cool);       /* the encounter did not happen (Repel, disabled): it settles down */
int CtrWildsCore_Judge(const CtrWildsCore *c, int slot);           /* wild power in percent of the lead's, or 0 when unknown */


/* The charge: a chaser that has caught up (CTRW_A_CHARGE) waits for the player's field input to start its battle. The decision is made
 * here, from what the game sees, so that it can be tested: the charge is kept while the player is between two tiles, started when they
 * stand still next to it, and dropped only for a reason (the heights differ, a script took the field, it is no longer next to them). */
enum { CTRW_CH_NONE = 0, CTRW_CH_WAIT, CTRW_CH_FIGHT, CTRW_CH_CANCEL };

typedef struct CtrWildCharge
{
    int dx, dy;                 /* the player's tile minus the wild's, now */
    uint8_t present;            /* the wild's object event still exists */
    uint8_t sameElev;           /* the player and the wild are at compatible heights (a water wild always is) */
    uint8_t moving;             /* the player is between two tiles */
    uint8_t locked;             /* a script, a menu or another event holds the field controls */
} CtrWildCharge;

int CtrWildsCore_Arm(CtrWildsCore *c, int slot);                   /* a wild has caught up: 1 if it is the one that waits, 0 if another already does */
int CtrWildsCore_Charge(CtrWildsCore *c, const CtrWildCharge *in); /* every field input frame: CTRW_CH_* (FIGHT: start its battle now) */
int CtrWildsCore_ChargeSlot(const CtrWildsCore *c);                /* the waiting wild's slot, -1 when none */
void CtrWildsCore_Disarm(CtrWildsCore *c);
void CtrWildsCore_Interrupt(CtrWildsCore *c);                      /* the field was taken (a script, a menu): the charge is dropped and its wild settles */
void CtrWildsCore_BattleStarted(CtrWildsCore *c);                  /* a battle began: no charge stays, the wilds closing in settle down */

#endif
