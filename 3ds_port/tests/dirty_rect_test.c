/* The dirty-rectangle list (include/3ds_dirty_rect.h): coverage, no overlaps, and that 22 embers sparks a frame (the
 * New 3DS default on HIGH) keep the presented area near what the sparks really touch. Pure C. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "3ds_dirty_rect.h"

static uint32_t sSeed = 12345;
static int rnd(int n)
{
    sSeed = sSeed * 1664525u + 1013904223u;
    return (int)((sSeed >> 8) % (uint32_t)n);
}

int main(void)
{
    static unsigned char marked[CTR_DIRTY_W * CTR_DIRTY_H];
    long total = 0, sparkTotal = 0;
    const int frames = 2000, sparks = 22, size = 5;

    assert(CTR_DIRTY_MAX >= 24);
    for (int f = 0; f < frames; ++f) {
        CtrDirtyList list;

        CtrDirty_Clear(&list);
        memset(marked, 0, sizeof(marked));
        for (int i = 0; i < sparks; ++i) {
            int x = rnd(CTR_DIRTY_W + 8) - 4, y = rnd(CTR_DIRTY_H + 8) - 4;

            CtrDirty_Add(&list, x, y, x + size, y + size);
            for (int v = 0; v < size; ++v)
                for (int u = 0; u < size; ++u)
                    if ((unsigned)(x + u) < CTR_DIRTY_W && (unsigned)(y + v) < CTR_DIRTY_H)
                        marked[(y + v) * CTR_DIRTY_W + x + u] = 1;
        }
        assert(list.count <= CTR_DIRTY_MAX);
        /* Every changed pixel is inside the list, and the rectangles never overlap. */
        for (int y = 0; y < CTR_DIRTY_H; ++y)
            for (int x = 0; x < CTR_DIRTY_W; ++x)
                if (marked[y * CTR_DIRTY_W + x]) {
                    int in = 0;

                    ++sparkTotal;
                    for (int i = 0; i < list.count; ++i)
                        in |= x >= list.rects[i].x0 && x < list.rects[i].x1 && y >= list.rects[i].y0 && y < list.rects[i].y1;
                    assert(in);
                }
        for (int i = 0; i < list.count; ++i)
            for (int j = i + 1; j < list.count; ++j)
                assert(!CtrDirtyRect_Overlap(&list.rects[i], &list.rects[j]));
        total += CtrDirty_Pixels(&list);
    }
    printf("dirty rect: %ld px presented/frame, %ld px touched/frame (%.1f%% of the canvas)\n", total / frames, sparkTotal / frames,
           100.0 * (double)(total / frames) / (CTR_DIRTY_W * CTR_DIRTY_H));
    /* With CTR_DIRTY_MAX = 8 this was about 28% of the canvas; it must stay well under that. */
    assert(total / frames < CTR_DIRTY_W * CTR_DIRTY_H * 12 / 100);
    puts("dirty rect: ok");
    return 0;
}
