/* Original neutral geometric icons and pack-backed glyph storage.
 * No ROM-derived pixels are initialized in this header. */
typedef struct { uint8_t width; uint32_t rows[16]; } PtGlyph;
typedef struct { uint8_t w, h; const uint8_t *pixels; uint16_t pal[16]; } PtImage;
static PtGlyph sPtFont[256];
static bool sPtFontReady[256];
static uint8_t sTwinIconPixels[7][12 * 12];
static PtImage sPt_icon_pokedex, sPt_icon_pokemon, sPt_icon_bag, sPt_icon_card;
static PtImage sPt_icon_save, sPt_icon_options, sPt_icon_shoes;
/* Port-authored 12x12 silhouettes (not restored game assets).
 * One bit per pixel, left-to-right, from high to low in each 12-bit row.
 * The seven menu icons are distinctive rather than the former shared box. */
static const uint16_t sTwinIconRows[7][12] = {
    /* Pokedex */
    {0x000, 0x3fc, 0x284, 0x2b4, 0x2b4, 0x284, 0x2b4, 0x284, 0x284, 0x3fc, 0x000, 0x000},
    /* Pokemon */
    {0x060, 0x1f8, 0x30c, 0x606, 0x462, 0xf0f, 0xf0f, 0x462, 0x606, 0x30c, 0x1f8, 0x060},
    /* Bag */
    {0x0f0, 0x108, 0x108, 0x3fc, 0x204, 0x264, 0x264, 0x204, 0x204, 0x204, 0x3fc, 0x000},
    /* Trainer Card */
    {0x000, 0x7fe, 0x402, 0x5de, 0x552, 0x5de, 0x402, 0x5fe, 0x402, 0x7fe, 0x000, 0x000},
    /* Save */
    {0x000, 0x3fc, 0x204, 0x2f4, 0x2f4, 0x204, 0x2f4, 0x294, 0x294, 0x2f4, 0x3fc, 0x000},
    /* Options */
    {0x060, 0x1f8, 0x36c, 0x606, 0x666, 0xe97, 0xe97, 0x666, 0x606, 0x36c, 0x1f8, 0x060},
    /* Running Shoes */
    {0x000, 0x000, 0x0e0, 0x1e0, 0x1c0, 0x3c0, 0x3f8, 0x61e, 0x7fe, 0xffe, 0x000, 0x000},
};
/* Original red running shoe, hand-authored here, not ROM or reference-image
 * pixels. Same 12x12 canvas/placement as the existing shoe. 0 transparent,
 * 1 dark outline, 2 red upper, 3 lit red, 4 white sole/laces, 5 blue trim,
 * 6 yellow motion marks. */
static const char sTwinShoe[12][13] = {
    "000000110000", "000001451000", "000001521000",
    "000012321000", "000123421000", "001234221000",
    "012344221660", "123422221000", "154444441660",
    "014444410000", "001111100660", "000000000000",
};
static int CtrPokeTouch_PackMenuIcon(unsigned icon, uint8_t pixels[12 * 12], uint16_t pal[16]);

static void TwinIconsPrepare(void)
{
    static bool ready;
    PtImage *icons[] = {&sPt_icon_pokedex, &sPt_icon_pokemon, &sPt_icon_bag,
        &sPt_icon_card, &sPt_icon_save, &sPt_icon_options, &sPt_icon_shoes};
    if (ready) return;
    ready = true;
    for (unsigned n = 0; n < 7; ++n) {
        icons[n]->w = icons[n]->h = 12;
        icons[n]->pixels = sTwinIconPixels[n];
        icons[n]->pal[1] = 0xffff;
        for (unsigned y = 0; y < 12; ++y)
            for (unsigned x = 0; x < 12; ++x)
                sTwinIconPixels[n][y * 12 + x] = (sTwinIconRows[n][y] >> (11 - x)) & 1u;
        /* FireRed originals come from the user's verified .pak, not this source tree.
         * Keep the existing neutral art if any external asset is unavailable. */
        if (n < 6)
            CtrPokeTouch_PackMenuIcon(n, sTwinIconPixels[n], icons[n]->pal);
        else {
            static const uint16_t shoePal[] = {
                0, 0x18c3, 0xd104, 0xfa48, 0xffff, 0x2b18, 0xfe80
            }; /* Original RGB565 colours, not a game palette. */
            memcpy(icons[n]->pal, shoePal, sizeof(shoePal));
            for (unsigned y = 0; y < 12; ++y)
                for (unsigned x = 0; x < 12; ++x)
                    sTwinIconPixels[n][y * 12 + x] = (uint8_t)(sTwinShoe[y][x] - '0');
        }
    }
}
extern const uint8_t *CtrTwinFontData(void);
extern unsigned CtrTwinFontWidth(unsigned code);
