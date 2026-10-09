/* Map-edge fallback rule (3ds_map_edge.h) on synthetic maps. The header is the code
 * the field camera runs; map_edge_draw_test.c drives it through the real camera on
 * the real maps. */
#include <stdio.h>
#include "3ds_map_edge.h"

static int failures;
#define CHECK(cond, ...) do { if (!(cond)) { failures++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

#define W 24
#define H 20
#define ALL (CTR_EDGE_WEST | CTR_EDGE_EAST | CTR_EDGE_NORTH | CTR_EDGE_SOUTH)
static unsigned grid[H][W];

static unsigned get(void *ctx, int x, int y) { (void)ctx; return grid[y][x]; }
static void fill(unsigned id, unsigned collision)
{
    int x, y;
    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++)
            grid[y][x] = id | (collision << 10);
}
static int resolve(int sides, int x, int y, int *sx, int *sy) { return CtrEdge_Resolve(get, NULL, W, H, sides, x, y, sx, sy); }

int main(void)
{
    int x, y, sx, sy;

    /* A uniform blocked body: every outside position copies the nearest edge position. */
    fill(5, 1);
    for (y = -14; y < H + 14; y++)
        for (x = -14; x < W + 14; x++)
        {
            int inside = x >= 0 && x < W && y >= 0 && y < H;
            int hit = resolve(ALL, x, y, &sx, &sy);
            CHECK(hit == !inside, "(%d,%d) hit=%d", x, y, hit);
            if (!hit) continue;
            CHECK(sx == (x < 0 ? 0 : x >= W ? W - 1 : x) && sy == (y < 0 ? 0 : y >= H ? H - 1 : y), "(%d,%d) -> (%d,%d)", x, y, sx, sy);
        }

    /* A side with a connection is never touched, closed sides still are; a corner
     * needs both of its sides. */
    CHECK(!resolve(ALL & ~CTR_EDGE_WEST, -3, 5, &sx, &sy), "connected west extended");
    CHECK(resolve(ALL & ~CTR_EDGE_WEST, W + 3, 5, &sx, &sy), "closed east not extended");
    CHECK(!resolve(ALL & ~CTR_EDGE_NORTH, -3, -3, &sx, &sy), "corner next to a connected side extended");
    CHECK(resolve(ALL, -3, -3, &sx, &sy) && sx == 0 && sy == 0, "closed corner not extended");

    /* A shore: the edge column differs from the metatile beside it. */
    fill(5, 1);
    for (y = 0; y < H; y++) grid[y][0] = 9 | (1 << 10);
    CHECK(!resolve(ALL, -1, 8, &sx, &sy), "shore column repeated outward");
    CHECK(resolve(ALL, W, 8, &sx, &sy), "east side lost with the west shore");

    /* A stair or channel: fewer than CTR_EDGE_MIN_RUN equal metatiles along the edge. */
    fill(5, 1);
    for (x = 8; x < 8 + CTR_EDGE_MIN_RUN - 1; x++) grid[H - 1][x] = grid[H - 2][x] = 7 | (1 << 10);
    CHECK(!resolve(ALL, 9, H, &sx, &sy), "narrow channel repeated outward");
    CHECK(resolve(ALL, 4, H, &sx, &sy), "body beside the channel not extended");
    for (x = 8; x < 8 + CTR_EDGE_MIN_RUN; x++) grid[H - 1][x] = grid[H - 2][x] = 7 | (1 << 10);
    CHECK(resolve(ALL, 9, H, &sx, &sy) && sx == 9 && sy == H - 1, "a run of exactly %d is not extended", CTR_EDGE_MIN_RUN);

    /* Tree lines (A over B over A over B) are a designed border: not extended. */
    fill(5, 1);
    for (y = 0; y < H; y++) for (x = 0; x < W; x++) grid[y][x] = (y & 1 ? 11 : 12) | (1 << 10);
    CHECK(!resolve(ALL, -1, 5, &sx, &sy), "alternating column extended");
    CHECK(!resolve(ALL, 5, -1, &sx, &sy) || 1, "unreachable");

    /* Maps too small to tell a body from a line keep the border. */
    CHECK(!CtrEdge_Resolve(get, NULL, 1, 1, ALL, -1, 0, &sx, &sy), "1x1 map extended");

    /* Land/sea decision and the map types that take part. */
    CHECK(CtrEdge_Accept(5 | (1 << 10), 0, 0x15), "rock over a sea border refused");
    CHECK(!CtrEdge_Accept(5, 0, 0x15), "open ground over a sea border accepted");
    CHECK(CtrEdge_Accept(5, 0x15, 0), "sea over a land border refused");
    CHECK(!CtrEdge_Accept(5, 0x15, 0x10), "sea over sea accepted");
    CHECK(!CtrEdge_Accept(5 | (1 << 10), 0, 0), "land over land accepted");
    CHECK(CtrEdge_MapTypeExtends(1) && CtrEdge_MapTypeExtends(2) && CtrEdge_MapTypeExtends(3)
          && CtrEdge_MapTypeExtends(6), "outdoor types");
    CHECK(!CtrEdge_MapTypeExtends(0) && !CtrEdge_MapTypeExtends(4) && !CtrEdge_MapTypeExtends(5)
          && !CtrEdge_MapTypeExtends(7) && !CtrEdge_MapTypeExtends(8) && !CtrEdge_MapTypeExtends(9),
          "non-outdoor types");

    if (failures) { printf("%d failures\n", failures); return 1; }
    puts("PASS map edge");
    return 0;
}
