/* Native half of the bottom screen: the only part that touches libctru.
 *
 * The game-side canvas already has the framebuffer layout (RGB565,
 * column-major, each column bottom-to-top), so presenting is a copy of
 * whole columns plus a data-cache flush over the same range.
 *
 * Two framebuffers: the copies go to the one not shown, and the frame's end
 * asks for the swap, which the screen takes at its next VBlank. Writing the
 * shown one tore the picture wherever the scan was. The buffer drawn next is
 * the one shown before, a frame behind: what changed in the frame before is
 * copied into it too, from the canvas, which holds the whole picture.
 */
#include <3ds.h>
#include <stdbool.h>
#include <string.h>
#include "3ds_bottom.h"

typedef struct { bool on; int x0, y0, x1, y1; } Box;

/* This frame's copies, and the last swapped frame's (the buffer drawn now lacks them). */
static Box sNow, sBefore;
static const uint16_t *sCanvas;

static uint16_t *Framebuffer(void)
{
    if (gfxGetScreenFormat(GFX_BOTTOM) != GSP_RGB565_OES)
        return NULL;
    gfxSetDoubleBuffering(GFX_BOTTOM, true);
    return (uint16_t *)gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);
}

static void Grow(Box *box, int x0, int y0, int x1, int y1)
{
    if (!box->on) {
        *box = (Box){true, x0, y0, x1, y1};
        return;
    }
    if (x0 < box->x0) box->x0 = x0;
    if (y0 < box->y0) box->y0 = y0;
    if (x1 > box->x1) box->x1 = x1;
    if (y1 > box->y1) box->y1 = y1;
}

/* Rows [y0, y1) of columns [x0, x1): each column is stored bottom to top, so the contiguous run starting at
 * HEIGHT - y1. */
static void Copy(uint16_t *fb, const uint16_t *canvas, int x0, int y0, int x1, int y1)
{
    size_t rows = (size_t)(y1 - y0);

    for (int x = x0; x < x1; ++x) {
        size_t offset = (size_t)x * CTR_BOTTOM_HEIGHT + (size_t)(CTR_BOTTOM_HEIGHT - y1);

        memcpy(fb + offset, canvas + offset, rows * sizeof(uint16_t));
    }
}

void CtrBottom_BlitRect(const uint16_t *canvas, int x0, int y0, int x1, int y1)
{
    uint16_t *fb = Framebuffer();

    if (fb == NULL || canvas == NULL)
        return;
    if (x0 < 0)
        x0 = 0;
    if (y0 < 0)
        y0 = 0;
    if (x1 > CTR_BOTTOM_WIDTH)
        x1 = CTR_BOTTOM_WIDTH;
    if (y1 > CTR_BOTTOM_HEIGHT)
        y1 = CTR_BOTTOM_HEIGHT;
    if (x0 >= x1 || y0 >= y1)
        return;
    Copy(fb, canvas, x0, y0, x1, y1);
    Grow(&sNow, x0, y0, x1, y1);
    sCanvas = canvas;
}

void CtrBottom_BlitFrameEnd(void)
{
    uint16_t *fb;
    Box both;

    if (!sNow.on)
        return;
    fb = Framebuffer();
    if (fb == NULL || sCanvas == NULL) {
        sNow.on = false;
        return;
    }
    if (sBefore.on)
        Copy(fb, sCanvas, sBefore.x0, sBefore.y0, sBefore.x1, sBefore.y1);
    both = sNow;
    if (sBefore.on)
        Grow(&both, sBefore.x0, sBefore.y0, sBefore.x1, sBefore.y1);
    /* One flush over the span from the first column's run to the last's. */
    {
        size_t first = (size_t)both.x0 * CTR_BOTTOM_HEIGHT + (size_t)(CTR_BOTTOM_HEIGHT - both.y1);
        size_t last = (size_t)(both.x1 - 1) * CTR_BOTTOM_HEIGHT + (size_t)(CTR_BOTTOM_HEIGHT - both.y0);

        GSPGPU_FlushDataCache(fb + first, (uint32_t)((last - first) * sizeof(uint16_t)));
    }
    gfxScreenSwapBuffers(GFX_BOTTOM, false);
    sBefore = sNow;
    sNow.on = false;
}
