#ifndef FIRERED_3DS_DIRTY_RECT_H
#define FIRERED_3DS_DIRTY_RECT_H

#include <stdint.h>

/* Dirty rectangles of the bottom screen's canvas (320x240, in screen
 * coordinates: x right, y down; stored column-major, each column bottom to
 * top). A fixed small list per frame, no allocation. Shared by the PokeTouch
 * drawing, the CPU blit and the GPU chrome upload (and their host tests). */
enum {
    CTR_DIRTY_MAX = 8,
    CTR_DIRTY_W = 320,
    CTR_DIRTY_H = 240,
};

typedef struct {
    int x0, y0, x1, y1;         /* [x0, x1) x [y0, y1) */
} CtrDirtyRect;

typedef struct {
    CtrDirtyRect rects[CTR_DIRTY_MAX];
    int count;
} CtrDirtyList;

static inline int CtrDirtyRect_Overlap(const CtrDirtyRect *a, const CtrDirtyRect *b)
{
    return a->x0 < b->x1 && b->x0 < a->x1 && a->y0 < b->y1 && b->y0 < a->y1;
}

static inline CtrDirtyRect CtrDirtyRect_Union(const CtrDirtyRect *a, const CtrDirtyRect *b)
{
    CtrDirtyRect u = {a->x0 < b->x0 ? a->x0 : b->x0, a->y0 < b->y0 ? a->y0 : b->y0,
                      a->x1 > b->x1 ? a->x1 : b->x1, a->y1 > b->y1 ? a->y1 : b->y1};
    return u;
}

static inline long CtrDirtyRect_Area(const CtrDirtyRect *r)
{
    return (long)(r->x1 - r->x0) * (r->y1 - r->y0);
}

static inline void CtrDirty_Clear(CtrDirtyList *list)
{
    list->count = 0;
}

/* Pixels the list covers (rectangles never overlap in the list). */
static inline long CtrDirty_Pixels(const CtrDirtyList *list)
{
    long n = 0;

    for (int i = 0; i < list->count; ++i)
        n += CtrDirtyRect_Area(&list->rects[i]);
    return n;
}

/* Adds [x0, x1) x [y0, y1), clipped to the canvas. A rectangle that overlaps
 * one already listed is united with it (so the list never holds overlapping
 * rectangles); far-apart changes stay separate. Only when all
 * CTR_DIRTY_MAX slots are taken is the new one united with the listed
 * rectangle it grows least, which may in turn swallow others it now
 * overlaps. */
static inline void CtrDirty_Add(CtrDirtyList *list, int x0, int y0, int x1, int y1)
{
    CtrDirtyRect r = {x0 < 0 ? 0 : x0, y0 < 0 ? 0 : y0,
                      x1 > CTR_DIRTY_W ? CTR_DIRTY_W : x1, y1 > CTR_DIRTY_H ? CTR_DIRTY_H : y1};

    if (r.x0 >= r.x1 || r.y0 >= r.y1)
        return;
    for (;;) {
        int merged = 0;

        for (int i = 0; i < list->count; ++i)
            if (CtrDirtyRect_Overlap(&r, &list->rects[i])) {
                r = CtrDirtyRect_Union(&r, &list->rects[i]);
                list->rects[i] = list->rects[--list->count];
                merged = 1;
                break;
            }
        if (merged)
            continue;
        if (list->count < CTR_DIRTY_MAX)
            break;
        {
            int best = 0;
            long growth = -1;

            for (int i = 0; i < list->count; ++i) {
                CtrDirtyRect u = CtrDirtyRect_Union(&r, &list->rects[i]);
                long g = CtrDirtyRect_Area(&u) - CtrDirtyRect_Area(&list->rects[i]);

                if (growth < 0 || g < growth) {
                    growth = g;
                    best = i;
                }
            }
            r = CtrDirtyRect_Union(&r, &list->rects[best]);
            list->rects[best] = list->rects[--list->count];
        }
    }
    list->rects[list->count++] = r;
}

/* The canvas pixels of every rectangle into a 512-wide RGB565 texture in the GPU's tiled order (8x8 tiles, Morton
 * order inside them: CtrVideo_Texel's layout), texels[texel(x, y)] = canvas(x, y). Nothing outside the rectangles is
 * read or written. A texel's offset is its column's part plus its row's (their bits never overlap), the column's
 * worked out once a column and the row's from a table, so no call is made per pixel. */
static inline void CtrDirty_Upload(const uint16_t *canvas, const CtrDirtyList *list, uint16_t *texels)
{
    static uint32_t rowPart[CTR_DIRTY_H];

    if (rowPart[CTR_DIRTY_H - 1] == 0)
        for (unsigned y = 0; y < CTR_DIRTY_H; ++y)
            rowPart[y] = (y / 8) * (512 / 8) * 64 + (((y & 1) << 1) | ((y & 2) << 2) | ((y & 4) << 3));
    for (int i = 0; i < list->count; ++i) {
        const CtrDirtyRect *r = &list->rects[i];

        for (int x = r->x0; x < r->x1; ++x) {
            const uint16_t *column = canvas + x * CTR_DIRTY_H;
            uint32_t colPart = (uint32_t)(x / 8) * 64 + ((x & 1) | ((x & 2) << 1) | ((x & 4) << 2));

            for (int y = r->y0; y < r->y1; ++y)
                texels[colPart + rowPart[y]] = column[CTR_DIRTY_H - 1 - y];
        }
    }
}

#endif
