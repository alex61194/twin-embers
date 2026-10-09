/* ARM11 entry point for the real FireRed game image (data pack).
 *
 * Unlike the probe main, this boots the pack-backed executable: the NOBITS game
 * tables are filled from the pack (src/3ds_rex.c) before AgbMain() runs. Stage
 * logging goes once to sdmc:/3ds/twinembers/port.log (never per frame).
 */
#include <3ds.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "3ds_audio.h"
#include "3ds_assets.h"
#include "3ds_bottom.h"
#include "3ds_data.h"
#include "3ds_dma.h"
#include "3ds_flash.h"
#include "3ds_input.h"
#include "3ds_log.h"
#include "3ds_platform.h"
#include "3ds_rex.h"
#include "3ds_video.h"
#include "gba_shadow.h"
#include "3ds_perf.h"
#include "git_version.h"

/* BUGFIX corrections to tables that now arrive unmodified from the pack. */
extern void CtrPokemon_ApplyBugfixes(void);

extern void AgbMain(void);
extern void CtrGameBridge_StartWatchdog(void);
extern void CtrGameBridge_StopWatchdog(void);

/*
 * The game leaves through exit() from its VBlank wait (HOME menu close,
 * fatal errors), never through the end of main. libctru then unmaps the
 * heap, where the watchdog's and the GSP event thread's stacks live; a GSP
 * event arriving after that faults on the missing stack (Luma data abort in
 * gspEventThreadMain). Every exit therefore stops those threads first.
 */
static void ShutdownPlatform(void)
{
    static bool done;
    if (done) return;
    done = true;
    CtrLog_Write(CTR_LOG_BOOT, "exit: watchdog");
    CtrGameBridge_StopWatchdog();
    CtrLog_Write(CTR_LOG_BOOT, "exit: video");
    CtrVideo_Shutdown();
    CtrLog_Write(CTR_LOG_BOOT, "exit: gfx");
    gfxExit();
    CtrLog_Write(CTR_LOG_BOOT, "exit: platform done");
}

/* Each exit step is logged before and after, so a hang on the way out shows
 * as the last line of port.log (use log.sync to keep it). */
#define TRACED_EXIT(name, call) \
    static void name(void) \
    { \
        CtrLog_Write(CTR_LOG_BOOT, "exit: " #name); \
        call; \
        CtrLog_Write(CTR_LOG_BOOT, "exit: " #name " done"); \
    }
TRACED_EXIT(ExitData, CtrData_Shutdown())
TRACED_EXIT(ExitAssets, CtrAssets_Shutdown())
TRACED_EXIT(ExitAudio, CtrAudio_Shutdown())

int main(void)
{
    /* New 3DS: the application core at 804 MHz with its L2 cache. Ignored by
     * an Old 3DS. Frame pacing stays on the VBlank, so only headroom grows. */
    osSetSpeedupEnable(true);
    CtrLog_Write(CTR_LOG_BOOT, "native main reached");
    CtrLog_Write(CTR_LOG_BOOT, "FireRed3DS git=%s dirty=%u AgbMain=%p",
                 CTR_BUILD_GIT, CTR_BUILD_DIRTY, (void *)AgbMain);
    {
        bool isNew3ds = false;
        APT_CheckNew3DS(&isNew3ds);
        CtrLog_Write(CTR_LOG_BOOT, "console: %s", isNew3ds ? "New 3DS, 804 MHz + L2 requested" : "Old 3DS");
    }
    GbaShadow_Reset();
    CtrDma_Reset();
    CtrInput_Clear();
    CtrLog_Write(CTR_LOG_BOOT, "shadow memory initialized");
    gfxInitDefault();
    atexit(ShutdownPlatform);
    consoleInit(GFX_BOTTOM, NULL);
    mkdir("sdmc:/3ds", 0777);
    mkdir("sdmc:/3ds/twinembers", 0777);
    if (!CtrFlash_InitMigrating("sdmc:/3ds/twinembers/twinembers.sav",
                                  "sdmc:/3ds/pokefirered/twinembers.sav",
                                  "sdmc:/3ds/pokefirered/firered.sav"))
        CtrPlatform_Fatal("Twin Embers save could not be opened or migrated. Original saves preserved; check SD space and permissions.");
    CtrLog_Write(CTR_LOG_FS, "flash initialized");
    if (!CtrVideo_Init())
        CtrPlatform_Fatal("FireRed compositor init failed");
    CtrVideo_Bind((CtrVideoMemory){
        .vram = gGbaShadow.vram,
        .palette = (const uint16_t *)gGbaShadow.pltt,
        .oam = (const uint16_t *)gGbaShadow.oam,
        .regs = (const uint16_t *)gGbaShadow.regs,
    });
    CtrLog_Write(CTR_LOG_VIDEO, "video initialized");
    /* Bottom UI takes the bottom screen after boot logs; console stays as
     * fallback when bottom is disabled or fails. First FIELD redraw overwrites
     * the console framebuffer. */
    CtrBottom_Init();
    romfsInit();
    if (!CtrData_Init())
        CtrPlatform_ShowDataError(CtrData_ErrorTitle(), CtrData_ErrorDetail());
    atexit(ExitData);
    if (!CtrRex_Init((uintptr_t)AgbMain))
        CtrPlatform_ShowDataError("The game data could not be loaded.",
                                 "The data pack does not match this engine.\nRun the builder again.");
    CtrPokemon_ApplyBugfixes();
    if (!CtrAssets_Init((uintptr_t)AgbMain))
        CtrPlatform_ShowDataError("FireRed graphics could not be loaded.",
                                 "The fonts/UI data does not match this engine. Rebuild the data pack.");
    atexit(ExitAssets);
    CtrAssets_StartWarmup();
    /* Audio is optional: without DSP firmware the game runs silently. The
     * VBlank bridge leaves through exit(0), so shutdown is also atexit. */
    CtrAudio_Init();
    atexit(ExitAudio);
    CtrGameBridge_StartWatchdog();
    CtrPerf_SetMemory(gGbaShadow.vram, gGbaShadow.pltt, gGbaShadow.oam);
    CtrPerf_Init(SYSCLOCK_ARM11, (uintptr_t)AgbMain);
    CtrLog_Write(CTR_LOG_GAME, "entering AgbMain");
    AgbMain();
    CtrLog_Write(CTR_LOG_GAME, "AgbMain returned");
    CtrAudio_Shutdown();
    CtrFlash_Close();
    ShutdownPlatform();
    return 0;
}
