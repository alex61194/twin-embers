/* The intro's title on the 400x240 canvas, 1:1: its bands (DrawTitleBand) and its flames on the canvas's sides
 * (DrawTitleFlameCopies). Included by 3ds_video.c, whose statics it uses. */

/*
 * The title composed for the taller screen, every tile of it 1:1. FireRed's title is a 240x160 picture whose bands run
 * straight across: BG2 holds the orange line and the black band above the teal (rows 0-29) and the black band with PRESS
 * START and the red footer under it (rows 112-159); BG3 holds the teal between them. Floating in the middle of a 400x240
 * canvas those bands left 40 rows of black above and below the picture. Here the top block goes to the canvas's top, the
 * teal's own tile row fills down from it, and the black band with PRESS START stays on its own rows: the teal ends at
 * Charizard's middle and PRESS START is level with his tail, as on the GBA, and the flames rise from behind the band.
 * Only the red footer (from tile row 18) goes to the canvas's bottom, the band's last black tile row filling the rows
 * between. Each band continues to the canvas's sides with its own edge tile column; the logo, Charizard, the flames and
 * the slash stay exactly where FireRed puts them (their effects belong to those places).
 */
#define TITLE_TOP_ROWS 30
#define TITLE_BOTTOM_ROW 112
#define TITLE_FOOTER_ROW 144
#define TITLE_BLACK_ROW 136
#define TITLE_TEAL_ROW 32
static bool TitleComposed(unsigned bg)
{
    return sIntro && !NativeIntroActive() && sNativeIntroScene == CTR_INTRO_TITLE && (bg == 2 || bg == 3);
}

/* Source rect of the layer's map (x, y of its picture, scroll added) onto the stage at (x0, y0)..(x1, y1). */
static void IntroBlit(unsigned bg, int x0, int x1, int y0, int y1, int sx, int sy)
{
    int cx0 = x0 > sClipX0 ? x0 : sClipX0, cx1 = x1 < sClipX1 ? x1 : sClipX1;
    int cy0 = y0 > sClipY0 ? y0 : sClipY0, cy1 = y1 < sClipY1 ? y1 : sClipY1;

    if (cx0 >= cx1 || cy0 >= cy1) return;
    if (LayerDrawable(bg))
    {
        DrawLayerRect(bg, x0, x1, y0, y1, sx, sy, 1, 1, false, 0, 160);
        return;
    }
    {
        int fx = sx + (cx0 - x0), fy = sy + (cy0 - y0), viewX = sViewX, viewY = sViewY;

        C2D_Flush();
        Scissor(cx0, cy0, cx1, cy1);
        sViewX = viewX + cx0 - fx;
        sViewY = viewY + cy0 - fy;
        DrawTextSpan(bg, fx, fx + (cx1 - cx0), fy, fy + (cy1 - cy0));
        C2D_Flush();
        sViewX = viewX;
        sViewY = viewY;
        RestoreScissor();
    }
}

/* Map rows [top, bottom) of a title layer onto the stage rows from y0, across the canvas: the map's own columns, then
 * its last tile column onward and its first one backward. */
static void TitleRows(unsigned bg, int top, int bottom, int y0)
{
    int y1 = y0 + (bottom - top);

    IntroBlit(bg, 0, 256, y0, y1, 0, top);                      /* the map's own columns, 240..255 included */
    for (int x = 256; x < VIEW_RIGHT; x += 8)
        IntroBlit(bg, x, x + 8, y0, y1, 248, top);              /* the last tile column, onward */
    for (int x = 0; x > VIEW_LEFT; x -= 8)
        IntroBlit(bg, x - 8, x, y0, y1, 0, top);                /* the first one, backward */
}

static bool DrawTitleBand(unsigned bg)
{
    int lift = -CTR_VIEW_Y, drop = CTR_GAME_HEIGHT - CTR_VIEW_Y - 160; /* the canvas's top and bottom, from the stage's */

    if (!TitleComposed(bg)) return false;
    ViewBase();
    TitleRows(bg, 0, TITLE_TOP_ROWS, lift);                                     /* orange line, black band: the top */
    TitleRows(bg, TITLE_BOTTOM_ROW, TITLE_FOOTER_ROW, TITLE_BOTTOM_ROW);        /* PRESS START's band: in its place */
    for (int y = TITLE_FOOTER_ROW; y < TITLE_FOOTER_ROW + drop; y += 8)         /* its last black row, to the footer */
        TitleRows(bg, TITLE_BLACK_ROW, TITLE_BLACK_ROW + (y + 8 <= TITLE_FOOTER_ROW + drop ? 8 : TITLE_FOOTER_ROW + drop - y), y);
    TitleRows(bg, TITLE_FOOTER_ROW, 160, TITLE_FOOTER_ROW + drop);               /* the red footer: the bottom */
    if (bg == 3)
        for (int y = TITLE_TOP_ROWS + lift; y < TITLE_BOTTOM_ROW; y += 8)
        {
            int y1 = y + 8 < TITLE_BOTTOM_ROW ? y + 8 : TITLE_BOTTOM_ROW;

            IntroBlit(bg, 0, 256, y, y1, 0, TITLE_TEAL_ROW);     /* the teal tile row, as it is, down to the band */
            IntroBlit(bg, VIEW_LEFT, 0, y, y1, 0, TITLE_TEAL_ROW);
            IntroBlit(bg, 256, VIEW_RIGHT, y, y1, 0, TITLE_TEAL_ROW);
        }
    return true;
}

/*
 * The title's flames on the canvas's sides. FireRed spawns them across its 240 columns only (Task_FlameSpawner), from
 * behind the black band, and they rise into the teal; the canvas is 80 columns wider on each side. A flame is a plain
 * 16x16 sprite of priority 3 (the blank ones are too, and draw nothing). Each one in columns 0-79 is drawn again 240 to
 * the right, and each one in columns 80-144 again 160 to the left, so both sides burn with FireRed's own flames, in the
 * same frame of the same animation and behind the same band, and FireRed has no more sprites to keep.
 */
static void DrawTitleFlameCopies(unsigned attr0, unsigned attr1, unsigned attr2, int x, int y, bool effects)
{
    int shifts[2] = {x < 80 ? 240 : 0, x >= 80 && x <= 144 ? -160 : 0};

    if (!sIntro || NativeIntroActive() || sNativeIntroScene != CTR_INTRO_TITLE || (attr0 & 0x300) || (attr0 >> 14) != 0
        || (attr1 >> 14) != 1 || ((attr0 >> 10) & 3) != 0 || x < 0)
        return;
    for (int c = 0; c < 2; ++c)
    {
        int cx = x + shifts[c];

        if (!shifts[c] || cx >= sClipX1 || cx + 16 <= sClipX0 || y >= sClipY1 || y + 16 <= sClipY0) continue;
        Blend(4, effects, false);
        ViewBase();
        for (unsigned ty = 0; ty < 2; ++ty)
            for (unsigned tx = 0; tx < 2; ++tx)
            {
                bool flipX = (attr1 & 0x1000) != 0, flipY = (attr1 & 0x2000) != 0;
                unsigned tile = CtrVideo_ObjTile(attr2 & 1023, flipX ? 1 - tx : tx, flipY ? 1 - ty : ty, 16, false, Reg(0) & 0x40);

                DrawTile(0x10000 + tile * 32, 16 + (attr2 >> 12), false, cx + CTR_VIEW_X + sLayerShift + (int)tx * 8,
                         y + CTR_VIEW_Y + (int)ty * 8, flipX, flipY);
            }
    }
}
