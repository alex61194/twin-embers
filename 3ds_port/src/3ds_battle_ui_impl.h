/* The battle screen (included at the end of 3ds_poketouch.c, whose drawing helpers and design system it uses).
 * The lower screen while FireRed's battle is on the upper one, in the shared design (3ds_ui_theme.h) wearing
 * PokeTouch's red chrome (sUiRed): the dark red backdrop, the strip of six Poke Balls on top, then the body: the MAIN
 * buttons in the chrome's reds (FIGHT the brightest), the move cards in their type's colour, the target cards (the
 * opposing side red, the player's blue), the Safari Zone's four, the battle Bag and the battle Party. Selection and
 * focus keep the blue and the white ring PokeTouch uses. Everything shown is what the controller reports; nothing
 * here moves or resizes a rectangle. */

#include "3ds_icons.h"
#include "3ds_type_chart.h"

/* Arcade HUD Font: 8-pixel tall, high-impact bold typography */
static const uint8_t sArcadeGlyphs[96][8] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* 32: ' ' */
    {0x03, 0x03, 0x03, 0x03, 0x03, 0x00, 0x03, 0x03}, /* 33: '!' */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* 34: '"' */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* 35: '#' */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* 36: '$' */
    {0x19, 0x1B, 0x06, 0x0C, 0x0C, 0x18, 0x1B, 0x03}, /* 37: '%' */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* 38: '&' */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* 39: '\'' */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* 40: '(' */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* 41: ')' */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* 42: '*' */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* 43: '+' */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* 44: ',' */
    {0x00, 0x00, 0x00, 0x1F, 0x1F, 0x00, 0x00, 0x00}, /* 45: '-' */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x03}, /* 46: '.' */
    {0x01, 0x03, 0x06, 0x06, 0x0C, 0x0C, 0x18, 0x10}, /* 47: '/' */
    {0x0E, 0x19, 0x1B, 0x1D, 0x19, 0x19, 0x19, 0x0E}, /* 48: '0' */
    {0x0C, 0x1C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1F}, /* 49: '1' */
    {0x0E, 0x19, 0x01, 0x02, 0x06, 0x0C, 0x18, 0x1F}, /* 50: '2' */
    {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x01, 0x1E}, /* 51: '3' */
    {0x06, 0x0E, 0x1A, 0x1A, 0x1F, 0x02, 0x02, 0x02}, /* 52: '4' */
    {0x1F, 0x18, 0x18, 0x1E, 0x01, 0x01, 0x19, 0x0E}, /* 53: '5' */
    {0x0E, 0x18, 0x18, 0x1E, 0x19, 0x19, 0x19, 0x0E}, /* 54: '6' */
    {0x1F, 0x01, 0x02, 0x06, 0x04, 0x0C, 0x0C, 0x08}, /* 55: '7' */
    {0x0E, 0x19, 0x19, 0x0E, 0x19, 0x19, 0x19, 0x0E}, /* 56: '8' */
    {0x0E, 0x19, 0x19, 0x0F, 0x01, 0x01, 0x19, 0x0E}, /* 57: '9' */
    {0x00, 0x03, 0x03, 0x00, 0x00, 0x03, 0x03, 0x00}, /* 58: ':' */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* 59: ';' */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* 60: '<' */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* 61: '=' */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* 62: '>' */
    {0x0E, 0x19, 0x01, 0x02, 0x04, 0x00, 0x04, 0x04}, /* 63: '?' */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* 64: '@' */
    {0x04, 0x0E, 0x0E, 0x1B, 0x1B, 0x1F, 0x1B, 0x1B}, /* 65: 'A' */
    {0x1E, 0x19, 0x19, 0x1E, 0x19, 0x19, 0x19, 0x1E}, /* 66: 'B' */
    {0x0F, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x0F}, /* 67: 'C' */
    {0x1E, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x1E}, /* 68: 'D' */
    {0x1F, 0x18, 0x18, 0x1E, 0x18, 0x18, 0x18, 0x1F}, /* 69: 'E' */
    {0x1F, 0x18, 0x18, 0x1E, 0x18, 0x18, 0x18, 0x18}, /* 70: 'F' */
    {0x0F, 0x18, 0x18, 0x18, 0x1B, 0x19, 0x19, 0x0F}, /* 71: 'G' */
    {0x1B, 0x1B, 0x1B, 0x1F, 0x1B, 0x1B, 0x1B, 0x1B}, /* 72: 'H' */
    {0x07, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x07}, /* 73: 'I' */
    {0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0C}, /* 74: 'J' */
    {0x1B, 0x1A, 0x1C, 0x1C, 0x1E, 0x1B, 0x19, 0x19}, /* 75: 'K' */
    {0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1F}, /* 76: 'L' */
    {0x63, 0x77, 0x77, 0x7F, 0x6B, 0x63, 0x63, 0x63}, /* 77: 'M' */
    {0x31, 0x39, 0x39, 0x3D, 0x35, 0x37, 0x33, 0x31}, /* 78: 'N' */
    {0x0E, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x0E}, /* 79: 'O' */
    {0x1E, 0x19, 0x19, 0x1E, 0x18, 0x18, 0x18, 0x18}, /* 80: 'P' */
    {0x1C, 0x32, 0x32, 0x32, 0x32, 0x36, 0x32, 0x1D}, /* 81: 'Q' */
    {0x1E, 0x19, 0x19, 0x1E, 0x1C, 0x1A, 0x19, 0x19}, /* 82: 'R' */
    {0x0F, 0x18, 0x18, 0x0E, 0x03, 0x03, 0x03, 0x1E}, /* 83: 'S' */
    {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}, /* 84: 'T' */
    {0x1B, 0x1B, 0x1B, 0x1B, 0x1B, 0x1B, 0x1B, 0x0E}, /* 85: 'U' */
    {0x1B, 0x1B, 0x1B, 0x1B, 0x0E, 0x0E, 0x04, 0x04}, /* 86: 'V' */
    {0x63, 0x63, 0x63, 0x6B, 0x6B, 0x7F, 0x3E, 0x22}, /* 87: 'W' */
    {0x1B, 0x1B, 0x0E, 0x04, 0x04, 0x0E, 0x1B, 0x1B}, /* 88: 'X' */
    {0x1B, 0x1B, 0x1B, 0x0E, 0x04, 0x04, 0x04, 0x04}, /* 89: 'Y' */
    {0x1F, 0x03, 0x06, 0x0C, 0x0C, 0x18, 0x18, 0x1F}, /* 90: 'Z' */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* 91: '[' */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* 92: '\\' */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* 93: ']' */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* 94: '^' */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* 95: '_' */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* 96: '`' */
    {0x04, 0x0E, 0x0E, 0x1B, 0x1B, 0x1F, 0x1B, 0x1B}, /* 97: 'a' */
    {0x1E, 0x19, 0x19, 0x1E, 0x19, 0x19, 0x19, 0x1E}, /* 98: 'b' */
    {0x0F, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x0F}, /* 99: 'c' */
    {0x1E, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x1E}, /* 100: 'd' */
    {0x1F, 0x18, 0x18, 0x1E, 0x18, 0x18, 0x18, 0x1F}, /* 101: 'e' */
    {0x1F, 0x18, 0x18, 0x1E, 0x18, 0x18, 0x18, 0x18}, /* 102: 'f' */
    {0x0F, 0x18, 0x18, 0x18, 0x1B, 0x19, 0x19, 0x0F}, /* 103: 'g' */
    {0x1B, 0x1B, 0x1B, 0x1F, 0x1B, 0x1B, 0x1B, 0x1B}, /* 104: 'h' */
    {0x07, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x07}, /* 105: 'i' */
    {0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0C}, /* 106: 'j' */
    {0x1B, 0x1A, 0x1C, 0x1C, 0x1E, 0x1B, 0x19, 0x19}, /* 107: 'k' */
    {0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1F}, /* 108: 'l' */
    {0x63, 0x77, 0x77, 0x7F, 0x6B, 0x63, 0x63, 0x63}, /* 109: 'm' */
    {0x31, 0x39, 0x39, 0x3D, 0x35, 0x37, 0x33, 0x31}, /* 110: 'n' */
    {0x0E, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x0E}, /* 111: 'o' */
    {0x1E, 0x19, 0x19, 0x1E, 0x18, 0x18, 0x18, 0x18}, /* 112: 'p' */
    {0x1C, 0x32, 0x32, 0x32, 0x32, 0x36, 0x32, 0x1D}, /* 113: 'q' */
    {0x1E, 0x19, 0x19, 0x1E, 0x1C, 0x1A, 0x19, 0x19}, /* 114: 'r' */
    {0x0F, 0x18, 0x18, 0x0E, 0x03, 0x03, 0x03, 0x1E}, /* 115: 's' */
    {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}, /* 116: 't' */
    {0x1B, 0x1B, 0x1B, 0x1B, 0x1B, 0x1B, 0x1B, 0x0E}, /* 117: 'u' */
    {0x1B, 0x1B, 0x1B, 0x1B, 0x0E, 0x0E, 0x04, 0x04}, /* 118: 'v' */
    {0x63, 0x63, 0x63, 0x6B, 0x6B, 0x7F, 0x3E, 0x22}, /* 119: 'w' */
    {0x1B, 0x1B, 0x0E, 0x04, 0x04, 0x0E, 0x1B, 0x1B}, /* 120: 'x' */
    {0x1B, 0x1B, 0x1B, 0x0E, 0x04, 0x04, 0x04, 0x04}, /* 121: 'y' */
    {0x1F, 0x03, 0x06, 0x0C, 0x0C, 0x18, 0x18, 0x1F}, /* 122: 'z' */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* 123: '{' */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* 124: '|' */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* 125: '}' */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* 126: '~' */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* 127: '\x7f' */
};

static const uint8_t sArcadeWidths[96] = {
    3, /* 32 */
    2, /* 33 */
    4, /* 34 */
    4, /* 35 */
    4, /* 36 */
    5, /* 37 */
    4, /* 38 */
    4, /* 39 */
    4, /* 40 */
    4, /* 41 */
    4, /* 42 */
    4, /* 43 */
    4, /* 44 */
    5, /* 45 */
    2, /* 46 */
    5, /* 47 */
    5, /* 48 */
    5, /* 49 */
    5, /* 50 */
    5, /* 51 */
    5, /* 52 */
    5, /* 53 */
    5, /* 54 */
    5, /* 55 */
    5, /* 56 */
    5, /* 57 */
    2, /* 58 */
    4, /* 59 */
    4, /* 60 */
    4, /* 61 */
    4, /* 62 */
    5, /* 63 */
    4, /* 64 */
    5, /* 65 */
    5, /* 66 */
    5, /* 67 */
    5, /* 68 */
    5, /* 69 */
    5, /* 70 */
    5, /* 71 */
    5, /* 72 */
    3, /* 73 */
    5, /* 74 */
    5, /* 75 */
    5, /* 76 */
    7, /* 77 */
    6, /* 78 */
    5, /* 79 */
    5, /* 80 */
    6, /* 81 */
    5, /* 82 */
    5, /* 83 */
    5, /* 84 */
    5, /* 85 */
    5, /* 86 */
    7, /* 87 */
    5, /* 88 */
    5, /* 89 */
    5, /* 90 */
    4, /* 91 */
    4, /* 92 */
    4, /* 93 */
    4, /* 94 */
    4, /* 95 */
    4, /* 96 */
    5, /* 97 */
    5, /* 98 */
    5, /* 99 */
    5, /* 100 */
    5, /* 101 */
    5, /* 102 */
    5, /* 103 */
    5, /* 104 */
    3, /* 105 */
    5, /* 106 */
    5, /* 107 */
    5, /* 108 */
    7, /* 109 */
    6, /* 110 */
    5, /* 111 */
    5, /* 112 */
    6, /* 113 */
    5, /* 114 */
    5, /* 115 */
    5, /* 116 */
    5, /* 117 */
    5, /* 118 */
    7, /* 119 */
    5, /* 120 */
    5, /* 121 */
    5, /* 122 */
    4, /* 123 */
    4, /* 124 */
    4, /* 125 */
    4, /* 126 */
    4, /* 127 */
};

static int BArcadeCharWidth(char c)
{
    unsigned char uc = (unsigned char)c;
    if (uc >= 'a' && uc <= 'z') uc -= ('a' - 'A');
    if (uc >= 32 && uc < 128)
        return sArcadeWidths[uc - 32];
    return 4;
}

static int BArcadeTextWidth(const char *s, int scale)
{
    int w = 0;
    while (*s) {
        if ((unsigned char)s[0] == 0xC3 && (unsigned char)s[1] == 0xA9) {
            w += (sArcadeWidths['E' - 32] + 1) * scale;
            s += 2;
            continue;
        }
        w += (BArcadeCharWidth(*s) + 1) * scale;
        ++s;
    }
    return w > 0 ? w - scale : 0;
}

static void BArcadeDrawChar(char c, int x, int y, uint16_t color, int scale, int limit, int isAccent)
{
    unsigned char uc = (unsigned char)c;
    if (uc >= 'a' && uc <= 'z') uc -= ('a' - 'A');
    if (uc < 32 || uc >= 128) uc = '?';
    const uint8_t *rows = sArcadeGlyphs[uc - 32];
    int w = sArcadeWidths[uc - 32];

    for (int r = 0; r < 8; ++r) {
        uint8_t row_bits = rows[r];
        int py = y + r * scale;
        if (py < 0 || py >= 240) continue;
        for (int b = 0; b < w; ++b) {
            int px = x + b * scale;
            if (px < 0 || px + scale > limit || px >= 320) continue;
            if (row_bits & (1u << (w - 1 - b))) {
                if (scale == 1)
                    Px(px, py, color);
                else
                    Fill(px, py, scale, scale, color);
            }
        }
    }
    if (isAccent) {
        /* Acute accent mark for Ã© (é/É) */
        int ax = x + (w - 2) * scale, ay = y - 2 * scale;
        if (scale == 1) {
            if (ax >= 0 && ax < limit && ay >= 0 && ay < 240) Px(ax, ay, color);
            if (ax + 1 >= 0 && ax + 1 < limit && ay - 1 >= 0 && ay - 1 < 240) Px(ax + 1, ay - 1, color);
        } else {
            if (ax >= 0 && ax + scale <= limit && ay >= 0 && ay + scale <= 240) Fill(ax, ay, scale, scale, color);
            if (ax + scale >= 0 && ax + 2 * scale <= limit && ay - scale >= 0 && ay <= 240) Fill(ax + scale, ay - scale, scale, scale, color);
        }
    }
}

static void BArcadeDrawText(const char *s, int x, int y, uint16_t fg, uint16_t shadow, int scale, int limit, int dropShadow)
{
    if (dropShadow > 0) {
        for (int d = dropShadow; d >= 1; --d) {
            int cx = x + d;
            int cy = y + d;
            const char *p = s;
            while (*p) {
                if ((unsigned char)p[0] == 0xC3 && (unsigned char)p[1] == 0xA9) {
                    BArcadeDrawChar('E', cx, cy, shadow, scale, limit, 1);
                    cx += (sArcadeWidths['E' - 32] + 1) * scale;
                    p += 2;
                    continue;
                }
                char ch = *p++;
                BArcadeDrawChar(ch, cx, cy, shadow, scale, limit, 0);
                cx += (BArcadeCharWidth(ch) + 1) * scale;
            }
        }
    }
    {
        int cx = x;
        int cy = y;
        const char *p = s;
        while (*p) {
            if ((unsigned char)p[0] == 0xC3 && (unsigned char)p[1] == 0xA9) {
                BArcadeDrawChar('E', cx, cy, fg, scale, limit, 1);
                cx += (sArcadeWidths['E' - 32] + 1) * scale;
                p += 2;
                continue;
            }
            char ch = *p++;
            BArcadeDrawChar(ch, cx, cy, fg, scale, limit, 0);
            cx += (BArcadeCharWidth(ch) + 1) * scale;
        }
    }
}

static void BHudDrawMoveTitle(const char *title, int x, int y, int limit)
{
    static const int8_t dxs[8] = {-1,  0,  1, -1, 1, -1, 0, 1};
    static const int8_t dys[8] = {-1, -1, -1,  0, 0,  1, 1, 1};

    for (int d = 0; d < 8; ++d) {
        int cx = x + dxs[d];
        int cy = y + dys[d];
        const char *p = title;
        while (*p) {
            char ch = *p++;
            BArcadeDrawChar(ch, cx, cy, UIS_OUTLINE, 1, limit, 0);
            cx += BArcadeCharWidth(ch) + 1;
        }
    }
    {
        int cx = x;
        int cy = y;
        const char *p = title;
        while (*p) {
            char ch = *p++;
            BArcadeDrawChar(ch, cx, cy, UI_TEXT, 1, limit, 0);
            cx += BArcadeCharWidth(ch) + 1;
        }
    }
}


static const CtrPokeTouchRect sBStrip = {3, 3, 314, 31};
static const CtrPokeTouchRect sBBody = {3, 34, 314, 203};
#include "battle_move_skin.h"
#include "battle_action_skin.h"

static const CtrPokeTouchRect sBMStrip = {MSK_STRIP_X0, MSK_STRIP_Y0, MSK_STRIP_X1 - MSK_STRIP_X0, MSK_STRIP_Y1 - MSK_STRIP_Y0};
static const CtrPokeTouchRect sBMFooter = {MSK_FOOT_X0, MSK_FOOT_Y0, MSK_FOOT_X1 - MSK_FOOT_X0, MSK_FOOT_Y1 - MSK_FOOT_Y0};
static const CtrPokeTouchRect sBMCancel = {MSK_CANCEL_X, MSK_CANCEL_Y, MSK_CANCEL_W, MSK_CANCEL_H};

/* The area a card answers to: its frame, inside the cell its picture fills. */
static CtrPokeTouchRect BMCardRect(int i)
{
    CtrPokeTouchRect r = {(int16_t)(kMskCardPos[i][0] + 3), (int16_t)(kMskCardPos[i][1] + 3), MSK_CARD_W - 6, MSK_CARD_H - 6};

    return r;
}

static CtrPokeTouchRect BMCellRect(int i)
{
    CtrPokeTouchRect r = {kMskCardPos[i][0], kMskCardPos[i][1], MSK_CARD_W, MSK_CARD_H};

    return r;
}

/* An action card's area: the card's picture without the glow round it. */
static CtrPokeTouchRect ABCardRect(int i)
{
    CtrPokeTouchRect r = {(int16_t)(kAbCardPos[i][0] + 3), (int16_t)(kAbCardPos[i][1] + 3), AB_CARD_W - 6, AB_CARD_H - 6};

    return r;
}

static const CtrPokeTouchRect sBCard[4] = {{6, 40, 151, 68}, {163, 40, 151, 68}, {6, 114, 151, 69}, {163, 114, 151, 69}};
static const CtrPokeTouchRect sBCancel = {6, 190, 308, 30};
/* The Safari Zone's four actions in the controller's 2x2 order (BALL, BAIT, ROCK, RUN). */
static const CtrPokeTouchRect sBSafari[4] = {{8, 42, 150, 90}, {162, 42, 150, 90}, {8, 138, 150, 90}, {162, 138, 150, 90}};
/* The battle Bag: three tabs (the Bag's real pockets), the item list on the left, the detail on the right, CANCEL. */
static const CtrPokeTouchRect sBTab[3] = {{8, 36, 100, 26}, {111, 36, 100, 26}, {214, 36, 98, 26}};
static const int sBTabPocket[3] = {0, 2, 1};            /* the tabs show Items, Poke Balls, Key Items: FireRed's pockets 0, 2, 1 */
static const char *const sBTabLabel[3] = {"ITEMS", "BALLS", "KEY ITEMS"};
static const CtrPokeTouchRect sBList = {5, 66, 172, 134};
static const CtrPokeTouchRect sBDetail = {181, 66, 133, 134};
static const CtrPokeTouchRect sBBagCancel = {5, 204, 309, 27};
static const CtrPokeTouchRect sBUpArrow = {155, 68, 20, 14}, sBDownArrow = {155, 184, 20, 14};

static CtrPokeTouchRect BRow(int i)
{
    CtrPokeTouchRect r = {9, (int16_t)(70 + 25 * i), 150, 24};
    return r;
}

static CtrPokeTouchRect BMenuRow(int i)
{
    CtrPokeTouchRect r = {187, (int16_t)(104 + 30 * i), 121, 26};
    return r;
}
/* The Party (in battle and on the field): the six Pokemon as raised cards in two columns of three on the embers, and
 * under them the note card with what the Party says and CANCEL. */
static const CtrPokeTouchRect sBPNote = {4, 176, 236, 60};
static const CtrPokeTouchRect sBPCancel = {244, 180, 72, 52};
#define BP_DEPTH 3

static CtrPokeTouchRect BPanel(int i)
{
    CtrPokeTouchRect r = {(int16_t)(i & 1 ? 163 : 6), (int16_t)(6 + 56 * (i >> 1)), 151, 50};
    return r;
}

/* A row of the Party's menu: one column of up to four, two columns beyond. */
static CtrPokeTouchRect BPMenuRow(int count, int i)
{
    int cols = count > 4 ? 2 : 1, rows = (count + cols - 1) / cols, col = cols == 2 ? i / rows : 0, row = cols == 2 ? i % rows : i;
    CtrPokeTouchRect r = {(int16_t)(cols == 2 ? 44 + 118 * col : 100), (int16_t)(88 - 15 * rows + 30 * row),
                          (int16_t)(cols == 2 ? 112 : 120), 27};
    return r;
}

static int BIn(const CtrPokeTouchRect *r, int x, int y)
{
    return UiIn(r, x, y);
}

int CtrBattleUi_TabPocket(int tab)
{
    return tab >= 0 && tab < 3 ? sBTabPocket[tab] : -1;
}

int CtrBattleUi_Hit(const CtrBattleView *v, int x, int y)
{
    if (v->state == BUI_ACTION)
        for (int i = 0; i < 4; ++i) {
            CtrPokeTouchRect r = ABCardRect(i);

            if (BIn(&r, x, y))
                return i;
        }
    if (v->state == BUI_MOVE) {
        for (int i = 0; i < 4; ++i)
            if (v->move[i].present) {
                CtrPokeTouchRect r = BMCardRect(i);

                if (BIn(&r, x, y))
                    return i;
            }
        if (BIn(&sBMCancel, x, y))
            return BUI_CANCEL;
    }
    if (v->state == BUI_TARGET) {
        for (int i = 0; i < 4; ++i)
            if (v->target[i].present && BIn(&sBCard[i], x, y))
                return i;
        if (BIn(&sBCancel, x, y))
            return BUI_CANCEL;
        return -1;
    }
    if (v->state == BUI_SAFARI) {
        for (int i = 0; i < 4; ++i)
            if (BIn(&sBSafari[i], x, y))
                return i;
        return -1;
    }
    if (v->state == BUI_PARTY) {
        const CtrBattleParty *p = &v->pty;

        if (p->panel == BUI_PTY_TEXT)
            return BUI_PTY_MESSAGE;                         /* anywhere: the message waits for A */
        if (p->panel == BUI_PTY_ACTIONS || p->panel == BUI_PTY_YESNO) {
            int n = p->menuCount > 8 ? 8 : p->menuCount;

            for (int i = 0; i < n; ++i) {
                CtrPokeTouchRect r = BPMenuRow(n, i);
                if (BIn(&r, x, y))
                    return BUI_PTY_MENU + i;
            }
            return -1;
        }
        if (p->mode != BUI_PTY_FORCED && BIn(&sBPCancel, x, y))
            return BUI_PTY_CANCEL;
        for (int i = 0; i < 6; ++i) {
            CtrPokeTouchRect r = BPanel(i);
            if (p->present[i] && BIn(&r, x, y))
                return BUI_PTY_SLOT + i;
        }
        return -1;
    }
    if (v->state == BUI_BAG) {
        const CtrBattleBag *b = &v->bag;

        if (BIn(&sBBagCancel, x, y))
            return BUI_BAG_CANCEL;
        for (int i = 0; i < 3; ++i)
            if (BIn(&sBTab[i], x, y))
                return BUI_BAG_TAB + i;
        if (b->message[0] && BIn(&sBDetail, x, y))
            return BUI_BAG_MESSAGE;
        if (b->menuCount > 0 && BIn(&sBDetail, x, y)) {
            for (int i = 0; i < b->menuCount && i < 3; ++i) {
                CtrPokeTouchRect r = BMenuRow(i);
                if (BIn(&r, x, y))
                    return BUI_BAG_MENU + i;
            }
            return -1;
        }
        if (b->count > BUI_BAG_ROWS && BIn(&sBUpArrow, x, y) && b->first > 0)
            return BUI_BAG_UP;
        if (b->count > BUI_BAG_ROWS && BIn(&sBDownArrow, x, y) && b->first + BUI_BAG_ROWS < b->count)
            return BUI_BAG_DOWN;
        for (int i = 0; i < BUI_BAG_ROWS; ++i) {
            CtrPokeTouchRect r = BRow(i);
            if (b->row[i].item && BIn(&r, x, y))
                return BUI_BAG_ROW + i;
        }
    }
    return -1;
}

/* A Poke Ball of the party strip: red and white for a healthy Pokemon, amber-gold for one with a status condition
 * (kind == 3), grey for one that fainted (kind == 2), an empty socket for no Pokemon (kind == 0). */
static void BallDraw(int cx, int cy, int kind)
{
    const uint16_t sTop[4] = {UIS_PANEL_LO, UI_RGB(226, 62, 52), UI_RGB(120, 128, 146), UI_RGB(236, 174, 36)};
    int k = kind >= 0 && kind <= 3 ? kind : 0;

    for (int dy = -10; dy < 10; ++dy)
        for (int dx = -10; dx < 10; ++dx) {
            int d2 = (2 * dx + 1) * (2 * dx + 1) + (2 * dy + 1) * (2 * dy + 1);    /* in half pixels, radius 10 */
            uint16_t c;

            if (d2 > 20 * 20)
                continue;
            if (k == 0)
                c = d2 > 17 * 17 ? UIS_BORDER : UIS_PANEL_LO;
            else if (d2 > 17 * 17 || (dy >= -1 && dy < 1))
                c = UIS_OUTLINE;
            else if (d2 < 6 * 6)
                c = d2 < 3 * 3 ? UI_TEXT : UIS_OUTLINE;
            else if (dy < 0)
                c = dx < -2 && dy < -4 && k == 1 ? UI_RGB(255, 140, 120)
                  : dx < -2 && dy < -4 && k == 3 ? UI_RGB(255, 228, 120) : sTop[k];
            else
                c = k == 2 ? UI_RGB(170, 176, 190) : UI_RGB(236, 240, 246);
            Px(cx + dx, cy + dy, c);
        }
}

/* Subtle circular watermark Pokéball, strictly clipped within `clip` */
static void BWatermarkBallIn(const CtrPokeTouchRect *clip, int cx, int cy, int rad, uint16_t color)
{
    int r2_out = rad * rad;
    int r2_in = (rad - 1) * (rad - 1);
    int r_c = rad / 4;
    int r2_c_out = (r_c + 1) * (r_c + 1);
    int r2_c_in = (r_c - 1) * (r_c - 1);
    int r2_dot = (r_c / 2) * (r_c / 2);

    for (int dy = -rad; dy <= rad; ++dy) {
        int py = cy + dy;
        if (clip && (py < clip->y || py >= clip->y + clip->h)) continue;
        if (py < 0 || py >= 240) continue;
        for (int dx = -rad; dx <= rad; ++dx) {
            int px = cx + dx;
            if (clip && (px < clip->x || px >= clip->x + clip->w)) continue;
            if (px < 0 || px >= 320) continue;
            int d2 = dx * dx + dy * dy;
            if (d2 <= r2_out && d2 >= r2_in) {
                Px(px, py, color);
            } else if (d2 <= r2_out && (dy == 0 || dy == -1) && (dx < -r_c || dx > r_c)) {
                Px(px, py, color);
            } else if (d2 <= r2_c_out && d2 >= r2_c_in) {
                Px(px, py, color);
            } else if (d2 <= r2_dot) {
                Px(px, py, color);
            }
        }
    }
}

/* Exactly six slots, one for each of the player's party places, on a dark strip. */
static void BStrip(const CtrBattleView *v)
{
    static const int xs[6] = {50, 94, 138, 182, 226, 270};

    /* The chrome panel of PokeTouch: a light line under the top edge, a dark one over the bottom. */
    UiPanel(&sBStrip, UIS_PANEL, UIS_BORDER);
    Fill(sBStrip.x + 3, sBStrip.y + 1, sBStrip.w - 6, 1, UIS_BORDER_LIT);
    Fill(sBStrip.x + 3, sBStrip.y + sBStrip.h - 2, sBStrip.w - 6, 1, UIS_PANEL_LO);

    /* Metallic corner rivets */
    Px(sBStrip.x + 3, sBStrip.y + 3, UIS_PANEL_LO); Px(sBStrip.x + 4, sBStrip.y + 4, UIS_BORDER_LIT);
    Px(sBStrip.x + sBStrip.w - 4, sBStrip.y + 3, UIS_PANEL_LO); Px(sBStrip.x + sBStrip.w - 5, sBStrip.y + 4, UIS_BORDER_LIT);
    Px(sBStrip.x + 3, sBStrip.y + sBStrip.h - 4, UIS_PANEL_LO); Px(sBStrip.x + 4, sBStrip.y + sBStrip.h - 5, UIS_BORDER_LIT);
    Px(sBStrip.x + sBStrip.w - 4, sBStrip.y + sBStrip.h - 4, UIS_PANEL_LO); Px(sBStrip.x + sBStrip.w - 5, sBStrip.y + sBStrip.h - 5, UIS_BORDER_LIT);

    /* Decorative watermark Pokéballs on the far left and right edges */
    BWatermarkBallIn(&sBStrip, sBStrip.x + 15, 18, 9, UI_RGB(68, 14, 18));
    BWatermarkBallIn(&sBStrip, sBStrip.x + sBStrip.w - 15, 18, 9, UI_RGB(68, 14, 18));

    for (int i = 0; i < 6; ++i) {
        if (v->party[i] && i == 0) {
            /* Active Pokémon golden aura ring */
            for (int dy = -13; dy <= 13; ++dy)
                for (int dx = -13; dx <= 13; ++dx) {
                    int d2 = dx * dx + dy * dy;
                    if (d2 >= 11 * 11 && d2 <= 13 * 13)
                        Px(xs[i] + dx, 18 + dy, d2 <= 12 * 12 ? UI_RGB(255, 230, 90) : UI_RGB(200, 140, 20));
                }
        }
        BallDraw(xs[i], 18, v->party[i]);
        if (v->party[i] == 3) {
            /* Yellow [PAR] status condition pill badge */
            int px = xs[i] + 1, py = 18 + 2;
            CtrPokeTouchRect pill = {(int16_t)px, (int16_t)py, 20, 8};
            UiRoundFill(pill.x, pill.y, pill.w, pill.h, 2, UI_RGB(240, 180, 20));
            UiRoundRect(&pill, 2, UI_RGB(255, 235, 90));
            UiText("PAR", px + 2, py + 1, UI_RGB(60, 35, 0), 0, 1, px + 20);
        }
    }
}

/* Golden glowing corner brackets and inner amber contour for focused card/button. */
static void BFocusGlowBrackets(const CtrPokeTouchRect *r)
{
    uint16_t gold = UI_RGB(255, 224, 80);
    uint16_t bright = UI_RGB(255, 255, 210);
    uint16_t halo = UI_RGB(200, 140, 20);
    int arm = 7;
    CtrPokeTouchRect rim = UiInset(r, 1);

    UiRing(&rim, UI_RADIUS_BUTTON - 1, 1, halo);

    /* Top-Left Bracket */
    Fill(r->x + 1, r->y + 1, arm, 2, gold);
    Fill(r->x + 1, r->y + 1, 2, arm, gold);
    Px(r->x + 1, r->y + 1, bright);
    Px(r->x + 2, r->y + 2, bright);

    /* Top-Right Bracket */
    Fill(r->x + r->w - 1 - arm, r->y + 1, arm, 2, gold);
    Fill(r->x + r->w - 3, r->y + 1, 2, arm, gold);
    Px(r->x + r->w - 2, r->y + 1, bright);
    Px(r->x + r->w - 3, r->y + 2, bright);

    /* Bottom-Left Bracket */
    Fill(r->x + 1, r->y + r->h - 3, arm, 2, gold);
    Fill(r->x + 1, r->y + r->h - 1 - arm, 2, arm, gold);
    Px(r->x + 1, r->y + r->h - 2, bright);
    Px(r->x + 2, r->y + r->h - 3, bright);

    /* Bottom-Right Bracket */
    Fill(r->x + r->w - 1 - arm, r->y + r->h - 3, arm, 2, gold);
    Fill(r->x + r->w - 3, r->y + r->h - 1 - arm, 2, arm, gold);
    Px(r->x + r->w - 2, r->y + r->h - 2, bright);
    Px(r->x + r->w - 3, r->y + r->h - 3, bright);
}

/* 8x8 silhouettes for FireRed's 18 types (its type numbers 0-17: Normal, Fighting, Flying, Poison, Ground, Rock, Bug,
 * Ghost, Steel, ???, Fire, Water, Grass, Electric, Psychic, Ice, Dragon, Dark), one byte a row, the high bit the left
 * pixel. Hand-drawn for this port. */
static void BTypeIconN(int x, int y, uint8_t type, int size, uint16_t ink, uint16_t ink2)
{
    static const uint8_t sIcon[18][8] = {
        {0x3C, 0x7E, 0xC3, 0xC3, 0xC3, 0xC3, 0x7E, 0x3C},   /* Normal: a ring */
        {0x78, 0x7C, 0x7F, 0x7F, 0x7E, 0x3E, 0x3C, 0x3C},   /* Fighting: a fist */
        {0x03, 0x0F, 0x3F, 0x7F, 0xFF, 0x66, 0x24, 0x00},   /* Flying: a wing */
        {0x38, 0x7C, 0x6C, 0x7C, 0x38, 0x28, 0x10, 0x00},   /* Poison: a skull */
        {0x00, 0x08, 0x1C, 0x3E, 0x7F, 0xFF, 0xFF, 0xFF},   /* Ground: a mountain */
        {0x00, 0x3C, 0x7E, 0xFF, 0xFF, 0xFF, 0x7E, 0x3C},   /* Rock: a boulder */
        {0x99, 0x5A, 0x3C, 0x7E, 0x7E, 0x3C, 0x5A, 0x99},   /* Bug: a beetle */
        {0x3C, 0x7E, 0xDB, 0xFF, 0xFF, 0xFF, 0xFF, 0xAA},   /* Ghost: a sheet with eyes */
        {0x18, 0x5A, 0x3C, 0xE7, 0xE7, 0x3C, 0x5A, 0x18},   /* Steel: a cog */
        {0x3C, 0x66, 0x06, 0x0C, 0x18, 0x00, 0x18, 0x00},   /* ???: a question mark */
        {0x10, 0x18, 0x38, 0x3C, 0x7E, 0x7F, 0x7F, 0x3E},   /* Fire: a flame */
        {0x10, 0x10, 0x38, 0x38, 0x7C, 0x7C, 0x7C, 0x38},   /* Water: a drop */
        {0x03, 0x0F, 0x3F, 0x7F, 0x7E, 0xBC, 0x30, 0x40},   /* Grass: a leaf */
        {0x0C, 0x18, 0x30, 0x7E, 0x0C, 0x18, 0x30, 0x20},   /* Electric: a bolt */
        {0x3C, 0x42, 0x99, 0xA5, 0xA5, 0x99, 0x42, 0x3C},   /* Psychic: a spiral */
        {0x10, 0x92, 0x54, 0x38, 0xFE, 0x38, 0x54, 0x92},   /* Ice: a snowflake */
        {0x40, 0x60, 0x70, 0x78, 0x7C, 0x3C, 0x18, 0x08},   /* Dragon: a fang */
        {0x3C, 0x78, 0xF0, 0xF0, 0xF0, 0xF0, 0x78, 0x3C},   /* Dark: a crescent */
    };

    static const uint16_t sIcon12[18][12] = {
        {0x1F8, 0x30C, 0x606, 0x402, 0xC03, 0xC03, 0xC03, 0xC03, 0x402, 0x606, 0x30C, 0x1F8},
        {0x36C, 0x7FC, 0x7FE, 0x7FE, 0x7FE, 0x7FE, 0x3FC, 0x3FC, 0x1FC, 0x1F8, 0x1F8, 0x1F8},
        {0x003, 0x00F, 0x03F, 0x0FF, 0x3FE, 0x7FC, 0xFF8, 0xFC0, 0xF00, 0xE00, 0xC00, 0x800},
        {0x1F8, 0x3FC, 0x7FE, 0x7FE, 0x666, 0x666, 0x7FE, 0x198, 0x1F8, 0x168, 0x1F8, 0x000},
        {0x000, 0x000, 0x060, 0x0F2, 0x1FB, 0x3FE, 0x7FF, 0x7FF, 0xFFF, 0xFFF, 0xFFF, 0xFFF},
        {0x000, 0x1F8, 0x3FC, 0x7FE, 0x7FF, 0xFFF, 0xFFF, 0xFFF, 0x7FF, 0x7FE, 0x3FC, 0x1F8},
        {0x204, 0x108, 0x0F0, 0x1F8, 0x3FC, 0xBFD, 0x7FE, 0xBFD, 0x3FC, 0x5FA, 0x8F1, 0x000},
        {0x1F8, 0x3FC, 0x7FE, 0x666, 0x666, 0x7FE, 0x7FE, 0x7FE, 0x7FE, 0x6F6, 0x462, 0x000},
        {0x060, 0x2F4, 0x3FC, 0x79E, 0xF0F, 0xF0F, 0xF0F, 0x79E, 0x3FC, 0x2F4, 0x060, 0x000},
        {0x1F8, 0x3FC, 0x70E, 0x70E, 0x00E, 0x01C, 0x038, 0x038, 0x000, 0x038, 0x038, 0x000},
        {0x040, 0x060, 0x0E4, 0x1EC, 0x3EE, 0x3FE, 0x7FF, 0x7FF, 0x7DF, 0x38E, 0x1FC, 0x0F8},
        {0x040, 0x040, 0x0E0, 0x0E0, 0x1F0, 0x3F8, 0x3F8, 0x7FC, 0x7FC, 0x7FC, 0x3F8, 0x1F0},
        {0x00F, 0x03F, 0x0FF, 0x1FF, 0x3FF, 0x3FE, 0x7FC, 0x7F8, 0xEF0, 0xD80, 0xA00, 0x800},
        {0x03C, 0x078, 0x0F0, 0x1E0, 0x3FE, 0x3FE, 0x01C, 0x038, 0x070, 0x060, 0x0C0, 0x080},
        {0x000, 0x1F8, 0x204, 0x4F2, 0x50A, 0x56A, 0x56A, 0x50A, 0x4F2, 0x204, 0x1F8, 0x000},
        {0x060, 0x264, 0x168, 0x0F0, 0x861, 0xFFF, 0xFFF, 0x861, 0x0F0, 0x168, 0x264, 0x060},
        {0xC03, 0xE07, 0xF0F, 0xF9F, 0x7FE, 0x3FC, 0x1F8, 0x0F0, 0x090, 0x198, 0x108, 0x000},
        {0x1F0, 0x3C0, 0x780, 0x780, 0xF00, 0xF00, 0xF00, 0xF08, 0x79C, 0x7FE, 0x3FC, 0x1F8}
    };

    if (type > 17)
        return;
    if (size == 12) {
        for (int r = 0; r < 12; ++r)
            for (int c = 0; c < 12; ++c)
                if (sIcon12[type][r] & (0x800u >> c))
                    Px(x + c, y + r, r < 5 ? ink : ink2);
        return;
    }
    for (int r = 0; r < size; ++r)
        for (int c = 0; c < size; ++c)
            if (sIcon[type][r * 8 / size] & (0x80u >> (c * 8 / size)))
                Px(x + c, y + r, r * 2 < size ? ink : ink2);      /* lighter on top, the second tone below */
}

static void BTypeIcon(int x, int y, uint8_t type, uint16_t ink)
{
    BTypeIconN(x, y, type, 8, ink, ink);
}

/* PokeTouch's side buttons carry a coloured tick to tell them apart on the one red; the battle's wear it on their top
 * edge, centred (the left edge is where the longest label, POKeMON, reaches): FIGHT amber, BAG green, POKeMON blue,
 * RUN yellow. */
static void BAccent(const CtrPokeTouchRect *r, int dy, uint16_t color)
{
    int x = r->x + (r->w - 28) / 2;

    UiRoundFill(x, r->y + 6 + dy, 28, 6, 2, UIS_OUTLINE);
    UiRoundFill(x + 1, r->y + 7 + dy, 26, 4, 1, color);
    Fill(x + 3, r->y + 7 + dy, 22, 1, UiMix(color, UI_TEXT, 3));
}

/* Whether button or card `i` looks different between two views of the same screen: pressed, or focused. */
static int BLooksChanged(const CtrBattleView *a, const CtrBattleView *b, int i)
{
    return (a->pressed == i) != (b->pressed == i)
        || (a->showCursor && a->cursor == i) != (b->showCursor && b->cursor == i);
}

/* Appends `word` to `line` (a space first when it is not empty), within `size`. */
static void BAppend(char *line, size_t size, const char *word)
{
    size_t n = strlen(line);

    if (n && n + 1 < size)
        line[n++] = ' ';
    for (; *word && n + 1 < size; ++word)
        line[n++] = *word;
    line[n] = 0;
}

/* Running text wrapped to `width` pixels, `maxLines` at most (FireRed's line breaks are spaces here). */
static void BDescribeN(const char *text, int x, int y, int width, int maxLines, uint16_t color)
{
    char word[24], line[80], joined[80];
    int lines = 0;
    const char *p = text;

    line[0] = 0;
    while (*p && lines < maxLines) {
        int w = 0;

        while (*p == ' ' || *p == '\n')
            ++p;
        while (*p && *p != ' ' && *p != '\n' && w < (int)sizeof(word) - 1)
            word[w++] = *p++;
        word[w] = 0;
        if (!w)
            break;
        memcpy(joined, line, sizeof(joined));
        BAppend(joined, sizeof(joined), word);
        if (line[0] && UiTextWidth(joined, 1) > width) {
            UiText(line, x, y + lines * UI_LINE_H, color, UIS_TEXT_SH, 1, x + width);
            line[0] = 0;
            if (++lines >= maxLines)
                return;
            BAppend(line, sizeof(line), word);
        } else
            memcpy(line, joined, sizeof(line));
    }
    if (line[0] && lines < maxLines)
        UiText(line, x, y + lines * UI_LINE_H, color, UIS_TEXT_SH, 1, x + width);
}

/* The same running text in the small font: `maxLines` of 8 pixels, wrapped to `width`. */
static void BDescribeSmall(const char *text, int x, int y, int width, int maxLines, uint16_t color)
{
    char word[24], line[96], joined[96];
    int lines = 0;
    const char *p = text;

    line[0] = 0;
    while (*p && lines < maxLines) {
        int w = 0;

        while (*p == ' ' || *p == '\n')
            ++p;
        while (*p && *p != ' ' && *p != '\n' && w < (int)sizeof(word) - 1)
            word[w++] = *p++;
        word[w] = 0;
        if (!w)
            break;
        memcpy(joined, line, sizeof(joined));
        BAppend(joined, sizeof(joined), word);
        if (line[0] && UiTextWidthSmall(joined) > width) {
            UiTextSmall(line, x, y + lines * 8, color, UIS_TEXT_SH, x + width);
            line[0] = 0;
            if (++lines >= maxLines)
                return;
            BAppend(line, sizeof(line), word);
        } else
            memcpy(line, joined, sizeof(line));
    }
    if (line[0] && lines < maxLines)
        UiTextSmall(line, x, y + lines * 8, color, UIS_TEXT_SH, x + width);
}

/* Deep saturated elemental background tone for the card body */
static uint16_t BCardBodyColor(int type)
{
    switch (type) {
    case 10: return UI_RGB(58, 8, 14);   /* Fire: deep saturated crimson */
    case 4:  return UI_RGB(50, 34, 14);  /* Ground: deep earthy gold-brown */
    case 5:  return UI_RGB(50, 42, 18);  /* Rock: deep mineral bronze */
    case 11: return UI_RGB(12, 30, 68);  /* Water: deep ocean indigo */
    case 12: return UI_RGB(14, 48, 18);  /* Grass: deep jungle green */
    case 13: return UI_RGB(54, 46, 10);  /* Electric: deep amber gold */
    case 14: return UI_RGB(54, 12, 38);  /* Psychic: deep magenta */
    case 15: return UI_RGB(16, 48, 58);  /* Ice: deep glacier cyan */
    case 16: return UI_RGB(34, 16, 68);  /* Dragon: deep royal violet */
    case 17: return UI_RGB(28, 24, 28);  /* Dark: deep slate charcoal */
    default: return UI_RGB(36, 32, 40);  /* Normal/other: dark neutral slate */
    }
}

/* The PP colour FireRed's move list uses: the usual ink, then yellow at half, orange at a quarter, red at none. */
static uint16_t BPpColor(unsigned pp, unsigned max)
{
    return pp == 0 ? UI_HP_LOW : pp * 4 <= max ? UI_WARN : pp * 2 <= max ? UI_HP_MID : UI_TEXT;
}

/* A card of the battle (a move, a target): raised, its header band in `color` with `title`, the body a dark tint of
 * the same colour. Returns the vertical offset of its content (1 while pressed). An empty slot is a dark socket. */
static int BCardFrame(const CtrPokeTouchRect *r, uint16_t color, int type, const char *title, int state)
{
    int pressed = (state & UI_PRESSED) != 0, dy = pressed ? 1 : 0;
    uint16_t head = pressed ? UiMix(color, UIS_OUTLINE, 2) : color;
    int bottom = r->h - 2 - (pressed ? 1 : UI_DEPTH);

    UiRoundRect(r, UI_RADIUS_BUTTON, UIS_OUTLINE);
    if (!title) {
        UiRoundFill(r->x + 1, r->y + 1, r->w - 2, r->h - 2, UI_RADIUS_BUTTON - 1, UIS_PANEL_LO);
        UiTextCentered("-", r, -1, UI_TEXT_OFF, UIS_TEXT_SH, 1);
        return 0;
    }

    /* Card body with deep saturated elemental tone */
    {
        int bodyY = r->y + 21 + dy;
        int bodyH = (r->y + 1 + dy + bottom) - bodyY;
        if (bodyH > 0) {
            uint16_t base = type >= 0 ? BCardBodyColor(type) : UIS_PANEL;
            uint16_t c_top = UiMix(base, color, pressed ? 2 : 3);
            uint16_t c_mid = base;
            uint16_t c_bot = UiMix(base, UI_RGB(6, 4, 8), 2);
            int seam = r->y + 45 + dy - bodyY;     /* between the badge row (to y+42) and the PP row (from y+48) */

            if (seam < 1 || seam > bodyH - 3)
                seam = bodyH * 2 / 3;
            UiRoundBand(r->x + 1, bodyY, r->w - 2, bodyH, UI_RADIUS_BUTTON - 1, 0, 1, c_bot);
            Fill(r->x + 1, bodyY, r->w - 2, seam, c_top);
            Fill(r->x + 1, bodyY + seam, r->w - 2, bodyH - seam - 2, c_mid);
            Fill(r->x + 1, bodyY, 1, bodyH - 2, UiMix(c_mid, UI_TEXT, 2));
            Fill(r->x + 1, bodyY, r->w - 2, 1, UIS_OUTLINE);
        }
    }

    /* Glossy Header Band */
    UiRoundBand(r->x + 1, r->y + 1 + dy, r->w - 2, 21, UI_RADIUS_BUTTON - 1, 1, 0, head);
    Fill(r->x + 3, r->y + 1 + dy, r->w - 6, 1, UiMix(head, UI_TEXT, 5));
    Fill(r->x + 2, r->y + 2 + dy, r->w - 4, 1, UiMix(head, UI_TEXT, 3));
    Fill(r->x + 2, r->y + 3 + dy, r->w - 4, 5, UiMix(head, UI_TEXT, 2));
    Fill(r->x + 1, r->y + 19 + dy, r->w - 2, 1, UiMix(head, UIS_OUTLINE, 3));
    Fill(r->x + 1, r->y + 20 + dy, r->w - 2, 1, UIS_OUTLINE);

    /* Diagonal 45° pinstripes across the header band */
    for (int hy = 2; hy < 20; ++hy) {
        int py = r->y + hy + dy;
        uint16_t stripe = UiMix(head, UI_RGB(255, 255, 255), 2);
        for (int hx = 2; hx < r->w - 2; ++hx) {
            if ((hx + hy) % 7 < 2)
                Px(r->x + hx, py, stripe);
        }
    }

    /* Left mini emblem box for elemental icon */
    if (type >= 0) {
        int ibX = r->x + 5, ibY = r->y + 4 + dy;
        CtrPokeTouchRect ibox = {(int16_t)ibX, (int16_t)ibY, 14, 13};
        UiRoundFill(ibox.x, ibox.y, ibox.w, ibox.h, 2, UI_RGB(20, 6, 8));
        UiRoundRect(&ibox, 2, UI_RGB(64, 20, 24));
        BTypeIcon(ibox.x + 3, ibox.y + 2, (uint8_t)type, UI_TEXT);
    }

    /* The title in white with a solid 1px black outline, in the arcade lettering. */
    {
        int tx = type >= 0 ? r->x + 23 : r->x + 8;
        int limit = r->x + r->w - 18;

        BHudDrawMoveTitle(title, tx, r->y + 7 + dy, limit);
    }

    if (state & UI_FOCUSED)
        BFocusGlowBrackets(r);

    return dy;
}

/* The matchup tag beside the type badge: what the move does to the foe's types. x2 green, x4 lime with a gold rim,
 * x1/2 and x1/4 orange, x0 red; an arrow or a cross says it again without colour. A neutral hit has no tag. 40x14: the
 * 5-pixel glyph at +4, the text to end 4 from the right edge (x1/2 is the widest, 24 wide, so they never meet). */
#define BTAG_W 40
static void BMatchupTag(uint8_t matchup, int x, int y)
{
    static const char *const kText[] = {"", "x0", "x1/4", "x1/2", "x2", "x4"};
    static const uint16_t kColor[] = {0, UI_RGB(190, 40, 40), UI_RGB(168, 62, 26), UI_RGB(204, 84, 24),
                                      UI_RGB(40, 150, 70), UI_RGB(110, 245, 30)};
    static const uint8_t kUp[5] = {0x04, 0x0E, 0x1F, 0x0E, 0x0E};          /* x2, x4 */
    static const uint8_t kDown[5] = {0x0E, 0x0E, 0x1F, 0x0E, 0x04};        /* x1/2, x1/4 */
    static const uint8_t kCross[5] = {0x11, 0x0A, 0x04, 0x0A, 0x11};       /* x0 */
    CtrPokeTouchRect tag = {(int16_t)x, (int16_t)y, BTAG_W, 14};
    const uint8_t *glyph;
    uint16_t ink;

    if (matchup == CTR_FX_NONE || matchup > CTR_FX_QUADRUPLE)
        return;
    ink = matchup == CTR_FX_QUADRUPLE ? UI_RGB(15, 35, 5) : UI_TEXT;
    UiRoundFill(tag.x, tag.y, tag.w, tag.h, 3, matchup == CTR_FX_QUADRUPLE ? UI_RGB(255, 224, 102) : UIS_OUTLINE);
    UiRoundFill(tag.x + 1, tag.y + 1, tag.w - 2, tag.h - 2, 2, kColor[matchup]);
    Fill(tag.x + 2, tag.y + 1, tag.w - 4, 1, UiMix(kColor[matchup], UI_TEXT, 3));

    glyph = matchup == CTR_FX_IMMUNE ? kCross : (matchup == CTR_FX_HALF || matchup == CTR_FX_QUARTER) ? kDown : kUp;
    for (int r = 0; r < 5; ++r)
        for (int c = 0; c < 5; ++c)
            if (glyph[r] & (1u << (4 - c)))
                Px(tag.x + 4 + c, tag.y + 4 + r, ink);
    UiTextRight(kText[matchup], tag.x + tag.w - 4, tag.y + (tag.h - UI_TEXT_H) / 2 + 1, ink,
                matchup == CTR_FX_QUADRUPLE ? 0 : UIS_TEXT_SH, 1);
}

/* Sculpted CANCEL button with an inner metallic bevel ring. */
static void BCancel(const CtrPokeTouchRect *r, int pressed)
{
    int state = pressed ? UI_PRESSED : 0;
    int dy = pressed ? 1 : 0;
    CtrPokeTouchRect rim = UiInset(r, 1);

    UiButton(r, &sUiS->cancel, state);
    if (r->h >= 36) {
        BWatermarkBallIn(r, r->x + r->w / 2, r->y + r->h / 2 + dy, 14,
                         pressed ? UI_RGB(110, 12, 14) : UI_RGB(155, 18, 20));
    }
    int scale = r->h >= 28 ? 2 : 1;
    int tw = BArcadeTextWidth("CANCEL", scale);
    int tx = r->x + (r->w - tw) / 2;
    int ty = r->y + (r->h - 8 * scale) / 2 + dy;
    BArcadeDrawText("CANCEL", tx, ty, UI_TEXT, UIS_OUTLINE, scale, r->x + r->w - 2, scale);
    UiRing(&rim, UI_RADIUS_BUTTON - 1, 1, pressed ? sUiS->cancel.body : UIS_BORDER_LIT);
}

/* Capitals, digits and a few marks on a 5x7 grid (the move screen's counts and labels). */
static const char kBMChars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/%-.x+?";
static const uint8_t kBMRows[][7] = {
        {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}   /* A */,
        {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}   /* B */,
        {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}   /* C */,
        {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E}   /* D */,
        {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}   /* E */,
        {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}   /* F */,
        {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F}   /* G */,
        {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}   /* H */,
        {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}   /* I */,
        {0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C}   /* J */,
        {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}   /* K */,
        {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}   /* L */,
        {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}   /* M */,
        {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}   /* N */,
        {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}   /* O */,
        {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}   /* P */,
        {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}   /* Q */,
        {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}   /* R */,
        {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}   /* S */,
        {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}   /* T */,
        {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}   /* U */,
        {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04}   /* V */,
        {0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11}   /* W */,
        {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11}   /* X */,
        {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}   /* Y */,
        {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}   /* Z */,
        {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}   /* 0 */,
        {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}   /* 1 */,
        {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}   /* 2 */,
        {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E}   /* 3 */,
        {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}   /* 4 */,
        {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E}   /* 5 */,
        {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}   /* 6 */,
        {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}   /* 7 */,
        {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}   /* 8 */,
        {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C}   /* 9 */,
        {0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10}   /* / */,
        {0x19, 0x19, 0x02, 0x04, 0x08, 0x13, 0x13}   /* % */,
        {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00}   /* - */,
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x06}   /* . */,
        {0x00, 0x00, 0x11, 0x0A, 0x04, 0x0A, 0x11}   /* x */,
        {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00}   /* + */,
        {0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04}   /* ? */
};

/* ---- The move-selection screen, drawn from its artwork (docs/media/move-screen*.png, turned into tables by
 * tools/make_move_skin.py): the backdrop as painted, the cards and capsules recoloured per type from the artwork's
 * own four, and over them what the battle says (names, PP, matchups, the party's balls and HP, the description). ---- */
static const uint8_t kMsSegLow[3] = {248, 104, 8};          /* the artwork's Water card, at a fifth of its PP */
static const uint8_t kMsEmpty[3] = {72, 44, 44};            /* an empty move slot's frame */

static int MsClamp(int v)
{
    return v < 0 ? 0 : v > 255 ? 255 : v;
}

static uint16_t MsRgb(int r, int g, int b)
{
    return UI_RGB(MsClamp(r), MsClamp(g), MsClamp(b));
}

static uint16_t MsGet(int x, int y)
{
    return (unsigned)x < W && (unsigned)y < H ? sCanvas[(size_t)x * H + (H - 1 - (size_t)y)] : 0;
}

/* `c` over the pixel by a cover of `a` (0..255). */
static void MsBlend(int x, int y, int r, int g, int b, int a)
{
    uint16_t o;
    int or_, og, ob;

    if (a <= 0)
        return;
    if (a >= 255) {
        Px(x, y, MsRgb(r, g, b));
        return;
    }
    o = MsGet(x, y);
    or_ = (o >> 11) * 255 / 31;
    og = ((o >> 5) & 63) * 255 / 63;
    ob = (o & 31) * 255 / 31;
    Px(x, y, MsRgb(or_ + (r - or_) * a / 255, og + (g - og) * a / 255, ob + (b - ob) * a / 255));
}

static void MsBg(int x, int y, int w, int h)
{
    for (int v = 0; v < h; ++v)
        for (int u = 0; u < w; ++u)
            if ((unsigned)(x + u) < W && (unsigned)(y + v) < H)
                Px(x + u, y + v, MSK_BG(x + u, y + v));
}

static void MsShot(const uint16_t *src, int w, int h, int x, int y)
{
    for (int v = 0; v < h; ++v)
        for (int u = 0; u < w; ++u)
            Px(x + u, y + v, src[v * w + u]);
}

/* The 5x7 lettering softened as the artwork's: each stem one pixel and a half (two when `bold`). */
static void MsTextB(const char *s, int x, int y, const uint8_t col[3], int bold)
{
    for (; *s; ++s, x += 6) {
        const char *p = strchr(kBMChars, *s);
        int g = p && *s ? (int)(p - kBMChars) : -1;

        if (g < 0)
            continue;
        for (int r = 0; r < 7; ++r)
            for (int c = 0; c < 5; ++c)
                if (kBMRows[g][r] & (1u << (4 - c))) {
                    MsBlend(x + c, y + r, col[0], col[1], col[2], 255);
                    if (!(c < 4 && (kBMRows[g][r] & (1u << (3 - c)))))
                        MsBlend(x + c + 1, y + r, col[0], col[1], col[2], bold ? 255 : 120);
                }
    }
}

static void MsText(const char *s, int x, int y, const uint8_t col[3])
{
    MsTextB(s, x, y, col, 0);
}

static int MsTextWidth(const char *s)
{
    int n = (int)strlen(s);

    return n ? n * 6 : 0;
}

/* The move name's lettering (8 rows, after the artwork's). */
static void MsTitle(const char *s, int x, int y, int limit)
{
    for (; *s; ++s) {
        const char *p = strchr(kMskTitleChars, *s);
        int g = p && *s ? (int)(p - kMskTitleChars) : (int)(strchr(kMskTitleChars, ' ') - kMskTitleChars);
        int w = kMskTitleWidth[g];

        if (x + w > limit)
            return;
        for (int r = 0; r < 8; ++r)
            for (int c = 0; c < w; ++c)
                MsBlend(x + c, y + r, 255, 255, 255, kMskTitleRows[g][r][c] * 17);
        x += w + 1;
    }
}

static void MsTiny(const char *s, int x, int y, int r, int g, int b)
{
    for (; *s; ++s, x += 4) {
        const char *p = strchr(kMskTinyChars, *s);

        if (!p || !*s)
            continue;
        for (int row = 0; row < 5; ++row)
            for (int c = 0; c < 3; ++c)
                if (kMskTinyRows[p - kMskTinyChars][row] & (4u >> c))
                    MsBlend(x + c, y + row, r, g, b, 255);
    }
}

static void BMStrip(const CtrBattleView *v)
{
    static const uint8_t kStatus[7][3] = {{0, 0, 0}, {184, 88, 208}, {0, 0, 0}, {150, 150, 176},
                                          {100, 200, 232}, {232, 96, 48}, {224, 88, 176}};
    static const char *const kStatusName[7] = {"", "PSN", "PAR", "SLP", "FRZ", "BRN", "PKRS"};

    MsBg(sBMStrip.x, sBMStrip.y, sBMStrip.w, sBMStrip.h);
    for (int i = 0; i < 6; ++i) {
        int bx = kMskSlotX[i];
        unsigned hp = v->partyHp[i];

        if (!v->party[i])
            continue;
        MsShot(&kMskTrack[0][0], MSK_TRACK_W, MSK_TRACK_H, bx + MSK_TRACK_U, MSK_TRACK_V);
        if (hp) {
            int fill = (int)(hp * 34 / 100);

            if (fill < 1)
                fill = 1;
            for (int r = 0; r < 5; ++r) {
                const uint8_t *g = kMskHpGreen[r], *y = kMskHpYellow[r];
                int rr = hp > 50 ? g[0] : hp > 20 ? y[0] : 230 * (g[1] + 40) / 255;
                int gg = hp > 50 ? g[1] : hp > 20 ? y[1] : 56 * (g[1] + 40) / 255;
                int bb = hp > 50 ? g[2] : hp > 20 ? y[2] : 44 * (g[1] + 40) / 255;

                for (int u = 0; u < fill; ++u)
                    Px(bx + u, 42 + r, MsRgb(rr, gg, bb));
            }
        }
        if (i == 0)
            MsShot(&kMskBallLead[0][0], MSK_LEAD_W, MSK_LEAD_H, bx + MSK_LEAD_U, MSK_LEAD_V);
        else
            MsShot(v->party[i] == 2 ? &kMskBallFaint[0][0] : &kMskBall[0][0], MSK_BALL_W, MSK_BALL_H,
                   bx + MSK_BALL_U, MSK_BALL_V);
        if (hp > 0 && hp < 100) {
            char pct[8];

            snprintf(pct, sizeof(pct), "%u%%", hp);
            MsTiny(pct, bx + 25, 35, 200, 170, 120);
        }
        if (v->partyStatus[i] >= 1 && v->partyStatus[i] <= 6) {
            int px = bx + 25, py = MSK_PAR_Y, st = v->partyStatus[i];

            if (st == 2)
                MsShot(&kMskPar[0][0], MSK_PAR_W, MSK_PAR_H, px, py);
            else {
                /* PAR's capsule in the status's colour, its letters taken out and written again */
                for (int r = 0; r < MSK_PAR_H; ++r)
                    for (int c = 0; c < MSK_PAR_W; ++c) {
                        uint16_t p = kMskPar[r][c];
                        int pr = (p >> 11) * 255 / 31, pg = ((p >> 5) & 63) * 255 / 63, pb = (p & 31) * 255 / 31;
                        int l = (pr * 3 + pg * 6 + pb) / 10;

                        if (r >= 4 && r <= 8 && c >= 3 && c <= 15) {
                            uint16_t q = kMskPar[r][2];
                            l = (((q >> 11) * 255 / 31) * 3 + (((q >> 5) & 63) * 255 / 63) * 6 + ((q & 31) * 255 / 31)) / 10;
                        }
                        if (pr - pb > 40 && l > 30)
                            Px(px + c, py + r, MsRgb(kStatus[st][0] * l / 150, kStatus[st][1] * l / 150, kStatus[st][2] * l / 150));
                        else
                            Px(px + c, py + r, p);
                    }
                MsTiny(kStatusName[st], px + (MSK_PAR_W - (int)strlen(kStatusName[st]) * 4 + 1) / 2, py + 4,
                       (kStatus[st][0] + 255) / 2, (kStatus[st][1] + 255) / 2, (kStatus[st][2] + 255) / 2);
            }
        }
    }
}

/* A card: the artwork's card in the type's colours, its capsule, icon, name, matchup, PP cells and count. */
static void BMCard(int i, const CtrBattleView *v)
{
    const CtrBattleMove *m = &v->move[i];
    int ox = kMskCardPos[i][0], oy = kMskCardPos[i][1];
    int type = m->present && m->type < 18 ? m->type : 0;
    const uint8_t *n = m->present ? kMskTypeNeon[type] : kMsEmpty;
    int pressed = v->pressed == i;

    /* The glow round the frame from the artwork's card in the type's colour; inside it, the owner's card for the type. */
    for (int y = 0; y < MSK_CARD_H; ++y)
        for (int x = 0; x < MSK_CARD_W; ++x) {
            const int16_t *t = kMskCard[y][x];

            if (m->present && x >= 3 && x < 3 + MSK_DLX_W && y >= 3 && y < 3 + MSK_DLX_H)
                Px(ox + x, oy + y, kMskDlx[type][y - 3][x - 3]);
            else
                Px(ox + x, oy + y, MsRgb(t[0] + t[3] * n[0] / 256, t[1] + t[3] * n[1] / 256, t[2] + t[3] * n[2] / 256));
        }
    if (!m->present)
        return;
    {
        static const uint8_t kWhite[3] = {248, 252, 248};
        char pp[16];

        /* The PP bar, one piece, the same place and length on every card, leaning like the cards' cells: lit from the left as far as the PP left,
         * empty after; the lit part in the card's colour over half, yellow at half, orange at a quarter (FireRed's steps). */
        static const uint8_t kWarn[2][3] = {{248, 216, 48}, {248, 136, 32}};
        int warn = m->pp * 4 <= m->maxPp ? 1 : m->pp * 2 <= m->maxPp ? 0 : -1;
        int x0 = MSK_DLX_BAR_X0, x1 = MSK_DLX_BAR_X1;
        int fill = m->maxPp ? (int)((unsigned)m->pp * (unsigned)(x1 - x0 + 1) / m->maxPp) : 0;

        if (m->pp && fill < 1)
            fill = 1;
        for (int y = 0; y < MSK_DLX_BAR_ROWS; ++y) {
            int v = MSK_DLX_BAR_V0 + y, off = MSK_DLX_SEG_ROW >= v ? (MSK_DLX_SEG_ROW - v) / 2 : -((v - MSK_DLX_SEG_ROW + 1) / 2);
            const uint8_t *l = kMskDlxBar[type].lit[y], *e = kMskDlxBar[type].empty[y];
            uint16_t lit = MsRgb(l[0], l[1], l[2]);

            if (warn >= 0) {                                  /* the warning colour, as bright as this row of the bar */
                int lum = (l[0] * 3 + l[1] * 6 + l[2]) / 10, ref = (kWarn[warn][0] * 3 + kWarn[warn][1] * 6 + kWarn[warn][2]) / 10;

                lit = MsRgb(kWarn[warn][0] * lum / ref, kWarn[warn][1] * lum / ref, kWarn[warn][2] * lum / ref);
            }
            for (int x = x0; x <= x1; ++x)
                Px(ox + 3 + x + off, oy + 3 + v, x - x0 < fill ? lit : MsRgb(e[0], e[1], e[2]));
        }
        MsTitle(m->name, ox + 13, oy + 18, ox + 112);
        MsText(m->typeName, ox + 3 + kMskDlxPillMid[type] - MsTextWidth(m->typeName) / 2, oy + 36, kWhite);
        MsText("PP", ox + 3 + 80, oy + 50, kWhite);              /* "PP" where the card writes it, the count to its right */
        snprintf(pp, sizeof(pp), "%u/%u", m->pp, m->maxPp);
        MsText(pp, ox + 128 - MsTextWidth(pp), oy + 50, m->pp ? kWhite : kMsSegLow);
        if (m->matchup != CTR_FX_NONE && m->matchup <= CTR_FX_NEUTRAL) {
            /* x2 star, x1/2 and x1/4 diamond, x0 cross, x4 star, x1 dash */
            static const char *const kText[] = {"", "x0", "x1/4", "x1/2", "x2", "x4", "x1"};
            static const uint8_t kMark[] = {0, 2, 1, 1, 0, 3, 4};
            static const struct { const uint8_t *ink, *a; uint8_t w, h, v, gap; } kMarks[5] = {
                {kMskMark0Ink, &kMskMark0[0][0], MSK_MARK0_W, MSK_MARK0_H, MSK_MARK0_V, MSK_MARK0_GAP},
                {kMskMark1Ink, &kMskMark1[0][0], MSK_MARK1_W, MSK_MARK1_H, MSK_MARK1_V, MSK_MARK1_GAP},
                {kMskMark2Ink, &kMskMark2[0][0], MSK_MARK2_W, MSK_MARK2_H, MSK_MARK2_V, MSK_MARK2_GAP},
                {kMskMark3Ink, &kMskMark3[0][0], MSK_MARK3_W, MSK_MARK3_H, MSK_MARK3_V, MSK_MARK3_GAP},
                {kMskMark4Ink, &kMskMark4[0][0], MSK_MARK4_W, MSK_MARK4_H, MSK_MARK4_V, MSK_MARK4_GAP}};
            int mk = kMark[m->matchup];
            const uint8_t *ink = kMarks[mk].ink, *a = kMarks[mk].a;
            int mw = kMarks[mk].w, mh = kMarks[mk].h, mv = kMarks[mk].v, gap = kMarks[mk].gap;
            int tx = ox + 128 - MsTextWidth(kText[m->matchup]), gx = tx - gap - mw;

            MsTextB(kText[m->matchup], tx, oy + 35, ink, 1);
            for (int y = 0; y < mh; ++y)
                for (int x = 0; x < mw; ++x)
                    MsBlend(gx + x, oy + mv + y, ink[0], ink[1], ink[2], a[y * mw + x]);
        }
    }
    if (pressed)                                             /* pressed: the artwork lights the card up */
        for (int y = 0; y < MSK_CARD_H; ++y)
            for (int x = 0; x < MSK_CARD_W; ++x) {
                uint16_t o = MsGet(ox + x, oy + y);

                Px(ox + x, oy + y, MsRgb((o >> 11) * 255 / 31 * 9 / 8 + 6, ((o >> 5) & 63) * 255 / 63 * 9 / 8 + 6,
                                        (o & 31) * 255 / 31 * 9 / 8 + 6));
            }
    if (v->showCursor && v->cursor == i) {
        CtrPokeTouchRect r = BMCardRect(i);

        BFocusGlowBrackets(&r);
    }
}

static void BMFooter(const CtrBattleView *v)
{
    static const uint8_t kLabel[3] = {160, 216, 228}, kWhite[3] = {248, 252, 248};
    int sel = v->cursor >= 0 && v->cursor < 4 && v->move[v->cursor].present ? v->cursor : 0;
    const CtrBattleMove *m = &v->move[sel];

    MsBg(sBMFooter.x, sBMFooter.y, sBMFooter.w, sBMFooter.h);
    if (!m->present)
        return;
    {
        const char *acc = m->accuracy[0] && m->accuracy[0] != '-' ? m->accuracy : "--";
        char val[16];

        MsText("ACC", 23, 202, kLabel);
        snprintf(val, sizeof(val), "%s%s", acc, acc[0] == '-' ? "" : "%");
        MsText(val, 44, 202, kWhite);
        if (m->effectChance) {
            MsText("EFF", 79, 202, kLabel);
            snprintf(val, sizeof(val), "%u%%", m->effectChance);
            MsText(val, 100, 202, kWhite);
        }
        if (m->stab)
            MsShot(&kMskStab[0][0], MSK_STAB_W, MSK_STAB_H, MSK_STAB_X, MSK_STAB_Y);
    }
    if (m->desc[0])
        BDescribeSmall(m->desc, 22, 214, 190, 2, UI_RGB(248, 244, 240));
    else
        UiTextSmall(m->type < 10 ? "Physical move" : "Special move", 22, 214, UI_RGB(200, 180, 180), 0, 212);
}

static void BMCancel(int pressed)
{
    if (pressed)
        MsShot(&kMskCancelDown[0][0], MSK_CANCEL_W, MSK_CANCEL_H, MSK_CANCEL_X, MSK_CANCEL_Y);
    else
        MsBg(MSK_CANCEL_X, MSK_CANCEL_Y, MSK_CANCEL_W, MSK_CANCEL_H);
}

static void BMoveScreen(const CtrBattleView *v)
{
    MsBg(0, 0, W, H);
    BMStrip(v);
    for (int i = 0; i < 4; ++i)
        BMCard(i, v);
    BMFooter(v);
    BMCancel(v->pressed == BUI_CANCEL);
}

/* ---- The action screen (battle_action_skin.h, made by tools/make_action_skin.py): the artwork's frame, the party's balls
 * and HP, the four action cards as painted, and the footer with the Pokemon in the lead and FireRed's question. ---- */
static const CtrPokeTouchRect sABStrip = {AB_STRIP_X0, AB_STRIP_Y0, AB_STRIP_X1 - AB_STRIP_X0, AB_STRIP_Y1 - AB_STRIP_Y0};
static const CtrPokeTouchRect sABFoot = {AB_FOOT_X0, AB_FOOT_Y0, AB_FOOT_X1 - AB_FOOT_X0, AB_FOOT_Y1 - AB_FOOT_Y0};

static int BAbScreen(int state)
{
    return state == BUI_ACTION || state == BUI_IDLE;
}

static void AbBg(int x, int y, int w, int h)
{
    for (int v = 0; v < h; ++v)
        for (int u = 0; u < w; ++u)
            if ((unsigned)(x + u) < W && (unsigned)(y + v) < H)
                Px(x + u, y + v, AB_BG(x + u, y + v));
}

static void AbStrip(const CtrBattleView *v)
{
    static const uint8_t kStatus[7][3] = {{0, 0, 0}, {184, 88, 208}, {0, 0, 0}, {150, 150, 176},
                                          {100, 200, 232}, {232, 96, 48}, {224, 88, 176}};
    static const char *const kStatusName[7] = {"", "PSN", "PAR", "SLP", "FRZ", "BRN", "PKRS"};

    AbBg(sABStrip.x, sABStrip.y, sABStrip.w, sABStrip.h);
    for (int i = 0; i < 6; ++i) {
        int bx = kAbSlotX[i];
        unsigned hp = v->partyHp[i];

        if (!v->party[i])
            continue;
        MsShot(&kAbTrack[0][0], AB_TRACK_W, AB_TRACK_H, bx + AB_TRACK_U, AB_TRACK_V);
        if (hp) {
            int fill = (int)(hp * 34 / 100);

            if (fill < 1)
                fill = 1;
            for (int r = 0; r < 5; ++r) {
                const uint8_t *g = kAbHpGreen[r], *y = kAbHpYellow[r];
                int rr = hp > 50 ? g[0] : hp > 20 ? y[0] : 230 * (g[1] + 40) / 255;
                int gg = hp > 50 ? g[1] : hp > 20 ? y[1] : 56 * (g[1] + 40) / 255;
                int bb = hp > 50 ? g[2] : hp > 20 ? y[2] : 44 * (g[1] + 40) / 255;

                for (int u = 0; u < fill; ++u)
                    Px(bx + u, 42 + r, MsRgb(rr, gg, bb));
            }
        }
        if (i == 0)
            MsShot(&kAbBallLead[0][0], AB_LEAD_W, AB_LEAD_H, bx + AB_LEAD_U, AB_LEAD_V);
        else
            MsShot(v->party[i] == 2 ? &kAbBallFaint[0][0] : &kAbBall[0][0], AB_BALL_W, AB_BALL_H,
                   bx + AB_BALL_U, AB_BALL_V);
        if (hp > 0 && hp < 100) {
            char pct[8];

            snprintf(pct, sizeof(pct), "%u%%", hp);
            MsTiny(pct, bx + 25, 35, 200, 170, 120);
        }
        if (v->partyStatus[i] >= 1 && v->partyStatus[i] <= 6) {
            int px = bx + 25, py = AB_PAR_Y, st = v->partyStatus[i];

            if (st == 2)
                MsShot(&kAbPar[0][0], AB_PAR_W, AB_PAR_H, px, py);
            else {
                /* PAR's capsule in the status's colour, its letters taken out and written again */
                for (int r = 0; r < AB_PAR_H; ++r)
                    for (int c = 0; c < AB_PAR_W; ++c) {
                        uint16_t p = kAbPar[r][c];
                        int pr = (p >> 11) * 255 / 31, pg = ((p >> 5) & 63) * 255 / 63, pb = (p & 31) * 255 / 31;
                        int l = (pr * 3 + pg * 6 + pb) / 10;

                        if (r >= 4 && r <= 8 && c >= 3 && c <= 15) {
                            uint16_t q = kAbPar[r][2];
                            l = (((q >> 11) * 255 / 31) * 3 + (((q >> 5) & 63) * 255 / 63) * 6 + ((q & 31) * 255 / 31)) / 10;
                        }
                        if (pr - pb > 40 && l > 30)
                            Px(px + c, py + r, MsRgb(kStatus[st][0] * l / 150, kStatus[st][1] * l / 150, kStatus[st][2] * l / 150));
                        else
                            Px(px + c, py + r, p);
                    }
                MsTiny(kStatusName[st], px + (AB_PAR_W - (int)strlen(kStatusName[st]) * 4 + 1) / 2, py + 4,
                       (kStatus[st][0] + 255) / 2, (kStatus[st][1] + 255) / 2, (kStatus[st][2] + 255) / 2);
            }
        }
    }
}

/* A card as painted; pressed it is lit, selected by the D-pad it wears the golden brackets. */
static void AbCard(int i, const CtrBattleView *v)
{
    int ox = kAbCardPos[i][0], oy = kAbCardPos[i][1];
    int pressed = v->pressed == i, focused = v->showCursor && v->cursor == i;

    for (int y = 0; y < AB_CARD_H; ++y)
        for (int x = 0; x < AB_CARD_W; ++x) {
            uint16_t c = kAbCard[i][y][x];

            if (pressed) {
                int r = (c >> 11) * 255 / 31, g = ((c >> 5) & 63) * 255 / 63, b = (c & 31) * 255 / 31;

                c = MsRgb(r * 9 / 8 + 8, g * 9 / 8 + 8, b * 9 / 8 + 8);
            }
            Px(ox + x, oy + y, c);
        }
    if (focused) {
        CtrPokeTouchRect r = {(int16_t)(ox + 1), (int16_t)(oy + 1), AB_CARD_W - 2, AB_CARD_H - 2};   /* round the card's whole frame */

        BFocusGlowBrackets(&r);
    }
}

/* The footer: the lead's name, level and HP as numbers and a bar, and FireRed's question. */
static void AbFooter(const CtrBattleView *v)
{
    static const uint8_t kWhite[3] = {248, 252, 248};
    char text[40], name[BUI_NAME_LEN];
    int n = 0;

    AbBg(sABFoot.x, sABFoot.y, sABFoot.w, sABFoot.h);
    for (; v->activeName[n] && n < BUI_NAME_LEN - 1; ++n) {
        char ch = v->activeName[n];

        name[n] = (char)(ch >= 'a' && ch <= 'z' ? ch - ('a' - 'A') : ch);
    }
    name[n] = 0;
    if (!v->activeMaxHp)
        return;
    MsTiny(name, AB_NAME_X, AB_NAME_Y, kWhite[0], kWhite[1], kWhite[2]);
    snprintf(text, sizeof(text), "%u", (unsigned)v->activeLevel);
    MsTiny(text, AB_LEVEL_X, AB_LEVEL_Y, kAbLevelInk[0], kAbLevelInk[1], kAbLevelInk[2]);
    {
        unsigned pct = (unsigned)v->activeHp * 100u / v->activeMaxHp;
        int fill = (int)((unsigned)v->activeHp * AB_HPBAR_W / v->activeMaxHp);

        if (v->activeHp && fill < 1)
            fill = 1;
        for (int r = 0; r < AB_HPBAR_H; ++r)
            for (int u = 0; u < AB_HPBAR_W; ++u) {
                const uint8_t *g = kAbHpGreen[r + 1], *y = kAbHpYellow[r + 1];
                int rr = pct > 50 ? g[0] : pct > 20 ? y[0] : 230 * (g[1] + 40) / 255;
                int gg = pct > 50 ? g[1] : pct > 20 ? y[1] : 56 * (g[1] + 40) / 255;
                int bb = pct > 50 ? g[2] : pct > 20 ? y[2] : 44 * (g[1] + 40) / 255;

                Px(AB_HPBAR_X + u, AB_HPBAR_Y + r, u < fill ? MsRgb(rr, gg, bb) : MsRgb(36, 12, 14));
            }
    }
    snprintf(text, sizeof(text), "%u/%u", (unsigned)v->activeHp, (unsigned)v->activeMaxHp);
    MsTiny(text, AB_HPTEXT_RIGHT - 4 * (int)strlen(text) + 1, AB_HPTEXT_Y, kWhite[0], kWhite[1], kWhite[2]);
    if (v->state == BUI_ACTION) {
        snprintf(text, sizeof(text), "What will %s do?", name);
        UiText(text, AB_MESSAGE_X, AB_MESSAGE_Y - 1, UI_TEXT, UIS_TEXT_SH, 1, 290);
    }
}

static void BActionScreen(const CtrBattleView *v)
{
    AbBg(0, 0, W, H);
    AbStrip(v);
    AbFooter(v);
    if (v->state == BUI_ACTION)
        for (int i = 0; i < 4; ++i)
            AbCard(i, v);
}

/* Aiming a move in a double battle: a card for each battler FireRed lets it be aimed at, the opposing ones on top in
 * FIGHT's red, the player's below in POKeMON's blue: the name, the level, the matchup badge, the side and the HP bar. */
static void BTargetCard(int i, const CtrBattleView *v)
{
    static const UiButtonStyle sFoe = UI_STYLE_FIGHT;
    static const UiButtonStyle sAlly = {UI_RGB(206, 108, 22), UI_RGB(244, 156, 52), UI_RGB(128, 62, 10)};   /* amber, not blue */
    const CtrPokeTouchRect *r = &sBCard[i];
    const CtrBattleTarget *t = &v->target[i];
    int state = (v->pressed == i ? UI_PRESSED : 0) | (v->showCursor && v->cursor == i ? UI_FOCUSED : 0);
    int dy;

    if (!t->present) {
        BCardFrame(r, 0, -1, NULL, 0);
        return;
    }
    dy = BCardFrame(r, t->side ? sAlly.body : sFoe.body, -1, t->name, state);
    {
        char level[12], hp[20];
        CtrPokeTouchRect tag = {(int16_t)(r->x + r->w - 46), (int16_t)(r->y + 27 + dy), 38, 14};
        CtrPokeTouchRect bar = {(int16_t)(r->x + 8), (int16_t)(r->y + 46 + dy), (int16_t)(r->w - 16), 6};

        snprintf(level, sizeof(level), "Lv%u", t->level);
        UiText(level, r->x + 8, r->y + 30 + dy, UI_TEXT, UIS_TEXT_SH, 1, r->x + 60);
        BMatchupTag(t->matchup, r->x + r->w - 90, r->y + 27 + dy);
        UiBadge(&tag, t->side ? sAlly.shade : sFoe.shade, t->side ? "ALLY" : "FOE");
        UiHpBar(&bar, t->hp, t->maxHp);
        if (t->side) {
            snprintf(hp, sizeof(hp), "%u/%u", t->hp, t->maxHp);
            UiTextRight(hp, r->x + r->w - 8, r->y + 55 + dy, UIS_TEXT_DIM, UIS_TEXT_SH, 1);
        }
    }
}

static void BTarget(const CtrBattleView *v)
{
    for (int i = 0; i < 4; ++i)
        BTargetCard(i, v);
    BCancel(&sBCancel, v->pressed == BUI_CANCEL);
}

static void BSafariButton(int i, const CtrBattleView *v)
{
    const UiButtonStyle *sStyle[4] = {&sUiS->fight, &sUiS->bag, &sUiS->pokemon, &sUiS->run};
    static const char *const label[4] = {"BALL", "BAIT", "ROCK", "RUN"};
    static const int sSafariIcon[4] = {BI_POKEBALL, BI_BAIT, BI_ROCK, BI_SHOE};
    static const uint16_t sSafariAccent[4] = {UI_RGB(70, 172, 255), UI_RGB(236, 112, 140), UI_RGB(190, 182, 170), UI_RGB(72, 206, 104)};
    const CtrPokeTouchRect *r = &sBSafari[i];
    int state = (v->pressed == i ? UI_PRESSED : 0) | (v->showCursor && v->cursor == i ? UI_FOCUSED : 0);
    int dy = (state & UI_PRESSED) ? 1 : 0, cx = r->x + r->w / 2;

    BIconsPrepare();
    UiButton(r, sStyle[i], state);
    BAccent(r, dy, sSafariAccent[i]);
    {
        /* centred by its own size (BALL is 34x34, RUN 42x34), in the 38 pixels between the accent and the label */
        int w = sBImg[sSafariIcon[i]].image.w * sBImg[sSafariIcon[i]].scale, h = sBImg[sSafariIcon[i]].image.h * sBImg[sSafariIcon[i]].scale;

        BIconAt(sSafariIcon[i], cx - w / 2, r->y + 12 + (38 - h) / 2 + dy);
    }
    UiText(label[i], cx - UiTextWidth(label[i], 2) / 2, r->y + 52 + dy, UI_TEXT, sStyle[i]->shade, 2, r->x + r->w);
}

/* The Safari Zone's menu: BALL, BAIT, ROCK, RUN (the words of FireRed's gText_SafariZoneMenu), as MAIN's buttons. */
static void BSafari(const CtrBattleView *v)
{
    for (int i = 0; i < 4; ++i)
        BSafariButton(i, v);
}

/* The battle Party: a header, the six Pokemon in two columns of three, CANCEL. */
extern int CtrBottomParty_Icon(int slot, unsigned char *tilesOut, unsigned short *palOut);
/* Party icons: FireRed's 32x32 icon of a slot, decoded once per species and slot. */
static struct {
    unsigned icon;
    int ok;
    uint8_t pixels[32 * 32];
    uint16_t pal[16];
} sBPIcon[6];

static int BPartyIcon(int slot, unsigned icon)
{
    static unsigned char tiles[512];
    static unsigned short pal[16];

    if (sBPIcon[slot].icon == icon + 1)
        return sBPIcon[slot].ok;
    memset(tiles, 0, sizeof(tiles));
    sBPIcon[slot].icon = icon + 1;
    sBPIcon[slot].ok = CtrBottomParty_Icon(slot, tiles, pal);
    if (!sBPIcon[slot].ok)
        return 0;
    for (int y = 0; y < 32; ++y)
        for (int x = 0; x < 32; ++x) {
            unsigned byte = tiles[(y / 8 * 4 + x / 8) * 32 + y % 8 * 4 + x % 8 / 2];
            sBPIcon[slot].pixels[y * 32 + x] = (uint8_t)((byte >> (x & 1) * 4) & 15);
        }
    for (int i = 0; i < 16; ++i) {
        unsigned c = pal[i];
        sBPIcon[slot].pal[i] = RGB((c & 31) << 3, ((c >> 5) & 31) << 3, ((c >> 10) & 31) << 3);
    }
    return 1;
}

/* Text wrapped into `maxLines` lines of `width`, in `ink` over `shadow`. */
static void BPWrap(const char *text, int x, int y, int width, int maxLines, uint16_t ink, uint16_t shadow)
{
    char word[24], line[80], joined[80];
    int lines = 0;
    const char *p = text;

    line[0] = 0;
    while (*p && lines < maxLines) {
        int w = 0;

        while (*p == ' ' || *p == '\n')
            ++p;
        while (*p && *p != ' ' && *p != '\n' && w < (int)sizeof(word) - 1)
            word[w++] = *p++;
        word[w] = 0;
        if (!w)
            break;
        memcpy(joined, line, sizeof(joined));
        BAppend(joined, sizeof(joined), word);
        if (line[0] && UiTextWidth(joined, 1) > width) {
            UiText(line, x, y + lines * 16, ink, shadow, 1, x + width);
            line[0] = 0;
            if (++lines >= maxLines)
                return;
            BAppend(line, sizeof(line), word);
        } else
            memcpy(line, joined, sizeof(line));
    }
    if (line[0] && lines < maxLines)
        UiText(line, x, y + lines * 16, ink, shadow, 1, x + width);
}

/* A Party card: a raised card (amber and glowing for the Party's selection, grey for one that fainted) with the icon
 * on its round plate, the name, the level or a status chip, the HP bar and numbers. An empty slot is a hollow. */
static void BPartyPanel(int i, const CtrBattleView *v)
{
    static const char *const code[7] = {"", "PSN", "PAR", "SLP", "FRZ", "BRN", "PKRS"};
    static const uint16_t tint[7] = {0, UI_RGB(170, 80, 190), UI_RGB(214, 170, 30), UI_RGB(120, 128, 150),
                                     UI_RGB(80, 170, 220), UI_RGB(226, 96, 48), UI_RGB(200, 90, 160)};
    const CtrBattleParty *p = &v->pty;
    CtrPokeTouchRect r = BPanel(i);
    int fainted = p->present[i] && !p->egg[i] && p->hp[i] == 0, selected = p->cursor == i;
    int press = v->pressed == BUI_PTY_SLOT + i;
    const EmPal *pal = selected ? &kEmAmber : fainted ? &kEmGrey : &kEmCream;
    int fy, fh = r.h - BP_DEPTH;

    TsBgRect(r.x - 2, r.y - 2, r.w + 4, r.h + 4);
    if (!p->present[i]) {
        UiRoundFill(r.x, r.y, r.w, fh, 5, UI_RGB(140, 70, 60));
        UiRoundFill(r.x + 1, r.y + 1, r.w - 2, fh - 2, 4, UI_RGB(70, 18, 16));
        return;
    }
    if (selected)
        EmGlowRing(r.x, r.y, r.w, r.h);
    fy = EmButton(r.x, r.y, r.w, r.h, pal, BP_DEPTH, press);
    UiDisc(r.x + 21, fy + 21, 15, UiMix(pal->bottom, pal->edge, 2));
    UiDisc(r.x + 21, fy + 21, 14, UiMix(pal->top, pal->bottom, 6));
    if (BPartyIcon(i, p->icon[i]))
        for (int y = 0; y < 32; ++y)
            for (int x = 0; x < 32; ++x) {
                uint8_t index = sBPIcon[i].pixels[y * 32 + x];
                if (index)
                    Px(r.x + 5 + x, fy + 3 + y, fainted ? Tone(sBPIcon[i].pal[index], 0) : sBPIcon[i].pal[index]);
            }
    UiText(p->name[i], r.x + 41, fy + 4, pal->ink, pal->inkSh, 1, r.x + r.w - 4);
    if (p->egg[i])
        return;
    {
        char text[24];
        CtrPokeTouchRect bar = {(int16_t)(r.x + 41), (int16_t)(fy + 34), 52, 6};

        if (fainted || (p->status[i] >= 1 && p->status[i] <= 6)) {
            CtrPokeTouchRect badge = {(int16_t)(r.x + 41), (int16_t)(fy + 17), 30, 13};

            UiBadge(&badge, fainted ? UI_RGB(90, 80, 90) : tint[p->status[i]], fainted ? "FNT" : code[p->status[i]]);
        } else {
            snprintf(text, sizeof(text), "Lv%u", p->level[i]);
            UiText(text, r.x + 41, fy + 18, pal->ink, pal->inkSh, 1, r.x + r.w - 4);
        }
        UiRoundFill(bar.x - 1, bar.y - 1, bar.w + 2, bar.h + 2, 2, UI_RGB(64, 72, 104));
        UiHpBar(&bar, p->hp[i], p->maxHp[i]);
        snprintf(text, sizeof(text), "%u/%u", p->hp[i], p->maxHp[i]);
        UiText(text, r.x + r.w - 6 - UiTextWidth(text, 1), fy + 30, pal->ink, pal->inkSh, 1, r.x + r.w);
    }
}

/* CANCEL: violet, as BACK. */
static void BPCancelButton(int pressed)
{
    const CtrPokeTouchRect *r = &sBPCancel;
    int fy;

    TsBgRect(r->x, r->y, r->w, r->h);
    fy = EmButton(r->x, r->y, r->w, r->h, &kEmViolet, BP_DEPTH, pressed);
    EmLabel("CANCEL", r->x, r->w, fy, r->h - BP_DEPTH, &kEmViolet);
}

/* The note card: what the Party says (its question or hint), or, with no CANCEL, that one must be sent out. */
static void BPNoteCard(const CtrBattleView *v)
{
    const CtrPokeTouchRect *n = &sBPNote;
    const char *text = v->pty.caption[0] ? v->pty.caption
                     : v->pty.mode == BUI_PTY_FORCED ? "A POK\xC3\xA9MON must be sent out."
                     : v->pty.mode == BUI_PTY_TARGET ? "Use on which one?" : "Choose a POK\xC3\xA9MON.";

    TsBgRect(n->x, n->y, n->w, n->h);
    EmNote(n->x, n->y, n->w, n->h);
    BPWrap(text, n->x + 12, n->y + 14, n->w - 18, 2, EM_DARK, EM_DARK_SH);
}

/* What the Party has open over its slots, over the dimmed Party: its menu (the selected row amber, the last violet),
 * or the message it waits on in a card. */
static void BPartyOverlay(const CtrBattleView *v)
{
    const CtrBattleParty *p = &v->pty;
    CtrPokeTouchRect all = {0, 0, W, H};

    if (p->panel == BUI_PTY_TEXT) {
        CtrPokeTouchRect box = {20, 70, 280, 76};

        UiDim(&all);
        EmNote(box.x, box.y, box.w, box.h);
        BPWrap(p->message, box.x + 14, box.y + 10, box.w - 24, 3, EM_DARK, EM_DARK_SH);
        UiText("Tap to continue", box.x + box.w - 14 - UiTextWidth("Tap to continue", 1) - 9, box.y + box.h - 16,
               EM_DIM, EM_DARK_SH, 1, box.x + box.w);
        UiArrow(box.x + box.w - 18, box.y + box.h - 16, 1, EM_STRIPE);
    } else if (p->panel == BUI_PTY_ACTIONS || p->panel == BUI_PTY_YESNO) {
        int n = p->menuCount > 8 ? 8 : p->menuCount;

        UiDim(&all);
        for (int i = 0; i < n; ++i) {
            CtrPokeTouchRect r = BPMenuRow(n, i);
            const EmPal *pal = p->menuCursor == i ? &kEmAmber : i == n - 1 ? &kEmViolet : &kEmCream;
            int fy;

            if (p->menuCursor == i)
                EmGlowRing(r.x, r.y, r.w, r.h);
            fy = EmButton(r.x, r.y, r.w, r.h, pal, BP_DEPTH, v->pressed == BUI_PTY_MENU + i);
            EmLabel(p->menuName[i], r.x, r.w, fy, r.h - BP_DEPTH, pal);
        }
    }
}

static void BParty(const CtrBattleView *v)
{
    TsBackdrop();
    for (int i = 0; i < 6; ++i)
        BPartyPanel(i, v);
    BPNoteCard(v);
    if (v->pty.mode != BUI_PTY_FORCED)
        BPCancelButton(v->pressed == BUI_PTY_CANCEL);
    BPartyOverlay(v);
}

/* Item icons: FireRed's 24x24 icon of an item, decoded once into a collision-free 12-slot FIFO cache. */
#define BICONS 12
static struct {
    unsigned item;
    int ok;
    uint8_t pixels[24 * 24];
    uint16_t pal[16];
} sBIcon[BICONS];
static int sBIconNext;

static int BItemIcon(unsigned item)
{
    static unsigned char tiles[0x200];
    static unsigned short pal[16];
    int slot = -1;

    if (item == 0)
        return -1;
    for (int i = 0; i < BICONS; ++i)
        if (sBIcon[i].item == item)
            return sBIcon[i].ok ? i : -1;
    for (int i = 0; i < BICONS; ++i)
        if (sBIcon[i].item == 0) {
            slot = i;
            break;
        }
    if (slot < 0) {
        slot = sBIconNext;
        sBIconNext = (sBIconNext + 1) % BICONS;
    }
    memset(tiles, 0, sizeof(tiles));
    sBIcon[slot].item = item;
    sBIcon[slot].ok = CtrPokeTouch_ItemIcon(item, tiles, pal);
    if (!sBIcon[slot].ok)
        return -1;
    for (int y = 0; y < 24; ++y)
        for (int x = 0; x < 24; ++x) {
            unsigned byte = tiles[(y / 8 * 3 + x / 8) * 32 + y % 8 * 4 + x % 8 / 2];
            sBIcon[slot].pixels[y * 24 + x] = (uint8_t)((byte >> (x & 1) * 4) & 15);
        }
    for (int i = 0; i < 16; ++i) {
        unsigned c = pal[i];
        sBIcon[slot].pal[i] = RGB((c & 31) << 3, ((c >> 5) & 31) << 3, ((c >> 10) & 31) << 3);
    }
    return slot;
}

/* An item's icon at `scale` times its size, its top-left corner at (x, y). */
static void BItemDraw(unsigned item, int x, int y, int scale)
{
    int slot = BItemIcon(item);

    if (slot >= 0)
        UiImage(sBIcon[slot].pixels, sBIcon[slot].pal, 24, 24, 24, x, y, scale, 1);
}

static void BTabs(const CtrBattleView *v)
{
    for (int i = 0; i < 3; ++i)
        UiTab(&sBTab[i], sBTabLabel[i], (v->bag.pocket == sBTabPocket[i] ? UI_SELECTED : 0)
                                        | (v->pressed == BUI_BAG_TAB + i ? UI_PRESSED : 0));
}

/* A list row: the icon, the name and (but for key items) the quantity; the Bag's selection is the blue of a selection
 * with a bright edge, the row under a touch the pressed blue. */
static void BBagRow(int i, const CtrBattleView *v)
{
    const CtrBattleBag *b = &v->bag;
    CtrPokeTouchRect r = BRow(i);
    int selected = b->first + i == b->cursor, press = v->pressed == BUI_BAG_ROW + i;

    Fill(r.x, r.y, r.w, r.h, UIS_PANEL_LO);
    if (!b->row[i].item)
        return;
    if (selected || press) {
        UiRoundFill(r.x, r.y, r.w, r.h - 1, UI_RADIUS_PANEL, press ? UIS_SELECT_HI : UIS_SELECT);
        Fill(r.x + 1, r.y + 4, 2, r.h - 9, UIS_FOCUS_LIT);
    } else
        UiSeparator(r.x + 4, r.y + r.h - 1, r.w - 8);
    BItemDraw(b->row[i].item, r.x + 5, r.y, 1);
    UiText(b->row[i].name, r.x + 32, r.y + 8, UI_TEXT, UIS_TEXT_SH, 1, r.x + r.w - 30);
    if (b->pocket != 1) {                       /* key items are one of each: no count */
        char qty[8];

        snprintf(qty, sizeof(qty), "x%u", b->row[i].qty > 999 ? 999 : b->row[i].qty);
        UiTextRight(qty, r.x + r.w - 6, r.y + 8, UIS_TEXT_DIM, UIS_TEXT_SH, 1);
    }
}

static void BBagList(const CtrBattleView *v)
{
    const CtrBattleBag *b = &v->bag;

    UiPanel(&sBList, UIS_PANEL_LO, UIS_BORDER);
    for (int i = 0; i < BUI_BAG_ROWS; ++i)
        BBagRow(i, v);
    if (b->count > BUI_BAG_ROWS) {
        if (b->first > 0)
            UiArrow(sBUpArrow.x + 7, sBUpArrow.y + 4, -2, UIS_FOCUS);
        if (b->first + BUI_BAG_ROWS < b->count)
            UiArrow(sBDownArrow.x + 7, sBDownArrow.y + 4, 2, UIS_FOCUS);
    }
}

/* The right panel: the selected item (icon on its plate, name, description); over it the open menu or the Bag's
 * message. */
static void BBagDetail(const CtrBattleView *v)
{
    const UiButtonStyle *sFirst = &sUiS->primary, *sOther = &sUiS->cancel;
    const CtrBattleBag *b = &v->bag;
    CtrPokeTouchRect plate = {218, 74, 58, 56}, name = {185, 136, 125, 12};

    UiPanel(&sBDetail, UIS_PANEL, UIS_BORDER);
    if (b->message[0]) {
        BDescribeN(b->message, 189, 76, 117, 7, UI_TEXT);
        UiText("Tap", 260, 180, UIS_TEXT_DIM, UIS_TEXT_SH, 1, 300);
        UiArrow(282, 180, 1, UIS_FOCUS);
        return;
    }
    if (b->menuCount > 0) {
        if (b->selItem) {
            BItemDraw(b->selItem, 189, 74, 1);
            UiText(b->selName, 217, 82, UI_TEXT, UIS_TEXT_SH, 1, 310);
        }
        for (int i = 0; i < b->menuCount && i < 3; ++i) {
            CtrPokeTouchRect r = BMenuRow(i);
            int state = (v->pressed == BUI_BAG_MENU + i ? UI_PRESSED : 0) | (b->menuCursor == i ? UI_FOCUSED : 0);

            UiLabelButton(&r, i == 0 ? sFirst : sOther, state, b->menuName[i], 1);
        }
        return;
    }
    UiRoundFill(plate.x, plate.y, plate.w, plate.h, UI_RADIUS_BUTTON, UIS_PANEL_LO);
    if (b->selItem) {
        BItemDraw(b->selItem, plate.x + 5, plate.y + 4, 2);
        UiTextCentered(b->selName, &name, name.y, UI_TEXT, UIS_TEXT_SH, 1);
        UiSeparator(189, 151, 117);
        BDescribeN(b->selDesc, 189, 157, 117, 3, UIS_TEXT_DIM);
    } else
        UiTextCentered("Close the Bag", &name, name.y, UIS_TEXT_DIM, UIS_TEXT_SH, 1);
}

static void BBag(const CtrBattleView *v)
{
    BTabs(v);
    BBagList(v);
    BBagDetail(v);
    BCancel(&sBBagCancel, v->pressed == BUI_BAG_CANCEL);
}

/* The body: the navy backdrop and the screen for the controller's state (nothing at all while it is busy). */
static void BBody(const CtrBattleView *v)
{
    Fill(sBBody.x, sBBody.y, sBBody.w, sBBody.h, UIS_BG);
    if (v->state == BUI_BAG)
        BBag(v);
    else if (v->state == BUI_TARGET)
        BTarget(v);
    else if (v->state == BUI_SAFARI)
        BSafari(v);
}

void CtrBattleUi_Draw(uint16_t *canvas, const CtrBattleView *v)
{
    sCanvas = canvas;
    sUiS = &sUiRed;
    if (v->state == BUI_MOVE) {
        BMoveScreen(v);
        sUiS = &sUiNavy;
        return;
    }
    if (BAbScreen(v->state)) {
        BActionScreen(v);
        sUiS = &sUiNavy;
        return;
    }
    if (v->state == BUI_PARTY) {
        BParty(v);
        sUiS = &sUiNavy;
        return;
    }
    Fill(0, 0, W, H, UIS_BG);
    Fill(0, 0, W, 2, UIS_BG_TOP);
    BStrip(v);
    BBody(v);
    sUiS = &sUiNavy;
}

/* Each changed battle element is presented through the shared dirty-rectangle path. */
static void BMark(CtrDirtyList *dirty, const CtrPokeTouchRect *r)
{
    CtrDirty_Add(dirty, r->x, r->y, r->x + r->w, r->y + r->h);
}

/* Whether two Party views differ only by `cursor` (with no modal open), so only the affected slot panels need redraw. */
static int BPartySlotsOnly(const CtrBattleParty *a, const CtrBattleParty *b)
{
    CtrBattleParty copy = *a;

    if (a->panel != BUI_PTY_NONE || b->panel != BUI_PTY_NONE)
        return 0;
    copy.cursor = b->cursor;
    return memcmp(&copy, b, sizeof(copy)) == 0;
}

void CtrBattleUi_Update(uint16_t *canvas, const CtrBattleView *old, const CtrBattleView *v, CtrDirtyList *dirty)
{
    sCanvas = canvas;
    sUiS = &sUiRed;
    CtrDirty_Clear(dirty);
    if (BAbScreen(v->state) || BAbScreen(old->state)) {
        if (old->state != v->state || memcmp(old->party, v->party, sizeof(v->party))
            || memcmp(old->partyHp, v->partyHp, sizeof(v->partyHp)) || memcmp(old->partyStatus, v->partyStatus, sizeof(v->partyStatus))
            || strcmp(old->activeName, v->activeName) || old->activeLevel != v->activeLevel
            || old->activeHp != v->activeHp || old->activeMaxHp != v->activeMaxHp) {
            CtrPokeTouchRect all = {0, 0, W, H};

            CtrBattleUi_Draw(canvas, v);
            sUiS = &sUiRed;
            BMark(dirty, &all);
        } else {
            for (int i = 0; i < 4; ++i)
                if (BLooksChanged(old, v, i)) {
                    CtrPokeTouchRect g = {kAbCardPos[i][0], kAbCardPos[i][1], AB_CARD_W, AB_CARD_H};

                    AbBg(g.x, g.y, g.w, g.h);
                    AbCard(i, v);
                    BMark(dirty, &g);
                }
        }
        sUiS = &sUiNavy;
        return;
    }
    if (v->state == BUI_MOVE || old->state == BUI_MOVE) {
        /* The move screen has its own chassis: leaving or entering it, or a change of moves or of the party, redraws
         * everything; otherwise only the card, the footer or CANCEL that looks different. */
        if (old->state != v->state || memcmp(old->move, v->move, sizeof(v->move))
            || memcmp(old->party, v->party, sizeof(v->party)) || memcmp(old->partyHp, v->partyHp, sizeof(v->partyHp))
            || memcmp(old->partyStatus, v->partyStatus, sizeof(v->partyStatus))) {
            CtrPokeTouchRect all = {0, 0, W, H};

            CtrBattleUi_Draw(canvas, v);
            sUiS = &sUiRed;
            BMark(dirty, &all);
        } else {
            for (int i = 0; i < 4; ++i)
                if (BLooksChanged(old, v, i)) {
                    CtrPokeTouchRect g = BMCellRect(i);

                    BMCard(i, v);
                    BMark(dirty, &g);
                }
            if (old->cursor != v->cursor || old->showCursor != v->showCursor) {
                BMFooter(v);
                BMark(dirty, &sBMFooter);
            }
            if ((old->pressed == BUI_CANCEL) != (v->pressed == BUI_CANCEL)) {
                BMCancel(v->pressed == BUI_CANCEL);
                BMark(dirty, &sBMCancel);
            }
        }
        sUiS = &sUiNavy;
        return;
    }
    if (v->state == BUI_PARTY || old->state == BUI_PARTY) {
        /* The Party has its own screen: a move of the selection or a press redraws its cards, anything else all of it. */
        if (old->state != v->state || !BPartySlotsOnly(&old->pty, &v->pty)
            || ((old->pressed >= BUI_PTY_MENU && old->pressed < BUI_PTY_MESSAGE) != (v->pressed >= BUI_PTY_MENU && v->pressed < BUI_PTY_MESSAGE))
            || (v->pty.panel != BUI_PTY_NONE && old->pressed != v->pressed)) {
            CtrPokeTouchRect all = {0, 0, W, H};

            BParty(v);
            BMark(dirty, &all);
        } else {
            for (int i = 0; i < 6; ++i)
                if ((old->pty.cursor == i) != (v->pty.cursor == i)
                    || (old->pressed == BUI_PTY_SLOT + i) != (v->pressed == BUI_PTY_SLOT + i)) {
                    CtrPokeTouchRect r = UiInset(&(CtrPokeTouchRect){BPanel(i).x, BPanel(i).y, BPanel(i).w, BPanel(i).h}, -2);

                    BPartyPanel(i, v);
                    BMark(dirty, &r);
                }
            if (v->pty.mode != BUI_PTY_FORCED && (old->pressed == BUI_PTY_CANCEL) != (v->pressed == BUI_PTY_CANCEL)) {
                BPCancelButton(v->pressed == BUI_PTY_CANCEL);
                BMark(dirty, &sBPCancel);
            }
        }
        sUiS = &sUiNavy;
        return;
    }
    if (memcmp(old->party, v->party, sizeof(v->party))) {
        BStrip(v);
        BMark(dirty, &sBStrip);
    }
    if (old->state != v->state
        || (v->state == BUI_BAG && (memcmp(&old->bag, &v->bag, sizeof(v->bag)) || old->pressed != v->pressed))
        || (v->state == BUI_TARGET && memcmp(old->target, v->target, sizeof(v->target)))) {
        BBody(v);
        BMark(dirty, &sBBody);
    } else if (v->state == BUI_TARGET && (old->pressed != v->pressed || old->cursor != v->cursor || old->showCursor != v->showCursor)) {
        for (int i = 0; i < 4; ++i)
            if (BLooksChanged(old, v, i)) {
                Fill(sBCard[i].x, sBCard[i].y, sBCard[i].w, sBCard[i].h, UIS_BG);
                BTargetCard(i, v);
                BMark(dirty, &sBCard[i]);
            }
        if ((old->pressed == BUI_CANCEL) != (v->pressed == BUI_CANCEL)) {
            Fill(sBCancel.x, sBCancel.y, sBCancel.w, sBCancel.h, UIS_BG);
            BCancel(&sBCancel, v->pressed == BUI_CANCEL);
            BMark(dirty, &sBCancel);
        }
    } else if (v->state == BUI_SAFARI && (old->pressed != v->pressed || old->cursor != v->cursor || old->showCursor != v->showCursor)) {
        for (int i = 0; i < 4; ++i)
            if (BLooksChanged(old, v, i)) {
                Fill(sBSafari[i].x, sBSafari[i].y, sBSafari[i].w, sBSafari[i].h, UIS_BG);
                BSafariButton(i, v);
                BMark(dirty, &sBSafari[i]);
            }
    }
    sUiS = &sUiNavy;
}
