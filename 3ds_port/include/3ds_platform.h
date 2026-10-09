#ifndef CTR_PLATFORM_H
#define CTR_PLATFORM_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include "3ds_log.h"
#include "3ds_input.h"

/* SDK-free boundary: game translation units never include libctru headers. */
typedef struct {
    float frameMs, workMs, peakWorkMs;
    /* Of the last frame: the game up to its VBlank wait, then audio and the
     * VBlank handler. What remains of workMs is the present. */
    float gameMs, vblankMs;
    uint32_t slowFrames;
} CtrTiming;

typedef struct
{
    void (*vblank)(void);
    void (*videoPresent)(void);
    void (*audioFrame)(void);
    void (*reset)(void);
    void (*shutdown)(void);
} CtrPlatformHooks;

bool CtrPlatform_Init(void);
void CtrPlatform_Shutdown(void);
bool CtrPlatform_BeginFrame(void);
void CtrPlatform_EndFrame(void);
void CtrPlatform_SetHooks(const CtrPlatformHooks *hooks);
void CtrPlatform_RequestReset(void);
void CtrPlatform_RequestExit(void);
void CtrPlatform_Fatal(const char *reason) __attribute__((noreturn));
void CtrPlatform_ShowDataError(const char *title, const char *detail) __attribute__((noreturn));
uint64_t CtrPlatform_Milliseconds(void);
/* Raw ARM11 ticks, for game translation units that cannot include libctru. */
uint64_t CtrPlatform_Ticks(void);
float CtrPlatform_TickMs(uint64_t ticks);
uint64_t CtrPlatform_FrameCount(void);
const CtrTiming *CtrPlatform_GetTiming(void);
void CtrPlatform_Diagnostic(uint32_t gameFrames, uint32_t aPresses, uint32_t checks);
void CtrPlatform_ReportMemory(const char *stage);

bool CtrFs_Init(void);
void CtrFs_Shutdown(void);
FILE *CtrFs_OpenAsset(const char *relativePath);
FILE *CtrFs_OpenData(const char *relativePath, const char *mode);

void CtrGame_Init(void);
void CtrGame_Frame(void);
void CtrGame_VBlank(void);
uint32_t CtrGame_Frames(void);
uint32_t CtrGame_APresses(void);
uint32_t CtrGame_Checks(void);

/* C identifiers cannot start with '3'. Logs retain the plan's 3DS_STUB tag. */
#define CTR_STUB(id, message) CtrLog_Write(CTR_LOG_GAME, "[3DS_STUB] %s: %s", id, message)
#define CTR_UNIMPLEMENTED(id) CtrPlatform_Fatal("[3DS_UNIMPLEMENTED] " id)

#endif
