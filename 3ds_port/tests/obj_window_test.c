/* Sprite coordinate window of the native field (3ds_obj_window.h).
 *
 * The header is the production code: the compositor (3ds_video.c) unwraps raw OAM with
 * CtrObj_UnwrapX/Y and event_object_movement.c hides sprites with CtrObj_OffScreen, so
 * this test drives exactly what the game runs. LEGACY_* below is the visibility test the
 * port used before (FIELD_VIEW_OFFSET margins on top of the 400x240 view); it is kept
 * only to show the bug it caused.
 */
#include <stdio.h>
#include <stdlib.h>
#include "3ds_obj_window.h"

#define OFFX 80
#define OFFY 32
#define W CTR_OBJ_SCREEN_W
#define H CTR_OBJ_SCREEN_H

static int failures;
#define CHECK(cond, ...) do { if (!(cond)) { if (failures++ < 30) { printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } } while (0)

/* What the old event_object_movement.c decided (nonzero: hide). */
static int legacy_off(int x, int y, int w, int h, int minX)
{
    int x2 = x + w, y2 = y + h;
    return x >= W + OFFX + 16 || x2 < minX - OFFX || y >= H + OFFY + 16 || y2 < -OFFY - 16;
}

/* The production decision, as CalcWhetherObjectIsOffscreen composes it. */
static int off(int x, int y, int w, int h, int minX)
{
    return legacy_off(x, y, w, h, minX) || CtrObj_OffScreen(x, y, x + w, y + h, minX - OFFX, -OFFY - 16);
}

/* Where a sprite that survived the decision ends up: OAM keeps 9 and 8 bits. */
static void drawn_at(int x, int y, int *dx, int *dy)
{
    *dx = CtrObj_UnwrapX(x & 511, W);
    *dy = CtrObj_UnwrapY(y & 255, H);
}

static int touches_screen(int x, int y, int w, int h) { return x < W && x + w > 0 && y < H && y + h > 0; }

static const int sizes[] = {8, 16, 32, 64};

/* Count the sprites a decision lets through that draw somewhere other than where they are. */
static int ghosts(int (*decide)(int, int, int, int, int), int minX, int *firstX, int *firstY, int *firstW, int *firstH)
{
    int n = 0;
    for (int si = 0; si < 4; ++si)
        for (int sj = 0; sj < 4; ++sj)
            for (int y = -400; y <= 700; ++y)
                for (int x = -700; x <= 900; x += 1)
                {
                    int w = sizes[si], h = sizes[sj];
                    if (decide(x, y, w, h, minX)) continue;
                    int dx, dy;
                    drawn_at(x, y, &dx, &dy);
                    if (dx != x || dy != y)
                    {
                        if (!n) { *firstX = x; *firstY = y; *firstW = w; *firstH = h; }
                        ++n;
                    }
                }
    return n;
}

static void test_the_old_decision_ghosts(void)
{
    int fx = 0, fy = 0, fw = 0, fh = 0;
    int n = ghosts(legacy_off, -16, &fx, &fy, &fw, &fh);
    CHECK(n > 0, "the legacy visibility test is expected to alias; it did not");
    printf("legacy decision: %d aliased placements, first %dx%d at (%d,%d)\n", n, fw, fh, fx, fy);
}

static void test_no_sprite_is_drawn_elsewhere(void)
{
    int fx, fy, fw, fh;
    CHECK(ghosts(off, -16, &fx, &fy, &fw, &fh) == 0, "aliased placement %dx%d at (%d,%d)", fw, fh, fx, fy);
    CHECK(ghosts(off, -32, &fx, &fy, &fw, &fh) == 0, "Ss Anne margin: aliased %dx%d at (%d,%d)", fw, fh, fx, fy);
}

/* Anything representable that shows a pixel must still be shown. */
static void test_visible_sprites_survive(void)
{
    for (int si = 0; si < 4; ++si)
        for (int sj = 0; sj < 4; ++sj)
            for (int y = -100; y < 340; ++y)
                for (int x = -200; x < 500; ++x)
                {
                    int w = sizes[si], h = sizes[sj];
                    if (!touches_screen(x, y, w, h) || !CtrObj_TopLeftRepresentable(x, y)) continue;
                    CHECK(!off(x, y, w, h, -16), "%dx%d at (%d,%d) shows pixels but was hidden", w, h, x, y);
                }
}

static void test_edges(void)
{
    /* Y: -16 and 0 are shown; 239 is the last line; 240, 255, 256 and 288 are below. */
    CHECK(!off(100, -16, 16, 32, -16), "16x32 at y=-16 must show its lower half");
    CHECK(!off(100, 0, 16, 16, -16), "y=0");
    CHECK(!off(100, 239, 16, 16, -16), "y=239 shows one line");
    CHECK(off(100, 240, 16, 16, -16), "y=240 is below the screen");
    CHECK(off(100, 255, 16, 16, -16), "y=255");
    CHECK(off(100, 256, 16, 16, -16), "y=256 aliases y=0");
    CHECK(off(100, 288, 16, 16, -16), "y=288 aliases y=32");
    /* Tall sprites above the top: the part of the range that OAM cannot carry. */
    CHECK(!off(100, -15, 16, 16, -16), "16x16 at y=-15 shows one line");
    CHECK(off(100, -17, 16, 16, -16), "16x16 at y=-17 shows nothing");
    /* X */
    CHECK(!off(-15, 100, 16, 16, -16), "x=-15 shows one column");
    CHECK(!off(399, 100, 16, 16, -16), "x=399 shows one column");
    CHECK(off(400, 100, 16, 16, -16), "x=400 is right of the screen");
    CHECK(off(480, 100, 32, 16, -16), "x=480 aliases x=-32: a 32-wide sprite would reappear on the left");
    CHECK(off(512, 100, 16, 16, -16), "x=512 aliases x=0");
    CHECK(!off(-100, 100, 128, 64, -32), "a wide sprite entering from the left is still carried");
}

/* Route 8 (pret/pokefirered 037335f4, data/maps/Route8/map.json): two Cut trees, 16x32 graphics. */
static const struct { int tx, ty; } cutTrees[] = {{33, 15}, {47, 12}};

/* A tile is 16 px; the camera shows the 400x240 view with the player's tile at its centre.
 * Screen position of an object's top-left = world pixel minus camera pixel, in screen space. */
static void screen_pos(int tx, int ty, int camX, int camY, int *sx, int *sy)
{
    *sx = tx * 16 - camX;
    *sy = ty * 16 - camY - 16; /* 16x32 sprites stand one tile above their tile */
}

static void test_route8_cut_trees_all_cameras(void)
{
    int shown[2] = {0, 0};
    /* Route 8 is 60x? tiles; sweep every camera position the player can reach. */
    for (int camY = -240; camY < 60 * 16; ++camY)
        for (int camX = -400; camX < 80 * 16; camX += 3)
            for (int t = 0; t < 2; ++t)
            {
                int sx, sy, dx, dy;
                screen_pos(cutTrees[t].tx, cutTrees[t].ty, camX, camY, &sx, &sy);
                if (off(sx, sy, 16, 32, -16)) continue;
                drawn_at(sx, sy, &dx, &dy);
                CHECK(dx == sx && dy == sy, "Cut tree %d at camera (%d,%d): true (%d,%d) drawn at (%d,%d)",
                      t, camX, camY, sx, sy, dx, dy);
                ++shown[t];
                if (!(dx == sx && dy == sy)) return;
            }
    CHECK(shown[0] > 0 && shown[1] > 0, "both trees must be visible from some camera position");
}

/* The bug as reported: the tree at (33,15) wraps to the top while it is below the view. */
static void test_route8_reported_case(void)
{
    int found = 0;
    for (int camY = -100; camY < 40 * 16 && !found; ++camY)
    {
        int sx, sy, dx, dy;
        screen_pos(33, 15, 33 * 16 - 200, camY, &sx, &sy);
        if (sy < 256 || sy >= 288) continue;     /* the old margin that aliases 0..31 */
        found = 1;
        drawn_at(sx, sy, &dx, &dy);
        CHECK(!legacy_off(sx, sy, 16, 32, -16) && dy < 32, "legacy test should keep it and wrap it to the top (sy=%d dy=%d)", sy, dy);
        CHECK(off(sx, sy, 16, 32, -16), "the tree 256+ lines below the view must be hidden (sy=%d)", sy);
    }
    CHECK(found, "no camera position put the tree in the old margin");
}

/* Scrolling one pixel at a time never makes a sprite jump: shown positions move by exactly the step. */
static void test_scrolling_is_continuous(void)
{
    for (int dir = 0; dir < 4; ++dir)
        for (int si = 0; si < 4; ++si)
        {
            int w = sizes[si], h = 32 > sizes[si] ? 32 : sizes[si];
            int prevShown = 0, px = 0, py = 0;
            int stepX = dir == 0 ? 1 : dir == 1 ? -1 : 0, stepY = dir == 2 ? 1 : dir == 3 ? -1 : 0;
            int x = 150 - stepX * 700, y = 100 - stepY * 700;
            for (int i = 0; i < 1400; ++i, x += stepX, y += stepY)
            {
                int dx, dy, shown = !off(x, y, w, h, -16);
                if (shown)
                {
                    drawn_at(x, y, &dx, &dy);
                    CHECK(dx == x && dy == y, "dir %d: drawn (%d,%d) true (%d,%d)", dir, dx, dy, x, y);
                    if (prevShown) CHECK(abs(dx - px) + abs(dy - py) == 1, "dir %d: sprite jumped", dir);
                    px = dx; py = dy;
                }
                prevShown = shown;
            }
        }
}

static void test_neighbouring_sprites_are_independent(void)
{
    /* Two trees 16 lines apart straddling the old margin: each decided on its own geometry. */
    CHECK(off(100, 256, 16, 32, -16) && !off(100, 224, 16, 32, -16), "neighbours at y=256 and y=224");
    CHECK(off(100, -48, 16, 32, -16) && !off(100, -16, 16, 32, -16), "neighbours at y=-48 and y=-16");
}

/* Entering a map rebuilds every object's sprite and decides again from scratch: the decision is a
 * pure function of position, so a second pass over the same geometry gives the same answers. */
static void test_decision_is_stateless(void)
{
    for (int y = -100; y < 400; y += 7)
        for (int x = -200; x < 600; x += 5)
            CHECK(off(x, y, 16, 32, -16) == off(x, y, 16, 32, -16), "decision changed between calls");
}

int main(void)
{
    test_the_old_decision_ghosts();
    test_no_sprite_is_drawn_elsewhere();
    test_visible_sprites_survive();
    test_edges();
    test_route8_cut_trees_all_cameras();
    test_route8_reported_case();
    test_scrolling_is_continuous();
    test_neighbouring_sprites_are_independent();
    test_decision_is_stateless();
    if (failures) { printf("%d failures\n", failures); return 1; }
    printf("PASS sprite window: no aliasing, partial edges kept, Route 8 Cut trees, scrolling\n");
    return 0;
}
