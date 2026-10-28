/* GPU data cache flushes made by kernel call instead of through the GSP service; see below. */
#include <3ds.h>
#include <stdint.h>

#include "3ds_log.h"

/*
 * Data cache flushes for the GPU (Citro3D's C3D_FrameEnd flushes the whole linear heap every frame) go through
 * GSPGPU_FlushDataCache, a request to the GSP service on the system core, which makes the same kernel call for this
 * process: a round trip to the other core per flush, about 0.6 ms for the whole heap on an Old 3DS (measured by
 * ZallaxDev's Emerald 3DS, where this comes from). The kernel call made here does the same clean and invalidate with
 * no service in between; should the kernel refuse it, the service is used from then on. For the whole heap the two
 * are timed against each other over the first FLUSH_TRIAL flushes of each, the faster is kept, and the result logged.
 * Linked with -Wl,--wrap=GSPGPU_FlushDataCache (VIDEO_LDFLAGS in the Makefile) wherever 3ds_video.c is.
 */
Result __real_GSPGPU_FlushDataCache(const void *adr, u32 size);
Result __wrap_GSPGPU_FlushDataCache(const void *adr, u32 size);
static int sFlushDirect = 1;        /* 0: the kernel refused the call */
static int sFlushChoice = -1;       /* whole heap: -1 still timing, 0 service, 1 direct */
static uint64_t sFlushTrial[2];
static unsigned sFlushTrials[2];
#define FLUSH_TRIAL 120u
#define FLUSH_LARGE (1u << 20)

static Result FlushDirect(const void *adr, u32 size)
{
    Result rc = svcFlushProcessDataCache(CUR_PROCESS_HANDLE, (u32)adr, size);

    if (R_FAILED(rc))
    {
        sFlushDirect = 0;
        CtrLog_Write(CTR_LOG_VIDEO, "cache flush: kernel call refused (%08lx), GSP service used", (unsigned long)rc);
        return __real_GSPGPU_FlushDataCache(adr, size);
    }
    return rc;
}

Result __wrap_GSPGPU_FlushDataCache(const void *adr, u32 size)
{
    uint64_t start;
    Result rc;
    int way;

    if (sFlushDirect == 0)
        return __real_GSPGPU_FlushDataCache(adr, size);
    if (size < FLUSH_LARGE)
        return FlushDirect(adr, size);
    way = sFlushChoice >= 0 ? sFlushChoice : (int)((sFlushTrials[0] + sFlushTrials[1]) & 1);
    start = svcGetSystemTick();
    rc = way ? FlushDirect(adr, size) : __real_GSPGPU_FlushDataCache(adr, size);
    if (sFlushChoice < 0 && sFlushDirect != 0)
    {
        sFlushTrial[way] += svcGetSystemTick() - start;
        if (++sFlushTrials[way] >= FLUSH_TRIAL && sFlushTrials[!way] >= FLUSH_TRIAL)
        {
            float service = sFlushTrial[0] * 1000.0f / SYSCLOCK_ARM11 / sFlushTrials[0];
            float direct = sFlushTrial[1] * 1000.0f / SYSCLOCK_ARM11 / sFlushTrials[1];

            sFlushChoice = direct <= service;
            CtrLog_Write(CTR_LOG_VIDEO, "cache flush of the heap: GSP service %.3f ms, kernel call %.3f ms; %s kept",
                         (double)service, (double)direct, sFlushChoice ? "kernel call" : "GSP service");
        }
    }
    return rc;
}
