/* ARM11 entry point for the real FireRed game image (data pack).
 *
 * Unlike the probe main, this boots the pack-backed executable: the NOBITS game
 * tables are filled from the pack (src/3ds_rex.c) before AgbMain() runs. Stage
 * logging goes once to sdmc:/3ds/twinembers/port.log (never per frame).
 */
#include <3ds.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "3ds_audio.h"
#include "3ds_assets.h"
#include "3ds_bottom.h"
#include "3ds_data.h"
#include "3ds_dma.h"
#include "3ds_flash.h"
#include "3ds_forwarder.h"
#include "3ds_input.h"
#include "3ds_poketouch_options.h"
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
/* Leaving through the HOME Menu (aptMainLoop) is exit(0) from the VBlank bridge:
 * whatever is still pending reaches the card before the process ends. */
TRACED_EXIT(ExitFlash, CtrFlash_Close())

/*
 * Soft reset: A+B+START+SELECT, and the game's own DoSoftReset (after the save
 * is cleared, a failed save and the like). The GBA clears its RAM and starts
 * the game again; quitting and starting the 3DSX again took seconds. Here the
 * game's variables (the game_ram section, tools/game_ram_ld.py) go back to how
 * they were as AgbMain first started, and AgbMain is entered again: its own
 * start-up clears the emulated hardware (RegisterRamReset). Kept as they are:
 * the save (the port's flash image), the data the port loaded from the pack,
 * and the port itself.
 */
extern char __ctr_game_ram[], __ctr_game_ram_end[];
extern void (*gCtrSoftReset)(void);
static char *sGameAtStart;
static jmp_buf sAgbMainEntry;
static unsigned sResets;

static size_t GameRamBytes(void) { return (size_t)(__ctr_game_ram_end - __ctr_game_ram); }

static void BindVideo(void)
{
    CtrVideo_Bind((CtrVideoMemory){
        .vram = gGbaShadow.vram,
        .palette = (const uint16_t *)gGbaShadow.pltt,
        .oam = (const uint16_t *)gGbaShadow.oam,
        .regs = (const uint16_t *)gGbaShadow.regs,
    });
}

static void SoftResetGame(void)
{
    /* Whatever the game wrote reaches the card first, as on a quit. */
    if (!CtrFlash_Flush())
        CtrLog_Write(CTR_LOG_FS, "soft reset: the save could not be flushed");
    /* The sound worker mixes from the game's variables: not while they change. */
    CtrAudio_LockSound();
    memcpy(__ctr_game_ram, sGameAtStart, GameRamBytes());
    CtrDma_Reset();
    CtrAudio_ResetPsg();
    CtrAudio_UnlockSound();
    CtrAudio_ResetCatchUp();
    /* The chord is still held: the game reads the keys the next frame scans,
     * so it resets again each frame until they are let go, like the GBA. */
    GbaShadow_SetHeldKeys(0);
    BindVideo();
    ++sResets;
    longjmp(sAgbMainEntry, 1);
}

static void KeepGameAtStart(void)
{
    sGameAtStart = malloc(GameRamBytes());
    if (sGameAtStart == NULL)
    {
        CtrLog_Write(CTR_LOG_GAME, "soft reset unavailable (no memory): it quits instead");
        return;
    }
    memcpy(sGameAtStart, __ctr_game_ram, GameRamBytes());
    gCtrSoftReset = SoftResetGame;
    CtrLog_Write(CTR_LOG_GAME, "soft reset ready: game RAM %lu KiB",
                 (unsigned long)(GameRamBytes() >> 10));
}

/* Started by the HOME Menu forwarder (3ds_port/forwarder, 3ds_forwarder.h): it
 * made its own title Luma3DS's 3DSX title and passed the previous one. Put that
 * back first, so the forwarder runs its own code next time and the usual
 * Homebrew Launcher entry keeps working; Luma applies the selected title when
 * this process exits. Without the forwarder's argument nothing is written. */
extern int __system_argc;
extern char **__system_argv;

static uint64_t RestoreLumaHbldrTitle(void)
{
    uint64_t tid = CtrForwarder_RestoreTid(__system_argc, __system_argv);
    s64 luma;

    if (tid == 0 || R_FAILED(svcGetSystemInfo(&luma, 0x10000, 0)))
        return 0;
    *(volatile uint64_t *)(OS_SHAREDCFG_VADDR + CTR_FORWARDER_LUMA_SELECTED_TID_OFFSET) = tid;
    return tid;
}

int main(void)
{
    uint64_t restoredTid = RestoreLumaHbldrTitle();

    /* New 3DS: the application core at 804 MHz with its L2 cache. Ignored by
     * an Old 3DS. Frame pacing stays on the VBlank, so only headroom grows.
     * A New 3DS acting as an Old 3DS asks for 268 MHz without L2 instead. */
    osSetSpeedupEnable(CtrPlatform_IsNew3DS());
    CtrLog_Write(CTR_LOG_BOOT, "native main reached");
    CtrLog_Write(CTR_LOG_BOOT, "FireRed3DS %s git=%s dirty=%u AgbMain=%p",
                 CTR_BUILD_LABEL, CTR_BUILD_GIT, CTR_BUILD_DIRTY, (void *)AgbMain);
    {
        bool isNew3ds = CtrPlatform_IsNew3DS();
        CtrLog_Write(CTR_LOG_BOOT, "console: %s", isNew3ds ? "New 3DS, 804 MHz + L2 requested"
                     : CtrPlatform_Old3DSForced() ? "New 3DS acting as Old 3DS (forced: 268 MHz, no L2, no core 2)"
                     : "Old 3DS");
        if (!isNew3ds)
            CtrOptions_EmbersDefaultLow();      /* the sparks cost frames there: LOW until the player says otherwise */
        if (restoredTid != 0)
            CtrLog_Write(CTR_LOG_BOOT, "started by the HOME Menu forwarder; Luma 3DSX title %016llX restored",
                         (unsigned long long)restoredTid);
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
    atexit(ExitFlash);
    /* The save as it was at this start, kept beside it for a card or a build that fails later. */
    CtrFlash_Backup("sdmc:/3ds/twinembers/twinembers.sav.bak");
    if (!CtrVideo_Init())
        CtrPlatform_Fatal("FireRed compositor init failed");
    BindVideo();
    CtrVideo_SetBuildLabel(CTR_BUILD_LABEL);
    CtrLog_Write(CTR_LOG_VIDEO, "video initialized");
    /* Bottom UI takes the bottom screen after boot logs; console stays as
     * fallback when bottom is disabled or fails. First FIELD redraw overwrites
     * the console framebuffer. */
    CtrBottom_Init();
    {
        Result romfs = romfsInit();

        CtrData_SetRomfsResult(R_FAILED(romfs) ? (unsigned long)romfs : 0);
    }
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
    KeepGameAtStart();
    if (setjmp(sAgbMainEntry) == 0)
        CtrLog_Write(CTR_LOG_GAME, "entering AgbMain");
    else
        CtrLog_Write(CTR_LOG_GAME, "soft reset %u: entering AgbMain again", sResets);
    AgbMain();
    CtrLog_Write(CTR_LOG_GAME, "AgbMain returned");
    CtrAudio_Shutdown();
    CtrFlash_Close();
    ShutdownPlatform();
    return 0;
}
