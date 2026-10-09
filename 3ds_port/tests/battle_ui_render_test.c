/* The battle screen (3ds_battle_ui_impl.h) through the production renderer with synthetic game bridges.
 *
 * Every state of the controller (actions, moves, targets, Safari, Bag with its menu and message, Party with its modals)
 * is drawn; each press, cursor move and state change must leave the canvas exactly as a full redraw would (the dirty
 * rectangles are complete); the screen is the PokeTouch red (no navy of the shared theme remains in it) and the buttons'
 * hit areas answer to the same points as before. No ROM-derived data: the font is blank unless BATTLE_UI_FONT names
 * a 256x64-byte glyph file (a local viewing aid), and BATTLE_UI_OUT names a directory for one PPM per state.
 *
 * usage: battle_ui_render_test [print-hits] */
#include <assert.h>
#ifndef PT_SOURCE
#define PT_SOURCE "3ds_poketouch.c"
#endif
#include PT_SOURCE
#include <math.h>

static uint8_t font[256 * 64];
const uint8_t *CtrTwinFontData(void) { return font; }
unsigned CtrTwinFontWidth(unsigned code) { (void)code; return 6; }
#ifndef BATTLE_UI_EXTERNAL_LOAD
/* The pack's two item icons, synthesised: a literal-only GBA LZ stream of 24x24 4bpp tiles (3x3) and of a palette, so
 * the full-size icon path runs without game data. With BATTLE_UI_NO_PACK nothing is found and the fallbacks draw. */
int gNoPack;
void *CtrData_Load(const char *path, uint32_t *len)
{
    uint8_t raw[288];
    int palette = strstr(path, "icon_palettes/") != NULL;
    unsigned size = palette ? 32u : 288u;
    uint8_t *out;
    unsigned pos = 4;

    if (gNoPack || !strstr(path, "graphics/items/icon"))
        return NULL;
    memset(raw, 0, sizeof(raw));
    if (palette) {
        raw[2] = 0xff; raw[3] = 0x03;               /* index 1: green */
    } else {
        for (unsigned i = 0; i < 288; ++i)
            raw[i] = (uint8_t)((i % 32) < 16 ? 0x11 : 0x00);   /* stripes, so tile order shows */
        raw[0] = 0x21;                              /* the very first pixel: index 1, the second 2 */
    }
    *len = 4 + size + (size + 7) / 8;
    out = malloc(*len);
    assert(out);
    out[0] = 0x10; out[1] = (uint8_t)size; out[2] = (uint8_t)(size >> 8); out[3] = 0;
    for (unsigned i = 0; i < size; ++i) {
        if (!(i % 8)) out[pos++] = 0;
        out[pos++] = raw[i];
    }
    return out;
}
#endif
int CtrPokeTouch_ItemIcon(unsigned item, unsigned char *tiles, unsigned short *palette)
{
    memset(tiles, 0x11, 288); memset(palette, 0, 32);
    palette[1] = item == 1 ? 31 : item == 2 ? 31 << 5 : 31 << 10;
    return item != 0;
}
int CtrPokeTouch_MapArt(int group, uint16_t *map, uint8_t *tiles, uint16_t *pal, uint16_t *bgMap, uint8_t *bgTiles)
{ (void)group; (void)map; (void)tiles; (void)pal; (void)bgMap; (void)bgTiles; return 0; }
int CtrPokeTouch_MapPlayerIcon(uint8_t *tiles, uint16_t *pal) { (void)tiles; (void)pal; return 0; }
int CtrPokeTouch_MapCursorIcon(uint8_t *tiles, uint16_t *pal) { (void)tiles; (void)pal; return 0; }
int CtrBottomParty_Icon(int slot, unsigned char *tiles, unsigned short *pal)
{
    /* A flat 32x32 icon in the slot's own colour, so the plate and the faint tone are visible. */
    memset(tiles, 0x11, 512); memset(pal, 0, 32);
    pal[1] = (unsigned short)((slot * 5 + 6) | (31 - slot * 4) << 5 | (slot * 3) << 10);
    return 1;
}

static uint16_t canvas[W * H], reference[W * H];
static uint16_t at(int x, int y) { return canvas[x * H + H - 1 - y]; }

static void settle(const CtrBattleView *old, const CtrBattleView *v)
{
    CtrDirtyList dirty;

    CtrDirty_Clear(&dirty);
    CtrBattleUi_Update(canvas, old, v, &dirty);
    memcpy(reference, canvas, sizeof(canvas));
    CtrBattleUi_Draw(canvas, v);
    if (memcmp(reference, canvas, sizeof(canvas))) {
        for (int x = 0; x < W; ++x)
            for (int y = 0; y < H; ++y)
                if (reference[x * H + H - 1 - y] != canvas[x * H + H - 1 - y]) {
                    fprintf(stderr, "battle redraw mismatch at %d,%d state=%u pressed=%d cursor=%d\n", x, y, v->state,
                            v->pressed, v->cursor);
                    assert(0);
                }
    }
}

/* The silhouette of a drawn state: which pixels are not the backdrop's. Restyling must not move an edge. */
static unsigned sMaskCrc;
static void mask(void)
{
    uint16_t bg = at(1, 100);

    for (int x = 0; x < W; ++x)
        for (int y = 0; y < H; ++y)
            sMaskCrc = sMaskCrc * 33u + (at(x, y) != bg);
}

#ifndef BATTLE_UI_BEFORE
/* WCAG contrast of two RGB565 colours. */
static double lin(unsigned v) { double c = v / 255.0; return c <= 0.03928 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4); }
static double lum(uint16_t c)
{
    return 0.2126 * lin((c >> 11) << 3 | (c >> 13)) + 0.7152 * lin(((c >> 5) & 63) << 2 | ((c >> 5) & 63) >> 4)
         + 0.0722 * lin((c & 31) << 3 | (c & 31) >> 2);
}
static double contrast(uint16_t a, uint16_t b)
{
    double la = lum(a), lb = lum(b);

    return la > lb ? (la + 0.05) / (lb + 0.05) : (lb + 0.05) / (la + 0.05);
}

/* Recommendations for text on a game UI (WCAG 2.x): 4.5:1 for small text, 3:1 for large (2x and 3x labels and
 * UI components). The red surfaces and the button colours the battle screen draws white on are held to them. */
static void legibility(void)
{
    const UiSurface *red = &sUiRed;
    const UiButtonStyle *big[4] = {&red->fight, &red->bag, &red->pokemon, &red->run};
    const UiButtonStyle *small[3] = {&red->primary, &red->neutral, &red->cancel};
    const uint16_t surfaces[3] = {red->bg, red->panelLo, red->panel};

    for (int i = 0; i < 4; ++i) {
        assert(contrast(UI_TEXT, big[i]->body) >= 3.0 && contrast(UI_TEXT, big[i]->lit) >= 3.0);
    }
    for (int i = 0; i < 3; ++i)
        assert(contrast(UI_TEXT, small[i]->body) >= 4.5 && contrast(UI_TEXT, small[i]->lit) >= 4.5);
    for (int i = 0; i < 3; ++i) {
        assert(contrast(UI_TEXT, surfaces[i]) >= 4.5);
        assert(contrast(red->textDim, surfaces[i]) >= 4.5);
    }
    assert(contrast(UI_TEXT, UI_SELECT) >= 4.5 && contrast(UI_TEXT, UI_SELECT_HI) >= 4.5);
    /* The type colours behind a move's name or a badge: the ink is chosen to read on every one. */
    for (int t = 0; t < 18; ++t) {
        uint16_t c = UiTypeColor(t), ink = UiInkOn(c);

        assert(contrast(ink, c) >= 3.0);
    }
}
#endif

static void put(const char *dir, const char *name)
{
    char path[512];
    FILE *f;

    /* A modal dims the screen behind it, and a halved colour can land on the backdrop by chance: not a silhouette. */
    if (!strstr(name, "menu") && !strstr(name, "message") && !strstr(name, "yesno"))
        mask();
    if (getenv("BATTLE_UI_MASKS")) fprintf(stderr, "%s %08x\n", name, sMaskCrc);
    if (!dir)
        return;
    snprintf(path, sizeof(path), "%s/%s.ppm", dir, name);
    f = fopen(path, "wb");
    assert(f);
    fprintf(f, "P6\n%d %d\n255\n", W, H);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            unsigned c = at(x, y);
            fputc((int)((c >> 11) << 3), f);
            fputc((int)(((c >> 5) & 63) << 2), f);
            fputc((int)((c & 31) << 3), f);
        }
    fclose(f);
}

static unsigned sHitCrc;
static void hits(const CtrBattleView *v)
{
    for (int y = 0; y < H; y += 3)
        for (int x = 0; x < W; x += 3) {
            int h = CtrBattleUi_Hit(v, x, y);

            sHitCrc = sHitCrc * 31u + (unsigned)(h + 7);
        }
}

static void settle_summary(const CtrSummaryUiView *old, const CtrSummaryUiView *v)
{
    CtrDirtyList dirty;

    CtrDirty_Clear(&dirty);
    CtrSummaryUi_Update(canvas, old, v, &dirty);
    memcpy(reference, canvas, sizeof(canvas));
    CtrSummaryUi_Draw(canvas, v);
    if (memcmp(reference, canvas, sizeof(canvas))) {
        for (int x = 0; x < W; ++x)
            for (int y = 0; y < H; ++y)
                if (reference[x * H + H - 1 - y] != canvas[x * H + H - 1 - y]) {
                    fprintf(stderr, "summary redraw mismatch at %d,%d page=%d pressed=%d\n", x, y, v->data.page, v->pressed);
                    assert(0);
                }
    }
}

static void summary_hits(const CtrSummaryUiView *v)
{
    for (int y = 0; y < H; y += 3)
        for (int x = 0; x < W; x += 3)
            sHitCrc = sHitCrc * 31u + (unsigned)(CtrSummaryUi_Hit(v, x, y) + 7);
}

static void names(char *dst, size_t n, const char *s) { snprintf(dst, n, "%s", s); }

int main(int argc, char **argv)
{
    const char *out = getenv("BATTLE_UI_OUT"), *fontPath = getenv("BATTLE_UI_FONT");
    CtrBattleView v, old;
    int k;

    (void)argv;
#ifndef BATTLE_UI_EXTERNAL_LOAD
#ifndef BATTLE_UI_BEFORE
    gNoPack = getenv("BATTLE_UI_NO_PACK") != NULL;
    {
        uint8_t px[24 * 24];
        uint16_t pal[16];

        /* The full-size pack icon: GBA tile order, 3x3 tiles, no sampling; absent pack: nothing, and the fallback draws. */
        if (gNoPack)
            assert(!PokeTouch_PackIcon24("berry_pouch", px, pal));
        else {
            assert(PokeTouch_PackIcon24("berry_pouch", px, pal));
            assert(px[0] == 1 && px[1] == 2);                  /* tile 0, first byte */
            assert(px[8] == 1 && px[16] == 1 && px[8 * 24] == 1);    /* tiles 1, 2 and 3 (a row of 3) */
            assert(px[4 * 24] == 0 && px[4 * 24 + 7] == 0);    /* rows 4-7 of a tile are empty in the fixture */
            assert(pal[0] == 0 && pal[1] != 0);
        }
    }
#else
    gNoPack = getenv("BATTLE_UI_NO_PACK") != NULL;
#endif
#endif
    if (fontPath) {
        FILE *f = fopen(fontPath, "rb");

        assert(f && fread(font, 1, sizeof(font), f) == sizeof(font));
        fclose(f);
    }
#ifndef BATTLE_UI_BEFORE
    legibility();
#endif
    memset(&v, 0, sizeof(v));
    v.pressed = -1;
    v.party[0] = v.party[1] = v.party[2] = v.party[4] = 1; v.party[3] = 2;
    CtrBattleUi_Draw(canvas, &v);   /* BUI_IDLE: the backdrop and the strip */
    put(out, "00_idle");
    old = v;

    /* ACTION */
    v.state = BUI_ACTION; v.showCursor = 1; v.cursor = BUI_ACT_FIGHT;
    settle(&old, &v); old = v; put(out, "01_action"); hits(&v);
    for (k = 0; k < 4; ++k) {
        v.cursor = (int8_t)k; settle(&old, &v); old = v;
        v.pressed = (int16_t)k; settle(&old, &v); old = v;
        if (k == 3) put(out, "02_action_pressed_run");
        v.pressed = -1; settle(&old, &v); old = v;
    }
    v.showCursor = 0; settle(&old, &v); old = v;

    /* MOVE */
    v.state = BUI_MOVE; v.showCursor = 1; v.cursor = 1;
    for (k = 0; k < 4; ++k) {
        CtrBattleMove *m = &v.move[k];
        static const char *const nm[4] = {"FLAMETHROWER", "EARTHQUAKE", "SURF", "THUNDERBOLT"};
        static const char *const tn[4] = {"FIRE", "GROUND", "WATER", "ELECTR"};
        static const int ty[4] = {10, 4, 11, 13};

        m->present = k != 3 || 1; m->type = (uint8_t)ty[k];
        names(m->name, sizeof(m->name), nm[k]); names(m->typeName, sizeof(m->typeName), tn[k]);
        names(m->power, sizeof(m->power), k == 2 ? "-" : "90");
        m->maxPp = 15; m->pp = (uint8_t)(k == 0 ? 15 : k == 1 ? 7 : k == 2 ? 3 : 0);
    }
    v.move[3].present = 0;
    settle(&old, &v); old = v; put(out, "03_moves"); hits(&v);
    v.pressed = 2; settle(&old, &v); old = v; v.pressed = -1; settle(&old, &v); old = v;
    v.pressed = BUI_CANCEL; settle(&old, &v); old = v; put(out, "04_moves_cancel_pressed");
    v.pressed = -1; v.cursor = 0; settle(&old, &v); old = v;

    /* TARGET */
    v.state = BUI_TARGET;
    for (k = 0; k < 4; ++k) {
        CtrBattleTarget *t = &v.target[k];
        static const char *const nm[4] = {"RATICATE", "PIDGEOT", "CHARIZARD", "BLASTOISE"};

        t->present = 1; t->side = (uint8_t)(k >= 2); t->battler = (int8_t)k; t->level = (uint8_t)(30 + 7 * k);
        t->maxHp = 120; t->hp = (uint16_t)(120 - 35 * k);
        names(t->name, sizeof(t->name), nm[k]);
    }
    v.target[1].present = 0;
    settle(&old, &v); old = v; put(out, "05_targets"); hits(&v);
    v.pressed = 0; settle(&old, &v); old = v; v.pressed = -1; settle(&old, &v); old = v;

    /* SAFARI */
    v.state = BUI_SAFARI; v.cursor = 2;
    settle(&old, &v); old = v; put(out, "06_safari"); hits(&v);
    v.pressed = 1; settle(&old, &v); old = v; v.pressed = -1; settle(&old, &v); old = v;

    /* BAG */
    v.state = BUI_BAG;
    {
        CtrBattleBag *b = &v.bag;
        static const char *const nm[5] = {"POTION", "SUPER POTION", "ANTIDOTE", "REVIVE", "FULL HEAL"};

        b->pocket = 0; b->count = 9; b->cursor = 1; b->first = 0;
        for (k = 0; k < 5; ++k) { b->row[k].item = (uint16_t)(k + 1); b->row[k].qty = (uint16_t)(20 - 3 * k); names(b->row[k].name, BUI_ITEM_LEN, nm[k]); }
        b->selItem = 2; names(b->selName, BUI_ITEM_LEN, "SUPER POTION");
        names(b->selDesc, BUI_DESC_LEN, "Restores the HP of one POKeMON by 50 points.");
    }
    settle(&old, &v); old = v; put(out, "07_bag"); hits(&v);
    v.pressed = BUI_BAG_ROW + 2; settle(&old, &v); old = v; v.pressed = -1; settle(&old, &v); old = v;
    v.pressed = BUI_BAG_TAB + 1; settle(&old, &v); old = v; put(out, "08_bag_tab_pressed"); v.pressed = -1; settle(&old, &v); old = v;
    v.bag.menuCount = 2; v.bag.menuCursor = 0; names(v.bag.menuName[0], 12, "USE"); names(v.bag.menuName[1], 12, "CANCEL");
    settle(&old, &v); old = v; put(out, "09_bag_menu"); hits(&v);
    v.bag.menuCount = 0; names(v.bag.message, sizeof(v.bag.message), "The POKeMON's HP was restored by 50 points.");
    settle(&old, &v); old = v; put(out, "10_bag_message"); v.bag.message[0] = 0; settle(&old, &v); old = v;

    /* PARTY */
    v.state = BUI_PARTY;
    {
        CtrBattleParty *p = &v.pty;
        static const char *const nm[6] = {"CHARIZARD", "PIDGEOT", "SNORLAX", "ALAKAZAM", "GENGAR", ""};

        for (k = 0; k < 5; ++k) {
            p->present[k] = 1; p->level[k] = (uint8_t)(50 + k); p->maxHp[k] = 150; p->hp[k] = (uint16_t)(150 - 30 * k);
            p->icon[k] = (uint16_t)(k + 1); names(p->name[k], sizeof(p->name[k]), nm[k]);
        }
        p->status[1] = 2; p->status[4] = 5; p->hp[3] = 0; p->cursor = 2; p->mode = BUI_PTY_SWITCH;
    }
    settle(&old, &v); old = v; put(out, "11_party"); hits(&v);
    v.pressed = BUI_PTY_SLOT + 1; settle(&old, &v); old = v; v.pressed = -1; settle(&old, &v); old = v;
    v.pressed = BUI_PTY_CANCEL; settle(&old, &v); old = v; put(out, "12_party_cancel_pressed"); v.pressed = -1; settle(&old, &v); old = v;
    v.pty.panel = BUI_PTY_ACTIONS; v.pty.menuCount = 3; v.pty.menuCursor = 0;
    names(v.pty.menuName[0], 14, "SHIFT"); names(v.pty.menuName[1], 14, "SUMMARY"); names(v.pty.menuName[2], 14, "CANCEL");
    settle(&old, &v); old = v; put(out, "13_party_menu"); hits(&v);
    v.pty.panel = BUI_PTY_YESNO; v.pty.menuCount = 2; names(v.pty.menuName[0], 14, "YES"); names(v.pty.menuName[1], 14, "NO");
    settle(&old, &v); old = v;
    v.pty.panel = BUI_PTY_TEXT; names(v.pty.message, sizeof(v.pty.message), "CHARIZARD is already in battle!");
    settle(&old, &v); old = v; put(out, "14_party_message"); hits(&v);
    v.pty.panel = BUI_PTY_NONE; v.pty.mode = BUI_PTY_FORCED; settle(&old, &v); old = v; put(out, "15_party_forced"); hits(&v);
    v.state = BUI_ACTION; settle(&old, &v); old = v;
    v.state = BUI_IDLE; settle(&old, &v);

    /* SUMMARY */
    {
        CtrSummaryUiView sv, so;
        int tab;

        memset(&sv, 0, sizeof(sv));
        sv.pressed = -1;
        sv.icon = 1;
        sv.data.ready = 1; sv.data.mon = 0; sv.data.cursor = 0; sv.data.level[0] = 0;
        names(sv.data.nickname, 12, "CHARIZARD"); names(sv.data.species, 12, "CHARIZARD"); names(sv.data.level, 8, "Lv50");
        names(sv.data.gender, 2, "M"); names(sv.data.dex, 8, "No.006"); names(sv.data.ot, 16, "RED"); names(sv.data.id, 8, "12345");
        names(sv.data.item, 20, "LEFTOVERS"); names(sv.data.types[0], 12, "FIRE"); names(sv.data.types[1], 12, "FLYING");
        sv.data.typeIds[0] = 10; sv.data.typeIds[1] = 2;
        names(sv.data.hp, 12, "120/150"); sv.data.hpCur = 120; sv.data.hpMax = 150;
        for (k = 0; k < 5; ++k) names(sv.data.stats[k], 8, k == 0 ? "104" : "89");
        names(sv.data.exp, 12, "125000"); names(sv.data.nextExp, 12, "4200");
        names(sv.data.ability, 16, "BLAZE"); names(sv.data.abilityDesc, 64, "Powers up FIRE moves in a pinch.");
        names(sv.data.memo, 256, "Hatched at Pallet Town. Met at Lv5.");
        names(sv.data.nature, 12, "ADAMANT"); sv.data.ribbons = 2;
        for (k = 0; k < 4; ++k) {
            static const char *const mv[4] = {"FLAMETHROWER", "WING ATTACK", "DRAGON CLAW", "SLASH"};
            static const char *const mt[4] = {"FIRE", "FLYING", "DRAGON", "NORMAL"};
            static const unsigned char mi[4] = {10, 2, 16, 0};

            sv.data.moves[k] = (unsigned short)(k + 1);
            names(sv.data.moveNames[k], 16, mv[k]); names(sv.data.pp[k], 12, "15/15"); names(sv.data.moveTypes[k], 12, mt[k]);
            names(sv.data.power[k], 8, "90"); names(sv.data.accuracy[k], 8, "100"); sv.data.moveTypeIds[k] = mi[k];
        }
        names(sv.data.moveDesc, 256, "A powerful fire attack that may burn the target.");
        so = sv;
        CtrSummaryUi_Draw(canvas, &so);   /* the screen opens drawn in full; updates follow */
        for (tab = 0; tab < 5; ++tab) {
            static const char *const nm[5] = {"16_summary_info", "17_summary_stats", "18_summary_moves", "19_summary_move_details",
                                              "20_summary_trainer"};

            sv.data.page = tab == 1 ? SUI_PAGE_SKILLS : tab == 2 ? SUI_PAGE_MOVES : tab == 3 ? SUI_PAGE_MOVES_INFO : SUI_PAGE_INFO;
            sv.trainer = tab == 4;
            sv.data.cursor = tab == 3 ? 1 : 0;
            settle_summary(&so, &sv); so = sv; put(out, nm[tab]); summary_hits(&sv);
            sv.pressed = SUI_HIT_NEXT; settle_summary(&so, &sv); so = sv;
            sv.pressed = SUI_HIT_BACK; settle_summary(&so, &sv); so = sv;
            sv.pressed = SUI_HIT_TAB + 1; settle_summary(&so, &sv); so = sv;
            sv.pressed = -1; settle_summary(&so, &sv); so = sv;
        }
        sv.loading = 1; settle_summary(&so, &sv);
    }

    /* The red of PokeTouch: nothing of the shared navy theme may remain on any state. */
#ifndef BATTLE_UI_BEFORE
    {
        static const uint16_t navy[] = {UI_BG, UI_BG_TOP, UI_OUTLINE, UI_PANEL_LO, UI_PANEL, UI_PANEL_HI, UI_BORDER,
                                        UI_BORDER_LIT, UI_SEPARATOR, UI_TEXT_SH, UI_TEXT_DIM, UI_HP_TRACK,
                                        UI_RGB(34, 56, 100)};
        CtrBattleView s = v;
        int states[] = {BUI_IDLE, BUI_ACTION, BUI_MOVE, BUI_TARGET, BUI_SAFARI, BUI_BAG, BUI_PARTY};

        for (size_t i = 0; i < sizeof(states) / sizeof(states[0]); ++i) {
            s.state = (uint8_t)states[i];
            CtrBattleUi_Draw(canvas, &s);
            for (int n = 0; n < W * H; ++n)
                for (size_t j = 0; j < sizeof(navy) / sizeof(navy[0]); ++j)
                    if (canvas[n] == navy[j]) {
                        fprintf(stderr, "state %d keeps the navy theme colour %04x at %d\n", states[i], navy[j], n);
                        assert(0);
                    }
            assert(at(1, 100) == PT_RED_BG);
        }
        {
            CtrSummaryUiView sv;

            memset(&sv, 0, sizeof(sv));
            sv.pressed = -1; sv.data.ready = 1; sv.data.typeIds[0] = 10; sv.data.typeIds[1] = 2; sv.data.hpCur = 90; sv.data.hpMax = 150;
            for (int page = 0; page < 4; ++page) {
                sv.data.page = page;
                CtrSummaryUi_Draw(canvas, &sv);
                for (int n = 0; n < W * H; ++n)
                    for (size_t j = 0; j < sizeof(navy) / sizeof(navy[0]); ++j)
                        if (canvas[n] == navy[j]) {
                            fprintf(stderr, "summary page %d keeps the navy theme colour %04x at %d\n", page, navy[j], n);
                            assert(0);
                        }
                assert(at(1, 100) == PT_RED_BG);
            }
        }
    }
#endif
    if (argc > 1)
        printf("hits %08x\nmask %08x\n", sHitCrc, sMaskCrc);
    puts("PASS battle ui");
    return 0;
}
