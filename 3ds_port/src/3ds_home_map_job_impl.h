/* The HOME map's job (included by 3ds_poketouch.c, and alone by tests/map_worker_test.c): the Region Map's art copied once per map, the image built from it
 * (3ds_home_map_impl.h) on a worker of the lowest priority, and everything about that worker's life: starting it, asking it to stop, waiting for it to
 * stop, a region changed while it builds, the end of the process.
 *
 * The 240x160 screen of FireRed's Region Map (its tilemap, tiles and palette, decoded by the game bridge) is read once per map and redrawn at 4x; the
 * viewport then shows that image at one of its zooms round a centre; nothing is read from the game while the player walks.
 *
 * Who touches what (the rules the worker depends on):
 *   - the game's thread alone writes the art copies (sMapTilemap, ...), calls HmBegin (which clears the image's working memory), creates and joins the
 *     worker, and writes sMap / sMapStarted. It does so only while no worker runs: the worker is joined first.
 *   - the worker alone writes the image and the build's stage once it runs. The game's thread never joins to cancel: it sets sMapAbort and goes on, and
 *     the next frames find the worker gone (sMapRunning 0) before the next map's build starts, so a region changed again and again costs the game no wait.
 *   - sMapAbort, sMapRunning and the build's stage (HM_STAGE) are the words the two threads share, and every access is atomic: the stop request is
 *     checked between the rows of the feed and between the steps; the worker's last act is the release store of sMapRunning 0, which the game's thread
 *     reads with acquire before it touches anything the worker wrote or reads (and threadJoin orders the rest).
 *   - the image is shown only when the build of the map asked for is the one that is done (MapJobImageReady): never the previous map's, never a half one.
 *
 * Without the console's threads (the host tests) the job is the same, only the steps are taken by the game's thread, one a frame. */
#include <stdbool.h>

#define MAP_TILEMAP_W 30
#define MAP_TILEMAP_H 20
#define MAP_TILES 320
#define MAP_COLORS 80

/* A GBA colour (15 bits, red lowest) as RGB565. */
static uint16_t Gba(uint16_t c)
{
    unsigned r = c & 31, g = (c >> 5) & 31, b = (c >> 10) & 31;
    return (uint16_t)(r << 11 | (g << 1 | g >> 4) << 5 | b);
}

/* The palette entry the Region Map's screen shows at its pixel (px, py): the map's layer over the
 * background's; index 0 is clear in both, and the screen shows the backdrop, colour 0 of palette 0,
 * only where both are. */
static unsigned MapColor(const uint16_t *tilemap, const uint8_t *tiles, const uint16_t *bgTilemap, const uint8_t *bgTiles,
                         int px, int py)
{
    int tx = px / 8, ty = py / 8, x = px % 8, y = py % 8;
    unsigned entry[2] = {tilemap[ty * MAP_TILEMAP_W + tx], bgTilemap[ty * MAP_TILEMAP_W + tx]};

    for (int layer = 0; layer < 2; ++layer) {
        unsigned tile = entry[layer] & 0x3FF, bank = entry[layer] >> 12, index = 0;
        int sx = entry[layer] & 0x400 ? 7 - x : x, sy = entry[layer] & 0x800 ? 7 - y : y;
        if (tile < MAP_TILES) {
            uint8_t byte = (layer ? bgTiles : tiles)[tile * 32 + sy * 4 + sx / 2];
            index = (sx & 1 ? byte >> 4 : byte) & 15;
        }
        if (index && bank * 16 + index < MAP_COLORS)
            return bank * 16 + index;
    }
    return 0;
}

extern int CtrPokeTouch_MapArt(int group, uint16_t *tilemap, uint8_t *tiles, uint16_t *palette, uint16_t *bgTilemap,
                               uint8_t *bgTiles);

#include "3ds_home_map_impl.h"

/* The map asked for (its group and the version of its art) and whether its art was usable; the render test sets group to -1 to have it built again. */
static struct {
    int group;
    int version;
    int ok;
} sMap = {-1, 0, 0};
static int sMapStarted;                 /* the game's thread: the build of the map asked for was started (its art copied, HmBegin called) */

static uint16_t sMapTilemap[MAP_TILEMAP_W * MAP_TILEMAP_H];
static uint8_t sMapTiles[MAP_TILES * 32];
static uint16_t sMapBgTilemap[MAP_TILEMAP_W * MAP_TILEMAP_H];
static uint8_t sMapBgTiles[MAP_TILES * 32];
static uint16_t sMapGba[MAP_COLORS];

/* The worker. CTR_MAP_THREADS is what the host test defines (with a pthread version of <3ds/thread.h>) to run the console's code. */
#if defined(__3DS__) || defined(CTR_MAP_THREADS)
#include <3ds/thread.h>
#define MAP_THREADS 1
static Thread sMapThread;               /* the game's thread's handle of the worker; NULL when there is none (not yet joined once it ended) */
static int sMapRunning;                 /* shared: 1 from before the worker is created until its last act */
static int sMapAbort;                   /* shared: the game's thread asks the worker to stop */
#define MAP_ABORTED() (__atomic_load_n(&sMapAbort, __ATOMIC_RELAXED) != 0)
#else
#define MAP_THREADS 0
#define MAP_ABORTED() 0
#endif

/* The picture's 192x144 part, pixel by pixel, to the image's builder (from the copies of the game's art above); false when a stop was asked, between
 * two rows, so that a worker told to stop does not first read the whole of a map nobody wants now. */
static int MapFeedPixels(void)
{
    for (int y = 0; y < HM_SH; ++y) {
        if (MAP_ABORTED())
            return 0;
        for (int x = 0; x < HM_SW; ++x) {
            unsigned c = sMapGba[MapColor(sMapTilemap, sMapTiles, sMapBgTilemap, sMapBgTiles, HM_X0 + x, HM_Y0 + y)], r = c & 31, g = c >> 5 & 31, b = c >> 10 & 31;

            HmFeed(x, y, (int)(r << 3 | r >> 2), (int)(g << 3 | g >> 2), (int)(b << 3 | b >> 2));
        }
    }
    return 1;
}

#if MAP_THREADS
static void MapWorker(void *arg)
{
    (void)arg;
    if (MapFeedPixels())
        while (!MAP_ABORTED() && !HmDone() && HM_STAGE() != HMS_FAILED)
            HmStep();
    __atomic_store_n(&sMapRunning, 0, __ATOMIC_RELEASE);        /* the last act: nothing of the worker's is touched after it */
}

/* Asks the worker to stop; does not wait. */
static void MapWorkerStop(void)
{
    if (sMapThread)
        __atomic_store_n(&sMapAbort, 1, __ATOMIC_RELAXED);
}

/* True when no worker runs: one that ended is joined and freed here. False while one still runs (stopping or not). */
static int MapWorkerIdle(void)
{
    if (!sMapThread)
        return 1;
    if (__atomic_load_n(&sMapRunning, __ATOMIC_ACQUIRE))
        return 0;
    threadJoin(sMapThread, U64_MAX);
    threadFree(sMapThread);
    sMapThread = NULL;
    return 1;
}

/* The end of the process (and nothing else): stops the worker and waits for it, which takes at most one row of the feed or one step. */
static void MapJobShutdown(void)
{
    MapWorkerStop();
    if (sMapThread) {
        threadJoin(sMapThread, U64_MAX);
        threadFree(sMapThread);
        sMapThread = NULL;
    }
    __atomic_store_n(&sMapRunning, 0, __ATOMIC_RELAXED);
    sMapStarted = 0;
}
#else
static void MapWorkerStop(void) {}
static int MapWorkerIdle(void) { return 1; }
static void MapJobShutdown(void) { sMapStarted = 0; }
#endif

/* Starts the build of the map asked for; false when it cannot be now (a worker for another map still stops) or at all (no art: sMap.ok is 0). */
static int MapJobStart(void)
{
    if (!MapWorkerIdle())
        return 0;                       /* the worker of the map before this one is still stopping: the next frame asks again */
    if (sMap.group < 0 || sMap.group >= PT_MAP_COUNT
        || !CtrPokeTouch_MapArt(sMap.group, sMapTilemap, sMapTiles, sMapGba, sMapBgTilemap, sMapBgTiles)) {
        sMap.ok = 0;
        sMapStarted = 1;                /* decided: nothing to build */
        return 0;
    }
    HmBegin();
#if MAP_THREADS
    __atomic_store_n(&sMapAbort, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&sMapRunning, 1, __ATOMIC_RELEASE);
    sMapThread = threadCreate(MapWorker, NULL, 64 * 1024, 0x3F, -2, false);
    if (!sMapThread) {                  /* no worker (out of memory): the frames do it, as on the host */
        __atomic_store_n(&sMapRunning, 0, __ATOMIC_RELAXED);
        MapFeedPixels();
    }
#else
    MapFeedPixels();
#endif
    sMapStarted = 1;
    return 1;
}

/* One frame's work on the map's image; true once the image of the map asked for is there (HOME calls it for the map it shows, every frame). A map
 * changed while another builds stops that build and starts the new one as soon as the worker is gone. */
static bool MapJobPrepare(int group, int version)
{
    if (sMap.group != group || sMap.version != version) {
        sMap.group = group;
        sMap.version = version;
        sMap.ok = 1;                    /* until its art says otherwise */
        sMapStarted = 0;                /* what is in the image now is not this map's */
        MapWorkerStop();
    }
    if (!sMapStarted && !MapJobStart())
        return false;
    if (!sMap.ok)
        return false;
    if (HM_STAGE() == HMS_FAILED) {
        sMap.ok = 0;
        return false;
    }
#if MAP_THREADS
    if (sMapThread) {
        MapWorkerIdle();                /* a worker that has ended is joined and freed now, not at the next map */
        return HmDone() != 0;
    }
#endif
    HmStep();                           /* with a worker, the worker steps; this only asks whether it is done */
    return HmDone() != 0;
}

/* The image of the map asked for is built and is the one that is shown (never the previous map's while a new one is pending). */
static int MapJobImageReady(void)
{
    return sMapStarted && sMap.ok && HmDone();
}
