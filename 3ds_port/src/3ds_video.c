/* Adapted from ZallaxDev/pokeemerald-3Ds-dualscreen 3ds_video.c at the
 * revision in upstream.lock. Original port code: MIT; see
 * licenses/ZallaxDev-MIT.txt. Voxel remains disabled in this build. */
/* GBA compatibility compositor on PICA200. CPU work only decodes dirty tiles
 * into a texture atlas and submits geometry. All rasterization, transforms,
 * transparency, priority composition and scaling happen on the GPU. */
#include <3ds.h>
#include <citro2d.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "3ds_platform.h"
#include "3ds_vram_track.h"
#include "3ds_video.h"
#include "3ds_obj_window.h"
#include "3ds_effect_oam.h"
#include "3ds_dirty_rect.h"
#include "3ds_perf.h"
#include "3ds_presentation.h"
#include "3ds_intro_native.h"
#include "3ds_palette_fade.h"
#include "3ds_poketouch.h"

/* Native top-screen layout, shared by the surface and all stereo planes. */
static CtrPresentation sPresentation;
static bool sFieldMoveShowMon;


/* Count public submissions, not guessed PICA draw calls. Cache/draw timers
 * rotate a 1/32 sample across frames; coarse timers measure every frame. */
static const void *sPerfTexture, *sPerfTarget;
static uint64_t PerfDrawStart(void)
{
    CTR_PERF_COUNT(PERF_QUADS, 1);
    if (!gCtrPerf.enabled || ((gCtrPerf.counters[PERF_QUADS]+gCtrPerf.frame)&31)) return 0;
    CTR_PERF_COUNT(PERF_DRAW_SAMPLE_COUNT, 1);
    return CtrPerf_Begin();
}
static bool PerfImageAt(C2D_Image image, float x, float y, float z,
                        const C2D_ImageTint *tint, float sx, float sy)
{
    if (sPerfTexture != image.tex) { CTR_PERF_COUNT(PERF_TEXTURE_SWITCH, 1); sPerfTexture=image.tex; }
    uint64_t start=PerfDrawStart();
    bool result=C2D_DrawImageAt(image,x,y,z,tint,sx,sy);
    CtrPerf_End(PERF_DRAW_SAMPLE,start);
    return result;
}
static bool PerfRect(float x,float y,float z,float w,float h,uint32_t color)
{
    sPerfTexture=NULL;
    uint64_t start=PerfDrawStart();
    bool result=C2D_DrawRectSolid(x,y,z,w,h,color);
    CtrPerf_End(PERF_DRAW_SAMPLE,start);
    return result;
}
static void PerfFlush(void)
{
    CTR_PERF_COUNT(PERF_FLUSH_REQUEST, 1);
    uint64_t start=CtrPerf_Begin();
    C2D_Flush();
    CtrPerf_End(PERF_SUBMIT,start);
}
static void PerfSplit(u8 flags)
{
    CTR_PERF_COUNT(PERF_FRAME_SPLITS, 1);
    C3D_FrameSplit(flags);
}
#define C3D_FrameSplit PerfSplit
/* The target Citro2D draws on, for passes that need its dimensions. */
static C3D_RenderTarget *sDrawTarget;

static void PerfSceneBegin(C3D_RenderTarget *target)
{
    sDrawTarget = target;
    if (sPerfTarget != target) { CTR_PERF_COUNT(PERF_TARGET_SWITCH,1); sPerfTarget=target; }
    C2D_SceneBegin(target);
}
#define C2D_DrawImageAt PerfImageAt
#define C2D_DrawRectSolid PerfRect
#define C2D_Flush PerfFlush
#define C2D_SceneBegin PerfSceneBegin

/* The voxel overworld replaces this compositor's output while the player is
 * walking around; every other game state keeps the 2D path below. */
#ifndef CTR_VOXEL_ENABLED
#define CTR_VOXEL_ENABLED 0
#endif
#if CTR_VOXEL_ENABLED
#include "voxel/ctr_voxel.h"
#endif

/*
 * The game lays itself out over the whole 400x240 viewport, so screen and
 * logical coordinates are the same. A background only covers the part of the
 * screen its tilemap actually spans; see DrawTextBg.
 *
 * A GBA stage (CtrVideo_SetStage) is the exception: its 240x160 picture sits
 * 1:1 at (CTR_STAGE_X, CTR_STAGE_Y) and the rest of the screen is filled around
 * it. sViewX/Y is where the GBA origin lands on the screen; everything else
 * stays in GBA coordinates, so the space around the picture is negative or
 * past 240x160.
 *
 * A centred GBA screen (CtrVideo_SetCentred) sits at the same place, but its
 * margins are not filled from the picture: only the layers that wrap on the
 * GBA (an affine map with wrap-around) and sprites reach into them, and text
 * layers stop at the edge of the 240x160 screen.
 *
 * The battle scene (CtrVideo_SetBattle) sits centred across and on the bottom
 * edge, so its text box is the bottom of the top screen. The text box is
 * stretched to the whole width, and the scene above and beside it comes from
 * its own layers as the GBA would wrap them (DrawBattleBg).
 */
static int sViewX, sViewY;
/*
 * Magnification of the frame being composed: a screen point is
 * (gba + view) * zoom + offset. The battle scene is composed at
 * CTR_BATTLE_ZOOM and its text box at 1 (DrawBattleTextLayer).
 */
static float sZoom = 1.0f, sOffX, sOffY;
/* Size of the surface being composed: the screen, or the battle scene's. */
static int sTargetW = CTR_GAME_WIDTH, sTargetH = CTR_GAME_HEIGHT;
/* Height of the texture behind it: scissor rows count from its far edge. */
_Static_assert(CTR_OBJ_SCREEN_W == CTR_GAME_WIDTH && CTR_OBJ_SCREEN_H == CTR_GAME_HEIGHT,
               "the sprite window is the native field");
static int sSurfaceH = 256;
/*
 * The battle scene at 1.5 is not composed at 1.5: nearest sampling at a
 * fractional scale makes GBA pixels alternately one and two screen pixels
 * wide, which shimmers on hardware as anything moves. It is composed at 2,
 * every GBA pixel an exact 2x2 block, into a surface of its own, and that
 * surface is drawn at 0.75 with bilinear filtering: every pixel the same
 * size, edges a pixel soft ("sharp bilinear"). The text box is composed
 * after it at 1:1, crisp. See RenderBattleScene.
 */
#define SCENE_ZOOM 2.0f
#define SCENE_W 1024
#define SCENE_H 256
static C3D_Tex sSceneTex;
static C3D_RenderTarget *sScene;
static bool sSceneFailed;
static uint32_t sSceneUsedFrame, sSceneFailFrame;
/* Layers (Layers mask bits) the current composition pass leaves out. */
static unsigned sLayerExclude;
/* Screen pixels per GBA pixel that the stereo displacement is measured in. */
static float sShiftZoom = 1.0f;
#define CTR_VIEW_X sViewX
#define CTR_VIEW_Y sViewY
/* Requested by the game bridge, and what the current frame is composed as. */
static bool sIntroRequested, sIntro;
static unsigned sIntroBackgrounds, sIntroRepeat;
static int sIntroTop, sIntroBottom = 160;
static bool sStageRequested, sStage;
static bool sStageUiRequested, sStageUi;
static bool sCentredRequested, sCentred;
static bool sNativeUiRequested, sNativeUi;
static bool sBattleRequested, sBattle;
/* Optional strict GBA clipping; native field is the default. */
static bool sNativeRequested, sNative;
/* FireRed's battle transition owns the screen (battle_transition.c's VBlank callback): the field stays the native canvas
 * under it. This is NOT sNative, the strict 240x160 viewport. */
static bool sBattleTransitionRequested, sBattleTransition;
/* Visible extent in GBA coordinates. */
#define VIEW_LEFT (sNative ? 0 : (int)floorf(-sOffX / sZoom) - sViewX)
#define VIEW_TOP (sNative ? 0 : (int)floorf(-sOffY / sZoom) - sViewY)
#define VIEW_RIGHT (sNative ? 240 : (int)ceilf((sTargetW - sOffX) / sZoom) - sViewX)
#define VIEW_BOTTOM (sNative ? 160 : (int)ceilf((sTargetH - sOffY) / sZoom) - sViewY)

/*
 * Scanline scroll (CtrVideo_SetLineScroll): the eight BG scroll registers,
 * one value per line, and which of them the frame drives line by line.
 */
#define LINE_MAX 240
static uint16_t sLineScroll[8][LINE_MAX];
static unsigned sLineMask, sLineCount;
static uint16_t sLineBrightness[160];
static unsigned sLineBrightnessCount;
static bool sBrightnessOverride;
static unsigned sBrightnessValue;

#define ATLAS_SIZE 1024
#define CACHE_COUNT 16384
#define HASH_COUNT 32768
/* A full native BG with 2x2 mosaic needs 24,000 sample quads. */
#define MAX_DRAWS 32768

typedef struct
{
    uint32_t key, checked, paletteVersion;
    /* Changes whenever this slot is decoded again (sTileSerial). */
    uint32_t serial;
    uint8_t bytes[64];
    uint32_t colors[8];
    bool valid, visible;
} Tile;

#if CTR_VOXEL_ENABLED
static uint32_t sVoxelWeatherOam[4];
static bool sVoxelWeatherOnly;

void CtrVideo_ClearVoxelWeatherOam(void)
{
    memset(sVoxelWeatherOam, 0, sizeof(sVoxelWeatherOam));
}

void CtrVideo_MarkVoxelWeatherOam(unsigned first, unsigned end)
{
    if (end > 128) end = 128;
    for (unsigned i = first; i < end; ++i)
        sVoxelWeatherOam[i >> 5] |= 1u << (i & 31);
}
#else
void CtrVideo_ClearVoxelWeatherOam(void) {}
void CtrVideo_MarkVoxelWeatherOam(unsigned first, unsigned end)
{
    (void)first;
    (void)end;
}
#endif

static CtrVideoMemory sMemory;
/* OBJ Window is an OAM alpha mask, not visible sprite geometry. */
static uint8_t sObjWindow[240][400];
/* Rows of sObjWindow holding at least one OBJ window pixel. */
static bool sObjWindowRow[240];

const uint8_t *CtrVideo_GetBgVram(void) { return sMemory.vram; }

void CtrVideo_NotifyTilesetAnimWrite(const void *dest, unsigned bytes)
{
#if CTR_VOXEL_ENABLED
    uintptr_t base = (uintptr_t)sMemory.vram;
    uintptr_t address = (uintptr_t)dest;
    if (base == 0 || address < base || address - base >= 0x8000 || bytes == 0)
        return;
    unsigned offset = (unsigned)(address - base);
    if (bytes > 0x8000 - offset) bytes = 0x8000 - offset;
    CtrVoxel_NotifyTilesetAnimWrite(offset / 32, (offset % 32 + bytes + 31) / 32);
#else
    (void)dest;
    (void)bytes;
#endif
}
static CtrVideoStats sStats;
/* Bumped when the tile cache is emptied: slots a walk saw are then stale. */
static uint32_t sCacheGeneration;
/* Frame token of the last background palette change. */
static uint32_t sBgPaletteStamp;
/* Last presented VRAM and the frame token at which each kilobyte of it last
 * changed (3ds_vram_track.h): a cached tile compares its bytes only when its
 * block changed after it was last checked. Only CtrVideo_Present reads the
 * tile cache, and it updates these first. */
enum { VRAM_TRACK_BLOCKS = 0x18000 / CTR_VRAM_TRACK_BLOCK };
static uint32_t sVramCopy[0x18000 / 4];
static uint32_t sVramStamp[VRAM_TRACK_BLOCKS];
static C3D_Tex sAtlas, sSurface;
static C3D_RenderTarget *sLogical, *sTop, *sTopRight;
/*
 * The bottom screen through the GPU (CtrVideo_SetBottomViewport): its own
 * screen target, the PokeTouch canvas as an RGB565 texture (rewritten only
 * where the canvas changed) and, over the viewport, the GBA picture sampled
 * straight from the logical surface. Nothing is read back from the GPU.
 */
static C3D_RenderTarget *sBottom;
static C3D_Tex sChrome;
static bool sBottomGpu;
static const uint16_t *sChromeCanvas;
static CtrDirtyList sChromeDirty;
static bool sStereo;
static Tile sTiles[CACHE_COUNT];
static uint16_t sHash[HASH_COUNT];
static uint16_t sPalette[512];
static uint16_t sTexturePalette[512];
static uint8_t sMorton[64];
static C2D_ImageTint sTint;
static uint32_t sPaletteVersion[34];
/*
 * Whole-screen fades (the battle transition's gray flash, fades to black)
 * are BlendPalette steps of one base palette towards one colour. Field
 * layers are always composed from tiles in sFadeBase - the palette itself
 * whenever no such fade runs - so a fade step changes no cell: the layer is
 * drawn tinted towards sFadeTarget by sFadeCoeff / 16 (UpdateFade). The base
 * keeps per-bank versions and change masks like the palette's own (index 32
 * for 256-colour tiles), so it re-decodes no more than the palette path.
 */
static uint16_t sFadeBase[256];
static uint16_t sFadeBaseTexture[256];
static uint32_t sFadeVersion[33];
static uint32_t sFadeChanges[33][8];
static uint32_t sFadeSeenVersion;
static bool sFadeBaseValid;
static unsigned sFadeCoeff;
static uint16_t sFadeTarget;
/* Field layers composed from sFadeBase this frame. */
static bool sLayerFadeTint[4];
static uint32_t sPaletteChanges[34][8];
static unsigned sUsed;

/* Forgets which tiles the cache holds (their atlas slots are decoded again on demand). Only the
 * index goes: the atlas, the palettes, VRAM and the serial counter stay. */
static void TileCacheReset(void)
{
    memset(sHash, 0, sizeof(sHash));
    sUsed = 0;
    ++sCacheGeneration;
}

/* View modes of ApplyPresentationView; the intro is the one whose tiles the rest never uses again. */
#define VIDEO_VIEW_MODE_INTRO 6

/* The intro composes its own screens through the shared tile cache and leaves them behind: the
 * cache is emptied once, on the frame that leaves the intro, and at no other mode change. */
static bool ShouldResetTileCache(int oldMode, int newMode)
{
    return oldMode == VIDEO_VIEW_MODE_INTRO && newMode != VIDEO_VIEW_MODE_INTRO;
}

/* A view mode changes (rare: never per frame). The log says what the cache held as the mode ended. */
static void TileCacheOnModeChange(int oldMode, int newMode)
{
    bool reset = ShouldResetTileCache(oldMode, newMode);

    CtrLog_Write(CTR_LOG_VIDEO, "TILECACHE %s mode=%d->%d used=%u generation=%u",
                 reset ? "intro-exit(reset)" : "mode-change", oldMode, newMode, sUsed, (unsigned)sCacheGeneration);
    if (reset)
        TileCacheReset();
}
static bool sC3d, sC2d;
static uint64_t sFpsStart;
static unsigned sFpsFrames;
static uint32_t sReported;

/*
 * Current clip rectangle. With windows enabled the frame is composed as a
 * partition of rectangles and every layer is visited once per rectangle, so
 * geometry outside the current one must not be submitted at all: the scissor
 * would discard it after paying for it, which is what pushed a windowed
 * overworld frame to three times the draw calls it needs.
 */
static int sClipX0, sClipY0, sClipX1 = CTR_GAME_WIDTH, sClipY1 = CTR_GAME_HEIGHT;

static void ClipToView(void)
{
    sClipX0 = VIEW_LEFT;
    sClipY0 = VIEW_TOP;
    sClipX1 = VIEW_RIGHT;
    sClipY1 = VIEW_BOTTOM;
}

/* Per-frame time in the layer walk and in the sprites, reported with fps. */
static uint64_t sBgTicks, sObjTicks;

/*
 * Stereoscopic depth. The GBA already orders everything by priority, 0 nearest
 * and 3 furthest, so that is the depth scale: priority 3 stays at the screen
 * plane and the rest come forward, which puts interface windows in front of
 * the map and the sprites between the two.
 *
 * The frame is composed once per eye with each layer displaced by whole
 * pixels; a fractional displacement would resample the very pixel grid this
 * renderer exists to preserve. At the widest slider setting the nearest layer
 * separates by CTR_STEREO_PIXELS in each eye.
 */
#define CTR_STEREO_PIXELS 1.0f

/* Displacement per depth unit for the eye being composed, and the resulting
 * displacement of the layer being drawn, both in whole screen pixels. */
static float sParallax, sLayerShift;
/* Fixed horizontal placement of every layer, on top of the depth parallax.
 * Zero for the 2D compositor, which reproduces the GBA frame as it is. */
static float sLayerOrigin;

/*
 * Composing the whole frame twice costs twice the CPU, which is a frame an Old
 * 3DS does not have. Instead each depth plane is composed once into its own
 * surface and the two eyes are then four textured quads each: the layer walk,
 * which is where the time goes, runs once per frame however many eyes there
 * are.
 *
 * A layer that blends with what is underneath it needs that underneath in the
 * same surface, so the planes are cut only where nothing blends across, and
 * the last one holds every remaining priority. A frame where everything blends
 * ends up as a single plane: no depth, but no extra cost either.
 */
/*
 * Three planes rather than one per priority: each one costs a surface to clear
 * and a textured quad per eye, and the overworld has no frame to spare for a
 * fourth. The two furthest priorities share the last plane, which still leaves
 * the interface, the near layer and the background at three depths.
 */
#define CTR_PRIORITIES 4
#define CTR_BANDS 3
static C3D_Tex sBandTex[CTR_BANDS];
static C3D_RenderTarget *sBand[CTR_BANDS];
static bool sBandsReady, sBandsFailed;
/* How many of the CTR_BANDS planes are really there: 3, or 2 when the third did not fit (BandsCreate). */
static unsigned sBandCount;
/* Asked by a stage whose layer textures found no VRAM while three planes held it: the third plane goes (BandsShrink),
 * not all of them. Honoured outside the frame, like sPlaneReleaseAsked. */
static bool sPlaneShrinkAsked;
/* A failed allocation is tried again this many frames later, not never: the
 * VRAM it needs may have been in use by the overworld only for a while. */
#define CTR_BANDS_RETRY_FRAMES 300u
static uint32_t sBandsRetryFrame;
/* Whether each plane drew anything; an empty one is not worth a quad. */
static bool sBandUsed[CTR_BANDS];
/* Depth planes the last frame used, 0 when it was composed per eye. */
static unsigned sPlanes;
/* Priorities the current composition pass may draw. */
static unsigned sPriorityMask = 15;

/*
 * Last blend configuration submitted. Reset whenever something else may have
 * changed the GPU state, which is once per eye pass.
 */
static unsigned sBlendKey = ~0u;
static void BlendForget(void) { sBlendKey = ~0u; }

static unsigned Reg(unsigned offset) { return sMemory.regs[offset / 2]; }
static unsigned Min(unsigned a, unsigned b) { return a < b ? a : b; }

static uint32_t SnapshotHash(const uint8_t *data, unsigned size)
{
    uint32_t hash = 2166136261u;
    for (unsigned i = 0; i < size; ++i)
        hash = (hash ^ data[i]) * 16777619u;
    return hash;
}

void CtrVideo_DumpState(void)
{
    unsigned objects = 0, windows = 0, affine = 0, semi = 0, mosaic = 0;
    for (unsigned i = 0; i < 128; ++i)
    {
        unsigned a0 = sMemory.oam[i * 4];
        unsigned mode = (a0 >> 10) & 3;
        if ((!(a0 & 0x100) && (a0 & 0x200)) || mode == 3 || (a0 >> 14) == 3)
            continue;
        ++objects;
        windows += mode == 2;
        semi += mode == 1;
        affine += (a0 & 0x100) != 0;
        mosaic += (a0 & 0x1000) != 0;
    }
    CtrLog_Write(CTR_LOG_VIDEO,
        "SNAP DISPCNT=%04x BG-enabled=%x stage=%u centred=%u battle=%u lineMask=%x lines=%u",
        Reg(0), (Reg(0) >> 8) & 15, sStageRequested, sCentredRequested,
        sBattleRequested, sLineMask, sLineCount);
    for (unsigned bg = 0; bg < 4; ++bg)
    {
        unsigned cnt = Reg(8 + bg * 2), cb = (cnt >> 2) & 3, sb = (cnt >> 8) & 31;
        CtrLog_Write(CTR_LOG_VIDEO,
            "SNAP BG%uCNT=%04x scroll=%u,%u char=%u screen=%u hash-char=%08lx hash-map=%08lx",
            bg, cnt, Reg(0x10 + bg * 4), Reg(0x12 + bg * 4), cb, sb,
            (unsigned long)SnapshotHash(sMemory.vram + cb * 0x4000, 0x4000),
            (unsigned long)SnapshotHash(sMemory.vram + sb * 0x800, 0x800));
    }
    CtrLog_Write(CTR_LOG_VIDEO,
        "SNAP WIN0H/V=%04x/%04x WIN1H/V=%04x/%04x WININ=%04x WINOUT=%04x",
        Reg(0x40), Reg(0x44), Reg(0x42), Reg(0x46), Reg(0x48), Reg(0x4a));
    CtrLog_Write(CTR_LOG_VIDEO,
        "SNAP BLDCNT=%04x BLDALPHA=%04x BLDY=%04x MOSAIC=%04x OBJ-enabled=%u configured=%u window=%u affine=%u semi=%u mosaic=%u",
        Reg(0x50), Reg(0x52), Reg(0x54), Reg(0x4c), !!(Reg(0) & 0x1000),
        objects, windows, affine, semi, mosaic);
    CtrLog_Write(CTR_LOG_VIDEO,
        "SNAP PAL0=%04x,%04x,%04x,%04x,%04x,%04x,%04x,%04x,%04x,%04x,%04x,%04x,%04x,%04x,%04x,%04x hash=%08lx",
        sMemory.palette[0], sMemory.palette[1], sMemory.palette[2], sMemory.palette[3],
        sMemory.palette[4], sMemory.palette[5], sMemory.palette[6], sMemory.palette[7],
        sMemory.palette[8], sMemory.palette[9], sMemory.palette[10], sMemory.palette[11],
        sMemory.palette[12], sMemory.palette[13], sMemory.palette[14], sMemory.palette[15],
        (unsigned long)SnapshotHash((const uint8_t *)sMemory.palette, 0x400));
}

static void Error(unsigned bit, const char *reason)
{
    if (!(sReported & (1u << bit)))
    {
        sReported |= 1u << bit;
        ++sStats.errors;
        CtrLog_Write(CTR_LOG_ERROR, "VIDEO: %s", reason);
    }
}

static bool BgCharacterAddressValid(unsigned bg, unsigned control, unsigned map,
                                    unsigned chars, unsigned entry, unsigned address)
{
    if (address < 0x10000) return true;
    if (!(sReported & (1u << 4)))
        CtrLog_Write(CTR_LOG_ERROR,
            "VIDEO_BG_ADDRESS bg=%u cnt=%04x char=%05x map=%05x entry=%04x tile=%u palette=%u address=%05x",
            bg, control, chars, map, entry, entry & 1023, entry >> 12, address);
    Error(4, "BG character address exceeds 64 KiB");
    return false;
}

static unsigned Read16(unsigned offset)
{
    if (offset >= 0x10000)
    {
        Error(0, "BG tilemap outside BG VRAM");
        return 0;
    }
    return sMemory.vram[offset] | (sMemory.vram[offset + 1] << 8);
}

static void UpdatePalette(void)
{
    bool bgChanged = false, objChanged = false;
    for (unsigned bank = 0; bank < 32; ++bank)
        if (memcmp(sPalette + bank * 16, sMemory.palette + bank * 16, 32))
        {
            unsigned group = bank < 16 ? 32 : 33;
            if (bank < 16 ? !bgChanged : !objChanged)
                memset(sPaletteChanges[group], 0, sizeof(sPaletteChanges[group]));
            memset(sPaletteChanges[bank], 0, sizeof(sPaletteChanges[bank]));
            for (unsigned index = 0; index < 16; ++index)
            {
                unsigned p = bank * 16 + index;
                if (sPalette[p] == sMemory.palette[p]) continue;
                CTR_PERF_COUNT(PERF_PALETTE_COLORS, 1);
                sPalette[p] = sMemory.palette[p];
                sTexturePalette[p] = CtrVideo_RGBA5551(sPalette[p]);
                sPaletteChanges[bank][0] |= 1u << index;
                unsigned entry = p & 255;
                sPaletteChanges[group][entry / 32] |= 1u << (entry & 31);
            }
            ++sPaletteVersion[bank];
            if (bank < 16) bgChanged = true; else objChanged = true;
        }
    if (bgChanged)
    {
        ++sPaletteVersion[32];
        sBgPaletteStamp = sStats.frames + 1;
    }
    if (objChanged) ++sPaletteVersion[33];
}

/* After UpdatePalette: is the background palette a BlendPalette step of the
 * base? Anything else (a palette animation, a partial fade) becomes the new
 * base, and the layers follow the palette as before. */
static void UpdateFade(void)
{
    unsigned coeff;
    uint16_t target;

    if (sFadeBaseValid && sFadeSeenVersion == sPaletteVersion[32]) return;
    sFadeSeenVersion = sPaletteVersion[32];
    if (sFadeBaseValid && CtrPal_DetectBlend(sFadeBase, sPalette, 256, &coeff, &target))
    {
        sFadeCoeff = coeff;
        sFadeTarget = target;
        return;
    }
    bool any = false;
    for (unsigned bank = 0; bank < 16; ++bank)
    {
        if (sFadeBaseValid && !memcmp(sFadeBase + bank * 16, sPalette + bank * 16, 32)) continue;
        if (!any) memset(sFadeChanges[32], 0, sizeof(sFadeChanges[32]));
        memset(sFadeChanges[bank], 0, sizeof(sFadeChanges[bank]));
        for (unsigned index = 0; index < 16; ++index)
        {
            unsigned entry = bank * 16 + index;
            if (sFadeBaseValid && sFadeBase[entry] == sPalette[entry]) continue;
            sFadeChanges[bank][0] |= 1u << index;
            sFadeChanges[32][entry / 32] |= 1u << (entry & 31);
        }
        ++sFadeVersion[bank];
        any = true;
    }
    if (any) ++sFadeVersion[32];
    memcpy(sFadeBase, sPalette, sizeof(sFadeBase));
    memcpy(sFadeBaseTexture, sTexturePalette, sizeof(sFadeBaseTexture));
    sFadeBaseValid = true;
    sFadeCoeff = 0;
}

static bool TileVersionChanged(const Tile *tile, uint32_t version, const uint32_t *changes, unsigned words)
{
    if (tile->paletteVersion == version) return false;
    /* The last change mask is sufficient only for the next version. Tiles
     * returning after multiple palette changes are conservatively rebuilt. */
    if (tile->paletteVersion + 1 != version) return true;
    for (unsigned i = 0; i < words; ++i)
        if (tile->colors[i] & changes[i]) return true;
    return false;
}

static bool TilePaletteChanged(const Tile *tile, unsigned paletteId)
{
    return TileVersionChanged(tile, sPaletteVersion[paletteId], sPaletteChanges[paletteId],
                              paletteId < 32 ? 1u : 8u);
}

/*
 * Resolves a tile to its atlas slot, uploading it if its bytes or its palette
 * changed. Returns -1 for a tile that is entirely transparent, which draws
 * nothing. The caller builds the subtexture: a slot is a fixed square of the
 * atlas, so that is four multiplications and no shared state.
 */
static uint32_t sTileSerial;

static int GetTileSlotFrom(unsigned address, unsigned bank, bool color256, bool fadeBase);

static int GetTileSlot(unsigned address, unsigned bank, bool color256)
{
    return GetTileSlotFrom(address, bank, color256, false);
}

/* fadeBase: decode in sFadeBase (background tiles only), a separate slot. */
static int GetTileSlotFrom(unsigned address, unsigned bank, bool color256, bool fadeBase)
{
    CTR_PERF_COUNT(PERF_CACHE_REQUEST,1);
    uint64_t perfCache=0;
    if (gCtrPerf.enabled && !((gCtrPerf.counters[PERF_CACHE_REQUEST]+gCtrPerf.frame)&31))
    {
        CTR_PERF_COUNT(PERF_CACHE_SAMPLE_COUNT,1);
        perfCache=CtrPerf_Begin();
    }
    if (address + (color256 ? 64 : 32) > 0x18000)
    {
        Error(1, "tile outside logical VRAM");
        address = 0;
    }
    unsigned paletteId = color256 ? 32 + (bank >= 16) : bank;
    uint32_t key = ((address / 32) * 34 + paletteId + 1) | (fadeBase ? 0x80000000u : 0);
    unsigned hash = (key * 2654435761u) & (HASH_COUNT - 1);
    while (sHash[hash] && sTiles[sHash[hash] - 1].key != key)
        hash = (hash + 1) & (HASH_COUNT - 1);
    if (!sHash[hash])
    {
        CTR_PERF_COUNT(PERF_CACHE_MISS,1);
        if (sUsed == CACHE_COUNT)
            CtrPlatform_Fatal("VIDEO tile cache exhausted in a single frame");
        sHash[hash] = ++sUsed;
        sTiles[sUsed - 1] = (Tile){.key = key};
    }
    else { CTR_PERF_COUNT(PERF_CACHE_HIT,1); }
    unsigned slot = sHash[hash] - 1;
    Tile *tile = &sTiles[slot];
    if (!tile->valid || tile->checked != sStats.frames + 1)
    {
        unsigned bytes = color256 ? 64 : 32;
        unsigned fadeId = paletteId < 32 ? paletteId : 32;
        bool paletteDirty=tile->valid && (fadeBase
            ? TileVersionChanged(tile, sFadeVersion[fadeId], sFadeChanges[fadeId], fadeId < 32 ? 1u : 8u)
            : TilePaletteChanged(tile,paletteId));
        bool vramDirty=tile->valid && !paletteDirty
            && CtrVramTrack_MayDiffer(sVramStamp,address,tile->checked)
            && memcmp(tile->bytes,sMemory.vram+address,bytes);
        if (!tile->valid || paletteDirty || vramDirty)
        {
            uint64_t perfDecode=perfCache ? CtrPerf_Begin() : 0;
            CTR_PERF_COUNT(PERF_REGENERATE,1);
            CTR_PERF_COUNT(PERF_PALETTE_INVALIDATE,paletteDirty);
            CTR_PERF_COUNT(PERF_VRAM_INVALIDATE,vramDirty);
            CTR_PERF_COUNT(PERF_ATLAS_BYTES,128);
            CTR_PERF_COUNT(PERF_VRAM_READ_BYTES,bytes);
            if (perfDecode) CTR_PERF_COUNT(PERF_DECODE_SAMPLE_COUNT,1);
            memcpy(tile->bytes, sMemory.vram + address, bytes);
            unsigned paletteBase = color256 ? (bank >= 16 ? 256 : 0) : bank * 16;
            uint16_t *dest = (uint16_t *)sAtlas.data + slot * 64;
            const uint16_t *palette = fadeBase && paletteBase < 256 ? sFadeBaseTexture : sTexturePalette;
            memset(tile->colors, 0, sizeof(tile->colors));
            tile->visible = false;
            for (unsigned pixel = 0; pixel < 64; ++pixel)
            {
                unsigned index = color256 ? tile->bytes[pixel]
                    : (tile->bytes[pixel / 2] >> ((pixel & 1) * 4)) & 15;
                dest[sMorton[pixel]] = index ? palette[paletteBase + index] : 0;
                if (index)
                {
                    tile->visible = true;
                    tile->colors[index / 32] |= 1u << (index & 31);
                }
            }
            /* C3D_FrameEnd(0) flushes linear memory once before submitting
             * the queue, including this atlas and Citro2D's geometry. A
             * separate GSP service call per tile costs hundreds of calls
             * per palette-animation frame without improving visibility. */
            tile->valid = true;
            tile->serial = ++sTileSerial;
            ++sStats.uploads;
            CtrPerf_End(PERF_DECODE_SAMPLE,perfDecode);
        }
        tile->paletteVersion = fadeBase ? sFadeVersion[paletteId < 32 ? paletteId : 32] : sPaletteVersion[paletteId];
        tile->checked = sStats.frames + 1;
    }
    CtrPerf_End(PERF_CACHE_SAMPLE,perfCache);
    return tile->visible ? (int)slot : -1;
}

static void DrawSlotTinted(int slot, float x, float y, bool flipX, bool flipY, const C2D_ImageTint *tint)
{
    unsigned tileX = ((unsigned)slot % (ATLAS_SIZE / 8)) * 8;
    unsigned tileY = ((unsigned)slot / (ATLAS_SIZE / 8)) * 8;
    Tex3DS_SubTexture sub = {8, 8, tileX / (float)ATLAS_SIZE,
        1.0f - tileY / (float)ATLAS_SIZE,
        (tileX + 8) / (float)ATLAS_SIZE, 1.0f - (tileY + 8) / (float)ATLAS_SIZE};

    if (++sStats.tiles >= MAX_DRAWS - 32)
    {
        Error(2, "GPU geometry budget exceeded");
        return;
    }
    if (!C2D_DrawImageAt((C2D_Image){&sAtlas, &sub}, x, y, 0, tint,
                         flipX ? -1 : 1, flipY ? -1 : 1))
        Error(3, "Citro2D geometry submission failed");
}

static void DrawSlot(int slot, float x, float y, bool flipX, bool flipY)
{
    DrawSlotTinted(slot, x, y, flipX, flipY, &sTint);
}

static void Blend(unsigned layer, bool effects, bool semiTransparent)
{
    /*
     * This flushes the batch and rewrites the blend state, so it costs a GPU
     * draw call. The registers it reads are fixed for the frame, and nothing
     * between two calls touches the blend state, so asking for the same
     * configuration again is a no-op: an overworld frame with forty sprites
     * asked for it forty times.
     */
    unsigned brightness = sBrightnessOverride ? sBrightnessValue : Reg(0x54);
    unsigned key = layer | (effects << 8) | (semiTransparent << 9)
                 | ((brightness & 31) << 10);

    if (key == sBlendKey) return;
    sBlendKey = key;
    uint64_t perfStart=CtrPerf_Begin();

    C2D_Flush();
    C3D_AlphaTest(true, GPU_GREATER, 0);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA,
                  GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ZERO);
    unsigned control = Reg(0x50), effect = (control >> 6) & 3;
    C2D_PlainImageTint(&sTint, C2D_Color32(255, 255, 255, 255), 0);
    if (!effects) { CtrPerf_End(PERF_BLEND,perfStart); return; }
    if ((effect == 1 && (control & (1u << layer))) || semiTransparent)
    {
        /* GPU implements independently clamped EVA/EVB (not 1-EVA).
         * Valid for the title's BG1 over BG0/backdrop; arbitrary interleaved
         * target-2 masks are a phase-6 per-pixel effect, reported below. */
        if ((control >> 8) & 63)
        {
            unsigned eva = Min(Reg(0x52) & 31, 16) * 255 / 16;
            unsigned evb = Min((Reg(0x52) >> 8) & 31, 16) * 255 / 16;
            C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_CONSTANT_COLOR,
                          GPU_CONSTANT_ALPHA, GPU_ONE, GPU_ZERO);
            C3D_BlendingColor(C2D_Color32(eva, eva, eva, evb));
        }
    }
    else if ((control & (1u << layer)) && effect >= 2)
    {
        unsigned c = effect == 2 ? 255 : 0;
        C2D_PlainImageTint(&sTint, C2D_Color32(c, c, c, 255), Min(brightness, 16) / 16.0f);
    }
    CtrPerf_End(PERF_BLEND,perfStart);
}

__attribute__((weak)) unsigned CtrIntro_ReadActors(CtrIntroActor *actors, unsigned capacity)
{ (void)actors; (void)capacity; return 0; }
__attribute__((weak)) void CtrIntro_ReadScene1Pose(CtrIntroScene1Pose *pose)
{ memset(pose, 0, sizeof(*pose)); }
__attribute__((weak)) void CtrIntro_ReadScene2Pose(CtrIntroScene2Pose *pose)
{ memset(pose, 0, sizeof(*pose)); }
__attribute__((weak)) void CtrIntro_ReadScene3Pose(CtrIntroScene3Pose *pose)
{ memset(pose, 0, sizeof(*pose)); }
__attribute__((weak)) int CtrIntro_ReadTitleFooterX(void) { return 0; }

#include "3ds_intro_fallback.h"

static void DrawTile(unsigned address, unsigned bank, bool color256,
                     float x, float y, bool flipX, bool flipY)
{
    int slot = GetTileSlot(address, bank, color256);
    if (slot >= 0) DrawSlot(slot, x, y, flipX, flipY);
}

/*
 * Walks the tiles of a text background that cover [left, right) x [top, bottom)
 * of the screen. With a mirror axis (given doubled, -1 for none) each tile is
 * drawn reflected about it instead, flipped, so the region it lands on becomes
 * the mirror image of the one walked.
 */
/* Extra horizontal displacement of what DrawTextSpanAt draws, for a band that
 * is moved as a whole (the battle text box, DrawBattleText). */
static int sSpanShiftX;

/* The base transform of everything composed: the current zoom. */
static void ViewBase(void)
{
    C2D_ViewReset();
    if (sZoom != 1.0f)
    {
        C2D_ViewTranslate(sOffX, sOffY);
        C2D_ViewScale(sZoom, sZoom);
    }
}

/* An affine layer's or sprite's own matrix, on top of the zoom. */
static void ViewAffine(C3D_Mtx *matrix)
{
    for (unsigned r = 0; r < 2; ++r)
        matrix->r[r] = FVec4_Scale(matrix->r[r], sZoom);
    matrix->r[0].w += sOffX;
    matrix->r[1].w += sOffY;
    C2D_ViewRestore(matrix);
}

static void DrawTextSpanAt(unsigned bg, int left, int right, int top, int bottom,
                           int mirrorX2, int mirrorY2)
{
    unsigned control = Reg(8 + bg * 2), size = control >> 14;
    unsigned map = ((control >> 8) & 31) * 0x800;
    unsigned chars = ((control >> 2) & 3) * 0x4000;
    bool color256 = (control & 128) != 0;
    unsigned scrollX = Reg(0x10 + bg * 4) & 511, scrollY = Reg(0x12 + bg * 4) & 511;

    /*
     * A tilemap entry repeats across the layer: the field overlays are mostly
     * one blank entry, and a viewport of 400x240 walks 1500 of them per layer
     * per frame. This memo turns a repeat into one comparison instead of a
     * hash probe and a revalidation, which is what a 60fps overworld needs on
     * an Old 3DS. The stamp makes it valid for this call only, so every entry
     * is still revalidated once per layer per frame.
     */
    static uint32_t sMemoStamp[1024];
    static uint16_t sMemoEntry[1024];
    static int16_t sMemoSlot[1024];
    static uint32_t sStamp;
    uint32_t stamp = ++sStamp;

    ViewBase();
    for (int y = top / 8 - 1; y * 8 <= bottom; ++y)
    {
        int py = y * 8 - (int)(scrollY & 7);
        unsigned row = (unsigned)(y + (int)(scrollY / 8));
        unsigned rowBase;

        if (py + 8 <= top || py >= bottom) continue;
        rowBase = map + CtrVideo_TextMapOffset(0, row, size);
        for (int x = left / 8 - 1; x * 8 <= right; ++x)
        {
            int px = x * 8 - (int)(scrollX & 7);
            unsigned column = (unsigned)(x + (int)(scrollX / 8)) & ((size & 1) ? 63u : 31u);
            unsigned entry, index;
            int slot;

            /* Inside the GBA area the wrap-around is kept exactly as on
             * hardware; only the margins are denied to a narrow background. */
            if (px + 8 <= left || px >= right) continue;
            /* Columns past 31 live in the next screenblock, 1024 entries on. */
            entry = Read16(rowBase + (column & 31) * 2 + (column >> 5) * 2048);
            index = entry & 1023;
            if (sMemoStamp[index] == stamp && sMemoEntry[index] == entry)
            {
                slot = sMemoSlot[index];
            }
            else
            {
                unsigned address = chars + index * (color256 ? 64 : 32);
                if (!BgCharacterAddressValid(bg, control, map, chars, entry, address)) continue;
                slot = GetTileSlot(address, entry >> 12, color256);
                sMemoStamp[index] = stamp;
                sMemoEntry[index] = (uint16_t)entry;
                sMemoSlot[index] = (int16_t)slot;
            }
            if (slot < 0) continue;
            {
                bool flipX = (entry & 1024) != 0, flipY = (entry & 2048) != 0;
                int dx = px, dy = py;

                if (mirrorX2 >= 0) { dx = mirrorX2 - px - 8; flipX = !flipX; }
                if (mirrorY2 >= 0) { dy = mirrorY2 - py - 8; flipY = !flipY; }
                DrawSlot(slot, dx + sSpanShiftX + CTR_VIEW_X + sLayerShift, dy + CTR_VIEW_Y, flipX, flipY);
            }
        }
    }
}

static void DrawTextSpan(unsigned bg, int left, int right, int top, int bottom)
{
    DrawTextSpanAt(bg, left, right, top, bottom, -1, -1);
}

/*
 * The scissor the window partition set for the rectangle being composed, so a
 * pass that narrows it can put it back.
 */
static bool sScissored;

static void Scissor(int x0, int y0, int x1, int y1)
{
    float sx0 = (x0 + CTR_VIEW_X) * sZoom + sOffX, sx1 = (x1 + CTR_VIEW_X) * sZoom + sOffX;
    float sy0 = (y0 + CTR_VIEW_Y) * sZoom + sOffY, sy1 = (y1 + CTR_VIEW_Y) * sZoom + sOffY;

    if (sx0 < 0) sx0 = 0;
    if (sy0 < 0) sy0 = 0;
    C3D_SetScissor(GPU_SCISSOR_NORMAL, (unsigned)roundf(sx0), (unsigned)(sSurfaceH - roundf(sy1)),
                   (unsigned)roundf(sx1), (unsigned)(sSurfaceH - roundf(sy0)));
}

static void RestoreScissor(void)
{
    if (sScissored) Scissor(sClipX0, sClipY0, sClipX1, sClipY1);
    else C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
}

/*
 * How a stage fills the screen around its 240x160 picture. A GBA background
 * is one picture drawn for 240x160, so outside that area there is nothing the
 * game drew for it: the port fills it from the picture itself.
 *
 * - Across a background that scrolls sideways - the scenery of the bike ride,
 *   drifting clouds - the tilemap is continuous art made to wrap around, so
 *   the margins show the wrap-around, exactly as the GBA brings those columns
 *   onto its own screen a moment later.
 * - A still picture - the leaves of the Game Freak logo, the legendaries, the
 *   title screen - is carried outwards from its own edge tiles and faded into
 *   black (DrawEdgeRegion), like the iris of a film frame. Mirroring was
 *   tried and rejected: it shows every character on an edge twice.
 *
 * Whether a background scrolls is decided from the last STILL_FRAMES frames
 * and held for SCROLL_HOLD_FRAMES after it stops, so a scene does not switch
 * filling halfway through. A new control register starts the judgement over.
 */
#define STILL_FRAMES 32
#define STILL_SPAN 4
static uint16_t sScrollHistory[4][STILL_FRAMES];
static uint32_t sSceneKey[4];
static bool sScrolls[4];
/* The last frame each layer was seen moving. */
static uint32_t sMovedFrame[4];
/*
 * How long a layer that moved keeps being treated as scrolling: long enough
 * that a scene pausing its scenery for a moment does not switch filling, short
 * enough that the next scene on the same registers starts from what it does.
 */
#define SCROLL_HOLD_FRAMES 120
/* When each still background was last measured (MeasureStill). */
static uint32_t sPictureFrame[4];

static void RecordScroll(void)
{
    for (unsigned bg = 0; bg < 4; ++bg)
    {
        uint32_t key = Reg(8 + bg * 2) | ((Reg(0) & 7) << 16) | ((Reg(0) >> (8 + bg) & 1) << 20);
        /* A horizontal scroll driven line by line is a wave, not a pan: the
         * register only holds the first line of it. */
        unsigned scroll = (sLineMask & (1u << (bg * 2))) ? 0 : Reg(0x10 + bg * 4) & 511;
        int low = 0, high = 0;

        if (key != sSceneKey[bg])
        {
            sSceneKey[bg] = key;
            sScrolls[bg] = false;
            sPictureFrame[bg] = 0;
            for (unsigned i = 0; i < STILL_FRAMES; ++i) sScrollHistory[bg][i] = scroll;
        }
        sScrollHistory[bg][sStats.frames % STILL_FRAMES] = scroll;
        for (unsigned i = 0; i < STILL_FRAMES; ++i)
        {
            /* Unwrapped against the current value, so 511 -> 0 is one step. */
            int delta = (((int)sScrollHistory[bg][i] - (int)scroll + 256) & 511) - 256;

            if (delta < low) low = delta;
            if (delta > high) high = delta;
        }
        /* A shake of a few pixels is not scrolling; a pan is. */
        if (high - low > STILL_SPAN)
        {
            sScrolls[bg] = true;
            sMovedFrame[bg] = sStats.frames;
        }
        else if (sScrolls[bg] && sStats.frames - sMovedFrame[bg] > SCROLL_HOLD_FRAMES)
        {
            sScrolls[bg] = false;
        }
    }
}

/* Clamps a cell to the clip rectangle; false when nothing of it is left. */
static bool ClipCell(int *x0, int *x1, int *y0, int *y1)
{
    if (*x0 < sClipX0) *x0 = sClipX0;
    if (*x1 > sClipX1) *x1 = sClipX1;
    if (*y0 < sClipY0) *y0 = sClipY0;
    if (*y1 > sClipY1) *y1 = sClipY1;
    return *x0 < *x1 && *y0 < *y1;
}

/*
 * The rows a background's picture really covers. The intro's cinematic scenes
 * are drawn only between their letterbox bars - the clouds and Rayquaza have
 * nothing above line 32 or below 128 - so when window 0 is a full-width band
 * that hides this layer outside it, the band is the picture, and the space
 * the stage opens above and below it is filled from the band's own edges.
 */
static void PictureRows(unsigned bg, int *top, int *bottom)
{
    unsigned display = Reg(0), across = Reg(0x40), rows = Reg(0x44);

    *top = 0;
    *bottom = 160;
    if (!(display & 0x2000) || (Reg(0x4a) & (1u << bg)) || !(Reg(0x48) & (1u << bg))) return;
    if ((across >> 8) != 0 || ((across & 255) < 240 && (across & 255) != 0)) return;
    if ((rows >> 8) >= (rows & 255) || (rows & 255) > 160) return;
    *top = (int)(rows >> 8);
    *bottom = (int)(rows & 255);
}

/*
 * One cell of the 3x3 partition of a stage around its picture: the part of
 * the current clip rectangle inside [x0, x1) x [y0, y1), drawn from the source
 * the cell calls for. A mirror axis is given doubled; -1 means drawn directly.
 * A mirrored cell only ever takes from the picture [0, 240) x [top, bottom).
 */
static void DrawStageCell(unsigned bg, int x0, int x1, int y0, int y1,
                          int mirrorX2, int mirrorY2, int top, int bottom)
{
    int sx0, sx1, sy0, sy1;

    if (!ClipCell(&x0, &x1, &y0, &y1)) return;
    sx0 = mirrorX2 < 0 ? x0 : mirrorX2 - x1;
    sx1 = mirrorX2 < 0 ? x1 : mirrorX2 - x0;
    sy0 = mirrorY2 < 0 ? y0 : mirrorY2 - y1;
    sy1 = mirrorY2 < 0 ? y1 : mirrorY2 - y0;
    if (mirrorX2 >= 0) { if (sx0 < 0) sx0 = 0; if (sx1 > 240) sx1 = 240; }
    if (mirrorY2 >= 0) { if (sy0 < top) sy0 = top; if (sy1 > bottom) sy1 = bottom; }
    if (sx0 >= sx1 || sy0 >= sy1) return;
    /* Whole tiles overhang a cell; the scissor keeps each cell to itself. */
    C2D_Flush();
    Scissor(x0, y0, x1, y1);
    DrawTextSpanAt(bg, sx0, sx1, sy0, sy1, mirrorX2, mirrorY2);
    C2D_Flush();
    RestoreScissor();
}

/*
 * Whether a still layer is a panorama: art in the columns the GBA screen never
 * reaches (30 and 31 of a 32-column map), as dense as just inside the edge,
 * means a picture made to wrap - the sky of the bike ride, which holds still
 * while the scenery in front of it scrolls. Measured every few frames, which
 * is plenty for art that does not move.
 */
static bool sPanorama[4];

static void MeasureStill(unsigned bg)
{
    unsigned control = Reg(8 + bg * 2), size = control >> 14;
    unsigned map = ((control >> 8) & 31) * 0x800;
    unsigned chars = ((control >> 2) & 3) * 0x4000;
    bool color256 = (control & 128) != 0;
    unsigned scrollX = Reg(0x10 + bg * 4) & 511, scrollY = Reg(0x12 + bg * 4) & 511;
    unsigned beyond = 0, inside = 0;

    if (sPictureFrame[bg] && sStats.frames - sPictureFrame[bg] < 8) return;
    sPictureFrame[bg] = sStats.frames | 1;
    for (unsigned y = 0; y < 20 && !(size & 1); ++y)
    {
        unsigned rowBase = map + CtrVideo_TextMapOffset(0, y + scrollY / 8, size);

        for (unsigned x = 28; x < 32; ++x)
        {
            unsigned entry = Read16(rowBase + ((x + scrollX / 8) & 31) * 2);
            unsigned address = chars + (entry & 1023) * (color256 ? 64 : 32);

            if (address < 0x10000 && GetTileSlot(address, entry >> 12, color256) >= 0)
                ++*(x < 30 ? &inside : &beyond);
        }
    }
    sPanorama[bg] = beyond && beyond * 4 >= inside * 3;
}

/*
 * The margins of a still scene: the picture's own edge carried outwards and
 * faded into black, like the iris of a film frame. Each margin tile repeats
 * the tile at the edge of the picture in its row (or column, or corner), and
 * its four corners are tinted towards black by their distance from the
 * picture, so the fade is a smooth gradient, not steps of eight pixels.
 * Nothing is mirrored or duplicated, so a character on the edge of the art is
 * never seen twice.
 */
#define FADE_ACROSS 56.0f
#define FADE_DOWN 28.0f

static float EdgeFade(int x, int y, int top, int bottom)
{
    float across = x < 0 ? -x / FADE_ACROSS : x > 240 ? (x - 240) / FADE_ACROSS : 0.0f;
    float down = y < top ? (top - y) / FADE_DOWN : y > bottom ? (y - bottom) / FADE_DOWN : 0.0f;
    float fade = across > down ? across : down;

    return fade > 1.0f ? 1.0f : fade;
}

/*
 * The zones past the fade are black whatever the still layers hold there, so
 * those layers skip them and the frame starts with them black instead: a
 * rectangle each rather than a column of fully faded tiles per layer. Layers
 * that wrap and sprites still draw over them.
 */
static bool sUnderlaid;

static void StageUnderlay(void)
{
    unsigned display = Reg(0), mode = display & 7;
    bool still = false;
    u32 black = C2D_Color32(0, 0, 0, 255);

    sUnderlaid = false;
    /* Under windows the margins may be the backdrop on purpose (the
     * letterbox, which flashes with it), so nothing is assumed there. */
    if (!sStage || sStageUi || (display & 128) || (display & 0x6000)) return;
    for (unsigned bg = 0; bg < 4; ++bg)
        if ((display & (0x100u << bg)) && !sScrolls[bg] && mode != 2 && !(mode == 1 && bg == 2))
            still = true;
    if (!still) return;
    ViewBase();
    C2D_DrawRectSolid(0, 0, 0, CTR_VIEW_X - FADE_ACROSS, CTR_GAME_HEIGHT, black);
    C2D_DrawRectSolid(CTR_VIEW_X + 240 + FADE_ACROSS, 0, 0, CTR_VIEW_X - FADE_ACROSS, CTR_GAME_HEIGHT, black);
    C2D_DrawRectSolid(0, 0, 0, CTR_GAME_WIDTH, CTR_VIEW_Y - FADE_DOWN, black);
    C2D_DrawRectSolid(0, CTR_VIEW_Y + 160 + FADE_DOWN, 0, CTR_GAME_WIDTH, CTR_VIEW_Y - FADE_DOWN, black);
    sUnderlaid = true;
}

/*
 * The GBA brightness effect a layer is under (BLDCNT effect 2 or 3), as the
 * colour it moves towards and how far; 0 when there is none.
 */
static float LayerBrightness(unsigned layer, bool *white)
{
    unsigned control = Reg(0x50), effect = (control >> 6) & 3;

    *white = effect == 2;
    if (!(control & (1u << layer)) || effect < 2) return 0.0f;
    return Min(Reg(0x54) & 31, 16) / 16.0f;
}

/*
 * One corner of a fading tile. The fade to black f and the layer's own
 * brightness effect k are one tint: darkening gives c(1-k)(1-f), which is a
 * blend of 1-(1-k)(1-f) towards black; brightening gives
 * (c(1-k) + k)(1-f), a blend of the same amount towards a grey of
 * k(1-f) / blend. Either way a fully faded corner is black.
 */
static void FadeCorner(C2D_ImageTint *tint, C2D_Corner corner, float f, float k, bool white)
{
    float blend = 1.0f - (1.0f - k) * (1.0f - f);
    float grey = white && blend > 0.0f ? k * (1.0f - f) / blend : 0.0f;
    unsigned level = (unsigned)(grey * 255.0f + 0.5f);

    C2D_SetImageTint(tint, corner, C2D_Color32(level, level, level, 255), blend);
}

static void DrawEdgeRegion(unsigned bg, int x0, int x1, int y0, int y1, int top, int bottom)
{
    unsigned control = Reg(8 + bg * 2), size = control >> 14;
    unsigned map = ((control >> 8) & 31) * 0x800;
    unsigned chars = ((control >> 2) & 3) * 0x4000;
    bool color256 = (control & 128) != 0;
    unsigned scrollX = Reg(0x10 + bg * 4) & 511, scrollY = Reg(0x12 + bg * 4) & 511;
    int ox = (int)(scrollX & 7), oy = (int)(scrollY & 7);
    /* The grid positions of the tiles on the picture's edges. */
    int firstX = -ox, lastX = ((239 + ox) & ~7) - ox;
    int firstY = ((top + oy) & ~7) - oy, lastY = ((bottom - 1 + oy) & ~7) - oy;
    bool white;
    float bright = LayerBrightness(bg, &white);
    /* A margin row repeats one edge tile, so the last lookup usually answers. */
    unsigned lastEntry = ~0u;
    int lastSlot = -1;

    if (!ClipCell(&x0, &x1, &y0, &y1)) return;
    C2D_Flush();
    Scissor(x0, y0, x1, y1);
    ViewBase();
    for (int py = ((y0 + oy) & ~7) - oy - 8; py < y1; py += 8)
    {
        int sy = py < firstY ? firstY : py > lastY ? lastY : py;
        unsigned rowBase = map + CtrVideo_TextMapOffset(0, (unsigned)(sy + (int)scrollY) >> 3, size);

        if (py + 8 <= y0) continue;
        for (int px = ((x0 + ox) & ~7) - ox - 8; px < x1; px += 8)
        {
            int sx = px < firstX ? firstX : px > lastX ? lastX : px;
            unsigned column = ((unsigned)(sx + (int)scrollX) >> 3) & ((size & 1) ? 63u : 31u);
            unsigned entry = Read16(rowBase + (column & 31) * 2 + (column >> 5) * 2048);
            unsigned address = chars + (entry & 1023) * (color256 ? 64 : 32);
            int slot;

            if (px + 8 <= x0 || address >= 0x10000) continue;
            if (entry != lastEntry)
            {
                lastEntry = entry;
                lastSlot = GetTileSlot(address, entry >> 12, color256);
            }
            slot = lastSlot;
            if (slot < 0) continue;
            /* Past the fade the tile would be black: StageUnderlay drew that. */
            if (sUnderlaid && EdgeFade(px, py, top, bottom) >= 1.0f && EdgeFade(px + 8, py, top, bottom) >= 1.0f
                && EdgeFade(px, py + 8, top, bottom) >= 1.0f && EdgeFade(px + 8, py + 8, top, bottom) >= 1.0f)
                continue;
            {
                C2D_ImageTint fade;

                FadeCorner(&fade, C2D_TopLeft, EdgeFade(px, py, top, bottom), bright, white);
                FadeCorner(&fade, C2D_TopRight, EdgeFade(px + 8, py, top, bottom), bright, white);
                FadeCorner(&fade, C2D_BotLeft, EdgeFade(px, py + 8, top, bottom), bright, white);
                FadeCorner(&fade, C2D_BotRight, EdgeFade(px + 8, py + 8, top, bottom), bright, white);
                DrawSlotTinted(slot, px + CTR_VIEW_X + sLayerShift, py + CTR_VIEW_Y,
                               entry & 1024, entry & 2048, &fade);
            }
        }
    }
    C2D_Flush();
    RestoreScissor();
}

static void DrawStageBg(unsigned bg)
{
    int top, bottom;
    bool wraps = sScrolls[bg];

    PictureRows(bg, &top, &bottom);
    if (!wraps)
    {
        MeasureStill(bg);
        /* A still panorama wraps too when the scene around it scrolls. */
        if (sPanorama[bg])
            for (unsigned other = 0; other < 4; ++other)
                if (other != bg && sScrolls[other] && (Reg(0) & (0x100u << other))) wraps = true;
    }
    if (wraps)
    {
        /*
         * A layer that scrolls - the bike ride - is continuous art made to
         * wrap around: across, one span wrapping as on the GBA; above and
         * below, the mirror image of the picture's own edge rows.
         */
        const int rows[3][3] = {{-512, top, 2 * top}, {top, bottom, -1}, {bottom, 512, 2 * bottom}};

        for (unsigned r = 0; r < 3; ++r)
            DrawStageCell(bg, VIEW_LEFT, VIEW_RIGHT, rows[r][0], rows[r][1], -1, rows[r][2],
                          top, bottom);
        return;
    }
    DrawStageCell(bg, 0, 240, top, bottom, -1, -1, top, bottom);
    DrawEdgeRegion(bg, -512, 512, -512, top, top, bottom);
    DrawEdgeRegion(bg, -512, 512, bottom, 512, top, bottom);
    DrawEdgeRegion(bg, -512, 0, top, bottom, top, bottom);
    DrawEdgeRegion(bg, 240, 512, top, bottom, top, bottom);
}

/*
 * The battle scene. Its picture is 240x160 like any GBA screen, but it is
 * made of a scene that the GBA itself carries past the screen - the terrain is
 * a 512-pixel map whose sky continues on both sides, the entry grass and the
 * move backgrounds are tiles that repeat - and a text box that is one frame
 * of caps and a middle. So nothing is faded or mirrored here:
 *
 * - The text box (the last six tile rows of BG0) is moved to the left edge of
 *   the screen and its middle repeated up to a right cap on the right edge.
 *   Everything in it - the message, the prompt arrow, a menu page - keeps its
 *   place from the left cap, as it does on the GBA.
 * - A layer that wraps on its own - a 64-column map, or a 32-column one whose
 *   hidden columns hold art as dense as the visible ones (MeasureStill) - is
 *   drawn as the GBA wraps it across the sides. Above the picture, each tile
 *   row is taken where the map wraps it; where that row is empty, from the
 *   same row of a 32-row picture repeated in a 64-row map; and on the terrain,
 *   from the picture's own top row, which is what its map repeats above it
 *   (rows 30 and 31 of every battle terrain are its row 0).
 * - Anything else - windows, a battler copied into a background - stays in
 *   the 240x160 picture, where the game placed it.
 */
#define BATTLE_BAND_TOP 112
#define BATTLE_BAND_CAP 16

static unsigned MapEntry(unsigned map, unsigned size, unsigned column, unsigned row)
{
    column &= (size & 1) ? 63u : 31u;
    return Read16(map + CtrVideo_TextMapOffset(0, row, size) + (column & 31) * 2 + ((column >> 5) & 1) * 2048);
}

static int EntrySlot(unsigned chars, unsigned entry, bool color256)
{
    unsigned address = chars + (entry & 1023) * (color256 ? 64 : 32);

    if (address >= 0x10000) return -1;
    return GetTileSlot(address, entry >> 12, color256);
}

/* Part of the picture's own tiles drawn moved by shift, kept to [x0, x1) x [y0, y1). */
static void DrawBattleCut(unsigned bg, int x0, int x1, int y0, int y1, int shift)
{
    if (!ClipCell(&x0, &x1, &y0, &y1)) return;
    C2D_Flush();
    Scissor(x0, y0, x1, y1);
    sSpanShiftX = shift;
    DrawTextSpanAt(bg, x0 - shift, x1 - shift, y0, y1, -1, -1);
    sSpanShiftX = 0;
    C2D_Flush();
    RestoreScissor();
}

/*
 * The tile that fills a row of the text box between its caps, from the row's
 * left cap tile (column 0 of the box, which no window covers). FireRed puts the
 * message window on columns 1-28, so the tiles beside the cap are window tiles
 * holding text pixels; repeating one of them across the widened box smeared
 * fragments of the message to the right of the text. The frame's own tiles are
 * numbered in graphics/battle_interface/textbox.bin: the rows of the message
 * box (caps 3, 8, 13) have two cap tiles and then the middle two on, the rows of
 * the menu boxes (caps 18, 21, 24) one cap and the middle next. Others: 0.
 */
static unsigned BattleBoxMiddle(unsigned cap)
{
    unsigned tile = cap & 1023;
    unsigned step = tile == 3 || tile == 8 || tile == 13 ? 2 : tile == 18 || tile == 21 || tile == 24 ? 1 : 0;

    return step ? (cap & ~1023u) | (tile + step) : 0;
}

static void DrawBattleText(unsigned bg)
{
    unsigned control = Reg(8 + bg * 2), size = control >> 14;
    unsigned map = ((control >> 8) & 31) * 0x800;
    unsigned chars = ((control >> 2) & 3) * 0x4000;
    bool color256 = (control & 128) != 0;
    unsigned scrollX = Reg(0x10 + bg * 4) & 511, scrollY = Reg(0x12 + bg * 4) & 511;
    unsigned rows = (size & 2) ? 64 : 32;
    /* The column under the left edge of the picture: the left cap. */
    unsigned first = scrollX >> 3;
    int ox = (int)(scrollX & 7), oy = (int)(scrollY & 7);
    int fill0 = 240 - BATTLE_BAND_CAP + VIEW_LEFT, fill1 = VIEW_RIGHT - BATTLE_BAND_CAP;
    int x0 = fill0, x1 = fill1, y0 = BATTLE_BAND_TOP, y1 = 160;

    /* Above the text box: windows, where the game put them. */
    DrawBattleCut(bg, 0, 240, 0, BATTLE_BAND_TOP, 0);
    /* The box from its left cap, on the left edge of the screen... */
    DrawBattleCut(bg, VIEW_LEFT, fill0, BATTLE_BAND_TOP, 160, VIEW_LEFT);
    /* ...its right cap on the right edge... */
    DrawBattleCut(bg, fill1, VIEW_RIGHT, BATTLE_BAND_TOP, 160, VIEW_RIGHT - 240);
    /*
     * ...and its middle in between: the tile that fills the row between its
     * caps (BattleBoxMiddle), taken from the left cap since the windows cover
     * the tiles beside it.
     */
    if (!ClipCell(&x0, &x1, &y0, &y1)) return;
    C2D_Flush();
    Scissor(x0, y0, x1, y1);
    ViewBase();
    for (int py = ((y0 + oy) & ~7) - oy; py < y1; py += 8)
    {
        unsigned row = ((unsigned)(py + (int)scrollY) >> 3) & (rows - 1);
        unsigned cap = MapEntry(map, size, first, row), inner = MapEntry(map, size, first + 1, row);
        unsigned middle, next = MapEntry(map, size, first + 2, row);
        int slot;

        if (!cap && !inner) continue;
        middle = BattleBoxMiddle(cap);
        if (!middle)
            middle = next == inner ? inner : (inner & ~1023u) | ((inner + 1) & 1023);
        slot = EntrySlot(chars, middle, color256);
        if (slot < 0) continue;
        for (int px = ((x0 + ox) & ~7) - ox; px < x1; px += 8)
            DrawSlot(slot, px + CTR_VIEW_X + sLayerShift, py + CTR_VIEW_Y, middle & 1024, middle & 2048);
    }
    C2D_Flush();
    RestoreScissor();
}

/*
 * The text box is not magnified: it keeps its own pixels on the bottom 48
 * lines, under a scene composed at CTR_BATTLE_ZOOM. The rectangle being
 * composed is carried from the scene's coordinates to the box's through the
 * screen, drawn there, and put back.
 */
static void DrawBattleTextLayer(unsigned bg)
{
    int viewX = sViewX, viewY = sViewY;
    float zoom = sZoom, offX = sOffX, offY = sOffY;
    int clipX0 = sClipX0, clipY0 = sClipY0, clipX1 = sClipX1, clipY1 = sClipY1;

    float shift = sLayerShift;

    sZoom = 1.0f;
    sOffX = sOffY = 0.0f;
    sLayerShift = shift * sShiftZoom;
    sViewX = CTR_BATTLE_X;
    sViewY = CTR_BATTLE_Y;
    sClipX0 = (int)roundf((clipX0 + viewX) * zoom + offX) - sViewX;
    sClipX1 = (int)roundf((clipX1 + viewX) * zoom + offX) - sViewX;
    sClipY0 = (int)roundf((clipY0 + viewY) * zoom + offY) - sViewY;
    sClipY1 = (int)roundf((clipY1 + viewY) * zoom + offY) - sViewY;
    DrawBattleText(bg);
    C2D_Flush();
    sZoom = zoom;
    sOffX = offX;
    sOffY = offY;
    sLayerShift = shift;
    sViewX = viewX;
    sViewY = viewY;
    sClipX0 = clipX0;
    sClipY0 = clipY0;
    sClipX1 = clipX1;
    sClipY1 = clipY1;
    RestoreScissor();
    ViewBase();
}

/* Whether a tile row of a background has anything to show across the screen. */
static bool BattleRowShown(unsigned map, unsigned size, unsigned chars, bool color256,
                           unsigned row, unsigned column, unsigned count)
{
    for (unsigned i = 0; i < count; ++i)
        if (EntrySlot(chars, MapEntry(map, size, column + i, row), color256) >= 0)
            return true;
    return false;
}

static void DrawBattleBg(unsigned bg)
{
    unsigned control = Reg(8 + bg * 2), size = control >> 14;
    unsigned map = ((control >> 8) & 31) * 0x800;
    unsigned chars = ((control >> 2) & 3) * 0x4000;
    bool color256 = (control & 128) != 0;
    unsigned scrollX = Reg(0x10 + bg * 4) & 511, scrollY = Reg(0x12 + bg * 4) & 511;
    unsigned columns = (size & 1) ? 64 : 32, rows = (size & 2) ? 64 : 32;
    int ox = (int)(scrollX & 7), oy = (int)(scrollY & 7);
    int x0 = sClipX0, x1 = sClipX1, y0 = sClipY0, y1 = sClipY1;
    unsigned firstColumn, count;
    unsigned lastEntry = ~0u;
    int lastSlot = -1;
    bool wraps = columns == 64;

    if (!wraps)
    {
        MeasureStill(bg);
        wraps = sPanorama[bg];
    }
    if (!wraps)
    {
        DrawBattleCut(bg, 0, 240, 0, 160, 0);
        return;
    }
    if (!ClipCell(&x0, &x1, &y0, &y1)) return;
    firstColumn = (unsigned)(((x0 + ox) & ~7) - ox + (int)scrollX) >> 3;
    count = (unsigned)(x1 - (((x0 + ox) & ~7) - ox) + 7) / 8 + 1;
    /*
     * Above the picture a layer only carries on if its art reaches the top
     * edge. The entry grass of the intro is a band that slides down out of
     * the picture: its map wraps it round to the top, which the GBA never
     * shows and which here repeated the grass along the top of the screen.
     */
    bool above = bg == 3 || BattleRowShown(map, size, chars, color256,
                                           (scrollY >> 3) & (rows - 1), firstColumn, count);

    ViewBase();
    for (int py = ((y0 + oy) & ~7) - oy; py < y1; py += 8)
    {
        int row = (int)(((unsigned)(py + (int)scrollY) >> 3) & (rows - 1));

        if (py + 8 <= 0 && !above) continue;
        /* A row wholly above the picture: where its art comes from. */
        if (py + 8 <= 0 && !BattleRowShown(map, size, chars, color256, row, firstColumn, count))
        {
            if (rows == 64 && BattleRowShown(map, size, chars, color256, row ^ 32, firstColumn, count))
                row ^= 32;
            else if (bg == 3)
                row = (int)((scrollY >> 3) & (rows - 1));
            else
                continue;
        }
        for (int px = ((x0 + ox) & ~7) - ox; px < x1; px += 8)
        {
            unsigned column = ((unsigned)(px + (int)scrollX) >> 3) & (columns - 1);
            unsigned entry = MapEntry(map, size, column, (unsigned)row);

            if (entry != lastEntry)
            {
                lastEntry = entry;
                lastSlot = EntrySlot(chars, entry, color256);
            }
            if (lastSlot >= 0)
                DrawSlot(lastSlot, px + CTR_VIEW_X + sLayerShift, py + CTR_VIEW_Y, entry & 1024, entry & 2048);
        }
    }
}

static bool LayerDrawable(unsigned bg);
static void DrawLayerRect(unsigned bg, int x0, int x1, int y0, int y1,
                          float sx, float sy, float dx, float dy, bool fade, int top, int bottom);

static void DrawTextBg(unsigned bg)
{
    unsigned control = Reg(8 + bg * 2), size = control >> 14;
    /*
     * A background covers as many pixels as its tilemap spans: 256 for the
     * 32-column size the fixed screens use, 512 for the 64-column one the port
     * widened for the field camera. Inside that band the GBA wrap-around
     * applies exactly, so scrolling behaves as on hardware; past it there is
     * nothing to show on a 400px screen, and the line stays backdrop rather
     * than repeating the same image.
     */
    /* Only the moving invocation strip is authored to repeat past a narrow
     * tilemap. Its window is already 400px; the BG walker must cover it too. */
    int right = sFieldMoveShowMon && bg == 0 ? CTR_GAME_WIDTH
              : (int)Min((size & 1) ? 512 : 256, CTR_GAME_WIDTH);
    int bottom = (int)Min((size & 2) ? 512 : 256, CTR_GAME_HEIGHT);
    int left = 0, top = 0;

    if (sStage)
    {
        DrawStageBg(bg);
        return;
    }
    if (sBattle)
    {
        if (bg == 0) DrawBattleText(bg);
        else DrawBattleBg(bg);
        return;
    }
    if (sCentred)
    {
        /* The picture from the layer's texture when it has one: a quad instead of one per tile. */
        if (LayerDrawable(bg) && !sSpanShiftX)
        {
            ViewBase();
            DrawLayerRect(bg, 0, 240, 0, 160, 0, 0, 1, 1, false, 0, 0);
            return;
        }
        if (right > 240) right = 240;
        if (bottom > 160) bottom = 160;
    }
    if (right > sClipX1) right = sClipX1;
    if (bottom > sClipY1) bottom = sClipY1;
    if (left < sClipX0) left = sClipX0;
    if (top < sClipY0) top = sClipY0;
    if (right < left) right = left;
    if (bottom < top) bottom = top;
    DrawTextSpan(bg, left, right, top, bottom);
}

/*
 * Layer textures. A text background is composed once, unscrolled, into a
 * texture of its own that repeats exactly like its tilemap wraps, and kept
 * there for as long as its tilemap, its tiles and its palettes stay the same
 * (LayerRenderCells, per cell). Drawing it is then a handful of quads cut from that texture
 * instead of one quad per 8x8 tile:
 *
 * - a background that scrolls per line (the waves of the intro and the title)
 *   is one quad per run of lines sharing a scroll;
 * - a stage background (the intro, the title) is its picture plus the bands
 *   that fill the screen around it, about 35 quads for a whole 400x240 layer.
 *
 * On an Old 3DS a tile quad costs ~4.6 us of CPU, and the intro's leaves are
 * four 400x240 layers: 4500 quads and 22 ms a frame walked tile by tile. The
 * leaves never change while they are on screen, so the tilemap walk now
 * happens once for the scene.
 *
 * 16-bit colour is enough: the GBA has 15 bits and a transparent one.
 */
#define LAYER_IDLE_FRAMES 180
#define LAYER_RETRY_FRAMES 300u
typedef struct
{
    C3D_Tex tex;
    C3D_RenderTarget *target;
    uint32_t usedFrame;
    /* Frame token and tile-cache generation of the last full walk of this
     * layer's map, and whether that walk drew the whole layer untinted
     * (LayerRenderCells skips the walk while nothing it read can differ). */
    uint32_t walked;
    uint32_t walkedGeneration;
    bool walkedPlain;
    bool valid;
} LayerTexture;
static LayerTexture sLayers[4];
/* Whether each background is drawn from its texture this frame. */
static bool sLayerReady[4];
/* When each background's texture last failed to be placed (0: not failing): only that layer waits. */
static uint32_t sLayerFail[4];
static bool sLayerFailLogged;
/* Native screens draw their text backgrounds from layer textures. */
static bool sFieldLayers;
static bool sFieldBarnDoorWipe;

static unsigned LineValue(unsigned reg, int y, unsigned base)
{
    if (!(sLineMask & (1u << reg))) return base;
    /* A stage's picture has 160 lines; the lines above and below it carry
     * the wave on with the same period rather than stopping it flat. */
    if (sStage) y = ((y % 160) + 160) % 160;
    /* Around the battle scene, the lines next to it: the intro's two halves
     * slide in from the edges of the screen. */
    if (sBattle) y = y < 0 ? 0 : y > 159 ? 159 : y;
    if (y >= 0 && (unsigned)y < sLineCount)
        return sLineScroll[reg][y] & 511;
    return base;
}

/* The shown text backgrounds of the current display mode. */
static unsigned TextBackgrounds(void)
{
    unsigned display = Reg(0), mode = display & 7, found = 0;

    if (mode > 1) return 0;
    for (unsigned bg = 0; bg < 4; ++bg)
    {
        if (!(display & (0x100u << bg))) continue;
        if (mode == 1 && bg >= 2) continue;
        found |= 1u << bg;
    }
    return found;
}

/* Which text backgrounds this frame scrolls per line. */
static unsigned LineBackgrounds(void)
{
    unsigned found = 0, text = TextBackgrounds();

    for (unsigned bg = 0; bg < 4; ++bg)
        if ((text & (1u << bg)) && (sLineMask & (3u << (bg * 2)))) found |= 1u << bg;
    return found;
}

static void LayerRelease(unsigned bg)
{
    LayerTexture *layer = &sLayers[bg];

    if (layer->target) C3D_RenderTargetDelete(layer->target);
    if (layer->tex.data) C3D_TexDelete(&layer->tex);
    memset(layer, 0, sizeof(*layer));
}

static void LayersRelease(void)
{
    for (unsigned bg = 0; bg < 4; ++bg) LayerRelease(bg);
}

/*
 * Outside the frame, where creating or deleting a target cannot stall one in
 * flight: a texture for every background drawn from one this frame, as large
 * as its tilemap. One that cannot be placed falls back to the tile walk and
 * is retried after a while, logged once.
 */
/* A texture and render target for background bg at its tilemap's size; nothing is left behind on failure. */
static bool LayerCreate(unsigned bg, unsigned width, unsigned height)
{
    LayerTexture *layer = &sLayers[bg];

    if (C3D_TexInitVRAM(&layer->tex, width, height, GPU_RGBA5551)
        && (layer->target = C3D_RenderTargetCreateFromTex(&layer->tex, GPU_TEXFACE_2D, 0, -1)))
        return true;
    LayerRelease(bg);
    return false;
}

static void LayersPrepare(void)
{
    /* A centred screen is drawn from its textures too (DrawTextBg). */
    unsigned want = NativeIntroActive() ? 0 : LineBackgrounds() | (sIntro || sStage || sFieldLayers || sCentred ? TextBackgrounds() : 0);

    for (unsigned bg = 0; bg < 4; ++bg)
    {
        LayerTexture *layer = &sLayers[bg];
        unsigned size = Reg(8 + bg * 2) >> 14;
        unsigned width = (size & 1) ? 512 : 256, height = (size & 2) ? 512 : 256;

        sLayerReady[bg] = false;
        if (!(want & (1u << bg)))
        {
            if (layer->tex.data && sStats.frames - layer->usedFrame > LAYER_IDLE_FRAMES)
                LayerRelease(bg);
            continue;
        }
        if (layer->tex.data && (layer->tex.width != width || layer->tex.height != height))
            LayerRelease(bg);
        if (!layer->tex.data)
        {
            if (sLayerFail[bg] && sStats.frames - sLayerFail[bg] < LAYER_RETRY_FRAMES) continue;
            if (!LayerCreate(bg, width, height))
            {
                /*
                 * A block this size may be missing with enough free in pieces: the textures kept for backgrounds
                 * this frame does not draw (they are not in `want`, so nothing of this frame uses them, and this
                 * is outside the frame) go before this one gives up.
                 */
                unsigned freed = 0;

                for (unsigned other = 0; other < 4; ++other)
                    if (!(want & (1u << other)) && sLayers[other].tex.data)
                    {
                        LayerRelease(other);
                        ++freed;
                    }
                if (!freed || !LayerCreate(bg, width, height))
                {
                    /* A stage short of VRAM while three planes hold it: the third plane goes (outside the frame,
                     * BandsShrink) and the layer is tried again next frame instead of waiting LAYER_RETRY_FRAMES. */
                    if ((sStage || sIntro) && sBandsReady && sBandCount > 2)
                    {
                        sPlaneShrinkAsked = true;
                        continue;
                    }
                    if (!sLayerFailLogged)
                        CtrLog_Write(CTR_LOG_ERROR, "VIDEO: no VRAM for a %ux%u layer texture (free=%lu); "
                                     "tile walk", width, height, (unsigned long)vramSpaceFree());
                    sLayerFailLogged = true;
                    sLayerFail[bg] = sStats.frames | 1; /* only this layer waits; the others still try */
                    continue;
                }
            }
            sLayerFail[bg] = 0;
            C3D_TexSetFilter(&layer->tex, GPU_NEAREST, GPU_NEAREST);
            C3D_TexSetWrap(&layer->tex, GPU_REPEAT, GPU_REPEAT);
        }
        layer->usedFrame = sStats.frames;
        sLayerReady[bg] = true;
    }
}

/*
 * Every layer texture is kept cell by cell (from pokeemerald-3Ds-dualscreen).
 * Each cell remembers what was drawn in it - the upload serial of its tile,
 * which changes whenever the tile's bytes or palette do, and its flips - and
 * only the cells whose tile, tilemap entry or palette changed are cleared and
 * drawn again: a row or a column as a map scrolls, the animated tiles when
 * they animate. A frame where most cells changed is recomposed whole. Every
 * cell's tile is still validated every frame, as the tile walk did.
 */
#define LAYER_CELLS 4096
static uint32_t sCellSig[4][LAYER_CELLS];
static unsigned sCellControl[4];

/*
 * A field layer is sampled only through DrawFieldBgTex: the window of the
 * texture at its scroll, at most CTR_GAME_WIDTH x CTR_GAME_HEIGHT. A palette
 * fade (the battle transition's flash) re-uploads every tile it touches, which
 * changed every cell's signature and recomposed the whole 64x64 texture -
 * ~4100 quads a frame for a map that had not moved. Only the cells in that
 * window are composed; the others keep their old signature, so each is drawn
 * again exactly when it is next in view. Spans are [start, start + count)
 * modulo the tilemap, at most two each way.
 */
typedef struct { unsigned start[2], count[2], spans; } CellSpans;

static void VisibleSpans(CellSpans *out, unsigned scroll, unsigned pixels, unsigned cells)
{
    unsigned first = (scroll / 8) % cells, last = (scroll + pixels - 1) / 8;
    unsigned n = last - scroll / 8 + 1;

    if (n >= cells) { out->start[0] = 0; out->count[0] = cells; out->spans = 1; return; }
    out->start[0] = first;
    out->count[0] = first + n <= cells ? n : cells - first;
    out->spans = first + n <= cells ? 1 : 2;
    out->start[1] = 0;
    out->count[1] = n - out->count[0];
}

static bool InSpans(const CellSpans *spans, unsigned cell)
{
    for (unsigned i = 0; i < spans->spans; ++i)
        if (cell - spans->start[i] < spans->count[i]) return true;
    return false;
}

static bool FieldLayerWindowed(unsigned bg)
{
    return sFieldLayers && !sIntro && !sStage && bg != 0 && !(LineBackgrounds() & (1u << bg));
}

/* Did any VRAM block overlapping [address, address + bytes) change after
 * token `after`? An address outside VRAM counts as changed. */
static bool VramChangedAfter(unsigned address, unsigned bytes, uint32_t after)
{
    if (bytes == 0) return false;
    if (address >= 0x18000) return true;
    if (bytes > 0x18000 - address) bytes = 0x18000 - address;
    for (unsigned b = address / CTR_VRAM_TRACK_BLOCK; b <= (address + bytes - 1) / CTR_VRAM_TRACK_BLOCK; ++b)
        if (sVramStamp[b] > after) return true;
    return false;
}

static bool LayerRenderCells(unsigned bg)
{
    static uint16_t dirty[LAYER_CELLS];
    static int16_t cellSlot[LAYER_CELLS];
    static uint16_t cellEntry[LAYER_CELLS];
    static uint32_t cellSig[LAYER_CELLS];
    static uint32_t memoStamp[1024], stamp;
    static uint16_t memoEntry[1024];
    static int16_t memoSlot[1024];
    LayerTexture *layer = &sLayers[bg];
    unsigned control = Reg(8 + bg * 2), size = control >> 14;
    unsigned map = ((control >> 8) & 31) * 0x800;
    unsigned chars = ((control >> 2) & 3) * 0x4000;
    bool color256 = (control & 128) != 0;
    unsigned columns = (size & 1) ? 64 : 32, rows = (size & 2) ? 64 : 32;
    unsigned cells = rows * columns;
    bool full = !layer->valid || sCellControl[bg] != control;
    unsigned count = 0, visible = cells;
    CellSpans spanX, spanY;
    bool windowed = !full && FieldLayerWindowed(bg);

    if (windowed)
    {
        VisibleSpans(&spanX, Reg(0x10 + bg * 4) & 511, Min(columns * 8, CTR_GAME_WIDTH), columns);
        VisibleSpans(&spanY, Reg(0x12 + bg * 4) & 511, Min(rows * 8, CTR_GAME_HEIGHT), rows);
        visible = (spanX.count[0] + spanX.count[1] * (spanX.spans > 1))
                * (spanY.count[0] + spanY.count[1] * (spanY.spans > 1));
        windowed = visible < cells;
    }
    /* Nothing the walk reads has changed since the last whole-layer walk:
     * no cell can differ. Anything uncertain (windowed, tinted, a new cache
     * generation, a changed palette or map/tile VRAM) walks. */
    if (!full && !windowed && !sLayerFadeTint[bg]
        && layer->walkedPlain
        && layer->walkedGeneration == sCacheGeneration
        && sBgPaletteStamp <= layer->walked
        && !VramChangedAfter(map, rows * columns * 2, layer->walked)
        && !VramChangedAfter(chars, Min(1024u * (color256 ? 64 : 32), 0x10000u - chars), layer->walked))
        return false;
    layer->walked = sStats.frames + 1;
    layer->walkedGeneration = sCacheGeneration;
    layer->walkedPlain = !windowed && !sLayerFadeTint[bg];
    ++stamp;
    for (unsigned row = 0; row < rows; ++row)
    {
        unsigned rowBase = map + CtrVideo_TextMapOffset(0, row, size);

        if (windowed && !InSpans(&spanY, row)) continue;
        for (unsigned column = 0; column < columns; ++column)
        {
            unsigned cell = row * columns + column;
            if (windowed && !InSpans(&spanX, column)) continue;
            unsigned entry = Read16(rowBase + (column & 31) * 2 + (column >> 5) * 2048);
            unsigned index = entry & 1023;
            int slot;

            if (memoStamp[index] == stamp && memoEntry[index] == entry)
                slot = memoSlot[index];
            else
            {
                unsigned address = chars + index * (color256 ? 64 : 32);

                slot = address < 0x10000 ? GetTileSlotFrom(address, entry >> 12, color256, sLayerFadeTint[bg]) : -1;
                memoStamp[index] = stamp;
                memoEntry[index] = (uint16_t)entry;
                memoSlot[index] = (int16_t)slot;
            }
            cellSlot[cell] = (int16_t)slot;
            cellEntry[cell] = (uint16_t)entry;
            /* Nothing drawn is 0; anything drawn has the top bit set. */
            cellSig[cell] = slot < 0 ? 0
                : (((sTiles[slot].serial << 2) | ((entry >> 10) & 3)) | 1u << 31);
            if (!full && sCellSig[bg][cell] == cellSig[cell]) continue;
            dirty[count++] = (uint16_t)cell;
        }
    }
    if (count == 0) return false;
    /* Most of the window changed: clear it as whole rectangles (at most four
     * with wrap) and draw every cell in it, instead of a clear per cell. */
    bool wholeWindow = windowed && count > visible / 2;
    if (wholeWindow)
    {
        count = 0;
        for (unsigned row = 0; row < rows; ++row)
            if (InSpans(&spanY, row))
                for (unsigned column = 0; column < columns; ++column)
                    if (InSpans(&spanX, column)) dirty[count++] = (uint16_t)(row * columns + column);
    }
    else if (full || count > cells / 2)
    {
        full = true;
        count = cells;
    }
    /* Past half the frame's geometry the layer is walked tile by tile this
     * frame instead, and composed on a later one. */
    if (sStats.tiles + count >= MAX_DRAWS / 2)
    {
        sLayerReady[bg] = false;
        layer->walkedPlain = false;
        return false;
    }
    if (full)
        memcpy(sCellSig[bg], cellSig, cells * sizeof(*cellSig));
    else
        for (unsigned i = 0; i < count; ++i)
            sCellSig[bg][dirty[i]] = cellSig[dirty[i]];
    layer->valid = true;
    sCellControl[bg] = control;

    BlendForget();
    if (full)
    {
        C2D_TargetClear(layer->target, 0);
        C2D_SceneBegin(layer->target);
        C2D_ViewReset();
        for (unsigned cell = 0; cell < cells; ++cell)
            dirty[cell] = (uint16_t)cell;
    }
    else
    {
        /* Clear the changed cells to transparent first: a tile's transparent
         * pixels must not keep what was drawn there before. */
        C2D_SceneBegin(layer->target);
        C2D_ViewReset();
        C2D_Flush();
        C3D_AlphaTest(false, GPU_ALWAYS, 0);
        C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
        if (wholeWindow)
            for (unsigned y = 0; y < spanY.spans; ++y)
                for (unsigned x = 0; x < spanX.spans; ++x)
                    C2D_DrawRectSolid(spanX.start[x] * 8, spanY.start[y] * 8, 0,
                                      spanX.count[x] * 8, spanY.count[y] * 8, 0);
        else
            for (unsigned i = 0; i < count; ++i)
                C2D_DrawRectSolid((dirty[i] % columns) * 8, (dirty[i] / columns) * 8, 0, 8, 8, 0);
        C2D_Flush();
        BlendForget();
    }
    Blend(bg, false, false);
    for (unsigned i = 0; i < count; ++i)
    {
        unsigned cell = dirty[i];

        if (cellSlot[cell] >= 0)
            DrawSlot(cellSlot[cell], (cell % columns) * 8, (cell / columns) * 8,
                     cellEntry[cell] & 1024, cellEntry[cell] & 2048);
    }
    C2D_Flush();
    BlendForget();
    return true;
}

/* Inside the frame, before the logical surface: the maps that changed. */
static void LayersRender(void)
{
    bool any = false;
    unsigned blend = Reg(0x50);

    for (unsigned bg = 0; bg < 4; ++bg)
    {
        /* Only where the layer's own tint is plain: a brightness effect on
         * it (BLDCNT effect 2/3) keeps the per-palette path. */
        bool brightness = ((blend >> 6) & 3) >= 2 && (blend & (1u << bg));
        sLayerFadeTint[bg] = sLayerReady[bg] && FieldLayerWindowed(bg) && !brightness;
        if (sLayerReady[bg] && LayerRenderCells(bg)) any = true;
    }
    /* Rendering to a texture and then sampling it needs a command split. */
    if (any) C3D_FrameSplit(0);
}

static bool LayerDrawable(unsigned bg)
{
    return sLayerReady[bg] && sLayers[bg].valid;
}

/*
 * A text background of a native screen from its layer texture: one quad,
 * wrapping as its tilemap does, over the band the tilemap spans (DrawTextBg's
 * limits), instead of a quad per tile (from pokeemerald-3Ds-dualscreen).
 */
static bool DrawFieldBgTex(unsigned bg)
{
    LayerTexture *layer = &sLayers[bg];
    float width, height, sx, sy;
    int x0 = sClipX0 > 0 ? sClipX0 : 0, y0 = sClipY0 > 0 ? sClipY0 : 0, x1, y1;

    if (!sFieldLayers || bg == 0 || !LayerDrawable(bg)) return false;
    width = layer->tex.width;
    height = layer->tex.height;
    x1 = (int)Min((unsigned)layer->tex.width, CTR_GAME_WIDTH);
    y1 = (int)Min((unsigned)layer->tex.height, CTR_GAME_HEIGHT);
    if (x1 > sClipX1) x1 = sClipX1;
    if (y1 > sClipY1) y1 = sClipY1;
    if (x0 >= x1 || y0 >= y1) return true;
    sx = Reg(0x10 + bg * 4) & 511;
    sy = Reg(0x12 + bg * 4) & 511;
    ViewBase();
    {
        const Tex3DS_SubTexture run = {(u16)(x1 - x0), (u16)(y1 - y0),
            (sx + x0) / width, 1.0f - (sy + y0) / height,
            (sx + x1) / width, 1.0f - (sy + y1) / height};

        C2D_ImageTint fade;
        const C2D_ImageTint *tint = &sTint;
        if (sLayerFadeTint[bg] && sFadeCoeff)
        {
            uint32_t rgb = CtrVideo_RGBA8(sFadeTarget, true);
            C2D_PlainImageTint(&fade, C2D_Color32(rgb >> 24, rgb >> 16, rgb >> 8, 255), sFadeCoeff / 16.0f);
            tint = &fade;
        }
        ++sStats.tiles;
        C2D_DrawImageAt((C2D_Image){&layer->tex, &run}, x0 + CTR_VIEW_X + sLayerShift,
                        y0 + CTR_VIEW_Y, 0, tint, 1, 1);
    }
    return true;
}

/*
 * One rectangle [x0, x1) x [y0, y1) of the screen, in GBA coordinates, cut
 * from a layer texture: the screen point (x0, y0) shows the picture point
 * (sx, sy), and each screen pixel steps (dx, dy) through the picture - 1 to
 * copy, -1 to mirror, a fraction to stretch. Clipped to the current clip
 * rectangle by moving its texture coordinates, so it needs no scissor. With
 * fade, its corners are tinted by EdgeFade.
 */
static void DrawLayerRect(unsigned bg, int x0, int x1, int y0, int y1,
                          float sx, float sy, float dx, float dy, bool fade, int top, int bottom)
{
    LayerTexture *layer = &sLayers[bg];
    int cx0 = x0 > sClipX0 ? x0 : sClipX0, cx1 = x1 < sClipX1 ? x1 : sClipX1;
    int cy0 = y0 > sClipY0 ? y0 : sClipY0, cy1 = y1 < sClipY1 ? y1 : sClipY1;
    float width = layer->tex.width, height = layer->tex.height;
    float ox = Reg(0x10 + bg * 4) & 511, oy = Reg(0x12 + bg * 4) & 511;
    float s0, s1, t0, t1;
    C2D_ImageTint tint = sTint;

    if (cx0 >= cx1 || cy0 >= cy1) return;
    s0 = sx + (cx0 - x0) * dx + ox;
    s1 = sx + (cx1 - x0) * dx + ox;
    t0 = sy + (cy0 - y0) * dy + oy;
    t1 = sy + (cy1 - y0) * dy + oy;
    if (fade)
    {
        bool white;
        float bright = LayerBrightness(bg, &white);

        FadeCorner(&tint, C2D_TopLeft, EdgeFade(cx0, cy0, top, bottom), bright, white);
        FadeCorner(&tint, C2D_TopRight, EdgeFade(cx1, cy0, top, bottom), bright, white);
        FadeCorner(&tint, C2D_BotLeft, EdgeFade(cx0, cy1, top, bottom), bright, white);
        FadeCorner(&tint, C2D_BotRight, EdgeFade(cx1, cy1, top, bottom), bright, white);
    }
    {
        /*
         * A subtexture whose top is below its bottom means "rotated" to
         * Tex3DS, so a mirror is drawn as the plain cut flipped by a negative
         * scale, the same way flipped tiles are. (Only unfaded cuts mirror.)
         */
        bool flipY = t1 < t0;
        float ta = flipY ? t1 : t0, tb = flipY ? t0 : t1;
        const Tex3DS_SubTexture cut = {(u16)(cx1 - cx0), (u16)(cy1 - cy0),
            s0 / width, 1.0f - ta / height, s1 / width, 1.0f - tb / height};

        ++sStats.tiles;
        C2D_DrawImageAt((C2D_Image){&layer->tex, &cut}, cx0 + CTR_VIEW_X + sLayerShift,
                        cy0 + CTR_VIEW_Y, 0, &tint, 1, flipY ? -1 : 1);
    }
}

/* One corner cell: the corner block stretched to it, faded, skipped if black. */
static void DrawCornerCell(unsigned bg, int x0, int x1, int y0, int y1, float sx, float sy,
                           int top, int bottom)
{
    if (y0 < VIEW_TOP) y0 = VIEW_TOP;
    if (y1 > VIEW_BOTTOM) y1 = VIEW_BOTTOM;
    if (y0 < top && y1 > top) y1 = top;
    if (y0 < bottom && y1 > bottom) y0 = bottom;
    if (x0 >= x1 || y0 >= y1) return;
    if (sUnderlaid && EdgeFade(x0, y0, top, bottom) >= 1.0f && EdgeFade(x1, y0, top, bottom) >= 1.0f
        && EdgeFade(x0, y1, top, bottom) >= 1.0f && EdgeFade(x1, y1, top, bottom) >= 1.0f)
        return;
    DrawLayerRect(bg, x0, x1, y0, y1, sx, sy, 8.0f / (x1 - x0), 8.0f / (y1 - y0), true, top, bottom);
}

/*
 * A stage background from its texture, filled around the picture exactly as
 * DrawStageBg does it tile by tile: a layer that scrolls wraps across and is
 * mirrored above and below; a still one is carried out from its edges - the
 * 8-pixel band along each edge repeated outwards, the corners stretched from
 * the corner block - and faded to black.
 */
/* Oak's callback owns CONTROLS (three pages), Pikachu (three pages), and
 * the speech. BG0 is live UI; BG2 contains the trainer portrait, not a
 * panorama. Both retain their authored 240x160 extent, including scroll.
 * BG1 contains the decoration and the control icons. Keep that entire
 * picture 1:1 too. Only its uniform outside colour is composed into the
 * additional canvas; never repeat an 8px border tile, icon, or watermark.
 *
 * In the pinned Oak maps, column zero is the unadorned surround: Controls
 * puts icons in columns 1..5, Pikachu's frame starts inside column zero,
 * and speech BG1 consists entirely of horizontal colour bands. Decode its
 * outside pixel to a solid colour, coalesce equal rows, and continue the
 * top/bottom colour beyond the authored height. No texture is stretched.
 * This is deliberately callback-specific, not a generic stage heuristic.
 */
static uint32_t OakSurroundColor(int y)
{
    const unsigned bg = 1;
    unsigned control = Reg(8 + bg * 2), size = control >> 14;
    unsigned map = ((control >> 8) & 31) * 0x800;
    unsigned chars = ((control >> 2) & 3) * 0x4000;
    bool color256 = (control & 128) != 0;
    unsigned x = Reg(0x10 + bg * 4) & 511;
    unsigned row = (unsigned)(y < 0 ? 0 : y > 159 ? 159 : y);
    row += Reg(0x12 + bg * 4) & 511;
    unsigned entry = MapEntry(map, size, x / 8, row / 8);
    unsigned px = x & 7, py = row & 7;
    if (entry & 1024) px = 7 - px;
    if (entry & 2048) py = 7 - py;
    unsigned address = chars + (entry & 1023) * (color256 ? 64 : 32);
    if (!BgCharacterAddressValid(bg, control, map, chars, entry, address)) return 0;
    unsigned pixel = sMemory.vram[address + py * (color256 ? 8 : 4) + (color256 ? px : px / 2)];
    if (!color256) pixel = (pixel >> ((px & 1) * 4)) & 15;
    if (!pixel) return 0; /* GBA transparent colour still exposes the backdrop. */
    unsigned index = color256 ? pixel : (entry >> 12) * 16 + pixel;
    uint32_t rgb = CtrVideo_RGBA8(sPalette[index], true);
    bool white;
    float bright = LayerBrightness(bg, &white);
    unsigned r = rgb >> 24, g = (rgb >> 16) & 255, b = (rgb >> 8) & 255;
    unsigned target = white ? 255 : 0;
    r = (unsigned)(r + ((float)target - r) * bright);
    g = (unsigned)(g + ((float)target - g) * bright);
    b = (unsigned)(b + ((float)target - b) * bright);
    return C2D_Color32(r, g, b, 255);
}

static void DrawOakSurround(void)
{
    ViewBase();
    for (int y = sClipY0; y < sClipY1;)
    {
        uint32_t color = OakSurroundColor(y);
        int end = y + 1;
        while (end < sClipY1 && OakSurroundColor(end) == color) ++end;
        if (color)
            C2D_DrawRectSolid(sClipX0 + CTR_VIEW_X + sLayerShift, y + CTR_VIEW_Y, 0,
                             sClipX1 - sClipX0, end - y, color);
        y = end;
    }
}

static void DrawStageUiText(unsigned bg)
{
    int left = sClipX0 > 0 ? sClipX0 : 0;
    int top = sClipY0 > 0 ? sClipY0 : 0;
    int right = sClipX1 < 240 ? sClipX1 : 240;
    int bottom = sClipY1 < 160 ? sClipY1 : 160;
    if (left >= right || top >= bottom) return;
    if (LayerDrawable(bg))
    {
        ViewBase();
        DrawLayerRect(bg, 0, 240, 0, 160, 0, 0, 1, 1, false, 0, 160);
        return;
    }
    /* Clip direct-tile fallback: a partially scrolled 8px cell can cross
     * the right/bottom GBA edge without the explicit scissor. */
    C2D_Flush();
    Scissor(left, top, right, bottom);
    DrawTextSpan(bg, left, right, top, bottom);
    C2D_Flush();
    RestoreScissor();
}

static bool DrawStageBgTex(unsigned bg)
{
    int top, bottom;
    bool wraps;

    if (!sStage || !LayerDrawable(bg)) return false;
    PictureRows(bg, &top, &bottom);
    wraps = sScrolls[bg];
    if (!wraps)
    {
        MeasureStill(bg);
        /* A still panorama wraps too when the scene around it scrolls. */
        if (sPanorama[bg])
            for (unsigned other = 0; other < 4; ++other)
                if (other != bg && sScrolls[other] && (Reg(0) & (0x100u << other))) wraps = true;
    }
    ViewBase();
    if (wraps)
    {
        DrawLayerRect(bg, VIEW_LEFT, VIEW_RIGHT, top, bottom, VIEW_LEFT, top, 1, 1, false, top, bottom);
        DrawLayerRect(bg, VIEW_LEFT, VIEW_RIGHT, VIEW_TOP, top, VIEW_LEFT, 2 * top - VIEW_TOP, 1, -1,
                      false, top, bottom);
        DrawLayerRect(bg, VIEW_LEFT, VIEW_RIGHT, bottom, VIEW_BOTTOM, VIEW_LEFT, bottom, 1, -1,
                      false, top, bottom);
        return true;
    }
    DrawLayerRect(bg, 0, 240, top, bottom, 0, top, 1, 1, false, top, bottom);
    for (int x = 0; x > VIEW_LEFT; x -= 8)
        DrawLayerRect(bg, x - 8, x, top, bottom, 0, top, 1, 1, true, top, bottom);
    for (int x = 240; x < VIEW_RIGHT; x += 8)
        DrawLayerRect(bg, x, x + 8, top, bottom, 232, top, 1, 1, true, top, bottom);
    for (int y = top; y > VIEW_TOP; y -= 8)
        DrawLayerRect(bg, 0, 240, y - 8, y, 0, top, 1, 1, true, top, bottom);
    for (int y = bottom; y < VIEW_BOTTOM; y += 8)
        DrawLayerRect(bg, 0, 240, y, y + 8, 0, bottom - 8, 1, 1, true, top, bottom);
    /*
     * The corners: the 8x8 corner block of the picture, in 16-pixel cells
     * each with its own fade, so the gradient bends round the corner instead
     * of being smeared across one quad. Cells already black are left to
     * StageUnderlay when it drew.
     */
    for (int y = top; y > VIEW_TOP; y -= 16)
        for (int x = 0; x > VIEW_LEFT; x -= 16)
        {
            DrawCornerCell(bg, x - 16, x, y - 16, y, 0, top, top, bottom);
            DrawCornerCell(bg, 240 - x, 256 - x, y - 16, y, 232, top, top, bottom);
        }
    for (int y = bottom; y < VIEW_BOTTOM; y += 16)
        for (int x = 0; x > VIEW_LEFT; x -= 16)
        {
            DrawCornerCell(bg, x - 16, x, y, y + 16, 0, bottom - 8, top, bottom);
            DrawCornerCell(bg, 240 - x, 256 - x, y, y + 16, 232, bottom - 8, top, bottom);
        }
    return true;
}

/*
 * A text background of the intro is its own map, and the GBA only showed the first 240 columns of it. The map is 256
 * wide (or 512): its columns 240..255 are real FireRed data that the old screen never reached, so they are shown as
 * they are (Nidorino's rump and the grass in scene 2 live there), and nothing is drawn where the map ends, which on the
 * hardware is where it wraps. Objects stay on the GBA's own screen (bg < 0); a scenery layer is composed by
 * DrawIntroBgTex and is not clipped here.
 */
static int IntroMapWidth(int bg)
{
    int width = (Reg(8 + bg * 2) & 0x4000) ? 512 : 256;

    return width < VIEW_RIGHT ? width : VIEW_RIGHT;
}

/*
 * Where a background's own map ends and the scene has nothing of it, a layer FireRed's map has no more columns for
 * (scene 2's Nidorino stands in grass that runs on) can be told to go on with its last tile column: complete tiles of
 * the same map, background only, never a smeared pixel and never a piece of the Pokemon (the last column holds only
 * grass). Anything else simply ends where its map does, as on the hardware.
 */
static unsigned sIntroEdgeFill;

static int IntroMapRight(int bg)
{
    return (sIntroEdgeFill & (1u << bg)) ? VIEW_RIGHT : IntroMapWidth(bg);
}

/* Native canvas padding consists of complete edge tiles, never magnified pixels. */
static int IntroSpan(int p, int end, int first, int last, bool repeat, int *source)
{
    int length = end - p;
    *source = p;
    if (repeat) return length;
    if (p < first || p >= last)
    {
        int edge = p < first ? first : last;
        int phase = ((p - edge) % 8 + 8) % 8;
        *source = (p < first ? first : last - 8) + phase;
        if (length > 8 - phase) length = 8 - phase;
        if (p < first && length > first - p) length = first - p;
    }
    else if (length > last - p) length = last - p;
    return length;
}

static bool DrawIntroBgTex(unsigned bg)
{
    if (!sIntro) return false;
    bool textured = LayerDrawable(bg);
    bool scenery = (sIntroBackgrounds & (1u << bg)) != 0;
    if (!scenery)
    {
        if (!textured) return false;
        int width = IntroMapWidth(bg);

        ViewBase();
        DrawLayerRect(bg, 0, width, 0, 160, 0, 0, 1, 1, false, 0, 160);
        if ((sIntroEdgeFill & (1u << bg)) && !(Reg(0x10 + bg * 4) & 255) && width == 256)
            for (int x = width; x < sClipX1; x += 8)
                DrawLayerRect(bg, x, x + 8, 0, 160, width - 8, 0, 1, 1, false, 0, 160);
        return true;
    }
    int viewX = sViewX, viewY = sViewY;
    unsigned baseX = Reg(0x10 + bg * 4) & 511, baseY = Reg(0x12 + bg * 4) & 511;
    bool lines = (sLineMask & (3u << (bg * 2))) != 0;
    bool repeat = (sIntroRepeat & (1u << bg)) != 0;
    ViewBase();
    for (int y = sClipY0, h; y < sClipY1; y += h)
    {
        int sy;
        h = IntroSpan(y, sClipY1, sIntroTop, sIntroBottom, false, &sy);
        if (lines) h = 1;
        unsigned ox = LineValue(bg * 2, sy, baseX), oy = LineValue(bg * 2 + 1, sy, baseY);
        for (int x = sClipX0, w; x < sClipX1; x += w)
        {
            int sx;
            w = IntroSpan(x, sClipX1, 0, 240, repeat, &sx);
            if (textured)
            {
                DrawLayerRect(bg, x, x + w, y, y + h,
                              sx + (int)ox - (int)baseX, sy + (int)oy - (int)baseY,
                              1, 1, false, 0, 160);
            }
            else
            {
                C2D_Flush();
                Scissor(x, y, x + w, y + h);
                int fx = sx + (int)ox - (int)baseX, fy = sy + (int)oy - (int)baseY;
                sViewX = viewX + x - fx;
                sViewY = viewY + y - fy;
                DrawTextSpan(bg, fx, fx + w, fy, fy + h);
                C2D_Flush();
                sViewX = viewX; sViewY = viewY;
            }
        }
    }
    if (!textured) RestoreScissor();
    return true;
}

/*
 * The title composed for the taller screen, every tile of it 1:1. FireRed's title is a 240x160 picture whose bands run
 * straight across: BG2 holds the orange line and the black band above the teal (rows 0-29) and the black band with PRESS
 * START and the red footer under it (rows 112-159); BG3 holds the teal between them. Floating in the middle of a 400x240
 * canvas those bands left 40 rows of black above and below the picture. Here the top block goes to the canvas's top and
 * the bottom block to its bottom, the teal's own tile row fills what is between, and each band continues to the canvas's
 * sides with its own edge tile column; the logo, Charizard, the flames and the slash stay exactly where FireRed puts
 * them (their effects belong to those places), now over teal.
 */
#define TITLE_TOP_ROWS 30
#define TITLE_BOTTOM_ROW 112
#define TITLE_TEAL_ROW 32
static bool TitleComposed(unsigned bg)
{
    return sIntro && !NativeIntroActive() && sNativeIntroScene == CTR_INTRO_TITLE && (bg == 2 || bg == 3);
}

/* Source rect of the layer's map (x, y of its picture, scroll added) onto the stage at (x0, y0)..(x1, y1). */
static void IntroBlit(unsigned bg, int x0, int x1, int y0, int y1, int sx, int sy)
{
    int cx0 = x0 > sClipX0 ? x0 : sClipX0, cx1 = x1 < sClipX1 ? x1 : sClipX1;
    int cy0 = y0 > sClipY0 ? y0 : sClipY0, cy1 = y1 < sClipY1 ? y1 : sClipY1;

    if (cx0 >= cx1 || cy0 >= cy1) return;
    if (LayerDrawable(bg))
    {
        DrawLayerRect(bg, x0, x1, y0, y1, sx, sy, 1, 1, false, 0, 160);
        return;
    }
    {
        int fx = sx + (cx0 - x0), fy = sy + (cy0 - y0), viewX = sViewX, viewY = sViewY;

        C2D_Flush();
        Scissor(cx0, cy0, cx1, cy1);
        sViewX = viewX + cx0 - fx;
        sViewY = viewY + cy0 - fy;
        DrawTextSpan(bg, fx, fx + (cx1 - cx0), fy, fy + (cy1 - cy0));
        C2D_Flush();
        sViewX = viewX;
        sViewY = viewY;
        RestoreScissor();
    }
}

static bool DrawTitleBand(unsigned bg)
{
    static const int blocks[2][2] = {{0, TITLE_TOP_ROWS}, {TITLE_BOTTOM_ROW, 160}};
    static const int shift[2] = {-40, 40};      /* the top block up to the canvas's top, the bottom block down to its bottom */

    if (!TitleComposed(bg)) return false;
    ViewBase();
    for (int b = 0; b < 2; ++b)
    {
        int top = blocks[b][0], bottom = blocks[b][1], y0 = top + shift[b], y1 = bottom + shift[b];

        IntroBlit(bg, 0, 256, y0, y1, 0, top);                  /* the map's own columns, 240..255 included */
        for (int x = 256; x < VIEW_RIGHT; x += 8)
            IntroBlit(bg, x, x + 8, y0, y1, 248, top);          /* the last tile column, onward */
        for (int x = 0; x > VIEW_LEFT; x -= 8)
            IntroBlit(bg, x - 8, x, y0, y1, 0, top);            /* the first one, backward */
    }
    if (bg == 3)
        for (int y = TITLE_TOP_ROWS + shift[0]; y < TITLE_BOTTOM_ROW + shift[1]; y += 8)
        {
            int y1 = y + 8 < TITLE_BOTTOM_ROW + shift[1] ? y + 8 : TITLE_BOTTOM_ROW + shift[1];

            IntroBlit(bg, 0, 256, y, y1, 0, TITLE_TEAL_ROW);     /* the teal tile row, as it is, between the blocks */
            IntroBlit(bg, VIEW_LEFT, 0, y, y1, 0, TITLE_TEAL_ROW);
            IntroBlit(bg, 256, VIEW_RIGHT, y, y1, 0, TITLE_TEAL_ROW);
        }
    return true;
}

static bool DrawLineBg(unsigned bg)
{
    if (sIntro && (sIntroBackgrounds & (1u << bg))) return DrawIntroBgTex(bg);
    unsigned baseX = Reg(0x10 + bg * 4) & 511, baseY = Reg(0x12 + bg * 4) & 511;
    /* A still stage fades its margins (DrawEdgeRegion); so do these layers. */
    bool fade = sStage && !sScrolls[bg];
    int top = 0, bottom = 160;
    int clipX0 = sClipX0, clipX1 = sClipX1, clipY0 = sClipY0, clipY1 = sClipY1;
    /* The battle terrain repeats its top tile row above the picture, as
     * DrawBattleBg does; this is the line where that starts. */
    bool clampTop = sBattle && bg == 3;
    C3D_Tex *tex;
    float width, height;

    if (!(LineBackgrounds() & (1u << bg)) || !LayerDrawable(bg)) return false;
    tex = &sLayers[bg].tex;
    width = tex->width;
    height = tex->height;
    if (fade) PictureRows(bg, &top, &bottom);
    if (sBattle && !(Reg(8 + bg * 2) & 0x4000))
    {
        /* A 32-column layer only wraps into the sides when it is made to. */
        MeasureStill(bg);
        if (!sPanorama[bg])
        {
            if (clipX0 < 0) clipX0 = 0;
            if (clipX1 > 240) clipX1 = 240;
            if (clipY0 < 0) clipY0 = 0;
            if (clipY1 > 160) clipY1 = 160;
            clampTop = false;
        }
    }
    ViewBase();
    for (int y = clipY0; y < clipY1;)
    {
        unsigned x = LineValue(bg * 2, y, baseX), v = LineValue(bg * 2 + 1, y, baseY);
        int end = y + 1;
        /* The texture row line y shows. */
        int ty = (int)v + y;
        int rowTop = -(int)(v & 7);

        if (clampTop && y < rowTop)
        {
            /* The tile row under line 0, once per eight lines above it. */
            int phase = ((y - rowTop) % 8 + 8) % 8;

            ty = (int)(v - (v & 7)) + phase;
            end = y + 8 - phase;
            if (end > clipY1) end = clipY1;
        }
        else while (end < clipY1 && LineValue(bg * 2, end, baseX) == x
               && LineValue(bg * 2 + 1, end, baseY) == v)
        {
            /* The fade is linear between these lines, so a run stops at them. */
            if (fade && (end == top || end == bottom || end == top - (int)FADE_DOWN
                         || end == bottom + (int)FADE_DOWN))
                break;
            ++end;
        }
        /* Left margin, picture, right margin: each piece gets its own fade. */
        for (unsigned piece = 0; piece < 3; ++piece)
        {
            int x0 = fade ? (piece == 0 ? clipX0 : piece == 1 ? 0 : 240) : clipX0;
            int x1 = fade ? (piece == 0 ? 0 : piece == 1 ? 240 : clipX1) : clipX1;

            if (x0 < clipX0) x0 = clipX0;
            if (x1 > clipX1) x1 = clipX1;
            if (x0 >= x1) { if (!fade) break; continue; }
            {
                /* The texture repeats, so coordinates past its edges wrap. */
                const Tex3DS_SubTexture run = {(u16)(x1 - x0), (u16)(end - y),
                    ((int)x + x0) / width, 1.0f - ty / height,
                    ((int)x + x1) / width, 1.0f - (ty + end - y) / height};
                C2D_ImageTint tint = sTint;

                if (fade)
                {
                    bool white;
                    float bright = LayerBrightness(bg, &white);

                    FadeCorner(&tint, C2D_TopLeft, EdgeFade(x0, y, top, bottom), bright, white);
                    FadeCorner(&tint, C2D_TopRight, EdgeFade(x1, y, top, bottom), bright, white);
                    FadeCorner(&tint, C2D_BotLeft, EdgeFade(x0, end, top, bottom), bright, white);
                    FadeCorner(&tint, C2D_BotRight, EdgeFade(x1, end, top, bottom), bright, white);
                }
                ++sStats.tiles;
                C2D_DrawImageAt((C2D_Image){tex, &run}, x0 + CTR_VIEW_X + sLayerShift,
                                y + CTR_VIEW_Y, 0, &tint, 1, 1);
            }
            if (!fade) break;
        }
        y = end;
    }
    return true;
}

static void DrawAffineBg(unsigned bg)
{
    unsigned control = Reg(8 + bg * 2), base = bg == 2 ? 0x20 : 0x30;
    float a = (int16_t)Reg(base) / 256.0f, b = (int16_t)Reg(base + 2) / 256.0f;
    float c = (int16_t)Reg(base + 4) / 256.0f, d = (int16_t)Reg(base + 6) / 256.0f;
    float rx = CtrVideo_AffineReference(Reg(base + 8) | (Reg(base + 10) << 16)) / 256.0f;
    float ry = CtrVideo_AffineReference(Reg(base + 12) | (Reg(base + 14) << 16)) / 256.0f;
    float det = a * d - b * c;
    if (fabsf(det) < 0.00001f) { Error(5, "singular affine BG matrix"); return; }
    C3D_Mtx matrix;
    Mtx_Identity(&matrix);
    matrix.r[0] = FVec4_New(d / det, -b / det, 0, (b * ry - d * rx) / det + CTR_VIEW_X + sLayerShift);
    matrix.r[1] = FVec4_New(-c / det, a / det, 0, (c * rx - a * ry) / det + CTR_VIEW_Y);
    ViewAffine(&matrix);
    float minX = rx, maxX = rx, minY = ry, maxY = ry;
    for (unsigned corner = 1; corner < 4; ++corner)
    {
        float x = (corner & 1) ? sClipX1 : sClipX0;
        float y = (corner & 2) ? sClipY1 : sClipY0;
        float sx = rx + a * x + b * y, sy = ry + c * x + d * y;
        minX = fminf(minX, sx); maxX = fmaxf(maxX, sx);
        minY = fminf(minY, sy); maxY = fmaxf(maxY, sy);
    }
    int firstX = (int)floorf(minX / 8), lastX = (int)floorf(maxX / 8);
    int firstY = (int)floorf(minY / 8), lastY = (int)floorf(maxY / 8);
    unsigned tiles = 16u << (control >> 14), mask = tiles - 1;
    unsigned map = ((control >> 8) & 31) * 0x800, chars = ((control >> 2) & 3) * 0x4000;
    /*
     * A centred GBA screen fills its margins with its affine map repeated,
     * wrap-around or not: without it the space above and left of the picture
     * would be empty while the map's own sea shows below and to the right.
     */
    bool wrap = (control & 0x2000) || sCentred;
    /*
     * Under minification the screen maps to far more of the map than exists.
     * Without wrap-around nothing outside the map is drawn anyway, so clamping
     * the scan to it bounds the work by the map instead of by how small the
     * layer was scaled: the title logo shrinks enough to blow any fixed budget
     * and would disappear for those frames.
     */
    if (!wrap)
    {
        if (firstX < 0) firstX = 0;
        if (firstY < 0) firstY = 0;
        if (lastX >= (int)tiles) lastX = (int)tiles - 1;
        if (lastY >= (int)tiles) lastY = (int)tiles - 1;
    }
    if (lastX < firstX || lastY < firstY) return;
    if ((lastX - firstX + 1) * (lastY - firstY + 1) > 16384)
    { Error(6, "affine BG minification exceeds baseline budget"); return; }
    for (int y = firstY; y <= lastY; ++y)
        for (int x = firstX; x <= lastX; ++x)
        {
            if (!wrap && ((unsigned)x >= tiles || (unsigned)y >= tiles)) continue;
            unsigned entry = map + ((unsigned)y & mask) * tiles + ((unsigned)x & mask);
            if (entry >= 0x10000) { Error(0, "affine map outside BG VRAM"); continue; }
            DrawTile(chars + sMemory.vram[entry] * 64, 0, true, x * 8, y * 8, false, false);
        }
    ViewBase();
}

static CtrEffectOam sEffectOamPending[CTR_EFFECT_OAM_COUNT];
static CtrEffectOam sEffectOamShown[CTR_EFFECT_OAM_COUNT];

void CtrVideo_ClearEffectOam(void)
{
    memset(sEffectOamPending, 0, sizeof(sEffectOamPending));
}

void CtrVideo_MarkEffectOam(unsigned index, uint16_t attr0, uint16_t attr1, int x, int y)
{
    if (index < CTR_EFFECT_OAM_COUNT)
        sEffectOamPending[index] = (CtrEffectOam){x, y, attr0, attr1, true};
}

void CtrVideo_CommitEffectOam(void)
{
    memcpy(sEffectOamShown, sEffectOamPending, sizeof(sEffectOamShown));
}

static void DrawObjects(unsigned priority, bool effects)
{
    if (NativeIntroActive()) { DrawNativeIntroObjects(priority, effects); return; }
    static const uint8_t dimensions[3][4][2] = {
        {{8,8},{16,16},{32,32},{64,64}},
        {{16,8},{32,8},{32,16},{64,32}},
        {{8,16},{8,32},{16,32},{32,64}}
    };
    for (int i = 127; i >= 0; --i)
    {
        CTR_PERF_COUNT(PERF_OAM_VISITS,1);
#if CTR_VOXEL_ENABLED
        if (sVoxelWeatherOnly && !(sVoxelWeatherOam[i >> 5] & (1u << (i & 31))))
            continue;
#endif
        const uint16_t *obj = sMemory.oam + i * 4;
        unsigned attr0 = obj[0], attr1 = obj[1], attr2 = obj[2];
        bool affine = (attr0 & 0x100) != 0, color256 = (attr0 & 0x2000) != 0;
        if ((!affine && (attr0 & 0x200)) || ((attr2 >> 10) & 3) != priority) continue;
        unsigned mode = (attr0 >> 10) & 3, shape = attr0 >> 14;
        if (mode == 2) continue;
        if (mode == 3 || shape == 3) continue;
        if (attr0 & 0x1000) Error(8, "OBJ mosaic not supported");
        unsigned width = dimensions[shape][attr1 >> 14][0], height = dimensions[shape][attr1 >> 14][1];
        unsigned boxW = affine && (attr0 & 0x200) ? width * 2 : width;
        unsigned boxH = affine && (attr0 & 0x200) ? height * 2 : height;
        int x = attr1 & 511, y = attr0 & 255;
        bool effectPosition = sFieldLayers
            && CtrEffectOam_Resolve(sEffectOamShown, (unsigned)i, attr0, attr1, &x, &y);
        if (!effectPosition && (sIntro || sStage || sCentred || sBattle))
        {
            /*
             * The GBA's own reading: a sprite only comes back from the other
             * side when it crosses the end of the coordinate range. Unused
             * entries sit at y=240 x=464, which the wide view would otherwise
             * turn into a column of blank tiles at (-48, -16), in its margin.
             */
            if (x + (int)boxW > 512) x -= 512;
            if (y + (int)boxH > 256) y -= 256;
        }
        else if (!effectPosition)
        {
            x = CtrObj_UnwrapX(x, VIEW_RIGHT);
            y = CtrObj_UnwrapY(y, VIEW_BOTTOM);
#if CTR_VOXEL_ENABLED
            if (sVoxelWeatherOnly)
            {
                x += CTR_STAGE_X;
                y += CTR_STAGE_Y;
            }
#endif
        }
        if (x >= sClipX1 || x + (int)boxW <= sClipX0
         || y >= sClipY1 || y + (int)boxH <= sClipY0) continue;
        ++sStats.sprites;
        CTR_PERF_COUNT(PERF_SPRITES,1);
        uint64_t perfAffine=affine ? CtrPerf_Begin() : 0;
        CTR_PERF_COUNT(PERF_AFFINE_OBJECTS,affine);
        Blend(4, effects, mode == 1);
        ViewBase();
        if (affine)
        {
            unsigned index = ((attr1 >> 9) & 31) * 16;
            float a = (int16_t)sMemory.oam[index + 3] / 256.0f;
            float b = (int16_t)sMemory.oam[index + 7] / 256.0f;
            float c = (int16_t)sMemory.oam[index + 11] / 256.0f;
            float d = (int16_t)sMemory.oam[index + 15] / 256.0f;
            float det = a * d - b * c;
            if (fabsf(det) < 0.00001f) { Error(9, "singular OBJ affine matrix"); CtrPerf_End(PERF_OBJ_AFFINE,perfAffine); continue; }
            C3D_Mtx matrix;
            Mtx_Identity(&matrix);
            matrix.r[0] = FVec4_New(d / det, -b / det, 0, x + CTR_VIEW_X + sLayerShift + boxW / 2.0f - (d * width - b * height) / (2 * det));
            matrix.r[1] = FVec4_New(-c / det, a / det, 0, y + CTR_VIEW_Y + boxH / 2.0f - (a * height - c * width) / (2 * det));
            ViewAffine(&matrix);
        }
        bool flipX = !affine && (attr1 & 0x1000), flipY = !affine && (attr1 & 0x2000);
        for (unsigned ty = 0; ty < height / 8; ++ty)
            for (unsigned tx = 0; tx < width / 8; ++tx)
            {
                unsigned sx = flipX ? width / 8 - 1 - tx : tx;
                unsigned sy = flipY ? height / 8 - 1 - ty : ty;
                unsigned tile = CtrVideo_ObjTile(attr2 & 1023, sx, sy, width, color256, Reg(0) & 0x40);
                DrawTile(0x10000 + tile * 32, 16 + (attr2 >> 12), color256,
                         (affine ? 0 : x + CTR_VIEW_X + sLayerShift) + (int)tx * 8,
                         (affine ? 0 : y + CTR_VIEW_Y) + (int)ty * 8, flipX, flipY);
            }
        CtrPerf_End(PERF_OBJ_AFFINE,perfAffine);
    }
    ViewBase();
}

/*
 * The quest log's playback windows are BG0 windows authored for 240x160: the
 * "Previously on your quest" bar on GBA rows 0-15, the scene description box
 * on rows 112-143 and the bottom bar on rows 144-159. Every pixel row of them
 * is one pattern repeated across (the bars' fill, the box's edge lines and
 * fill) with text at the left. quest_log.c sets this while those windows
 * exist; the top bar then sits on the canvas top and the lower band on its
 * bottom, left-aligned, each continued to the right edge with its own last
 * tile column (x 232-239), 1:1.
 */
static bool sQuestLogLayout;

void CtrVideo_SetQuestLogLayout(bool layout) { sQuestLogLayout = layout; }

/* The standard map-name popup's task is alive (3ds_game_bridge.c asks FireRed every frame). Its window is BG0 map rows
 * 232-247 (map_name_popup.c: tilemapTop 29, height 2) and DrawTextBorderOuter frames it with a tile row above and one
 * below: map rows 224-255, 32 pixels (every floor variant only changes its width). The frame is part of the popup:
 * left on the ordinary path its bottom row lands at y 96-103 of the canvas, a strip across the middle. */
#define MAP_NAME_POPUP_MAP_Y 224
#define MAP_NAME_POPUP_HEIGHT 32
static bool sMapNamePopup;

void CtrVideo_SetMapNamePopup(bool active) { sMapNamePopup = active; }

/* GBA BG0 rect [gx0,gx1) x [gy0,gy1) drawn with the view at (viewX, viewY),
 * inside the native clip (x0..x1, y0..y1) being composed. */
static void DrawFieldUiRect(unsigned bg, int viewX, int viewY, int gx0, int gx1, int gy0, int gy1,
                            int x0, int x1, int y0, int y1)
{
    sViewX = viewX;
    sViewY = viewY;
    sClipX0 = x0 - viewX > gx0 ? x0 - viewX : gx0;
    sClipX1 = x1 - viewX < gx1 ? x1 - viewX : gx1;
    sClipY0 = y0 - viewY > gy0 ? y0 - viewY : gy0;
    sClipY1 = y1 - viewY < gy1 ? y1 - viewY : gy1;
    if (sClipX0 < sClipX1 && sClipY0 < sClipY1)
        DrawTextBg(bg);
}

/* BG0 keeps its GBA window allocation. Move the 240x160 UI rectangle to the
 * bottom centre without changing the field BG scroll or object coordinates. */
void CtrVideo_SetFieldMoveShowMon(bool active) { sFieldMoveShowMon = active; }

static void DrawFieldUi(unsigned bg)
{
    int x0 = sClipX0, x1 = sClipX1, y0 = sClipY0, y1 = sClipY1;
    int vx = sViewX, vy = sViewY;
    if (sFieldMoveShowMon)
    {
        /* The streak strip repeats horizontally at its own pixel scale, with
         * its 80px height centred on the same native point as the Pokemon. */
        DrawFieldUiRect(bg, 0, CTR_STAGE_Y, 0, CTR_GAME_WIDTH, 0, 160,
                        x0, x1, y0, y1);
    }
    else if (sQuestLogLayout)
    {
        static const int bands[2][3] = {{0, 16, 0}, {112, 160, CTR_GAME_HEIGHT - 48}};
        for (unsigned b = 0; b < 2; ++b)
        {
            int gy0 = bands[b][0], gy1 = bands[b][1], viewY = bands[b][2] - gy0;
            DrawFieldUiRect(bg, 0, viewY, 0, 240, gy0, gy1, x0, x1, y0, y1);
            for (int x = 240; x < CTR_GAME_WIDTH; x += 8)
                DrawFieldUiRect(bg, x - 232, viewY, 232, 240, gy0, gy1, x0, x1, y0, y1);
        }
        DrawFieldUiRect(bg, CTR_STAGE_X, CTR_GAME_HEIGHT - 160, 0, 240, 16, 112, x0, x1, y0, y1);
    }
    else if (sMapNamePopup)
    {
        /* The map-name popup is a framed window at BG0 map rows 224-255 (below the GBA screen) that FireRed scrolls
         * into view through the 256-pixel map's wrap (BG0VOFS), sliding in from the screen's top edge. Its band of
         * GBA screen rows goes to the canvas top, 1:1, as on the GBA; every other row stays where the field UI is
         * (dialogue boxes do not move). Only GBA rows 0-159 are ever drawn, so the rest of the wrapped map (the
         * popup sliding out below the GBA screen) never shows on the taller canvas. */
        int start = (MAP_NAME_POPUP_MAP_Y - (int)(Reg(0x12) & 255)) & 255;
        int a0 = start < 160 ? start : 160, a1 = start + MAP_NAME_POPUP_HEIGHT < 160 ? start + MAP_NAME_POPUP_HEIGHT : 160;
        int b1 = start + MAP_NAME_POPUP_HEIGHT - 256 > 160 ? 160 : start + MAP_NAME_POPUP_HEIGHT - 256;
        int cut[2][2] = {{0, b1 > 0 ? b1 : 0}, {a0, a1 > a0 ? a1 : a0}};   /* the popup's rows: the wrapped head, the body */
        int gy = 0;

        for (int i = 0; i < 2; ++i)
        {
            if (cut[i][0] >= cut[i][1])
                continue;
            DrawFieldUiRect(bg, CTR_STAGE_X, CTR_GAME_HEIGHT - 160, 0, 240, gy, cut[i][0], x0, x1, y0, y1);
            DrawFieldUiRect(bg, CTR_STAGE_X, 0, 0, 240, cut[i][0], cut[i][1], x0, x1, y0, y1);
            gy = cut[i][1];
        }
        DrawFieldUiRect(bg, CTR_STAGE_X, CTR_GAME_HEIGHT - 160, 0, 240, gy, 160, x0, x1, y0, y1);
    }
    else
        /* The transition's own BG0 (its tile patterns) is centred on the canvas, like the GBA's centre under the player;
         * the field's text boxes sit at the bottom. */
        DrawFieldUiRect(bg, CTR_STAGE_X, sBattleTransition ? CTR_STAGE_Y : CTR_GAME_HEIGHT - 160, 0, 240, 0, 160, x0, x1, y0, y1);
    sViewX = vx; sViewY = vy;
    sClipX0 = x0; sClipX1 = x1; sClipY0 = y0; sClipY1 = y1;
    ViewBase();
}

/* GBA BG mosaic samples the top-left screen pixel of each WxH group, before
 * scroll/affine lookup. A one-pixel subtexture stretched over that group keeps
 * palette, alpha, layer tint and GPU composition identical to normal tiles.
 * The ordinary tile path remains in use when MOSAIC encodes 1x1. */
static void DrawMosaicBg(unsigned bg, bool affine)
{
    unsigned control = Reg(8 + bg * 2), mosaic = Reg(0x4c);
    int mx = (mosaic & 15) + 1, my = ((mosaic >> 4) & 15) + 1;
    unsigned map = ((control >> 8) & 31) * 0x800;
    unsigned chars = ((control >> 2) & 3) * 0x4000;
    unsigned size = control >> 14;
    bool color256 = (control & 128) != 0;
    unsigned mapW = (size & 1) ? 64 : 32;
    unsigned mapH = (size & 2) ? 64 : 32;
    float a = 0, b = 0, c = 0, d = 0, rx = 0, ry = 0;
    if (affine)
    {
        unsigned reg = bg == 2 ? 0x20 : 0x30;
        a = (int16_t)Reg(reg) / 256.0f;
        b = (int16_t)Reg(reg + 2) / 256.0f;
        c = (int16_t)Reg(reg + 4) / 256.0f;
        d = (int16_t)Reg(reg + 6) / 256.0f;
        rx = CtrVideo_AffineReference(Reg(reg + 8) | (Reg(reg + 10) << 16)) / 256.0f;
        ry = CtrVideo_AffineReference(Reg(reg + 12) | (Reg(reg + 14) << 16)) / 256.0f;
        mapW = mapH = 16u << size;
        color256 = true;
    }
    ViewBase();
    for (int y = CtrVideo_MosaicOrigin(sClipY0, my) - my; y < sClipY1; y += my)
    for (int x = CtrVideo_MosaicOrigin(sClipX0, mx) - mx; x < sClipX1; x += mx)
    {
        int sx = x, sy = y;
        if (sIntro && (sIntroBackgrounds & (1u << bg)))
        {
            IntroSpan(x, x + mx, 0, 240, sIntroRepeat & (1u << bg), &sx);
            IntroSpan(y, y + my, sIntroTop, sIntroBottom, false, &sy);
        }
        unsigned entry, index, tx, ty, address;
        int slot;
        if (affine)
        {
            sx = (int)floorf(rx + a * x + b * y);
            sy = (int)floorf(ry + c * x + d * y);
            if (!(control & 0x2000) && !sCentred &&
                (sx < 0 || sy < 0 || sx >= (int)mapW * 8 || sy >= (int)mapH * 8)) continue;
            sx &= (int)mapW * 8 - 1;
            sy &= (int)mapH * 8 - 1;
            entry = sMemory.vram[map + (sy / 8) * mapW + sx / 8];
        }
        else
        {
            sx = (sx + (Reg(0x10 + bg * 4) & 511)) & ((int)mapW * 8 - 1);
            sy = (sy + (Reg(0x12 + bg * 4) & 511)) & ((int)mapH * 8 - 1);
            entry = Read16(map + CtrVideo_TextMapOffset(sx / 8, sy / 8, size));
        }
        index = affine ? entry : entry & 1023;
        address = chars + index * (color256 ? 64 : 32);
        if (!BgCharacterAddressValid(bg, control, map, chars, entry, address)) continue;
        slot = GetTileSlot(address, entry >> 12, color256);
        if (slot < 0) continue;
        tx = (unsigned)sx & 7;
        ty = (unsigned)sy & 7;
        if (!affine)
        {
            if (entry & 0x400) tx = 7 - tx;
            if (entry & 0x800) ty = 7 - ty;
        }
        if (color256 ? !sTiles[slot].bytes[ty * 8 + tx]
                     : !(sTiles[slot].bytes[ty * 4 + tx / 2] >> ((tx & 1) * 4) & 15)) continue;
        unsigned atlasX = (unsigned)slot % (ATLAS_SIZE / 8) * 8 + tx;
        unsigned atlasY = (unsigned)slot / (ATLAS_SIZE / 8) * 8 + ty;
        Tex3DS_SubTexture sub = {1, 1, atlasX / (float)ATLAS_SIZE,
            1.0f - atlasY / (float)ATLAS_SIZE,
            (atlasX + 1) / (float)ATLAS_SIZE,
            1.0f - (atlasY + 1) / (float)ATLAS_SIZE};
        if (++sStats.tiles >= MAX_DRAWS - 32) { Error(2, "GPU geometry budget exceeded"); return; }
        C2D_DrawImageAt((C2D_Image){&sAtlas, &sub}, x + CTR_VIEW_X + sLayerShift,
                        y + CTR_VIEW_Y, 0, &sTint, mx, my);
    }
}

/*
 * Objects keep the GBA's screen, because the intro parks and recycles its sprites just past its edges. The Game Freak
 * scene is the exception: its star starts past the right edge and its sparkles fly off it, and on the wider canvas
 * that is part of the picture, so they are drawn wherever they are, at their own size and speed.
 */
static void IntroObjectSpan(int *left, int *right)
{
    bool wide = sNativeIntroScene == CTR_INTRO_GF;

    *left = wide ? VIEW_LEFT : 0;
    *right = wide ? VIEW_RIGHT : 240;
}

static bool IntroClipLayer(int bg, bool scenery)
{
    int left = 0, right;

    if (bg >= 0)
        right = IntroMapRight(bg);
    else
        IntroObjectSpan(&left, &right);
    if (!sIntro || scenery || NativeIntroActive() || (bg >= 0 && TitleComposed(bg))) return false;
    if (sClipX0 < left) sClipX0 = left;
    if (sClipX0 > right) sClipX0 = right;
    if (sClipX1 < left) sClipX1 = left;
    if (sClipX1 > right) sClipX1 = right;
    if (sClipY0 < 0) sClipY0 = 0;
    if (sClipY0 > 160) sClipY0 = 160;
    if (sClipY1 < 0) sClipY1 = 0;
    if (sClipY1 > 160) sClipY1 = 160;
    C2D_Flush();
    Scissor(sClipX0, sClipY0, sClipX1, sClipY1);
    return true;
}

static void Layers(unsigned mask)
{
    unsigned display = Reg(0), mode = display & 7;
    mask &= ~sLayerExclude;
    if (mode > 2) { Error(10, "bitmap display modes are not part of baseline"); return; }
    for (int priority = 3; priority >= 0; --priority)
    {
        /* One depth plane at a time while composing them separately. */
        if (!(sPriorityMask & (1u << priority))) continue;
        /* Priority is the depth scale: 3 stays at the screen plane. */
        sLayerShift = (sLayerOrigin + sParallax * (3 - priority)) / sShiftZoom;
        for (int bg = 3; bg >= 0; --bg)
        {
            if (!(mask & (1u << bg)) || !(display & (0x100u << bg)) || (Reg(8 + bg * 2) & 3) != (unsigned)priority) continue;
            if ((mode == 1 && bg == 3) || (mode == 2 && bg < 2)) continue;
            CTR_PERF_COUNT(PERF_BG_LAYERS, 1);
            Blend(bg, mask & 32, false);
            /* Which stage a slow frame is in: the layer walk, the sprites or
             * the GPU. Guessing that from fps alone costs a hardware run. */
            uint64_t start = svcGetSystemTick();
            int clipX0 = sClipX0, clipX1 = sClipX1, clipY0 = sClipY0, clipY1 = sClipY1;
            bool clipped = IntroClipLayer(bg, sIntroBackgrounds & (1u << bg));
            if (sClipX0 < sClipX1 && sClipY0 < sClipY1)
            {
                if (DrawNativeIntroBg(bg)) {}
                else if ((Reg(8 + bg * 2) & 0x40) && (Reg(0x4c) & 0xff))
                    DrawMosaicBg(bg, (mode == 1 && bg == 2) || mode == 2);
                else if ((mode == 1 && bg == 2) || mode == 2) DrawAffineBg(bg);
                else if (sBattle && bg == 0) DrawBattleTextLayer(bg);
                else if (sFieldLayers && bg == 0) DrawFieldUi(bg);
                else if (sStageUi)
                {
                    if (bg == 1) DrawOakSurround();
                    DrawStageUiText(bg);
                }
                else if (DrawTitleBand(bg)) {}
                else if (!DrawLineBg(bg) && !DrawIntroBgTex(bg) && !DrawFieldBgTex(bg) && !DrawStageBgTex(bg)) DrawTextBg(bg);
            }
            if (clipped)
            {
                C2D_Flush();
                sClipX0 = clipX0; sClipX1 = clipX1;
                sClipY0 = clipY0; sClipY1 = clipY1;
                RestoreScissor();
            }
            uint64_t elapsed=svcGetSystemTick()-start;
            sBgTicks += elapsed;
            if (gCtrPerf.enabled)
            {
                gCtrPerf.ticks[PERF_BG] += elapsed;
                if ((mode == 1 && bg == 2) || mode == 2) gCtrPerf.ticks[PERF_BG_AFFINE] += elapsed;
            }
        }
        if ((mask & 16) && (display & 0x1000))
        {
            uint64_t start = svcGetSystemTick();
            int clipX0 = sClipX0, clipX1 = sClipX1, clipY0 = sClipY0, clipY1 = sClipY1;
            bool clipped = IntroClipLayer(-1, false);
            if (sClipX0 < sClipX1 && sClipY0 < sClipY1) DrawObjects(priority, mask & 32);
            if (clipped)
            {
                C2D_Flush();
                sClipX0 = clipX0; sClipX1 = clipX1;
                sClipY0 = clipY0; sClipY1 = clipY1;
                RestoreScissor();
            }
            uint64_t elapsed=svcGetSystemTick()-start;
            sObjTicks += elapsed;
            if (gCtrPerf.enabled) gCtrPerf.ticks[PERF_OBJ] += elapsed;
        }
    }
}

/*
 * A window edge is an 8-bit field, so 255 is the largest value the game can
 * write. On a 240x160 screen that already meant "past the edge"; on this
 * viewport it has to keep meaning the edge of the screen, or everything beyond
 * x=255 falls outside every window. The overworld sets WIN0H=0x00FF with
 * WININ showing all layers and WINOUT showing only the empty text layer, which
 * is exactly the black band on the right of the map.
 *
 * A window whose first edge is also saturated is empty on hardware (WIN1H is
 * set to 0xFFFF for that purpose) and stays empty here.
 */
static int WindowEdge(unsigned limits, bool last, unsigned extent)
{
    unsigned first = limits >> 8, end = limits & 255;

    if (last)
        return (end >= 255 && first < 255) ? (int)extent : (int)Min(end, extent);
    return (int)Min(first, extent);
}

/*
 * One axis of a window, in GBA coordinates. A stage keeps the GBA's own
 * reading of the registers (an end past the screen, or before the start, means
 * the screen edge), and a window that reaches an edge of the GBA screen reaches
 * the edge of the stage: that is what the game meant by it, and the letterbox
 * of the intro would otherwise leave its margins outside every window.
 */
/*
 * The battle intro opens its scene through window 0: a band across the whole
 * screen, with nothing shown outside it, growing from the middle line to the
 * full height. That is a curtain over the whole picture, so on the battle
 * screen it keeps the picture's proportions like the stage's letterbox does;
 * any other battle window (a spotlight, a move's band around a battler) marks
 * a place in the scene and stays 1:1.
 */
static bool BattleCurtain(void)
{
    unsigned across = Reg(0x40);

    return sBattle && sZoom == 1.0f && (Reg(0) & 0x2000) && !(Reg(0x4a) & 63)
        && (across >> 8) == 0 && (across & 255) >= 240;
}

/*
 * What an intro window is for. A wipe or a curtain covers the display: the title's wipe is a window of the whole height
 * whose side moves across, the Game Freak scene's curtain a window of the whole width whose top and bottom open. Those
 * are presentation effects, so the edge that moves crosses the whole canvas. Any other window is a local mask of the
 * scene (the cinematic scene's half-width window): it stays in the GBA's own geometry on the stage, and only an edge
 * that sits on the GBA screen's edge goes on to the canvas's. The art under either is never touched.
 */
static bool IntroWindowCurtain(unsigned h, unsigned v)
{
    return ((h >> 8) == 0 && (h & 255) >= 240) || ((v >> 8) == 0 && (v & 255) >= 160);
}

/* Of window w: a wipe or a curtain (see above), for the intro and for the battle transition alike. */
static bool WindowCurtain(unsigned w)
{
    return IntroWindowCurtain(Reg(0x40 + 2 * w), Reg(0x44 + 2 * w));
}

static void WindowSpan(unsigned limits, bool vertical, bool curtain, int *first, int *last)
{
    /* A native GBA screen reads its window registers exactly as hardware. */
    bool gba = sIntro || sStage || sBattle || sNative;
    unsigned extent = gba ? (vertical ? 160u : 240u)
                          : (vertical ? (unsigned)CTR_GAME_HEIGHT : (unsigned)CTR_GAME_WIDTH);

    if (sFieldLayers && sFieldMoveShowMon)
    {
        unsigned size = vertical ? 160u : 240u;
        int a = WindowEdge(limits, false, size), b = WindowEdge(limits, true, size);
        if (a >= b)
        {
            *first = *last = vertical ? a + CTR_STAGE_Y
                                     : (a * CTR_GAME_WIDTH + 120) / 240;
        }
        else if (!vertical)
        {
            *first = (a * CTR_GAME_WIDTH + 120) / 240;
            *last = (b * CTR_GAME_WIDTH + 120) / 240;
        }
        else if (a == 0 && b >= 160)
        {
            *first = 0;
            *last = CTR_GAME_HEIGHT;
        }
        else
        {
            *first = a + CTR_STAGE_Y;
            *last = b + CTR_STAGE_Y;
        }
        return;
    }
    /* Only the field's barn-door task uses normalized 0..240 horizontal bounds.
     * Decode both edges across the native width, including empty edge windows;
     * other field effects keep their existing 1:1 register interpretation. */
    if (sFieldLayers && sFieldBarnDoorWipe && !vertical) {
        *first = ((int)Min(limits >> 8, 240u) * CTR_GAME_WIDTH + 120) / 240;
        *last = ((int)Min(limits & 255, 240u) * CTR_GAME_WIDTH + 120) / 240;
        return;
    }
    /*
     * FireRed's battle transitions are authored for the 240x160 display, and their windows say where on it. A wipe or a
     * curtain (a window of the whole width or the whole height) stands for the physical display: its edges are placed over
     * the whole canvas (0..240 -> 0..400, 0..160 -> 0..240, the centre 120 -> 200, 80 -> 120). Any other window is local to
     * the scene and keeps the GBA's own geometry, centred on the canvas like the GBA's centre; an edge on the GBA's edge
     * goes on to the canvas's. An empty window stays empty. Only the effect's geometry is mapped: the field under it, its
     * camera, objects and tiles are the native canvas, untouched.
     */
    if (sFieldLayers && sBattleTransition)
    {
        int size = vertical ? 160 : 240, canvas = vertical ? CTR_GAME_HEIGHT : CTR_GAME_WIDTH;
        int origin = vertical ? CTR_STAGE_Y : CTR_STAGE_X;
        int a = WindowEdge(limits, false, (unsigned)size), b = WindowEdge(limits, true, (unsigned)size);

        if ((limits >> 8) > (limits & 255) && (limits & 255) < 255) b = a;
        if (a >= b)
        {
            *first = *last = (a * canvas + size / 2) / size;
        }
        else if (curtain)
        {
            *first = (a * canvas + size / 2) / size;
            *last = (b * canvas + size / 2) / size;
        }
        else
        {
            *first = a == 0 ? 0 : a + origin;
            *last = b >= size ? canvas : b + origin;
        }
        return;
    }
    *first = WindowEdge(limits, false, extent);
    *last = WindowEdge(limits, true, extent);
    /* Field effects such as Surf and Fly restore the overworld's full-screen
     * mask as 0..240/0..160 rather than its usual 0..255/0..255. On the GBA
     * both cover the whole display. Keep that meaning for the native field;
     * otherwise WINOUT (BG0 only) blacks out the new right and bottom area
     * until the next map load resets WIN0 to 0..255. */
    if (sFieldLayers && *first == 0
        && (limits & 255) >= (vertical ? 160u : 240u))
        *last = (int)extent;
    if (!gba) return;
    if ((limits & 255) > extent || (limits >> 8) > (limits & 255)) *last = (int)extent;
    if (*first >= *last) return;
    if (NativeIntroActive() && vertical)
    {
        if (NativeIntroCinematic())
        {
            *first = (*first - 32) * 240 / 96;
            *last = (*last - 32) * 240 / 96;
        }
        else
        {
            *first = *first * 240 / 160;
            *last = *last * 240 / 160;
        }
        if (*first < 0) *first = 0;
        if (*last > 240) *last = 240;
        return;
    }
    if (sIntro && !vertical && curtain)
    {
        *first = VIEW_LEFT + *first * (VIEW_RIGHT - VIEW_LEFT) / (int)extent;
        *last = VIEW_LEFT + *last * (VIEW_RIGHT - VIEW_LEFT) / (int)extent;
        return;
    }
    if (vertical && sBattle && !BattleCurtain())
    {
        if (*first == 0) *first = VIEW_TOP;
        if (*last >= (int)extent) *last = VIEW_BOTTOM;
        return;
    }
    if (vertical)
    {
        /*
         * Vertically the stage keeps the picture's proportions: the intro's
         * cinematic bars close in to 32 lines of 160, and on a 240-line screen
         * that has to be 48 lines of 240, or the scene between them is a thin
         * strip in the middle of a black screen.
         */
        *first = VIEW_TOP + *first * (VIEW_BOTTOM - VIEW_TOP) / (int)extent;
        *last = VIEW_TOP + *last * (VIEW_BOTTOM - VIEW_TOP) / (int)extent;
        return;
    }
    if (*first == 0) *first = VIEW_LEFT;
    if (*last >= (int)extent) *last = VIEW_RIGHT;
}

static bool Inside(int p, unsigned limits, bool vertical, bool curtain)
{
    int first, last;

    WindowSpan(limits, vertical, curtain, &first, &last);
    /* Same reading of an empty window as the partition uses. */
    return first < last && p >= first && p < last;
}

/*
 * The OBJ window mask depends only on the window objects' attributes and
 * matrices, the mosaic and OBJ mapping registers and OBJ VRAM. A battle move's
 * mask (the stat change's copy of the battler) holds still for its whole
 * animation, and rebuilding it per pixel cost ~8 ms a frame; it is rebuilt
 * only when one of those inputs changed. Returns whether it changed. The
 * native title draws its slash mask from the window actors instead, so there
 * they are the inputs.
 */
#define OBJ_WINDOW_SIG (2 + 128 * 8)
static uint16_t sObjWindowSig[OBJ_WINDOW_SIG + 2];
static unsigned sObjWindowSigLength;
static uint8_t sObjWindowVram[0x8000];
static bool sObjWindowBuilt;

static bool BuildObjWindow(void)
{
    uint64_t perfObjWindow = CtrPerf_Begin();
    static const uint8_t dimensions[3][4][2] = {
        {{8,8},{16,16},{32,32},{64,64}},
        {{16,8},{32,8},{32,16},{64,32}},
        {{8,16},{8,32},{16,32},{32,64}}
    };
    uint16_t sig[OBJ_WINDOW_SIG + 2];
    unsigned length = 0;
    /* OBJ VRAM the window objects read: their tile runs with 1D mapping
     * (FireRed's), all of it with 2D. Comparing 32 KiB a frame cost ~1.5 ms. */
    unsigned vramLo = 0x8000, vramHi = 0;
    bool title = NativeIntroTitle();
    sig[length++] = (uint16_t)(title ? 0xffff : Reg(0) & 0x40);
    sig[length++] = (uint16_t)Reg(0x4c);
    for (unsigned i = 0; title && i < sNativeIntroCount; ++i)
        if (sNativeIntroActors[i].mode == 2 && sNativeIntroActors[i].tag == 3)
        {
            sig[length++] = (uint16_t)sNativeIntroActors[i].x;
            sig[length++] = (uint16_t)sNativeIntroActors[i].y;
        }
    for (unsigned i = 0; !title && i < 128; ++i)
    {
        const uint16_t *obj = sMemory.oam + i * 4;
        if (((obj[0] >> 10) & 3) != 2) continue;
        sig[length++] = (uint16_t)i;
        sig[length++] = obj[0];
        sig[length++] = obj[1];
        sig[length++] = obj[2];
        if (Reg(0) & 0x40)
        {
            unsigned shape = obj[0] >> 14 < 3 ? obj[0] >> 14 : 0;
            unsigned pixels = dimensions[shape][obj[1] >> 14][0] * dimensions[shape][obj[1] >> 14][1];
            unsigned lo = (obj[2] & 1023) * 32, hi = lo + (obj[0] & 0x2000 ? pixels : pixels / 2);
            if (lo < vramLo) vramLo = lo;
            if (hi > vramHi) vramHi = hi;
        }
        else
        {
            vramLo = 0;
            vramHi = 0x8000;
        }
        if (obj[0] & 0x100)
        {
            unsigned matrix = ((obj[1] >> 9) & 31) * 16;
            sig[length++] = sMemory.oam[matrix + 3];
            sig[length++] = sMemory.oam[matrix + 7];
            sig[length++] = sMemory.oam[matrix + 11];
            sig[length++] = sMemory.oam[matrix + 15];
        }
    }
    if (vramHi > 0x8000) vramHi = 0x8000;
    if (vramHi < vramLo) vramHi = vramLo;
    sig[length++] = (uint16_t)(vramLo >> 1);
    sig[length++] = (uint16_t)(vramHi >> 1);
    if (sObjWindowBuilt && length == sObjWindowSigLength
        && memcmp(sig, sObjWindowSig, length * sizeof(*sig)) == 0
        && memcmp(sObjWindowVram + vramLo, sMemory.vram + 0x10000 + vramLo, vramHi - vramLo) == 0)
    {
        CtrPerf_End(PERF_OBJ_WINDOW, perfObjWindow);
        return false;
    }
    memcpy(sObjWindowSig, sig, length * sizeof(*sig));
    sObjWindowSigLength = length;
    memcpy(sObjWindowVram + vramLo, sMemory.vram + 0x10000 + vramLo, vramHi - vramLo);
    sObjWindowBuilt = true;
    memset(sObjWindow, 0, sizeof(sObjWindow));
    memset(sObjWindowRow, 0, sizeof(sObjWindowRow));
    if (title)
    {
        NativeIntroObjWindow();
        CtrPerf_End(PERF_OBJ_WINDOW, perfObjWindow);
        return true;
    }
    for (unsigned i = 0; i < 128; ++i)
    {
        const uint16_t *obj = sMemory.oam + i * 4;
        unsigned a0 = obj[0], a1 = obj[1], a2 = obj[2];
        unsigned shape = a0 >> 14;
        bool affine = (a0 & 0x100) != 0;
        if (((a0 >> 10) & 3) != 2 || shape == 3 || (!affine && (a0 & 0x200)))
            continue;
        int width = dimensions[shape][a1 >> 14][0];
        int height = dimensions[shape][a1 >> 14][1];
        int boxW = affine && (a0 & 0x200) ? width * 2 : width;
        int boxH = affine && (a0 & 0x200) ? height * 2 : height;
        int x = a1 & 511, y = a0 & 255;
        if (x + boxW > 512) x -= 512;
        if (y + boxH > 256) y -= 256;
        float pa = 1, pb = 0, pc = 0, pd = 1;
        if (affine)
        {
            unsigned matrix = ((a1 >> 9) & 31) * 16;
            pa = (int16_t)sMemory.oam[matrix + 3] / 256.0f;
            pb = (int16_t)sMemory.oam[matrix + 7] / 256.0f;
            pc = (int16_t)sMemory.oam[matrix + 11] / 256.0f;
            pd = (int16_t)sMemory.oam[matrix + 15] / 256.0f;
        }
        int left = x < 0 ? 0 : x, top = y < 0 ? 0 : y;
        int right = x + boxW > 240 ? 240 : x + boxW;
        int bottom = y + boxH > 160 ? 160 : y + boxH;
        unsigned mosaicX = ((Reg(0x4c) >> 8) & 15) + 1;
        unsigned mosaicY = ((Reg(0x4c) >> 12) & 15) + 1;
        if (right>left && bottom>top) CTR_PERF_COUNT(PERF_WINDOW_PIXELS,(right-left)*(bottom-top));
        for (int py = top; py < bottom; ++py)
        for (int px = left; px < right; ++px)
        {
            int dx = px - x, dy = py - y;
            if (a0 & 0x1000)
            {
                dx -= dx % (int)mosaicX;
                dy -= dy % (int)mosaicY;
            }
            int sx, sy;
            if (affine)
            {
                sx = (int)floorf(pa * (dx - boxW / 2.0f) + pb * (dy - boxH / 2.0f) + width / 2.0f);
                sy = (int)floorf(pc * (dx - boxW / 2.0f) + pd * (dy - boxH / 2.0f) + height / 2.0f);
            }
            else
            {
                sx = (a1 & 0x1000) ? width - 1 - dx : dx;
                sy = (a1 & 0x2000) ? height - 1 - dy : dy;
            }
            if (sx >= 0 && sy >= 0 && sx < width && sy < height
             && CtrVideo_ObjOpaque(sMemory.vram + 0x10000, a2 & 1023,
                                   (unsigned)sx, (unsigned)sy, (unsigned)width,
                                   (a0 & 0x2000) != 0, (Reg(0) & 0x40) != 0))
            {
                sObjWindow[py][px] = 1;
                sObjWindowRow[py] = true;
            }
        }
    }
    CtrPerf_End(PERF_OBJ_WINDOW, perfObjWindow);
    return true;
}

typedef struct { int x0, x1; unsigned mask; } WindowRun;

/*
 * The window registers and the view do not change while a frame is composed
 * (layer drawing restores any view it borrows), so each window's spans are
 * resolved once per frame instead of once per pixel. Per pixel the mask is
 * still WIN0 > WIN1 > OBJ window > outside, read exactly as Inside() does.
 */
typedef struct {
    bool on[2], objOn;
    int x0[2], x1[2], y0[2], y1[2];
    unsigned inside[2], obj, outside;
} WindowSpans;

static void WindowSpansRead(unsigned display, WindowSpans *spans)
{
    memset(spans, 0, sizeof(*spans)); /* compared with memcmp */
    for (unsigned w = 0; w < 2; ++w)
    {
        spans->on[w] = (display & (0x2000u << w)) != 0;
        WindowSpan(Reg(0x40 + 2*w), false, WindowCurtain(w), &spans->x0[w], &spans->x1[w]);
        WindowSpan(Reg(0x44 + 2*w), true, WindowCurtain(w), &spans->y0[w], &spans->y1[w]);
        spans->inside[w] = (Reg(0x48) >> (8*w)) & 63;
    }
    spans->objOn = (display & 0x8000) != 0;
    spans->obj = (Reg(0x4a) >> 8) & 63;
    spans->outside = Reg(0x4a) & 63;
}

static unsigned WindowMaskOf(const WindowSpans *spans, const bool row[2],
                             const uint8_t *obj, int x)
{
    if (row[0] && x >= spans->x0[0] && x < spans->x1[0])
        return spans->inside[0];
    if (row[1] && x >= spans->x0[1] && x < spans->x1[1])
        return spans->inside[1];
    if (obj && x >= 0 && x < (NativeIntroTitle() ? 400 : 240) && obj[x])
        return spans->obj;
    return spans->outside;
}

static unsigned WindowRunAdd(WindowRun *runs, unsigned count, int x0, int x1, unsigned mask)
{
    if (count && runs[count - 1].mask == mask)
        runs[count - 1].x1 = x1;
    else
        runs[count++] = (WindowRun){x0, x1, mask};
    return count;
}

/*
 * Maximal runs of one mask along row y, left to right. Without an OBJ
 * window pixel on the row the mask can only change at a WIN0/WIN1 edge, so
 * the row is classified per segment between edges (the title redraws the
 * frame once per 3D depth plane, three partitions a frame); otherwise per
 * pixel. Both give the same runs.
 */
static uint16_t sFlashWindow[160];
static unsigned sFlashWindowCount;

/* These bounds describe a screen-wide effect in 240x160 units, not a
 * local field rectangle. Saturated endpoints reach the native screen edge. */
static void FlashWindowSpan(unsigned limits, int *first, int *last)
{
    unsigned a = Min(limits >> 8, 240u), b = Min(limits & 255, 240u);
    *first = (int)((a * CTR_GAME_WIDTH + 120) / 240);
    *last = b > a ? (int)((b * CTR_GAME_WIDTH + 120) / 240) : *first;
}

static unsigned WindowRuns(int y, const WindowSpans *spans, int left, int right, WindowRun *runs)
{
    WindowSpans flashSpans;
    if (sFieldLayers && sFlashWindowCount && spans->on[0]) {
        unsigned line = (unsigned)(y > 0 ? y : 0) * 160 / CTR_GAME_HEIGHT;
        if (line >= sFlashWindowCount) line = sFlashWindowCount - 1;
        flashSpans = *spans;
        FlashWindowSpan(sFlashWindow[line], &flashSpans.x0[0], &flashSpans.x1[0]);
        spans = &flashSpans;
    }
    bool row[2];
    for (unsigned w = 0; w < 2; ++w)
        row[w] = spans->on[w] && y >= spans->y0[w] && y < spans->y1[w];
    const uint8_t *obj = spans->objOn && y >= 0 && y < (NativeIntroTitle() ? 240 : 160) && sObjWindowRow[y] ? sObjWindow[y] : NULL;
    unsigned count = 0;
    if (obj)
    {
        for (int x = left; x < right; ++x)
            count = WindowRunAdd(runs, count, x, x + 1, WindowMaskOf(spans, row, obj, x));
        return count;
    }
    int cuts[6];
    unsigned n = 0;
    cuts[n++] = left;
    for (unsigned w = 0; w < 2; ++w)
    {
        if (!row[w]) continue;
        if (spans->x0[w] > left && spans->x0[w] < right) cuts[n++] = spans->x0[w];
        if (spans->x1[w] > left && spans->x1[w] < right) cuts[n++] = spans->x1[w];
    }
    for (unsigned i = 1; i < n; ++i)
        for (unsigned j = i; j > 0 && cuts[j] < cuts[j - 1]; --j)
        { int t = cuts[j]; cuts[j] = cuts[j - 1]; cuts[j - 1] = t; }
    cuts[n] = right;
    for (unsigned i = 0; i < n; ++i)
        if (cuts[i] < cuts[i + 1])
            count = WindowRunAdd(runs, count, cuts[i], cuts[i + 1],
                                 WindowMaskOf(spans, row, NULL, cuts[i]));
    return count;
}

/*
 * The OBJ window partition: maximal runs per row, and adjacent rows with the
 * same runs sharing a band. Every rectangle goes to emit, band by band from
 * the top and run by run from the left, together tiling the whole view.
 */
typedef void (*WindowEmit)(int x0, int y0, int x1, int y1, unsigned mask);

static void WindowPartition(const WindowSpans *spans, WindowEmit emit)
{
    WindowRun rows[2][401];
    int left = VIEW_LEFT, right = VIEW_RIGHT, bottom = VIEW_BOTTOM;
    int y = VIEW_TOP;
    unsigned current = 0, count = y < bottom ? WindowRuns(y, spans, left, right, rows[0]) : 0;
    /* Each row's runs are computed once; equal adjacent rows share a band. */
    while (y < bottom)
    {
        const WindowRun *runs = rows[current];
        WindowRun *next = rows[current ^ 1];
        unsigned n = 0;
        int end = y + 1;
        while (end < bottom)
        {
            n = WindowRuns(end, spans, left, right, next);
            if (n != count || memcmp(runs, next, n * sizeof(*runs)) != 0) break;
            ++end;
        }
        for (unsigned r = 0; r < count; ++r)
            emit(runs[r].x0, y, runs[r].x1, end, runs[r].mask);
        y = end;
        current ^= 1;
        count = n;
    }
}

/* Scissor path: the layers of each rectangle, drawn under its scissor. */
static void WindowDrawRect(int x0, int y0, int x1, int y1, unsigned mask)
{
    C2D_Flush();
    Scissor(x0, y0, x1, y1);
    sScissored = true;
    sClipX0 = x0; sClipX1 = x1;
    sClipY0 = y0; sClipY1 = y1;
    Layers(mask);
}

/*
 * Stencil path. A window shape that is not a rectangle (the title logo's
 * shine, a battle move's OBJ window sprite) changes the runs on nearly every
 * row, and the scissor path walked every layer and sprite once per band and
 * run: hundreds of scene walks a frame. The same rectangles instead write
 * their mask into a stencil buffer, and every distinct mask walks its layers
 * once, stencil-tested to exactly the pixels its rectangles cover. Each pixel
 * still receives only its own mask's layers, drawn in the same order, over
 * the same destination.
 */
#define WINDOW_RECTS 4096
typedef struct { int x0, y0, x1, y1; unsigned mask; } WindowRect;
static WindowRect sWindowRects[WINDOW_RECTS];
static unsigned sWindowRectCount;
/* Bumped whenever sWindowRects is rebuilt. A stencil buffer is only touched
 * by ComposeStencil, so one already marked with the current rectangles keeps
 * its marks and the mark pass is skipped (an OBJ window held still). */
static uint32_t sWindowRectSerial = 1;
static void *sStencilBuf;
static uint32_t sStencilMarked;
static bool sStencilFailed;
/*
 * The battle scene is composed into its own 1024x256 surface (sScene), which
 * the logical surface's stencil cannot serve: a battle move's OBJ window
 * (the stat change mask) fell back to scissor bands there, ~48 rectangles each
 * walking every layer and sprite - ~100 ms frames. The scene gets a stencil of
 * its own size, made with it and released with it (SceneRelease).
 */
static void *sSceneStencilBuf;
static uint32_t sSceneStencilMarked;
static bool sSceneStencilFailed;
/* The scene stencil is retried (a battle must not stay on the slow scissor bands for want of VRAM at one
 * instant), and a failure asks ScenePrepare to give back what the battle cannot use. */
static uint32_t sSceneStencilRetryFrame;
static bool sMakeRoomAsked;

static bool StencilTarget(void)
{
    if (sDrawTarget && sDrawTarget == sScene) return true;
    return sDrawTarget && sDrawTarget->frameBuf.width == 512
                       && sDrawTarget->frameBuf.height == sPresentation.textureHeight;
}

static void *StencilBuffer(void)
{
    return sDrawTarget && sDrawTarget == sScene ? sSceneStencilBuf : sStencilBuf;
}

static void WindowCollectRect(int x0, int y0, int x1, int y1, unsigned mask)
{
    if (sWindowRectCount < WINDOW_RECTS)
        sWindowRects[sWindowRectCount] = (WindowRect){x0, y0, x1, y1, mask};
    ++sWindowRectCount;
}

static bool StencilReady(void)
{
    if (sDrawTarget && sDrawTarget == sScene)
    {
        if (sSceneStencilBuf) return true;
        if (sSceneStencilFailed && sStats.frames < sSceneStencilRetryFrame) return false;
        bool retry = sSceneStencilFailed;
        sSceneStencilMarked = 0;
        sSceneStencilBuf = vramAlloc(C3D_CalcDepthBufSize(sScene->frameBuf.width, sScene->frameBuf.height,
                                                          GPU_RB_DEPTH24_STENCIL8));
        sSceneStencilFailed = sSceneStencilBuf == NULL;
        if (sSceneStencilFailed)
        {
            sSceneStencilRetryFrame = sStats.frames + 90;
            sMakeRoomAsked = true;
        }
        if (!sSceneStencilFailed || !retry)
            CtrLog_Write(CTR_LOG_VIDEO, sSceneStencilBuf ? "battle scene stencil ready: %ux%u (VRAM free=%lu)"
                                                         : "battle scene stencil unavailable %ux%u (VRAM free=%lu): scissor bands",
                         (unsigned)sScene->frameBuf.width, (unsigned)sScene->frameBuf.height,
                         (unsigned long)vramSpaceFree());
        return sSceneStencilBuf != NULL;
    }
    if (sStencilBuf) return true;
    if (sStencilFailed) return false;
    sStencilMarked = 0;
    sStencilBuf = vramAlloc(C3D_CalcDepthBufSize(512, sPresentation.textureHeight, GPU_RB_DEPTH24_STENCIL8));
    if (!sStencilBuf)
    {
        sStencilFailed = true;
        CtrLog_Write(CTR_LOG_VIDEO, "OBJ window stencil unavailable: scissor bands");
    }
    else
        CtrLog_Write(CTR_LOG_VIDEO, "window stencil ready: 512x%d (VRAM free=%lu)",
                     sPresentation.textureHeight, (unsigned long)vramSpaceFree());
    return sStencilBuf != NULL;
}

/* The target pixels Scissor(x0, y0, x1, y1) keeps, as a Citro2D rectangle. */
static void StencilRect(int x0, int y0, int x1, int y1)
{
    float sx0 = (x0 + CTR_VIEW_X) * sZoom + sOffX, sx1 = (x1 + CTR_VIEW_X) * sZoom + sOffX;
    float sy0 = (y0 + CTR_VIEW_Y) * sZoom + sOffY, sy1 = (y1 + CTR_VIEW_Y) * sZoom + sOffY;

    if (sx0 < 0) sx0 = 0;
    if (sy0 < 0) sy0 = 0;
    float left = roundf(sx0), right = roundf(sx1), top = roundf(sy0), bottom = roundf(sy1);
    if (right > left && bottom > top)
        C2D_DrawRectSolid(left, top, 0, right - left, bottom - top, C2D_Color32(255, 255, 255, 255));
}

/*
 * The rectangles in sWindowRects tile the view, each tagged with a key below
 * 256. Every key's rectangles write it into the stencil, then pass draws each
 * key once over its bounding box, stencil-tested to exactly its rectangles.
 */
typedef void (*StencilPass)(int x0, int y0, int x1, int y1, unsigned key);

static void ComposeStencil(StencilPass pass)
{
    bool used[256] = {false};
    int box[256][4];
    for (unsigned i = 0; i < sWindowRectCount; ++i)
    {
        const WindowRect *r = &sWindowRects[i];
        int *b = box[r->mask];
        if (!used[r->mask])
        {
            used[r->mask] = true;
            b[0] = r->x0; b[1] = r->y0; b[2] = r->x1; b[3] = r->y1;
            continue;
        }
        if (r->x0 < b[0]) b[0] = r->x0;
        if (r->y0 < b[1]) b[1] = r->y0;
        if (r->x1 > b[2]) b[2] = r->x1;
        if (r->y1 > b[3]) b[3] = r->y1;
    }

    /* Flush pending colour writes before the depth-stencil buffer changes. */
    C2D_Flush();
    GPUCMD_AddWrite(GPUREG_FRAMEBUFFER_FLUSH, 1);
    C3D_FrameBuf *fb = C3D_GetFrameBuf();
    C3D_FrameBuf saved = *fb;
    void *stencil = StencilBuffer();
    uint32_t *marked = stencil == sSceneStencilBuf ? &sSceneStencilMarked : &sStencilMarked;
    C3D_FrameBufDepth(fb, stencil, GPU_RB_DEPTH24_STENCIL8);

    /* Mark: stencil only, no colour or depth writes. Every z is 0, so with
     * Citro2D's GEQUAL depth test nothing was ever depth-rejected; turning the
     * test off for the passes below keeps every fragment it kept. */
    C3D_Mtx view;
    C2D_ViewSave(&view);
    C2D_ViewReset();
    C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
    C3D_DepthTest(false, GPU_ALWAYS, 0);
    C3D_StencilOp(GPU_STENCIL_REPLACE, GPU_STENCIL_REPLACE, GPU_STENCIL_REPLACE);
    for (unsigned mask = 0; mask < 256 && *marked != sWindowRectSerial; ++mask)
    {
        if (!used[mask]) continue;
        C3D_StencilTest(true, GPU_ALWAYS, (int)mask, 0xFF, 0xFF);
        for (unsigned i = 0; i < sWindowRectCount; ++i)
            if (sWindowRects[i].mask == mask)
                StencilRect(sWindowRects[i].x0, sWindowRects[i].y0,
                            sWindowRects[i].x1, sWindowRects[i].y1);
        C2D_Flush();
    }
    C2D_ViewRestore(&view);
    *marked = sWindowRectSerial;

    /* Draw: each mask once, only where the stencil holds it. */
    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);
    C3D_StencilOp(GPU_STENCIL_KEEP, GPU_STENCIL_KEEP, GPU_STENCIL_KEEP);
    for (unsigned mask = 0; mask < 256; ++mask)
    {
        if (!used[mask]) continue;
        C2D_Flush();
        C3D_StencilTest(true, GPU_EQUAL, (int)mask, 0xFF, 0x00);
        pass(box[mask][0], box[mask][1], box[mask][2], box[mask][3], mask);
    }

    C2D_Flush();
    GPUCMD_AddWrite(GPUREG_FRAMEBUFFER_FLUSH, 1);
    C3D_StencilTest(false, GPU_ALWAYS, 0, 0xFF, 0x00);
    /* Citro2D's own depth state (C2D_Prepare). */
    C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);
    *C3D_GetFrameBuf() = saved;
}

/*
 * In 3D the same frame is composed once per depth plane. The OBJ window
 * mask and its partition depend only on the registers, OAM and VRAM, which
 * do not change while a frame is presented, and on the view; the planes of
 * one frame share them instead of rebuilding them per plane.
 */
typedef struct {
    uint32_t frame;
    unsigned display;
    float zoom, offX, offY;
    int viewX, viewY, targetW, targetH;
    bool native, stage, battle;
} WindowCacheKey;
static WindowCacheKey sWindowCacheKey;
static WindowSpans sWindowCacheSpans;
static bool sWindowCacheValid;

void CtrVideo_SetFlashWindow(const uint16_t *bounds, unsigned count)
{
    if (!bounds) count = 0;
    if (count > 160) count = 160;
    if (count != sFlashWindowCount
        || (count && memcmp(bounds, sFlashWindow, count * sizeof(*bounds)) != 0))
        sWindowCacheValid = false;
    sFlashWindowCount = count;
    if (count) memcpy(sFlashWindow, bounds, count * sizeof(*bounds));
}


static void ComposeObjWindow(unsigned display)
{
    WindowCacheKey key;
    memset(&key, 0, sizeof(key));
    key.frame = sStats.frames;
    key.display = display;
    key.zoom = sZoom; key.offX = sOffX; key.offY = sOffY;
    key.viewX = sViewX; key.viewY = sViewY;
    key.targetW = sTargetW; key.targetH = sTargetH;
    key.native = sNative; key.stage = sStage; key.battle = sBattle;
    bool cached = sWindowCacheValid && memcmp(&key, &sWindowCacheKey, sizeof(key)) == 0;
    if (!cached)
    {
        /* Same mask, registers and view as the last frame: same partition. */
        bool maskChanged = BuildObjWindow();
        WindowSpans spans;
        WindowSpansRead(display, &spans);
        WindowCacheKey previous = sWindowCacheKey;
        previous.frame = key.frame;
        if (!sWindowCacheValid || maskChanged || memcmp(&previous, &key, sizeof(key)) != 0
            || memcmp(&spans, &sWindowCacheSpans, sizeof(spans)) != 0)
        {
            sWindowCacheSpans = spans;
            sWindowRectCount = 0;
            ++sWindowRectSerial;
            WindowPartition(&sWindowCacheSpans, WindowCollectRect);
        }
        sWindowCacheKey = key;
        sWindowCacheValid = true;
    }
    /* Stencil rectangles share the frame's Citro2D geometry buffer. */
    bool stencil = StencilTarget()
                && sWindowRectCount <= WINDOW_RECTS
                && sStats.tiles + sWindowRectCount < MAX_DRAWS / 2
                && StencilReady();
    if (stencil)
        ComposeStencil(WindowDrawRect);
    else
        WindowPartition(&sWindowCacheSpans, WindowDrawRect);
    C2D_Flush();
    C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
    sScissored = false;
    ClipToView();
}

/*
 * Only brightness effects need the title's BLDY scanline values. Adjacent
 * lines with equal values share a band; each band draws every layer with its
 * value. The title's fades give up to 160 bands - a full scene walk each - so
 * on the stencil path each distinct value is drawn once, over exactly its
 * bands. Without the stencil the bands draw one by one, as before.
 */
static unsigned sBrightnessKeys[160];
static unsigned sBrightnessKeyCount;

static unsigned LineBrightnessAt(int y)
{
    if (NativeIntroTitle()) y = y * 160 / 240;
    unsigned line = y < 0 ? 0 : (unsigned)y >= sLineBrightnessCount
                  ? sLineBrightnessCount - 1 : (unsigned)y;
    return sLineBrightness[line];
}

static void BrightnessDrawValue(int x0, int y0, int x1, int y1, unsigned value)
{
    C2D_Flush();
    Scissor(x0, y0, x1, y1);
    sScissored = true;
    sClipX0 = x0; sClipX1 = x1;
    sClipY0 = y0; sClipY1 = y1;
    sBrightnessOverride = true;
    sBrightnessValue = value;
    Layers(63);
}

static void BrightnessDrawKey(int x0, int y0, int x1, int y1, unsigned key)
{
    BrightnessDrawValue(x0, y0, x1, y1, sBrightnessKeys[key]);
}

static void ComposeLineBrightness(void)
{
    int left = VIEW_LEFT, right = VIEW_RIGHT, top = VIEW_TOP, bottom = VIEW_BOTTOM;
    bool stencil = StencilTarget();
    sWindowCacheValid = false;
    sWindowRectCount = 0;
    ++sWindowRectSerial;
    sBrightnessKeyCount = 0;
    for (int y = top; stencil && y < bottom; )
    {
        unsigned value = LineBrightnessAt(y);
        int end = y + 1;
        while (end < bottom && LineBrightnessAt(end) == value) ++end;
        unsigned key = 0;
        while (key < sBrightnessKeyCount && sBrightnessKeys[key] != value) ++key;
        if (key == sBrightnessKeyCount)
        {
            if (sBrightnessKeyCount == 160) { stencil = false; break; }
            sBrightnessKeys[sBrightnessKeyCount++] = value;
        }
        WindowCollectRect(left, y, right, end, key);
        y = end;
    }
    stencil = stencil && sWindowRectCount <= WINDOW_RECTS
           && sStats.tiles + sWindowRectCount < MAX_DRAWS / 2 && StencilReady();
    if (stencil)
    {
        ComposeStencil(BrightnessDrawKey);
    }
    else
    {
        for (int y = top; y < bottom; )
        {
            unsigned value = LineBrightnessAt(y);
            int end = y + 1;
            while (end < bottom && LineBrightnessAt(end) == value) ++end;
            BrightnessDrawValue(left, y, right, end, value);
            y = end;
        }
    }
    C2D_Flush();
    C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
    sScissored = sBrightnessOverride = false;
    ClipToView();
}

static void ComposeImpl(void)
{
    unsigned display = Reg(0);
    if ((display & 0x8000) || (sFieldLayers && sFlashWindowCount && (display & 0x2000)))
    { ComposeObjWindow(display); return; }
    if (!(display & 0x6000))
    {
        if (sLineBrightnessCount && ((Reg(0x50) >> 6) & 3) >= 2)
        {
            ComposeLineBrightness();
            return;
        }
        if (sNative)
        {
            C2D_Flush();
            Scissor(0, 0, 240, 160);
            sScissored = true;
        }
        Layers(63);
        if (sNative)
        {
            C2D_Flush();
            C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
            sScissored = false;
        }
        return;
    }
    /* Partition by window edges; each nonoverlapping rectangle has a uniform
     * WIN0 > WIN1 > outside mask. GPU scissor clips transformed primitives. */
    /* Window rectangles are GBA coordinates; the partition covers the whole
     * viewport so the margins keep the "outside" mask. */
    int xs[6] = {VIEW_LEFT, VIEW_RIGHT};
    int ys[6] = {VIEW_TOP, VIEW_BOTTOM};
    unsigned nx = 2, ny = 2;
    for (unsigned w = 0; w < 2; ++w)
        if (display & (0x2000u << w))
        {
            int x0, x1, y0, y1;

            WindowSpan(Reg(0x40 + 2*w), false, WindowCurtain(w), &x0, &x1);
            WindowSpan(Reg(0x44 + 2*w), true, WindowCurtain(w), &y0, &y1);

            /*
             * A window left enabled with both edges past the screen is how the
             * game turns one off: the overworld runs with WIN1H = 0xFFFF all
             * the time. It covers nothing, so its edges must not cut the
             * frame. Cutting there split every overworld frame in two at
             * x=255 - the end of the GBA's addressable width, which is exactly
             * where the seam showed - and walked every layer twice to do it.
             */
            if (x0 >= x1 || y0 >= y1) continue;
            xs[nx++] = x0;
            xs[nx++] = x1;
            ys[ny++] = y0;
            ys[ny++] = y1;
        }
    for (unsigned i = 0; i < nx; ++i) for (unsigned j = i + 1; j < nx; ++j)
        if (xs[j] < xs[i]) { int t=xs[j]; xs[j]=xs[i]; xs[i]=t; }
    for (unsigned i = 0; i < ny; ++i) for (unsigned j = i + 1; j < ny; ++j)
        if (ys[j] < ys[i]) { int t=ys[j]; ys[j]=ys[i]; ys[i]=t; }
    for (unsigned y = 0; y + 1 < ny; ++y) for (unsigned x = 0; x + 1 < nx; ++x)
    {
        if (xs[x] == xs[x+1] || ys[y] == ys[y+1]) continue;
        unsigned mask = Reg(0x4a) & 63;
        for (int w = 1; w >= 0; --w)
            if ((display & (0x2000u << w))
             && Inside(xs[x], Reg(0x40+2*w), false, WindowCurtain(w))
             && Inside(ys[y], Reg(0x44+2*w), true, WindowCurtain(w)))
                mask = (Reg(0x48) >> (8*w)) & 63;
        C2D_Flush();
        Scissor(xs[x], ys[y], xs[x+1], ys[y+1]);
        sScissored = true;
        sClipX0 = xs[x]; sClipX1 = xs[x+1];
        sClipY0 = ys[y]; sClipY1 = ys[y+1];
        Layers(mask);
    }
    C2D_Flush();
    C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
    sScissored = false;
    ClipToView();
}

static void Compose(void)
{
    uint64_t start=CtrPerf_Begin();
    uint64_t layers=gCtrPerf.ticks[PERF_BG]+gCtrPerf.ticks[PERF_OBJ];
    ComposeImpl();
    if (start)
    {
        uint64_t elapsed=CtrPerf_Begin()-start;
        uint64_t drawn=gCtrPerf.ticks[PERF_BG]+gCtrPerf.ticks[PERF_OBJ]-layers;
        if (elapsed>drawn) gCtrPerf.ticks[PERF_WINDOW]+=elapsed-drawn;
    }
}

static bool CreateLogicalSurface(void)
{
    if (!C3D_TexInitVRAM(&sSurface, 512, 256, GPU_RGBA8)) return false;
    sLogical = C3D_RenderTargetCreateFromTex(&sSurface, GPU_TEXFACE_2D, 0, GPU_RB_DEPTH16);
    if (sLogical) return true;
    C3D_TexDelete(&sSurface);
    memset(&sSurface, 0, sizeof(sSurface));
    return false;
}

bool CtrVideo_Init(void)
{
    const char *step = "C3D_Init command buffer";
    sC3d = C3D_Init(0x100000);
    if (!sC3d) goto fail;
    step = "C2D_Init geometry buffers";
    sC2d = C2D_Init(MAX_DRAWS);
    if (!sC2d) goto fail;
    step = "tile atlas in linear memory";
    if (!C3D_TexInit(&sAtlas, ATLAS_SIZE, ATLAS_SIZE, GPU_RGBA5551)) goto fail;
    /* Citro3D rejects render-to-texture targets outside VRAM. The atlas is
     * CPU-updated linear memory, but the GPU-written surface MUST be VRAM. */
    step = "logical surface in VRAM";
    sPresentation = CtrPresentation_Layout();
    if (!CreateLogicalSurface()) goto fail;
    C3D_TexSetFilter(&sAtlas, GPU_NEAREST, GPU_NEAREST);
    C3D_TexSetFilter(&sSurface, GPU_NEAREST,
                               GPU_NEAREST);
    C3D_TexSetWrap(&sAtlas, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
    C3D_TexSetWrap(&sSurface, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
    step = "top screen render target";
    sTop = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    if (!sTop) goto fail;
    /* The right eye is created once and only rendered while the 3D slider is
     * up; a console without it simply never composes a second pass. */
    sTopRight = C2D_CreateScreenTarget(GFX_TOP, GFX_RIGHT);
    if (!sTopRight) CtrLog_Write(CTR_LOG_VIDEO, "no right eye target: 3D disabled");
    /* Optional: without them the bottom screen stays on the CPU path. Not
     * C2D_CreateScreenTarget: its transfer writes RGB8, and the bottom
     * framebuffer is RGB565 (consoleInit, and the CPU path draws 565). */
    sBottom = C3D_RenderTargetCreate(GSP_SCREEN_WIDTH, GSP_SCREEN_HEIGHT_BOTTOM, GPU_RB_RGBA8, GPU_RB_DEPTH16);
    if (sBottom)
        C3D_RenderTargetSetOutput(sBottom, GFX_BOTTOM, GFX_LEFT,
            GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0)
            | GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB565)
            | GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));
    if (sBottom && C3D_TexInit(&sChrome, 512, 256, GPU_RGB565))
    {
        C3D_TexSetFilter(&sChrome, GPU_NEAREST, GPU_NEAREST);
        memset(sChrome.data, 0, sChrome.size);
    }
    else
        CtrLog_Write(CTR_LOG_VIDEO, "no bottom screen target: PokeTouch viewport disabled");
    for (unsigned i = 0; i < 64; ++i)
        sMorton[i] = CtrVideo_Texel(i & 7, i / 8, 8);
    for (unsigned i = 0; i < 512; ++i)
        sTexturePalette[i] = CtrVideo_RGBA5551(sPalette[i]);
    C2D_Prepare();
#if CTR_VOXEL_ENABLED
    /* Citro3D and Citro2D are up; the voxel module only adds its own shader,
     * textures and buffers on top of them. */
    step = "voxel overworld renderer";
    if (!CtrVoxel_Init()) goto fail;
#endif
    sFpsStart = CtrPlatform_Milliseconds();
    CtrLog_Write(CTR_LOG_VIDEO, "VIDEO_PRESENT mode=native400 logical=400x240 compose=400x240 texture=512x256 scale=1.0 margins=0,0 filter=nearest VRAMfree=%lu",
                 (unsigned long)vramSpaceFree());
    return true;
fail:
    CtrLog_Write(CTR_LOG_ERROR, "VIDEO init failed: %s (linear free=%lu, VRAM free=%lu)",
                 step, (unsigned long)linearSpaceFree(), (unsigned long)vramSpaceFree());
    CtrVideo_Shutdown();
    return false;
}

void CtrVideo_Bind(CtrVideoMemory memory)
{
    sMemory = memory;
    TileCacheReset();
}

/*
 * Composes the logical frame with this eye's parallax and blits it to one
 * screen buffer. Render-to-texture followed by sampling that texture needs a
 * GPU command split, and so does composing the surface again for the other
 * eye, so each pass ends with one.
 */
/*
 * Outside the frame: the battle scene's surface while the battle is up, given
 * back a few seconds after. Without VRAM for it the scene is composed at 1.5
 * directly, and the allocation is tried again later.
 */
static void SceneRelease(void)
{
    if (sSceneStencilBuf) vramFree(sSceneStencilBuf);
    sSceneStencilBuf = NULL;
    sSceneStencilFailed = false;
    if (sScene) C3D_RenderTargetDelete(sScene);
    if (sSceneTex.data) C3D_TexDelete(&sSceneTex);
    sScene = NULL;
    memset(&sSceneTex, 0, sizeof(sSceneTex));
}

static void ScenePrepare(void)
{
    if (!sBattle)
    {
        if (sScene && sStats.frames - sSceneUsedFrame > LAYER_IDLE_FRAMES) SceneRelease();
        return;
    }
    sSceneUsedFrame = sStats.frames;
    /* The 3D depth planes are not used by the battle (its scene is composed per eye): their VRAM is the
     * battle's to take. When the scene surface or its stencil could not be placed, the window stencil of the
     * other screens goes too; either comes back on demand once the battle is over. */
    if (sBandsReady) CtrVideo_RequestPlaneRelease(); /* released at the start of the next frame */
    if (sMakeRoomAsked)
    {
        sMakeRoomAsked = false;
        if (sStencilBuf)
        {
            vramFree(sStencilBuf);
            sStencilBuf = NULL;
            sStencilFailed = false;
            CtrLog_Write(CTR_LOG_VIDEO, "window stencil released for the battle scene (VRAM free=%lu)", (unsigned long)vramSpaceFree());
        }
    }
    if (sScene || (sSceneFailed && sStats.frames - sSceneFailFrame < LAYER_RETRY_FRAMES)) return;
    for (unsigned attempt = 0; attempt < 2 && !sScene; ++attempt)
    {
        if (C3D_TexInitVRAM(&sSceneTex, SCENE_W, SCENE_H, GPU_RGBA5551)
            && (sScene = C3D_RenderTargetCreateFromTex(&sSceneTex, GPU_TEXFACE_2D, 0, -1)))
            break;
        SceneRelease();
        /* The overworld's atlases of other maps are what a 2D screen gets
         * back first, as for the depth planes (BandsReady). */
#if CTR_VOXEL_ENABLED
        if (attempt == 0 && CtrVoxel_ReleaseIdleVram() == 0) break;
#else
        break;
#endif
    }
    if (!sScene)
    {
        if (!sSceneFailed)
            CtrLog_Write(CTR_LOG_ERROR, "VIDEO: no VRAM for the battle scene surface (free=%lu); "
                         "unfiltered zoom", (unsigned long)vramSpaceFree());
        sSceneFailed = true;
        sSceneFailFrame = sStats.frames;
        sMakeRoomAsked = true;
        return;
    }
    CtrLog_Write(CTR_LOG_VIDEO, "battle scene surface ready: filtered zoom (VRAM free=%lu)",
                 (unsigned long)vramSpaceFree());
    sSceneFailed = false;
    C3D_TexSetFilter(&sSceneTex, GPU_LINEAR, GPU_LINEAR);
    C3D_TexSetWrap(&sSceneTex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
}

/*
 * The battle scene - everything but the text box - at SCENE_ZOOM into its own
 * surface, then that surface on the logical one at CTR_BATTLE_ZOOM. The scene
 * surface starts at the GBA pixel on the screen's top-left corner, so its
 * origin lands on screen (ox, oy), at most a screen pixel off the edge.
 */
static void RenderBattleScene(uint32_t clear)
{
    float zoom = sZoom, offX = sOffX, offY = sOffY;
    /*
     * The surface holds the lowest SCENE_H / SCENE_ZOOM lines of the scene.
     * What the screen shows above them is sky carried up from the terrain's
     * top row (DrawBattleBg), so it is filled from the surface's own top tile
     * rows, repeated, rather than doubling the surface for a dozen lines.
     */
    int left = VIEW_LEFT, top = VIEW_TOP, lowest = 112 - (int)(SCENE_H / SCENE_ZOOM);
    float ox, oy;

    if (top < lowest) top = lowest;
    ox = left * zoom + offX;
    oy = top * zoom + offY;
    float scale = zoom / SCENE_ZOOM;
    int width = (int)ceilf((CTR_GAME_WIDTH - ox) / scale);
    int height = (int)ceilf((CTR_GAME_HEIGHT - 48 - oy) / scale);
    Tex3DS_SubTexture cut;

    if (width > SCENE_W) width = SCENE_W;
    if (height > SCENE_H) height = SCENE_H;
    cut = (Tex3DS_SubTexture){(u16)width, (u16)height, 0, 1,
        width / (float)SCENE_W, 1 - height / (float)SCENE_H};
    sZoom = SCENE_ZOOM;
    sOffX = -left * SCENE_ZOOM;
    sOffY = -top * SCENE_ZOOM;
    sTargetW = width;
    sTargetH = height;
    sSurfaceH = SCENE_H;
    ClipToView();
    BlendForget();
    /* Cleared transparent: the backdrop is the logical surface's own clear,
     * which is RGBA8. A colour cleared into this 5551 surface comes out
     * wrong - black was a bright blue. */
    (void)clear;
    C2D_TargetClear(sScene, 0);
    C2D_SceneBegin(sScene);
    Blend(5, false, false);
    sLayerExclude = 1;
    if (!(Reg(0) & 128)) Compose();
    sLayerExclude = 0;
    C2D_Flush();
    C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
    C3D_FrameSplit(0);

    sZoom = zoom;
    sOffX = offX;
    sOffY = offY;
    sTargetW = CTR_GAME_WIDTH;
    sTargetH = CTR_GAME_HEIGHT;
    sSurfaceH = 256;
    ClipToView();
    BlendForget();
    C2D_SceneBegin(sLogical);
    C2D_ViewReset();
    Blend(5, false, false);
    C2D_DrawImageAt((C2D_Image){&sSceneTex, &cut}, ox, oy, 0, NULL, scale, scale);
    {
        /* Two tile rows, the period of the stripes the terrain repeats. */
        const float rows = 16 * SCENE_ZOOM;
        const Tex3DS_SubTexture strip = {(u16)width, (u16)rows, 0, 1,
            width / (float)SCENE_W, 1 - rows / SCENE_H};

        for (float y = oy; y > 0; )
        {
            y -= rows * scale;
            C2D_DrawImageAt((C2D_Image){&sSceneTex, &strip}, ox, y, 0, NULL, scale, scale);
        }
    }
    C2D_Flush();
}

/* The black letterbox is outside the GBA display, not its backdrop.
 * Draw palette[0] beneath transparent layers in the native game rectangle.
 * Geometry also works on the 5551 stereo targets, whose clear packing differs
 * from the RGBA8 logical surface. */
static void NativeBackdrop(uint32_t color)
{
    if (!sNative) return;
    ViewBase();
    C2D_DrawRectSolid(CTR_VIEW_X, CTR_VIEW_Y, 0, 240, 160, color);
}

/* Compose the frame into the logical surface. */
static void ComposeLogical(uint32_t clear, float parallax)
{
    bool scene = sBattle && sScene;

    sParallax = parallax;
    sLayerShift = 0;
    BlendForget();
    C2D_TargetClear(sLogical, sNative ? C2D_Color32(0, 0, 0, 255) : clear);
    C2D_SceneBegin(sLogical);
    Blend(5, false, false);
    NativeBackdrop(clear);
    if (scene)
    {
        RenderBattleScene(clear);
        C2D_SceneBegin(sLogical);
        Blend(5, false, false);
    }
    StageUnderlay();
    /* After a scene composed on its own, only the text box is left. */
    if (scene) sLayerExclude = 63 & ~(1u | 32u);
    if (!(Reg(0) & 128)) Compose();
    sLayerExclude = 0;
    C2D_Flush();
    C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
    C3D_FrameSplit(0);
}

static void RenderEye(C3D_RenderTarget *target, uint32_t clear, float parallax)
{
    const Tex3DS_SubTexture logical = {sPresentation.width, sPresentation.height, 0, 1,
        sPresentation.width / 512.0f, 1 - sPresentation.height / (float)sPresentation.textureHeight};

    ComposeLogical(clear, parallax);
    BlendForget();
    C2D_TargetClear(target, C2D_Color32(0, 0, 0, 255));
    C2D_SceneBegin(target);
    C2D_ViewReset();
    Blend(5, false, false);
    C2D_DrawImageAt((C2D_Image){&sSurface, &logical}, sPresentation.x, sPresentation.y, 0, NULL,
                    sPresentation.scale, sPresentation.scale);
    C2D_Flush();
    C3D_FrameSplit(0);
}

#if CTR_VOXEL_ENABLED
/*
 * The game's own UI and weather over the voxel world. BG0 carries text; only
 * OAM entries tagged by the sprite sorter as weather are composed. Drawing all
 * OBJ here would duplicate the player and every NPC over their billboards.
 */
/*
 * The text layer is a 32-column tilemap, so it only addresses the left 256px
 * of the 400px viewport, and widening it does not fit in background VRAM (see
 * the note on sStandardTextBox_WindowTemplates in src/menu.c). That is a
 * limit on where the *game* can put a window, not on where this compositor can
 * draw one: in voxel mode BG0 is drawn on its own, so the whole band can be
 * placed where it belongs. The standard text box spans x=16..232 within the
 * band, so its centre reaches the middle of the viewport at +76.
 */
#define CTR_VOXEL_UI_SHIFT 76.0f

/* Fixed north-facing camera: the upper 88 pixels are the distant background.
 * Four one-pixel taps, fading to zero towards the focus plane, reuse the world
 * surface. No extra target, depth readback or full-screen blur on Old 3DS. */
static void VoxelBackgroundBlur(void)
{
    static const int offsets[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    C2D_ImageTint tint;
    C2D_AlphaImageTint(&tint, 0.0f);
    C2D_TopImageTint(&tint, C2D_Color32(255, 255, 255, 56), 0.0f);
    for (unsigned i = 0; i < 4; ++i)
    {
        float x = 1.0f + offsets[i][0], y = 1.0f + offsets[i][1];
        const Tex3DS_SubTexture region = {CTR_GAME_WIDTH - 2, 87,
            x / 512.0f, 1.0f - y / 256.0f,
            (x + CTR_GAME_WIDTH - 2) / 512.0f, 1.0f - (y + 87) / 256.0f};
        C2D_DrawImageAt((C2D_Image){&sSurface, &region}, 1, 1, 0, &tint, 1, 1);
    }
}

static void ComposeVoxelOverlay(void)
{
    sPriorityMask = 15;
    sParallax = 0;
    sLayerOrigin = 0.0f;
    sLayerShift = 0.0f;
    sClipX0 = sClipY0 = 0;
    sClipX1 = CTR_GAME_WIDTH;
    sClipY1 = CTR_GAME_HEIGHT;
    sVoxelWeatherOnly = true;
    Layers(1u << 4);
    sVoxelWeatherOnly = false;
    /* Text windows and prompts must stay above the precipitation. */
    sLayerOrigin = CTR_VOXEL_UI_SHIFT;
    Layers(1u << 0);
    sLayerOrigin = 0.0f;
}

/*
 * The voxel path of the single Citro3D frame opened by CtrVideo_Present.
 * Same shape as RenderEye: compose the logical surface, split, blit it to the
 * screen. What changes is who composes it.
 */
static void RenderVoxel(uint32_t clear)
{
    const Tex3DS_SubTexture logical = {CTR_GAME_WIDTH, CTR_GAME_HEIGHT, 0, 1,
        CTR_GAME_WIDTH / 512.0f, 1 - CTR_GAME_HEIGHT / 256.0f};

    /* C2D_TargetClear clears colour and depth, which the 3D pass needs. */
    C2D_TargetClear(sLogical, clear);
    CtrVoxel_Draw(sLogical, 0.0f);

    /* Finish the world before sampling it. UI is drawn on top after blur. */
    C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
    C3D_FrameSplit(0);

    /* Back to the 2D compositor, which assumes its own program and no depth. */
    C2D_Prepare();
    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);
    BlendForget();
    C2D_TargetClear(sTop, C2D_Color32(0, 0, 0, 255));
    C2D_SceneBegin(sTop);
    C2D_ViewReset();
    Blend(5, false, false);
    C2D_DrawImageAt((C2D_Image){&sSurface, &logical}, 0, 0, 0, NULL, 1, 1);
    VoxelBackgroundBlur();
    if (!(Reg(0) & 128)) ComposeVoxelOverlay();
    C2D_Flush();
    C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
    C3D_FrameSplit(0);
}
#endif

/*
 * How many depth planes this frame can be split into.
 *
 * A layer that blends with whatever lies beneath it has to keep that beneath
 * in the same surface, so everything from its priority down stays together.
 * That is GBA alpha blending, which needs a target-1 layer, a target-2 layer
 * and a nonzero second coefficient (the overworld leaves the effect enabled
 * with no target-1 layer at all, which blends nothing), and a sprite in the
 * semi-transparent OBJ mode, which blends whatever its own priority allows.
 *
 * The result is planes 0..count-2 holding one priority each and the last
 * holding the rest, so the planes never change what a frame looks like.
 */
static unsigned DepthPlanes(void)
{
    unsigned display = Reg(0), control = Reg(0x50);
    unsigned target1 = control & 63, target2 = (control >> 8) & 63;
    unsigned merge = 4;

    if (((control >> 6) & 3) == 1 && target1 && target2 && ((Reg(0x52) >> 8) & 31))
    {
        if (target1 & 16) merge = 0;
        for (unsigned bg = 0; bg < 4 && merge; ++bg)
            if ((target1 & (1u << bg)) && (display & (0x100u << bg)))
            {
                unsigned priority = Reg(8 + bg * 2) & 3;
                if (priority < merge) merge = priority;
            }
    }
    /*
     * Backgrounds scrolled together are one image cut into layers, like the
     * three metatile layers of the field, and giving them different depths
     * pulls their tiles a pixel apart. Only a shared nonzero scroll means
     * that: on a still screen every layer sits at zero and they are separate
     * pictures stacked on each other, which is exactly where depth belongs.
     */
    for (unsigned bg = 0; bg + 1 < 4 && merge; ++bg)
        for (unsigned other = bg + 1; other < 4 && merge; ++other)
        {
            unsigned scroll = Reg(0x10 + bg * 4) | (Reg(0x12 + bg * 4) << 16);
            unsigned priority, otherPriority;

            if (!(display & (0x100u << bg)) || !(display & (0x100u << other))) continue;
            if (!scroll || scroll != (Reg(0x10 + other * 4) | (Reg(0x12 + other * 4) << 16)))
                continue;
            priority = Reg(8 + bg * 2) & 3;
            otherPriority = Reg(8 + other * 2) & 3;
            if (otherPriority < priority) priority = otherPriority;
            if (priority < merge) merge = priority;
        }
    if (display & 0x1000)
        for (unsigned i = 0; i < 128 && merge; ++i)
        {
            unsigned attr0 = sMemory.oam[i * 4];
            unsigned priority;

            if (!(attr0 & 0x100) && (attr0 & 0x200)) continue;
            if (((attr0 >> 10) & 3) != 1) continue;
            priority = (sMemory.oam[i * 4 + 2] >> 10) & 3;
            if (priority < merge) merge = priority;
        }
    /*
     * The merge only ever goes down within a scene. A sprite that comes and goes - a semi-transparent shine, a
     * sparkle - would otherwise merge the planes while it is there and split them again when it leaves, every
     * layer behind it jumping from one depth to the next and back. A scene is its display control, the
     * backgrounds' priorities and whether it is a stage or the intro; when they change, the depths may too.
     * Staying merged is always correct (what blends keeps what lies beneath it in its own surface), only flatter.
     */
    {
        static unsigned stickyMerge = 4, stickyKey = ~0u;
        unsigned key = (display & 0x1f07) | (Reg(8) & 3) << 16 | (Reg(10) & 3) << 18 | (Reg(12) & 3) << 20
                     | (Reg(14) & 3) << 22 | (unsigned)sStage << 24 | (unsigned)sIntro << 25;

        if (key != stickyKey) stickyMerge = 4;
        stickyKey = key;
        if (merge < stickyMerge) stickyMerge = merge;
        merge = stickyMerge;
    }
    return merge + 1 > CTR_BANDS ? CTR_BANDS : merge + 1;
}

/*
 * Released again once unused for a few seconds - the slider lowered, or the
 * voxel overworld on screen, which never composes planes. They are 1.5 MiB of
 * VRAM, and kept for good after the title screen was shown in 3D they left
 * the overworld's atlases, pages and chunks starving for it.
 */
#define CTR_BANDS_IDLE_FRAMES 180
static uint32_t sBandsUsedFrame;
/* Asked by the overworld when an atlas found no VRAM; honoured before the next
 * frame opens, since deleting a render target may wait for the GPU. */
static bool sPlaneReleaseAsked;
void CtrVideo_RequestPlaneRelease(void)
{
    sPlaneReleaseAsked = true;
}

/*
 * Set inside a frame that wanted the planes and found none. They are made
 * before the next frame opens: a failed attempt frees the planes it did get,
 * and C3D_RenderTargetDelete inside an open frame is svcBreak(USERBREAK_PANIC)
 * in citro3d - on an Old 3DS, whose VRAM often holds only two of the three,
 * that was opening a menu with the slider up. Until then, per eye.
 */
static bool sBandsWanted;

static void BandsRelease(void)
{
    CTR_PERF_COUNT(PERF_RESOURCE_RELEASE,1);
    for (unsigned i = 0; i < CTR_BANDS; ++i)
    {
        if (sBand[i]) C3D_RenderTargetDelete(sBand[i]);
        if (sBandTex[i].data) C3D_TexDelete(&sBandTex[i]);
        sBand[i] = NULL;
        memset(&sBandTex[i], 0, sizeof(sBandTex[i]));
    }
    sBandCount = 0;
    sBandsReady = false;
}

/* The last plane goes, the others stay (outside the frame, like BandsRelease: its target may be in flight). */
static void BandsShrink(void)
{
    unsigned last = sBandCount - 1;

    C3D_RenderTargetDelete(sBand[last]);
    C3D_TexDelete(&sBandTex[last]);
    sBand[last] = NULL;
    memset(&sBandTex[last], 0, sizeof(sBandTex[last]));
    --sBandCount;
}

/* Allocated on first use: a console that never opens the 3D slider never pays
 * the VRAM. A failure here is not fatal, it just keeps the direct path. */
static bool BandsCreate(void);

/* In the frame: whether the planes are there, asking for them if not. */
static bool BandsUsable(void)
{
    sBandsUsedFrame = sStats.frames;
    if (!sBandsReady) sBandsWanted = true;
    return sBandsReady;
}

/* Outside the frame only; see sBandsWanted. */
static bool BandsReady(void)
{
    sBandsUsedFrame = sStats.frames;
    if (sBandsReady) return true;
    if (sBandsFailed && sStats.frames < sBandsRetryFrame) return false;
    for (unsigned attempt = 0; attempt < 3 && !sBandsReady; ++attempt)
    {
        if (BandsCreate())
            sBandsReady = true;
        /*
         * The overworld keeps an atlas per tileset pair it has met, and a
         * 2D screen - a menu, a battle - is where that VRAM is wanted back:
         * the atlases of maps not on screen go, the current map's stay.
         */
#if CTR_VOXEL_ENABLED
        else if (attempt == 0)
            CtrVoxel_ReleaseIdleVram();
        else if (attempt == 1 && sScene && !sBattle && !sBattleRequested && !sNativeRequested)
            SceneRelease();
#else
        else if (attempt == 0 && sScene && !sBattle && !sBattleRequested && !sNativeRequested)
            SceneRelease();
#endif
        else
            break;
    }
    if (!sBandsReady)
    {
        if (!sBandsFailed)
            CtrLog_Write(CTR_LOG_ERROR, "VIDEO: no VRAM for 3D depth planes (free=%lu); composing "
                         "flat until retry", (unsigned long)vramSpaceFree());
        sBandsFailed = true;
        sBandsRetryFrame = sStats.frames + CTR_BANDS_RETRY_FRAMES;
        return false;
    }
    sBandsFailed = false;
    CtrLog_Write(CTR_LOG_VIDEO, "3D depth planes ready: %u (VRAM free=%lu)", sBandCount,
                 (unsigned long)vramSpaceFree());
    return true;
}

/* The three planes and their targets; two when only the third does not fit; otherwise nothing at all. */
static bool BandsCreate(void)
{
    CTR_PERF_COUNT(PERF_RESOURCE_CREATE,1);
    sBandCount = 0;
    for (unsigned i = 0; i < CTR_BANDS; ++i)
    {
        /* 5551 keeps 15-bit colour and coverage. Three native planes use
         * 768 KiB. Allocation is optional. */
        if (!C3D_TexInitVRAM(&sBandTex[i], 512, sPresentation.textureHeight, GPU_RGBA5551)) goto fail;
        C3D_TexSetFilter(&sBandTex[i], GPU_NEAREST,
                                      GPU_NEAREST);
        C3D_TexSetWrap(&sBandTex[i], GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
        /* No depth buffer: Citro2D draws in submission order, and clearing one
         * per plane per frame is memory traffic this path exists to avoid. */
        sBand[i] = C3D_RenderTargetCreateFromTex(&sBandTex[i], GPU_TEXFACE_2D, 0, -1);
        if (!sBand[i]) goto fail;
        sBandCount = i + 1;
    }
    return true;
fail:
    /* Two planes still give depth: keep them and drop only the one that failed (its texture may exist). */
    if (sBandCount >= 2)
    {
        unsigned i = sBandCount;

        if (sBand[i]) C3D_RenderTargetDelete(sBand[i]);
        if (sBandTex[i].data) C3D_TexDelete(&sBandTex[i]);
        sBand[i] = NULL;
        memset(&sBandTex[i], 0, sizeof(sBandTex[i]));
        return true;
    }
    BandsRelease();
    return false;
}

/*
 * Plane i holds priority i on its own, except the last, which holds every
 * remaining priority so that anything blending with what is beneath it keeps
 * that beneath in the same surface. The last plane therefore always reaches
 * priority 3, sits at the screen plane and carries the backdrop; the others
 * start transparent and are stacked on top of it.
 */
static unsigned BandMask(unsigned band, unsigned count)
{
    if (band + 1 < count) return 1u << band;
    return 15u & ~((1u << band) - 1);
}

static float BandDepth(unsigned band, unsigned count)
{
    return band + 1 < count ? (float)(CTR_PRIORITIES - 1 - band) : 0.0f;
}

/*
 * The depth planes are RGBA5551 textures, but the backdrop reaches here as RGBA8 (C2D_Color32). The GPU clears
 * a target with the raw word of its own format, so an RGBA8 value cleared into a plane was read back as a
 * 5551 pixel from its low half: a flat blue with the 3D slider up. The clear value is packed as 5551.
 */
static uint32_t PlaneColor(uint32_t color)
{
    unsigned r = color & 255, g = (color >> 8) & 255, b = (color >> 16) & 255, a = color >> 24;

    return (r >> 3) << 11 | (g >> 3) << 6 | (b >> 3) << 1 | (a >= 128);
}

/* C2D_TargetClear for an RGBA5551 render target. */
static void PlaneClear(C3D_RenderTarget *target, uint32_t color)
{
    C2D_Flush();
    C3D_FrameSplit(0);
    C3D_RenderTargetClear(target, C3D_CLEAR_ALL, PlaneColor(color), 0);
}

/* Composes every depth plane into its own surface, with no displacement. */
static void RenderBands(unsigned count, uint32_t backdrop)
{
    sParallax = 0;
    for (int band = (int)count - 1; band >= 0; --band)
    {
        uint32_t before = sStats.tiles;

        sPriorityMask = BandMask(band, count);
        sLayerShift = 0;
        BlendForget();
        PlaneClear(sBand[band], band + 1 == (int)count && !sNative ? backdrop : 0);
        C2D_SceneBegin(sBand[band]);
        if (band + 1 == (int)count)
        {
            Blend(5, false, false);
            NativeBackdrop(backdrop);
            StageUnderlay();
        }
        if (!(Reg(0) & 128)) Compose();
        C2D_Flush();
        C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
        /* The furthest plane carries the backdrop, so it is never empty. */
        sBandUsed[band] = sStats.tiles != before || band + 1 == (int)count;
    }
    sPriorityMask = 15;
    /* Rendering to a texture and then sampling it needs a command split. */
    C3D_FrameSplit(0);
}

/* Stacks the depth planes on one screen buffer, each displaced by its depth. */
static void BlitBands(C3D_RenderTarget *target, unsigned count, float parallax)
{
    const Tex3DS_SubTexture logical = {sPresentation.width, sPresentation.height, 0, 1,
        sPresentation.width / 512.0f, 1 - sPresentation.height / (float)sPresentation.textureHeight};

    BlendForget();
    C2D_TargetClear(target, C2D_Color32(0, 0, 0, 255));
    C2D_SceneBegin(target);
    C2D_ViewReset();
    Blend(5, false, false);
    for (int band = (int)count - 1; band >= 0; --band)
        if (sBandUsed[band])
            C2D_DrawImageAt((C2D_Image){&sBandTex[band], &logical},
                            sPresentation.x + parallax * BandDepth(band, count), sPresentation.y,
                            0, NULL, sPresentation.scale, sPresentation.scale);
    C2D_Flush();
}

/*
 * Fast-forward indicator: >>N in the top-right corner, in the FPS counter's
 * 3x5 pixel font at twice its size, over a translucent box. Drawn last and
 * at zero parallax in both eyes.
 */
static unsigned sFastForward = 1;

void CtrVideo_SetFastForward(unsigned speed)
{
    if (speed != sFastForward)
        CtrLog_Write(CTR_LOG_VIDEO, "fast-forward %ux at frame %lu", speed, (unsigned long)sStats.frames);
    sFastForward = speed;
}

static void DrawFastForward(C3D_RenderTarget *target)
{
    static const char *const glyphs[] = {
        "100010001010100", /* > */
        "111001111100111", /* 2 */
        "111001111001111", /* 3 */
        "101101111001001", /* 4 */
    };
    const float pixel = 2.0f, advance = 4 * pixel;
    unsigned digit = sFastForward >= 2 && sFastForward <= 4 ? sFastForward - 1 : 0;
    unsigned shown[3] = {0, 0, digit};
    float width = 3 * advance + pixel * 3, x0 = 400.0f - 2 - width, y0 = 2;

    C2D_Prepare();
    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);
    BlendForget();
    C2D_SceneBegin(target);
    C2D_ViewReset();
    Blend(5, false, false);
    C2D_DrawRectSolid(x0, y0, 0, width, 5 * pixel + pixel * 4, C2D_Color32(0, 0, 0, 160));
    for (unsigned i = 0; i < 3; ++i)
    {
        const char *bits = glyphs[shown[i]];
        float x = x0 + pixel * 2 + i * advance, y = y0 + pixel * 2;
        for (unsigned p = 0; p < 15; ++p)
            if (bits[p] == '1')
                C2D_DrawRectSolid(x + (p % 3) * pixel, y + (p / 3) * pixel, 0,
                                  pixel, pixel, C2D_Color32(255, 255, 255, 255));
    }
    C2D_Flush();
}

#ifndef CTR_SHOW_FPS
#define CTR_SHOW_FPS 1
#endif
#if CTR_SHOW_FPS
/*
 * The FPS counter (SHOW_FPS=1): a 3x5 pixel font, each pixel a 1x1 solid
 * rectangle, over a translucent box in the top-right corner (the 2 pixel margin
 * it had on the left, from the box's own width). Drawn last, over whatever the
 * frame composed, and at zero parallax in both eyes. The PokeTouch option turns
 * it off: it is then not drawn at all.
 */
static bool sFpsCounter = true;

void CtrVideo_SetFpsCounter(bool on)
{
    sFpsCounter = on;
}
#define FPS_PIXEL 1.0f
#define FPS_ADVANCE (4 * FPS_PIXEL)

static const char *const sFpsGlyphs[] =
{
    "111101101101111", "010110010010111", "111001111100111", "111001111001111",
    "101101111001001", "111100111001111", "111100111101111", "111001001001001",
    "111101111101111", "111101111001111",
    "111100111100100", /* F */
    "111101111100100", /* P */
};

static void DrawFps(C3D_RenderTarget *target)
{
    unsigned fps = (unsigned)(sStats.fps + 0.5f);
    unsigned glyphs[8], count = 0;
    char digits[4];
    int length = snprintf(digits, sizeof(digits), "%u", fps > 999 ? 999 : fps);

    glyphs[count++] = 10;   /* F */
    glyphs[count++] = 11;   /* P */
    glyphs[count++] = 5;    /* S, which is a 5 */
    glyphs[count++] = ~0u;  /* space */
    for (int i = 0; i < length; ++i)
        glyphs[count++] = (unsigned)(digits[i] - '0');

    /* Whatever path composed the frame, the counter draws with the 2D
     * program and no depth test, as RenderVoxel's UI pass does. */
    C2D_Prepare();
    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);
    BlendForget();
    C2D_SceneBegin(target);
    C2D_ViewReset();
    Blend(5, false, false);
    float boxWidth = count * FPS_ADVANCE + FPS_PIXEL * 3, margin = 2.0f;
    float origin = CTR_GAME_WIDTH - margin - boxWidth;
    /* The fast-forward indicator owns the corner while it shows: the counter sits to its left. */
    if (sFastForward > 1)
        origin -= 3 * 2.0f * 4 + 2.0f * 3 + margin;
    C2D_DrawRectSolid(origin, 2, 0, boxWidth, 5 * FPS_PIXEL + FPS_PIXEL * 4,
                      C2D_Color32(0, 0, 0, 160));
    for (unsigned i = 0; i < count; ++i)
    {
        if (glyphs[i] == ~0u) continue;
        const char *bits = sFpsGlyphs[glyphs[i]];
        float x = origin + FPS_PIXEL * 2 + i * FPS_ADVANCE, y = 2 + FPS_PIXEL * 2;
        for (unsigned p = 0; p < 15; ++p)
            if (bits[p] == '1')
                C2D_DrawRectSolid(x + (p % 3) * FPS_PIXEL, y + (p / 3) * FPS_PIXEL, 0,
                                  FPS_PIXEL, FPS_PIXEL, C2D_Color32(255, 255, 255, 255));
    }
    C2D_Flush();
}
#else
void CtrVideo_SetFpsCounter(bool on)
{
    (void)on;
}
#endif

/*
 * The bottom screen runs some of the game's menus without showing them (the
 * party menu, the bag): while it does, the top screen keeps the last frame it
 * presented, the world as it was when the menu opened. Nothing is rendered;
 * the frame is only paced to the display.
 */
static bool sHoldTop;
static bool sBottomPcField;
void CtrVideo_SetBottomViewportPcField(bool on) { sBottomPcField = on; }

bool CtrVideo_SetBottomViewport(bool on)
{
    on = on && sBottom && sChrome.data;
    if (on != sBottomGpu)
    {
        /* The CPU path writes the shown buffer; the GPU path swaps two. */
        gfxSetDoubleBuffering(GFX_BOTTOM, on);
        CtrLog_Write(CTR_LOG_VIDEO, "bottom screen %s at frame %lu", on ? "on the GPU" : "on the CPU",
                     (unsigned long)sStats.frames);
        sBottomGpu = on;
    }
    return on;
}

void CtrVideo_BottomCanvasRect(const uint16_t *canvas, int x0, int y0, int x1, int y1)
{
    sChromeCanvas = canvas;
    CtrDirty_Add(&sChromeDirty, x0, y0, x1, y1);
}

static uint32_t sChromeStatPixels;
static uint64_t sChromeStatTicks;

void CtrVideo_BottomUploadStats(uint32_t *pixels, uint64_t *ticks)
{
    *pixels = sChromeStatPixels;
    *ticks = sChromeStatTicks;
    sChromeStatPixels = 0;
    sChromeStatTicks = 0;
}

/* After FrameBegin, when the GPU no longer reads the texture: the changed
 * rectangles of the canvas into it (the canvas is column-major, bottom to
 * top), and nothing else. */
static void UploadChrome(void)
{
    uint64_t start;

    if (!sChromeCanvas || sChromeDirty.count == 0) return;
    start = CtrPerf_PlatformClock();
    CtrDirty_Upload(sChromeCanvas, &sChromeDirty, (uint16_t *)sChrome.data, CtrVideo_Texel);
    sChromeStatPixels += (uint32_t)CtrDirty_Pixels(&sChromeDirty);
    sChromeStatTicks += CtrPerf_PlatformClock() - start;
    CtrDirty_Clear(&sChromeDirty);
}

/* The bottom screen: the canvas, then the GBA picture (240x160 at the stage
 * position of the logical surface) over the viewport, pixel for pixel. */
static void RenderBottom(void)
{
    const Tex3DS_SubTexture chrome = {320, 240, 0, 1, 320 / 512.0f, 1 - 240 / 256.0f};
    /* PC's first menu is drawn on BG0 of the field, aligned with its bottom
     * 160px. The actual box screens retain the original centred stage crop. */
    const int sourceY = sBottomPcField ? CTR_GAME_HEIGHT - 160 : CTR_STAGE_Y;
    const Tex3DS_SubTexture gba = {240, 160, CTR_STAGE_X / 512.0f, 1 - sourceY / 256.0f,
        (CTR_STAGE_X + 240) / 512.0f, 1 - (sourceY + 160) / 256.0f};

    BlendForget();
    C2D_TargetClear(sBottom, C2D_Color32(0, 0, 0, 255));
    C2D_SceneBegin(sBottom);
    C2D_ViewReset();
    Blend(5, false, false);
    C2D_DrawImageAt((C2D_Image){&sChrome, &chrome}, 0, 0, 0, NULL, 1, 1);
    C2D_DrawImageAt((C2D_Image){&sSurface, &gba}, PT_VIEW_X, PT_VIEW_Y, 0, NULL, 1, 1);
    C2D_Flush();
    C3D_FrameSplit(0);
}

static bool sHoldBottom;

void CtrVideo_HoldBottom(bool hold)
{
    sHoldBottom = hold;
}

void CtrVideo_HoldTop(bool hold)
{
    if (hold != sHoldTop)
        CtrLog_Write(CTR_LOG_VIDEO, "top screen %s at frame %lu", hold ? "held" : "released",
                     (unsigned long)sStats.frames);
    sHoldTop = hold;
}

static void ApplyPresentationView(void)
{
    static int lastMode = -1;
    sIntro = sIntroRequested;
    sPresentation = CtrPresentation_Layout();
    sStage = sStageRequested && !sIntro;
    sStageUi = sStage && sStageUiRequested;
    sCentred = sCentredRequested && !sStage && !sIntro;
    sBattle = sBattleRequested && !sStage && !sCentred && !sIntro;
    sNativeUi = sNativeUiRequested && !sStage && !sCentred && !sBattle && !sIntro;
    sNative = sNativeRequested && !sStage && !sCentred && !sBattle && !sNativeUi && !sIntro;
    sBattleTransition = sBattleTransitionRequested && !sStage && !sCentred && !sBattle && !sNativeUi && !sNative && !sIntro;
    sTargetW = CTR_GAME_WIDTH;
    sTargetH = CTR_GAME_HEIGHT;
    sSurfaceH = 256;
    sZoom = sBattle ? CTR_BATTLE_ZOOM : 1.0f;
    sOffX = sBattle ? CTR_GAME_WIDTH / 2 - 120 * sZoom : 0;
    sOffY = sBattle ? CTR_GAME_HEIGHT - 48 - 112 * sZoom : 0;
    sShiftZoom = sZoom;
    sViewX = (sIntro && !CtrIntroNative_Active()) || sStage || sCentred || sNative ? CTR_STAGE_X : 0;
    sViewY = (sIntro && !CtrIntroNative_Active()) || sStage || sCentred || sNative ? CTR_STAGE_Y : 0;
    ClipToView();
    sFieldLayers = !sStage && !sCentred && !sBattle && !sNative && !sNativeUi && !sIntro;
    int mode = sIntro ? 6 : sNativeUi ? 5 : sStage ? 1 : sCentred ? 2 : sBattle ? 3 : sNative ? 4 : 0;
    if (mode != lastMode)
    {
        TileCacheOnModeChange(lastMode, mode);
        CtrLog_Write(CTR_LOG_VIDEO, mode == 6 ? "VIDEO_MODE intro native400x240" :
                     mode == 5 ? "VIDEO_MODE native-ui" :
                     mode == 0 ? "VIDEO_MODE field native400" : "VIDEO_MODE fixed scene=%d", mode);
        lastMode = mode;
    }
}

void CtrVideo_Present(void)
{
    if (!sMemory.regs) CtrPlatform_Fatal("VIDEO has no logical memory bound");
    if (sHoldTop && !sBottomGpu)
    {
        uint64_t holdWait = CtrPerf_Begin();
        gspWaitForVBlank();
        CtrPerf_End(PERF_GPU_WAIT, holdWait);
        ++sStats.frames;
        return;
    }
    uint64_t perfRender=CtrPerf_Begin(), perfPrepare=CtrPerf_Begin();
    sPerfTexture=sPerfTarget=NULL;
    /* Wait for previous GPU work before editing the atlas. C3D owns VBlank
     * pacing and swap: no gfxSwapBuffers/gspWaitForVBlank in this path. */
    /* Outside the frame, where deleting a render target may wait for the GPU. */
    if (sBandsReady && (sPlaneReleaseAsked || sStats.frames - sBandsUsedFrame > CTR_BANDS_IDLE_FRAMES))
    {
        BandsRelease();
        CtrLog_Write(CTR_LOG_VIDEO, "3D depth planes released (VRAM free=%lu)",
                     (unsigned long)vramSpaceFree());
    }
    else if (sBandsReady && sPlaneShrinkAsked && sBandCount > 2)
    {
        BandsShrink();
        CtrLog_Write(CTR_LOG_VIDEO, "3D depth planes: %u, the third to a layer texture (VRAM free=%lu)",
                     sBandCount, (unsigned long)vramSpaceFree());
    }
    sPlaneReleaseAsked = sPlaneShrinkAsked = false;
    if (sBandsWanted)
    {
        sBandsWanted = false;
        BandsReady();
    }
    ApplyPresentationView();
    /* Before anything reads the tile cache this frame (token frames + 1). */
    CtrVramTrack_Update(sMemory.vram, sVramCopy, sVramStamp, VRAM_TRACK_BLOCKS, sStats.frames + 1);
    LayersPrepare();
    ScenePrepare();
    CtrPerf_End(PERF_PREPARE,perfPrepare);
    uint64_t waitStart = svcGetSystemTick();
    if (!C3D_FrameBegin(C3D_FRAME_SYNCDRAW)) return;
    uint64_t start = svcGetSystemTick();
    sStats.waitMs = (start - waitStart) * 1000.0 / SYSCLOCK_ARM11;
    if (gCtrPerf.enabled) gCtrPerf.ticks[PERF_GPU_WAIT]+=start-waitStart;
#if CTR_VOXEL_ENABLED
    /*
     * FrameBegin returns on a VBlank, so the time between two returns is a
     * whole number of display frames: two of them is a frame the screen
     * showed twice. Logged with what the frame before it spent - the present
     * (and the voxel builds in it), then the game and its VBlank handler - so
     * that a slow renderer can be told from a slow game on hardware.
     */
    {
        static uint64_t sLastBegin;
        static unsigned sDropsLogged;
        /* A screen that runs at 30 for a while is one finding, not a line per
         * frame: a steady run is logged once a second with how many frames it
         * dropped, and only a hitch of three frames or more is always logged. */
        static uint32_t sLastDropLogged;
        static unsigned sDropsQuiet;
        float gap = sLastBegin ? (start - sLastBegin) * 1000.0f / SYSCLOCK_ARM11 : 0.0f;

        if (gap > 24.0f && sStats.frames > 120 && sDropsLogged < 3000)
        {
            if (gap < 45.0f && sStats.frames - sLastDropLogged < 60)
                ++sDropsQuiet;
            else
            {
                const CtrTiming *timing = CtrPlatform_GetTiming();
                const CtrVoxelStats *voxel = CtrVoxel_GetStats();

                ++sDropsLogged;
                sLastDropLogged = sStats.frames;
                /* The voxel figures are the last voxel frame's: on a 2D frame they
                 * are stale, and present= alone is the 2D compositor. */
                CtrLog_Write(CTR_LOG_VIDEO, "DROP frame=%lu gap=%.1fms (+%u quiet): present=%.1f "
                             "(voxel=%.1f: world=%.1f atlas=%.1f chunks=%.1f sprites=%.1f) "
                             "game=%.1f audio+vblank=%.1f",
                             (unsigned long)sStats.frames, gap, sDropsQuiet, sStats.cpuMs,
                             voxel->updateMs, voxel->worldMs, voxel->atlasMs,
                             voxel->meshMs - voxel->atlasMs, voxel->spritesMs,
                             timing->gameMs, timing->vblankMs);
                sDropsQuiet = 0;
            }
        }
        sLastBegin = start;
    }
#endif
    sStats.tiles = sStats.uploads = sStats.sprites = 0;
    sBgTicks = sObjTicks = 0;
    sStats.display = Reg(0);
    if (sUsed > CACHE_COUNT - 4096) TileCacheReset();
    uint64_t perfPalette=CtrPerf_Begin();
    UpdatePalette();
    UpdateFade();
    NativeIntroPrepare();
    CtrPerf_End(PERF_PALETTE,perfPalette);
    if (sStage || sBattle) RecordScroll();
    uint16_t backdrop = (Reg(0) & 128) ? 0x7fff : sPalette[0];
    uint32_t rgb = CtrVideo_RGBA8(backdrop, true);
    uint32_t clear = C2D_Color32(rgb >> 24, rgb >> 16, rgb >> 8, 255);
    /*
     * The 3D slider decides the separation, and at zero the right eye is not
     * composed at all: with 3D off this is the same single pass as before.
     * Whole pixels only, so every layer stays on the pixel grid in both eyes.
     */
#if CTR_VOXEL_ENABLED
    /*
     * Preparing the voxel frame is part of the decision. If the atlas or the
     * mesh could not be built this frame, the 2D compositor draws it: leaving
     * the previous map's geometry on screen would show the wrong place.
     */
    /* The overworld, drawn in 3D or - while its first atlas or mesh is still
     * being made - by the 2D compositor. Either way it never takes the depth
     * planes: holding them there is what kept the overworld's atlas out of
     * VRAM for good, the 2D picture standing in for it frame after frame. */
    bool overworld = !sStage && !sCentred && !sBattle && CtrVoxel_IsAvailable();
    bool voxel = overworld && CtrVoxel_Update();
    /* A new map still being made - a frame or two, behind the fade - is
     * black rather than the 2D picture flashing up before the 3D one. */
    bool blank = overworld && !voxel && CtrVoxel_IsWarmingUp();
#else
    const bool voxel = false, overworld = false, blank = false;
#endif
    float slider = osGet3DSliderState();
    /* Real stereoscopy for the voxel world is V8; the layer parallax of the
     * 2D path means nothing for a 3D scene, so it stays off there. */
    bool stereo = !voxel && !blank && !sBottomGpu && sTopRight && slider > 0.0f
                  && roundf(slider * CTR_STEREO_PIXELS) > 0.0f;
    /* A 2D screen composed per eye walks every layer twice, which on an Old
     * 3DS is 30 fps in a menu. Without its planes it stays flat until they
     * can be made (before the next frame, see sBandsWanted). */
    /* The battle scene is two passes of its own (RenderBattleScene), and cheap
     * enough to be composed per eye. */
    bool planes = stereo && !overworld && !sStage && !sBattle && BandsUsable();
    if (stereo && !overworld && !sStage && !sBattle && !planes) stereo = false;
    if (stereo != sStereo) { gfxSet3D(stereo); sStereo = stereo; }
    sStats.stereo = stereo ? roundf(slider * CTR_STEREO_PIXELS) : 0;
    uint64_t perfLayers=CtrPerf_Begin();
    if (!voxel && !blank) LayersRender();
    CtrPerf_End(PERF_PREPARE,perfLayers);

    if (voxel)
    {
#if CTR_VOXEL_ENABLED
        sPlanes = 0;
        RenderVoxel(clear);
#endif
    }
    else if (blank)
    {
        sPlanes = 0;
        C2D_TargetClear(sTop, C2D_Color32(0, 0, 0, 255));
    }
    else if (sBottomGpu)
    {
        /* A held TOP keeps its last frame: only the logical surface is
         * composed, for the bottom screen's viewport. */
        sPlanes = 0;
        if (!sHoldBottom) UploadChrome();
        if (sHoldTop) ComposeLogical(clear, 0.0f);
        else RenderEye(sTop, clear, 0.0f);
        /* Held: no bottom target is drawn, so the screen keeps its last frame. */
        if (!sHoldBottom) RenderBottom();
    }
    else if (!stereo)
    {
        sPlanes = 0;
        RenderEye(sTop, clear, 0.0f);
    }
    else if (planes)
    {
        sPlanes = DepthPlanes();
        if (sPlanes > sBandCount) sPlanes = sBandCount; /* the planes that exist; the last holds the rest */
        RenderBands(sPlanes, clear);
        BlitBands(sTop, sPlanes, sStats.stereo);
        BlitBands(sTopRight, sPlanes, -(float)sStats.stereo);
    }
    else
    {
        /* A stage, drawn from its layer textures, costs little enough to be
         * composed per eye; otherwise only without memory for the planes. */
        sPlanes = 0;
        RenderEye(sTop, clear, sStats.stereo);
        RenderEye(sTopRight, clear, -(float)sStats.stereo);
    }
#if CTR_SHOW_FPS
    if (sFpsCounter)
    {
        if (!sHoldTop) DrawFps(sTop);
        if (stereo) DrawFps(sTopRight);
    }
#endif
    if (sFastForward > 1)
    {
        if (!sHoldTop) DrawFastForward(sTop);
        if (stereo) DrawFastForward(sTopRight);
    }
    sStats.commandUsage=C3D_GetCmdBufUsage();
    sStats.planes=sPlanes;
    uint64_t perfFinalize=CtrPerf_Begin();
    C3D_FrameEnd(0);
    CtrPerf_End(PERF_FINALIZE,perfFinalize);
    CtrPerf_End(PERF_RENDER,perfRender);
    ++sStats.frames;
    ++sFpsFrames;
    sStats.cpuMs = (svcGetSystemTick() - start) * 1000.0 / SYSCLOCK_ARM11;
    sStats.gpuMs = C3D_GetDrawingTime();
    uint64_t now = CtrPlatform_Milliseconds();
    if (now - sFpsStart >= 1000)
    {
        sStats.fps = sFpsFrames * 1000.0f / (now - sFpsStart);
        sFpsStart = now;
        sFpsFrames = 0;
    }
    if (sStats.frames % 600 == 0)
    {
        CtrLog_Write(CTR_LOG_VIDEO, "frames=%lu fps=%.1f cpu=%.2fms bg=%.2fms obj=%.2fms gpu=%.2fms "
                     "3d=%lupx%s x%lu quads=%lu sprites=%lu errors=%lu cache=%u linear=%lu vram=%lu",
                     (unsigned long)sStats.frames, sStats.fps, sStats.cpuMs,
                     sBgTicks * 1000.0 / SYSCLOCK_ARM11, sObjTicks * 1000.0 / SYSCLOCK_ARM11,
                     sStats.gpuMs, (unsigned long)sStats.stereo,
                     sStats.stereo ? (sPlanes ? "/planes" : "/eyes") : "", (unsigned long)sPlanes,
                     (unsigned long)sStats.tiles, (unsigned long)sStats.sprites,
                     (unsigned long)sStats.errors, sUsed,
                     (unsigned long)linearSpaceFree(), (unsigned long)vramSpaceFree());
#if CTR_VOXEL_ENABLED
        if (voxel)
        {
            const CtrVoxelStats *stats = CtrVoxel_GetStats();
            CtrLog_Write(CTR_LOG_VIDEO,
                         "VOXEL chunks=%u/%u missing=%u pending=%u builds=%u atlas=%u verts=%u "
                         "mesh=%.2fms peak=%.2fms update=%.2fms peak=%.2fms dropped=%u "
                         "anim=%u/%u refl=%u",
                         stats->visibleChunks, stats->chunks, stats->chunksMissing,
                         stats->pendingBuilds, stats->meshRebuilds, stats->atlasRebuilds,
                         stats->vertices, stats->meshMs, stats->meshPeakMs,
                         stats->updateMs, stats->updatePeakMs, stats->dropped,
                         stats->animationUploads, stats->animatedMetatiles,
                         stats->reflections);
        }
#endif
    }
}

const CtrVideoStats *CtrVideo_GetStats(void) { return &sStats; }

void CtrVideo_SetIntro(bool intro) { sIntroRequested = intro; }
void CtrVideo_SetIntroBackgrounds(unsigned mask, unsigned repeat, unsigned top, unsigned bottom)
{
    sIntroBackgrounds = mask & 15;
    sIntroEdgeFill = 0;
    sIntroRepeat = repeat & sIntroBackgrounds;
    sIntroTop = top;
    sIntroBottom = bottom;
}
void CtrVideo_SetIntroEdgeFill(unsigned mask) { sIntroEdgeFill = mask & 15; }
void CtrVideo_SetStage(bool stage) { sStageRequested = stage; }
void CtrVideo_SetStageUi(bool active) { sStageUiRequested = active; }
void CtrVideo_SetNativeViewport(bool native) { sNativeRequested = native; }
void CtrVideo_SetBattleTransition(bool active) { sBattleTransitionRequested = active; }
void CtrVideo_SetCentred(bool centred) { sCentredRequested = centred; }
void CtrVideo_SetNativeUi(bool nativeUi) { sNativeUiRequested = nativeUi; }
void CtrVideo_SetBattle(bool battle) { sBattleRequested = battle; }
void CtrVideo_SetFieldBarnDoorWipe(bool active) { sFieldBarnDoorWipe = active; }

void CtrVideo_SetLineScroll(unsigned reg, bool wide, const void *values, unsigned lines)
{
    const uint16_t *source = values;
    unsigned regs = wide ? 2 : 1;

    sLineMask = 0;
    sLineCount = 0;
    if (!values || reg + regs * 2 > 16) return;
    if (lines > LINE_MAX) lines = LINE_MAX;
    /*
     * The first line gets the register the VBlank handler wrote from entry 0.
     * The DMA then fires at the end of every line, so line y shows entry y-1.
     */
    for (unsigned r = 0; r < regs; ++r)
        for (unsigned y = 0; y < lines; ++y)
            sLineScroll[reg / 2 + r][y] = source[(y ? y - 1 : 0) * regs + r];
    sLineMask = ((1u << regs) - 1) << (reg / 2);
    sLineCount = lines;
}

void CtrVideo_SetLineBrightness(const uint16_t *values, unsigned lines)
{
    sLineBrightnessCount = 0;
    if (!values || !lines) return;
    if (lines > 160) lines = 160;
    memcpy(sLineBrightness, values, lines * sizeof(*values));
    sLineBrightnessCount = lines;
}

void CtrVideo_Shutdown(void)
{
    /* Leaving through the HOME menu: the system has taken the GPU back and
     * no VBlank arrives any more, so C3D_FrameSync (and the target deletes
     * that wait for queued work) never return and the console hangs on
     * "closing". The process ends right after; the system reclaims it all. */
    if (aptShouldClose())
        return;
#if CTR_VOXEL_ENABLED
    CtrVoxel_Shutdown();
#endif
    LayersRelease();
    C3D_FrameSync();
    NativeIntroRelease();
    SceneRelease();
    /* Target deletion waits for queued GPU work before buffers are freed. */
    if (sLogical) C3D_RenderTargetDelete(sLogical);
    if (sTop) C3D_RenderTargetDelete(sTop);
    if (sTopRight) C3D_RenderTargetDelete(sTopRight);
    if (sBottom) C3D_RenderTargetDelete(sBottom);
    if (sChrome.data) C3D_TexDelete(&sChrome);
    BandsRelease();
    sBandsFailed = false;
    if (sC2d) C2D_Fini();
    if (sSurface.data) C3D_TexDelete(&sSurface);
    if (sAtlas.data) C3D_TexDelete(&sAtlas);
    if (sC3d) C3D_Fini();
    if (sStencilBuf) vramFree(sStencilBuf);
    sStencilBuf = NULL;
    sStencilFailed = false;
    sLogical = sTop = sTopRight = sBottom = NULL;
    sBottomGpu = false;
    memset(&sChrome, 0, sizeof(sChrome));
    sC2d = sC3d = false;
    memset(&sSurface, 0, sizeof(sSurface));
    memset(&sAtlas, 0, sizeof(sAtlas));
}
