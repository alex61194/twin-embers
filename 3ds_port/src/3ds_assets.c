#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "3ds_assets.h"
#include "3ds_data.h"
#include "3ds_pak.h"
#include "3ds_log.h"
#include "3ds_perf.h"
#if defined(__3DS__)
#include <3ds.h>
typedef LightLock AssetLock;
typedef CondVar AssetCond;
typedef Thread AssetThread;
#define ASSET_LOCK_INIT(l) LightLock_Init(l)
#define ASSET_LOCK(l) LightLock_Lock(l)
#define ASSET_UNLOCK(l) LightLock_Unlock(l)
#define ASSET_COND_INIT(c) CondVar_Init(c)
#define ASSET_COND_WAIT(c, l) CondVar_Wait((c), (l))
#define ASSET_COND_WAKE_ALL(c) CondVar_WakeUp((c), ARBITRATION_SIGNAL_ALL)
/* Lowest-priority worker, like the log writer: never competes with the game. */
#define ASSET_THREAD_START(t, fn) ((*(t) = threadCreate(fn, NULL, 16 * 1024, 0x3F, -2, false)) != NULL)
/* Bounded: exit must never hang the console on a stuck worker. */
#define ASSET_THREAD_JOIN(t) (R_SUCCEEDED(threadJoin(t, 3000000000ULL)) ? (threadFree(t), true) : false)
#define ASSET_THREAD_FN(name) static void name(void *arg)
#define ASSET_THREAD_RETURN return
#else
#include <pthread.h>
typedef pthread_mutex_t AssetLock;
typedef pthread_cond_t AssetCond;
typedef pthread_t AssetThread;
#define ASSET_LOCK_INIT(l) pthread_mutex_init(l, NULL)
#define ASSET_LOCK(l) pthread_mutex_lock(l)
#define ASSET_UNLOCK(l) pthread_mutex_unlock(l)
#define ASSET_COND_INIT(c) pthread_cond_init(c, NULL)
#define ASSET_COND_WAIT(c, l) pthread_cond_wait((c), (l))
#define ASSET_COND_WAKE_ALL(c) pthread_cond_broadcast(c)
#define ASSET_THREAD_START(t, fn) (pthread_create(t, NULL, fn, NULL) == 0)
#define ASSET_THREAD_JOIN(t) (pthread_join(t, NULL) == 0)
#define ASSET_THREAD_FN(name) static void *name(void *arg)
#define ASSET_THREAD_RETURN return NULL
#endif

extern unsigned char __bss_start__[], __bss_end__[];
#ifndef CTR_ASSETS_INDEX_PATH
#define CTR_ASSETS_INDEX_PATH "romfs:/engine/assets.bin"
#endif
#ifndef CTR_ASSETS_BSS_START
#define CTR_ASSETS_BSS_START __bss_start__
#define CTR_ASSETS_BSS_END __bss_end__
#else
extern unsigned char testStorage[];
#endif
struct Header { uint32_t magic, version, reference, count, strings, crc; };
struct Record { uint32_t address, size, path, crc, group; };
_Static_assert(sizeof(struct Header) == 24, "asset header layout");
_Static_assert(sizeof(struct Record) == 20, "asset record layout");

static unsigned char *sBody;
/* Per-record state: IDLE, BUSY (one thread owns the read) or LOADED. Changed
 * only under sAssetLock; the LOADED fast path reads it with acquire. */
enum { RECORD_IDLE, RECORD_BUSY, RECORD_LOADED };
static unsigned char *sLoaded;
static AssetLock sAssetLock;
static AssetCond sAssetDone;
static AssetThread sWarmThread;
static bool sWarmRunning;
static bool sWarmStop;
#define WARM_STOPPED() __atomic_load_n(&sWarmStop, __ATOMIC_ACQUIRE)
static struct Record *sRecords;
static const char *sStrings;
static uint32_t sCount;
static uintptr_t sDelta, sFirst, sEnd;

void CtrAssets_Shutdown(void)
{
    /* The warm-up worker reads the tables: stop and join it first. */
    if (sWarmRunning)
    {
        __atomic_store_n(&sWarmStop, true, __ATOMIC_RELEASE);
        if (!ASSET_THREAD_JOIN(sWarmThread))
            return; /* still running: leave the tables it reads alone */
        sWarmRunning = false;
    }
    free(sBody);
    free(sLoaded);
    sBody = sLoaded = NULL;
    sRecords = NULL;
    sStrings = NULL;
    sCount = 0;
    sDelta = sFirst = sEnd = 0;
}

static uintptr_t Address(uint32_t i) { return sRecords[i].address + sDelta; }

/* The read and CRC check of one record into its reserved storage. The caller
 * owns the record (RECORD_BUSY). The warm-up worker leaves the perf counters
 * to the game thread. */
static bool ReadRecord(uint32_t i, bool warm)
{
    const struct Record *row = &sRecords[i];
    const char *path = sStrings + row->path;
    uint32_t crc;
    uint64_t perfStart = warm ? 0 : CtrPerf_Begin();
    if (!CtrData_ReadIntoCrc(path, (void *)Address(i), row->size, &crc) || crc != row->crc)
    {
        if (!warm)
        {
            CtrPerf_AssetEnd(perfStart);
            CTR_PERF_COUNT(PERF_ASSET_FAILURES, 1);
        }
        CtrLog_Write(CTR_LOG_ERROR, "asset payload mismatch: %s", path);
        return false;
    }
    if (warm) return true;
    CTR_PERF_COUNT(PERF_ASSETS_LOADED, 1);
    CTR_PERF_COUNT(PERF_ASSET_BYTES, row->size);
    uint64_t elapsed = CtrPerf_AssetEnd(perfStart);
    (void)elapsed;
#if defined(PLATFORM_3DS) || (defined(CTR_PERF_ENABLED) && CTR_PERF_ENABLED)
    /* Successful lazy loads happen once per record/session. The diagnostic
     * is additionally rate limited; all loads still enter frame aggregates. */
    if (CtrPerf_SlowAsset(elapsed))
        CtrLog_Write(CTR_LOG_FS, "PERF_ASSET frame=%lu id=%lu bytes=%lu ticks=%llu backend=%s path=%s",
            (unsigned long)gCtrPerf.frame, (unsigned long)i, (unsigned long)row->size,
            (unsigned long long)elapsed, CtrData_BackendName(), path);
#endif
    return true;
}

/* Claims an idle record, reads it without holding the lock, publishes the
 * result and wakes waiters. The game thread waits for a record another thread
 * is reading (only that record), then rechecks: if that read failed the
 * record is idle again and it retries it itself. The warm-up worker skips
 * records already loaded or owned. */
enum { LOAD_FAILED, LOAD_SKIPPED, LOAD_DONE };

static int AcquireRecord(uint32_t i, bool warm)
{
    if (__atomic_load_n(&sLoaded[i], __ATOMIC_ACQUIRE) == RECORD_LOADED) return LOAD_SKIPPED;
    ASSET_LOCK(&sAssetLock);
    while (!warm && sLoaded[i] == RECORD_BUSY) ASSET_COND_WAIT(&sAssetDone, &sAssetLock);
    if (sLoaded[i] != RECORD_IDLE)
    {
        ASSET_UNLOCK(&sAssetLock);
        return LOAD_SKIPPED;
    }
    sLoaded[i] = RECORD_BUSY;
    ASSET_UNLOCK(&sAssetLock);
    bool ok = ReadRecord(i, warm);
    ASSET_LOCK(&sAssetLock);
    __atomic_store_n(&sLoaded[i], ok ? RECORD_LOADED : RECORD_IDLE, __ATOMIC_RELEASE);
    ASSET_COND_WAKE_ALL(&sAssetDone);
    ASSET_UNLOCK(&sAssetLock);
    return ok ? LOAD_DONE : LOAD_FAILED;
}

static bool LoadRecord(uint32_t i) { return AcquireRecord(i, false) != LOAD_FAILED; }

/* Records the warm-up preloads, in this group order and, within a group, in
 * path order so the PAK read-ahead window serves neighbouring payloads. */
static int ComparePaths(const void *a, const void *b)
{
    uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;
    int order = strcmp(sStrings + sRecords[x].path, sStrings + sRecords[y].path);
    return order ? order : (x > y) - (x < y);
}

ASSET_THREAD_FN(WarmMain)
{
    static const unsigned char groups[] = {4, 3, 2, 5};
    uint32_t *ids = malloc((sCount ? sCount : 1) * sizeof(*ids));
    unsigned long records = 0, bytes = 0;
    (void)arg;
    for (unsigned g = 0; ids && g < sizeof(groups) && !WARM_STOPPED(); ++g)
    {
        uint32_t count = 0;
        for (uint32_t i = 0; i < sCount; ++i)
            if ((sRecords[i].group & 255) == groups[g]) ids[count++] = i;
        qsort(ids, count, sizeof(*ids), ComparePaths);
        for (uint32_t k = 0; k < count && !WARM_STOPPED(); ++k)
            if (AcquireRecord(ids[k], true) == LOAD_DONE)
            {
                ++records;
                bytes += sRecords[ids[k]].size;
            }
    }
    free(ids);
    if (!WARM_STOPPED())
        CtrLog_Write(CTR_LOG_FS, "asset warm-up complete: %lu records / %lu bytes", records, bytes);
    ASSET_THREAD_RETURN;
}

void CtrAssets_StartWarmup(void)
{
    if (!sBody || sWarmRunning) return;
    __atomic_store_n(&sWarmStop, false, __ATOMIC_RELEASE);
    if (!ASSET_THREAD_START(&sWarmThread, WarmMain)) return;
    sWarmRunning = true;
    CtrLog_Write(CTR_LOG_FS, "asset warm-up started");
}

bool CtrAssets_EnsureRange(const void *source, size_t bytes)
{
    uintptr_t start = (uintptr_t)source;
    if (!sBody || !bytes) return true;
    if (bytes > UINTPTR_MAX - start) return false;
    uintptr_t end = start + bytes;
    if (start >= sEnd || end <= sFirst) return true;
    /* Find the first overlapping resident array/file slice. An interior
     * pointer resolves to the same reserved storage, without rewriting it. */
    uint32_t lo = 0, hi = sCount;
    while (lo < hi)
    {
        uint32_t mid = lo + (hi - lo) / 2;
        if (Address(mid) + sRecords[mid].size <= start) lo = mid + 1;
        else hi = mid;
    }
    while (lo < sCount && Address(lo) < end)
    {
        if (!LoadRecord(lo)) return false;
        ++lo;
    }
    return true;
}

bool CtrAssets_Init(uintptr_t referenceRuntime)
{
    FILE *file = fopen(CTR_ASSETS_INDEX_PATH, "rb");
    struct Header header;
    unsigned char *body = NULL;
    bool ok = false;
    CtrAssets_Shutdown();
    if (!file || fread(&header, 1, sizeof(header), file) != sizeof(header)
        || header.magic != 0x53413346u || header.version != 1
        || header.count > 65536 || header.strings > 8 * 1024 * 1024)
        goto done;
    size_t records = header.count * sizeof(struct Record);
    size_t bytes = records + header.strings;
    body = malloc(bytes ? bytes : 1);
    if (!body || fread(body, 1, bytes, file) != bytes || fgetc(file) != EOF
        || CtrPak_Crc32(0, body, bytes) != header.crc)
        goto done;
    uintptr_t delta = referenceRuntime - header.reference, previousEnd = 0;
    for (uint32_t i = 0; i < header.count; ++i)
    {
        struct Record row;
        memcpy(&row, body + i * sizeof(row), sizeof(row));
        uintptr_t address = row.address + delta;
        uint32_t group = row.group & 255, prefix = row.group >> 24;
        if (!group || group > 5 || prefix > 1
            || (!prefix && row.group != group)
            || (prefix && (group != 5 || row.size != 32
                || address < (uintptr_t)CTR_ASSETS_BSS_START + 2))
            || !row.size || row.path >= header.strings
            || !memchr(body + records + row.path, 0, header.strings - row.path)
            || address < (uintptr_t)CTR_ASSETS_BSS_START || address > (uintptr_t)CTR_ASSETS_BSS_END
            || row.size > (uintptr_t)CTR_ASSETS_BSS_END - address)
            goto done;
        if (i && address - (prefix ? 2 : 0) < previousEnd) goto done;
        previousEnd = address + row.size;
    }
    ASSET_LOCK_INIT(&sAssetLock);
    ASSET_COND_INIT(&sAssetDone);
    sLoaded = calloc(header.count ? header.count : 1, 1);
    if (!sLoaded) goto done;
    sBody = body;
    body = NULL;
    sRecords = (struct Record *)sBody;
    sStrings = (const char *)sBody + records;
    sCount = header.count;
    sDelta = delta;
    sFirst = sCount ? Address(0) : 0;
    sEnd = previousEnd;
    /* Spinda's eight coordinate bytes are graphics metadata. Restore them
     * in the original inline bitmap structures without loading the files. */
    for (uint32_t i = 0; i < sCount; ++i)
        if (sRecords[i].group >> 24)
        {
            unsigned char *prefix = (unsigned char *)Address(i) - 2;
            prefix[0] = (unsigned char)(sRecords[i].group >> 8);
            prefix[1] = (unsigned char)(sRecords[i].group >> 16);
        }
    /* Only directly indexed common fonts/UI are resident at startup.
     * All later groups remain zero-filled until their consumers access them. */
    for (uint32_t i = 0; i < sCount; ++i)
        if (sRecords[i].group == 1 && !LoadRecord(i)) goto done;
    CtrLog_Write(CTR_LOG_FS, "assets: %lu resident ranges indexed", (unsigned long)sCount);
    ok = true;
done:
    if (file) fclose(file);
    free(body);
    if (!ok) CtrAssets_Shutdown();
    return ok;
}
