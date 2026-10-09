/* Small native services needed by the 2D compositor during diagnostics. */
#include <3ds.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "3ds_platform.h"
#include "3ds_perf.h"

uint64_t CtrPerf_PlatformClock(void) { return svcGetSystemTick(); }

uint64_t CtrPlatform_Milliseconds(void)
{
    return svcGetSystemTick() / (SYSCLOCK_ARM11 / 1000);
}

#define CTR_LOG_PATH "sdmc:/3ds/twinembers/port.log"
#define CTR_LOG_QUEUE_BYTES (64u * 1024u)

/*
 * port.log writer. Each line used to create both directories, open, append
 * and close port.log on the calling thread: ~70 ms of SD access per line on
 * the game thread, so every periodic stats line dropped several frames. Lines
 * are now formatted by the caller into a RAM queue, in call order, and a
 * lowest-priority thread appends them while the game waits for VBlank. The
 * file content is unchanged; each batch still ends with fclose. The game
 * thread never waits for the writer: a line that does not fit in a full queue
 * is dropped and counted, and atexit drains the queue and appends one
 * "LOG dropped=N" line, so Fatal and normal exits keep every queued line.
 *
 * Diagnostics are opt-in. With none of sdmc:/3ds/twinembers/debug.txt,
 * log.sync, warp.trace or tm.trace present nothing is written and no thread
 * exists. Any of them enables the async logger; creating log.sync restores
 * the synchronous per-line writes for crash diagnosis before a fault.
 */
enum { LOG_UNINIT, LOG_OFF, LOG_SYNC, LOG_ASYNC };
static int sLogMode = LOG_UNINIT;
static bool sMoveTrace;
static LightLock sLogLock;
static LightEvent sLogWake;
static Thread sLogThread;
static volatile bool sLogStop;
static char sLogQueue[CTR_LOG_QUEUE_BYTES];
static size_t sLogQueued;
static unsigned sLogDropped;
static char sLogBatch[CTR_LOG_QUEUE_BYTES];
static CtrLogDeferredFormatter sDeferredFormatter;
static bool sWarpTrace;

static void LogAppend(const char *text, size_t length)
{
    FILE *file = fopen(CTR_LOG_PATH, "a");
    if (file == NULL) return;
    fwrite(text, 1, length, file);
    fclose(file);
}

static void LogWriteBatch(void)
{
    LightLock_Lock(&sLogLock);
    size_t length = sLogQueued;
    memcpy(sLogBatch, sLogQueue, length);
    sLogQueued = 0;
    LightLock_Unlock(&sLogLock);
    if (length) LogAppend(sLogBatch, length);
}

static void LogWriteDeferred(void)
{
    LightLock_Lock(&sLogLock);
    CtrLogDeferredFormatter formatter = sDeferredFormatter;
    LightLock_Unlock(&sLogLock);
    if (!formatter) return;
    size_t length;
    while ((length = formatter(sLogBatch, sizeof(sLogBatch))) != 0)
        LogAppend(sLogBatch, length);
}

static void LogThreadMain(void *arg)
{
    (void)arg;
    while (!sLogStop)
    {
        LightEvent_Wait(&sLogWake);
        LogWriteBatch();
        LogWriteDeferred();
    }
}

static void LogShutdown(void)
{
    sLogStop = true;
    LightEvent_Signal(&sLogWake);
    threadJoin(sLogThread, U64_MAX);
    threadFree(sLogThread);
    LogWriteBatch();
    LogWriteDeferred();
    if (sLogDropped)
    {
        char summary[48];
        int length = snprintf(summary, sizeof(summary), "LOG dropped=%u\n", sLogDropped);
        if (length > 0) LogAppend(summary, (size_t)length);
    }
    /* Lines from later exit handlers go straight to SD. */
    sLogMode = LOG_SYNC;
}

static bool FileExists(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL) return false;
    fclose(file);
    return true;
}

static void LogInit(void)
{
    sLogMode = LOG_OFF;
    sWarpTrace = FileExists("sdmc:/3ds/twinembers/warp.trace");
    sMoveTrace = FileExists("sdmc:/3ds/twinembers/tm.trace");
    bool sync = FileExists("sdmc:/3ds/twinembers/log.sync");
    if (!sWarpTrace && !sMoveTrace && !sync && !FileExists("sdmc:/3ds/twinembers/debug.txt"))
        return;
    mkdir("sdmc:/3ds", 0777);
    mkdir("sdmc:/3ds/twinembers", 0777);
    sLogMode = LOG_SYNC;
    if (sync) return;
    LightLock_Init(&sLogLock);
    LightEvent_Init(&sLogWake, RESET_ONESHOT);
    sLogThread = threadCreate(LogThreadMain, NULL, 16 * 1024, 0x3F, -2, false);
    if (sLogThread == NULL) return;
    atexit(LogShutdown);
    sLogMode = LOG_ASYNC;
}

bool CtrLog_SetDeferredFormatter(CtrLogDeferredFormatter formatter)
{
    if (sLogMode == LOG_UNINIT) LogInit();
    if (sLogMode != LOG_ASYNC || !formatter) return false;
    LightLock_Lock(&sLogLock);
    bool available = sDeferredFormatter == NULL;
    if (available) sDeferredFormatter = formatter;
    LightLock_Unlock(&sLogLock);
    return available;
}

void CtrLog_DeferredLock(void) { LightLock_Lock(&sLogLock); }
void CtrLog_DeferredUnlock(void) { LightLock_Unlock(&sLogLock); }
void CtrLog_WakeDeferred(void) { LightEvent_Signal(&sLogWake); }

void CtrLog_MoveTrace(const char *stage, unsigned step, uintptr_t cb2, uintptr_t vblank, uintptr_t hblank)
{
    static unsigned emitted, lastStep;
    static const char *lastStage;
    if (sLogMode == LOG_UNINIT) LogInit();
    if (!sMoveTrace || emitted >= 160) return;
    if (lastStage && !strcmp(stage, lastStage) && step == lastStep) return;
    lastStage = stage;
    lastStep = step;
    ++emitted;
    CtrLog_Write(CTR_LOG_GAME, "TM_TRACE stage=%s step=%u cb2=%08lx vblank=%08lx hblank=%08lx",
                 stage, step, (unsigned long)cb2, (unsigned long)vblank, (unsigned long)hblank);
}

void CtrLog_WarpTrace(const char *stage, unsigned step)
{
    static unsigned emitted;
    if (sLogMode == LOG_UNINIT) LogInit();
    /* A stuck map step must not grow an unlimited diagnostic log. */
    if (sWarpTrace && emitted < 2048)
    {
        ++emitted;
        CtrLog_Write(CTR_LOG_GAME, "WARP_TRACE stage=%s step=%u", stage, step);
    }
}

void CtrLog_Write(CtrLogCategory category, const char *format, ...)
{
    (void)category;
    if (sLogMode == LOG_UNINIT) LogInit();
    if (sLogMode == LOG_OFF) return;
    char stack[512];
    char *line = stack;
    va_list args;
    va_start(args, format);
    int length = vsnprintf(stack, sizeof(stack) - 1, format, args);
    va_end(args);
    if (length < 0) return;
    if ((size_t)length >= sizeof(stack) - 1)
    {
        line = malloc((size_t)length + 2);
        if (line == NULL) return;
        va_start(args, format);
        vsnprintf(line, (size_t)length + 1, format, args);
        va_end(args);
    }
    line[length++] = '\n';
    if (sLogMode != LOG_ASYNC)
    {
        LogAppend(line, (size_t)length);
    }
    else
    {
        LightLock_Lock(&sLogLock);
        bool fits = (size_t)length <= CTR_LOG_QUEUE_BYTES - sLogQueued;
        if (fits)
        {
            memcpy(sLogQueue + sLogQueued, line, (size_t)length);
            sLogQueued += (size_t)length;
        }
        else
            ++sLogDropped;
        LightLock_Unlock(&sLogLock);
        LightEvent_Signal(&sLogWake);
    }
    if (line != stack) free(line);
}

void CtrPlatform_Fatal(const char *reason)
{
    CtrLog_Write(CTR_LOG_ERROR, "fatal: %s", reason);
    fprintf(stderr, "FireRed 3DS: %s\n", reason);
    exit(1);
}
