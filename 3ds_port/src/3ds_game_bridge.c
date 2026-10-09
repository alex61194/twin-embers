/* FireRed keeps ownership of its main loop. This is its VBlank boundary on
 * ARM11, adapted from ZallaxDev's DualScreen game bridge (MIT). */
#include <3ds.h>
#include <stdlib.h>
#include <string.h>

#include "global.h"
#include "main.h"
#include "intro.h"
#include "m4a.h"
#include "scanline_effect.h"
#include "sprite.h"
#include "dma3.h"
#include "palette.h"
#include "task.h"
#include "3ds_audio.h"
#include "3ds_audio_cadence.h"
#include "3ds_input.h"
#include "3ds_bottom.h"
#include "3ds_dma.h"
#include "3ds_flash.h"
#include "3ds_log.h"
#include "3ds_video.h"
#include "gba_shadow.h"
#include "3ds_perf.h"
#include "3ds_fast_forward.h"

extern IntrFunc gIntrTable[];

/* Callback ownership selects authored GBA geometry without scene addresses.
 * Retain the previous view while setup temporarily has no VBlank callback. */
#define CTR_STAGE_CALLBACKS 64
typedef struct
{
    IntrCallback callbacks[CTR_STAGE_CALLBACKS];
    unsigned count;
    bool on;
} StageSet;
static StageSet sStage, sStageUi, sCentred, sBattle, sTransition, sNativeUi, sIntro;

extern const u16 *CtrTrainerCard_FlipScroll(void);
extern bool32 CtrField_BarnDoorWipeActive(void);
extern bool32 IsMapNamePopupTaskActive(void);

static void RememberCallback(StageSet *set, IntrCallback callback)
{
    if (!callback) return;
    for (unsigned i = 0; i < set->count; ++i)
        if (set->callbacks[i] == callback) return;
    if (set->count < CTR_STAGE_CALLBACKS)
        set->callbacks[set->count++] = callback;
    else
        CtrLog_Write(CTR_LOG_ERROR, "VIDEO stage callback registry full");
}

void CtrIntro_SetVBlankCallback(IntrCallback callback)
{
    RememberCallback(&sIntro, callback);
    SetVBlankCallback(callback);
}

void CtrStage_SetVBlankCallback(IntrCallback callback)
{
    RememberCallback(&sStage, callback);
    SetVBlankCallback(callback);
}

/* Only oak_speech.o registers here: CONTROLS, Pikachu and the speech.
 * Keep its 1:1 UI/portraits over the native colour surround, separate from
 * credits and from the intro/title callback class. */
void CtrStageUi_SetVBlankCallback(IntrCallback callback)
{
    RememberCallback(&sStage, callback);
    RememberCallback(&sStageUi, callback);
    SetVBlankCallback(callback);
}

void CtrCentred_SetVBlankCallback(IntrCallback callback)
{
    RememberCallback(&sCentred, callback);
    SetVBlankCallback(callback);
}
void CtrBattle_SetVBlankCallback(IntrCallback callback)
{
    RememberCallback(&sBattle, callback);
    SetVBlankCallback(callback);
}
void CtrTransition_SetVBlankCallback(IntrCallback callback)
{
    RememberCallback(&sTransition, callback);
    SetVBlankCallback(callback);
}
void CtrNativeUi_SetVBlankCallback(IntrCallback callback)
{
    RememberCallback(&sNativeUi, callback);
    SetVBlankCallback(callback);
}

static void UpdateSet(StageSet *set, IntrCallback callback)
{
    set->on = false;
    for (unsigned i = 0; i < set->count; ++i)
        if (set->callbacks[i] == callback) set->on = true;
}

extern int CtrField_CaveTransitionActive(void);
extern int CtrFieldEffect_ShowMonActive(void);
extern int CtrField_FlashMaskActive(void);
extern int CtrBottom_IsFieldCallback(void);

static void UpdateStage(void)
{
    IntrCallback callback = gMain.vblankCallback;
    if (callback)
    {
        UpdateSet(&sIntro, callback);
        UpdateSet(&sStage, callback);
        UpdateSet(&sStageUi, callback);
        UpdateSet(&sCentred, callback);
        UpdateSet(&sBattle, callback);
        UpdateSet(&sTransition, callback);
        UpdateSet(&sNativeUi, callback);
    }
    CtrVideo_SetIntro(sIntro.on);
    CtrVideo_SetStage(sStage.on);
    CtrVideo_SetStageUi(sStageUi.on);
    CtrVideo_SetCentred(sCentred.on);
    CtrVideo_SetBattle(sBattle.on);
    /* The battle transition owns the effect, not the viewport: its masks are authored for 240x160, but the field under
     * them stays the native 400x240 canvas (CtrVideo_SetNativeViewport, the strict centred viewport, is no longer used). */
    CtrVideo_SetBattleTransition(sTransition.on);
    CtrVideo_SetNativeUi(sNativeUi.on);
    CtrVideo_SetFieldBarnDoorWipe(CtrField_BarnDoorWipeActive() != 0);
    CtrVideo_SetNativeViewport(CtrField_CaveTransitionActive() != 0);
    CtrVideo_SetFieldMoveShowMon(CtrFieldEffect_ShowMonActive() != 0);
}


static void ProbeAudio(unsigned frame)
{
    const struct SoundInfo *sound = &gSoundInfo;
    unsigned offset = 0, peakA = 0, peakB = 0, cgb = 0;
    unsigned count = sound->pcmSamplesPerVBlank;

    if (sound->ident != ID_NUMBER || !count || count > PCM_DMA_BUF_SIZE)
    {
        CtrLog_Write(CTR_LOG_GAME, "AUDIO frame=%u unavailable ident=%08lx samples=%u",
                     frame, (unsigned long)sound->ident, count);
        return;
    }
    offset = CtrAudio_PcmSliceOffset(sound->pcmDmaCounter,
                                       sound->pcmDmaPeriod, count);
    if (offset + count > PCM_DMA_BUF_SIZE)
    {
        CtrLog_Write(CTR_LOG_GAME, "AUDIO frame=%u invalid slice=%u+%u counter=%u",
                     frame, offset, count, sound->pcmDmaCounter);
        return;
    }
    for (unsigned i = 0; i < count; ++i)
    {
        /* Plane 0 feeds FIFO A (right), plane 1 FIFO B (left): SoundInit. */
        int a = sound->pcmBuffer[offset + i];
        int b = sound->pcmBuffer[PCM_DMA_BUF_SIZE + offset + i];
        unsigned absA = a < 0 ? -a : a;
        unsigned absB = b < 0 ? -b : b;
        if (absA > peakA) peakA = absA;
        if (absB > peakB) peakB = absB;
    }
    if (sound->cgbChans != NULL)
        for (unsigned i = 0; i < 4; ++i)
            if (sound->cgbChans[i].statusFlags & SOUND_CHANNEL_SF_ON)
                ++cgb;
    CtrLog_Write(CTR_LOG_GAME,
        "AUDIO frame=%u rate=%ld samples=%u counter=%u off=%u pcmPeakAB=%u,%u cgb=%u nr51=%02x nr52=%02x",
        frame, (long)sound->pcmFreq, count, sound->pcmDmaCounter, offset,
        peakA, peakB, cgb, gGbaShadow.regs[0x81], gGbaShadow.regs[0x84]);
    /* Cumulative CGB events decoded from CgbSound's register writes. */
    const CtrCgbStats *events = CtrAudio_CgbStats();
    CtrLog_Write(CTR_LOG_GAME,
        "PSG frame=%u peak=%u starts=%lu,%lu,%lu,%lu stops=%lu,%lu,%lu,%lu wave=%lu sweep=%lu",
        frame, CtrAudio_PsgPeak(),
        (unsigned long)events->starts[0], (unsigned long)events->starts[1],
        (unsigned long)events->starts[2], (unsigned long)events->starts[3],
        (unsigned long)events->stops[0], (unsigned long)events->stops[1],
        (unsigned long)events->stops[2], (unsigned long)events->stops[3],
        (unsigned long)events->waveUpdates, (unsigned long)events->sweepUpdates);
}

/* One compact snapshot per scene, low frequency: 45 frames after
 * gMain.callback2 changes (fades and sprite setup done), at most 32 per
 * process. It compares what FireRed built (gSprites, gMain.oamBuffer) with
 * what the renderer consumes (the OAM shadow) and what it drew last frame,
 * so a missing sprite can be placed before or after the OAM copy. */
static void ProbeScene(unsigned frame)
{
    static MainCallback sLastCallback2;
    static unsigned sArmed, sLogged;
    unsigned inUse = 0, visible = 0, mismatch = 0, shown = 0, listed = 0;
    int firstMismatch = -1;

    if (gMain.callback2 != sLastCallback2)
    {
        sLastCallback2 = gMain.callback2;
        sArmed = 45;
        return;
    }
    if (sArmed == 0 || --sArmed != 0 || sLogged >= 32)
        return;
    ++sLogged;

    for (unsigned i = 0; i < MAX_SPRITES; ++i)
    {
        inUse += gSprites[i].inUse;
        visible += gSprites[i].inUse && !gSprites[i].invisible;
    }
    for (unsigned i = 0; i < 128; ++i)
    {
        const uint16_t *oam = (const uint16_t *)(gGbaShadow.oam + i * 8);
        unsigned a0 = oam[0], a1 = oam[1];
        int x = a1 & 511, y = a0 & 255;

        if (memcmp(&gMain.oamBuffer[i], oam, 8) != 0)
        {
            if (firstMismatch < 0)
                firstMismatch = (int)i;
            ++mismatch;
        }
        if (!(a0 & 0x100) && (a0 & 0x200))
            continue;
        if (x >= 240) x -= 512;
        if (y >= 160) y -= 256;
        if (x <= -128 || y <= -128)
            continue;
        ++shown;
    }
    const CtrVideoStats *video = CtrVideo_GetStats();
    CtrLog_Write(CTR_LOG_VIDEO,
        "SCENE frame=%u cb1=%p cb2=%p vblank=%p state=%u sprites=%u/%u oamShown=%u "
        "oamMismatch=%u first=%d oamLoadOff=%u drawnSprites=%lu quads=%lu errors=%lu",
        frame, (void *)gMain.callback1, (void *)gMain.callback2,
        (void *)gMain.vblankCallback, gMain.state, visible, inUse, shown,
        mismatch, firstMismatch, gMain.oamLoadDisabled,
        (unsigned long)video->sprites, (unsigned long)video->tiles,
        (unsigned long)video->errors);
    for (unsigned i = 0; i < 128 && listed < 12; ++i)
    {
        const uint16_t *oam = (const uint16_t *)(gGbaShadow.oam + i * 8);
        if (!(oam[0] & 0x100) && (oam[0] & 0x200))
            continue;
        if ((oam[0] & 255) == 160 && (oam[1] & 511) == 304)
            continue; /* gDummyOamData */
        CtrLog_Write(CTR_LOG_VIDEO, "SCENE oam[%u]=%04x %04x %04x", i, oam[0], oam[1], oam[2]);
        ++listed;
    }
    CtrVideo_DumpState();
}

/* Game liveness. sHeartbeat advances once per VBlankIntrWait; the FIELD
 * line (every 300 frames) shows whether the game keeps presenting frames and
 * where it is; the watchdog thread reports a main loop that stops reaching
 * VBlankIntrWait at all, which no per-frame log could show. */
static volatile uint32_t sHeartbeat;

static void LogField(const char *tag, uint32_t beat)
{
    const CtrVideoStats *video = CtrVideo_GetStats();
    const uint16_t *regs = (const uint16_t *)gGbaShadow.regs;
    unsigned mapGroup = 0, mapNum = 0;
    int x = 0, y = 0;

    if (gSaveBlock1Ptr != NULL)
    {
        mapGroup = (u8)gSaveBlock1Ptr->location.mapGroup;
        mapNum = (u8)gSaveBlock1Ptr->location.mapNum;
        x = gSaveBlock1Ptr->pos.x;
        y = gSaveBlock1Ptr->pos.y;
    }
    CtrLog_Write(CTR_LOG_GAME,
        "%s beat=%lu cb1=%p cb2=%p vblank=%p state=%u fade=%u dma3Pending=%u map=%u.%u pos=%d,%d",
        tag, (unsigned long)beat, (void *)gMain.callback1, (void *)gMain.callback2,
        (void *)gMain.vblankCallback, gMain.state, gPaletteFade.active,
        WaitDma3Request(-1) == -1, mapGroup, mapNum, x, y);
    CtrLog_Write(CTR_LOG_GAME,
        "%s DISPCNT=%04x BGCNT=%04x,%04x,%04x,%04x OFS=%u,%u %u,%u %u,%u %u,%u "
        "quads=%lu sprites=%lu errors=%lu fps=%.1f",
        tag, regs[0], regs[4], regs[5], regs[6], regs[7],
        regs[8], regs[9], regs[10], regs[11], regs[12], regs[13], regs[14], regs[15],
        (unsigned long)video->tiles, (unsigned long)video->sprites,
        (unsigned long)video->errors, video->fps);
}

static void LogTasks(void)
{
    for (unsigned i = 0; i < NUM_TASKS; ++i)
        if (gTasks[i].isActive)
            CtrLog_Write(CTR_LOG_GAME, "STALL task[%u]=%p", i, (void *)gTasks[i].func);
}

static Thread sWatchdog;
static LightEvent sWatchdogStop;

/* HOME Menu, sleep and the power menu suspend the game inside aptMainLoop.
 * The hook may run off the game thread, so it only raises flags: the
 * watchdog stays quiet while suspended, and the game thread resyncs input
 * and the audio clock when it gets back (VBlankIntrWait). */
static CtrFastForward sFastForward;
static unsigned sFlashFailures;   /* consecutive frames whose save flush failed */

static aptHookCookie sAptHook;
static volatile bool sSuspended, sResumed;
static bool sAptHooked;

static void Lifecycle(APT_HookType type, void *param)
{
    (void)param;
    if (type == APTHOOK_ONSUSPEND || type == APTHOOK_ONSLEEP)
        sSuspended = true;
    else if (type == APTHOOK_ONRESTORE || type == APTHOOK_ONWAKEUP)
    {
        sSuspended = false;
        sResumed = true;
    }
}

static void ResumeAfterSuspend(void)
{
    if (!sResumed)
        return;
    sResumed = false;
    CtrInput_ResyncLive();
    CtrAudio_ResetCatchUp();
    CtrLog_Write(CTR_LOG_BOOT, "resume: input resync, audio catch-up rebased");
}

static void WatchdogMain(void *arg)
{
    uint32_t last = sHeartbeat;
    unsigned still = 0;

    (void)arg;
    /* Sleeps 250 ms per check; the stop event ends it at once on exit. */
    while (LightEvent_WaitTimeout(&sWatchdogStop, 250000000LL))
    {
        uint32_t beat = sHeartbeat;
        if (beat != last || sSuspended)
        {
            last = beat;
            still = 0;
            continue;
        }
        ++still;
        /* Reported at 2 s and again at 10 s: the second shows whether a
         * stuck loop still moves (state, DMA queue) or is frozen solid. */
        if (still == 8 || still == 40)
        {
            LogField(still == 8 ? "STALL" : "STALL10", beat);
            LogTasks();
        }
    }
}

void CtrGameBridge_StartWatchdog(void)
{
    s32 priority = 0x30;

    svcGetThreadPriority(&priority, CUR_THREAD_HANDLE);
    /* Higher priority than the game thread so it runs while that spins. */
    if (priority > 0x18)
        --priority;
    LightEvent_Init(&sWatchdogStop, RESET_STICKY);
    sWatchdog = threadCreate(WatchdogMain, NULL, 8 * 1024, priority, -2, false);
    if (sWatchdog == NULL)
        CtrLog_Write(CTR_LOG_GAME, "watchdog unavailable");
    aptHook(&sAptHook, Lifecycle, NULL);
    sAptHooked = true;
}

/* Its stack is heap memory, which libctru unmaps at exit: stop it first. */
void CtrGameBridge_StopWatchdog(void)
{
    if (sAptHooked)
    {
        aptUnhook(&sAptHook);
        sAptHooked = false;
    }
    if (sWatchdog == NULL) return;
    LightEvent_Signal(&sWatchdogStop);
    threadJoin(sWatchdog, U64_MAX);
    threadFree(sWatchdog);
    sWatchdog = NULL;
}

void VBlankIntrWait(void)
{
    CtrPerf_GameEnd();
    uintptr_t perfCallback = (uintptr_t)gMain.callback2, perfTask = 0;
    for (unsigned i = 0; i < NUM_TASKS; ++i)
        if (gTasks[i].isActive) { perfTask = (uintptr_t)gTasks[i].func; break; }
    /* One-shot marker: proves the game reached its first VBlank wait without
     * spamming the log every frame. */
    static bool firstVBlankLogged = false;
    uint32_t beat = ++sHeartbeat;
    if (beat % 300 == 0)
        LogField("FIELD", beat);
    if (!firstVBlankLogged)
    {
        firstVBlankLogged = true;
        CtrLog_Write(CTR_LOG_GAME, "first VBlank");
    }
    /* Copyright has finished fading in by this boundary, before its frame-140
     * fade out. Sample BEFORE Present, not after VBlank uploads the next frame.
     * Saturate the counter: exactly one snapshot per process, even on reset. */
    static unsigned probeFrame;
    if (probeFrame < 60 && ++probeFrame == 60)
    {
        CtrLog_Write(CTR_LOG_VIDEO,
            "SNAP frame=60 cb1=%p cb2=%p copyright=%u state=%u hblank=%p scanline=%u",
            (void *)gMain.callback1, (void *)gMain.callback2,
            gMain.callback2 == CB2_InitCopyrightScreenAfterBootup, gMain.state,
            (void *)gMain.hblankCallback, gScanlineEffect.state);
        CtrVideo_DumpState();
    }
    /* Title's DMA0 writes BLDY after each GBA scanline. Capture its current
     * transfer before the frame is presented; VBlank arms the next transfer. */
    uint16_t lineBrightness[160];
    uint64_t perfScanline = CtrPerf_Begin();
    if (CtrDma_ReadHBlank16(0, gGbaShadow.regs + 0x54,
                             lineBrightness, 160))
        CtrVideo_SetLineBrightness(lineBrightness, 160);
    else
        CtrVideo_SetLineBrightness(NULL, 0);
    uint16_t flashWindow[160];
    if (CtrField_FlashMaskActive() && CtrBottom_IsFieldCallback()
        && CtrDma_ReadHBlank16(0, gGbaShadow.regs + 0x40, flashWindow, 160))
        CtrVideo_SetFlashWindow(flashWindow, 160);
    else
        CtrVideo_SetFlashWindow(NULL, 0);
    CtrPerf_End(PERF_SCANLINE, perfScanline);
    /* The Trainer Card's flip scrolls BG0 line by line from its HBlank callback
     * (HBlankCB_TrainerCard); there is no HBlank here, so its buffer is BG0's
     * per-line vertical scroll for this frame, line y showing entry y - 1 as
     * the callback writes it at the end of the line before. */
    {
        const uint16_t *flip = CtrTrainerCard_FlipScroll();
        CtrVideo_SetLineScroll(2, false, flip, flip != NULL ? 160 : 0);
    }
    /* Input and the bottom UI run before composition so an already-decided
     * TOP hold suppresses the very frame that requests it: opening Party via
     * physical START, bottom touch or battle never flashes the GBA Party
     * screen. Game-visible behavior is unchanged: FireRed samples KEYINPUT
     * from its main loop (ReadKeys), never from its VBlank handler, and
     * SetHeldKeys still lands once per VBlank before game code resumes.
     * PERF_VBLANK now spans input/bottom/keys instead of DMA-to-keys. */
    uint64_t perfVblank = CtrPerf_Begin();
    CtrInput_Scan();
    /* Bottom UI runs after the single HID poll and before the game reads
     * keys: touch drives the real Party state machine via injected D-pad/A.
     * Physical controls cancel any running Plan inside CtrBottom_Frame. */
    CtrBottom_Frame();
    CtrVideo_SetMapNamePopup(IsMapNamePopupTaskActive() != 0);   /* the map-name popup's own lifetime, for the compositor */
    GbaShadow_SetHeldKeys((uint16_t)(CtrBottom_FilterGameKeys(CtrInput_Get()->held) | CtrBottom_InjectedKeys()));
    CtrPerf_End(PERF_VBLANK, perfVblank);
    /* The overworld turbo (3ds_fast_forward.h): a frame that is not shown skips the
     * compositor, and with it the wait for the screen, and the sound engine;
     * the game, its VBlank handler, input, bottom screen and save still run.
     * START (a new press) cycles it where CtrBottom_TurboCanCycle allows, and it is in force only where
     * CtrBottom_TurboApplies says the stable field, or a battle's stable core, is the screen's owner. */
    CtrFastForward_Turbo(&sFastForward, (CtrInput_Get()->physicalDown & CTR_KEY_START) != 0,
                         CtrBottom_TurboCanCycle() != 0, CtrBottom_TurboApplies() != 0);
    bool shown = CtrFastForward_Show(&sFastForward);
    CtrVideo_SetFastForward(CtrFastForward_Speed(&sFastForward));
    UpdateStage();
    /* FrameBegin inside the compositor is the frame pacing boundary. */
    if (shown)
        CtrVideo_Present();
    CtrDma_RunVBlank();
    /* A failed flush keeps its sectors pending (3ds_flash.c), so a transient SD error is
     * retried on the next frames; only one that persists for a second stops the game. */
    if (CtrFlash_Flush())
        sFlashFailures = 0;
    else if (++sFlashFailures == 1)
        CtrLog_Write(CTR_LOG_FS, "save flush failed; retrying");
    else if (sFlashFailures >= 60)
        exit(0);
    /* GBA scanline order per frame: VCount match at line 150 advances the
     * m4a PCM ring (m4aSoundVSync), then VBlank at line 160 mixes the fresh
     * slice (SoundMain). Dispatching only VBlank left pcmDmaCounter stuck. */
    CtrAudio_SetSkipSound(!shown);
    GbaShadow_DispatchVCount(gIntrTable[0]);
    GbaShadow_DispatchVBlank(gIntrTable[4]);
    CtrAudio_SetSkipSound(false);
    /* GBA raises these from interrupts regardless of a lagging main loop.
     * Replay only the m4a tick pair (not VBlankIntr, so no game-frame
     * logic) for sound frames real time says were missed. With the sound
     * worker, each pair is one more hand-over (3ds_sound.c). */
    for (unsigned extra = shown ? CtrAudio_CatchUpFrames() : 0; extra != 0; --extra)
    {
        GbaShadow_DispatchVCount(m4aSoundVSync);
        GbaShadow_DispatchVBlank(m4aSoundMain);
    }
    ProbeScene(CtrVideo_GetStats()->frames);

    /* 180..187 spans eight consecutive frames: a full seven-slice ring cycle
     * plus wrap, proving each 224-sample block is visited once. Frame 600 is
     * a late stability sample. One-shot only, no per-frame logging. */
    static unsigned audioProbeFrame;
    if (audioProbeFrame < 600)
    {
        ++audioProbeFrame;
        if ((audioProbeFrame >= 180 && audioProbeFrame <= 187)
            || audioProbeFrame == 600)
        {
            /* The mix may be running on the sound worker: read it whole. */
            CtrAudio_LockSound();
            ProbeAudio(audioProbeFrame);
            CtrAudio_UnlockSound();
        }
    }

    if (!aptMainLoop())
        exit(0);
    ResumeAfterSuspend();
    const CtrVideoStats *perfVideo = CtrVideo_GetStats();
    CtrPerf_EndFrame(perfCallback, perfTask, perfVideo->display,
                     perfVideo->stereo, perfVideo->planes, perfVideo->gpuMs,
                     perfVideo->commandUsage);
}
