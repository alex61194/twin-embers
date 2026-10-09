/* The lower screen's components (3ds_ui_theme.h), included by 3ds_poketouch.c after its pixel, fill and font helpers.
 * Every PokeTouch and battle screen draws with these, so a button, a panel or a tab looks the same everywhere. Each
 * component draws only inside the rectangle it is given (the dirty-rectangle updates rely on it) and costs a few
 * rectangle fills: rounded corners are a short table of row cuts, the gloss a second fill, nothing per pixel but text,
 * icons and the ball. */

/* A component: not every screen uses every one. */
#define UI_FN static __attribute__((unused))

/* PokeTouch's exterior chrome, red (3ds_poketouch.c draws its frame with these; the battle screen's surfaces are too). */
#ifndef PT_RED_BG
#define PT_RED_BG UI_RGB(64, 4, 6)
#define PT_RED_PANEL UI_RGB(132, 12, 16)
#define PT_RED_EDGE UI_RGB(238, 58, 50)
#define PT_RED_LIGHT UI_RGB(255, 132, 92)
#endif

/* The surfaces a screen is built on, so one set of components can wear either of the two looks: the navy of the
 * shared theme (everything but the battle screen) and PokeTouch's red chrome (the battle screen). A screen picks
 * its own with sUiS before it draws and restores sUiNavy after. */
typedef struct {
    uint16_t bg, bgTop, outline, panelLo, panel, panelHi, border, borderLit, separator, textSh, textDim, track;
    UiButtonStyle primary, neutral, cancel;           /* the lead button of a group, the others, CANCEL */
    UiButtonStyle fight, bag, pokemon, run;           /* the battle's four actions */
} UiSurface;

static const UiSurface sUiNavy = {
    UI_BG, UI_BG_TOP, UI_OUTLINE, UI_PANEL_LO, UI_PANEL, UI_PANEL_HI, UI_BORDER, UI_BORDER_LIT, UI_SEPARATOR, UI_TEXT_SH,
    UI_TEXT_DIM, UI_HP_TRACK,
    UI_STYLE_PRIMARY, UI_STYLE_NEUTRAL, UI_STYLE_CANCEL,
    UI_STYLE_FIGHT, UI_STYLE_BAG, UI_STYLE_POKEMON, UI_STYLE_RUN,
};

/* PokeTouch's red: its frame, panels and buttons (PT_RED_*, sChrome and sChromeOn in 3ds_poketouch.c), the light
 * blue of a selection and white text kept as they are there, the text's shadow and secondary ink warmed to match. */
static const UiSurface sUiRed = {
    PT_RED_BG, UI_RGB(92, 8, 10), UI_RGB(28, 2, 3), UI_RGB(44, 3, 5), PT_RED_PANEL, UI_RGB(176, 34, 30), PT_RED_EDGE,
    PT_RED_LIGHT, UI_RGB(150, 22, 24), UI_RGB(30, 2, 3), UI_RGB(255, 208, 192), UI_RGB(40, 3, 5),
    {UI_RGB(208, 26, 20), UI_RGB(216, 32, 24), UI_RGB(112, 10, 8)},          /* primary: sChromeOn, calmer, so 1x labels keep 4.5:1 */
    {UI_RGB(96, 8, 12), UI_RGB(156, 34, 32), UI_RGB(52, 4, 6)},              /* neutral */
    {UI_RGB(120, 12, 16), UI_RGB(190, 48, 40), UI_RGB(46, 2, 4)},            /* cancel */
    {UI_RGB(218, 32, 24), UI_RGB(231, 78, 50), UI_RGB(120, 10, 8)},          /* fight: sChromeOn, gloss kept under white 3x text */
    {UI_RGB(190, 22, 22), UI_RGB(236, 77, 56), UI_RGB(102, 8, 8)},           /* bag: sChrome with the gloss held to 3.4:1 under white */
    {UI_RGB(190, 22, 22), UI_RGB(236, 77, 56), UI_RGB(102, 8, 8)},           /* pokemon */
    {UI_RGB(190, 22, 22), UI_RGB(236, 77, 56), UI_RGB(102, 8, 8)},           /* run */
};

static const UiSurface *sUiS = &sUiNavy;
#define UIS_BG (sUiS->bg)
#define UIS_BG_TOP (sUiS->bgTop)
#define UIS_OUTLINE (sUiS->outline)
#define UIS_PANEL_LO (sUiS->panelLo)
#define UIS_PANEL (sUiS->panel)
#define UIS_PANEL_HI (sUiS->panelHi)
#define UIS_BORDER (sUiS->border)
#define UIS_BORDER_LIT (sUiS->borderLit)
#define UIS_SEPARATOR (sUiS->separator)
#define UIS_TEXT_SH (sUiS->textSh)
#define UIS_TEXT_DIM (sUiS->textDim)
#define UIS_HP_TRACK (sUiS->track)

UI_FN int UiIn(const CtrPokeTouchRect *r, int x, int y)
{
    return x >= r->x && x < r->x + r->w && y >= r->y && y < r->y + r->h;
}

UI_FN CtrPokeTouchRect UiInset(const CtrPokeTouchRect *r, int d)
{
    CtrPokeTouchRect i = {(int16_t)(r->x + d), (int16_t)(r->y + d), (int16_t)(r->w - 2 * d), (int16_t)(r->h - 2 * d)};
    return i;
}

/* `a` blended toward `b` by n/8. */
UI_FN uint16_t UiMix(uint16_t a, uint16_t b, int n)
{
    int r = ((a >> 11) * (8 - n) + (b >> 11) * n) / 8;
    int g = (((a >> 5) & 63) * (8 - n) + ((b >> 5) & 63) * n) / 8;
    int bl = ((a & 31) * (8 - n) + (b & 31) * n) / 8;
    return (uint16_t)(r << 11 | g << 5 | bl);
}

/* How many pixels a rounded corner cuts from each end of its first rows, by radius. */
static const uint8_t sUiCut[6][4] = {{0}, {1}, {1}, {2, 1}, {3, 2, 1}, {4, 2, 1, 1}};

/* A filled rectangle with rounded corners: the corner rows as spans, the rest one fill. */
UI_FN void UiRoundFill(int x, int y, int w, int h, int radius, uint16_t c)
{
    int rows;

    if (w <= 0 || h <= 0)
        return;
    if (radius > 5)
        radius = 5;
    if (radius * 2 > h)
        radius = h / 2;
    if (radius * 2 > w)
        radius = w / 2;
    for (rows = 0; rows < 4 && sUiCut[radius][rows]; ++rows) {
        int cut = sUiCut[radius][rows];

        Fill(x + cut, y + rows, w - 2 * cut, 1, c);
        Fill(x + cut, y + h - 1 - rows, w - 2 * cut, 1, c);
    }
    Fill(x, y + rows, w, h - 2 * rows, c);
}

/* Rows [y, y + h) of a rounded shape whose corners are cut only at the top (`top`) and/or the bottom (`bottom`): the
 * pieces a button is made of, each pixel painted once. */
UI_FN void UiRoundBand(int x, int y, int w, int h, int radius, int top, int bottom, uint16_t c)
{
    int rows = 0, first = 0, last = h;

    if (w <= 0 || h <= 0)
        return;
    if (radius > 5)
        radius = 5;
    while (rows < 4 && sUiCut[radius][rows])
        ++rows;
    if (top)
        for (; first < rows && first < h; ++first)
            Fill(x + sUiCut[radius][first], y + first, w - 2 * sUiCut[radius][first], 1, c);
    if (bottom)
        for (int i = 0; i < rows && last > first; ++i, --last)
            Fill(x + sUiCut[radius][i], y + last - 1, w - 2 * sUiCut[radius][i], 1, c);
    Fill(x, y + first, w, last - first, c);
}

UI_FN void UiRoundRect(const CtrPokeTouchRect *r, int radius, uint16_t c)
{
    UiRoundFill(r->x, r->y, r->w, r->h, radius, c);
}

/* A rounded ring `width` pixels thick along the inside of `r`, over whatever is there. */
UI_FN void UiRing(const CtrPokeTouchRect *r, int radius, int width, uint16_t c)
{
    for (int i = 0; i < width; ++i) {
        int x = r->x + i, y = r->y + i, w = r->w - 2 * i, h = r->h - 2 * i, rad = radius - i < 0 ? 0 : radius - i;
        int rows = 0;

        if (w <= 0 || h <= 0)
            return;
        for (; rows < 4 && rad <= 5 && sUiCut[rad][rows]; ++rows) {
            int cut = sUiCut[rad][rows], next = rows + 1 < 4 ? sUiCut[rad][rows + 1] : 0;
            int span = cut - next > 1 ? cut - next : 1;

            Fill(x + cut, y + rows, span, 1, c);
            Fill(x + w - cut - span, y + rows, span, 1, c);
            Fill(x + cut, y + h - 1 - rows, span, 1, c);
            Fill(x + w - cut - span, y + h - 1 - rows, span, 1, c);
        }
        Fill(x + sUiCut[rad][0], y, w - 2 * sUiCut[rad][0], 1, c);
        Fill(x + sUiCut[rad][0], y + h - 1, w - 2 * sUiCut[rad][0], 1, c);
        Fill(x, y + rows, 1, h - 2 * rows, c);
        Fill(x + w - 1, y + rows, 1, h - 2 * rows, c);
    }
}

/* Text: FireRed's own glyphs (latin_normal), white on the dark surfaces with a dark shadow. `top` is where the
 * capitals start. */
UI_FN int UiTextWidth(const char *s, int scale)
{
    int w = 0;

    while (*s)
        w += Glyph(&s)->width;
    return w * scale;
}

UI_FN void UiText(const char *s, int x, int top, uint16_t fg, uint16_t shadow, int scale, int limit)
{
    int y = top - 3 * scale;

    while (*s) {
        const PtGlyph *g = Glyph(&s);

        if (x + g->width * scale > limit)
            break;
        for (int row = 0; row < 16; ++row)
            for (int col = 0; col < g->width; ++col) {
                unsigned v = (g->rows[row] >> (2 * col)) & 3;
                if (!v)
                    continue;
                if (scale == 1)
                    Px(x + col, y + row, v == 1 ? fg : shadow);
                else
                    Fill(x + col * scale, y + row * scale, scale, scale, v == 1 ? fg : shadow);
            }
        x += g->width * scale;
    }
}

/* Centred on `r` across, its capitals vertically centred when `top` < 0. */
UI_FN void UiTextCentered(const char *s, const CtrPokeTouchRect *r, int top, uint16_t fg, uint16_t shadow, int scale)
{
    int w = UiTextWidth(s, scale), x = r->x + (r->w - w) / 2;

    if (top < 0)
        top = r->y + (r->h - UI_TEXT_H * scale) / 2;
    UiText(s, x < r->x ? r->x : x, top, fg, shadow, scale, r->x + r->w);
}

/* Right-aligned to `right`. */
UI_FN void UiTextRight(const char *s, int right, int top, uint16_t fg, uint16_t shadow, int scale)
{
    int w = UiTextWidth(s, scale);

    UiText(s, right - w, top, fg, shadow, scale, right);
}

/* A panel: a 1px border, the body, a lighter first line (light from above). */
UI_FN void UiPanel(const CtrPokeTouchRect *r, uint16_t body, uint16_t border)
{
    UiRing(r, UI_RADIUS_PANEL, 1, border);
    UiRoundFill(r->x + 1, r->y + 1, r->w - 2, r->h - 2, UI_RADIUS_PANEL - 1, body);
    Fill(r->x + 3, r->y + 1, r->w - 6, 1, UiMix(body, border, 4));
}

/* A thin rule across a panel. */
UI_FN void UiSeparator(int x, int y, int w)
{
    Fill(x, y, w, 1, UIS_SEPARATOR);
}

/* A raised button: the outline, a lip in the shaded colour, the body with a gloss over its top half and a light first
 * line. Pressed it sinks (no lip, body one pixel lower and darker); focused it gets a white ring inside the outline;
 * selected a blue one; disabled it is grey. The caller writes the label (UiButtonLabelTop says where). */
UI_FN void UiButton(const CtrPokeTouchRect *r, const UiButtonStyle *style, int state)
{
    static const UiButtonStyle sDisabled = UI_STYLE_DISABLED;
    const UiButtonStyle *s = (state & UI_DISABLED) ? &sDisabled : style;
    int pressed = (state & UI_PRESSED) && !(state & UI_DISABLED);
    int x = r->x + 1, w = r->w - 2;

    if (pressed) {
        uint16_t body = UiMix(s->body, s->shade, 4);

        UiRoundRect(r, UI_RADIUS_BUTTON, UIS_OUTLINE);       /* transient: the simple way */
        UiRoundFill(x, r->y + 2, w, r->h - 3, UI_RADIUS_BUTTON - 1, body);
        Fill(x + 3, r->y + 2, w - 6, 1, s->shade);
    } else {
        int h = r->h - 2 - UI_DEPTH, gloss = h * 4 / 9;

        /* The outline as a ring and the face in three bands (gloss, body, lip) that fill its inside exactly: every
         * pixel is painted once. */
        UiRing(r, UI_RADIUS_BUTTON, 1, UIS_OUTLINE);
        UiRoundBand(x, r->y + 1, w, gloss + 1, UI_RADIUS_BUTTON - 1, 1, 0, UiMix(s->body, s->lit, 3));
        Fill(x + 3, r->y + 1, w - 6, 1, s->lit);
        Fill(x, r->y + 2 + gloss, w, h - gloss - 1, s->body);
        UiRoundBand(x, r->y + 1 + h, w, UI_DEPTH, UI_RADIUS_BUTTON - 1, 0, 1, s->shade);
    }
    if (state & UI_SELECTED) {
        CtrPokeTouchRect ring = UiInset(r, 1);

        UiRing(&ring, UI_RADIUS_BUTTON - 1, 1, UI_FOCUS);
    }
    if ((state & UI_FOCUSED) && !(state & UI_DISABLED)) {
        CtrPokeTouchRect ring = UiInset(r, 1);

        UiRing(&ring, UI_RADIUS_BUTTON - 1, UI_FOCUS_W, UI_TEXT);
    }
}

/* Where a label of `scale` sits on a button of that state: centred on the raised face (or the sunken one). */
UI_FN int UiButtonLabelTop(const CtrPokeTouchRect *r, int state, int scale)
{
    int face = r->h - 2 - ((state & UI_PRESSED) ? 1 : UI_DEPTH);

    return r->y + 1 + ((state & UI_PRESSED) ? 1 : 0) + (face - UI_TEXT_H * scale) / 2;
}

UI_FN uint16_t UiLabelColor(int state)
{
    return (state & UI_DISABLED) ? UI_TEXT_OFF : UI_TEXT;
}

/* A button with its label centred. */
UI_FN void UiLabelButton(const CtrPokeTouchRect *r, const UiButtonStyle *style, int state, const char *label, int scale)
{
    CtrPokeTouchRect face = *r;

    UiButton(r, style, state);
    face.y = (int16_t)UiButtonLabelTop(r, state, scale);
    UiTextCentered(label, &face, face.y, UiLabelColor(state), UIS_TEXT_SH, scale);
}

/* A focus ring alone (an element whose body is drawn its own way). */
UI_FN void UiFocus(const CtrPokeTouchRect *r)
{
    CtrPokeTouchRect ring = UiInset(r, 1);

    UiRing(&ring, UI_RADIUS_BUTTON - 1, UI_FOCUS_W, UI_TEXT);
}

/* A tab: the active one raised in the primary blue with a bright underline, the others flat and dark. */
UI_FN void UiTab(const CtrPokeTouchRect *r, const char *label, int state)
{
    const UiButtonStyle *sOn = &sUiS->primary, *sOff = &sUiS->neutral;
    int on = (state & UI_SELECTED) != 0;

    UiButton(r, on ? sOn : sOff, state & ~UI_SELECTED);
    if (on)
        Fill(r->x + 6, r->y + r->h - 3, r->w - 12, 2, UI_FOCUS_LIT);
    {
        CtrPokeTouchRect face = *r;

        UiTextCentered(label, &face, UiButtonLabelTop(r, state, 1), on ? UI_TEXT : UIS_TEXT_DIM, UIS_TEXT_SH, 1);
    }
}

/* White text where it reads (about 3:1 or better), the dark shadow colour on the light colours (yellow, sand...). */
UI_FN uint16_t UiInkOn(uint16_t bg)
{
    int r = (bg >> 11) << 3, g = ((bg >> 5) & 63) << 2, b = (bg & 31) << 3;

    return (299 * r + 587 * g + 114 * b) / 1000 > 118 ? UIS_TEXT_SH : UI_TEXT;
}

/* A badge (a pill): a coloured rounded box with a short white caption. */
UI_FN void UiBadge(const CtrPokeTouchRect *r, uint16_t color, const char *text)
{
    UiRoundRect(r, UI_RADIUS_BADGE, UIS_OUTLINE);
    UiRoundFill(r->x + 1, r->y + 1, r->w - 2, r->h - 2, UI_RADIUS_BADGE - 1, color);
    Fill(r->x + 2, r->y + 1, r->w - 4, 1, UiMix(color, UI_TEXT, 3));
    if (sUiS == &sUiRed) {      /* the battle screen: the ink follows the colour */
        uint16_t ink = UiInkOn(color);

        UiTextCentered(text, r, -1, ink, ink == UI_TEXT ? UiMix(color, UIS_OUTLINE, 5) : UiMix(color, UI_TEXT, 3), 1);
        return;
    }
    UiTextCentered(text, r, -1, UI_TEXT, UiMix(color, UIS_OUTLINE, 5), 1);
}

/* FireRed's type colours (its type numbers 0-17), for move cards and type badges. */
static const uint16_t sUiType[18] = {
    UI_RGB(150, 150, 112), UI_RGB(196, 48, 40), UI_RGB(146, 124, 226), UI_RGB(160, 64, 160), UI_RGB(216, 180, 96),
    UI_RGB(178, 152, 56), UI_RGB(160, 180, 32), UI_RGB(110, 88, 150), UI_RGB(170, 170, 196), UI_RGB(104, 104, 104),
    UI_RGB(232, 110, 40), UI_RGB(86, 136, 236), UI_RGB(110, 192, 72), UI_RGB(240, 200, 40), UI_RGB(240, 84, 130),
    UI_RGB(120, 206, 206), UI_RGB(104, 60, 236), UI_RGB(104, 80, 66),
};

UI_FN uint16_t UiTypeColor(int type)
{
    return sUiType[type >= 0 && type < 18 ? type : 0];
}

UI_FN void UiTypeBadge(const CtrPokeTouchRect *r, int type, const char *name)
{
    UiBadge(r, UiTypeColor(type), name);
}

/* A bar: the dark track and `value` of `max` filled in `color` (at least a pixel while it is not 0). */
UI_FN void UiProgressBar(const CtrPokeTouchRect *r, unsigned value, unsigned max, uint16_t color)
{
    int inner = r->w - 2, fill = max ? (int)((unsigned long)value * (unsigned)inner / max) : 0;

    if (value && fill < 1)
        fill = 1;
    if (fill > inner)
        fill = inner;
    UiRoundRect(r, 1, UIS_OUTLINE);
    Fill(r->x + 1, r->y + 1, inner, r->h - 2, UIS_HP_TRACK);
    if (fill > 0) {
        Fill(r->x + 1, r->y + 1, fill, r->h - 2, color);
        if (r->h > 4)
            Fill(r->x + 1, r->y + 1, fill, 1, UiMix(color, UI_TEXT, 3));
    }
}

/* FireRed's own thresholds: green over half, yellow over a fifth, red below. */
UI_FN uint16_t UiHpColor(unsigned hp, unsigned max)
{
    return hp * 2 > max ? UI_HP_HIGH : hp * 5 > max ? UI_HP_MID : UI_HP_LOW;
}

UI_FN void UiHpBar(const CtrPokeTouchRect *r, unsigned hp, unsigned max)
{
    UiProgressBar(r, hp, max, UiHpColor(hp, max));
}

/* A screen's header: a panel strip with a title on the left and an optional caption on the right. */
UI_FN void UiHeader(const CtrPokeTouchRect *r, const char *title, const char *caption)
{
    UiPanel(r, UIS_PANEL, UIS_BORDER);
    Fill(r->x + 1, r->y + r->h - 2, r->w - 2, 1, UI_FOCUS);
    UiText(title, r->x + 8, r->y + (r->h - UI_TEXT_H) / 2, UI_TEXT, UIS_TEXT_SH, 1, r->x + r->w - 4);
    if (caption && caption[0])
        UiTextRight(caption, r->x + r->w - 8, r->y + (r->h - UI_TEXT_H) / 2, UIS_TEXT_DIM, UIS_TEXT_SH, 1);
}

/* A footer: a dark strip with a hint centred in it. */
UI_FN void UiFooter(const CtrPokeTouchRect *r, const char *hint)
{
    UiPanel(r, UIS_PANEL_LO, UIS_SEPARATOR);
    if (hint && hint[0])
        UiTextCentered(hint, r, -1, UIS_TEXT_DIM, UIS_TEXT_SH, 1);
}

/* An indexed image (palette entries, 0 clear) at `scale`, toned grey when off. */
UI_FN void UiImage(const uint8_t *pixels, const uint16_t *pal, int stride, int w, int h, int x, int y, int scale, int on)
{
    for (int py = 0; py < h; ++py)
        for (int px = 0; px < w; ++px) {
            uint8_t v = pixels[py * stride + px];

            if (v)
                Fill(x + px * scale, y + py * scale, scale, scale, Tone(pal[v], on));
        }
}

/* A button that is an icon over a caption (or an icon alone when `label` is NULL). */
UI_FN void UiIconButton(const CtrPokeTouchRect *r, const UiButtonStyle *style, int state, const PtImage *icon, int scale,
                         const char *label, int labelScale)
{
    int face = r->h - 2 - ((state & UI_PRESSED) ? 1 : UI_DEPTH), dy = (state & UI_PRESSED) ? 1 : 0;
    int ih = icon ? icon->h * scale : 0, lh = label ? UI_TEXT_H * labelScale : 0, gap = icon && label ? 6 : 0;
    int top = r->y + 1 + dy + (face - ih - gap - lh) / 2;

    UiButton(r, style, state);
    if (icon)
        UiImage(icon->pixels, icon->pal, icon->w, icon->w, icon->h, r->x + (r->w - icon->w * scale) / 2, top, scale,
                !(state & UI_DISABLED));
    if (label) {
        CtrPokeTouchRect lr = *r;

        UiTextCentered(label, &lr, top + ih + gap, UiLabelColor(state), UIS_TEXT_SH, labelScale);
    }
}

/* Halves the brightness of what is on the canvas inside `r`: the backdrop of a modal (once, when it opens). */
UI_FN void UiDim(const CtrPokeTouchRect *r)
{
    int x0 = r->x < 0 ? 0 : r->x, y0 = r->y < 0 ? 0 : r->y;
    int x1 = r->x + r->w > W ? W : r->x + r->w, y1 = r->y + r->h > H ? H : r->y + r->h;

    for (int x = x0; x < x1; ++x) {
        uint16_t *p = sCanvas + (size_t)x * H + (H - (size_t)y1);

        for (int n = y1 - y0; n > 0; --n, ++p)
            *p = (uint16_t)((*p >> 1) & 0x7BEF);
    }
}

/* A modal: the dimmed screen, then a raised panel with a light border. */
UI_FN void UiModal(const CtrPokeTouchRect *backdrop, const CtrPokeTouchRect *r)
{
    UiDim(backdrop);
    UiRoundRect(r, UI_RADIUS_BUTTON, UIS_OUTLINE);
    UiRoundFill(r->x + 1, r->y + 1, r->w - 2, r->h - 2, UI_RADIUS_BUTTON - 1, UIS_BORDER_LIT);
    UiRoundFill(r->x + 2, r->y + 2, r->w - 4, r->h - 4, UI_RADIUS_PANEL - 1, UIS_PANEL);
    Fill(r->x + 4, r->y + 2, r->w - 8, 1, UIS_PANEL_HI);
}

/* A small triangle: dir -1 left, 1 right, -2 up, 2 down; 5 deep. */
UI_FN void UiArrow(int x, int y, int dir, uint16_t c)
{
    for (int i = 0; i < 5; ++i) {
        if (dir == -1 || dir == 1)
            Fill(dir < 0 ? x + i : x + 4 - i, y + 4 - i, 1, 1 + 2 * i, c);
        else
            Fill(x + 4 - i, dir < 0 ? y + i : y + 4 - i, 1 + 2 * i, 1, c);
    }
}

/* A filled disc of radius `r` (half pixels allowed through `d2max`), for balls and markers. */
UI_FN void UiDisc(int cx, int cy, int r, uint16_t c)
{
    for (int dy = -r; dy < r; ++dy)
        for (int dx = -r; dx < r; ++dx) {
            int d2 = (2 * dx + 1) * (2 * dx + 1) + (2 * dy + 1) * (2 * dy + 1);

            if (d2 <= 4 * r * r)
                Px(cx + dx, cy + dy, c);
        }
}
