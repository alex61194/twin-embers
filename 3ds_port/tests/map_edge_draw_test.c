/* Map-edge fallback through the production field camera.
 *
 * This translation unit includes the bootstrapped, patched fieldmap.c and
 * field_camera.c, so DrawWholeMapView, CameraUpdate, CameraMove, the backup map and
 * its connections are the code the game runs. Only the hardware around them is
 * replaced. Every outdoor map of the pinned upstream tree is drawn twice, with the
 * fallback and as vanilla (the map type forced to indoor), and the two pictures may
 * differ only where the documented rule allows it. The Cape Brink west shore from the
 * reported screenshot is checked explicitly.
 *
 * usage: map_edge_draw_test TREE WORLD.txt */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "global.h"
#include "fieldmap.h"
#include "field_camera.h"
#include "field_player_avatar.h"
#include "event_object_movement.h"
#include "sprite.h"
#include "gpu_regs.h"
#include "overworld.h"
#include "constants/map_types.h"
#include "3ds_map_edge.h"

/* ---- the hardware and game state the camera code touches ------------------------ */
struct SaveBlock1 sSave1;
struct SaveBlock2 sSave2;
struct SaveBlock1 *gSaveBlock1Ptr = &sSave1;
struct SaveBlock2 *gSaveBlock2Ptr = &sSave2;
static u16 sTilemap[3][0x800];
u16 *gBGTilemapBuffers1 = sTilemap[0];
u16 *gBGTilemapBuffers2 = sTilemap[1];
u16 *gBGTilemapBuffers3 = sTilemap[2];
struct Sprite gSprites[MAX_SPRITES + 1];
struct PlayerAvatar gPlayerAvatar;
s16 gSpriteCoordOffsetX, gSpriteCoordOffsetY;
u16 gPlttBufferFaded[0x200], gPlttBufferUnfaded[0x200];
const struct Coords32 gDirectionToVectors[] = {
    [DIR_NONE] = {0, 0}, [DIR_SOUTH] = {0, 1}, [DIR_NORTH] = {0, -1}, [DIR_WEST] = {-1, 0}, [DIR_EAST] = {1, 0},
    [DIR_SOUTHWEST] = {-1, 1}, [DIR_SOUTHEAST] = {1, 1}, [DIR_NORTHWEST] = {-1, -1}, [DIR_NORTHEAST] = {1, -1},
};
u8 AddCameraObject(u8 id) { (void)id; return 0; }
void DestroySprite(struct Sprite *s) { (void)s; }
u8 GetPlayerMovementDirection(void) { return DIR_SOUTH; }
void UpdateObjectEventsForCameraUpdate(s16 x, s16 y) { (void)x; (void)y; }
void ScheduleBgCopyTilemapToVram(u8 bg) { (void)bg; }
void SetGpuReg(u8 reg, u16 value) { (void)reg; (void)value; }
void AGBAssert(const char *f, int l, const char *e, int s) { (void)f; (void)l; (void)e; (void)s; }
void CpuSet(const void *src, void *dst, u32 control)
{
    u32 count = control & 0x1FFFFF;
    int fill = (control >> 24) & 1, wide = (control >> 26) & 1;
    u32 i;
    for (i = 0; i < count; i++)
        if (wide)
            ((u32 *)dst)[i] = ((const u32 *)src)[fill ? 0 : i];
        else
            ((u16 *)dst)[i] = ((const u16 *)src)[fill ? 0 : i];
}
void CpuFastSet(const void *src, void *dst, u32 control) { CpuSet(src, dst, control | (1u << 26)); }
void *DecompressAndCopyTileDataToVram2(u8 a, const void *b, u32 c, u16 d, u8 e) { (void)a; (void)b; (void)c; (void)d; (void)e; return NULL; }
void DecompressAndLoadBgGfxUsingHeap2(u8 a, const void *b, u32 c, u16 d, u8 e) { (void)a; (void)b; (void)c; (void)d; (void)e; }
u16 LoadBgTiles(u8 a, const void *b, u16 c, u16 d) { (void)a; (void)b; (void)c; (void)d; return 0; }
void LoadCompressedPalette(const u32 *a, u16 b, u16 c) { (void)a; (void)b; (void)c; }
void LoadPalette(const void *a, u16 b, u16 c) { (void)a; (void)b; (void)c; }
void QuestLog_BackUpPalette(u16 a, u16 b) { (void)a; (void)b; }
void RunOnLoadMapScript(void) {}
void TintPalette_GrayScale(u16 *a, u16 b) { (void)a; (void)b; }
void TintPalette_SepiaTone(u16 *a, u16 b) { (void)a; (void)b; }

#include "fieldmap.c"
#include "field_camera.c"

#undef malloc
#undef calloc
#undef free
void *calloc(size_t, size_t);

/* ---- the world, built from the tree --------------------------------------------- */
#define MAX_MAPS 600
static struct MapHeader sHeaders[MAX_MAPS];
static struct MapLayout sLayouts[MAX_MAPS];
static struct Tileset sTilesets[MAX_MAPS][2];
static struct MapConnections sConns[MAX_MAPS];
static struct MapConnection sConnList[MAX_MAPS][4];
static char sNames[MAX_MAPS][64];
static int sMapCount;
static const char *sTree;
static int failures;
#define CHECK(cond, ...) do { if (!(cond)) { if (failures++ < 40) { printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } } while (0)

const struct MapHeader *Overworld_GetMapHeaderByGroupAndId(u16 group, u16 num) { return &sHeaders[group * 256 + num]; }
void LoadMapFromCameraTransition(u8 group, u8 num) { gMapHeader = sHeaders[group * 256 + num]; InitMap(); }

static void *slurp(const char *rel, size_t *size)
{
    char path[512];
    FILE *f;
    void *data;
    snprintf(path, sizeof path, "%s/%s", sTree, rel);
    f = fopen(path, "rb");
    if (!f) { printf("cannot open %s\n", path); exit(2); }
    fseek(f, 0, SEEK_END);
    *size = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    /* Real cartridges read past a short table into whatever follows; give it zeros. */
    data = calloc(*size + 0x4000, 1);
    if (fread(data, 1, *size, f) != *size) exit(2);
    fclose(f);
    return data;
}

static void load_world(const char *worldPath)
{
    FILE *f = fopen(worldPath, "r");
    char tag[8], mapbin[256], borderbin[256], pm[256], pa[256], sm[256], sa[256], name[64];
    int idx = -1, type, w, h, bw, bh, nconn, left = 0;
    if (!f) { printf("cannot open %s\n", worldPath); exit(2); }
    while (fscanf(f, "%7s", tag) == 1)
    {
        if (!strcmp(tag, "MAP"))
        {
            size_t n;
            struct MapLayout *l;
            if (left) { printf("bad world\n"); exit(2); }
            if (fscanf(f, "%d %d %d %d %d %d %255s %255s %255s %255s %255s %255s %63s %d", &idx, &type, &w, &h, &bw, &bh,
                       mapbin, borderbin, pm, pa, sm, sa, name, &nconn) != 14) exit(2);
            l = &sLayouts[idx];
            l->width = w; l->height = h; l->borderWidth = bw; l->borderHeight = bh;
            l->map = slurp(mapbin, &n); l->border = slurp(borderbin, &n);
            sTilesets[idx][0].metatiles = slurp(pm, &n); sTilesets[idx][0].metatileAttributes = slurp(pa, &n);
            sTilesets[idx][1].metatiles = slurp(sm, &n); sTilesets[idx][1].metatileAttributes = slurp(sa, &n);
            sTilesets[idx][1].isSecondary = TRUE;
            l->primaryTileset = &sTilesets[idx][0]; l->secondaryTileset = &sTilesets[idx][1];
            sHeaders[idx].mapLayout = l; sHeaders[idx].mapType = type;
            sHeaders[idx].connections = nconn ? &sConns[idx] : NULL;
            sConns[idx].count = 0; sConns[idx].connections = sConnList[idx];
            strncpy(sNames[idx], name, 63);
            left = nconn;
            if (idx + 1 > sMapCount) sMapCount = idx + 1;
        }
        else if (!strcmp(tag, "CONN"))
        {
            char d; int off, target;
            struct MapConnection *c = &sConnList[idx][sConns[idx].count++];
            if (fscanf(f, " %c %d %d", &d, &off, &target) != 3 || left-- <= 0) exit(2);
            c->direction = d == 'S' ? CONNECTION_SOUTH : d == 'N' ? CONNECTION_NORTH : d == 'W' ? CONNECTION_WEST : CONNECTION_EAST;
            c->offset = off; c->mapGroup = target >> 8; c->mapNum = target & 255;   /* group*256+num is the index */
        }
    }
    fclose(f);
}

static int find_map(const char *name)
{
    int i;
    for (i = 0; i < sMapCount; i++)
        if (!strcmp(sNames[i], name)) return i;
    printf("no map %s\n", name);
    exit(2);
}

/* ---- state helpers --------------------------------------------------------------- */
static int sVanilla;   /* 1: every map reports a type that never extends */
static unsigned char sRealType[MAX_MAPS];

static void set_mode(int vanilla)
{
    int i;
    sVanilla = vanilla;
    for (i = 0; i < sMapCount; i++)
        sHeaders[i].mapType = vanilla ? MAP_TYPE_INDOOR : sRealType[i];
}

static void enter_map(int idx, int posX, int posY)
{
    if (getenv("MAP_EDGE_TRACE")) fprintf(stderr, "enter %s %d,%d\n", sNames[idx], posX, posY);
    gMapHeader = sHeaders[idx];
    InitMap();
    gSaveBlock1Ptr->pos.x = posX;
    gSaveBlock1Ptr->pos.y = posY;
    memset(sTilemap, 0, sizeof sTilemap);
    memset(&sFieldCameraOffset, 0, sizeof sFieldCameraOffset);
    memset(&gFieldCamera, 0, sizeof gFieldCamera);
    DrawWholeMapView();
}

/* One metatile as the tilemap holds it: its four tiles on each of the three layers,
 * read through the ring buffer the camera maintains. */
struct Cell { u16 t[3][4]; };
static void read_cell(int j, int i, struct Cell *c)   /* j,i: even tile offsets (j may be negative) from the view origin */
{
    int b, k;
    for (b = 0; b < 3; b++)
        for (k = 0; k < 4; k++)
        {
            u16 off = TilemapOffset(sFieldCameraOffset.xTileOffset + j + (k & 1), sFieldCameraOffset.yTileOffset + i + (k >> 1));
            c->t[b][k] = sTilemap[b][off];
        }
}

static void expected_cell(const struct MapLayout *l, u16 id, u8 layer, struct Cell *c)
{
    const u16 *tiles;
    int k;
    memset(c, 0, sizeof *c);
    if (id > NUM_METATILES_TOTAL) id = 0;
    if (id < NUM_METATILES_IN_PRIMARY) tiles = l->primaryTileset->metatiles + id * NUM_TILES_PER_METATILE;
    else tiles = l->secondaryTileset->metatiles + (id - NUM_METATILES_IN_PRIMARY) * NUM_TILES_PER_METATILE;
    /* DrawMetatile, restated: t[0] = BG1 buffer, t[1] = BG2, t[2] = BG3. */
    switch (layer)
    {
    case METATILE_LAYER_TYPE_SPLIT:
        for (k = 0; k < 4; k++) { c->t[2][k] = tiles[k]; c->t[1][k] = tiles[4 + k]; }
        break;
    case METATILE_LAYER_TYPE_COVERED:
        for (k = 0; k < 4; k++) { c->t[2][k] = tiles[k]; c->t[0][k] = tiles[4 + k]; }
        break;
    default:
        for (k = 0; k < 4; k++) { c->t[2][k] = 0x3014; c->t[0][k] = tiles[k]; c->t[1][k] = tiles[4 + k]; }
        break;
    }
}

static int same(const struct Cell *a, const struct Cell *b) { return !memcmp(a, b, sizeof *a); }

/* ---- the rule, restated from the contract ---------------------------------------- */
static unsigned vmap_metatile(void *ctx, int x, int y) { (void)ctx; return VMap.map[(x + MAP_OFFSET) + VMap.Xsize * (y + MAP_OFFSET)]; }

static int closed_sides(void)
{
    int closed = 15, i;
    if (gMapHeader.connections)
        for (i = 0; i < gMapHeader.connections->count; i++)
            switch (gMapHeader.connections->connections[i].direction)
            {
            case CONNECTION_WEST: closed &= ~CTR_EDGE_WEST; break;
            case CONNECTION_EAST: closed &= ~CTR_EDGE_EAST; break;
            case CONNECTION_NORTH: closed &= ~CTR_EDGE_NORTH; break;
            case CONNECTION_SOUTH: closed &= ~CTR_EDGE_SOUTH; break;
            }
    return closed;
}

/* Where the fallback may differ from vanilla: outside the map, backup cell undefined,
 * extending map type, and the rule resolves it. Returns 1 and the source if so. */
static int fallback_source(int bx, int by, int *sx, int *sy)
{
    const struct MapLayout *l = gMapHeader.mapLayout;
    int mx = bx - MAP_OFFSET, my = by - MAP_OFFSET;
    if (mx >= 0 && mx < l->width && my >= 0 && my < l->height) return 0;
    if (bx >= 0 && bx < VMap.Xsize && by >= 0 && by < VMap.Ysize && VMap.map[bx + VMap.Xsize * by] != MAPGRID_UNDEFINED) return 0;
    if (!CtrEdge_Resolve(vmap_metatile, NULL, l->width, l->height, closed_sides(), mx, my, sx, sy))
        return 0;
    return CtrEdge_Accept(vmap_metatile(NULL, *sx, *sy), MapGridGetMetatileBehaviorAt(*sx + MAP_OFFSET, *sy + MAP_OFFSET),
                          MapGridGetMetatileBehaviorAt(bx, by));
}

static int map_extends(int idx) { return CtrEdge_MapTypeExtends(sRealType[idx]); }

/* ---- tests ----------------------------------------------------------------------- */
static long gDiff, gChecked, gExtended;
static unsigned char sChanged[MAX_MAPS];   /* maps in which the fallback drew something else than vanilla */

/* Snapshot every cell the camera draws (VIEW_COLS x 16 metatiles), in backup
 * coordinates, together with what the documented rule says each cell may be. */
#define VIEW_COLS ((FIELD_CAMERA_VIEW_RIGHT - FIELD_CAMERA_VIEW_LEFT) / 2)
#define VIEW_X0 (FIELD_CAMERA_VIEW_LEFT / 2)
struct View
{
    struct Cell c[16][VIEW_COLS];
    struct Cell want[16][VIEW_COLS];
    unsigned char ext[16][VIEW_COLS];
    unsigned char valid[16][VIEW_COLS];   /* a real map tile, a backup-map tile or a connected map's tile */
    int ox, oy, map;
};

static void snap(struct View *v)
{
    int i, j, idx = (int)(gMapHeader.mapLayout - sLayouts);
    v->ox = gSaveBlock1Ptr->pos.x; v->oy = gSaveBlock1Ptr->pos.y; v->map = idx;
    for (i = 0; i < 16; i++)
        for (j = 0; j < VIEW_COLS; j++)
        {
            int bx = v->ox + VIEW_X0 + j, by = v->oy + i, sx, sy;
            read_cell((VIEW_X0 + j) * 2, i * 2, &v->c[i][j]);
            v->ext[i][j] = map_extends(idx) && fallback_source(bx, by, &sx, &sy);
            {
                const struct MapLayout *lay = gMapHeader.mapLayout;
                u16 id; u8 layer;
                int mx = bx - MAP_OFFSET, my = by - MAP_OFFSET;
                v->valid[i][j] = (mx >= 0 && mx < lay->width && my >= 0 && my < lay->height)
                    || (bx >= 0 && bx < VMap.Xsize && by >= 0 && by < VMap.Ysize && VMap.map[bx + VMap.Xsize * by] != MAPGRID_UNDEFINED)
                    || GetViewportConnectionMetatile(&lay, bx, by, &id, &layer);
            }
            if (v->ext[i][j])
            {
                u16 id = VMap.map[(sx + MAP_OFFSET) + VMap.Xsize * (sy + MAP_OFFSET)] & MAPGRID_METATILE_ID_MASK;
                expected_cell(gMapHeader.mapLayout, id, MapGridGetMetatileLayerTypeAt(sx + MAP_OFFSET, sy + MAP_OFFSET), &v->want[i][j]);
            }
        }
}

/* The fallback view may differ from the vanilla view of the same state only in cells
 * the rule extends, and there it must show exactly the in-map metatile's tiles. */
static void compare_views(const struct View *a, const struct View *v, const char *what, int strict)
{
    int i, j;
    CHECK(a->ox == v->ox && a->oy == v->oy && a->map == v->map, "%s: runs diverged", what);
    for (i = 0; i < 16; i++)
        for (j = 0; j < VIEW_COLS; j++)
        {
            gChecked++;
            gExtended += a->ext[i][j];
            if (same(&a->c[i][j], &v->c[i][j]))
                continue;
            gDiff++;
            if (!strict)
            {
                /* After a map transition the game does not redraw the cells it keeps; those
                 * outside any map data were drawn before the transition (stale in vanilla
                 * as well). Real tiles must still match exactly. */
                CHECK(!a->valid[i][j], "%s %s: (%d,%d) is a real tile but differs from vanilla", what, sNames[a->map],
                      a->ox + VIEW_X0 + j - MAP_OFFSET, a->oy + i - MAP_OFFSET);
                continue;
            }
            sChanged[a->map] = 1;
            CHECK(a->ext[i][j] && same(&a->c[i][j], &a->want[i][j]),
                  "%s %s: (%d,%d) differs from vanilla%s", what, sNames[a->map],
                  a->ox + VIEW_X0 + j - MAP_OFFSET, a->oy + i - MAP_OFFSET,
                  a->ext[i][j] ? " and is not the extended metatile" : " outside the rule");
        }
}

/* 1. Initial full-map drawing at camera positions covering every outdoor map. */
static void test_full_draw(void)
{
    int m, px, py;
    static struct View a, v;
    for (m = 0; m < sMapCount; m++)
    {
        const struct MapLayout *l = sLayouts + m;
        if (!map_extends(m)) continue;
        for (py = 0; py < l->height; py += (l->height > 40 ? 3 : 1))
            for (px = 0; px < l->width; px += (l->width > 40 ? 3 : 1))
            {
                set_mode(0); enter_map(m, px, py); snap(&a);
                set_mode(1); enter_map(m, px, py); snap(&v);
                compare_views(&a, &v, "full", 1);
            }
    }
}

/* ---- incremental scrolling ------------------------------------------------------- */
/* One metatile of camera movement through the production CameraUpdate (4 frames at 4
 * pixels, as a walking player), then a flush frame for the pending vertical slice. */
static void step_camera(int dx, int dy)
{
    int f;
    gFieldCamera.movementSpeedX = dx * 4;
    gFieldCamera.movementSpeedY = dy * 4;
    for (f = 0; f < 4; f++)
        CameraUpdate();
    gFieldCamera.movementSpeedX = gFieldCamera.movementSpeedY = 0;
    CameraUpdate();
}

/* Walk `steps` metatiles in (dx,dy) from (px,py) and snapshot after every step. */
static int walk(int m, int px, int py, int dx, int dy, int steps, int vanilla, struct View *out)
{
    int i;
    set_mode(vanilla);
    enter_map(m, px, py);
    for (i = 0; i < steps; i++)
    {
        const struct MapLayout *cur;
        step_camera(dx, dy);
        /* the player never takes the camera beyond the map: stop where it would */
        cur = gMapHeader.mapLayout;
        if (gSaveBlock1Ptr->pos.x < 0 || gSaveBlock1Ptr->pos.x >= cur->width
            || gSaveBlock1Ptr->pos.y < 0 || gSaveBlock1Ptr->pos.y >= cur->height)
            return i;
        snap(&out[i]);
    }
    return steps;
}

/* After a scroll the ring buffer must hold what a fresh full draw of the same camera
 * position would hold. Counts, per step, the cells where it does not. */
static void scroll_pass(int m, int px, int py, int dx, int dy, int steps, int vanilla, int *bad)
{
    static struct View inc, fresh;
    int i, ii, jj;
    set_mode(vanilla);
    enter_map(m, px, py);
    for (i = 0; i < steps; i++)
    {
        static u16 keepMap[3][0x800];
        struct FieldCameraOffset keepCam;
        step_camera(dx, dy);
        snap(&inc);
        keepCam = sFieldCameraOffset;
        memcpy(keepMap, sTilemap, sizeof keepMap);
        DrawWholeMapView();
        snap(&fresh);
        memcpy(sTilemap, keepMap, sizeof keepMap);   /* carry on from the scrolled state */
        sFieldCameraOffset = keepCam;
        bad[i] = 0;
        for (ii = 0; ii < 16; ii++)
            for (jj = 0; jj < VIEW_COLS; jj++)
                bad[i] += !same(&inc.c[ii][jj], &fresh.c[ii][jj]);
    }
}

static void test_scroll_consistency(void)
{
    static const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    int m, d, k, i, runs = 0;
    for (m = 0; m < sMapCount; m++)
    {
        const struct MapLayout *l = sLayouts + m;
        if (!map_extends(m) || !sChanged[m]) continue;
        for (d = 0; d < 4; d++)
            for (k = 0; k < 4; k++)
            {
                int steps = 12, bad[16], vbad[16];
                int sx = dirs[d][0] > 0 ? l->width - 1 - steps : (dirs[d][0] < 0 ? steps : ((k + 1) * l->width) / 5);
                int sy = dirs[d][1] > 0 ? l->height - 1 - steps : (dirs[d][1] < 0 ? steps : ((k + 1) * l->height) / 5);
                if (dirs[d][0] && (k & 1)) sy = ((k + 1) * l->height) / 5;
                if (sx < 0 || sy < 0 || sx >= l->width || sy >= l->height) continue;
                if (dirs[d][0] == 0 && dirs[d][1] == 0) continue;
                scroll_pass(m, sx, sy, dirs[d][0], dirs[d][1], steps, 0, bad);
                scroll_pass(m, sx, sy, dirs[d][0], dirs[d][1], steps, 1, vbad);
                runs++;
                for (i = 0; i < steps; i++)
                    CHECK(bad[i] <= vbad[i], "%s dir %d,%d from %d,%d step %d: %d cells of the scrolled view differ from a fresh draw (vanilla: %d)",
                          sNames[m], dirs[d][0], dirs[d][1], sx, sy, i, bad[i], vbad[i]);
            }
    }
    printf("scroll consistency: %d runs\n", runs);
    CHECK(runs > 0, "no scroll run");
}

/* Differential walks, including map transitions through real connections: the fallback
 * walk and the vanilla walk are the same walk; they may differ only where the rule says. */
static void test_walks(void)
{
    static const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    static struct View a[80], v[80];
    int m, d, i, transitions = 0;
    for (m = 0; m < sMapCount; m++)
    {
        const struct MapLayout *l = sLayouts + m;
        if (!map_extends(m)) continue;
        for (d = 0; d < 4; d++)
        {
            int sx = dirs[d][0] > 0 ? 1 : (dirs[d][0] < 0 ? l->width - 2 : l->width / 2);
            int sy = dirs[d][1] > 0 ? 1 : (dirs[d][1] < 0 ? l->height - 2 : l->height / 2);
            int steps = (dirs[d][0] ? l->width : l->height) + 20;
            int na, nv;
            if (steps > 79) steps = 79;
            na = walk(m, sx, sy, dirs[d][0], dirs[d][1], steps, 0, a);
            nv = walk(m, sx, sy, dirs[d][0], dirs[d][1], steps, 1, v);
            CHECK(na == nv, "%s: the two walks stopped at different steps (%d/%d)", sNames[m], na, nv);
            steps = na < nv ? na : nv;
            int moved = 0;
            for (i = 0; i < steps; i++)
            {
                if (a[i].map != m) moved = 1;
                if (i && a[i].map != a[i - 1].map) transitions++;
                compare_views(&a[i], &v[i], "walk", !moved);
            }
        }
    }
    printf("walks crossed %d map transitions\n", transitions);
    CHECK(transitions > 0, "no walk crossed a connection");
}

/* ---- the reported screenshot: Two Island, Cape Brink, west shore ----------------- */
/* The map's west edge column is a shore tile that differs from the metatile beside it.
 * Repeating the edge outward (a blind stretch) draws that shore tile again and again,
 * which is the field of vertical stripes in the screenshot. The fallback must leave
 * those rows to the border, at every camera position that shows them. */
static void test_cape_brink(void)
{
    int m = find_map("TwoIsland_CapeBrink"), y, x, px, py, shoreRows = 0, shown = 0;
    const struct MapLayout *l = sLayouts + m;
    static struct View a, v;
    unsigned shore;

    set_mode(0); enter_map(m, 0, 0);
    shore = vmap_metatile(NULL, 0, 20) & 0x3ff;
    for (y = 0; y < l->height; y++)
        if ((vmap_metatile(NULL, 0, y) & 0x3ff) == shore && (vmap_metatile(NULL, 1, y) & 0x3ff) != shore)
        {
            shoreRows++;
            int sx, sy;
            CHECK((MapGridGetMetatileIdAt(-1 + MAP_OFFSET, y + MAP_OFFSET) & 0x3ff) != shore, "the border is the shore tile at row %d", y);
            /* the shape rule alone already refuses a shore edge, before any land/sea decision */
            CHECK(!CtrEdge_Resolve(vmap_metatile, NULL, l->width, l->height, closed_sides(), -1, y, &sx, &sy),
                  "the shore tile at row %d would be repeated outward", y);
        }
    CHECK(shoreRows >= 12, "Cape Brink's west edge no longer has the shore column (%d rows)", shoreRows);

    for (py = 0; py < l->height; py++)
        for (px = 0; px < 4; px++)
        {
            set_mode(0); enter_map(m, px, py); snap(&a);
            set_mode(1); enter_map(m, px, py); snap(&v);
            for (y = 0; y < 16; y++)
                for (x = 0; x < VIEW_COLS; x++)
                {
                    int mx = a.ox + VIEW_X0 + x - MAP_OFFSET, my = a.oy + y - MAP_OFFSET;
                    if (mx >= 0 || my < 0 || my >= l->height) continue;
                    if ((vmap_metatile(NULL, 0, my) & 0x3ff) == shore && (vmap_metatile(NULL, 1, my) & 0x3ff) != shore)
                    {
                        shown++;
                        CHECK(!a.ext[y][x] && same(&a.c[y][x], &v.c[y][x]), "Cape Brink (%d,%d): west of a shore row is not the border", mx, my);
                    }
                }
        }
    CHECK(shown > 500, "too few screenshot cells examined (%d)", shown);
    printf("Cape Brink: %d shore rows, %d drawn cells west of them, all the border (a blind stretch draws the shore tile)\n", shoreRows, shown);
}

/* Which maps change is a decision of the rule: pin it. */
static void test_changed_maps(void)
{
    static const char *const expected[] = {
        "FourIsland", "OneIsland", "OneIsland_KindleRoad", "OneIsland_TreasureBeach", "Route25", "SixIsland_WaterPath",
        "ThreeIsland_BondBridge", "TwoIsland", "TwoIsland_CapeBrink",
    };
    int m, e;
    for (m = 0; m < sMapCount; m++)
    {
        int want = 0;
        for (e = 0; e < (int)(sizeof expected / sizeof expected[0]); e++)
            want |= !strcmp(sNames[m], expected[e]);
        CHECK(!!sChanged[m] == want, "%s: the fallback %s the picture, expected %s", sNames[m], sChanged[m] ? "changes" : "leaves",
              want ? "a change" : "no change");
    }
}

/* Debug aid: print what DrawMetatileAt would pick for a ring of `pad` metatiles around a map. */
static void dump_map(const char *name, int pad, int vanilla)
{
    int m = find_map(name), x, y;
    const struct MapLayout *l = sLayouts + m;
    set_mode(vanilla);
    enter_map(m, 0, 0);
    for (y = -pad; y < l->height + pad; y++)
    {
        for (x = -pad; x < l->width + pad; x++)
        {
            const struct MapLayout *lay = l;
            u16 id = MapGridGetMetatileIdAt(x + MAP_OFFSET, y + MAP_OFFSET);
            u8 layer = 0;
            if (!GetViewportConnectionMetatile(&lay, x + MAP_OFFSET, y + MAP_OFFSET, &id, &layer))
                GetEdgeExtensionMetatile(lay, x + MAP_OFFSET, y + MAP_OFFSET, &id, &layer);
            printf("%d ", id);
        }
        puts("");
    }
}

int main(int argc, char **argv)
{
    int i;
    if (argc >= 6 && !strcmp(argv[3], "--dump"))
    {
        sTree = argv[1];
        load_world(argv[2]);
        for (i = 0; i < sMapCount; i++) sRealType[i] = sHeaders[i].mapType;
        dump_map(argv[4], 12, atoi(argv[5]));
        return 0;
    }
    if (argc < 3) { printf("usage: %s TREE WORLD\n", argv[0]); return 2; }
    sTree = argv[1];
    load_world(argv[2]);
    for (i = 0; i < sMapCount; i++) sRealType[i] = sHeaders[i].mapType;
    test_full_draw();
    test_changed_maps();
    test_cape_brink();
    test_walks();
    test_scroll_consistency();
    printf("checked %ld cells: %ld extended by the rule, %ld differ from vanilla\n", gChecked, gExtended, gDiff);
    if (failures) { printf("%d failures\n", failures); return 1; }
    puts("PASS map edge draw");
    return 0;
}
