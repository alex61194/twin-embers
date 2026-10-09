#ifndef CTR_LOG_H
#define CTR_LOG_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* One bounded diagnostic producer may supply immutable snapshots. Formatting
 * and SD writes run on the existing logger thread, never the game thread.
 * The formatter must not call CtrLog_Write (it already owns the output batch).
 * Registration fails in synchronous mode; producers must never wait for space. */
typedef size_t (*CtrLogDeferredFormatter)(char *buffer, size_t capacity);
bool CtrLog_SetDeferredFormatter(CtrLogDeferredFormatter formatter);
void CtrLog_DeferredLock(void);
void CtrLog_DeferredUnlock(void);
void CtrLog_WakeDeferred(void);
/* Opt-in warp.trace markers. Pair with log.sync for freeze diagnosis only. */
void CtrLog_WarpTrace(const char *stage, unsigned step);
/* Development-only tm.trace lifecycle diagnostics, bounded per launch. */
void CtrLog_MoveTrace(const char *stage, unsigned step, uintptr_t cb2, uintptr_t vblank, uintptr_t hblank);

typedef enum
{
    CTR_LOG_BOOT, CTR_LOG_VIDEO, CTR_LOG_INPUT, CTR_LOG_FS,
    CTR_LOG_AUDIO, CTR_LOG_GAME, CTR_LOG_ERROR
} CtrLogCategory;

void CtrLog_Init(void);
void CtrLog_Close(void);
void CtrLog_SetOverlay(bool enabled);
void CtrLog_DrawOverlay(const char *text);
void CtrLog_ShowFatal(const char *reason);
void CtrLog_Write(CtrLogCategory category, const char *format, ...)
    __attribute__((format(printf, 2, 3)));

#endif
