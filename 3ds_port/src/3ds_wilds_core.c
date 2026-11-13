/* Visible wild Pokemon, the pure part (see include/3ds_wilds_core.h). No game headers: the host tests build it alone. */
#include <stdio.h>
#include <string.h>

#include "3ds_wilds_core.h"

static int Abs(int v)
{
    return v < 0 ? -v : v;
}

static int Max(int a, int b)
{
    return a > b ? a : b;
}

static int Min(int a, int b)
{
    return a < b ? a : b;
}

/* Settings ---------------------------------------------------------------------------------------------------------------- */

int CtrWildsCore_DensityCap(int density)
{
    return density == 1 ? 2 : density == 2 ? 4 : density == 3 ? 6 : 0;
}

int CtrWildsCore_AggroPercent(int aggro)
{
    return aggro >= 1 && aggro <= 4 ? aggro * 10 : 0;
}

/* The table and its random numbers ---------------------------------------------------------------------------------------- */

void CtrWildsCore_Init(CtrWildsCore *c, uint32_t seed)
{
    memset(c, 0, sizeof(*c));
    c->rng = seed;
    for (int i = 0; i < CTRW_SLOTS; ++i)
        c->wild[i].obj = CTRW_NONE;
}

CtrWildsCore gCtrWilds;

uint16_t CtrWildsCore_Rand(CtrWildsCore *c)
{
    if (c->rng == 0)
        c->rng = 0x2545F491u;       /* the game's table starts zeroed */
    c->rng = c->rng * 1103515245u + 12345u;
    return (uint16_t)(c->rng >> 16);
}

uint32_t CtrWildsCore_Seed(CtrWildsCore *c)
{
    uint32_t hi = CtrWildsCore_Rand(c);

    return (hi << 16) | CtrWildsCore_Rand(c);
}

void CtrWildsCore_Stir(CtrWildsCore *c, uint32_t salt)
{
    c->rng += salt;
}

void CtrWildsCore_Clear(CtrWildsCore *c)
{
    for (int i = 0; i < CTRW_SLOTS; ++i)
    {
        memset(&c->wild[i], 0, sizeof(c->wild[i]));
        c->wild[i].obj = CTRW_NONE;
    }
    c->active = 0;
    c->fails = 0;
}

void CtrWildsCore_SetStats(CtrWildsCore *c, int on)
{
    uint8_t flag = (uint8_t)(on != 0);

    if (c->statsOn == flag)
        return;
    c->statsOn = flag;
    memset(&c->stats, 0, sizeof(c->stats));     /* either way from zero: counting begins now, or nothing is left of it */
}

void CtrWildsCore_Forget(CtrWildsCore *c, int slot)
{
    if (slot < 0 || slot >= CTRW_SLOTS || c->wild[slot].state == CTRW_S_FREE)
        return;
    memset(&c->wild[slot], 0, sizeof(c->wild[slot]));
    c->wild[slot].obj = CTRW_NONE;
    if (c->active)
        --c->active;
    CtrWildsCore_Removed(c);
}

void CtrWildsCore_Removed(CtrWildsCore *c)
{
    if (c->spawnTimer >= CTRW_REMOVED_MIN + CTRW_REMOVED_SPAN)
        c->spawnTimer = (uint16_t)(CTRW_REMOVED_MIN + CtrWildsCore_Rand(c) % CTRW_REMOVED_SPAN);
}

int CtrWildsCore_MapChanged(CtrWildsCore *c, uint16_t map)
{
    int had;

    if (c->mapKnown && c->map == map)
        return -1;
    had = c->active;
    CtrWildsCore_Clear(c);
    c->map = map;
    c->mapKnown = 1;
    c->mapLand = c->mapWater = 0;
    c->probed = 0;
    c->operative = c->spawnable = 0;
    c->target = 0;
    c->fails = 0;
    c->tick = 0;            /* the first frame counts the free object events */
    memset(&c->stats, 0, sizeof(c->stats));
    c->spawnTimer = (uint16_t)(CTRW_FIRST_MIN + CtrWildsCore_Rand(c) % CTRW_FIRST_SPAN);    /* the first wild comes after half a second (the core's own RNG) */
    return had;
}

int CtrWildsCore_Idle(CtrWildsCore *c, int density)
{
    if (density > 0 || c->active != 0)
        return 0;
    /* Off and nothing out: the game's step encounters are back at once. The frame that would clear these never runs from here. */
    c->operative = c->spawnable = 0;
    c->fails = 0;
    return 1;
}

int CtrWildsCore_Frame(CtrWildsCore *c, const CtrWildEnv *env)
{
    c->aggro = (uint8_t)CtrWildsCore_ClampAggro(env->aggro);
    c->leadPower = env->leadPower;
    c->operative = env->density > 0 && env->consoleCap > 0 && !env->blocked && (c->mapLand || c->mapWater);
    if (!c->operative)
        c->spawnable = 0;
    return !c->operative && c->active > 0;
}

int CtrWildsCore_Unsupported(CtrWildsCore *c)
{
    if (!c->probed || c->mapLand || c->mapWater || c->active != 0)
        return 0;
    c->operative = c->spawnable = 0;
    c->fails = 0;
    return 1;
}

int CtrWildsCore_IsOrphan(const CtrWildsCore *c, int index, int localId, int marked)
{
    int slot = localId - CTRW_LOCALID;

    if (!marked || slot < 0 || slot >= CTRW_SLOTS)
        return 0;       /* a map's own object, a companion or anything not made by this system is never touched */
    return !(c->wild[slot].state != CTRW_S_FREE && c->wild[slot].obj == index);
}

int CtrWildsCore_HasRoamer(const CtrWildsCore *c)
{
    for (int i = 0; i < CTRW_SLOTS; ++i)
        if (c->wild[i].state != CTRW_S_FREE && c->wild[i].roamer)
            return 1;
    return 0;
}

int CtrWildsCore_Stale(const CtrWildsCore *c, int slot, int screenDist)
{
    const CtrWild *w;

    if (slot < 0 || slot >= CTRW_SLOTS)
        return 0;
    w = &c->wild[slot];
    return (w->state == CTRW_S_IDLE || w->state == CTRW_S_WANDER) && w->age > 1500 + 300 * slot && (int)w->dist > screenDist;
}

/* The budget -------------------------------------------------------------------------------------------------------------- */

int CtrWildsCore_Target(int density, int consoleCap, int freeObjects, int active)
{
    int cap = Min(Min(CtrWildsCore_DensityCap(density), consoleCap), CTRW_SLOTS);
    int room = freeObjects > CTRW_RESERVE ? freeObjects - CTRW_RESERVE : 0;

    return Max(0, Min(cap, active + room));
}

int CtrWildsCore_ShedCount(int freeObjects, int active)
{
    int n = freeObjects < CTRW_RESERVE ? CTRW_RESERVE - freeObjects : 0;

    return Min(n, active);
}

int CtrWildsCore_FarthestSlot(const CtrWildsCore *c)
{
    int best = CTRW_NONE, far = -1;

    for (int i = 0; i < CTRW_SLOTS; ++i)
        if (c->wild[i].state != CTRW_S_FREE && (int)c->wild[i].dist >= far)
        {
            far = c->wild[i].dist;
            best = i;
        }
    return best;
}

int CtrWildsCore_Tick(CtrWildsCore *c, int density, int consoleCap, CtrWildFreeFn freeFn, void *ctx)
{
    int freeObjects, cap, shed;

    if (c->tick != 0)
    {
        --c->tick;
        return -1;
    }
    c->tick = CTRW_TICK - 1;
    freeObjects = freeFn(ctx);
    cap = Min(Min(CtrWildsCore_DensityCap(density), consoleCap), CTRW_SLOTS);
    c->target = (uint8_t)CtrWildsCore_Target(density, consoleCap, freeObjects, c->active);
    c->spawnable = c->active > 0 || c->target > 0;
    shed = Max(CtrWildsCore_ShedCount(freeObjects, c->active), c->active - cap);
    return Max(0, Min(shed, c->active));
}

int CtrWildsCore_Stalled(const CtrWildsCore *c)
{
    return c->active == 0 && c->fails >= CTRW_STALL_FAILS;
}

int CtrWildsCore_SuppressStep(const CtrWildsCore *c, int tileKind)
{
    if (!c->operative || !c->spawnable || CtrWildsCore_Stalled(c))
        return 0;
    return (tileKind == CTRW_TILE_LAND && c->mapLand) || (tileKind == CTRW_TILE_WATER && c->mapWater);
}

/* Spawning ---------------------------------------------------------------------------------------------------------------- */

int CtrWildsCore_SpawnDue(CtrWildsCore *c)
{
    if (!c->operative)
        return 0;
    if (c->statsOn)         /* the only per-frame cost of the diagnostics: one byte tested where the spawn logic runs (never with the option off) */
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

int CtrWildsCore_FreeSlot(const CtrWildsCore *c)
{
    if (c->active >= c->target)
        return CTRW_NONE;
    for (int i = 0; i < CTRW_SLOTS; ++i)
        if (c->wild[i].state == CTRW_S_FREE)
            return i;
    return CTRW_NONE;
}

int CtrWildsCore_AcceptKind(CtrWildsCore *c, int tile, int hasLand, int hasWater, int surfing)
{
    if (tile == CTRW_TILE_LAND)
        return hasLand && !surfing;
    if (tile == CTRW_TILE_WATER)
    {
        if (!hasWater)
            return 0;
        if (surfing || !hasLand)
            return 1;
        return CtrWildsCore_Rand(c) % 3 == 0;   /* on foot, with land too: a third of the water cells */
    }
    return 0;
}

/* The three zones of the ring (0 left of the screen, 1 right of it, 2 below it; the view has no margin above): its box, and how many
 * cells it has (0 when empty). The sides are clamped to CTRW_ZONE_MAX so nothing overflows. */
static int Zone(const CtrWildRing *r, int zone, int *x0, int *y0, int *w)
{
    int x1, y1, h;

    if (zone == 0)
    {
        *x0 = r->left; x1 = r->px - r->halfW - 1; *y0 = r->top; y1 = r->bottom;
    }
    else if (zone == 1)
    {
        *x0 = r->px + r->halfW + 1; x1 = r->right; *y0 = r->top; y1 = r->bottom;
    }
    else
    {
        *x0 = r->left; x1 = r->right; *y0 = r->py + r->halfH + 1; y1 = r->bottom;
    }
    if (x1 < *x0 || y1 < *y0)
        return 0;
    *w = (int)((int64_t)x1 - *x0 < CTRW_ZONE_MAX ? (int64_t)x1 - *x0 + 1 : CTRW_ZONE_MAX);
    h = (int)((int64_t)y1 - *y0 < CTRW_ZONE_MAX ? (int64_t)y1 - *y0 + 1 : CTRW_ZONE_MAX);
    return *w * h;
}

int CtrWildsCore_PickCell(CtrWildsCore *c, const CtrWildRing *r, CtrWildTileFn tileFn, void *ctx, int hasLand, int hasWater,
                          int surfing, int *x, int *y, int *kind)
{
    int x0[3], y0[3], w[3], n[3], total = 0;

    /* A cell chosen uniformly over the three zones taken together (each zone weighs its number of cells; the bottom corners that the
     * side zones also hold weigh twice). A long strip is not drawn as often as a short one, which the old third-each choice did. */
    for (int z = 0; z < 3; ++z)
    {
        n[z] = Zone(r, z, &x0[z], &y0[z], &w[z]);
        total += n[z];
    }
    if (total <= 0)
        return 0;
    for (int tries = 0; tries < CTRW_PICK_TRIES; ++tries)
    {
        uint32_t pick = CtrWildsCore_Rand(c);
        int z = 0, cx, cy, tile;

        if (total > 0xFFFF)
            pick = (pick << 16) | CtrWildsCore_Rand(c);
        pick %= (uint32_t)total;
        while (z < 2 && pick >= (uint32_t)n[z])
        {
            pick -= (uint32_t)n[z];
            ++z;
        }
        cx = x0[z] + (int)(pick % (uint32_t)w[z]);
        cy = y0[z] + (int)(pick / (uint32_t)w[z]);
        tile = tileFn(ctx, cx, cy);
        if (tile == CTRW_TILE_NONE || !CtrWildsCore_AcceptKind(c, tile, hasLand, hasWater, surfing))
            continue;
        *x = cx;
        *y = cy;
        *kind = tile;
        return 1;
    }
    return 0;
}

static void NoteFill(CtrWildsCore *c)
{
    if (!c->stats.gotFill && c->target > 0 && c->active >= c->target)
    {
        c->stats.gotFill = 1;
        c->stats.fillFrames = c->stats.frames;
    }
}

void CtrWildsCore_NoteFail(CtrWildsCore *c, int reason)
{
    if (c->statsOn && reason >= 0 && reason < CTRW_FAIL_REASONS)
        ++c->stats.fail[reason];
}

void CtrWildsCore_SpawnDone(CtrWildsCore *c, int success)
{
    int wasStalled = CtrWildsCore_Stalled(c);

    if (c->statsOn)
        ++c->stats.attempts;
    if (success)
    {
        c->fails = 0;
        if (c->statsOn)
        {
            ++c->stats.ok;
            if (!c->stats.gotFirst)
            {
                c->stats.gotFirst = 1;
                c->stats.firstFrames = c->stats.frames;
            }
            NoteFill(c);
        }
        /* active already counts the wild just made: with fewer than half of the target out the next comes sooner */
        if (c->active * 2 < c->target)
            c->spawnTimer = (uint16_t)(CTRW_FILL_MIN + CtrWildsCore_Rand(c) % CTRW_FILL_SPAN);
        else
            c->spawnTimer = (uint16_t)(CTRW_TOPUP_MIN + CtrWildsCore_Rand(c) % CTRW_TOPUP_SPAN);
        return;
    }
    if (c->fails < 255)
        ++c->fails;
    if (c->statsOn && !wasStalled && CtrWildsCore_Stalled(c))
        ++c->stats.stalls;
    if (CtrWildsCore_Stalled(c))        /* nowhere to appear: it goes on trying, a little less often */
        c->spawnTimer = (uint16_t)(CTRW_STALLED_MIN + CtrWildsCore_Rand(c) % CTRW_STALLED_SPAN);
    else
        c->spawnTimer = (uint16_t)(CTRW_FAIL_MIN + CtrWildsCore_Rand(c) % CTRW_FAIL_SPAN);
}

int CtrWildsCore_StatsDue(CtrWildsCore *c)
{
    if (!c->statsOn || c->stats.attempts == 0 || c->stats.frames < c->stats.nextLog)
        return 0;
    c->stats.nextLog = c->stats.frames + CTRW_STATS_PERIOD;
    return 1;
}

int CtrWildsCore_StatsLine(const CtrWildsCore *c, char *buf, unsigned cap)
{
    const CtrWildsStats *s = &c->stats;
    int n;

    if (cap == 0)
        return 0;
    if (!c->statsOn)
    {
        buf[0] = 0;
        return 0;
    }
    n = snprintf(buf, cap, "wilds: map=%04X att=%u ok=%u nocell=%u roll=%u noobj=%u first=%u fill=%u stalls=%u t=%u", (unsigned)c->map,
                 (unsigned)s->attempts, (unsigned)s->ok, (unsigned)s->fail[CTRW_FAIL_NOCELL], (unsigned)s->fail[CTRW_FAIL_ROLL],
                 (unsigned)s->fail[CTRW_FAIL_NOOBJ], (unsigned)s->firstFrames, (unsigned)s->fillFrames, (unsigned)s->stalls,
                 (unsigned)s->frames);
    if (n < 0)
    {
        buf[0] = 0;
        return 0;
    }
    return n < (int)cap ? n : (int)cap - 1;
}

int CtrWildsCore_Commit(CtrWildsCore *c, int slot, const CtrWildMon *mon, int gender, int shiny, int gfx)
{
    CtrWild *w;

    if (slot < 0 || slot >= CTRW_SLOTS || c->wild[slot].state != CTRW_S_FREE)
        return -1;
    w = &c->wild[slot];
    memset(w, 0, sizeof(*w));
    w->state = CTRW_S_IDLE;
    w->obj = CTRW_NONE;
    w->species = mon->species;
    w->level = mon->level;
    w->water = (uint8_t)(mon->water != 0);
    w->roamer = (uint8_t)(mon->roamer != 0);
    w->personality = mon->personality;
    w->power = mon->power;
    w->gender = (uint8_t)gender;
    w->shiny = (uint8_t)(shiny != 0);
    w->gfx = (uint16_t)gfx;
    w->temper = CtrWildsCore_Rand(c) % 100 < (unsigned)CtrWildsCore_AggroPercent(c->aggro) ? CTRW_TEMPER_AGGRESSIVE : CTRW_TEMPER_CALM;
    w->timer = (uint16_t)(30 + CtrWildsCore_Rand(c) % 60);
    w->cool = 90;           /* it does not react in its first second */
    ++c->active;
    return w->temper;
}

void CtrWildsCore_SetObject(CtrWildsCore *c, int slot, int obj)
{
    if (slot >= 0 && slot < CTRW_SLOTS)
        c->wild[slot].obj = (uint8_t)obj;
}

/* The Pokemon ------------------------------------------------------------------------------------------------------------- */

uint32_t CtrWildsCore_Personality(CtrWildsCore *c, int nature)
{
    uint32_t p = CtrWildsCore_Seed(c);
    uint64_t q = (uint64_t)(p - p % 25u) + (uint32_t)(nature % 25);

    if (q > 0xFFFFFFFFu)
        q -= 25;
    return (uint32_t)q;
}

int CtrWildsCore_Nature(uint32_t personality)
{
    return (int)(personality % 25u);
}

int CtrWildsCore_Gender(int genderRatio, uint32_t personality)
{
    if (genderRatio == 0 || genderRatio == 254 || genderRatio == 255)
        return genderRatio;
    return genderRatio > (int)(personality & 0xFF) ? 254 : 0;
}

int CtrWildsCore_Shiny(uint32_t otId, uint32_t personality)
{
    uint32_t v = (otId >> 16) ^ (otId & 0xFFFF) ^ (personality >> 16) ^ (personality & 0xFFFF);

    return v < 8;
}

uint32_t CtrWildsCore_Power(int level, int baseStatSum)
{
    return (uint32_t)level * (uint32_t)baseStatSum;
}

int CtrWildsCore_RollRoamer(CtrWildsCore *c, int roamerHere, int roamerOut)
{
    if (!roamerHere || roamerOut)
        return 0;
    return CtrWildsCore_Rand(c) % 4 == 0;
}

int CtrWildsCore_SpeciesOk(int species, int hasPicture, int isUnown)
{
    return species > 0 && hasPicture && !isUnown;
}

int CtrWildsCore_RepelBlocks(int repelActive, int wildLevel, int leadLevel)
{
    if (!repelActive)
        return 0;
    return leadLevel <= 0 || wildLevel < leadLevel;
}

/* The mind ---------------------------------------------------------------------------------------------------------------- */

static uint8_t Opposite(uint8_t dir)
{
    return (uint8_t)(((dir - 1) ^ 1) + 1);
}

/* The two directions toward the player (the longer axis first), or away from them. */
static void Directions(CtrWildsCore *c, int dx, int dy, int away, uint8_t out[2])
{
    int xFirst = Abs(dx) >= Abs(dy);
    uint8_t first = xFirst ? (dx > 0 ? CTRW_DIR_EAST : CTRW_DIR_WEST) : (dy > 0 ? CTRW_DIR_SOUTH : CTRW_DIR_NORTH);
    uint8_t second;

    if (xFirst)
        second = dy == 0 ? (CtrWildsCore_Rand(c) & 1 ? CTRW_DIR_SOUTH : CTRW_DIR_NORTH) : (dy > 0 ? CTRW_DIR_SOUTH : CTRW_DIR_NORTH);
    else
        second = dx == 0 ? (CtrWildsCore_Rand(c) & 1 ? CTRW_DIR_EAST : CTRW_DIR_WEST) : (dx > 0 ? CTRW_DIR_EAST : CTRW_DIR_WEST);
    if (away)
    {
        first = Opposite(first);
        second = Opposite(second);
    }
    out[0] = first;
    out[1] = second;
}

int CtrWildsCore_Judge(const CtrWildsCore *c, int slot)
{
    uint64_t ratio;

    if (slot < 0 || slot >= CTRW_SLOTS || c->leadPower == 0)
        return 0;
    ratio = (uint64_t)c->wild[slot].power * 100u / c->leadPower;
    return ratio > 10000 ? 10000 : (int)ratio;
}

void CtrWildsCore_Calm(CtrWildsCore *c, int slot, int cool)
{
    CtrWild *w;

    if (slot < 0 || slot >= CTRW_SLOTS || c->wild[slot].state == CTRW_S_FREE)
        return;
    w = &c->wild[slot];
    w->state = CTRW_S_IDLE;
    w->cool = (uint16_t)cool;
    w->steps = 0;
    w->timer = 60;
}

void CtrWildsCore_StepResult(CtrWildsCore *c, int slot, int ok)
{
    if (slot >= 0 && slot < CTRW_SLOTS && !ok)
        c->wild[slot].steps = 0;        /* blocked: the walk, the flight or the chase ends */
}

static void Step(CtrWildsCore *c, CtrWild *w, int dx, int dy, int away, CtrWildAct *act)
{
    act->kind = CTRW_A_STEP;
    act->n = 2;
    act->fast = (uint8_t)away;
    Directions(c, dx, dy, away, act->dir);
    --w->steps;
}

void CtrWildsCore_Think(CtrWildsCore *c, int slot, const CtrWildSense *s, CtrWildAct *act)
{
    CtrWild *w;
    int dist, manh;

    memset(act, 0, sizeof(*act));
    if (slot < 0 || slot >= CTRW_SLOTS || c->wild[slot].state == CTRW_S_FREE)
        return;
    w = &c->wild[slot];
    dist = Max(Abs(s->dx), Abs(s->dy));
    manh = Abs(s->dx) + Abs(s->dy);
    w->dist = (uint8_t)Min(dist, 255);
    if (w->age != 0xFFFF)
        ++w->age;
    if (w->cool != 0)
        --w->cool;

    /* Noticing the player. */
    if ((w->state == CTRW_S_IDLE || w->state == CTRW_S_WANDER) && w->cool == 0 && s->sameElev && manh > 0)
    {
        int range = w->temper == CTRW_TEMPER_AGGRESSIVE ? CTRW_NOTICE_AGGRESSIVE : (s->fast ? 4 : CTRW_NOTICE_CALM);

        if (dist <= range)
        {
            int ratio = CtrWildsCore_Judge(c, slot);
            int weak = ratio != 0 && ratio < 70;

            if (w->temper == CTRW_TEMPER_AGGRESSIVE)
            {
                /* surprised first; then it charges, or, against a much stronger Pokemon, runs */
                w->after = weak ? CTRW_S_FLEE : CTRW_S_CHASE;
                w->state = CTRW_S_ALERT;
                w->timer = 30;
                act->kind = CTRW_A_ALERT;
                return;
            }
            /* a calm one only runs, from a Pokemon that is much stronger than it: 10% just under the limit, 50% at 30% */
            if (weak && CtrWildsCore_Rand(c) % 100 < (unsigned)(10 + (70 - (ratio < 30 ? 30 : ratio))))
            {
                w->state = CTRW_S_FLEE;
                w->steps = (uint8_t)(2 + CtrWildsCore_Rand(c) % 3);
                w->timer = 0;
            }
            else
                w->cool = 300;
        }
    }

    if (w->state == CTRW_S_ALERT)
    {
        if (dist > CTRW_CHASE_RANGE || !s->sameElev)
        {
            CtrWildsCore_Calm(c, slot, 300);
            return;
        }
        if (w->timer != 0)
        {
            --w->timer;
            return;
        }
        w->state = w->after;
        w->steps = (uint8_t)(w->after == CTRW_S_CHASE ? 12 : 2 + CtrWildsCore_Rand(c) % 3);
    }

    if (w->state == CTRW_S_IDLE)
    {
        if (w->timer != 0)
        {
            --w->timer;
            return;
        }
        if (s->busy)
            return;
        if (CtrWildsCore_Rand(c) % 100 < 40)
        {
            act->kind = CTRW_A_TURN;
            act->dir[0] = (uint8_t)(1 + CtrWildsCore_Rand(c) % 4);
            act->n = 1;
            w->timer = (uint16_t)(60 + CtrWildsCore_Rand(c) % 120);
        }
        else
        {
            w->state = CTRW_S_WANDER;
            w->steps = (uint8_t)(1 + CtrWildsCore_Rand(c) % 3);
        }
        return;
    }
    if (w->state == CTRW_S_WANDER)
    {
        if (w->timer != 0)
        {
            --w->timer;
            return;
        }
        if (s->busy)
            return;
        if (w->steps == 0)
        {
            w->state = CTRW_S_IDLE;
            w->timer = (uint16_t)(60 + CtrWildsCore_Rand(c) % 120);
            return;
        }
        act->kind = CTRW_A_STEP;
        act->n = 2;
        act->dir[0] = (uint8_t)(1 + CtrWildsCore_Rand(c) % 4);
        act->dir[1] = (uint8_t)(1 + act->dir[0] % 4);
        --w->steps;
        w->timer = (uint16_t)(6 + CtrWildsCore_Rand(c) % 16);       /* a pause between steps */
        return;
    }
    if (w->state == CTRW_S_CHASE)
    {
        if (dist > CTRW_CHASE_RANGE || !s->sameElev)
        {
            CtrWildsCore_Calm(c, slot, 600);
            return;
        }
        if (manh == 1)
        {
            w->state = CTRW_S_ENCOUNTER;
            w->timer = 90;
            act->kind = CTRW_A_CHARGE;
            return;
        }
        if (s->busy)
            return;
        if (w->steps == 0)
        {
            CtrWildsCore_Calm(c, slot, 900);        /* it tired of the chase */
            return;
        }
        Step(c, w, s->dx, s->dy, 0, act);
        act->fast = 0;
        return;
    }
    if (w->state == CTRW_S_FLEE)
    {
        if (s->busy)
            return;
        if (w->steps == 0)
        {
            CtrWildsCore_Calm(c, slot, 600);
            if (dist >= 6 && CtrWildsCore_Rand(c) % 100 < 30)
                act->kind = CTRW_A_LEAVE;           /* far enough: it is gone, with a 30% chance */
            return;
        }
        Step(c, w, s->dx, s->dy, 1, act);
        return;
    }
    if (w->state == CTRW_S_ENCOUNTER)
    {
        if (w->timer != 0)
            --w->timer;
        else
            CtrWildsCore_Calm(c, slot, 600);        /* the field input never started it */
    }
}
