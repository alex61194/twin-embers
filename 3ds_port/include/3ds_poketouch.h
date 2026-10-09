#ifndef CTR_POKETOUCH_H
#define CTR_POKETOUCH_H

#include <stdbool.h>
#include <stdint.h>
#include "3ds_dirty_rect.h"
#include "3ds_summary.h"

/*
 * PokeTouch: the bottom screen (320x240). A view over FireRed's own menu
 * and shortcuts: every button maps to the game's START menu entry or key
 * item use (3ds_bottom.c issues them); this module only lays out, draws and
 * hit-tests. The frame on the left holds the viewport, where the GBA picture
 * of the open menu is shown 1:1 (240x160) by the GPU, and the contextual bar
 * under it. Drawing goes into the bottom canvas (RGB565,
 * column-major, each column bottom-to-top) when the view changes.
 */
enum {
    PT_POKEDEX, PT_POKEMON, PT_BAG, PT_CARD, PT_SAVE, PT_OPTIONS, /* side column (the map is HOME, no button) */
    PT_REGISTERED,  /* shortcut 1, X: the registered item (SELECT) */
    PT_BIKE,        /* shortcut 2, Y: the Bicycle */
    PT_RUN,         /* Running Shoes: the running mode on and off (B runs as ever) */
    PT_COUNT,
    PT_CLOSE = PT_COUNT, /* START menu exit pick for the bridge; not a button */
    PT_FLY,              /* the field command that Flies to the selected map section; not a button */
    PT_NONE = 0xff
};

/* What Fly does at a map section (CtrPokeTouch_FlyStatus): not a destination, or
 * one, and why it cannot be used (unvisited, no party member that knows Fly, no
 * badge for it, not outdoors, the field busy), or can. */
enum { PT_FLY_NONE, PT_FLY_UNVISITED, PT_FLY_NO_MON, PT_FLY_NO_BADGE, PT_FLY_NO_PLACE, PT_FLY_BUSY, PT_FLY_OK };

typedef struct { int16_t x, y, w, h; } CtrPokeTouchRect;

/* The viewport: the GBA picture's 240x160, pixel for pixel. */
#define PT_VIEW_X 4
#define PT_VIEW_Y 4
#define PT_VIEW_W 240
#define PT_VIEW_H 160
/* The HOME map: FireRed's own Region Map art, drawn passively over the field's
 * live state (CtrPokeTouch_Map* in the game, patch 0064). The maps are its
 * REGIONMAP_* values. */
enum { PT_MAP_KANTO, PT_MAP_SEVII123, PT_MAP_SEVII45, PT_MAP_SEVII67, PT_MAP_COUNT };
/* HOME shows the Region Map's 192x128 at (24, 28) of its 240x160 screen (the 22x15 grid of 8x8
 * cells at (32, 32) and the 16x16 icons around it) enlarged by exactly 5/4, nearest neighbour,
 * into the viewport: cells are 10x10 from (10, 5), icons 20x20. Only HOME uses this; the other
 * screens keep the viewport 1:1 with the GBA picture. */
#define PT_HOME_SRC_X 24
#define PT_HOME_SRC_Y 28
#define PT_HOME_X 10                    /* viewport pixel of cell (0, 0) */
#define PT_HOME_Y 5
#define PT_HOME_CELL 10
#define PT_HOME_ICON 20
/* The native OPTIONS screen (3ds_poketouch_options.c holds the rows): it fills the viewport with up
 * to PT_OPT_MAX rows, each a label on the left and, on the right, "<" value ">". A row is 228x20 at
 * (PT_VIEW_X + 6, PT_VIEW_Y + 4 + 22 row). On a row, the label selects it (its description goes to
 * the bar), "<" steps the value back and the value or ">" steps it forward. */
#define PT_OPT_MAX 7
#define PT_OPT_LABEL_LEN 16
#define PT_OPT_VALUE_LEN 12
enum { PT_OPT_ZONE_NONE, PT_OPT_ZONE_LABEL, PT_OPT_ZONE_PREV, PT_OPT_ZONE_NEXT };
/* FireRed's own options (the game bridge, ctr_poketouch_options.c), in the order of its menu. */
enum { PT_OPT_TEXT_SPEED, PT_OPT_BATTLE_SCENE, PT_OPT_BATTLE_STYLE, PT_OPT_SOUND, PT_OPT_BUTTON_MODE, PT_OPT_FRAME,
       PT_OPT_FPS, PT_OPT_GAME_COUNT };
/* The SAVE screen (the facts come from FireRed's save window, ctr_poketouch.h CtrSave_*): the whole
 * viewport shows the game's save information, and the contextual bar becomes the dialogue: its two or
 * three text lines on the left and, on the right of the same bar, YES / NO (SAVE_CONFIRM), nothing
 * (SAVE_WRITING) or OK (SAVE_SUCCESS, SAVE_ERROR). */
enum { SAVE_CONFIRM, SAVE_WRITING, SAVE_SUCCESS, SAVE_ERROR };
enum { PT_SAVE_NONE, PT_SAVE_FIRST, PT_SAVE_SECOND };  /* the button: YES or OK, NO */
/* Contextual bar text: three lines of FireRed's font inside 228 pixels. */
#define PT_BAR_LINES 3
#define PT_BAR_LEN 40

typedef struct {
    uint8_t enabled[PT_COUNT];  /* usable now: drawn normally, else dimmed */
    uint8_t pressed;            /* element under a held touch, or PT_NONE */
    uint8_t selected;           /* the active destination, or PT_NONE */
    uint8_t running;            /* the running mode is on (Running Shoes) */
    uint8_t viewport;           /* the GPU shows a GBA picture in the viewport */
    uint8_t home;               /* the HOME map fills the viewport (no GBA picture) */
    uint8_t mapGroup;           /* which map: PT_MAP_KANTO..PT_MAP_SEVII67 */
    uint8_t mapVersion;         /* what else its art depends on (CtrPokeTouch_MapVersion) */
    uint8_t playerFemale;       /* Leaf's map icon rather than Red's */
    int8_t playerX, playerY;    /* the player's cell on the map (22x15), -1: none */
    int8_t cursorX, cursorY;    /* the selected cell (FireRed's cursor icon), -1: none */
    uint8_t cursorFrame;        /* which of the cursor's two blink frames (0 with none) */
    uint8_t options;            /* the OPTIONS screen fills the viewport (no GBA picture, no map) */
    uint8_t optCount;           /* its rows */
    uint8_t optSelected;        /* the row the bar describes */
    int8_t optPressed;          /* the row under a held touch, or -1, and where on it: PT_OPT_ZONE_* */
    uint8_t optZone;
    char optLabel[PT_OPT_MAX][PT_OPT_LABEL_LEN];
    char optValue[PT_OPT_MAX][PT_OPT_VALUE_LEN];
    uint8_t save;               /* the SAVE screen fills the viewport (no GBA picture, no map) */
    uint8_t saveState;          /* SAVE_* */
    uint8_t saveSel;            /* the selected button: 0 YES, 1 NO */
    uint8_t savePressed;        /* PT_SAVE_* under a held touch */
    uint8_t saveBadges;         /* badges earned, 0-8 */
    uint8_t saveHasDex;         /* the Pokedex line is shown */
    uint8_t saveMinutes;        /* the play clock */
    uint16_t saveHours;
    uint16_t saveDex;           /* Pokemon caught */
    uint32_t saveMoney;
    uint8_t backVisible;        /* the contextual BACK control is on the bar (a screen with no touch exit of its own) */
    uint8_t backEnabled;        /* it can be used now: drawn normally, else dimmed */
    uint8_t backPressed;        /* it is under a held touch */
    uint16_t registeredItem;    /* 0: nothing registered */
    uint16_t bikeItem;          /* the Bicycle's item id, 0 when not owned */
    char bar[PT_BAR_LINES][PT_BAR_LEN]; /* contextual text, "" for no line */
    char playerName[16];        /* the Trainer Card button's label (UTF-8) */
} CtrPokeTouchView;

extern const CtrPokeTouchRect gCtrPokeTouchRects[PT_COUNT];
/* BACK: a shell control in the bar's bottom-right corner, outside the viewport. It
 * stands for the B button of the open screen (3ds_bottom.c sends the real B). */
extern const CtrPokeTouchRect gCtrPokeTouchBack;
/* Whether a bottom-screen pixel is on BACK's rectangle. */
bool CtrPokeTouch_HitBack(int x, int y);

/* The OPTIONS row and zone (PT_OPT_ZONE_*) under a bottom-screen pixel; false when none. */
bool CtrPokeTouch_OptionHit(int x, int y, int rows, int *row, int *zone);

/* SAVE's button under a bottom-screen pixel (PT_SAVE_*) in the bar for the dialogue state: YES and NO
 * while confirming, OK (PT_SAVE_FIRST) after a result, none while writing. */
int CtrPokeTouch_SaveHit(int x, int y, int state);

/* PT_* under a bottom-screen pixel, or PT_NONE. */
int CtrPokeTouch_HitTest(int x, int y);
/* A bottom-screen pixel inside the viewport as GBA coordinates; false
 * outside the viewport. */
bool CtrPokeTouch_ViewportToGba(int x, int y, int *gx, int *gy);
/* Draw the whole screen for `view` into the bottom canvas. */
void CtrPokeTouch_Draw(uint16_t *canvas, const CtrPokeTouchView *view);
/* The canvas holds `old` drawn: redraw only the elements that look different
 * in `view` (a press, a state, the bar). Each element draws only inside its
 * rectangle, and that rectangle (a changed map icon: its old and new place)
 * is added to `dirty` to be presented; nothing is added when nothing changed. */
void CtrPokeTouch_Update(uint16_t *canvas, const CtrPokeTouchView *old, const CtrPokeTouchView *view,
                         CtrDirtyList *dirty);

/*
 * The battle screen (3ds_battle_ui.inc, CtrBattleUi_*): the whole lower screen while FireRed's battle is on the
 * upper one. While the player's controller waits at the action menu it is the MAIN design (FIGHT large, BAG, RUN
 * and POKeMON below, the party's six Poke Balls above); at the move list, the four move cards (name, type,
 * description and power of the real moves) and CANCEL; at any other moment only the frame and the balls.
 * Everything shown is what the controller reports: the screen has no state of its own. In the battle Bag (BUI_BAG, the
 * Bag FireRed opens from BAG) it is a remote for that Bag: its pockets as tabs, the pocket's items with icon and quantity,
 * the selected item's detail, and CANCEL; what it shows is the Bag's own state and save data. In the battle Party (BUI_PARTY:
 * a switch, the replacement for a Pokemon that fainted, or the target of an item) it is a remote for FireRed's Party: the
 * six Pokemon as panels in two columns (sprite, name, level, HP bar and numbers, status), its menu or message when it has one
 * open, and CANCEL unless the Party cannot be cancelled.
 */
enum { BUI_IDLE = 0, BUI_ACTION, BUI_MOVE, BUI_BAG, BUI_PARTY, BUI_TARGET, BUI_SAFARI };
/* Buttons (CtrBattleUi_Hit): the controller's action numbers while choosing an action, the move slots 0-3 and
 * BUI_CANCEL while choosing a move; in the Bag the codes below. */
enum { BUI_ACT_FIGHT = 0, BUI_ACT_BAG = 1, BUI_ACT_POKEMON = 2, BUI_ACT_RUN = 3, BUI_CANCEL = 4 };
#define BUI_NAME_LEN 18
#define BUI_DESC_LEN 112
typedef struct {
    uint8_t present;                /* a real move in the slot */
    uint8_t type;                   /* FireRed's type number (the card's colours) */
    char name[BUI_NAME_LEN];
    char typeName[10];
    char power[5];                  /* "90", or "-" for a move with no power */
    uint8_t pp, maxPp;              /* what the controller's own move list holds (CtrBattle_MovePP) */
    char desc[BUI_DESC_LEN];        /* FireRed's description, wrapped when drawn */
} CtrBattleMove;
enum { BUI_BAG_TAB = 100, BUI_BAG_ROW = 200, BUI_BAG_MENU = 300, BUI_BAG_CANCEL = 400, BUI_BAG_MESSAGE = 500,
       BUI_BAG_UP = 600, BUI_BAG_DOWN = 601 };
#define BUI_BAG_ROWS 5
#define BUI_ITEM_LEN 20
typedef struct {
    uint8_t pocket;                 /* FireRed's pocket: 0 Items, 1 Key Items, 2 Poke Balls */
    uint8_t bagState;               /* the Bag's own input state (CTR_BAG_*: list, menu, message, busy ...) */
    int16_t count;                  /* items in the pocket */
    int16_t cursor;                 /* the selected item (count is the list's own CANCEL row) */
    int16_t first;                  /* the item of the first row shown */
    struct {
        uint16_t item;              /* 0: no item in the row */
        uint16_t qty;
        char name[BUI_ITEM_LEN];
    } row[BUI_BAG_ROWS];
    uint16_t selItem;               /* the selected item, its name and description */
    char selName[BUI_ITEM_LEN];
    char selDesc[BUI_DESC_LEN];
    uint8_t menuCount;              /* the open context menu's rows (USE, CANCEL ...) */
    int8_t menuCursor;
    char menuName[3][12];
    char message[96];               /* what the Bag printed and waits on */
} CtrBattleBag;
enum { BUI_PTY_SLOT = 700, BUI_PTY_CANCEL = 710, BUI_PTY_MENU = 720, BUI_PTY_MESSAGE = 730 };
enum { BUI_PTY_SWITCH = 0, BUI_PTY_TARGET, BUI_PTY_FORCED };
enum { BUI_PTY_NONE = 0, BUI_PTY_ACTIONS, BUI_PTY_TEXT, BUI_PTY_YESNO };
typedef struct {
    uint8_t present[6];             /* a Pokemon in the slot */
    uint8_t egg[6];
    uint8_t status[6];              /* GetMonAilment: 1 PSN, 2 PAR, 3 SLP, 4 FRZ, 5 BRN, 6 PKRS */
    uint8_t level[6];
    uint16_t hp[6], maxHp[6];
    uint16_t icon[6];               /* FireRed's icon species of the slot (the icon is read through the bridge) */
    char name[6][11];
    int8_t cursor;                  /* the Party's own selection: a slot 0-5 */
    uint8_t mode;                   /* BUI_PTY_SWITCH, BUI_PTY_TARGET (an item's target), BUI_PTY_FORCED (no CANCEL) */
    uint8_t panel;                  /* BUI_PTY_*: what the Party has open over the slots */
    uint8_t menuCount;              /* its menu's rows (SHIFT, SUMMARY, CANCEL; YES, NO) */
    int8_t menuCursor;
    char menuName[6][14];
    char message[96];
} CtrBattleParty;
/* BUI_TARGET: a double battle's move being aimed. The four cards are the opposing battlers on top and the player's
 * below (left, right); only the ones FireRed lets the move be aimed at are `present`. The hit codes are the card
 * (0-3) or BUI_CANCEL; `cursor` is the card of the controller's own target cursor. BUI_SAFARI: BALL, BAIT, ROCK,
 * RUN, FireRed's own four, in the controller's 2x2 order (codes 0-3, `cursor` its cursor). */
typedef struct {
    uint8_t present;
    int8_t battler;                 /* the battle's battler number: what the controller's cursor is on */
    uint8_t side;                   /* 0 opposing, 1 the player's */
    uint8_t level;
    uint16_t hp, maxHp;
    char name[BUI_NAME_LEN];
} CtrBattleTarget;
typedef struct {
    uint8_t state;                  /* BUI_* */
    int16_t pressed;                /* the button held under a touch, or -1 */
    int8_t cursor;                  /* the controller's own cursor, drawn only while showCursor (the last input was a button) */
    uint8_t showCursor;
    uint8_t party[6];               /* 0 no Pokemon, 1 able, 2 fainted */
    CtrBattleMove move[4];
    CtrBattleBag bag;
    CtrBattleParty pty;
    CtrBattleTarget target[4];
} CtrBattleView;

void CtrBattleUi_Draw(uint16_t *canvas, const CtrBattleView *view);
/* The canvas holds `old` drawn: redraw only what differs in `view` and mark its dirty rectangles. */
void CtrBattleUi_Update(uint16_t *canvas, const CtrBattleView *old, const CtrBattleView *view, CtrDirtyList *dirty);
/* FireRed's pocket a battle Bag tab (0-2, left to right) shows. */
int CtrBattleUi_TabPocket(int tab);
/* The button under a canvas point of the screen `view` shows, or -1. */
int CtrBattleUi_Hit(const CtrBattleView *view, int x, int y);


/*
 * The Pokemon Summary (3ds_summary_ui.inc, CtrSummaryUi_*): the whole lower screen while FireRed's Summary of the party
 * runs hidden behind it, from the field's Party or the battle's (the same screen for both). It shows what the Summary
 * reports (CtrSummaryView) in the premium design: a header with the Pokemon (icon, nickname, gender, number, species,
 * level) and the buttons for the previous and next one, the tabs INFO / STATS / MOVES / TRAINER, the page and a footer
 * with BACK. INFO, STATS and MOVES are FireRed's three pages (its move details are MOVES with a move selected);
 * TRAINER is the trainer memo of its first page, shown on its own.
 */
enum { SUI_TAB_INFO = 0, SUI_TAB_STATS, SUI_TAB_MOVES, SUI_TAB_TRAINER, SUI_TABS };
/* Hit codes (CtrSummaryUi_Hit): the tabs, the previous / next Pokemon, BACK, a move card or row, the details' CANCEL. */
enum { SUI_HIT_TAB = 0, SUI_HIT_PREV = 10, SUI_HIT_NEXT = 11, SUI_HIT_BACK = 12, SUI_HIT_MOVE = 20, SUI_HIT_CANCEL = 24 };
/* FireRed's Summary pages (pokemon_summary_screen.h PSS_PAGE_*). */
enum { SUI_PAGE_INFO = 0, SUI_PAGE_SKILLS, SUI_PAGE_MOVES, SUI_PAGE_MOVES_INFO };
typedef struct {
    CtrSummaryView data;            /* what the Summary reports (the last complete report while it loads a Pokemon) */
    uint16_t icon;                  /* FireRed's icon species of the Pokemon shown (the icon cache's key) */
    int16_t pressed;                /* the hit code under a held touch, or -1 */
    uint8_t trainer;                /* TRAINER is shown (FireRed is on its first page) */
    uint8_t loading;                /* nothing reported yet: the frame only */
} CtrSummaryUiView;

void CtrSummaryUi_Draw(uint16_t *canvas, const CtrSummaryUiView *view);
/* The canvas holds `old` drawn: redraw only the parts that differ in `view` and mark their dirty rectangles. */
void CtrSummaryUi_Update(uint16_t *canvas, const CtrSummaryUiView *old, const CtrSummaryUiView *view, CtrDirtyList *dirty);
/* The hit code under a canvas point of the screen `view` shows, or -1 (a disabled control is not hit). */
int CtrSummaryUi_Hit(const CtrSummaryUiView *view, int x, int y);
/* The tab `view` shows (SUI_TAB_*). */
int CtrSummaryUi_Tab(const CtrSummaryUiView *view);

#endif
