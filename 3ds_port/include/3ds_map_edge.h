#ifndef CTR_MAP_EDGE_H
#define CTR_MAP_EDGE_H

/* Visual-only continuation of closed map edges.
 *
 * The GBA fills everything outside a map with the layout's 2x2 border block. On a
 * 240x160 screen that is barely seen; the 400x240 native view shows a lot of it, so
 * a mountain can end in a straight line and turn into sea. Repeating the last
 * row/column blindly is worse (a shore or cliff-edge tile becomes a field of
 * stripes, a staircase or a narrow channel becomes a long structure, a tree line
 * becomes a wall of forest), so a position outside the map takes its metatile from
 * inside the map only when the edge is a wide body of one plain metatile, as the map
 * data itself shows it:
 *
 *   - the same metatile fills the edge and the position behind it (not a shore or
 *     cliff face, whose tile differs from the one inside), and
 *   - it forms a run of at least CTR_EDGE_MIN_RUN equal metatiles along the edge
 *     (not a stair, a channel or a gate house: about half the screen or more).
 *
 * Anything else (shores, cliff corners, stairs, fences, houses, paths, trees) keeps
 * the border block. Of what remains, only a land/sea change is repaired
 * (CtrEdge_Accept): blocked terrain (rock, cliff body) over a sea border, or sea over
 * a land border. Land over land is a designed surround (a forest of trees, a rock
 * wall) and sea over sea only leaves a seam of slightly different shades; open ground
 * such as a road would read as a corridor running off the map. Sides that have a connection are never touched (the
 * connected map, or the border where it does not reach, is what the game always
 * showed). A corner is extended only when both of its sides qualify. Only the
 * picture changes: collision, behaviour, events and the save grid never see this. */

#include <stdbool.h>

/* MAP_TYPE_TOWN 1, MAP_TYPE_CITY 2, MAP_TYPE_ROUTE 3, MAP_TYPE_OCEAN_ROUTE 6. */
static inline bool CtrEdge_MapTypeExtends(unsigned mapType)
{
    return mapType == 1 || mapType == 2 || mapType == 3 || mapType == 6;
}

/* Sea metatile behaviours (MB_POND_WATER, MB_FAST_WATER, MB_DEEP_WATER, MB_OCEAN_WATER). */
static inline bool CtrEdge_IsSeaBehavior(unsigned behavior)
{
    return behavior == 0x10 || behavior == 0x11 || behavior == 0x12 || behavior == 0x15;
}

/* Map grid block: metatile id in bits 0-9, collision in bits 10-11. */
#define CTR_EDGE_ID(block) ((block) & 0x3FFu)
#define CTR_EDGE_BLOCKED(block) (((block) & 0xC00u) != 0)

/* May the in-map block `src` (with its behaviour) replace the border block whose
 * behaviour is borderBehavior? Only where land meets sea. */
static inline bool CtrEdge_Accept(unsigned srcBlock, unsigned srcBehavior, unsigned borderBehavior)
{
    bool srcSea = CtrEdge_IsSeaBehavior(srcBehavior), borderSea = CtrEdge_IsSeaBehavior(borderBehavior);
    if (srcSea == borderSea)
        return false;
    return srcSea || CTR_EDGE_BLOCKED(srcBlock);
}

enum
{
    CTR_EDGE_WEST = 1,
    CTR_EDGE_EAST = 2,
    CTR_EDGE_NORTH = 4,
    CTR_EDGE_SOUTH = 8
};

#define CTR_EDGE_MIN_RUN 8
#define CTR_EDGE_MIN_DEPTH 2

/* Map grid block of a real map position (0 <= x < width, 0 <= y < height). */
typedef unsigned (*CtrEdge_MetatileFn)(void *ctx, int x, int y);

/* Length of the run of equal metatiles through (x,y) along the row (horizontal) or
 * the column; size = the map's length on that axis. */
static inline int CtrEdge_Run(CtrEdge_MetatileFn get, void *ctx, int x, int y, bool horizontal, int size)
{
    unsigned t = CTR_EDGE_ID(get(ctx, x, y));
    int n = 1, p = horizontal ? x : y, q;
    for (q = p - 1; q >= 0 && CTR_EDGE_ID(get(ctx, horizontal ? q : x, horizontal ? y : q)) == t; q--)
        n++;
    for (q = p + 1; q < size && CTR_EDGE_ID(get(ctx, horizontal ? q : x, horizontal ? y : q)) == t; q++)
        n++;
    return n;
}

/* Is the edge metatile at (ex,ey) the start of a plain body: the same metatile for
 * CTR_EDGE_MIN_DEPTH positions inward ((sx,sy) = unit step), in a run of
 * CTR_EDGE_MIN_RUN or more along the edge (the edge line has alongSize metatiles,
 * horizontal when alongX)? */
static inline bool CtrEdge_Plain(CtrEdge_MetatileFn get, void *ctx, int ex, int ey, int sx, int sy,
                                 bool alongX, int alongSize)
{
    unsigned t = CTR_EDGE_ID(get(ctx, ex, ey));
    int k;
    for (k = 1; k < CTR_EDGE_MIN_DEPTH; k++)
        if (CTR_EDGE_ID(get(ctx, ex + k * sx, ey + k * sy)) != t)
            return false;
    return CtrEdge_Run(get, ctx, ex, ey, alongX, alongSize) >= CTR_EDGE_MIN_RUN;
}

/* Resolve a map-relative position (0,0 = first real metatile). Returns false when it
 * is inside the map or must keep the border block; otherwise stores the in-map
 * position whose metatile to copy in outX and outY (the clamped edge position).
 * closedSides = bit mask of the sides without a connection. */
static inline bool CtrEdge_Resolve(CtrEdge_MetatileFn get, void *ctx, int width, int height,
                                   int closedSides, int mapX, int mapY, int *outX, int *outY)
{
    bool west = mapX < 0, east = mapX >= width, north = mapY < 0, south = mapY >= height;
    int cx = west ? 0 : (east ? width - 1 : mapX);
    int cy = north ? 0 : (south ? height - 1 : mapY);

    if (!(west || east || north || south))
        return false;
    if (width < CTR_EDGE_MIN_DEPTH || height < CTR_EDGE_MIN_DEPTH)
        return false;
    if ((west && !(closedSides & CTR_EDGE_WEST)) || (east && !(closedSides & CTR_EDGE_EAST))
        || (north && !(closedSides & CTR_EDGE_NORTH)) || (south && !(closedSides & CTR_EDGE_SOUTH)))
        return false;
    if ((west || east) && !CtrEdge_Plain(get, ctx, cx, cy, west ? 1 : -1, 0, false, height))
        return false;
    if ((north || south) && !CtrEdge_Plain(get, ctx, cx, cy, 0, north ? 1 : -1, true, width))
        return false;
    *outX = cx;
    *outY = cy;
    return true;
}

#endif
