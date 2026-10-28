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
#include "overworld.h"
#include "fieldmap.h"
#include "constants/map_types.h"
#include "3ds_audio.h"
#include "3ds_audio_cadence.h"
#include "3ds_input.h"
#include "3ds_bottom.h"
#include "3ds_dma.h"
#include "3ds_flash.h"
#include "3ds_log.h"
#include "3ds_platform.h"
#include "3ds_video.h"
#include "gba_shadow.h"
#include "3ds_perf.h"
#include "3ds_fast_forward.h"
#include "3ds_settings.h"
#include "3ds_poketouch_options.h"
#include "3ds_day_night_lights.h"
#include "3ds_palette_fade.h"

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
/* Despite its name, TRUE while the forest preview's task is NOT running (map_preview_screen.c). */
extern bool32 ForestMapPreviewScreenIsRunning(void);

/* The field's own main callback on an outdoor map, outside a battle: where the
 * day/night tint applies (FireRed's outdoor types, as in 3ds_map_edge.h). */
static bool FieldOutdoors(void)
{
    unsigned type = gMapHeader.mapType;

    return gMain.callback1 == CB1_Overworld && !gMain.inBattle
        && (type == MAP_TYPE_TOWN || type == MAP_TYPE_CITY || type == MAP_TYPE_ROUTE || type == MAP_TYPE_OCEAN_ROUTE);
}

/* The windows lit at night, by tileset (3ds_day_night_lights.h). */
extern const struct Tileset gTileset_General, gTileset_PalletTown, gTileset_ViridianCity, gTileset_PewterCity,
    gTileset_VermilionCity, gTileset_LavenderTown, gTileset_CeladonCity, gTileset_FuchsiaCity,
    gTileset_SaffronCity, gTileset_IndigoPlateau, gTileset_CinnabarIsland, gTileset_SeviiIslands123,
    gTileset_SeviiIslands45, gTileset_SeviiIslands67;

#define LIGHTS(tileset, table) {&gTileset_##tileset, table, sizeof(table) / sizeof(table[0])}
static const struct
{
    const struct Tileset *tileset;
    const uint16_t *lights;
    unsigned count;
} sTilesetLights[] = {
    LIGHTS(General, kCtrLights_General),
    LIGHTS(PalletTown, kCtrLights_PalletTown),
    LIGHTS(ViridianCity, kCtrLights_ViridianCity),
    LIGHTS(PewterCity, kCtrLights_PewterCity),
    LIGHTS(VermilionCity, kCtrLights_VermilionCity),
    LIGHTS(LavenderTown, kCtrLights_LavenderTown),
    LIGHTS(CeladonCity, kCtrLights_CeladonCity),
    LIGHTS(FuchsiaCity, kCtrLights_FuchsiaCity),
    LIGHTS(SaffronCity, kCtrLights_SaffronCity),
    LIGHTS(IndigoPlateau, kCtrLights_IndigoPlateau),
    LIGHTS(CinnabarIsland, kCtrLights_CinnabarIsland),
    LIGHTS(SeviiIslands123, kCtrLights_SeviiIslands123),
    LIGHTS(SeviiIslands45, kCtrLights_SeviiIslands45),
    LIGHTS(SeviiIslands67, kCtrLights_SeviiIslands67),
};
#undef LIGHTS

static const uint16_t *TilesetLights(const struct Tileset *tileset, unsigned *count)
{
    for (unsigned i = 0; i < sizeof(sTilesetLights) / sizeof(sTilesetLights[0]); ++i)
        if (sTilesetLights[i].tileset == tileset)
        {
            *count = sTilesetLights[i].count;
            return sTilesetLights[i].lights;
        }
    *count = 0;
    return NULL;
}

/*
 * Where the field's tilemap stands on the map, for the windows' own hours.
 * field_camera.c keeps its tile offset to itself, but the offset only moves
 * with the camera: twice the camera's map position minus the offset (sCameraK)
 * stays the same while walking. It is found by matching the tilemap buffers
 * against the metatiles the map holds in view, then only checked each frame;
 * a map load or a connection changes it and it is found again.
 */
#define CAMERA_VIEW_LEFT (-4 - FIELD_VIEW_OFFSET_X / 8)
static bool sCameraKnown;
static int sCameraKX, sCameraKY;
/* An offset was found on this map: while it is searched for again the windows keep it (it only changes with a map
 * load or a connection) rather than all lighting up, which switched the windows of a whole building on and off. */
static bool sCameraFound;
static unsigned sCameraMap = ~0u;
/* Frames to wait before searching again after a search found nothing. */
static unsigned sCameraWait;
/* Searches in a row that found nothing: each one waits longer (CAMERA_WAIT_MAX at most). A search is ~500
 * offsets of up to 132 metatile lookups, ~47 ms on the emulated Old 3DS. */
static unsigned sCameraFails;
#define CAMERA_WAIT_MAX 64u
extern int CtrBottom_IsFieldCallback(void);
/* Where the offset was last checked, and for how many frames since nothing
 * moved: standing still, the check (132 metatiles looked up) runs one frame
 * in CAMERA_RECHECK, not every frame. */
#define CAMERA_RECHECK 16u
static int sCameraCheckedX = -1, sCameraCheckedY = -1;
static unsigned sCameraStill;
/* Frames in a row the known offset has failed its check. */
static unsigned sCameraBad;
/* A step moves the player before the field redraws its new row or column, so for a frame or two the buffers
 * disagree with a right offset: that is not a lost camera. Losing it lit every window until it was found again. */
#define CAMERA_GRACE 12

/* How many sampled in-map metatiles the buffers disagree with at this offset
 * (stops past limit); *samples counts the ones looked at. */
static unsigned CameraMisses(int tileX, int tileY, unsigned limit, unsigned *samples)
{
    const struct MapLayout *layout = gMapHeader.mapLayout;
    unsigned misses = 0;

    *samples = 0;
    for (int i = 4; i <= 24; i += 2)
        for (int j = 0; j <= 22; j += 2)
        {
            s32 x = gSaveBlock1Ptr->pos.x + j / 2, y = gSaveBlock1Ptr->pos.y + i / 2;
            u32 id;
            const u16 *tiles, *buffer;
            int tx = (tileX + j) & 63, ty = (tileY + i) & 31;
            unsigned offset = (tx >= 32 ? 1024u : 0u) + (unsigned)ty * 32 + (unsigned)(tx & 31);

            if (x < MAP_OFFSET || y < MAP_OFFSET || x >= layout->width + MAP_OFFSET || y >= layout->height + MAP_OFFSET)
                continue;
            id = MapGridGetMetatileIdAt(x, y);
            if (id >= NUM_METATILES_TOTAL) continue;
            tiles = id < NUM_METATILES_IN_PRIMARY
                ? layout->primaryTileset->metatiles + id * NUM_TILES_PER_METATILE
                : layout->secondaryTileset->metatiles + (id - NUM_METATILES_IN_PRIMARY) * NUM_TILES_PER_METATILE;
            CTR_ASSET_READ(tiles, NUM_TILES_PER_METATILE * sizeof(*tiles));
            buffer = MapGridGetMetatileLayerTypeAt(x, y) == METATILE_LAYER_TYPE_NORMAL ? gBGTilemapBuffers1 : gBGTilemapBuffers3;
            ++*samples;
            if (buffer[offset] != tiles[0] || buffer[offset + 1] != tiles[1]
                || buffer[offset + 32] != tiles[2] || buffer[offset + 33] != tiles[3])
                if (++misses > limit) return misses;
        }
    return misses;
}

static bool UpdateCamera(void)
{
    unsigned map = ((unsigned)gSaveBlock1Ptr->location.mapGroup << 8) | gSaveBlock1Ptr->location.mapNum;
    int px = gSaveBlock1Ptr->pos.x, py = gSaveBlock1Ptr->pos.y;
    unsigned samples, best = ~0u;

    if (!gMapHeader.mapLayout || !gBGTilemapBuffers1 || !gBGTilemapBuffers3) return false;
    if (map != sCameraMap) sCameraKnown = sCameraFound = false;
    sCameraMap = map;
    /* A menu over the map (Party, Pokedex, Bag, Summary...) has replaced the tilemap buffers with its own: there is
     * nothing to check or to find in them, and looking cost a 47 ms frame every nine (port.log, 0.2.5 Old 3DS mode).
     * What was found stays for the return to the field, whose own buffers are checked again then. */
    if (!CtrBottom_IsFieldCallback()) return sCameraKnown;
    if (sCameraKnown && px == sCameraCheckedX && py == sCameraCheckedY && ++sCameraStill < CAMERA_RECHECK)
        return true;
    sCameraStill = 0;
    sCameraCheckedX = px;
    sCameraCheckedY = py;
    if (sCameraKnown)
    {
        if (CameraMisses((px * 2 - sCameraKX) & 63, (py * 2 - sCameraKY) & 31, 8, &samples) <= 8)
        {
            sCameraBad = 0;
            return true;
        }
        /* Not a settled check: look again next frame. */
        sCameraCheckedX = -1;
        if (++sCameraBad <= CAMERA_GRACE) return true;
    }
    sCameraBad = 0;
    if (sCameraKnown)
        CtrLog_Write(CTR_LOG_VIDEO, "DAYNIGHT camera lost map=%04x pos=%d,%d", map, px, py);
    sCameraKnown = false;
    if (sCameraWait) { --sCameraWait; return false; }
    for (int ty = 0; ty < 32 && best; ty += 2)
        for (int tx = 0; tx < 64 && best; tx += 2)
        {
            unsigned misses = CameraMisses(tx, ty, best < 3 ? best : 3, &samples);

            if (misses < best && misses <= 2 && samples >= 24)
            {
                best = misses;
                sCameraKX = (px * 2 - tx) & 63;
                sCameraKY = (py * 2 - ty) & 31;
                sCameraKnown = true;
            }
        }
    if (!sCameraKnown)
    {
        sCameraWait = 8u << (sCameraFails < 3 ? sCameraFails : 3);
        if (sCameraWait > CAMERA_WAIT_MAX) sCameraWait = CAMERA_WAIT_MAX;
        ++sCameraFails;
    }
    else
    {
        sCameraFails = 0;
        sCameraFound = true;
        CtrLog_Write(CTR_LOG_VIDEO, "DAYNIGHT camera map=%04x pos=%d,%d offset=%d,%d misses=%u", map, px, py,
                     (px * 2 - sCameraKX) & 63, (py * 2 - sCameraKY) & 31, best);
    }
    return sCameraKnown;
}

static void UpdateDayNight(void)
{
    bool outdoors = CtrOptions_DayNight() && FieldOutdoors();
    const struct MapLayout *layout = gMapHeader.mapLayout;
    unsigned primaryCount = 0, secondaryCount = 0;
    const uint16_t *primary = outdoors && layout ? TilesetLights(layout->primaryTileset, &primaryCount) : NULL;
    const uint16_t *secondary = outdoors && layout ? TilesetLights(layout->secondaryTileset, &secondaryCount) : NULL;
    bool camera = outdoors && (primary || secondary) && gSaveBlock1Ptr && (UpdateCamera() || sCameraFound);

    CtrVideo_SetDayNight(outdoors, CtrOptions_DayNightSecond(), primary, primaryCount, secondary, secondaryCount);
    if (camera)
        CtrVideo_SetDayNightCamera(true, (gSaveBlock1Ptr->pos.x * 2 - sCameraKX) & 63,
                                   (gSaveBlock1Ptr->pos.y * 2 - sCameraKY) & 31, CAMERA_VIEW_LEFT,
                                   gSaveBlock1Ptr->pos.x, gSaveBlock1Ptr->pos.y, sCameraMap);
    else
        CtrVideo_SetDayNightCamera(false, 0, 0, 0, 0, 0, 0);
}

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
/* The trainer-pic transition's banners (battle_transition.c): running, and BG0's scroll per line. */
extern int CtrMugshot_Active(void);
extern int CtrTransition_Bg0Canvas(void);
extern void CtrMugshot_LineScroll(uint16_t *lines);

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
    CtrVideo_SetTransitionBg0Canvas(sTransition.on ? (unsigned)CtrTransition_Bg0Canvas() : 0);
    CtrVideo_SetNativeUi(sNativeUi.on);
    CtrVideo_SetFieldBarnDoorWipe(CtrField_BarnDoorWipeActive() != 0);
    CtrVideo_SetNativeViewport(CtrField_CaveTransitionActive() != 0);
    CtrVideo_SetFieldMoveShowMon(CtrFieldEffect_ShowMonActive() != 0);
}


/* How far FireRed's palette fade has taken the backgrounds towards one colour (0 none, 16 all the way), for what the
 * port draws over the screen (the title's version label). 0 when they are not one uniform fade. */
unsigned CtrGame_ScreenFade(uint16_t *target)
{
    unsigned coeff = 0;

    *target = 0;
    if (!CtrPal_DetectBlend(gPlttBufferUnfaded, gPlttBufferFaded, 256, &coeff, target))
        return 0;
    return coeff;
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

/* A VBlank span (input, lower screen, field state, keys) over 100 ms is logged part by part. */
#define VBLANK_SLOW_TICKS (SYSCLOCK_ARM11 / 10)
static unsigned long TicksMs(u64 ticks) { return (unsigned long)(ticks / (SYSCLOCK_ARM11 / 1000)); }
/* The turbo's time budget: one refresh of the screen (59.83 Hz), less what a shown frame does before it waits for the
 * refresh (the compositor's preparation) and some slack. */
#define TURBO_REFRESH_TICKS ((u64)SYSCLOCK_ARM11 * 1671 / 100000)
#define TURBO_SLACK_TICKS ((u64)SYSCLOCK_ARM11 / 500)

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
    /* The banners' edges are a WIN0H value per line, like the cave's flash. */
    if (((CtrField_FlashMaskActive() && CtrBottom_IsFieldCallback()) || CtrMugshot_Active())
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
        uint16_t banners[160];

        if (flip == NULL && CtrMugshot_Active())
        {
            /* The banners slide the other way in the lower half (HBlankCB_Mugshots). */
            CtrMugshot_LineScroll(banners);
            CtrVideo_SetLineScroll(0, false, banners, 160);
        }
        else
            CtrVideo_SetLineScroll(2, false, flip, flip != NULL ? 160 : 0);
    }
    /* Input and the bottom UI run before composition so an already-decided
     * TOP hold suppresses the very frame that requests it: opening Party via
     * physical START, bottom touch or battle never flashes the GBA Party
     * screen. Game-visible behavior is unchanged: FireRed samples KEYINPUT
     * from its main loop (ReadKeys), never from its VBlank handler, and
     * SetHeldKeys still lands once per VBlank before game code resumes.
     * PERF_VBLANK now spans input/bottom/keys instead of DMA-to-keys.
     * Each part is also timed on its own: a 3.2 s field frame in the 0.2.8
     * Old 3DS port.log was spent here with no data read, and the span alone
     * could not say where (VBLANK_SLOW, with the lower screen's state). */
    uint64_t perfVblank = CtrPerf_Begin();
    u64 phaseInput = svcGetSystemTick();
    CtrInput_Scan();
    u64 phaseBottom = svcGetSystemTick();
    /* Bottom UI runs after the single HID poll and before the game reads
     * keys: touch drives the real Party state machine via injected D-pad/A.
     * Physical controls cancel any running Plan inside CtrBottom_Frame. */
    CtrBottom_Frame();
    CtrBottom_BlitFrameEnd();
    u64 phaseField = svcGetSystemTick();
    CtrVideo_SetMapNamePopup(IsMapNamePopupTaskActive() != 0);   /* the map-name popup's own lifetime, for the compositor */
    CtrVideo_SetForestPreview(ForestMapPreviewScreenIsRunning() == FALSE);
    UpdateDayNight();
    u64 phaseKeys = svcGetSystemTick();
    GbaShadow_SetHeldKeys((uint16_t)(CtrBottom_FilterGameKeys(CtrInput_Get()->held) | CtrBottom_InjectedKeys()));
    u64 phaseEnd = svcGetSystemTick();
    CtrPerf_End(PERF_VBLANK, perfVblank);
    if (phaseEnd - phaseInput > VBLANK_SLOW_TICKS)
    {
        CtrLog_Write(CTR_LOG_GAME, "VBLANK_SLOW f=%lu input=%lu bottom=%lu field=%lu keys=%lu ms",
                     (unsigned long)gCtrPerf.frame, TicksMs(phaseBottom - phaseInput), TicksMs(phaseField - phaseBottom),
                     TicksMs(phaseKeys - phaseField), TicksMs(phaseEnd - phaseKeys));
        CtrBottom_LogState("VBLANK_SLOW");
    }
    /* The overworld turbo (3ds_fast_forward.h): a frame that is not shown skips the
     * compositor, and with it the wait for the screen, and the sound engine;
     * the game, its VBlank handler, input, bottom screen and save still run.
     * START (a new press) cycles it where CtrBottom_TurboCanCycle allows, and it is in force only where
     * CtrBottom_TurboApplies says the stable field, or a battle's stable core, is the screen's owner. */
    static bool turboRestored;
    if (!turboRestored)
    {
        turboRestored = true;
        sFastForward.turbo = CtrSettings_Turbo();   /* the speed chosen in an earlier session */
    }
    CtrFastForward_Turbo(&sFastForward, (CtrInput_Get()->physicalDown & CTR_KEY_START) != 0,
                         CtrBottom_TurboCanCycle() != 0, CtrBottom_TurboApplies() != 0, CtrBottom_MenuChangingOver() != 0);
    {
        u64 settingsStart = osGetTime();
        CtrSettings_SetTurbo(sFastForward.turbo);   /* writes the file only when a press changed it */
        u64 settingsMs = osGetTime() - settingsStart;
        if (settingsMs >= 5)
            CtrLog_Write(CTR_LOG_FS, "settings: the turbo change held the game thread %lu ms", (unsigned long)settingsMs);
    }
    /* A turbo frame is shown before its Nth when one more hidden frame (its cost measured as the game goes) would
     * not fit before the screen's next refresh: missing it would halve the frames shown and leave the time idle. */
    static u64 hiddenTicks = (u64)SYSCLOCK_ARM11 / 400, lastDecision;
    static bool lastShown = true;
    u64 decision = svcGetSystemTick();
    if (!lastShown) {
        u64 sample = decision - lastDecision;   /* a stall (a load) counts as one refresh, not as many */
        hiddenTicks = (hiddenTicks * 3 + (sample < TURBO_REFRESH_TICKS ? sample : TURBO_REFRESH_TICKS)) / 4;
    }
    bool late = decision - CtrVideo_FrameStartTick() + hiddenTicks + TURBO_SLACK_TICKS > TURBO_REFRESH_TICKS;
    bool shown = CtrFastForward_ShowBy(&sFastForward, late);
    lastDecision = decision;
    lastShown = shown;
    CtrVideo_SetFastForward(CtrFastForward_Speed(&sFastForward));
    UpdateStage();
    /* FrameBegin inside the compositor is the frame pacing boundary. */
    if (shown)
        CtrVideo_Present();
    CtrDma_RunVBlank();
    /* A failed flush keeps its sectors pending (3ds_flash.c), so a transient SD error is
     * retried on the next frames; only one that persists for a second stops the game, on
     * a screen that says so (a save the game itself makes, such as the Hall of Fame's,
     * has already been reported as written by then). */
    if (CtrFlash_Flush())
        sFlashFailures = 0;
    else if (++sFlashFailures == 1)
        CtrLog_Write(CTR_LOG_FS, "save flush failed; retrying");
    else if (sFlashFailures >= 60)
        CtrPlatform_ShowError("twinembers save error",
                              "The game could not be written to the SD card, so the last save may be incomplete.",
                              "Check the card (write-protect switch, free space) before playing again. "
                              "twinembers.sav.bak in /3ds/twinembers/ holds the save from the last start.");
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
