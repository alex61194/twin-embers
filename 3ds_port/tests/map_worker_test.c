/* The HOME map's worker (3ds_home_map_job_impl.h) on a computer: the console's own code, with POSIX threads standing in for libctru's
 * (tests/shim/3ds/thread.h). What it checks, besides that a built image is the one a plain, single-threaded build gives:
 *   - a stop asked of a worker that has not begun, or is between rows, ends it without a wait on the game's thread;
 *   - the region changed again and again (hundreds of times, at random moments) never shows another map's image: whenever the image is reported
 *     ready it is the one of the map last asked for, and the game's thread is never held for more than a moment by it;
 *   - a map with no art (or no group) is decided without a worker, and the next map builds;
 *   - the end of the process during a build joins the worker and leaves nothing running, and the job can start again;
 *   - a new version of the same map builds again.
 * Whether the worker is race free as such needs a thread sanitizer (tools/test_map_worker.py runs this with -fsanitize=thread when the compiler has it);
 * that the console's scheduler (priority 0x3F, the second core) behaves as the stand-in does is for a console or an emulator. */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define CTR_MAP_THREADS 1
#include "3ds_poketouch.h"
#pragma GCC diagnostic ignored "-Wunused-function"      /* the viewport half of the map header is not used here */
static void Px(int x, int y, uint16_t c) { (void)x; (void)y; (void)c; }   /* its drawing helper: the test draws nothing */
/* The feed calls this before each row (MAP_FEED_ROW_HOOK in the job header): a test arms it to hold the worker part way through the art, where it is
 * reading it. A held worker lets go when released, and also when a stop is asked, as a worker between two rows would. All of it is atomic. */
static void FeedHook(int row);
#define MAP_FEED_ROW_HOOK(row) FeedHook(row)
#include "3ds_home_map_job_impl.h"

static int gArtCalls;
static int gArtFail = -1;               /* a group whose art is not there */
static int gArtWhileRunning;            /* times the art was copied while a worker still ran (it must stay 0: the worker reads that art) */
static int gHookRow = -1, gHookReached, gHookRelease, gHookHonourAbort;   /* the feed's hold: the row to wait at (-1 none), reached, let go */

/* Art that differs from one group to the next: tiles and colours from a small generator seeded by the group. */
int CtrPokeTouch_MapArt(int group, uint16_t *tilemap, uint8_t *tiles, uint16_t *palette, uint16_t *bgTilemap, uint8_t *bgTiles)
{
    uint32_t seed = 0x9e3779b9u * (uint32_t)(group + 1);

    ++gArtCalls;
    if (__atomic_load_n(&sMapRunning, __ATOMIC_ACQUIRE))
        ++gArtWhileRunning;
    if (group == gArtFail)
        return 0;
    for (int i = 0; i < MAP_TILEMAP_W * MAP_TILEMAP_H; ++i) {
        seed = seed * 1664525u + 1013904223u;
        tilemap[i] = (uint16_t)((seed >> 16) % MAP_TILES);
        bgTilemap[i] = (uint16_t)((seed >> 8) % 7);
    }
    for (int i = 0; i < MAP_TILES * 32; ++i) {
        seed = seed * 1664525u + 1013904223u;
        tiles[i] = (uint8_t)(seed >> 24);
        bgTiles[i] = (uint8_t)(seed >> 16);
    }
    {
        /* the colours the builder tells apart (greens, roads, pale paths, sea, marks), dealt out differently for each group, so the images differ */
        const uint8_t *all[5 + 4 + 3 + 2 + 11];
        int n = 0;

        for (int i = 0; i < 5; ++i) all[n++] = kHmGreens[i];
        for (int i = 0; i < 4; ++i) all[n++] = kHmRoads[i];
        for (int i = 0; i < 3; ++i) all[n++] = kHmPales[i];
        for (int i = 0; i < 2; ++i) all[n++] = kHmSeas[i];
        for (int i = 0; i < 11; ++i) all[n++] = kHmMarks[i];
        for (int i = 0; i < MAP_COLORS; ++i) {
            const uint8_t *c = all[(i * (group + 1) + group * 5) % n];

            palette[i] = (uint16_t)((c[0] >> 3) | (c[1] >> 3) << 5 | (c[2] >> 3) << 10);
        }
    }
    return 1;
}

static uint64_t ImageHash(void)
{
    uint64_t h = 1469598103934665603ull;

    for (int y = 0; y < HM_H; ++y)
        for (int x = 0; x < HM_W; ++x)
            h = (h ^ sHmWorld[y][x]) * 1099511628211ull;
    for (int i = 0; i < sHmPalN; ++i)                           /* the colours in use: what is past them is left over from another map */
        h = (h ^ sHmPal[i]) * 1099511628211ull;
    return h;
}

/* The image a plain build (no thread) makes of a group: what a worker's must equal. */
static uint64_t Expect(int group)
{
    assert(CtrPokeTouch_MapArt(group, sMapTilemap, sMapTiles, sMapGba, sMapBgTilemap, sMapBgTiles));
    HmBegin();
    assert(MapFeedPixels());
    while (!HmDone() && HM_STAGE() != HMS_FAILED)
        HmStep();
    assert(HmDone());
    return ImageHash();
}

static double NowMs(void)
{
    struct timespec t;

    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

static void SleepUs(long us)
{
    struct timespec d = {us / 1000000, (us % 1000000) * 1000};

    nanosleep(&d, NULL);
}

static void FeedHook(int row)
{
    if (SHIM_GET(gHookRow) != row)
        return;
    SHIM_SET(gHookReached, 1);
    while (!SHIM_GET(gHookRelease) && !(SHIM_GET(gHookHonourAbort) && __atomic_load_n(&sMapAbort, __ATOMIC_RELAXED)))
        SleepUs(200);
}

/* Holds the next worker part way through its feed (at `row`); WaitHeld returns when it is there. It lets go only when released, or also when a stop is asked
 * (honourAbort): the first is a worker busy in one long row that the stop request has not reached yet, the second a worker that obeys at once. */
static void ArmHold(int row, int honourAbort)
{
    SHIM_SET(gHookHonourAbort, honourAbort);
    SHIM_SET(gHookReached, 0);
    SHIM_SET(gHookRelease, 0);
    SHIM_SET(gHookRow, row);
}

static void Disarm(int release)
{
    SHIM_SET(gHookRow, -1);
    SHIM_SET(gHookRelease, release);
}

static int WaitHeld(int ms)
{
    double end = NowMs() + ms;

    while (NowMs() < end) {
        if (SHIM_GET(gHookReached))
            return 1;
        SleepUs(200);
    }
    return 0;
}

/* The art the worker reads, as one number: it must not change while a worker is reading it. */
static uint64_t ArtHash(void)
{
    uint64_t h = 1469598103934665603ull;
    const uint8_t *parts[5] = {(const uint8_t *)sMapTilemap, sMapTiles, (const uint8_t *)sMapBgTilemap, sMapBgTiles, (const uint8_t *)sMapGba};
    const size_t sizes[5] = {sizeof(sMapTilemap), sizeof(sMapTiles), sizeof(sMapBgTilemap), sizeof(sMapBgTiles), sizeof(sMapGba)};

    for (int k = 0; k < 5; ++k)
        for (size_t i = 0; i < sizes[k]; ++i)
            h = (h ^ parts[k][i]) * 1099511628211ull;
    return h;
}

/* The game's thread's frames, until the image of (group, version) is there; false when it takes longer than `ms`. */
static int WaitReady(int group, int version, int ms)
{
    double end = NowMs() + ms;

    while (NowMs() < end) {
        if (MapJobPrepare(group, version) && MapJobImageReady())
            return 1;
        SleepUs(500);
    }
    return 0;
}

int main(void)
{
    uint64_t want[4];
    double worst = 0;

    for (int g = 0; g < 4; ++g)
        want[g] = Expect(g);
    assert(want[0] != want[1] && want[1] != want[2] && want[2] != want[3]);   /* the groups' images differ, so a mix-up shows */

    /* A map is built by the worker and equals the plain build. */
    assert(WaitReady(0, 0, 20000) && sMap.ok);
    assert(ImageHash() == want[0]);
    assert(WaitReady(0, 0, 20000));
    puts("ok  a worker builds the image a plain build gives");

    /* A stop asked of a worker that has not begun: it ends at once, without the game's thread waiting, and nothing is shown of its map. */
    SHIM_SET(gShimStartDelayMs, 60);
    {
        double t0 = NowMs(), took;

        assert(!MapJobPrepare(1, 0) || !MapJobImageReady());
        MapJobPrepare(2, 0);                                     /* the region changed before the first worker began */
        took = NowMs() - t0;
        assert(took < 30 && !MapJobImageReady());                /* no wait: the game's thread went on */
        assert(WaitReady(2, 0, 20000) && ImageHash() == want[2]);
    }
    SHIM_SET(gShimStartDelayMs, 0);
    puts("ok  a stop in front of a worker that has not begun costs no wait and shows nothing wrong");

    /* The region changed again and again, at random moments: the image reported ready is always the one of the map last asked for. */
    {
        uint32_t seed = 12345;
        int last = 0, checks = 0;

        for (int i = 0; i < 600; ++i) {
            double t0, took;

            seed = seed * 1664525u + 1013904223u;
            if ((seed >> 20) % 3 == 0)
                last = (int)((seed >> 12) % 4);                  /* a new region now and then, the same one for several frames otherwise */
            t0 = NowMs();
            if (MapJobPrepare(last, 0) && MapJobImageReady()) {
                assert(ImageHash() == want[last]);
                ++checks;
            }
            took = NowMs() - t0;
            worst = took > worst ? took : worst;
            SleepUs((long)((seed >> 4) % 1500));
        }
        assert(worst < 30);                                      /* no frame was held by the worker's stop or start */
        assert(WaitReady(3, 0, 20000) && ImageHash() == want[3]);
        printf("ok  600 frames with the region changing at random: every ready image was its map's (%d checked), slowest frame %.2f ms\n", checks, worst);
    }

    /* A map without art is decided without a worker and does not block the next one. */
    gArtFail = 1;
    assert(!MapJobPrepare(1, 0) && !sMap.ok && !sMapThread);
    assert(!MapJobPrepare(1, 0) && !sMap.ok);
    gArtFail = -1;
    assert(!MapJobPrepare(-1, 0) && !sMap.ok);                   /* no group: nothing to build */
    assert(WaitReady(0, 0, 20000) && ImageHash() == want[0]);
    puts("ok  a map with no art, and no map, are decided without a worker, and the next map builds");

    /* A new version of the same map builds again. */
    {
        int before = gArtCalls;

        assert(WaitReady(0, 1, 20000) && ImageHash() == want[0] && gArtCalls > before);
        puts("ok  a new version of a map builds again");
    }

    /* The end of the process during a build: the worker is joined, nothing keeps running, and the job can begin again. */
    for (int round = 0; round < 40; ++round) {
        SHIM_SET(gShimStartDelayMs, round % 3 ? 0 : 5);
        MapJobPrepare(round % 4, 100 + round);                  /* a build starts ... */
        SleepUs((long)(round % 7) * 400);                        /* ... and is somewhere in its feed or its steps ... */
        MapJobShutdown();                                        /* ... when the process ends */
        assert(!sMapThread && !__atomic_load_n(&sMapRunning, __ATOMIC_ACQUIRE) && !sMapStarted);
        assert(SHIM_GET(gShimThreadsCreated) == SHIM_GET(gShimThreadsFreed));        /* every worker joined and freed */
    }
    SHIM_SET(gShimStartDelayMs, 0);
    assert(WaitReady(2, 0, 20000) && ImageHash() == want[2]);
    MapJobShutdown();
    assert(SHIM_GET(gShimThreadsCreated) == SHIM_GET(gShimThreadsFreed));
    puts("ok  the end of the process joins the worker wherever it is, and the job starts again");

    /* The art is not written while a worker still reads it. The worker of group 1 is held at row 40 of its feed (reading the art); the region changes to
     * group 2 while it is held, again and again: the game's thread must neither copy the new art over it nor show anything, until the worker is gone. */
    assert(WaitReady(0, 0, 20000));
    {
        int calls, held = 0;
        uint64_t art;

        ArmHold(40, 0);
        MapJobPrepare(1, 0);
        assert(WaitHeld(5000));
        assert(__atomic_load_n(&sMapRunning, __ATOMIC_ACQUIRE) && sMapThread);
        art = ArtHash();
        calls = gArtCalls;
        for (int i = 0; i < 300; ++i) {
            assert(!MapJobPrepare(2, 0) || !__atomic_load_n(&sMapRunning, __ATOMIC_ACQUIRE));
            assert(!MapJobImageReady());
            if (__atomic_load_n(&sMapRunning, __ATOMIC_ACQUIRE)) {
                ++held;
                assert(ArtHash() == art && gArtCalls == calls);      /* still group 1's art, never copied over */
            }
            if (i == 150)
                SleepUs(2000);
        }
        assert(held > 0);
        Disarm(1);
        assert(WaitReady(2, 0, 20000) && ImageHash() == want[2]);
        assert(gArtWhileRunning == 0);                               /* and no copy of art, in the whole run so far, happened under a running worker */
        printf("ok  the art is not copied while a worker still reads it (%d frames asked for another map while one was held)\n", held);
    }
    /* The same kind of worker, held and then let go with nothing asked of it: it ends with its own map's image (its art was never touched). */
    {
        ArmHold(40, 0);
        MapJobPrepare(3, 0);
        assert(WaitHeld(5000));
        for (int i = 0; i < 100; ++i) {
            MapJobPrepare(3, 0);
            SleepUs(100);
        }
        Disarm(1);
        assert(WaitReady(3, 0, 20000) && ImageHash() == want[3] && gArtWhileRunning == 0);
        puts("ok  a worker held while the game's frames go on ends with its own map's image");
    }

    /* After MapJobShutdown. With no worker it does nothing, twice; with a worker held part way it stops it (a stop reaches a worker between rows),
     * joins and frees it without a long wait; what was built is not shown; and the job starts again by itself, with a fresh build of the same map. */
    MapJobShutdown();
    MapJobShutdown();
    assert(!sMapThread && !__atomic_load_n(&sMapRunning, __ATOMIC_ACQUIRE) && !sMapStarted && !MapJobImageReady());
    assert(SHIM_GET(gShimThreadsCreated) == SHIM_GET(gShimThreadsFreed));
    {
        double t0, took;
        int calls;

        ArmHold(60, 1);
        MapJobPrepare(3, 500);
        assert(WaitHeld(5000));
        assert(sMapThread && __atomic_load_n(&sMapRunning, __ATOMIC_ACQUIRE));
        t0 = NowMs();
        MapJobShutdown();
        took = NowMs() - t0;
        Disarm(0);
        assert(took < 500);
        assert(!sMapThread && !__atomic_load_n(&sMapRunning, __ATOMIC_ACQUIRE) && !sMapStarted && !MapJobImageReady());
        assert(SHIM_GET(gShimThreadsCreated) == SHIM_GET(gShimThreadsFreed));
        calls = gArtCalls;
        assert(WaitReady(3, 500, 20000) && ImageHash() == want[3] && gArtCalls > calls);   /* the same map and version builds again, whole */
        printf("ok  Shutdown with a worker held part way: stopped and joined in %.2f ms, nothing shown, and the same map builds again\n", took);
    }
    /* Shutdown when the worker has ended but was not joined yet (the game's thread had not asked again). */
    {
        double end = NowMs() + 20000;

        MapJobShutdown();
        MapJobPrepare(0, 600);                                       /* starts the build; no further frame joins it */
        assert(sMapThread);
        while (__atomic_load_n(&sMapRunning, __ATOMIC_ACQUIRE) && NowMs() < end)
            SleepUs(500);
        assert(!__atomic_load_n(&sMapRunning, __ATOMIC_ACQUIRE) && sMapThread);
        MapJobShutdown();
        assert(!sMapThread && !sMapStarted && !MapJobImageReady() && SHIM_GET(gShimThreadsCreated) == SHIM_GET(gShimThreadsFreed));
        assert(WaitReady(1, 600, 20000) && ImageHash() == want[1]);
        MapJobShutdown();
        assert(SHIM_GET(gShimThreadsCreated) == SHIM_GET(gShimThreadsFreed) && gArtWhileRunning == 0);
        puts("ok  Shutdown joins a worker that had ended, is safe twice, and a later frame starts the job again");
    }

    puts("PASS map worker");
    return 0;
}
