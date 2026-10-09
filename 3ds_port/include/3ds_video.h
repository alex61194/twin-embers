#ifndef CTR_VIDEO_H
#define CTR_VIDEO_H
#include <stdbool.h>
#include <stdint.h>

#define CTR_GAME_WIDTH 400
#define CTR_GAME_HEIGHT 240

/*
 * A GBA stage keeps the native pixel grid: the 240x160 picture sits 1:1 in the
 * middle of the 400x240 screen, and the compositor fills the space around it
 * from the picture's own art (see DrawStageBg in 3ds_video.c).
 */
#define CTR_STAGE_X ((CTR_GAME_WIDTH - 240) / 2)
#define CTR_STAGE_Y ((CTR_GAME_HEIGHT - 160) / 2)
/* The battle scene: centred across, resting on the bottom edge. */
#define CTR_BATTLE_X CTR_STAGE_X
#define CTR_BATTLE_Y (CTR_GAME_HEIGHT - 160)
/*
 * The battle scene itself is magnified: the middle of its bottom edge (GBA
 * 120,112) sits on screen (200,192), on top of the 1:1 text box. At 1.5 the
 * tile grid still lands on whole screen pixels.
 */
#define CTR_BATTLE_ZOOM 1.4f

typedef struct
{
    const uint8_t *vram;
    const uint16_t *palette;
    const uint16_t *oam;
    const uint16_t *regs;
} CtrVideoMemory;

typedef struct
{
    uint32_t frames, tiles, uploads, sprites, errors, stereo, planes;
    uint16_t display;
    float fps, cpuMs, gpuMs, waitMs, commandUsage;
} CtrVideoStats;

bool CtrVideo_Init(void);
void CtrVideo_Shutdown(void);
void CtrVideo_Bind(CtrVideoMemory memory);
void CtrVideo_Present(void);
/* Read-only snapshot of the exact shadow inputs and requested compositor view. */
void CtrVideo_DumpState(void);
/* Keep showing the last top-screen frame instead of the game's screen. */
void CtrVideo_HoldTop(bool hold);
/* The FPS counter (top-right corner of the top screen): shown by default; off, it is not drawn at all. */
void CtrVideo_SetFpsCounter(bool on);
/* The field's barn-door wipe encodes its horizontal bounds in GBA window units. */
void CtrVideo_SetFieldBarnDoorWipe(bool active);
/* Field-move Pokemon and streak strip share the native screen centre. */
void CtrVideo_SetFieldMoveShowMon(bool active);
/* Cave darkness: the complete normalized WIN0H scanline circle, or NULL to release. */
void CtrVideo_SetFlashWindow(const uint16_t *bounds, unsigned count);
/* The bottom screen keeps what it last showed (a route between two PokeTouch
 * screens: no HOME, black or unwinding GBA screen in between). */
void CtrVideo_HoldBottom(bool hold);
/* Fast-forward speed (1 = off), shown as >>N in the top screen's corner. */
void CtrVideo_SetFastForward(unsigned speed);
/*
 * The bottom screen presented by the GPU: every frame draws the bottom canvas
 * and, over its viewport (3ds_poketouch.h), the GBA picture sampled straight
 * from the composed logical surface at 0.85; with TOP held only that surface
 * is composed. Off: the CPU path (CtrBottom_BlitRect). Returns whether it is on
 * (false when the bottom target could not be made).
 */
bool CtrVideo_SetBottomViewport(bool on);
/* Only the PC field menu uses the lower-aligned GBA BG0 window as source. */
void CtrVideo_SetBottomViewportPcField(bool on);
/* The rectangle [x0, x1) x [y0, y1) of the bottom canvas changed (screen
 * coordinates, y down); only it is read, at the next Present. */
void CtrVideo_BottomCanvasRect(const uint16_t *canvas, int x0, int y0, int x1, int y1);
/* The bottom canvas upload since the last call: texels written and the ticks
 * it took (both reset). Opt-in profiling of the bottom subsystem. */
void CtrVideo_BottomUploadStats(uint32_t *pixels, uint64_t *ticks);
/* OAM entries belonging to field weather, tagged while BuildOamBuffer sorts sprites. */
void CtrVideo_ClearVoxelWeatherOam(void);
void CtrVideo_MarkVoxelWeatherOam(unsigned first, unsigned end);
void CtrVideo_NotifyTilesetAnimWrite(const void *dest, unsigned bytes);
const uint8_t *CtrVideo_GetBgVram(void);
/* Frees the 2D compositor's 3D depth planes before the next frame, for the
 * overworld when it cannot place an atlas. */
void CtrVideo_RequestPlaneRelease(void);
/*
 * Whether the frames that follow are a GBA stage: a screen composed as one
 * 240x160 picture (credits, Oak speech), shown 1:1 in the middle of the top
 * screen with the space around it filled from its own art. See docs/ARCHITECTURE.md.
 */
void CtrVideo_SetStage(bool stage);
/* Keep foreground text strictly inside 240x160 for the Oak/tutorial stage. */
void CtrVideo_SetStageUi(bool active);
/* Boot/intro/title: authored coordinates composed onto the native TOP canvas. */
void CtrVideo_SetIntro(bool intro);
/* Scenery tiles cover the native canvas; other layers retain their own pixel grid. */
void CtrVideo_SetIntroBackgrounds(unsigned mask, unsigned repeat, unsigned top, unsigned bottom);
/* Foreground intro backgrounds (a mask of BG numbers) whose last tile column goes on to the canvas's right edge. */
void CtrVideo_SetIntroEdgeFill(unsigned mask);
/* Strict centred GBA clipping for authored transitions (false in the field). */
void CtrVideo_SetNativeViewport(bool native);
/* FireRed's battle transition is running: the field stays the 400x240 canvas under its effect (it is not the strict 240x160 viewport). */
void CtrVideo_SetBattleTransition(bool active);
/*
 * Whether the frames that follow are a GBA screen shown centred (the fly map, the Town Map):
 * 1:1 at the stage position, with only the layers that wrap on the GBA and
 * sprites reaching into the margins.
 */
void CtrVideo_SetCentred(bool centred);
/* Full 400x240 interface with ordinary BG composition and no field UI shift. */
void CtrVideo_SetNativeUi(bool nativeUi);
/* While the quest log's playback windows exist: its top bar on the canvas top,
 * its description and bottom bar on the canvas bottom, both full width. */
void CtrVideo_SetQuestLogLayout(bool layout);
void CtrVideo_SetMapNamePopup(bool active);
/*
 * Whether the frames that follow are the battle scene: the 240x160 picture 1:1
 * at (CTR_BATTLE_X, CTR_BATTLE_Y), so that its text box lies on the bottom
 * edge of the top screen, the text box stretched to the full width and the
 * scene above and beside it carried on from its own layers. See docs/ARCHITECTURE.md.
 */
void CtrVideo_SetBattle(bool battle);
/*
 * Per-scanline values of background scroll registers for the next frame: reg
 * is the offset from BG0HOFS, wide means two registers per line (32-bit DMA),
 * values holds one unit per line. NULL values turns it off.
 */
void CtrVideo_SetLineScroll(unsigned reg, bool wide, const void *values, unsigned lines);
/* Per-scanline BLDY values produced by the title's HBlank DMA. NULL disables. */
void CtrVideo_SetLineBrightness(const uint16_t *values, unsigned lines);
const CtrVideoStats *CtrVideo_GetStats(void);
void CtrScene_Init(void);
void CtrScene_Update(void);
unsigned CtrScene_Mode(void);
void CtrCursor_Init(const void *tiles, const uint16_t *palette);
void CtrCursor_Update(bool visible);
void CtrCursor_GetPosition(int *x, int *y);

/* Pure, SDK-free address/format helpers, also used by host regression tests. */
uint32_t CtrVideo_TextMapOffset(unsigned x, unsigned y, unsigned size);
int CtrVideo_MosaicOrigin(int screen, unsigned size);
uint32_t CtrVideo_Texel(unsigned x, unsigned y, unsigned width);
uint32_t CtrVideo_RGBA8(uint16_t color, bool opaque);
uint16_t CtrVideo_RGBA5551(uint16_t color);
unsigned CtrVideo_ObjTile(unsigned base, unsigned x, unsigned y,
                          unsigned width, bool color256, bool mapping1d);
bool CtrVideo_ObjOpaque(const uint8_t *objVram, unsigned base,
                        unsigned x, unsigned y, unsigned width,
                        bool color256, bool mapping1d);
int32_t CtrVideo_AffineReference(uint32_t value);
#endif
