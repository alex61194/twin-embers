/* The field's Poke Ball trail battle transition, part of 3ds_video.c (included there, in this place; kept apart
 * for the 256 KiB source limit). Uses the compositor's own state; not a header of its own. */

/*
 * FireRed's Poke Ball trail battle transition (battle_transition.c): balls roll
 * across the screen in bands of 32 lines, each blacking out the tile column it
 * passes. Its trail was BG0 tiles, and BG0's map spans 256 pixels; the field
 * under it is this 400x240 canvas, so the trail is kept here, in the canvas's
 * own 8x8 cells, and drawn in BG0's place.
 */
#define TRAIL_COLUMNS (CTR_GAME_WIDTH / 8)
#define TRAIL_ROWS (CTR_GAME_HEIGHT / 8)
static uint64_t sTrail[TRAIL_ROWS];
static bool sTrailShown;

void CtrPokeballTrail_Reset(void)
{
    memset(sTrail, 0, sizeof(sTrail));
    sTrailShown = false;
}

/* A ball per band of 32 lines, down the whole height. */
unsigned CtrPokeballTrail_Count(void) { return (CTR_GAME_HEIGHT + 31) / 32; }

int CtrPokeballTrail_Right(void) { return CTR_GAME_WIDTH; }

/* Ball i's field effect arguments (x, y, side, delay): FireRed's five, then more in the same stagger. */
void CtrPokeballTrail_Ball(unsigned i, unsigned side, uint32_t *args)
{
    static const int8_t delays[] = {0, 16, 32, 8, 24, 4, 20, 36};

    args[0] = (uint32_t)(side ? CTR_GAME_WIDTH + 16 : -16);
    args[1] = i * 32 + 16;
    args[2] = side;
    args[3] = (uint32_t)delays[i % (sizeof(delays) / sizeof(delays[0]))];
}

/* The ball at (x, y), its centre, blacks out its tile column across its band. */
void CtrPokeballTrail_Mark(int x, int y)
{
    int column = x >> 3, row = y >> 3;

    if (x < 0 || column >= TRAIL_COLUMNS) return;
    for (int r = row - 2; r <= row + 1; ++r)
        if (r >= 0 && r < TRAIL_ROWS) sTrail[r] |= 1ull << column;
    sTrailShown = true;
}

static void DrawPokeballTrail(int x0, int x1, int y0, int y1)
{
    ViewBase();
    for (int r = 0; r < TRAIL_ROWS; ++r)
    {
        uint64_t bits = sTrail[r];
        int top = r * 8 > y0 ? r * 8 : y0, bottom = r * 8 + 8 < y1 ? r * 8 + 8 : y1;

        if (!bits || top >= bottom) continue;
        for (int c = 0; c < TRAIL_COLUMNS;)
        {
            if (!(bits >> c & 1)) { ++c; continue; }
            int end = c;
            while (end < TRAIL_COLUMNS && (bits >> end & 1)) ++end;
            int left = c * 8 > x0 ? c * 8 : x0, right = end * 8 < x1 ? end * 8 : x1;
            if (left < right)
                C2D_DrawRectSolid(left + CTR_VIEW_X, top + CTR_VIEW_Y, 0, right - left, bottom - top,
                                  C2D_Color32(0, 0, 0, 255));
            c = end;
        }
    }
}
