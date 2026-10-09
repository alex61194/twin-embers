/* Native half of the bottom screen: the only part that touches libctru.
 *
 * The game-side canvas already has the framebuffer layout (RGB565,
 * column-major, each column bottom-to-top), so presenting is a copy of
 * whole columns plus a data-cache flush over the same range.
 */
#include <3ds.h>
#include <string.h>
#include "3ds_bottom.h"

static uint16_t *Framebuffer(void)
{
    if (gfxGetScreenFormat(GFX_BOTTOM) != GSP_RGB565_OES)
        return NULL;
    return (uint16_t *)gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);
}

void CtrBottom_BlitRect(const uint16_t *canvas, int x0, int y0, int x1, int y1)
{
    uint16_t *fb = Framebuffer();
    size_t rows;

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
    /* Each column is stored bottom to top: rows [y0, y1) are the contiguous
     * run starting at HEIGHT - y1. */
    rows = (size_t)(y1 - y0);
    for (int x = x0; x < x1; ++x) {
        size_t offset = (size_t)x * CTR_BOTTOM_HEIGHT + (size_t)(CTR_BOTTOM_HEIGHT - y1);

        memcpy(fb + offset, canvas + offset, rows * sizeof(uint16_t));
    }
    /* One flush over the span from the first column's run to the last's. */
    {
        size_t first = (size_t)x0 * CTR_BOTTOM_HEIGHT + (size_t)(CTR_BOTTOM_HEIGHT - y1);
        size_t last = (size_t)(x1 - 1) * CTR_BOTTOM_HEIGHT + (size_t)(CTR_BOTTOM_HEIGHT - y0);

        GSPGPU_FlushDataCache(fb + first, (uint32_t)((last - first) * sizeof(uint16_t)));
    }
}
