#ifndef CTR_PERF_H
#define CTR_PERF_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/* Inclusive timings: task/sprite/DMA may be nested inside game or VBlank;
 * cache/decode/draw samples are nested inside BG/OBJ. Do not add them all. */
enum CtrPerfTime {
    PERF_GAME, PERF_CALLBACK, PERF_TASKS, PERF_ANIMATE, PERF_BG, PERF_BG_AFFINE, PERF_OBJ,
    PERF_OBJ_AFFINE, PERF_WINDOW, PERF_PALETTE, PERF_PREPARE, PERF_CACHE_SAMPLE,
    PERF_DECODE_SAMPLE, PERF_DRAW_SAMPLE, PERF_SUBMIT, PERF_BLEND, PERF_DMA, PERF_DECOMPRESS, PERF_SCANLINE,
    PERF_VBLANK, PERF_AUDIO, PERF_RENDER, PERF_GPU_WAIT, PERF_FINALIZE, PERF_LOG,
    PERF_ASSET_LOAD, PERF_DATA_READ, PERF_DATA_CRC, PERF_AUDIO_BRIDGE,
    PERF_NDSP_SUBMIT, PERF_OBJ_WINDOW, PERF_TIME_COUNT
};
enum CtrPerfCounter {
    PERF_CACHE_REQUEST, PERF_CACHE_HIT, PERF_CACHE_MISS, PERF_REGENERATE,
    PERF_PALETTE_INVALIDATE, PERF_VRAM_INVALIDATE, PERF_CACHE_SAMPLE_COUNT,
    PERF_DECODE_SAMPLE_COUNT, PERF_DRAW_SAMPLE_COUNT, PERF_ATLAS_BYTES,
    PERF_VRAM_READ_BYTES, PERF_PALETTE_COLORS, PERF_QUADS, PERF_FLUSH_REQUEST,
    PERF_TARGET_SWITCH, PERF_TEXTURE_SWITCH, PERF_OAM_VISITS, PERF_SPRITES,
    PERF_AFFINE_OBJECTS, PERF_WINDOW_PIXELS, PERF_DMA_BYTES, PERF_VRAM_BYTES,
    PERF_FULL_VRAM_COPY, PERF_PALETTE_BYTES, PERF_OAM_BYTES, PERF_OTHER_BYTES,
    PERF_TICK_READS, PERF_RESOURCE_CREATE, PERF_RESOURCE_RELEASE,
    PERF_ASSETS_LOADED, PERF_ASSET_BYTES, PERF_ASSET_FAILURES, PERF_DATA_READ_BYTES,
    PERF_BG_LAYERS, PERF_FRAME_SPLITS, PERF_PAK_READS, PERF_PAK_PHYS_READS,
    PERF_COUNTER_COUNT
};
typedef struct {
    bool enabled;
    uint32_t frame;
    uint64_t ticks[PERF_TIME_COUNT];
    uint32_t counters[PERF_COUNTER_COUNT];
    uint64_t maxAssetTicks;
    uint32_t audioQueued, audioRateMilliHz, audioUnderruns, audioDrops;
} CtrPerfFrame;
extern CtrPerfFrame gCtrPerf;
uint64_t CtrPerf_PlatformClock(void);
void CtrPerf_Init(uint64_t frequency, uintptr_t reference);
void CtrPerf_SetMemory(const void *vram, const void *palette, const void *oam);
void CtrPerf_Transfer(const void *dest, size_t bytes);
void CtrPerf_GameEnd(void);
void CtrPerf_EndFrame(uintptr_t callback, uintptr_t task, unsigned display,
                      unsigned stereo, unsigned planes, float gpuMs, float commands);

#if defined(PLATFORM_3DS) || (defined(CTR_PERF_ENABLED) && CTR_PERF_ENABLED)
static inline uint64_t CtrPerf_Begin(void)
{
    if (!gCtrPerf.enabled) return 0;
    ++gCtrPerf.counters[PERF_TICK_READS];
    return CtrPerf_PlatformClock();
}
static inline void CtrPerf_End(unsigned kind, uint64_t start)
{
    if (start) gCtrPerf.ticks[kind] += CtrPerf_Begin() - start;
}
#define CTR_PERF_COUNT(kind, amount) do { if (gCtrPerf.enabled) gCtrPerf.counters[kind] += (amount); } while (0)
#define CTR_PERF_TRANSFER(dest, bytes) do { if (gCtrPerf.enabled) CtrPerf_Transfer(dest, bytes); } while (0)
static inline uint64_t CtrPerf_AssetEnd(uint64_t start)
{
    if (!start) return 0;
    uint64_t elapsed = CtrPerf_Begin() - start;
    gCtrPerf.ticks[PERF_ASSET_LOAD] += elapsed;
    if (elapsed > gCtrPerf.maxAssetTicks) gCtrPerf.maxAssetTicks = elapsed;
    return elapsed;
}
bool CtrPerf_SlowAsset(uint64_t ticks);
#else
static inline uint64_t CtrPerf_Begin(void) { return 0; }
static inline void CtrPerf_End(unsigned kind, uint64_t start) { (void)kind; (void)start; }
#define CTR_PERF_COUNT(kind, amount) ((void)0)
#define CTR_PERF_TRANSFER(dest, bytes) ((void)0)
static inline uint64_t CtrPerf_AssetEnd(uint64_t start) { (void)start; return 0; }
static inline bool CtrPerf_SlowAsset(uint64_t ticks) { (void)ticks; return false; }
#endif
#endif
