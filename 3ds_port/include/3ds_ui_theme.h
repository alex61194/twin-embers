#ifndef CTR_UI_THEME_H
#define CTR_UI_THEME_H

#include <stdint.h>

/*
 * The lower screen's design system: one palette, one set of metrics and one set of components (3ds_ui_theme.inc)
 * that every PokeTouch and battle screen draws with. Dark navy panels, thin light borders, white text with a dark
 * shadow, buttons coloured by what they do, a bright blue for focus. Designed for the real 320x240 canvas: no
 * scaling, no alpha, no runtime images; a component is a handful of rectangle fills.
 */

/* RGB888 -> RGB565, at compile time. */
#define UI_RGB(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))

/* Surfaces, darkest first. */
#define UI_BG          UI_RGB(10, 16, 30)     /* the screen behind everything */
#define UI_BG_TOP      UI_RGB(16, 26, 46)     /* the band at the top of the screen (a cheap two-step gradient) */
#define UI_OUTLINE     UI_RGB(4, 8, 16)       /* the outline around buttons and cards */
#define UI_PANEL_LO    UI_RGB(15, 23, 40)     /* an inset (a value box, a list's background) */
#define UI_PANEL       UI_RGB(23, 34, 58)     /* a panel or card */
#define UI_PANEL_HI    UI_RGB(32, 47, 78)     /* a panel's top line, a raised row */
#define UI_BORDER      UI_RGB(56, 76, 116)    /* a panel's thin border */
#define UI_BORDER_LIT  UI_RGB(98, 126, 178)   /* a frame that holds a picture */
#define UI_SEPARATOR   UI_RGB(40, 56, 88)

/* Text. */
#define UI_TEXT        UI_RGB(244, 247, 252)
#define UI_TEXT_SH     UI_RGB(8, 12, 24)      /* the shadow under white text */
#define UI_TEXT_DIM    UI_RGB(150, 166, 196)  /* secondary information */
#define UI_TEXT_OFF    UI_RGB(86, 98, 124)    /* disabled */

/* Focus and selection. */
#define UI_FOCUS       UI_RGB(70, 172, 255)   /* the ring of the D-pad's focus, an active tab's underline */
#define UI_FOCUS_LIT   UI_RGB(186, 226, 255)
#define UI_SELECT      UI_RGB(30, 82, 160)    /* a selected row's body */
#define UI_SELECT_HI   UI_RGB(44, 108, 196)

/* State colours. */
#define UI_WARN        UI_RGB(238, 104, 52)
#define UI_DANGER      UI_RGB(226, 64, 60)
#define UI_OK          UI_RGB(72, 206, 104)
#define UI_HP_HIGH     UI_RGB(72, 208, 96)
#define UI_HP_MID      UI_RGB(242, 194, 48)
#define UI_HP_LOW      UI_RGB(234, 72, 56)
#define UI_HP_TRACK    UI_RGB(32, 38, 56)

/* Metrics. */
enum {
    UI_RADIUS_BUTTON = 4,     /* buttons and cards */
    UI_RADIUS_PANEL = 3,      /* panels, rows, tabs */
    UI_RADIUS_BADGE = 2,      /* badges and pills */
    UI_BORDER_W = 1,
    UI_FOCUS_W = 2,           /* the focus ring, inside the outline */
    UI_PAD = 6,               /* inner padding of a panel */
    UI_MARGIN = 4,            /* between neighbouring elements */
    UI_ROW_H = 24,            /* a list row */
    UI_TEXT_H = 9,            /* FireRed's capitals, in pixels */
    UI_LINE_H = 11,           /* lines of running text */
    UI_DEPTH = 3,             /* the shaded lip under a raised button */
};

/* A button's colours: its body, the gloss over its top half and the lip under it. */
typedef struct {
    uint16_t body, lit, shade;
} UiButtonStyle;

/* What an interactive element is showing: any combination. */
enum {
    UI_NORMAL = 0,
    UI_FOCUSED = 1 << 0,      /* the D-pad's focus (the controller's own cursor) */
    UI_PRESSED = 1 << 1,      /* under a held touch */
    UI_DISABLED = 1 << 2,     /* cannot be used now */
    UI_SELECTED = 1 << 3,     /* the active one of a group (a tab, the screen shown, a mode that is on) */
};

/* Functional styles: the same colour always means the same kind of action. */
#define UI_STYLE_FIGHT    {UI_RGB(206, 54, 54), UI_RGB(236, 98, 90), UI_RGB(138, 30, 36)}
#define UI_STYLE_BAG      {UI_RGB(40, 150, 78), UI_RGB(92, 200, 116), UI_RGB(22, 98, 50)}
#define UI_STYLE_POKEMON  {UI_RGB(38, 104, 206), UI_RGB(92, 156, 244), UI_RGB(22, 64, 142)}
#define UI_STYLE_RUN      {UI_RGB(198, 138, 22), UI_RGB(240, 190, 70), UI_RGB(140, 92, 12)}
#define UI_STYLE_CANCEL   {UI_RGB(34, 56, 100), UI_RGB(62, 92, 148), UI_RGB(18, 30, 60)}
#define UI_STYLE_PRIMARY  {UI_RGB(38, 104, 206), UI_RGB(92, 156, 244), UI_RGB(22, 64, 142)}
#define UI_STYLE_NEUTRAL  {UI_RGB(30, 44, 74), UI_RGB(48, 66, 104), UI_RGB(16, 24, 44)}
#define UI_STYLE_DISABLED {UI_RGB(52, 58, 74), UI_RGB(66, 72, 90), UI_RGB(34, 38, 50)}

#endif
