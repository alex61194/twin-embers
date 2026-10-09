#include <assert.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "3ds_dma.h"
#include "gba_shadow.h"
#include "3ds_assets.h"
#include "3ds_data.h"
#include "3ds_pak.h"
#include "3ds_log.h"
#include "3ds_perf.h"
#if CTR_PERF_ENABLED
uint64_t CtrPerf_PlatformClock(void) { static uint64_t now; return now += 2500; }
bool CtrLog_SetDeferredFormatter(CtrLogDeferredFormatter fn) { (void)fn; return true; }
void CtrLog_DeferredLock(void) {}
void CtrLog_DeferredUnlock(void) {}
void CtrLog_WakeDeferred(void) {}
const char *CtrData_BackendName(void) { return "host"; }
#endif
#ifdef CTR_TEST_BLIT
#include "global.h"
#include "blit.h"
#endif

unsigned char testStorage[1024];
static volatile unsigned reads;
/* Warm-up probes: the order paths are read in, a path whose read blocks until
 * released (gate), and a path whose first read fails. */
static const char *readOrder[16];
static const char *gatePath, *failOncePath;
static volatile int gateHit, gateRelease, gateDone, failedOnce;
static unsigned PathReads(const char *path)
{
    unsigned n = 0;
    for (unsigned i = 0; i < reads && i < 16; ++i) n += !strcmp(readOrder[i], path);
    return n;
}
static void *Release(void *arg)
{
    (void)arg;
    usleep(100000);
    gateRelease = 1;
    return NULL;
}
static void WaitFor(volatile int *flag)
{
    for (unsigned i = 0; i < 5000 && !*flag; ++i) usleep(1000);
    assert(*flag);
}
void CpuSet(const void *, void *, uint32_t);
void CpuFastSet(const void *, void *, uint32_t);
void LZ77UnCompWram(const void *, void *);
#ifdef CTR_TEST_SPINDA_DRAW
void TestDrawSpinda(uint32_t, unsigned char *);
void TestDrawSpindaReference(uint32_t, unsigned char *);
#endif
void CtrPlatform_ShowDataError(const char *title, const char *detail)
{ (void)title; (void)detail; abort(); }
void CtrLog_Write(CtrLogCategory category, const char *format, ...)
{ (void)category; (void)format; }
bool CtrData_ReadIntoCrc(const char *path, void *buffer, uint32_t size, uint32_t *crc)
{
    unsigned index = __atomic_fetch_add(&reads, 1, __ATOMIC_SEQ_CST);
    if (index < 16) readOrder[index] = strdup(path);
    if (failOncePath && !strcmp(path, failOncePath) && !failedOnce) { failedOnce = 1; return false; }
    if (gatePath && !strcmp(path, gatePath))
    {
        gateHit = 1;
        while (!gateRelease) usleep(1000);
    }
    FILE *file = fopen(path, "rb");
    if (!file) return false;
    bool ok = fread(buffer, 1, size, file) == size && fgetc(file) == EOF;
    fclose(file);
    if (ok) *crc = CtrPak_Crc32(0, buffer, size);
    if (gatePath && !strcmp(path, gatePath)) gateDone = 1;
    return ok;
}
int main(int argc, char **argv)
{
    assert(argc == 2);
#if CTR_PERF_ENABLED
    CtrPerf_Init(1000000, 0);
#endif
    memset(testStorage, 0xa5, sizeof(testStorage));
    bool ok = CtrAssets_Init((uintptr_t)testStorage);
    if (!strcmp(argv[1], "lazy-good") || !strcmp(argv[1], "lazy-bios"))
    {
        assert(ok && reads == 0 && testStorage[16] == 0xa5);
        unsigned char copied[32];
        CpuSet(testStorage + 16, copied, 8);
        assert(reads == 1);
        for (unsigned i = 0; i < 16; ++i) assert(copied[i] == i);
        assert(CtrAssets_EnsureRange(testStorage + 19, 4) && reads == 1);
#if CTR_PERF_ENABLED
        assert(gCtrPerf.counters[PERF_ASSETS_LOADED] == 1);
        assert(gCtrPerf.counters[PERF_ASSET_BYTES] == 16);
        assert(gCtrPerf.maxAssetTicks == 2500);
        assert(gCtrPerf.ticks[PERF_ASSET_LOAD] == 2500);
#endif
        GbaShadow_Reset();
        CtrDma_Reset();
        CtrDma_Set(3, testStorage + 28, copied, 0x84800004u);
        for (unsigned i = 0; i < 4; ++i)
            for (unsigned j = 0; j < 4; ++j) assert(copied[i * 4 + j] == (3 - i) * 4 + j);
        CpuFastSet(testStorage + 16, copied, 0x01000008u);
        for (unsigned i = 0; i < 32; ++i) assert(copied[i] == i % 4);
        assert(reads == 1 && CtrAssets_EnsureRange(copied, sizeof(copied)));
        assert(!CtrAssets_EnsureRange((void *)(UINTPTR_MAX - 3), 8));
    }
    else if (!strcmp(argv[1], "lazy-lz"))
    {
        assert(ok && reads == 0 && testStorage[16] == 0xa5);
        unsigned char decoded[4] = {0};
        LZ77UnCompWram(testStorage + 16, decoded);
        assert(reads == 1);
        for (unsigned i = 0; i < 4; ++i) assert(decoded[i] == 10 + i);
    }
#ifdef CTR_TEST_BLIT
    else if (!strcmp(argv[1], "lazy-blit"))
    {
        assert(ok && reads == 0);
        unsigned char copied[64] = {0};
        struct Bitmap src = {testStorage + 16, 128, 128};
        struct Bitmap dst = {copied, 8, 8};
        BlitBitmapRect4Bit(&src, &dst, 0, 0, 0, 0, 8, 8, 0xff);
        assert(reads == 1);
        for (unsigned i = 0; i < 32; ++i) assert(copied[i] == i);
        BlitBitmapRect4BitTo8Bit(&src, &dst, 0, 0, 0, 0, 8, 8, 0xff, 1);
        assert(reads == 1 && testStorage[48] == 0xa5);
        for (unsigned i = 0; i < 64; ++i)
            assert(copied[i] == 16 + ((i / 2 >> ((i & 1) * 4)) & 15));
    }
#endif
    else if (!strcmp(argv[1], "spinda-good"))
    {
        assert(ok && reads == 0);
        assert(testStorage[14] == 16 && testStorage[15] == 7 && testStorage[16] == 0xa5);
#ifdef CTR_TEST_SPINDA_DRAW
        unsigned char actual[2048], expected[2048];
        const uint32_t personalities[] = {0x88, 0x12, 0xff};
        for (unsigned i = 0; i < 3; ++i)
        {
            memset(actual, 0x11, sizeof(actual));
            memset(expected, 0x11, sizeof(expected));
            TestDrawSpinda(personalities[i], actual);
            TestDrawSpindaReference(personalities[i], expected);
            assert(reads == 1 && !memcmp(actual, expected, sizeof(actual)));
        }
#endif
        assert(CtrAssets_EnsureRange(testStorage + 14, 34) && reads == 1);
        for (unsigned i = 0; i < 32; ++i) assert(testStorage[16 + i] == i);
        assert(testStorage[14] == 16 && testStorage[15] == 7 && testStorage[48] == 0xa5);
    }
    else if (!strncmp(argv[1], "warm-", 5))
    {
        pthread_t releaser;
        assert(ok && reads == 1 && !strcmp(readOrder[0], "graphics/fonts/f1.bin"));
        if (!strcmp(argv[1], "warm-order"))
        {
            CtrAssets_StartWarmup();
            for (unsigned i = 0; i < 5000 && reads < 6; ++i) usleep(1000);
            usleep(50000);
            static const char *expect[] = {"graphics/fonts/f1.bin", "graphics/d4.bin", "graphics/c3a.bin",
                "graphics/c3b.bin", "graphics/b2.bin", "graphics/z5.bin"};
            assert(reads == 6);
            for (unsigned i = 0; i < 6; ++i) assert(!strcmp(readOrder[i], expect[i]));
            /* Everything is resident: neither the lazy path nor a second
             * warm-up reads anything again. */
            assert(CtrAssets_EnsureRange(testStorage + 16, 6 * 16));
            CtrAssets_StartWarmup();
            usleep(50000);
            assert(reads == 6);
            for (unsigned i = 0; i < 6; ++i) assert(testStorage[16 + i * 16 + 1] == 1);
        }
        else if (!strcmp(argv[1], "warm-claim"))
        {
            gatePath = "graphics/slow.bin";
            CtrAssets_StartWarmup();
            WaitFor(&gateHit);
            pthread_create(&releaser, NULL, Release, NULL);
            /* The worker owns the record: wait for it, do not read it twice. */
            assert(CtrAssets_EnsureRange(testStorage + 32, 16));
            pthread_join(releaser, NULL);
            assert(reads == 2 && PathReads(gatePath) == 1 && testStorage[33] == 1);
        }
        else if (!strcmp(argv[1], "warm-fail"))
        {
            failOncePath = "graphics/once.bin";
            CtrAssets_StartWarmup();
            WaitFor(&failedOnce);
            usleep(100000);
            /* The failed warm read left the record unloaded and retryable. */
            assert(reads == 2 && testStorage[32] == 0xa5);
            assert(CtrAssets_EnsureRange(testStorage + 32, 16));
            assert(reads == 3 && testStorage[33] == 1);
        }
        else if (!strcmp(argv[1], "warm-shutdown"))
        {
            gatePath = "graphics/slow.bin";
            CtrAssets_StartWarmup();
            WaitFor(&gateHit);
            pthread_create(&releaser, NULL, Release, NULL);
            /* Shutdown must wait for the read in flight before freeing. */
            CtrAssets_Shutdown();
            assert(gateDone);
            pthread_join(releaser, NULL);
        }
    }
    else if (!strcmp(argv[1], "lazy-bad"))
    {
        assert(ok && reads == 0);
        assert(!CtrAssets_EnsureRange(testStorage + 17, 1) && reads == 1);
#if CTR_PERF_ENABLED
        assert(gCtrPerf.counters[PERF_ASSET_FAILURES] == 1);
        assert(gCtrPerf.counters[PERF_ASSETS_LOADED] == 0);
#endif
    }
    else if (!strcmp(argv[1], "adjacent"))
    {
        assert(ok && reads == 1 && testStorage[32] == 0xa5);
        assert(CtrAssets_EnsureRange(testStorage + 28, 8) && reads == 2);
        for (unsigned i = 0; i < 32; ++i) assert(testStorage[16 + i] == i % 16);
    }
    else if (!strcmp(argv[1], "good"))
    {
        assert(ok && reads == 1);
        for (unsigned i = 0; i < 16; ++i) assert(testStorage[16 + i] == i);
        assert(testStorage[15] == 0xa5 && testStorage[32] == 0xa5);
    }
    else
    {
        assert(!ok);
        if (!strcmp(argv[1], "metadata-bad")) assert(reads == 0);
    }
    CtrAssets_Shutdown();
    return 0;
}
