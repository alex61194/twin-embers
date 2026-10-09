/* Aggregate only; no rendering decisions or game/audio changes. */
#include "3ds_perf.h"
#include "3ds_log.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

CtrPerfFrame gCtrPerf;
static bool sActive;
static char sLine[4096];
static size_t sLineUsed;
static void Append(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int n = vsnprintf(sLine+sLineUsed, sizeof(sLine)-sLineUsed, format, args);
    va_end(args);
    if (n > 0 && (size_t)n < sizeof(sLine)-sLineUsed) sLineUsed += n;
}
static void Emit(void)
{
    CtrLog_Write(CTR_LOG_VIDEO, "PERF_CSV %s", sLine);
    sLineUsed = 0;
    sLine[0] = 0;
}
static uint64_t sFrequency, sStart, sLastFlush;
static uintptr_t sMemory[3];
static double sClockNs;
static const char *const sTimes[] = {
    "game", "callback", "tasks", "animate", "bg", "bgAffine", "obj", "objAffine",
    "window", "palette", "prepare", "cacheSample", "decodeSample", "drawSample",
    "submit", "blend", "dma", "decompress", "scanline", "vblank", "audio", "render", "gpuWait", "finalize", "log",
    "assetLoad", "dataRead", "dataCrc", "audioBridge", "ndspSubmit", "objWindow"
};
static const char *const sCounters[] = {
    "cacheRequest", "cacheHit", "cacheMiss", "regenerate", "paletteInvalidate",
    "vramInvalidate", "cacheSamples", "decodeSamples", "drawSamples", "atlasWrittenBytes",
    "vramReadBytes", "paletteColors", "quads", "flushRequests", "targetSwitches",
    "textureSwitches", "oamVisits", "sprites", "affineObjects", "windowPixels",
    "dmaBytes", "vramBytes", "fullVramCopies", "paletteBytes", "oamBytes", "otherBytes", "tickReads", "resourceCreate", "resourceRelease",
    "assetsLoaded", "assetBytes", "assetFailures", "dataReadBytes", "bgLayers", "frameSplits", "pakReads", "pakPhysReads"
};
_Static_assert(sizeof(sTimes)/sizeof(*sTimes) == PERF_TIME_COUNT, "time columns");
_Static_assert(sizeof(sCounters)/sizeof(*sCounters) == PERF_COUNTER_COUNT, "counter columns");

typedef struct {
    uintptr_t cb, task;
    unsigned display, stereo, planes, frames, first, last, peakFrame;
    uint64_t wall, cpu, maxCpu, ticks[PERF_TIME_COUNT], counters[PERF_COUNTER_COUNT];
    unsigned over[4];
    double gpu, maxGpu, commands;
} Record;
static Record sRecords[32];
static unsigned sRecordsUsed;
/* Immutable CSV snapshots, consumed by the existing async logger. Queue pressure
 * drops CSV records with an explicit cumulative count; it never blocks gameplay.
 * Summary/spike counters still measure every frame. */
#define CSV_QUEUE_RECORDS 64
static Record sCsvQueue[CSV_QUEUE_RECORDS];
static unsigned sCsvHead, sCsvCount, sCsvDropped;

static double Ms(uint64_t ticks) { return (double)ticks * 1000.0 / sFrequency; }

static void FormatCsv(const Record *r)
{
    double n = r->frames;
    Append("%u,%u,%u,%lx,%lx,%x,%u,%u,%.3f,%.3f,%.3f,%.3f,%u,%.3f,%.3f,%u,%u,%u,%u",
        r->first, r->last, r->frames, (unsigned long)r->cb, (unsigned long)r->task,
        r->display, r->stereo, r->planes, n * sFrequency / r->wall,
        Ms(r->wall)/n, Ms(r->cpu)/n, Ms(r->maxCpu), r->peakFrame, r->gpu/n, r->maxGpu,
        r->over[0], r->over[1], r->over[2], r->over[3]);
    for (unsigned t = 0; t < PERF_TIME_COUNT; ++t) Append(",%.6f", Ms(r->ticks[t])/n);
    for (unsigned c = 0; c < PERF_COUNTER_COUNT; ++c) Append(",%.3f", r->counters[c]/n);
    Append(",%.4f,%.6f", r->commands/n,
        r->counters[PERF_TICK_READS] * sClockNs / 1000000.0 / n);
}

/* Only the logger thread (or its shutdown drain after joining it) calls this.
 * The caller supplies its existing 64 KiB output batch; no formatting, file I/O
 * or allocations happen under the short snapshot-copy lock. */
static size_t FormatDeferredCsv(char *buffer, size_t capacity)
{
    size_t used = 0;
    while (capacity-used >= sizeof(sLine)+16)
    {
        Record row;
        CtrLog_DeferredLock();
        bool available = sCsvCount != 0;
        if (available)
        {
            row = sCsvQueue[sCsvHead];
            sCsvHead = (sCsvHead+1) % CSV_QUEUE_RECORDS;
            --sCsvCount;
        }
        CtrLog_DeferredUnlock();
        if (!available) break;
        sLineUsed = 0;
        sLine[0] = 0;
        FormatCsv(&row);
        int n = snprintf(buffer+used, capacity-used, "PERF_CSV %s\n", sLine);
        if (n > 0 && (size_t)n < capacity-used) used += n;
    }
    return used;
}

static void Flush(void)
{
    uint64_t start = CtrPerf_PlatformClock();
    CtrLog_DeferredLock();
    for (unsigned i = 0; i < sRecordsUsed; ++i)
    {
        if (sCsvCount == CSV_QUEUE_RECORDS) { ++sCsvDropped; continue; }
        sCsvQueue[(sCsvHead+sCsvCount) % CSV_QUEUE_RECORDS] = sRecords[i];
        ++sCsvCount;
    }
    CtrLog_DeferredUnlock();
    sRecordsUsed = 0;
    /* Each new active record is initialized when allocated; old slots need not
     * be cleared on every flush. Only snapshot copies run on the main thread. */
    CtrLog_WakeDeferred();
    gCtrPerf.ticks[PERF_LOG] += CtrPerf_PlatformClock() - start;
    sLastFlush = gCtrPerf.frame;
}

static void Shutdown(void)
{
    if (!sActive) return;
    if (sRecordsUsed) Flush();
    CtrLog_Write(CTR_LOG_VIDEO, "PERF_FINAL frames=%lu csvDropped=%u",
        (unsigned long)gCtrPerf.frame, sCsvDropped);
    sActive = false;
}

void CtrPerf_Init(uint64_t frequency, uintptr_t reference)
{
    sFrequency = frequency;
    /* Runtime opt-out allows the same executable to compare instrumentation
     * overhead without changing game/renderer code. */
    FILE *off = fopen("sdmc:/3ds/twinembers/perf.off", "rb");
    if (off) { fclose(off); return; }
    if (!CtrLog_SetDeferredFormatter(FormatDeferredCsv))
    {
        CtrLog_Write(CTR_LOG_VIDEO, "PERF_DISABLED reason=asyncLoggerUnavailable remove=log.sync");
        return;
    }
    sActive = true;
    atexit(Shutdown);
    uint64_t start = CtrPerf_PlatformClock();
    for (unsigned i = 0; i < 1024; ++i) (void)CtrPerf_PlatformClock();
    sClockNs = (double)(CtrPerf_PlatformClock()-start) * 1e9 / frequency / 1024;
    Append("# PERF v3 baseline=a92e4f9008bd174edd77f9361902be08305dc21a reference=%lx tickHz=%llu clockNs=%.3f cacheDrawSample=1/32",
            (unsigned long)reference, (unsigned long long)frequency, sClockNs);
    Emit();
    Append("first,last,frames,cb,task,display,stereo,planes,fps,frameMs,cpuMs,maxCpuMs,maxCpuFrame,gpuCompletedMs,maxGpuCompletedMs,over16_67,over20,over25,over30");
    for (unsigned t = 0; t < PERF_TIME_COUNT; ++t) Append(",%sMs", sTimes[t]);
    for (unsigned c = 0; c < PERF_COUNTER_COUNT; ++c) Append(",%s", sCounters[c]);
    Append(",commandUsage,clockOverheadEstimateMs");
    Emit();
    CtrLog_Write(CTR_LOG_VIDEO, "PERF_CONFIG summary=600 spikeMs=17.5 severeMs=20 slowAssetMs=2 spikeLimit=8/60 slowAssetLimit=8/60 gpu=lastCompleted wait=FrameBeginCombined cpu=wallMinusWait inclusive=1 csvFormatter=loggerThread csvQueue=64 csvOverflow=dropAndCount");
    gCtrPerf.enabled = true;
    sStart = CtrPerf_PlatformClock();
}

void CtrPerf_SetMemory(const void *vram, const void *palette, const void *oam)
{ sMemory[0]=(uintptr_t)vram; sMemory[1]=(uintptr_t)palette; sMemory[2]=(uintptr_t)oam; }

void CtrPerf_Transfer(const void *dest, size_t bytes)
{
    uintptr_t p = (uintptr_t)dest;
    unsigned counter = PERF_OTHER_BYTES;
    if (p >= sMemory[0] && p-sMemory[0] < 0x18000)
    {
        counter = PERF_VRAM_BYTES;
        if (p == sMemory[0] && bytes >= 0x18000) ++gCtrPerf.counters[PERF_FULL_VRAM_COPY];
    }
    else if (p >= sMemory[1] && p-sMemory[1] < 0x400) counter=PERF_PALETTE_BYTES;
    else if (p >= sMemory[2] && p-sMemory[2] < 0x400) counter=PERF_OAM_BYTES;
    gCtrPerf.counters[counter] += bytes;
}

/* 600 fixed-point wall samples (2.4 KiB); sorting only at summary time.
 * Inclusive categories overlap; completed GPU time is asynchronous and is
 * never added to CPU or wall. Summary/logger cost remains visible next frame. */
#define WINDOW 600
static struct {
    unsigned n, loads, steady, spikes, severe, quiet, assetQuiet;
    uint64_t wall, cpu, wait, asset, maxAsset, maxAssetFrame, maxWall, loadWall, steadyWall;
    uint64_t cacheRequest, cacheMiss, quads, uploads, ticks;
    double gpu, commands;
    uint64_t audio, submit;
    uint32_t samples[WINDOW], underruns, drops, queuedMin, queuedMax;
} sRolling;
static unsigned sSpikeLines, sAssetLines, sDiagnosticWindow;
static void DiagnosticWindow(void)
{
    unsigned window = gCtrPerf.frame / 60;
    if (window != sDiagnosticWindow)
    {
        sDiagnosticWindow = window;
        sSpikeLines = sAssetLines = 0;
    }
}

bool CtrPerf_SlowAsset(uint64_t ticks)
{
    if (!gCtrPerf.enabled || Ms(ticks) <= 2.0) return false;
    DiagnosticWindow();
    if (sAssetLines >= 8) { ++sRolling.assetQuiet; return false; }
    ++sAssetLines;
    return true;
}

static int CompareSample(const void *a, const void *b)
{
    uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;
    return (x > y) - (x < y);
}
static void Rolling(uint64_t wall, uint64_t cpu, float gpu, float commands,
                    uintptr_t cb, unsigned stereo, unsigned planes)
{
    unsigned n = sRolling.n++;
    double wallMs = Ms(wall);
    sRolling.samples[n] = wallMs < 4294967.0 ? (uint32_t)(wallMs*1000.0) : UINT32_MAX;
    sRolling.wall += wall; sRolling.cpu += cpu;
    sRolling.wait += gCtrPerf.ticks[PERF_GPU_WAIT];
    sRolling.gpu += gpu; sRolling.commands += commands;
    if (wall > sRolling.maxWall) sRolling.maxWall = wall;
    sRolling.asset += gCtrPerf.ticks[PERF_ASSET_LOAD];
    if (gCtrPerf.ticks[PERF_ASSET_LOAD] > sRolling.maxAssetFrame) sRolling.maxAssetFrame = gCtrPerf.ticks[PERF_ASSET_LOAD];
    if (gCtrPerf.maxAssetTicks > sRolling.maxAsset) sRolling.maxAsset = gCtrPerf.maxAssetTicks;
    if (gCtrPerf.counters[PERF_ASSETS_LOADED] || gCtrPerf.counters[PERF_ASSET_FAILURES])
    { ++sRolling.loads; sRolling.loadWall += wall; }
    else { ++sRolling.steady; sRolling.steadyWall += wall; }
    sRolling.cacheRequest += gCtrPerf.counters[PERF_CACHE_REQUEST];
    sRolling.cacheMiss += gCtrPerf.counters[PERF_CACHE_MISS];
    sRolling.quads += gCtrPerf.counters[PERF_QUADS];
    sRolling.uploads += gCtrPerf.counters[PERF_REGENERATE];
    sRolling.ticks += gCtrPerf.counters[PERF_TICK_READS];
    sRolling.audio += gCtrPerf.ticks[PERF_AUDIO];
    sRolling.submit += gCtrPerf.ticks[PERF_NDSP_SUBMIT];
    if (!n || gCtrPerf.audioQueued < sRolling.queuedMin) sRolling.queuedMin = gCtrPerf.audioQueued;
    if (gCtrPerf.audioQueued > sRolling.queuedMax) sRolling.queuedMax = gCtrPerf.audioQueued;
    DiagnosticWindow();
    if (wallMs > 17.5)
    {
        ++sRolling.spikes; sRolling.severe += wallMs > 20.0;
        if (sSpikeLines++ < 8)
        {
            uint64_t render = gCtrPerf.ticks[PERF_RENDER];
            uint64_t wait = gCtrPerf.ticks[PERF_GPU_WAIT];
            uint64_t renderActive = render > wait ? render-wait : 0;
            const uint64_t costs[] = {gCtrPerf.ticks[PERF_GAME], renderActive,
                                     gCtrPerf.ticks[PERF_VBLANK], wait};
            const char *const names[] = {"game", "renderActive", "vblank", "wait"};
            unsigned dominant = 0;
            for (unsigned i=1; i<sizeof(costs)/sizeof(*costs); ++i)
                if (costs[i] > costs[dominant]) dominant=i;
            CtrLog_Write(CTR_LOG_VIDEO,
                "PERF_SPIKE%s f=%lu cb=%lx wall=%.2f cpu=%.2f wait=%.2f gpuCompleted=%.2f dominant=%s game=%.2f render=%.2f bg=%.2f obj=%.2f win=%.2f asset=%.2f read=%.2f crc=%.2f decomp=%.2f audio=%.2f ndsp=%.2f loads=%lu bytes=%lu maxAsset=%.2f miss=%lu/%lu quads=%lu uploads=%lu stereo=%u planes=%u cmd=%.3f q=%lu underruns=%lu drops=%lu pak=%lu/%lu",
                wallMs>20.0 ? "!" : "", (unsigned long)gCtrPerf.frame, (unsigned long)cb,
                wallMs, Ms(cpu), Ms(wait), (double)gpu, names[dominant],
                Ms(gCtrPerf.ticks[PERF_GAME]), Ms(gCtrPerf.ticks[PERF_RENDER]),
                Ms(gCtrPerf.ticks[PERF_BG]), Ms(gCtrPerf.ticks[PERF_OBJ]), Ms(gCtrPerf.ticks[PERF_WINDOW]),
                Ms(gCtrPerf.ticks[PERF_ASSET_LOAD]), Ms(gCtrPerf.ticks[PERF_DATA_READ]),
                Ms(gCtrPerf.ticks[PERF_DATA_CRC]), Ms(gCtrPerf.ticks[PERF_DECOMPRESS]),
                Ms(gCtrPerf.ticks[PERF_AUDIO]), Ms(gCtrPerf.ticks[PERF_NDSP_SUBMIT]),
                (unsigned long)gCtrPerf.counters[PERF_ASSETS_LOADED],
                (unsigned long)gCtrPerf.counters[PERF_ASSET_BYTES], Ms(gCtrPerf.maxAssetTicks),
                (unsigned long)gCtrPerf.counters[PERF_CACHE_MISS], (unsigned long)gCtrPerf.counters[PERF_CACHE_REQUEST],
                (unsigned long)gCtrPerf.counters[PERF_QUADS], (unsigned long)gCtrPerf.counters[PERF_REGENERATE],
                stereo, planes, (double)commands, (unsigned long)gCtrPerf.audioQueued,
                (unsigned long)gCtrPerf.audioUnderruns, (unsigned long)gCtrPerf.audioDrops,
                (unsigned long)gCtrPerf.counters[PERF_PAK_PHYS_READS], (unsigned long)gCtrPerf.counters[PERF_PAK_READS]);
        }
        else ++sRolling.quiet;
    }
    if (sRolling.n == WINDOW)
    {
        qsort(sRolling.samples, WINDOW, sizeof(*sRolling.samples), CompareSample);
        CtrLog_Write(CTR_LOG_VIDEO,
            "PERF_SUMMARY f=%lu n=600 fps=%.2f wall=%.3f cpu=%.3f gpuCompleted=%.3f wait=%.3f worst=%.3f p95=%.3f p99=%.3f spikes=%u severe=%u quiet=%u steadyN=%u steadyWall=%.3f loadN=%u loadWall=%.3f assetAvg=%.3f assetLoadAvg=%.3f assetFrameMax=%.3f singleAssetMax=%.3f slowAssetQuiet=%u missPct=%.3f quads=%.1f uploads=%.1f cmd=%.4f audio=%.3f ndsp=%.3f queue=%lu..%lu rateHz=%.3f underruns=%lu drops=%lu clockEstimateMs=%.6f csvDropped=%u",
            (unsigned long)gCtrPerf.frame, WINDOW*sFrequency/(double)sRolling.wall,
            Ms(sRolling.wall)/WINDOW, Ms(sRolling.cpu)/WINDOW, sRolling.gpu/WINDOW,
            Ms(sRolling.wait)/WINDOW, Ms(sRolling.maxWall), sRolling.samples[569]/1000.0, sRolling.samples[593]/1000.0,
            sRolling.spikes, sRolling.severe, sRolling.quiet,
            sRolling.steady, sRolling.steady ? Ms(sRolling.steadyWall)/sRolling.steady : 0.0,
            sRolling.loads, sRolling.loads ? Ms(sRolling.loadWall)/sRolling.loads : 0.0,
            Ms(sRolling.asset)/WINDOW, sRolling.loads ? Ms(sRolling.asset)/sRolling.loads : 0.0, Ms(sRolling.maxAssetFrame), Ms(sRolling.maxAsset), sRolling.assetQuiet,
            sRolling.cacheRequest ? 100.0*sRolling.cacheMiss/sRolling.cacheRequest : 0.0,
            sRolling.quads/(double)WINDOW, sRolling.uploads/(double)WINDOW, sRolling.commands/WINDOW,
            Ms(sRolling.audio)/WINDOW, Ms(sRolling.submit)/WINDOW,
            (unsigned long)sRolling.queuedMin, (unsigned long)sRolling.queuedMax,
            gCtrPerf.audioRateMilliHz/1000.0,
            (unsigned long)(gCtrPerf.audioUnderruns-sRolling.underruns),
            (unsigned long)(gCtrPerf.audioDrops-sRolling.drops), sRolling.ticks*sClockNs/1e6/WINDOW, sCsvDropped);
        uint32_t underruns=gCtrPerf.audioUnderruns, drops=gCtrPerf.audioDrops;
        memset(&sRolling, 0, sizeof(sRolling));
        sRolling.underruns=underruns; sRolling.drops=drops;
    }
}

void CtrPerf_GameEnd(void)
{ if (gCtrPerf.enabled) gCtrPerf.ticks[PERF_GAME] += CtrPerf_PlatformClock()-sStart; }

void CtrPerf_EndFrame(uintptr_t cb, uintptr_t task, unsigned display,
                      unsigned stereo, unsigned planes, float gpuMs, float commands)
{
    if (!gCtrPerf.enabled) return;
    uint64_t end = CtrPerf_PlatformClock(), wall = end-sStart;
    uint64_t cpu = wall > gCtrPerf.ticks[PERF_GPU_WAIT] ? wall-gCtrPerf.ticks[PERF_GPU_WAIT] : 0;
    Rolling(wall, cpu, gpuMs, commands, cb, stereo, planes);
    Record *r = sRecordsUsed ? &sRecords[sRecordsUsed-1] : NULL;
    if (!r || r->cb != cb || r->task != task || r->display != display
        || r->stereo != stereo || r->planes != planes || r->frames == 600)
    {
        if (sRecordsUsed == 32) Flush();
        r = &sRecords[sRecordsUsed++];
        *r = (Record){.cb=cb,.task=task,.display=display,.stereo=stereo,.planes=planes,.first=gCtrPerf.frame};
    }
    ++r->frames; r->last=gCtrPerf.frame;
    r->wall+=wall; r->cpu+=cpu;
    if (cpu>r->maxCpu) { r->maxCpu=cpu; r->peakFrame=gCtrPerf.frame; }
    const double thresholds[] = {16.67,20,25,30};
    for (unsigned i=0;i<4;++i) r->over[i] += Ms(cpu)>thresholds[i];
    for (unsigned t=0;t<PERF_TIME_COUNT;++t) r->ticks[t]+=gCtrPerf.ticks[t];
    for (unsigned c=0;c<PERF_COUNTER_COUNT;++c) r->counters[c]+=gCtrPerf.counters[c];
    r->gpu+=gpuMs; if (gpuMs>r->maxGpu) r->maxGpu=gpuMs;
    r->commands+=commands;
    gCtrPerf.maxAssetTicks = 0;
    memset(gCtrPerf.ticks,0,sizeof(gCtrPerf.ticks));
    memset(gCtrPerf.counters,0,sizeof(gCtrPerf.counters));
    ++gCtrPerf.frame;
    sStart=end;
    /* Async CSV records per 600 game frames, or a full record
     * buffer. Scene changes are records in RAM, never per-draw SD logs. */
    if (gCtrPerf.frame-sLastFlush >= 600 || sRecordsUsed == 32) Flush();
}
