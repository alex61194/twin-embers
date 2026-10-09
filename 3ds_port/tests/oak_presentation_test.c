/* Original synthetic GPU-call instrumentation; no game pixels or text. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint32_t u32;
#define CTR_GAME_WIDTH 400
#define CTR_GAME_HEIGHT 240
#define CTR_VIEW_X 80
#define CTR_VIEW_Y 40
#define FADE_ACROSS 56.0f
#define FADE_DOWN 28.0f
static uint8_t vram[0x10000];
static struct { uint8_t *vram; } sMemory = {vram};
static uint16_t regs[64], sPalette[256];
static int sClipX0 = -80, sClipX1 = 320, sClipY0 = -40, sClipY1 = 200;
static float sLayerShift;
static bool sStage, sStageUi, sUnderlaid, sScrolls[4], textured;
static int rectangles, layerCuts, tileSpans, scissors;
static uint32_t canvas[240][400];
static unsigned Reg(unsigned offset) { return regs[offset/2]; }
static unsigned Min(unsigned a, unsigned b) { return a < b ? a : b; }
static unsigned MapEntry(unsigned map, unsigned size, unsigned x, unsigned y)
{
    unsigned off = map + ((y&31)*32 + (x&31))*2;
    if (size&1) off += ((x>>5)&1)*2048;
    if (size&2) off += ((y>>5)&1)*((size&1)?4096:2048);
    return vram[off] | vram[off+1]<<8;
}
static bool BgCharacterAddressValid(unsigned bg, unsigned c, unsigned m,
                                    unsigned ch, unsigned e, unsigned a)
{ (void)bg;(void)c;(void)m;(void)ch;(void)e;return a < 0xf000; }
static uint32_t C2D_Color32(unsigned r, unsigned g, unsigned b, unsigned a)
{ return r | g<<8 | b<<16 | a<<24; }
static uint32_t CtrVideo_RGBA8(unsigned c, bool opaque)
{ (void)opaque;return ((c&31)*255/31)<<24 | (((c>>5)&31)*255/31)<<16 | (((c>>10)&31)*255/31)<<8 | 255; }
static void ViewBase(void) {}
static void C2D_Flush(void) {}
static void C2D_DrawRectSolid(int x,int y,int z,int w,int h,uint32_t color)
{
    (void)z;
    assert(x>=0 && y>=0 && x+w<=400 && y+h<=240);
    ++rectangles;
    for(int row=y;row<y+h;++row) for(int col=x;col<x+w;++col) canvas[row][col]=color;
}
static bool LayerDrawable(unsigned bg) { (void)bg;return textured; }
static void DrawLayerRect(unsigned bg,int x0,int x1,int y0,int y1,
                          float sx,float sy,float dx,float dy,bool fade,int top,int bottom)
{
    (void)bg;(void)top;(void)bottom;
    assert(x0==0 && x1==240 && y0==0 && y1==160);
    assert(sx==0 && sy==0 && dx==1 && dy==1 && !fade);
    ++layerCuts;
}
static void Scissor(int x0,int y0,int x1,int y1)
{ assert(x0>=0 && y0>=0 && x1<=240 && y1<=160);++scissors; }
static void RestoreScissor(void) { --scissors; }
static void DrawTextSpan(unsigned bg,int x0,int x1,int y0,int y1)
{ (void)bg;assert(scissors==1 && x0>=0 && y0>=0 && x1<=240 && y1<=160);++tileSpans; }
/* PRODUCTION FUNCTIONS */
static void fill_map(unsigned entry)
{
    for(unsigned i=0;i<1024;++i) {vram[0xf000+2*i]=entry;vram[0xf001+2*i]=entry>>8;}
}
int main(void)
{
    regs[5]=30<<8; /* BG1, 4bpp, map at f000. */
    fill_map(0x2001);
    memset(vram+32,0x33,32);
    sPalette[35]=31; /* uniform synthetic red outside */
    assert(OakSurroundColor(-40)==C2D_Color32(255,0,0,255));
    assert(OakSurroundColor(200)==OakSurroundColor(159));
    DrawOakSurround();
    assert(rectangles==1);
    for(int y=0;y<240;++y) for(int x=0;x<400;++x) assert(canvas[y][x]==C2D_Color32(255,0,0,255));
    /* Nonzero scroll, nibble selection and horizontal/vertical tile flips. */
    vram[32]=0x43;sPalette[36]=31<<5;regs[10]=1;
    assert(OakSurroundColor(0)==C2D_Color32(0,255,0,255));
    regs[10]=0;fill_map(0x2401);vram[35]=0x43;
    assert(OakSurroundColor(0)==C2D_Color32(0,255,0,255));
    fill_map(0x2801);vram[60]=0x34;
    assert(OakSurroundColor(0)==C2D_Color32(0,255,0,255));
    /* BG palette brightness still follows the game, not an edge fade. */
    regs[0x50/2]=(3<<6)|2;regs[0x54/2]=16;
    assert(OakSurroundColor(0)==C2D_Color32(0,0,0,255));
    regs[0x50/2]=(2<<6)|2;
    assert(OakSurroundColor(0)==C2D_Color32(255,255,255,255));
    regs[0x50/2]=0;
    /* 8bpp ignores the map palette bank; colour zero is transparent. */
    regs[5]|=128;fill_map(0xf001);vram[64]=4;sPalette[4]=31<<10;
    assert(OakSurroundColor(0)==C2D_Color32(0,0,255,255));
    vram[64]=0;assert(OakSurroundColor(0)==0);
    /* BG0 text, BG1 icons/frame and BG2 portraits each draw exactly once. */
    textured=true;
    for(unsigned bg=0;bg<3;++bg) DrawStageUiText(bg);
    assert(layerCuts==3);
    textured=false;
    for(unsigned bg=0;bg<3;++bg) DrawStageUiText(bg);
    assert(tileSpans==3 && scissors==0);
    sClipX0=240;sClipX1=320;
    DrawStageUiText(0);assert(tileSpans==3);
    sClipX0=-80;sClipX1=320;sClipY0=160;sClipY1=200;
    DrawStageUiText(0);assert(tileSpans==3);
    /* Oak bypasses black underlay, ordinary stages retain the baseline. */
    sClipY0=-40;sClipY1=200;regs[0]=0x200;sStage=true;sStageUi=true;
    rectangles=0;StageUnderlay();assert(!sUnderlaid && rectangles==0);
    sStageUi=false;StageUnderlay();assert(sUnderlaid && rectangles==4);
    puts("Oak production sampling, crisp clipping, fallback and isolated underlay: PASS");
    return 0;
}
