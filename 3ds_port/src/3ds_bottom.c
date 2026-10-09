/* Bottom screen: PokeTouch (3ds_poketouch.c) at home and around FireRed's own
 * Party and Bag screens, shown in its viewport by the GPU, over the real
 * FireRed state.
 *
 * Architecture (Zallax 3ds_bottom_ui.c as reference, scoped to this milestone):
 * - FireRed is authoritative; this is a lower-screen view/controller.
 * - No Party/Bag rules duplicated: a touch in the viewport is mapped back to
 *   the GBA picture (0.85 inverse) and FireRed's own windows say what is
 *   there: party slots enter the original Party input handler, menu rows
 *   walk the live menu cursor and confirm with A.
 * - Physical controls always win and cancel any running Plan.
 * - The field Bag is the same: the Bag's own windows say what a touch hits
 *   and its own input states take the presses (CtrBottomBag_*).
 * - So is the Summary (CtrBottomSummary_*): its header and move rows say what
 *   a touch hits, and pages turn through its own LEFT/RIGHT page flips.
 * - The TM Case and Berry Pouch the Bag (or Party's GIVE Bag) opens are child
 *   screens of that session, their lists driven as the Bag's
 *   (CtrBottomTMCase_*, CtrBottomPouch_*).
 * - BACK, a shell control on the bar, is the open screen's B button for the
 *   screens with no touch exit of their own (the Bag and its cases, the Summary
 *   past its first page): it sends the real B through the screen's own plan, so
 *   FireRed decides where that leads, and only while B would do something.
 * - The field's lower screen, HOME, is FireRed's own Region Map art drawn
 *   passively (3ds_poketouch.c, CtrPokeTouch_Map* in region_map.c): the Region
 *   Map never runs, TOP is live and nothing is held.
 * - While Party or the Bag is open TOP holds its previous field/battle frame
 *   (CtrVideo_HoldTop); normal Summary shares the hold and is shown in the
 *   viewport like Party, while the Fly map releases TOP after a short grace
 *   so it remains visible, and Party resumes on return.
 * - Redraws only when the snapshot changes; HID is never polled here.
 */
#include <stdio.h>
#include <string.h>

#include "3ds_bottom.h"
#include "3ds_battle_nav.h"
#include "3ds_storage_touch.h"
#include "3ds_storage_bridge.h"
#include "3ds_input.h"
#include "3ds_video.h"
#include "3ds_log.h"
#include "3ds_summary.h"
#include "3ds_poketouch.h"
#include "3ds_poketouch_options.h"
#include "3ds_perf.h"

int CtrBattleAction_Next(int cursor, int dir)
{
    return CtrBattleActionStep(cursor, dir);
}

/* Game bridge implemented by the FireRed patch (src/ctr_bottom_party.c).
 * Port code never includes game headers; these are the only game facts used. */
extern int CtrBottomParty_IsOpen(void);
extern void *CtrBottomParty_Callback(void);
extern void *CtrBottom_CurrentCallback(void);
extern int CtrBottom_IsOverworld(void);
extern int CtrBottom_IsFieldCallback(void);
extern int CtrBottom_IsFieldTransition(void);
extern int CtrBottom_IsBattleMain(void);
extern int CtrBottom_IsFading(void);
extern unsigned char StorageGetCurrentBox(void);
extern int CtrBottom_HasSave(void);
extern int CtrBottomParty_Slot(void);
extern int CtrBottomParty_Count(void);
extern int CtrBottomParty_Layout(void);
extern int CtrBottomParty_SlotEmpty(int slot);
extern unsigned CtrBottomParty_HP(int slot);
extern unsigned CtrBottomParty_MaxHP(int slot);
extern unsigned CtrBottomParty_Status(int slot);
extern unsigned CtrBottomParty_Species(int slot);
extern unsigned CtrBottomParty_Level(int slot);
extern unsigned CtrBottomParty_Gender(int slot);
extern unsigned CtrBottomParty_IsEgg(int slot);
extern void CtrBottomParty_Nickname(int slot, char *out);
extern unsigned CtrBottomParty_IconSpecies(int slot);
extern int CtrBottomParty_Icon(int slot, unsigned char *tilesOut, unsigned short *palOut);
extern int CtrField_TryOpenParty(void);
extern int CtrBottomParty_IsPartyCallback(void);
extern int CtrBottomParty_SelectionOpen(void);
extern int CtrBottomParty_ActionCount(void);
extern int CtrBottomParty_ActionId(int index);
extern void CtrBottomParty_ActionName(int index, char *out);
extern int CtrBottomParty_Cursor(void);
extern int CtrBottomParty_YesNoOpen(void);
extern int CtrBottomParty_Message(char *out);
extern int CtrBottomParty_InputReady(void);
extern int CtrBottomParty_TouchSlot(int slot);
extern int CtrBottomParty_ChooseMultiple(void);
extern int CtrBottomParty_CanAdvanceText(void);
extern unsigned CtrBottomParty_MenuSerial(void);
extern void CtrBottomParty_Description(int slot, char *out);
extern int CtrBattle_InBattle(void);
extern int CtrBattle_State(void);
extern int CtrBattle_ActionCursor(void);
extern int CtrBattle_MoveCursor(void);
extern int CtrBattle_TargetCursor(void);
extern int CtrBattle_TargetLegal(int target);
extern int CtrBattle_TargetPosition(int target);
extern int CtrBattle_TargetCount(void);
extern int CtrBattle_TargetInfo(int target, char *name, int size, int *level, int *hp, int *maxHp);
extern int CtrBattle_BagPending(void);
extern int CtrBattle_PartyPending(void);
extern int CtrBottomParty_InBattle(void);
extern int CtrBottomParty_Action(void);
extern int CtrBottomBag_InBattle(void);
extern int CtrBottomBag_Pocket(void);
extern unsigned CtrBottomBag_Quantity(int index);
extern void CtrBottomBag_ItemDescription(unsigned item, char *out, int size);
extern int CtrBottomBag_Message(char *out, int size);
extern int CtrBottomBag_ActionCount(void);
extern void CtrBottomBag_ActionName(int index, char *out, int size);
extern unsigned CtrBattle_Move(int slot);
extern void CtrBattle_MoveName(unsigned move, char *out, int size);
extern void CtrBattle_MoveDescription(unsigned move, char *out, int size);
extern int CtrBattle_MoveType(unsigned move);
extern void CtrBattle_TypeName(int type, char *out, int size);
extern unsigned CtrBattle_MovePower(unsigned move);
extern int CtrBattle_MovePP(int slot, int *pp, int *maxPp);
extern int CtrPokeTouch_PlayerFemale(void);
extern int CtrPokeTouch_Enabled(int entry);
extern int CtrPokeTouch_StartMenuOpen(void);
extern int CtrPokeTouch_Pick(int entry);
extern void CtrPokeTouch_RequestFieldCommand(unsigned entry);
extern unsigned CtrPokeTouch_RegisteredItem(void);
extern unsigned CtrPokeTouch_BikeItem(void);
extern int CtrPokeTouch_ToggleRun(void);
extern int CtrPokeTouch_RunModeShown(void);
extern void CtrPokeTouch_PlayerName(char *out, int size);
extern int CtrPokeTouch_LeavingFor(int entry);
extern void CtrPokeTouch_Location(char *out, int size);
extern int CtrPokeTouch_MapGroup(void);
extern int CtrPokeTouch_MapVersion(void);
extern int CtrPokeTouch_MapPlayer(int *x, int *y, int *mapsec);
extern int CtrPokeTouch_MapSection(int group, int x, int y, int *mapsec, int *dungeon);
extern void CtrPokeTouch_MapSectionName(unsigned mapsec, char *out, int size);
extern int CtrPokeTouch_FlyStatus(unsigned mapsec);
extern void CtrPokeTouch_RequestFly(unsigned mapsec);
extern int CtrBottomParty_HitAt(int gx, int gy);
extern int CtrField_TryOpenBag(void);
extern int CtrBottomBag_IsOpen(void);
extern int CtrBottomBag_FromParty(void);
extern int CtrBottomBag_State(void);
extern int CtrBottomBag_Count(void);
extern int CtrBottomBag_MaxShowed(void);
extern int CtrBottomBag_Cursor(void);
extern int CtrBottomBag_MenuCursor(void);
extern unsigned CtrBottomBag_Serial(void);
extern unsigned CtrBottomBag_Item(int index);
extern void CtrBottomBag_PocketName(char *out, int size);
extern void CtrBottomBag_ItemName(unsigned item, char *out, int size);
extern unsigned CtrBottomBag_Money(void);
extern int CtrBottomBag_HitAt(int gx, int gy);
extern int CtrBottomTMCase_IsOpen(void);
extern int CtrBottomTMCase_State(void);
extern int CtrBottomTMCase_Count(void);
extern int CtrBottomTMCase_MaxShowed(void);
extern int CtrBottomTMCase_Cursor(void);
extern unsigned CtrBottomTMCase_Serial(void);
extern unsigned CtrBottomTMCase_Item(int index);
extern void CtrBottomTMCase_Title(char *out, int size);
extern void CtrBottomTMCase_MoveName(unsigned item, char *out, int size);
extern int CtrBottomTMCase_HitAt(int gx, int gy);
extern int CtrBottomPouch_IsOpen(void);
extern int CtrBottomPouch_State(void);
extern int CtrBottomPouch_Count(void);
extern int CtrBottomPouch_MaxShowed(void);
extern int CtrBottomPouch_Cursor(void);
extern unsigned CtrBottomPouch_Serial(void);
extern unsigned CtrBottomPouch_Item(int index);
extern unsigned CtrBottomPouch_Quantity(int index);
extern void CtrBottomPouch_Title(char *out, int size);
extern int CtrBottomPouch_HitAt(int gx, int gy);
extern int CtrSave_Begin(void);
extern void CtrSave_End(void);
extern int CtrSave_IsDifferentFile(void);
extern int CtrSave_Run(void);
extern unsigned CtrSave_BadgeCount(void);
extern int CtrSave_HasPokedex(void);
extern unsigned CtrSave_DexCaught(void);
extern unsigned CtrSave_PlayHours(void);
extern unsigned CtrSave_PlayMinutes(void);
extern unsigned CtrSave_Money(void);
extern int CtrField_TryOpenCard(void);
extern int CtrBottomCard_IsOpen(void);
extern int CtrBottomCard_State(void);
extern int CtrField_TryOpenDex(void);
extern int CtrTurbo_FieldStable(void);
extern int CtrTurbo_BattleStable(void);
extern int CtrTurbo_CanCycle(void);
extern int CtrBottomDex_IsOpen(void);
extern int CtrBottomDex_State(void);
extern int CtrBottomDex_Cursor(void);
extern int CtrBottomDex_RowAt(int x, int y);
extern int CtrBottomDex_ArrowAt(int x, int y);
extern int CtrBottomDex_PageSlotAt(int x, int y);
extern int CtrBottomDex_PageSlot(void);
extern int CtrBottomDex_Page(void);
extern int CtrBottomDex_PageCanFlip(int direction);

/* CtrBottomDex_State (pokedex_screen.c): the real Pokedex is not taking input (set up, fading, flipping,
 * closing), or takes it on its top menu or a list (rows to choose), a category page or a Pokemon's page. B
 * goes one level back from all of them. */
enum { DEX_BUSY = 0, DEX_MENU, DEX_LIST, DEX_CATEGORY, DEX_PAGE };

/* CtrBottomCard_State (trainer_card.c): the real Trainer Card is not taking input (set up, fading,
 * flipping, closing), takes it on its front (A flips it, B closes it) or on its back (B flips it back,
 * A closes it). */
enum { CARD_BUSY = 0, CARD_FRONT, CARD_BACK };

#define PARTY_SIZE 6
#define SLOT_NONE -1

/* Modes shown on the bottom screen for this milestone. */
enum { MODE_OFF = 0, MODE_FIELD = 1, MODE_PARTY = 2, MODE_SUMMARY = 3, MODE_BAG = 4, MODE_TMCASE = 5,
       MODE_POUCH = 6, MODE_CARD = 7, MODE_DEX = 8, MODE_BATTLE = 9,
       MODE_STORAGE_PC = 10, MODE_STORAGE_BOX = 11, MODE_STORAGE_CHILD = 12, MODE_PARTY_CHILD = 13 };

/* CtrBattle_State (battle_controller_player.c): the player's controller waits at the action menu, at the move
 * list, or at neither (messages, animations, a command taken: not ready). */
enum { BATTLE_BUSY = 0, BATTLE_ACTION, BATTLE_MOVE, BATTLE_TARGET, BATTLE_SAFARI };

/* The Bag's child cases: the TM Case and the Berry Pouch. */
#define IS_CASE_MODE(mode) ((mode) == MODE_TMCASE || (mode) == MODE_POUCH)

/* A list screen (the Bag, the cases it opens): the same questions, answered
 * by its own bridge, drive its touch plan. */
typedef struct {
    int (*state)(void);
    int (*count)(void);
    int (*maxShowed)(void);
    int (*cursor)(void);
    unsigned (*serial)(void);
    int (*hitAt)(int gx, int gy);
} ListScreen;

/* CtrBottomBag_State and CtrBottomBag_HitAt (item_menu.c), and the TM Case's. */
enum { BAG_BUSY = 0, BAG_LIST, BAG_ACTIONS, BAG_YESNO, BAG_QUANTITY, BAG_MESSAGE };
#define BAG_HIT_MENU 0x100
#define BAG_HIT_POCKET_PREV 0x200
#define BAG_HIT_POCKET_NEXT 0x201
#define BAG_HIT_SCROLL_UP 0x202
#define BAG_HIT_SCROLL_DOWN 0x203
#define BAG_HIT_QTY_UP 0x204
#define BAG_HIT_QTY_DOWN 0x205
#define BAG_HIT_QTY_OK 0x206

/* CtrBottomParty_HitAt: a Party slot (0-5, 6 confirm, 7 cancel), or a row of
 * the open selection / yes-no menu from PARTY_HIT_MENU. */
#define PARTY_HIT_MENU 0x10

/* Hit zones. */
enum { HIT_NONE = 0xFF, HIT_SLOT = 0x20, HIT_POKEMON = 0x40, HIT_ACTION = 0x60, HIT_YES = 0x70, HIT_NO = 0x71, HIT_PANEL = 0x50, HIT_BACK = 0x51, HIT_CONFIRM = 0x52 };

/* Lower-panel states inside the Party session. ACTIONS mirrors the live
 * game selection menu; YESNO mirrors its yes/no prompt; MESSAGE shows the
 * live Party message while its text printer runs; HINT is transient. */
enum { PANEL_HINT = 0, PANEL_ACTIONS, PANEL_MESSAGE, PANEL_YESNO };

#define PARTY_MAX_ACTIONS 8
#define ACTION_NAME_LEN 16
#define MESSAGE_LEN 256

/* Plan: injected buttons driving the real Party state machine. PARTY walks
 * the card cursor, MENU walks a selection/yes-no menu cursor, PRESS issues
 * one guarded keypress (message advance). */
enum { PLAN_NONE = 0, PLAN_PARTY = 1, PLAN_MENU = 2, PLAN_PRESS = 3 };

typedef struct {
    int kind;
    int target;
    int steps;
    int wait;
    int release;
    uint16_t keys;
    int menuKind;
    int count;
    int slot;
    int actionIds[8];
    unsigned serial;
} Plan;

typedef struct {
    unsigned char mode;
    unsigned char partyCount;
    signed char cursor;
    signed char tapped;
    unsigned char presentMask;
    unsigned char status[PARTY_SIZE];
    unsigned char level[PARTY_SIZE];
    unsigned char gender[PARTY_SIZE];
    unsigned char isEgg[PARTY_SIZE];
    uint16_t hp[PARTY_SIZE];
    uint16_t maxHp[PARTY_SIZE];
    uint16_t icon[PARTY_SIZE];
    char nick[PARTY_SIZE][11];
    char description[PARTY_SIZE][24];
    unsigned char pressed;
    unsigned char panel;
    unsigned char actionCount;
    signed char actionCursor;
    unsigned char actionIds[PARTY_MAX_ACTIONS];
    char actionNames[PARTY_MAX_ACTIONS][ACTION_NAME_LEN];
    char message[MESSAGE_LEN];
    unsigned char chooseMultiple;
    unsigned char canChoose;
    unsigned char canAdvance;
    unsigned serial;
    CtrSummaryView summary;         /* a field Summary: FireRed's own, in the viewport */
    CtrSummaryUiView sum;           /* a battle Summary: its own screen (3ds_summary_ui.inc) */
    unsigned char summaryUi;        /* it is what the lower screen shows */
    CtrPokeTouchView touch;
    CtrBattleView battle;
    unsigned char battleUi;         /* the battle's design is what the lower screen shows (the battle, or its Bag) */
} Snapshot;

static void SummaryFill(CtrSummaryUiView *v);
static int NavPending(int mode);

/* The premium Summary is the battle's alone: a Summary opened from the field's Party stays FireRed's own. */
static int SummaryPremium(void)
{
    return CtrBottomSummary_Origin() == CTR_SUMMARY_BATTLE_PARTY;
}

/* Latched action list: kept while the selection menu is closed so the panel
 * stays stable across message/yes-no substates; refreshed while open. */
static unsigned char sLatchCount;
static unsigned char sLatchIds[PARTY_MAX_ACTIONS];
static char sLatchNames[PARTY_MAX_ACTIONS][ACTION_NAME_LEN];

/* Canvas in framebuffer layout: column-major, each column bottom-to-top. */
static uint16_t sCanvas[CTR_BOTTOM_WIDTH * CTR_BOTTOM_HEIGHT];
static Snapshot sShown;
static int sHaveShown;
static int sForceRedraw;
static int sEnabled = 1;
static int sInitDone;

/* Session: TOP hold across the hidden Party or Bag menu; `owner` is the
 * PokeTouch entry that opened it (PT_POKEMON or PT_BAG). */
static struct {
    int owner;
    int active;
    int battle;                    /* battle-owned menus and their Summary cannot route to field roots */
    int entered;
    void *partyCallback;
    int frames;
    int away;
} sSession;

/* The one limit of every wait between two screens of a lower-screen session: a Party that sets up, a Summary that loads,
 * the battle that comes back. The session's own grace and what is shown meanwhile (the last picture) end together. */
#define BRIDGE_MAX 90

/* Destination router (below). sScreen is the logical PokeTouch root being
 * shown: PT_POKEMON or PT_BAG while a lower-screen session is active, else
 * PT_NONE (HOME). sPendingScreen is the root the user tapped but that has not
 * been entered yet. Both leave sSession.owner alone to describe who owns the
 * hidden FireRed flow: a child Party of the Bag stays the Bag's until a tap
 * on the other root changes it. While a route is in flight (sRouteHold) the
 * last presentation stays on the bottom screen and TOP stays held. */
#define ROUTE_COOLDOWN 30       /* frames before another B on the same screen */
#define ROUTE_OPEN_TRIES 150    /* frames the field may take to accept the open */
#define ROUTE_TIMEOUT 900       /* frames for the whole unwind */
#define ROUTE_STABLE 8          /* frames the new root must stay ready before it is shown */
static int sScreen = PT_NONE;
static int sPendingScreen = PT_NONE;
static int sRouteHold;
static int sRouteFrames, sRouteWait, sRouteMode, sRouteTries, sRouteReady;
static uint16_t sRouteKey;     /* the route's own B, sent after the plans ran */
static int CanRouteDestination(int target, int mode);
enum { DEXPLAN_LIST = 0, DEXPLAN_SLOT, DEXPLAN_FLIP };
static struct {
    int active, target, state, steps, release;
    int kind, page;                 /* DEXPLAN_*; the habitat page a flip began on */
} sDexPlan;

/* Storage touch belongs to the screen the finger first pressed. The source
 * cursor/state is queried afresh for every step, never mirrored in the port.
 * An animation or the game's own menu must finish before another key. */
enum { ST_PLAN_NONE, ST_PLAN_PC, ST_PLAN_BOX, ST_PLAN_CONTEXT };
static struct {
    int active, mode, hit, inside, context;
} sStoreTouch;
static struct {
    int kind, targetArea, targetPos, postDir, target, steps, release, wait, context;
} sStorePlan;
static void StorePlanClear(void) { memset(&sStorePlan, 0, sizeof(sStorePlan)); }

static void BeginSessionPending(int owner);
static void OpenSave(void);

/* Native OPTIONS (3ds_poketouch_options.c): a PokeTouch screen of its own over
 * the live field, with no hidden FireRed menu behind it, so no session. sScreen
 * is PT_OPTIONS while it is up (a route that unwinds a real menu to the field
 * sets it there, and the session's end leaves it). `sel` is the row the bar
 * describes; `down` a touch that began on a row (`row`, `zone`), `inside` while
 * it is still on it. */
static struct {
    int sel;
    int down, row, zone, inside;
} sOpt;
/* The native roots: PokeTouch screens over the live field with no real menu
 * behind them. (The TRAINER CARD and the POKeDEX are real FireRed screens, sessions
 * like the Bag's.) */
static int NativeRoot(int screen)
{
    return screen == PT_OPTIONS || screen == PT_SAVE;
}
static int NativeActive(void)
{
    return NativeRoot(sScreen) && !sSession.active;
}
/* SAVE (3ds_poketouch.c SaveDraw): a native root that also freezes and locks the field while it is up,
 * as FireRed's save dialogue does (CtrSave_Begin / CtrSave_End), and keeps TOP on the frozen frame.
 * `state` is SAVE_CONFIRM, SAVE_WRITING, SAVE_SUCCESS or SAVE_ERROR; `sel` the selected button (0 YES,
 * 1 NO); `down`/`inside` a touch that began on a button (PT_SAVE_*), and while it is still on it. */
static struct {
    int state, sel;
    int down, inside, wait;
} sSave;
static int SaveActive(void)
{
    return sScreen == PT_SAVE && !sSession.active;
}
static int OptionsActive(void)
{
    return sScreen == PT_OPTIONS && !sSession.active;
}
static const char *RootName(int target)
{
    return target == PT_BAG ? "Bag" : target == PT_OPTIONS ? "Options" : target == PT_CARD ? "Card"
         : target == PT_SAVE ? "Save" : target == PT_POKEDEX ? "Pokedex" : "Party";
}

/* BACK (below): a touch that began on it, and whether it is still on it. */
static struct {
    int down;
    int inside;
} sBack;
static int BackVisible(int mode);
static int BackEnabled(int mode);
static int FieldEnabled(int id);

/* Plan + injected keys. */
static Plan sPlan;
static uint16_t sInjected;
static int sHeldTop;
/* The PC owns TOP from its first menu until the field returns; Summary, Bag
 * and box naming temporarily leave its callbacks without ending its session. */
static int sStorageSession;

/* Touch + hits. */
static struct {
    int active;
} sTouch;
static int sTapped = SLOT_NONE;

/* PokeTouch press: the element under the touch-down, acted on once when the
 * touch is released over it (released elsewhere: nothing). */
static struct {
    uint8_t id;
    uint8_t inside;
} sPoke = {PT_NONE, 0};

/* Colors RGB565. */
#define RGB565(r, g, b) ((uint16_t)(((r) & 31) << 11 | ((g) & 63) << 5 | ((b) & 31)))
#define C_BG RGB565(4, 8, 12)
#define C_CARD RGB565(28, 60, 28)
#define C_EMPTY RGB565(10, 20, 14)
#define C_BORDER RGB565(0, 0, 0)
#define C_SEL RGB565(31, 63, 0)
#define C_TEXT RGB565(0, 0, 0)
#define C_TITLE RGB565(31, 63, 31)
#define C_HP_OK RGB565(0, 63, 0)
#define C_HP_LOW RGB565(31, 63, 0)
#define C_HP_CRIT RGB565(31, 0, 0)
#define C_HP_EMPTY RGB565(8, 16, 8)
#define C_STATUS RGB565(31, 32, 0)

/* GBA key bits (match CTR_KEY_* low 10 bits). */
#define K_START (1u << 3)
#define K_A (1u << 0)
#define K_B (1u << 1)
#define K_UP (1u << 6)
#define K_DOWN (1u << 7)
#define K_LEFT (1u << 5)
#define K_RIGHT (1u << 4)

static void FillRect(int x, int y, int w, int h, uint16_t c)
{
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w > CTR_BOTTOM_WIDTH ? CTR_BOTTOM_WIDTH : x + w;
    int y1 = y + h > CTR_BOTTOM_HEIGHT ? CTR_BOTTOM_HEIGHT : y + h;
    for (int cx = x0; cx < x1; ++cx) {
        uint16_t *p = sCanvas + (size_t)cx * CTR_BOTTOM_HEIGHT + (CTR_BOTTOM_HEIGHT - (size_t)y1);
        for (int n = y1 - y0; n > 0; --n)
            *p++ = c;
    }
}

/* Party-owned callback remembered while Task_HandleChooseMonInput runs.
 * The action/context menu, switch, item and field-move substates run under
 * the same Party CB2 with a different task, so bottom stays PARTY while the
 * callback matches even when IsOpen is false. Cleared only on true exit. */
static void *sPartyMenuCallback;

extern int CtrBottomParty_UsesTopPresentation(void);
extern int CtrBottomPartyChild_IsOpen(void);
extern int CtrBottomPartyChild_IsMovePanel(void);
extern int CtrBottomSummary_IsMoveSelection(void);

static int CurrentMode(void)
{
    if (!sEnabled || !sInitDone)
        return MODE_OFF;
    if (!CtrBottom_HasSave())
        return MODE_OFF;
    if (CtrBottomStorage_PCMenu())
        return MODE_FIELD; /* Withdraw/Deposit/Move choices remain on TOP. */
    if (CtrBottomStorage_BoxOpen())
        return MODE_STORAGE_BOX;
    if (CtrBottomSummary_IsMoveSelection() || CtrBottomPartyChild_IsOpen()
        || CtrBottomPartyChild_IsMovePanel())
        return MODE_PARTY_CHILD;
    if (CtrBottomSummary_IsActive())
        return MODE_SUMMARY;
    /* The field Bag; or the Bag Party's GIVE opens, a child screen of the
     * Party session it belongs to (the session keeps its owner). */
    if (CtrBottomBag_IsOpen() && (!CtrBottomBag_FromParty() || sSession.active || sStorageSession))
        return MODE_BAG;
    /* The TM Case or Berry Pouch that Bag opens: only as a child of a
     * lower-screen session. */
    if (CtrBottomTMCase_IsOpen() && (sSession.active || sStorageSession))
        return MODE_TMCASE;
    if (CtrBottomPouch_IsOpen() && (sSession.active || sStorageSession))
        return MODE_POUCH;
    /* The real Trainer Card, from its setup to its close: the field must not
     * show through, so it is the mode for as long as the card is alive. */
    if (CtrBottomCard_IsOpen())
        return MODE_CARD;
    /* The real Pokedex, from its setup to its close, with all its screens and transitions. */
    if (CtrBottomDex_IsOpen())
        return MODE_DEX;
    if (CtrBottomParty_UsesTopPresentation())
        return MODE_OFF;
    /* FireRed's battle on the upper screen (its own main callback: not a Bag, Party or Summary opened from it):
     * the lower one is the battle screen from its first frame to its last. */
    if (CtrBattle_InBattle())
        return MODE_BATTLE;
    if (CtrBottomParty_IsOpen()) {
        sPartyMenuCallback = CtrBottom_CurrentCallback();
        return MODE_PARTY;
    }
    if (sPartyMenuCallback != NULL && CtrBottom_CurrentCallback() == sPartyMenuCallback)
        return MODE_PARTY;
    /* Party-owned frames before any Party task exists (menu setup): hold
     * from the first one so TOP never flashes the GBA Party screen. */
    if (CtrBottomParty_IsPartyCallback())
        return MODE_PARTY;
    /* True field CB2 only: Summary/Fly/map flows use other CB2 values, so the
     * session stays alive across them instead of dropping to FIELD. */
    if (CtrBottom_IsFieldCallback())
        return MODE_FIELD;
    if (sStorageSession)
        return MODE_STORAGE_CHILD; /* summary/box-name/PC item sub-screen */
    return MODE_OFF;
}

/* The bar under the Party screen: the Pokemon under the cursor. */
static void PartyBar(const Snapshot *s, CtrPokeTouchView *v)
{
    /* GetMonAilment's AILMENT_* (constants/party_menu.h). */
    static const char *const sStatus[] = {"", "Poisoned", "Paralyzed", "Asleep", "Frozen", "Burned",
                                          "PokÃ©rus", "Fainted"};
    int slot = s->cursor;

    if (slot < 0 || slot >= PARTY_SIZE || !((s->presentMask >> slot) & 1)) {
        snprintf(v->bar[0], PT_BAR_LEN, "Choose a PokÃ©mon.");
        return;
    }
    if (s->isEgg[slot]) {
        snprintf(v->bar[0], PT_BAR_LEN, "EGG");
        return;
    }
    snprintf(v->bar[0], PT_BAR_LEN, "%s  Lv%u", s->nick[slot], s->level[slot]);
    snprintf(v->bar[1], PT_BAR_LEN, "HP %u/%u", s->hp[slot], s->maxHp[slot]);
    if (s->hp[slot] == 0)
        snprintf(v->bar[2], PT_BAR_LEN, "Fainted");
    else if (s->status[slot] < sizeof(sStatus) / sizeof(sStatus[0]))
        snprintf(v->bar[2], PT_BAR_LEN, "%s", sStatus[s->status[slot]]);
}

/* The bar under the Summary screen: the Pokemon it shows. */
static void SummaryBar(const CtrSummaryView *v, CtrPokeTouchView *t)
{
    static const char *const sCodes[] = {"PSN", "PRZ", "SLP", "FRZ", "BRN", "PKR", "FNT"};
    static const char *const sNames[] = {"Poisoned", "Paralyzed", "Asleep", "Frozen", "Burned",
                                         "Pok\xc3\xa9rus", "Fainted"};

    if (!v->species[0])
        return;
    if (v->egg) {
        snprintf(t->bar[0], PT_BAR_LEN, "EGG");
        return;
    }
    const char *level = v->level;
    while (*level == ' ')       /* the Summary's level is right-aligned */
        ++level;
    snprintf(t->bar[0], PT_BAR_LEN, "%s  Lv%s", v->nickname, level);
    snprintf(t->bar[1], PT_BAR_LEN, "No.%s %s", v->dex, v->species);
    for (unsigned i = 0; i < sizeof(sCodes) / sizeof(sCodes[0]); ++i)
        if (!strcmp(v->status, sCodes[i]))
            snprintf(t->bar[2], PT_BAR_LEN, "%s", sNames[i]);
}

/* The bar under the Bag: the pocket, the item under the cursor, and the
 * X shortcut when it is the registered item (else the money). */
static void BagBar(CtrPokeTouchView *v)
{
    unsigned item = CtrBottomBag_Item(CtrBottomBag_Cursor());

    CtrBottomBag_PocketName(v->bar[0], PT_BAR_LEN);
    if (item != 0)
        CtrBottomBag_ItemName(item, v->bar[1], PT_BAR_LEN);
    if (item != 0 && item == v->registeredItem)
        snprintf(v->bar[2], PT_BAR_LEN, "On the X button");
    else
        snprintf(v->bar[2], PT_BAR_LEN, "Money  %u", CtrBottomBag_Money());
}

/* The bar under the TM Case: its title, the TM/HM under the cursor and the
 * move it teaches. */
static void TMCaseBar(CtrPokeTouchView *v)
{
    unsigned item = CtrBottomTMCase_Item(CtrBottomTMCase_Cursor());

    CtrBottomTMCase_Title(v->bar[0], PT_BAR_LEN);
    if (item != 0) {
        CtrBottomBag_ItemName(item, v->bar[1], PT_BAR_LEN);
        CtrBottomTMCase_MoveName(item, v->bar[2], PT_BAR_LEN);
    }
}

/* The bar under the Berry Pouch: its name, the berry under the cursor and how
 * many there are. */
static void PouchBar(CtrPokeTouchView *v)
{
    int cursor = CtrBottomPouch_Cursor();
    unsigned item = CtrBottomPouch_Item(cursor);

    CtrBottomPouch_Title(v->bar[0], PT_BAR_LEN);
    if (item != 0) {
        CtrBottomBag_ItemName(item, v->bar[1], PT_BAR_LEN);
        snprintf(v->bar[2], PT_BAR_LEN, "Quantity  %u", CtrBottomPouch_Quantity(cursor));
    }
}

/* HOME's selection: a tap on the map selects the map section under it (the
 * Region Map's own layers) and shows FireRed's cursor there, without stopping
 * the field; a second tap on the same section starts the real Fly when FireRed
 * allows it (CtrPokeTouch_FlyStatus), else does nothing. The selection is
 * dropped when the map changes or a menu takes the screen. */
#define MAP_TAP_SLOP 12         /* a touch that moves further is not a tap */
#define MAPSEC_NONE_VALUE 0     /* what the bridge reports for no map section */

static struct {
    int valid;
    int x, y;                   /* the selected cell */
    int group, version;         /* on this map */
    int mapsec, dungeon;        /* its map section and the dungeon over it (0: none) */
    int down, downX, downY, lastX, lastY;   /* a touch that began in the viewport (GBA pixels) */
    unsigned frames;            /* the cursor's blink clock */
} sHome;

/* The map section a tap at cell (cx, cy) means: the cell's own, else (the
 * cities are one cell) a Fly destination right beside it. */
static int HomeFind(int group, int cx, int cy, int *fx, int *fy, int *mapsec, int *dungeon)
{
    int best = -1, bx = 0, by = 0, bm = 0, bd = 0;

    if (CtrPokeTouch_MapSection(group, cx, cy, mapsec, dungeon)) {
        *fx = cx;
        *fy = cy;
        return 1;
    }
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) {
            int m, d, dist = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
            if (!dist || !CtrPokeTouch_MapSection(group, cx + dx, cy + dy, &m, &d) || m == MAPSEC_NONE_VALUE
                || CtrPokeTouch_FlyStatus((unsigned)m) == PT_FLY_NONE)
                continue;
            if (best < 0 || dist < best) {
                best = dist; bx = cx + dx; by = cy + dy; bm = m; bd = d;
            }
        }
    if (best < 0)
        return 0;
    *fx = bx; *fy = by; *mapsec = bm; *dungeon = bd;
    return 1;
}

static void HomeTap(int gx, int gy)
{
    int group = CtrPokeTouch_MapGroup(), version = CtrPokeTouch_MapVersion();
    int cx, cy, x, y, mapsec, dungeon;

    /* (gx, gy) is a viewport pixel: HOME's map is the enlarged one (PT_HOME_*). */
    if (gx < PT_HOME_X || gy < PT_HOME_Y) {
        sHome.valid = 0;
        return;
    }
    cx = (gx - PT_HOME_X) / PT_HOME_CELL;
    cy = (gy - PT_HOME_Y) / PT_HOME_CELL;
    if (cx >= 22 || cy >= 15) {
        sHome.valid = 0;
        return;
    }
    if (!HomeFind(group, cx, cy, &x, &y, &mapsec, &dungeon)) {
        sHome.valid = 0;            /* nothing there: the selection goes */
        return;
    }
    if (sHome.valid && sHome.x == x && sHome.y == y && sHome.group == group && sHome.version == version) {
        /* The second tap: asked again of the game, now. */
        if (mapsec != MAPSEC_NONE_VALUE && CtrPokeTouch_FlyStatus((unsigned)mapsec) == PT_FLY_OK) {
            CtrPokeTouch_RequestFly((unsigned)mapsec);
            sHome.valid = 0;
        }
        return;
    }
    sHome.valid = 1;
    sHome.x = x; sHome.y = y;
    sHome.group = group; sHome.version = version;
    sHome.mapsec = mapsec; sHome.dungeon = dungeon;
    sHome.frames = 0;
}

/* HOME's bar for the selection: its name, and what Fly says about it. */
static void HomeSelectionBar(CtrPokeTouchView *v)
{
    static const char *const sWhy[] = {
        [PT_FLY_UNVISITED] = "Not visited yet", [PT_FLY_NO_MON] = "No Pok\xc3\xa9mon knows Fly",
        [PT_FLY_NO_BADGE] = "A new Badge is needed", [PT_FLY_NO_PLACE] = "Can't Fly here",
        [PT_FLY_BUSY] = "Not right now",
    };
    int status = sHome.mapsec != MAPSEC_NONE_VALUE ? CtrPokeTouch_FlyStatus((unsigned)sHome.mapsec) : PT_FLY_NONE;

    CtrPokeTouch_MapSectionName((unsigned)(sHome.mapsec != MAPSEC_NONE_VALUE ? sHome.mapsec : sHome.dungeon),
                                v->bar[0], PT_BAR_LEN);
    if (status == PT_FLY_OK) {
        snprintf(v->bar[1], PT_BAR_LEN, "Fly available");
        snprintf(v->bar[2], PT_BAR_LEN, "Tap again to Fly");
    } else if (status != PT_FLY_NONE) {
        snprintf(v->bar[1], PT_BAR_LEN, "Fly unavailable");
        snprintf(v->bar[2], PT_BAR_LEN, "%s", sWhy[status]);
    } else if (sHome.mapsec != MAPSEC_NONE_VALUE && sHome.dungeon != MAPSEC_NONE_VALUE) {
        CtrPokeTouch_MapSectionName((unsigned)sHome.dungeon, v->bar[1], PT_BAR_LEN);
    }
}

/* HOME, the field's lower screen: FireRed's own map of where the player is,
 * and the location's name and region on the bar. */
static void HomeMapState(Snapshot *s)
{
    static const char *const sRegions[PT_MAP_COUNT] = {"Main region", "Island region", "Island region", "Island region"};
    int group = CtrPokeTouch_MapGroup(), px, py, mapsec;

    s->touch.home = 1;
    s->touch.mapGroup = (uint8_t)(group >= 0 && group < PT_MAP_COUNT ? group : PT_MAP_KANTO);
    s->touch.mapVersion = (uint8_t)CtrPokeTouch_MapVersion();
    s->touch.playerFemale = (uint8_t)(CtrPokeTouch_PlayerFemale() != 0);
    s->touch.playerX = s->touch.playerY = -1;
    if (CtrPokeTouch_MapPlayer(&px, &py, &mapsec)) {
        s->touch.playerX = (int8_t)px;
        s->touch.playerY = (int8_t)py;
    }
    s->touch.cursorX = s->touch.cursorY = -1;
    if (sHome.valid && (sHome.group != s->touch.mapGroup || sHome.version != s->touch.mapVersion))
        sHome.valid = 0;                /* another map: the selection means nothing */
    if (sHome.valid) {
        s->touch.cursorX = (int8_t)sHome.x;
        s->touch.cursorY = (int8_t)sHome.y;
        s->touch.cursorFrame = (uint8_t)((sHome.frames / 20) & 1);
        HomeSelectionBar(&s->touch);
        return;
    }
    CtrPokeTouch_Location(s->touch.bar[0], PT_BAR_LEN);
    snprintf(s->touch.bar[1], PT_BAR_LEN, "%s", sRegions[s->touch.mapGroup]);
    snprintf(s->touch.bar[2], PT_BAR_LEN, "Tap a destination");
}

/* The battle screen's touch: a button pressed under a finger (a BUI_ACT_* number). */
static struct {
    int down, inside, btn, state;
} sBat;
static int sBatTouchLast;           /* the last input was the touch screen: no cursor drawn */
static int sBatHold, sBatHoldFrames; /* BAG was picked: the upper screen keeps the battle until the Bag shows */

/* The six ball slots and the touch held under a finger: what every battle screen shares. */
static void BattleCommon(CtrBattleView *v)
{
    memset(v, 0, sizeof(*v));
    v->pressed = (int16_t)(sBat.down && sBat.inside ? sBat.btn : -1);
    v->cursor = -1;
    for (int i = 0; i < 6; ++i)
        v->party[i] = CtrBottomParty_SlotEmpty(i) ? 0 : CtrBottomParty_HP(i) == 0 && !CtrBottomParty_IsEgg(i) ? 2 : 1;
}

/* The battle Bag as it is now: its pocket, its items (the save's own), the selected one's detail, the open menu or the
 * message it waits on. */
static void BattleBagFill(CtrBattleView *v)
{
    CtrBattleBag *b = &v->bag;
    int count, shown;

    BattleCommon(v);
    v->state = BUI_BAG;
    b->pocket = (uint8_t)CtrBottomBag_Pocket();
    b->bagState = (uint8_t)CtrBottomBag_State();
    b->count = (int16_t)(count = CtrBottomBag_Count());
    b->cursor = (int16_t)CtrBottomBag_Cursor();
    shown = b->cursor >= count ? count - 1 : b->cursor;
    b->first = (int16_t)(shown - 2 < 0 ? 0 : shown - 2);
    if (b->first > count - BUI_BAG_ROWS)
        b->first = (int16_t)(count - BUI_BAG_ROWS < 0 ? 0 : count - BUI_BAG_ROWS);
    for (int i = 0; i < BUI_BAG_ROWS; ++i) {
        unsigned item = CtrBottomBag_Item(b->first + i);

        b->row[i].item = (uint16_t)item;
        if (item) {
            b->row[i].qty = (uint16_t)CtrBottomBag_Quantity(b->first + i);
            CtrBottomBag_ItemName(item, b->row[i].name, sizeof(b->row[i].name));
        }
    }
    if (b->cursor >= 0 && b->cursor < count) {
        b->selItem = (uint16_t)CtrBottomBag_Item(b->cursor);
        if (b->selItem) {
            CtrBottomBag_ItemName(b->selItem, b->selName, sizeof(b->selName));
            CtrBottomBag_ItemDescription(b->selItem, b->selDesc, sizeof(b->selDesc));
        }
    }
    if (b->bagState == BAG_ACTIONS) {
        int n = CtrBottomBag_ActionCount();

        b->menuCount = (uint8_t)(n > 3 ? 3 : n);
        b->menuCursor = (int8_t)CtrBottomBag_MenuCursor();
        for (int i = 0; i < b->menuCount; ++i)
            CtrBottomBag_ActionName(i, b->menuName[i], sizeof(b->menuName[i]));
    }
    if (b->bagState == BAG_MESSAGE)
        CtrBottomBag_Message(b->message, sizeof(b->message));
}

static void BatCopy(char *dst, size_t size, const char *src)
{
    size_t n = strlen(src);

    if (n >= size)
        n = size - 1;
    memcpy(dst, src, n);
    dst[n] = 0;
}

/* The battle Party as the Party is now (the lower screen's own Party snapshot `s`, which reads FireRed's Party): the six
 * slots, the Party's selection, what it was opened for, and its menu or message when it has one open. */
static void BattlePartyFill(Snapshot *s)
{
    CtrBattleView *v = &s->battle;
    CtrBattleParty *p = &v->pty;
    int action = CtrBottomParty_Action();

    BattleCommon(v);
    v->state = BUI_PARTY;
    p->cursor = (int8_t)(s->cursor >= 0 && s->cursor < PARTY_SIZE ? s->cursor : -1);
    p->mode = action == 1 ? BUI_PTY_FORCED : action == 3 ? BUI_PTY_TARGET : BUI_PTY_SWITCH;   /* SEND_OUT cannot be cancelled */
    for (int i = 0; i < PARTY_SIZE; ++i) {
        p->present[i] = (uint8_t)((s->presentMask >> i) & 1);
        if (!p->present[i])
            continue;
        p->egg[i] = s->isEgg[i];
        p->status[i] = s->status[i];
        p->level[i] = s->level[i];
        p->hp[i] = s->hp[i];
        p->maxHp[i] = s->maxHp[i];
        p->icon[i] = s->icon[i];
        snprintf(p->name[i], sizeof(p->name[i]), "%s", s->nick[i]);
    }
    if (s->panel == PANEL_ACTIONS || s->panel == PANEL_YESNO) {
        int n = s->panel == PANEL_YESNO ? 2 : s->actionCount > 6 ? 6 : s->actionCount;

        p->panel = s->panel == PANEL_YESNO ? BUI_PTY_YESNO : BUI_PTY_ACTIONS;
        p->menuCount = (uint8_t)n;
        p->menuCursor = s->actionCursor;
        for (int i = 0; i < n; ++i)
            BatCopy(p->menuName[i], sizeof(p->menuName[i]), s->panel == PANEL_YESNO ? (i == 0 ? "YES" : "NO") : s->actionNames[i]);
    } else if (s->panel == PANEL_MESSAGE) {
        p->panel = BUI_PTY_TEXT;
        BatCopy(p->message, sizeof(p->message), s->message);
    }
    s->battleUi = 1;
    s->touch.viewport = 0;
}

/* The battle screen as the controller is now: nothing but what FireRed reports. */
static void BattleFill(CtrBattleView *v)
{
    int state = CtrBattle_State();

    BattleCommon(v);
    v->state = state == BATTLE_ACTION ? BUI_ACTION : state == BATTLE_MOVE ? BUI_MOVE
             : state == BATTLE_TARGET ? BUI_TARGET : state == BATTLE_SAFARI ? BUI_SAFARI : BUI_IDLE;
    if (v->state == BUI_TARGET) {
        /* The cards: opposing battlers on top (left, right), the player's below. Only the ones the controller's own
         * rule (the one its D-pad walks by) lets the move be aimed at are shown. */
        static const int8_t card[4] = {2, 0, 3, 1};     /* by FireRed's position: PLAYER_LEFT, OPPONENT_LEFT, PLAYER_RIGHT, OPPONENT_RIGHT */
        int cursor = CtrBattle_TargetCursor();

        v->cursor = -1;

        for (int b = 0; b < CtrBattle_TargetCount() && b < 4; ++b) {
            int position = CtrBattle_TargetPosition(b), i;
            CtrBattleTarget *t;
            int level = 0, hp = 0, maxHp = 0;

            if (position < 0 || position > 3 || !CtrBattle_TargetLegal(b))
                continue;
            i = card[position];
            t = &v->target[i];
            if (!CtrBattle_TargetInfo(b, t->name, (int)sizeof(t->name), &level, &hp, &maxHp))
                continue;
            t->present = 1;
            t->battler = (int8_t)b;
            t->side = (uint8_t)((position & 1) == 0);
            t->level = (uint8_t)(level > 255 ? 255 : level);
            t->hp = (uint16_t)hp;
            t->maxHp = (uint16_t)maxHp;
            if (b == cursor)
                v->cursor = (int8_t)i;
        }
        v->showCursor = !sBatTouchLast;
        return;
    }
    if (v->state == BUI_SAFARI) {
        v->cursor = (int8_t)CtrBattle_ActionCursor();
        v->showCursor = !sBatTouchLast;
        return;
    }
    if (v->state == BUI_ACTION) {
        v->cursor = (int8_t)CtrBattle_ActionCursor();
        v->showCursor = !sBatTouchLast;
    }
    if (v->state != BUI_MOVE)
        return;
    v->cursor = (int8_t)CtrBattle_MoveCursor();
    v->showCursor = !sBatTouchLast;
    for (int i = 0; i < 4; ++i) {
        unsigned move = CtrBattle_Move(i);
        CtrBattleMove *m = &v->move[i];
        unsigned power;

        if (move == 0)
            continue;
        m->present = 1;
        m->type = (uint8_t)CtrBattle_MoveType(move);
        CtrBattle_MoveName(move, m->name, sizeof(m->name));
        CtrBattle_TypeName(m->type, m->typeName, sizeof(m->typeName));
        CtrBattle_MoveDescription(move, m->desc, sizeof(m->desc));
        {
            int pp, maxPp;

            if (CtrBattle_MovePP(i, &pp, &maxPp)) {
                m->pp = (uint8_t)(pp < 0 ? 0 : pp > 255 ? 255 : pp);
                m->maxPp = (uint8_t)(maxPp < 0 ? 0 : maxPp > 255 ? 255 : maxPp);
            }
        }
        power = CtrBattle_MovePower(move);
        if (power)
            snprintf(m->power, sizeof(m->power), "%u", power > 999 ? 999 : power);
        else
            snprintf(m->power, sizeof(m->power), "-");
    }
}

/* SAVE's view: FireRed's own save information (the facts SaveStatToString prints), the player's
 * icon key, and the dialogue's text for the state. No BACK: NO, B and the sidebar leave. */
static void SaveFill(CtrPokeTouchView *v)
{
    unsigned minutes = CtrSave_PlayMinutes();

    v->save = 1;
    v->saveState = (uint8_t)sSave.state;
    v->saveSel = (uint8_t)sSave.sel;
    v->savePressed = (uint8_t)(sSave.down && sSave.inside ? sSave.down : PT_SAVE_NONE);
    v->playerFemale = (uint8_t)(CtrPokeTouch_PlayerFemale() != 0);
    CtrPokeTouch_PlayerName(v->playerName, (int)sizeof(v->playerName));
    v->saveBadges = (uint8_t)CtrSave_BadgeCount();
    v->saveHasDex = (uint8_t)(CtrSave_HasPokedex() != 0);
    v->saveDex = (uint16_t)CtrSave_DexCaught();
    v->saveHours = (uint16_t)CtrSave_PlayHours();
    v->saveMinutes = (uint8_t)(minutes > 59 ? 59 : minutes);
    v->saveMoney = CtrSave_Money();
    switch (sSave.state) {
    case SAVE_WRITING:
        snprintf(v->bar[0], PT_BAR_LEN, "Saving...");
        snprintf(v->bar[1], PT_BAR_LEN, "Please don't turn off the power.");
        break;
    case SAVE_SUCCESS:
        snprintf(v->bar[0], PT_BAR_LEN, "Your game has been saved!");
        break;
    case SAVE_ERROR:
        snprintf(v->bar[0], PT_BAR_LEN, "The save file could not");
        snprintf(v->bar[1], PT_BAR_LEN, "be written. Try again.");
        break;
    default:
        if (CtrSave_IsDifferentFile()) {
            snprintf(v->bar[0], PT_BAR_LEN, "There is a different game");
            snprintf(v->bar[1], PT_BAR_LEN, "file saved. Overwrite it?");
        } else {
            snprintf(v->bar[0], PT_BAR_LEN, "Would you like to save");
            snprintf(v->bar[1], PT_BAR_LEN, "your adventure?");
        }
        break;
    }
}

static void SnapshotState(Snapshot *s, int mode, int pressed)
{
    memset(s, 0, sizeof(*s));
    s->mode = (unsigned char)mode;
    s->pressed = (unsigned char)pressed;
    s->cursor = -1;
    s->tapped = -1;
    /* PT_POKEDEX is zero: a zeroed view must not select that sidebar button. */
    s->touch.selected = PT_NONE;
    s->touch.pressed = PT_NONE;
    if (mode == MODE_PARTY_CHILD) {
        /* Learning/forgetting and the disc animation keep their native GBA
         * controls; sidebar routing must not unwind these modal callbacks. */
        s->touch.viewport = 1;
        s->touch.selected = sSession.active ? sSession.owner : PT_POKEMON;
        snprintf(s->touch.bar[0], PT_BAR_LEN, "MOVE / ITEM");
        snprintf(s->touch.bar[1], PT_BAR_LEN, "Use A and B to follow the prompts.");
        return;
    }
    if (mode == MODE_STORAGE_PC || mode == MODE_STORAGE_BOX || mode == MODE_STORAGE_CHILD) {
        /* Pixel-perfect copy of FireRed's existing GBA storage artwork in the
         * PokeTouch viewport. No reimplemented Pokemon, names, menus or sprites. */
        s->touch.viewport = 1;
        s->touch.backVisible = 1;
        s->touch.backEnabled = (uint8_t)BackEnabled(mode);
        s->touch.backPressed = (uint8_t)(sBack.inside != 0);
        CtrPokeTouch_PlayerName(s->touch.playerName, sizeof(s->touch.playerName));
        if (mode == MODE_STORAGE_PC) {
            snprintf(s->touch.bar[0], PT_BAR_LEN, "POKeMON STORAGE SYSTEM");
            snprintf(s->touch.bar[1], PT_BAR_LEN, "Tap a PC option to confirm.");
            snprintf(s->touch.bar[2], PT_BAR_LEN, "Original PC rules apply.");
        } else if (mode == MODE_STORAGE_BOX) {
            snprintf(s->touch.bar[0], PT_BAR_LEN, "BOX %02d", StorageGetCurrentBox() + 1);
            snprintf(s->touch.bar[1], PT_BAR_LEN, "Tap a slot to open its menu.");
            snprintf(s->touch.bar[2], PT_BAR_LEN, "BACK returns to the PC.");
        } else {
            snprintf(s->touch.bar[0], PT_BAR_LEN, "POKeMON STORAGE SYSTEM");
            snprintf(s->touch.bar[1], PT_BAR_LEN, "Use the game's own controls.");
        }
        return;
    }
    if (mode == MODE_BATTLE) {
        BattleFill(&s->battle);
        s->battleUi = 1;
        return;
    }
    if (mode == MODE_BAG && CtrBottomBag_InBattle()) {
        BattleBagFill(&s->battle);
        s->battleUi = 1;
        return;
    }
    if (mode == MODE_SUMMARY) {
        if (SummaryPremium()) {
            SummaryFill(&s->sum);
            s->summaryUi = 1;
            return;
        }
        /* The field's Summary is FireRed's own, in the viewport. */
        CtrBottomSummary_GetView(&s->summary);
        SummaryBar(&s->summary, &s->touch);
    }
    if (mode == MODE_FIELD || mode == MODE_PARTY || mode == MODE_SUMMARY || mode == MODE_BAG || IS_CASE_MODE(mode)
        || mode == MODE_CARD || mode == MODE_DEX) {
        s->touch.pressed = sPoke.inside ? sPoke.id : PT_NONE;
        s->touch.selected = PT_NONE;
        s->touch.running = (uint8_t)(CtrPokeTouch_RunModeShown() != 0);
        CtrPokeTouch_PlayerName(s->touch.playerName, sizeof(s->touch.playerName));
        s->touch.registeredItem = (uint16_t)CtrPokeTouch_RegisteredItem();
        s->touch.bikeItem = (uint16_t)CtrPokeTouch_BikeItem();
        s->touch.enabled[PT_RUN] = (uint8_t)(CtrPokeTouch_Enabled(PT_RUN) != 0);
    }
    if (mode == MODE_SUMMARY || mode == MODE_BAG || IS_CASE_MODE(mode) || mode == MODE_CARD || mode == MODE_DEX) {
        memset(s->touch.enabled, 0, sizeof(s->touch.enabled));
        s->touch.enabled[PT_RUN] = (uint8_t)(CtrPokeTouch_Enabled(PT_RUN) != 0);
        s->touch.enabled[PT_POKEMON] = (uint8_t)CanRouteDestination(PT_POKEMON, mode);
        s->touch.enabled[PT_BAG] = (uint8_t)CanRouteDestination(PT_BAG, mode);
        s->touch.enabled[PT_OPTIONS] = (uint8_t)CanRouteDestination(PT_OPTIONS, mode);
        s->touch.enabled[PT_CARD] = (uint8_t)CanRouteDestination(PT_CARD, mode);
        s->touch.enabled[PT_POKEDEX] = (uint8_t)CanRouteDestination(PT_POKEDEX, mode);
        /* The entry whose session this is: BAG for the field Bag, POKeMON
         * for Party's GIVE Bag (and Summary). */
        s->touch.selected = (uint8_t)(sSession.active ? sSession.owner : mode == MODE_BAG ? PT_BAG : PT_POKEMON);
        if (sStorageSession) {
            /* Summary and GIVE-item Bag opened from a box belong to the PC,
             * not to a field PokÃ©Touch root. */
            memset(s->touch.enabled, 0, sizeof(s->touch.enabled));
            s->touch.selected = PT_NONE;
        }
        s->touch.viewport = 1;
        s->touch.backVisible = (uint8_t)BackVisible(mode);
        s->touch.backEnabled = (uint8_t)BackEnabled(mode);
        s->touch.backPressed = (uint8_t)(sBack.inside != 0);
        if (mode == MODE_BAG) {
            s->serial = CtrBottomBag_Serial();
            s->cursor = (signed char)CtrBottomBag_Cursor();
            BagBar(&s->touch);
        }
        if (mode == MODE_TMCASE) {
            s->serial = CtrBottomTMCase_Serial();
            s->cursor = (signed char)CtrBottomTMCase_Cursor();
            TMCaseBar(&s->touch);
        }
        if (mode == MODE_POUCH) {
            s->serial = CtrBottomPouch_Serial();
            s->cursor = (signed char)CtrBottomPouch_Cursor();
            PouchBar(&s->touch);
        }
        if (mode == MODE_CARD) {
            /* The card itself shows its statistics: the bar only says how to turn it. */
            snprintf(s->touch.bar[0], PT_BAR_LEN, "TRAINER CARD");
            snprintf(s->touch.bar[1], PT_BAR_LEN, "Tap card to turn it over.");
        }
        if (mode == MODE_DEX) {
            /* The Pokedex draws everything itself: the bar only says how to use it. */
            int state = CtrBottomDex_State();

            snprintf(s->touch.bar[0], PT_BAR_LEN, "POKeDEX");
            snprintf(s->touch.bar[1], PT_BAR_LEN, state == DEX_MENU || state == DEX_LIST
                     ? "Tap to select, again to open." : "BACK goes one step back.");
        }
        return;
    }
    if (mode == MODE_FIELD) {
        for (int i = 0; i < PT_COUNT; ++i)
            s->touch.enabled[i] = (uint8_t)FieldEnabled(i);
        if (NativeActive()) {
            s->touch.selected = (uint8_t)sScreen;
            if (sScreen == PT_SAVE) {
                SaveFill(&s->touch);
                return;
            }
            s->touch.backVisible = (uint8_t)BackVisible(mode);
            s->touch.backEnabled = (uint8_t)BackEnabled(mode);
            s->touch.backPressed = (uint8_t)(sBack.inside != 0);
            CtrOptions_Fill(&s->touch, sOpt.sel);
            s->touch.optPressed = (int8_t)(sOpt.down && sOpt.inside ? sOpt.row : -1);
            s->touch.optZone = (uint8_t)(sOpt.down && sOpt.inside ? sOpt.zone : PT_OPT_ZONE_NONE);
            return;
        }
        HomeMapState(s);
        return;
    }
    if (mode == MODE_PARTY) {
        s->serial = CtrBottomParty_MenuSerial();
        s->chooseMultiple = (unsigned char)CtrBottomParty_ChooseMultiple();
        s->canChoose = (unsigned char)(CtrBottomParty_IsOpen() && CtrBottomParty_InputReady());
        s->canAdvance = (unsigned char)CtrBottomParty_CanAdvanceText();
        s->partyCount = (unsigned char)CtrBottomParty_Count();
        s->cursor = (signed char)CtrBottomParty_Slot();
        for (int i = 0; i < PARTY_SIZE; ++i) {
            int empty = CtrBottomParty_SlotEmpty(i);
            if (!empty)
                s->presentMask |= (unsigned char)(1u << i);
            else {
                s->nick[i][0] = 0;
                continue;
            }
            s->hp[i] = (uint16_t)CtrBottomParty_HP(i);
            s->maxHp[i] = (uint16_t)CtrBottomParty_MaxHP(i);
            s->status[i] = (unsigned char)CtrBottomParty_Status(i);
            s->level[i] = (unsigned char)CtrBottomParty_Level(i);
            s->gender[i] = (unsigned char)CtrBottomParty_Gender(i);
            s->isEgg[i] = (unsigned char)CtrBottomParty_IsEgg(i);
            s->icon[i] = (uint16_t)CtrBottomParty_IconSpecies(i);
            CtrBottomParty_Nickname(i, s->nick[i]);
            s->nick[i][10] = 0;
            CtrBottomParty_Description(i, s->description[i]);
            s->description[i][23] = 0;
        }
        CtrBottomParty_Message(s->message);
        s->message[MESSAGE_LEN - 1] = 0;
        if (CtrBottomParty_SelectionOpen()) {
            int n = CtrBottomParty_ActionCount();
            if (n > PARTY_MAX_ACTIONS)
                n = PARTY_MAX_ACTIONS;
            sLatchCount = 0;
            for (int i = 0; i < n; ++i) {
                sLatchIds[sLatchCount] = (unsigned char)CtrBottomParty_ActionId(i);
                CtrBottomParty_ActionName(i, sLatchNames[sLatchCount]);
                sLatchNames[sLatchCount][ACTION_NAME_LEN - 1] = 0;
                ++sLatchCount;
            }
            s->panel = PANEL_ACTIONS;
            s->actionCursor = (signed char)CtrBottomParty_Cursor();
        } else {
            s->actionCursor = -1;
            if (CtrBottomParty_YesNoOpen()) {
                s->panel = PANEL_YESNO;
                s->actionCursor = (signed char)CtrBottomParty_Cursor();
            } else if (s->canAdvance) {
                s->message[MESSAGE_LEN - 1] = 0;
                s->panel = PANEL_MESSAGE;
            } else {
                s->panel = PANEL_HINT;
            }
        }
        s->actionCount = sLatchCount;
        memcpy(s->actionIds, sLatchIds, sizeof(s->actionIds));
        for (int i = 0; i < PARTY_MAX_ACTIONS; ++i) {
            memcpy(s->actionNames[i], sLatchNames[i], ACTION_NAME_LEN);
            s->actionNames[i][ACTION_NAME_LEN - 1] = 0;
        }
        /* Party opened from the Bag (an item to give or use) stays the Bag's. */
        s->touch.selected = sSession.active && sSession.owner == PT_BAG ? PT_BAG : PT_POKEMON;
        s->touch.enabled[PT_POKEMON] = (uint8_t)CanRouteDestination(PT_POKEMON, mode);
        s->touch.enabled[PT_BAG] = (uint8_t)CanRouteDestination(PT_BAG, mode);
        s->touch.enabled[PT_OPTIONS] = (uint8_t)CanRouteDestination(PT_OPTIONS, mode);
        s->touch.enabled[PT_CARD] = (uint8_t)CanRouteDestination(PT_CARD, mode);
        s->touch.enabled[PT_POKEDEX] = (uint8_t)CanRouteDestination(PT_POKEDEX, mode);
        s->touch.viewport = 1;
        PartyBar(s, &s->touch);
        if (CtrBottomParty_InBattle())
            BattlePartyFill(s);
    }
}

/* The canvas rectangles that changed go to the screen: copied by the CPU, or,
 * while the GPU draws the bottom screen (a viewport is shown), taken from the
 * canvas at the next frame. Only those pixels move. */
static int sGpu;
static int sBridgeFrames;
static int sBridging;               /* the last frame kept the picture between two screens: nothing is touchable meanwhile */           /* frames the lower screen has kept its picture between two screens of a session */

static void PresentRect(int x0, int y0, int x1, int y1)
{
    if (sGpu)
        CtrVideo_BottomCanvasRect(sCanvas, x0, y0, x1, y1);
    else
        CtrBottom_BlitRect(sCanvas, x0, y0, x1, y1);
}

static void Present(const CtrDirtyList *dirty)
{
    for (int i = 0; i < dirty->count; ++i)
        PresentRect(dirty->rects[i].x0, dirty->rects[i].y0, dirty->rects[i].x1, dirty->rects[i].y1);
}

/* The battle screen's one transition: when the controller moves to another screen (MAIN, the moves, a target, the
 * Bag, the Party), the new body is presented top to bottom in REVEAL_BANDS bands, one a frame, a short wipe. Nothing
 * is drawn for it: the canvas already holds the new screen and each of its pixels is presented once, as without it,
 * only spread over the frames. Any other change, a full redraw or a hold first presents what is left. */
#define REVEAL_BANDS 3
static struct {
    int active;
    CtrDirtyRect r;                 /* what is left to present */
    int band;                       /* the rows presented a frame */
} sReveal;

static void RevealFlush(void)
{
    if (sReveal.active)
        PresentRect(sReveal.r.x0, sReveal.r.y0, sReveal.r.x1, sReveal.r.y1);
    sReveal.active = 0;
}

static void RevealStep(void)
{
    int y1;

    if (!sReveal.active)
        return;
    y1 = sReveal.r.y0 + sReveal.band;
    if (y1 >= sReveal.r.y1) {
        RevealFlush();
        return;
    }
    PresentRect(sReveal.r.x0, sReveal.r.y0, sReveal.r.x1, y1);
    sReveal.r.y0 = y1;
}

/* Presents an update that changes the battle's screen: the largest rectangle (the body) starts its wipe, the others
 * (the ball strip) go at once. */
static void PresentRevealed(const CtrDirtyList *dirty)
{
    int largest = 0;

    for (int i = 1; i < dirty->count; ++i)
        if (CtrDirtyRect_Area(&dirty->rects[i]) > CtrDirtyRect_Area(&dirty->rects[largest]))
            largest = i;
    for (int i = 0; i < dirty->count; ++i)
        if (i != largest)
            PresentRect(dirty->rects[i].x0, dirty->rects[i].y0, dirty->rects[i].x1, dirty->rects[i].y1);
    if (dirty->count == 0)
        return;
    sReveal.active = 1;
    sReveal.r = dirty->rects[largest];
    sReveal.band = (sReveal.r.y1 - sReveal.r.y0 + REVEAL_BANDS - 1) / REVEAL_BANDS;
    RevealStep();
}

static void Render(const Snapshot *s)
{
    if (s->summaryUi)
        CtrSummaryUi_Draw(sCanvas, &s->sum);
    else if (s->battleUi)
        CtrBattleUi_Draw(sCanvas, &s->battle);
    else if (s->mode != MODE_OFF || sSession.active)
        CtrPokeTouch_Draw(sCanvas, &s->touch);
    else
        FillRect(0, 0, CTR_BOTTOM_WIDTH, CTR_BOTTOM_HEIGHT, C_BG);
}

/* Plan helpers. */

static void PlanStart(int kind, int target)
{
    sPlan.kind = kind;
    sPlan.target = target;
    sPlan.steps = 0;
    sPlan.wait = 0;
    sPlan.release = 0;
    sPlan.keys = 0;
    sPlan.menuKind = CtrBottomParty_SelectionOpen() ? 1 : 2;
    sPlan.count = sPlan.menuKind == 1 ? CtrBottomParty_ActionCount() : 2;
    sPlan.slot = CtrBottomParty_Slot();
    sPlan.serial = CtrBottomParty_MenuSerial();
    for (int i = 0; i < 8; ++i)
        sPlan.actionIds[i] = CtrBottomParty_ActionId(i);
}

static void PlanPress(uint16_t keys)
{
    PlanStart(PLAN_PRESS, 0);
    sPlan.keys = keys;
}

static void PlanClear(void)
{
    sPlan.kind = PLAN_NONE;
    sPlan.target = 0;
    sPlan.steps = 0;
    sPlan.wait = 0;
    sPlan.release = 0;
    sPlan.keys = 0;
}

static int PlanCursor(int *cursor)
{
    if (sPlan.kind == PLAN_PARTY) {
        if (!CtrBottomParty_IsOpen() || CtrBottom_IsFading())
            return 0;
        *cursor = CtrBottomParty_Slot();
        return 1;
    }
    if (sPlan.kind == PLAN_MENU) {
        if (CtrBottom_IsFading())
            return 0;
        if (!CtrBottomParty_SelectionOpen() && !CtrBottomParty_YesNoOpen())
            return 0;
        *cursor = CtrBottomParty_Cursor();
        return 1;
    }
    return 0;
}

static uint16_t PlanStep(int cur, int target)
{
    if (sPlan.kind == PLAN_PARTY) {
        (void)CtrBottomParty_Layout;
        if (cur >= PARTY_SIZE)
            return K_DOWN;
    }
    if (target > cur)
        return K_DOWN;
    return K_UP;
}

static void RunPlan(void)
{
    int cur;

    sInjected = 0;
    if (sPlan.kind == PLAN_NONE) {
        sPlan.release = 0;
        return;
    }
    /* Physical controls always win. */
    if (CtrInput_Get()->held & 0x3ffu) {
        PlanClear();
        return;
    }
    /* An action plan belongs to the exact live menu that was touched. */
    if (sPlan.kind == PLAN_MENU) {
        int kind = CtrBottomParty_SelectionOpen() ? 1 : (CtrBottomParty_YesNoOpen() ? 2 : 0);
        int count = kind == 1 ? CtrBottomParty_ActionCount() : 2;
        int changed = CtrBottomParty_MenuSerial() != sPlan.serial || kind != sPlan.menuKind || count != sPlan.count || CtrBottomParty_Slot() != sPlan.slot;
        if (kind == 1)
            for (int i = 0; i < count && i < 8; ++i)
                changed |= sPlan.actionIds[i] != CtrBottomParty_ActionId(i);
        if (changed || !CtrBottomParty_InputReady()) { PlanClear(); return; }
    }
    if (sPlan.release) {
        sPlan.release = 0;
        return;
    }
    if (sPlan.kind == PLAN_PRESS) {
        /* Fire only while no menu owns the input; otherwise a press queued
         * for a message could confirm a freshly opened selection. */
        if (!CtrBottomParty_CanAdvanceText() && sPlan.keys != (1u << 1)) {
            PlanClear();
            return;
        }
        if (sPlan.keys == (1u << 1) && (!CtrBottomParty_IsOpen() || !CtrBottomParty_InputReady())) {
            PlanClear();
            return;
        }
        if (CtrBottomParty_SelectionOpen() || CtrBottomParty_YesNoOpen()) {
            PlanClear();
            return;
        }
        sInjected = sPlan.keys;
        PlanClear();
        sPlan.release = 1;
        return;
    }
    if (!PlanCursor(&cur)) {
        if (++sPlan.wait > 45)
            PlanClear();
        return;
    }
    if (++sPlan.steps > 24) {
        PlanClear();
        return;
    }
    if (cur == sPlan.target) {
        sInjected = K_A;
        PlanClear();
        sPlan.release = 1;
        return;
    }
    sInjected = PlanStep(cur, sPlan.target);
    sPlan.release = 1;
}

static const ListScreen sBagScreen = {
    CtrBottomBag_State, CtrBottomBag_Count, CtrBottomBag_MaxShowed, CtrBottomBag_Cursor,
    CtrBottomBag_Serial, CtrBottomBag_HitAt,
};
static const ListScreen sTMCaseScreen = {
    CtrBottomTMCase_State, CtrBottomTMCase_Count, CtrBottomTMCase_MaxShowed, CtrBottomTMCase_Cursor,
    CtrBottomTMCase_Serial, CtrBottomTMCase_HitAt,
};
static const ListScreen sPouchScreen = {
    CtrBottomPouch_State, CtrBottomPouch_Count, CtrBottomPouch_MaxShowed, CtrBottomPouch_Cursor,
    CtrBottomPouch_Serial, CtrBottomPouch_HitAt,
};

/* The list screen a mode shows, or NULL. */
static const ListScreen *ListFor(int mode)
{
    return mode == MODE_BAG ? &sBagScreen : mode == MODE_TMCASE ? &sTMCaseScreen
         : mode == MODE_POUCH ? &sPouchScreen : NULL;
}

/* Bag plan: presses for the Bag's own input states, apart from the Party plan.
 * LIST walks the list cursor to an index, MENU the open menu's cursor to a
 * row (then A), PRESS sends one button. Each belongs to the list screen
 * (Bag, TM Case or Berry Pouch), state and serial it was made for and is dropped as soon
 * as they change. */
enum { BAGPLAN_NONE = 0, BAGPLAN_LIST, BAGPLAN_MENU, BAGPLAN_PRESS };

static struct {
    int kind;
    const ListScreen *list;
    int state;
    unsigned serial;
    int target;
    int activate;
    int steps;
    int release;
    uint16_t keys;
} sBagPlan;

static void BagPlanClear(void)
{
    memset(&sBagPlan, 0, sizeof(sBagPlan));
}

static void BagPlanStart(const ListScreen *list, int kind, int target, int activate, uint16_t keys)
{
    BagPlanClear();
    sBagPlan.kind = kind;
    sBagPlan.list = list;
    sBagPlan.state = list->state();
    sBagPlan.serial = list->serial();
    sBagPlan.target = target;
    sBagPlan.activate = activate;
    sBagPlan.keys = keys;
}

static void RunBagPlan(const ListScreen *list)
{
    int cur;

    if (sBagPlan.kind == BAGPLAN_NONE)
        return;
    if (sBagPlan.list != list) {
        BagPlanClear();
        return;
    }
    /* Physical controls win, except horizontal keys routed through this Bag plan. */
    if (CtrBottom_FilterGameKeys(CtrInput_Get()->held) & 0x3ffu) {
        BagPlanClear();
        return;
    }
    if (sBagPlan.release) {
        sBagPlan.release = 0;
        return;
    }
    if (list->state() != sBagPlan.state || list->serial() != sBagPlan.serial
        || ++sBagPlan.steps > 64) {
        BagPlanClear();
        return;
    }
    if (sBagPlan.kind == BAGPLAN_PRESS) {
        sInjected = sBagPlan.keys;
        BagPlanClear();
        return;
    }
    cur = sBagPlan.kind == BAGPLAN_LIST ? list->cursor() : CtrBottomBag_MenuCursor();
    if (cur == sBagPlan.target) {
        if (sBagPlan.activate)
            sInjected = K_A;
        BagPlanClear();
        return;
    }
    sInjected = cur < sBagPlan.target ? K_DOWN : K_UP;
    sBagPlan.release = 1;
}

/* A touch in the Bag's (or a case's) viewport: what it shows there, in
 * the state it is in. A list row moves the cursor there (the selected row, or CANCEL, is
 * chosen); beside the Bag the pockets switch; the arrows scroll a page; menu
 * rows are chosen; the quantity box counts and confirms; a waiting message
 * advances. Outside a context menu or the quantity box is B. */
static int ProcessBagTouch(const ListScreen *list, const CtrInput *in)
{
    int gx, gy, hit, state, cursor, page;

    if (!in->touchDown || !sHaveShown || ListFor(sShown.mode) != list)
        return HIT_NONE;
    if (!CtrPokeTouch_ViewportToGba((int)in->touchX, (int)in->touchY, &gx, &gy))
        return HIT_NONE;
    state = list->state();
    hit = list->hitAt(gx, gy);
    cursor = list->cursor();
    page = list->maxShowed() - 1;
    switch (state) {
    case BAG_LIST:
        if (hit >= 0 && hit < BAG_HIT_MENU) {
            int choose = hit == cursor || hit == list->count();
            BagPlanStart(list, BAGPLAN_LIST, hit, choose, 0);
        } else if (hit == BAG_HIT_POCKET_PREV || hit == BAG_HIT_POCKET_NEXT)
            BagPlanStart(list, BAGPLAN_PRESS, 0, 0, hit == BAG_HIT_POCKET_PREV ? K_LEFT : K_RIGHT);
        else if (hit == BAG_HIT_SCROLL_UP && page > 0)
            BagPlanStart(list, BAGPLAN_LIST, cursor - page < 0 ? 0 : cursor - page, 0, 0);
        else if (hit == BAG_HIT_SCROLL_DOWN && page > 0) {
            int last = list->count();
            BagPlanStart(list, BAGPLAN_LIST, cursor + page > last ? last : cursor + page, 0, 0);
        } else
            return HIT_NONE;
        return HIT_PANEL;
    case BAG_ACTIONS:
    case BAG_YESNO:
        if (hit >= BAG_HIT_MENU && hit < BAG_HIT_POCKET_PREV)
            BagPlanStart(list, BAGPLAN_MENU, hit - BAG_HIT_MENU, 1, 0);
        else if (state == BAG_ACTIONS)
            BagPlanStart(list, BAGPLAN_PRESS, 0, 0, K_B);
        else
            return HIT_NONE;
        return HIT_PANEL;
    case BAG_QUANTITY:
        BagPlanStart(list, BAGPLAN_PRESS, 0, 0, hit == BAG_HIT_QTY_UP ? K_UP : hit == BAG_HIT_QTY_DOWN ? K_DOWN
                                          : hit == BAG_HIT_QTY_OK ? K_A : K_B);
        return HIT_PANEL;
    case BAG_MESSAGE:
        BagPlanStart(list, BAGPLAN_PRESS, 0, 0, K_A);
        return HIT_PANEL;
    }
    return HIT_NONE;
}

/* Summary plan: presses for the real Summary's input. PAGE flips pages one
 * LEFT/RIGHT at a time (B first out of the move details) toward a page, MOVE
 * walks the move cursor to a row (A first into the details from the moves
 * page), PRESS sends one button. Each press waits for the Summary to take
 * input again (no page flip running) and expects the page it leads to; a
 * different page, Pokemon or Summary, physical input or the step limit drops
 * the plan. */
enum { SUMPLAN_NONE = 0, SUMPLAN_PAGE, SUMPLAN_MOVE, SUMPLAN_PRESS };
#define SUMMARY_MOVES 2
#define SUMMARY_MOVES_INFO 3

static struct {
    int kind;
    int target;
    int activate;
    int expect;
    int mon;
    unsigned serial;
    int steps;
    int release;
    uint16_t keys;
} sSumPlan;

static void SumPlanClear(void)
{
    memset(&sSumPlan, 0, sizeof(sSumPlan));
}

static void SumPlanStart(int kind, int target, int activate, uint16_t keys)
{
    SumPlanClear();
    sSumPlan.kind = kind;
    sSumPlan.target = target;
    sSumPlan.activate = activate;
    sSumPlan.keys = keys;
    sSumPlan.expect = CtrBottomSummary_Page();
    sSumPlan.mon = CtrBottomSummary_Mon();
    sSumPlan.serial = CtrBottomSummary_Serial();
}

static void SumPlanPress(uint16_t keys, int expect)
{
    sInjected = keys;
    sSumPlan.expect = expect;
    sSumPlan.release = 1;
}

static void RunSumPlan(void)
{
    int page;

    if (sSumPlan.kind == SUMPLAN_NONE)
        return;
    /* Physical controls always win. */
    if (CtrInput_Get()->held & 0x3ffu) {
        SumPlanClear();
        return;
    }
    if (sSumPlan.release) {
        sSumPlan.release = 0;
        return;
    }
    if (CtrBottomSummary_Serial() != sSumPlan.serial || CtrBottomSummary_Mon() != sSumPlan.mon
        || ++sSumPlan.steps > 240) {
        SumPlanClear();
        return;
    }
    /* A page flip or redraw runs: nothing is pressed until it is over. */
    if (!CtrBottomSummary_Ready())
        return;
    page = CtrBottomSummary_Page();
    if (page != sSumPlan.expect) {
        SumPlanClear();
        return;
    }
    switch (sSumPlan.kind) {
    case SUMPLAN_PRESS:
        sInjected = sSumPlan.keys;
        SumPlanClear();
        return;
    case SUMPLAN_PAGE:
        if (page == sSumPlan.target)
            SumPlanClear();
        else if (page == SUMMARY_MOVES_INFO)
            SumPlanPress(K_B, SUMMARY_MOVES);
        else
            SumPlanPress(sSumPlan.target > page ? K_RIGHT : K_LEFT, sSumPlan.target > page ? page + 1 : page - 1);
        return;
    case SUMPLAN_MOVE: {
        int cur = CtrBottomSummary_MoveCursor();
        if (page == SUMMARY_MOVES)
            SumPlanPress(K_A, SUMMARY_MOVES_INFO);
        else if (page != SUMMARY_MOVES_INFO)
            SumPlanClear();
        else if (cur == sSumPlan.target) {
            if (sSumPlan.activate)
                sInjected = K_A;
            SumPlanClear();
        } else
            SumPlanPress(cur < sSumPlan.target ? K_DOWN : K_UP, SUMMARY_MOVES_INFO);
        return;
    }
    }
}

/* The Summary's own screen (3ds_summary_ui.inc): what FireRed's Summary reports, and the touch held on it. While FireRed
 * loads a Pokemon (its setup, a switch to another one) it reports no data: the last complete report of the same Summary
 * stays on the screen meanwhile, so nothing blinks. TRAINER is FireRed's first page shown as its trainer memo: chosen by
 * its tab, and left as soon as FireRed is on another page with no plan of ours walking it there. */
static struct {
    int down, inside, btn;          /* a touch that began on a control (SUI_HIT_*) and whether it is still on it */
    int trainer;
    unsigned serial;                /* the Summary instance the state above belongs to */
    CtrSummaryView latch;           /* the last complete report */
} sSum;

static void SummaryFill(CtrSummaryUiView *v)
{
    CtrSummaryView now;

    memset(v, 0, sizeof(*v));
    if (CtrBottomSummary_Serial() != sSum.serial) {
        /* Another Summary: nothing of the last one (TRAINER, a held touch, its report) carries over. */
        memset(&sSum, 0, sizeof(sSum));
        sSum.serial = CtrBottomSummary_Serial();
    }
    CtrBottomSummary_GetView(&now);
    if (now.species[0])
        sSum.latch = now;
    if (sSum.latch.species[0] && sSum.latch.serial == now.serial)
        v->data = sSum.latch;
    else
        v->loading = 1;
    if (now.page != SUI_PAGE_INFO && sSumPlan.kind == SUMPLAN_NONE)
        sSum.trainer = 0;
    v->trainer = (uint8_t)(sSum.trainer && v->data.page == SUI_PAGE_INFO);
    v->icon = (uint16_t)(v->loading ? 0 : CtrBottomParty_IconSpecies(v->data.mon));
    v->pressed = (int16_t)(sSum.down && sSum.inside ? sSum.btn : -1);
}

/* What a control does, through the Summary's own plans and keys: a tab turns FireRed's pages to its page (out of the
 * move details first), the previous and next Pokemon are its UP and DOWN, BACK is its B; a move card opens the details
 * on that move, a row there moves the cursor to it, and the selected row (or CANCEL) is chosen with A. */
static void SummaryAct(int hit)
{
    int page = CtrBottomSummary_Page();

    if (hit >= SUI_HIT_TAB && hit < SUI_HIT_TAB + SUI_TABS) {
        static const int target[SUI_TABS] = {SUI_PAGE_INFO, SUI_PAGE_SKILLS, SUI_PAGE_MOVES, SUI_PAGE_INFO};
        int tab = hit - SUI_HIT_TAB;

        if (tab == SUI_TAB_INFO || tab == SUI_TAB_TRAINER)
            sSum.trainer = tab == SUI_TAB_TRAINER;
        if (tab == SUI_TAB_MOVES && page == SUI_PAGE_MOVES_INFO)
            return;                                 /* already there: the details are the moves' */
        if (page != target[tab])
            SumPlanStart(SUMPLAN_PAGE, target[tab], 0, 0);
    } else if (hit == SUI_HIT_PREV || hit == SUI_HIT_NEXT) {
        if (page != SUI_PAGE_MOVES_INFO)
            SumPlanStart(SUMPLAN_PRESS, 0, 0, hit == SUI_HIT_PREV ? K_UP : K_DOWN);
    } else if (hit == SUI_HIT_BACK)
        SumPlanStart(SUMPLAN_PRESS, 0, 0, K_B);
    else if (hit >= SUI_HIT_MOVE && hit <= SUI_HIT_CANCEL) {
        int row = hit - SUI_HIT_MOVE;

        SumPlanStart(SUMPLAN_MOVE, row, page == SUMMARY_MOVES_INFO && (row == 4 || row == CtrBottomSummary_MoveCursor()), 0);
    }
}

/* Press, track, release: a control is pressed while a touch that began on it stays on it, and acts once when released
 * there; sliding off cancels it. A held physical button wins. */
static int ProcessSummaryTouch(const CtrInput *in)
{
    int hit = in->touchActive && sHaveShown && sShown.summaryUi
            ? CtrSummaryUi_Hit(&sShown.sum, (int)in->touchX, (int)in->touchY) : -1;

    if (sSumPlan.kind != SUMPLAN_NONE) {        /* what the last tap asked is still being walked: no second plan over it */
        sSum.down = sSum.inside = 0;
        return HIT_NONE;
    }

    if ((in->held & 0x3ffu) || !sHaveShown || !sShown.summaryUi) {
        sSum.down = sSum.inside = 0;
        return HIT_NONE;
    }
    if (in->touchDown) {
        sSum.down = sSum.inside = hit >= 0;
        sSum.btn = hit;
        return HIT_NONE;
    }
    if (!sSum.down)
        return HIT_NONE;
    if (in->touchActive) {
        sSum.inside = hit == sSum.btn;
        return HIT_NONE;
    }
    if (sSum.inside)
        SummaryAct(sSum.btn);
    sSum.down = sSum.inside = 0;
    return HIT_PANEL;
}

/* A touch in the Summary's viewport: what its header or move rows show there.
 * A page tab turns to that page; the page name to the previous page (out of
 * the move details: back); the controls hint's page part the way it points,
 * its A part presses A (CANCEL on the first page, DETAIL on the moves page);
 * a move row opens the details there, and in them moves the cursor (the
 * selected row, or CANCEL, is chosen). */
static int ProcessSummaryGbaTouch(const CtrInput *in)
{
    int gx, gy, hit, page;

    if (!in->touchDown || !sHaveShown || sShown.mode != MODE_SUMMARY)
        return HIT_NONE;
    if (!CtrPokeTouch_ViewportToGba((int)in->touchX, (int)in->touchY, &gx, &gy))
        return HIT_NONE;
    hit = CtrBottomSummary_HitAt(gx, gy);
    page = CtrBottomSummary_Page();
    if (hit >= CTR_SUMMARY_HIT_PAGE && hit <= CTR_SUMMARY_HIT_PAGE + SUMMARY_MOVES)
        SumPlanStart(SUMPLAN_PAGE, hit - CTR_SUMMARY_HIT_PAGE, 0, 0);
    else if (hit == CTR_SUMMARY_HIT_PAGE_NAME && page == SUMMARY_MOVES_INFO)
        SumPlanStart(SUMPLAN_PRESS, 0, 0, K_B);
    else if (hit == CTR_SUMMARY_HIT_PAGE_NAME && page > 0)
        SumPlanStart(SUMPLAN_PAGE, page - 1, 0, 0);
    else if (hit == CTR_SUMMARY_HIT_CONTROLS && page < SUMMARY_MOVES)
        SumPlanStart(SUMPLAN_PAGE, page + 1, 0, 0);
    else if (hit == CTR_SUMMARY_HIT_CONTROLS && page == SUMMARY_MOVES)
        SumPlanStart(SUMPLAN_PAGE, page - 1, 0, 0);
    else if (hit == CTR_SUMMARY_HIT_BUTTON_A)
        SumPlanStart(SUMPLAN_PRESS, 0, 0, K_A);
    else if (hit >= CTR_SUMMARY_HIT_MOVE && hit <= CTR_SUMMARY_HIT_MOVE + 4) {
        int row = hit - CTR_SUMMARY_HIT_MOVE;
        SumPlanStart(SUMPLAN_MOVE, row, page == SUMMARY_MOVES_INFO
                     && (row == 4 || row == CtrBottomSummary_MoveCursor()), 0);
    } else
        return HIT_NONE;
    return HIT_PANEL;
}

/* BACK. Shown on the Bag, the TM Case and the Berry Pouch (their CANCEL is a row
 * to scroll to) and on the Summary after its first page (its A part is CANCEL
 * there); never on HOME, whose map has nothing to leave, nor on Party, which has
 * its own touchable CANCEL. Enabled while the screen takes input, the same
 * moments its physical B would count: not through a fade, a setup or a page flip. */
static int BackVisible(int mode)
{
    if (mode == MODE_FIELD)
        return NativeActive() && sScreen != PT_SAVE;   /* a native screen: back to HOME (SAVE has NO and B) */
    if (ListFor(mode) != NULL || mode == MODE_CARD || mode == MODE_DEX
        || mode == MODE_STORAGE_PC || mode == MODE_STORAGE_BOX || mode == MODE_STORAGE_CHILD)
        return 1;
    return mode == MODE_SUMMARY && !SummaryPremium() && CtrBottomSummary_Page() > 0;   /* the premium one has its own BACK */
}

/* The key that leaves the real Trainer Card from the side it is on. */
static uint16_t CardLeaveKey(void)
{
    return CtrBottomCard_State() == CARD_BACK ? K_A : K_B;
}

static void CloseNative(void)
{
    if (!NativeRoot(sScreen))
        return;
    CtrLog_Write(CTR_LOG_VIDEO, "bottom: %s closed", RootName(sScreen));
    if (sScreen == PT_SAVE && !sSession.active)
        CtrSave_End();                  /* the field was frozen and locked for it */
    sScreen = PT_NONE;
    sOpt.down = sOpt.inside = 0;
    sSave.down = sSave.inside = 0;
}

static int BackEnabled(int mode)
{
    if (mode == MODE_FIELD)
        return NativeActive();
    if (CtrBottom_IsFading())
        return 0;
    if (mode == MODE_STORAGE_PC)
        return CtrBottomStorage_PCMenuReady();
    if (mode == MODE_STORAGE_BOX)
        return CtrBottomStorage_BoxReady() || CtrBottomStorage_ContextMenu()
            || CtrBottomStorage_BoxChoose() || CtrBottomStorage_YesNo();
    if (mode == MODE_STORAGE_CHILD)
        return 0; /* its original child view handles exit, not a fake B */
    if (mode == MODE_CARD)
        return CtrBottomCard_State() != CARD_BUSY;
    if (mode == MODE_DEX)
        return CtrBottomDex_State() != DEX_BUSY;
    if (ListFor(mode) != NULL)
        return ListFor(mode)->state() != BAG_BUSY;
    return mode == MODE_SUMMARY && CtrBottomSummary_Ready();
}

/* The press: the screen's own B through its own plan (dropped by physical input,
 * a state change or a page flip, like every other press), so FireRed decides
 * where it leads: the Bag leaves for the field (or Party), a case for the Bag,
 * the Summary for Party or its moves page. */
static void BackPress(int mode)
{
    if (mode == MODE_FIELD)
        CloseNative();                  /* no GBA screen behind it: no B to send */
    else if (mode == MODE_STORAGE_PC || mode == MODE_STORAGE_BOX)
        sRouteKey = K_B; /* use FireRed's exit and carried-mon checks */
    else if (mode == MODE_CARD)
        sRouteKey = CardLeaveKey();     /* the card's own exit: B on its front, A on its back */
    else if (mode == MODE_DEX)
        sRouteKey = K_B;                /* the Pokedex's own: a level back, out of it from its top menu */
    else if (ListFor(mode) != NULL)
        BagPlanStart(ListFor(mode), BAGPLAN_PRESS, 0, 0, K_B);
    else if (mode == MODE_SUMMARY)
        SumPlanStart(SUMPLAN_PRESS, 0, 0, K_B);
}

/* Pressed on touch-down on it, released off it: nothing; released on it: once,
 * if still enabled and no physical button is held (physical input wins). */
static void ProcessBack(int mode, const CtrInput *in)
{
    int on = in->touchActive && CtrPokeTouch_HitBack((int)in->touchX, (int)in->touchY);

    if (!BackVisible(mode) || (sHaveShown && mode != sShown.mode) || (in->held & 0x3ffu)) {
        sBack.down = sBack.inside = 0;
        return;
    }
    if (in->touchDown) {
        sBack.down = sBack.inside = on && BackEnabled(mode);
        return;
    }
    if (!sBack.down)
        return;
    if (in->touchActive) {
        sBack.inside = on;
        return;
    }
    if (sBack.inside && BackEnabled(mode))
        BackPress(mode);
    sBack.down = sBack.inside = 0;
}

/* Destination router. A tap on the other root of the sidebar (PT_POKEMON,
 * PT_BAG) unwinds the real FireRed screen through its own B, one press at a
 * time and only when the screen takes input, to the field, then opens the
 * requested root there exactly as HOME does. Nothing is destroyed or opened on
 * top of anything. PT_OPTIONS is a native screen (no real menu to open); the Trainer Card and the Pokedex are real ones. Not routed: PT_SAVE. */

static int RouteTargetMode(int target)
{
    return target == PT_BAG ? MODE_BAG : target == PT_CARD ? MODE_CARD : target == PT_POKEDEX ? MODE_DEX : MODE_PARTY;
}

/* The screen shown takes its B now: no fade, no popup, message, quantity,
 * yes/no or page flip in the way. While unwinding, Party's own action menu
 * does not count against it: the Summary returns to Party with that menu
 * open, and its B closes it (FireRed's own cancel). */
static int RouteScreenReady(int mode, int unwinding)
{
    if (CtrBottom_IsFading())
        return 0;
    if (ListFor(mode) != NULL)
        return ListFor(mode)->state() == BAG_LIST;
    if (mode == MODE_SUMMARY)
        return CtrBottomSummary_Ready();
    if (mode == MODE_CARD)
        return CtrBottomCard_State() != CARD_BUSY;
    if (mode == MODE_DEX)
        return CtrBottomDex_State() != DEX_BUSY;
    if (mode == MODE_PARTY)
        /* IsOpen is only the Party's slot input: under its action menu the
         * menu's own task runs and IsOpen is false. */
        return CtrBottomParty_InputReady() && !CtrBottomParty_YesNoOpen() && !CtrBottomParty_CanAdvanceText()
            && (CtrBottomParty_IsOpen() || (unwinding && CtrBottomParty_SelectionOpen()))
            && (unwinding || !CtrBottomParty_SelectionOpen());
    return 0;
}

static int CanRouteDestination(int target, int mode)
{
    if (sSession.battle)
        return 0;
    if (target != PT_POKEMON && target != PT_BAG && target != PT_CARD && target != PT_OPTIONS && target != PT_POKEDEX)
        return 0;
    if (!sSession.active || sPendingScreen != PT_NONE || target == sScreen)
        return 0;
    return RouteScreenReady(mode, 0);
}

static void RequestDestination(int target, int mode)
{
    if (!CanRouteDestination(target, mode))
        return;
    sPendingScreen = target;
    sRouteHold = 1;
    sRouteFrames = sRouteWait = sRouteTries = sRouteReady = 0;
    sRouteMode = -1;
    sTapped = SLOT_NONE;
    CtrLog_Write(CTR_LOG_VIDEO, "bottom: route %s -> %s", RootName(sScreen), RootName(target));
}

static void RouteAbort(const char *why)
{
    CtrLog_Write(CTR_LOG_VIDEO, "bottom: route to %s abandoned: %s", RootName(sPendingScreen), why);
    sPendingScreen = PT_NONE;
    sRouteHold = 0;
    sForceRedraw = 1;
}

/* In the field: open the requested root as HOME's tap does. FireRed accepts
 * it only when the field is idle, so this is retried for a while. Failing
 * ends the session: HOME, TOP released, nothing left held. */
static void OpenDestination(int target)
{
    int opened;

    if (NativeRoot(target)) {
        /* A native screen: nothing to open. The unwound session ends in the
         * field (TOP released there) and the bottom stays held until then. */
        sPendingScreen = PT_NONE;
        sScreen = target;
        sOpt.sel = 0;
        sOpt.down = sOpt.inside = 0;
        CtrLog_Write(CTR_LOG_VIDEO, "bottom: route reached the field: %s", RootName(target));
        return;
    }
    opened = target == PT_BAG ? CtrField_TryOpenBag() : target == PT_CARD ? CtrField_TryOpenCard()
           : target == PT_POKEDEX ? CtrField_TryOpenDex() : CtrField_TryOpenParty();
    if (opened) {
        sPendingScreen = PT_NONE;
        BeginSessionPending(target);
        return;
    }
    if (++sRouteTries > ROUTE_OPEN_TRIES) {
        CtrLog_Write(CTR_LOG_VIDEO, "bottom: route to %s failed in the field: back to HOME", RootName(target));
        sPendingScreen = PT_NONE;
        sRouteHold = 0;
        sSession.active = 0;
        sScreen = PT_NONE;
        sPartyMenuCallback = NULL;
        sForceRedraw = 1;
    }
}

/* Every frame while a route is pending, before the plans run. */
static void UpdateDestinationRoute(int mode)
{
    if (sRouteHold && sPendingScreen == PT_NONE) {
        /* The requested root is up and drawn, its setup and fade-in over (or
         * the session is gone): only now show it, never the GBA's setup. The
         * fade-in may start a few frames after the screen first takes input,
         * so it must stay ready for a while. */
        if (sSession.active && !(mode == RouteTargetMode(sScreen) && RouteScreenReady(mode, 0)))
            sRouteReady = 0;
        else if (!sSession.active || ++sRouteReady >= ROUTE_STABLE) {
            sRouteHold = 0;
            sRouteReady = 0;
            sForceRedraw = 1;
        }
    }
    if (sRouteHold && ++sRouteFrames > ROUTE_TIMEOUT) {
        if (sPendingScreen != PT_NONE)
            RouteAbort("timeout");
        else {
            sRouteHold = 0;
            sForceRedraw = 1;
        }
        return;
    }
    if (sPendingScreen == PT_NONE)
        return;
    if (!sSession.active) {
        RouteAbort("session ended");
        return;
    }
    if (mode == MODE_FIELD) {
        OpenDestination(sPendingScreen);
        return;
    }
    if (mode != sRouteMode)
        sRouteWait = 0;
    if (sRouteWait > 0) {
        --sRouteWait;
        return;
    }
    if (sPlan.kind != PLAN_NONE || sBagPlan.kind != BAGPLAN_NONE || sSumPlan.kind != SUMPLAN_NONE
        || sDexPlan.active)
        return;
    if (!RouteScreenReady(mode, 1))
        return;
    /* The screen's own B: FireRed decides where it leads (Summary to Party,
     * a case to the Bag, Party or Bag to the field). The Party plan refuses
     * a B into its action menu, so that one is sent directly. */
    if (mode == MODE_PARTY && CtrBottomParty_SelectionOpen())
        sRouteKey = K_B;
    else if (mode == MODE_PARTY)
        PlanPress(K_B);
    else
        BackPress(mode);            /* (the real Trainer Card: B from its front, A from its back) */
    sRouteMode = mode;
    sRouteWait = ROUTE_COOLDOWN;
}

/* A tap on the sidebar's other root while a lower-screen session runs: pressed
 * on touch-down, cancelled by moving off, one request on release over it. */
static void ProcessRouteTouch(int mode, const CtrInput *in)
{
    if (sPendingScreen != PT_NONE || (in->held & 0x3ffu) || (sHaveShown && mode != sShown.mode)) {
        sPoke.id = PT_NONE;
        sPoke.inside = 0;
        return;
    }
    if (in->touchDown) {
        int id = CtrPokeTouch_HitTest((int)in->touchX, (int)in->touchY);

        sPoke.id = (uint8_t)(CanRouteDestination(id, mode) ? id : PT_NONE);
        sPoke.inside = sPoke.id != PT_NONE;
        return;
    }
    if (sPoke.id == PT_NONE)
        return;
    if (in->touchActive) {
        sPoke.inside = CtrPokeTouch_HitTest((int)in->touchX, (int)in->touchY) == sPoke.id;
        return;
    }
    {
        int id = sPoke.id, inside = sPoke.inside;

        sPoke.id = PT_NONE;
        sPoke.inside = 0;
        if (inside)
            RequestDestination(id, mode);
    }
}

/* Session: TOP hold across the hidden Party menu. */

static void BeginSession(int owner)
{
    if (!sSession.active)
        CtrLog_Write(CTR_LOG_VIDEO, "bottom: hidden %s session", RootName(owner));
    sSession.owner = owner;
    sSession.battle = CtrBottomParty_InBattle() || CtrBottomBag_InBattle()
                   || CtrBottomSummary_Origin() == CTR_SUMMARY_BATTLE_PARTY;
    sScreen = owner;
    sSession.active = 1;
    sSession.entered = 0;
    sSession.partyCallback = CtrBottomParty_Callback();
    sSession.frames = 0;
    sSession.away = 0;
}

static int UpdateSession(int mode, int planRunning)
{
    int inParty = (mode == MODE_PARTY);
    int home = CtrBottom_IsFieldCallback() || CtrBottom_IsBattleMain();

    (void)planRunning;
    /* Picked in the START menu: TOP keeps the field from the pick, before the
     * fade, so the faded frames are never what it holds. */
    if (!sSession.active && mode == MODE_FIELD && CtrPokeTouch_LeavingFor(PT_POKEMON))
        BeginSessionPending(PT_POKEMON);
    if (!sSession.active && mode == MODE_FIELD && CtrPokeTouch_LeavingFor(PT_BAG))
        BeginSessionPending(PT_BAG);
    if (!sSession.active && mode == MODE_FIELD && CtrPokeTouch_LeavingFor(PT_CARD))
        BeginSessionPending(PT_CARD);
    if (!sSession.active && mode == MODE_FIELD && CtrPokeTouch_LeavingFor(PT_POKEDEX))
        BeginSessionPending(PT_POKEDEX);
    /* A Summary opened from the PC belongs to that PC, not to an ordinary
     * PokÃ©Touch Party session. Otherwise a box's Summary can leave a stale
     * session which intercepts later input and keeps the TOP held on exit. */
    if ((inParty || mode == MODE_SUMMARY || mode == MODE_PARTY_CHILD) && !sSession.active && !sStorageSession)
        BeginSession(PT_POKEMON);
    if (mode == MODE_BAG && !sSession.active && !sStorageSession)
        BeginSession(PT_BAG);
    if (mode == MODE_CARD && !sSession.active)
        BeginSession(PT_CARD);
    if (mode == MODE_DEX && !sSession.active)
        BeginSession(PT_POKEDEX);
    if (!sSession.active)
        return 0;
    ++sSession.frames;
    if (mode == MODE_BAG || IS_CASE_MODE(mode) || mode == MODE_CARD || mode == MODE_DEX) {
        sSession.entered = 1;
        sSession.away = 0;
        return 1;
    }
    if (mode == MODE_PARTY_CHILD) {
        sSession.entered = 1;
        sSession.away = 0;
        return 1;
    }
    if (mode == MODE_SUMMARY) {
        /* A Summary opened from the battle's Party belongs to the battle for as long as it exists, though FireRed's
         * callback is the Summary's own: TOP keeps the battle, nothing routes to the field's screens. */
        if (CtrBottomSummary_Origin() == CTR_SUMMARY_BATTLE_PARTY)
            sSession.battle = 1;
        sSession.away = 0;
        return 1;
    }
    if (CtrBottomParty_UsesTopPresentation())
        return 0;
    if (inParty) {
        /* Still on the remembered Party callback? A pending home request or
         * a Party-owned setup frame re-anchors to the live Party callback
         * (Init and Update use different callbacks). */
        void *cur = CtrBottom_CurrentCallback();
        if (sSession.partyCallback == NULL
            || (cur != sSession.partyCallback && CtrBottomParty_IsPartyCallback())) {
            sSession.partyCallback = cur;
            sSession.entered = 1;
            sSession.away = 0;
            return 1;
        }
        if (cur != sSession.partyCallback) {
            /* Summary/Fly map pushed on top: brief grace, then show it. */
            if (++sSession.away < BRIDGE_MAX || sPendingScreen != PT_NONE)
                return 1;
            return 0;
        }
        sSession.entered = 1;
        sSession.away = 0;
        return 1;
    }
    if (home) {
        sSession.away = 0;
        /* A route passes through the field: not the end of the session. */
        if (sPendingScreen != PT_NONE)
            return 1;
        if ((sSession.entered || sSession.frames > 30) && !CtrBottom_IsFading()) {
            sSession.active = 0;
            if (!NativeRoot(sScreen))   /* a route to a native screen ends its session here */
                sScreen = PT_NONE;
            sPartyMenuCallback = NULL;
            CtrLog_Write(CTR_LOG_VIDEO, "bottom: hidden %s session over", RootName(sSession.owner));
            return 0;
        }
        return 1;
    }
    /* Setup / map reload passes through here briefly (a route's unwind and
     * open take as long as they take: its own timeout bounds it). */
    return ++sSession.away < BRIDGE_MAX || sPendingScreen != PT_NONE;
}

static void BeginSessionPending(int owner)
{
    sSession.owner = owner;
    sSession.battle = 0;
    sScreen = owner;
    sSession.active = 1;
    sSession.entered = 0;
    sSession.partyCallback = NULL;
    sSession.frames = 0;
    sSession.away = 0;
    /* The root opens from the field or a route: its picture is not shown (the
     * viewport would sample the GBA's half-built screen) until it is ready. */
    sRouteHold = 1;
    sRouteFrames = 0;
    sRouteReady = 0;
    CtrLog_Write(CTR_LOG_VIDEO, "bottom: %s session requested from home", RootName(owner));
}

/* PokeTouch actions: FireRed's own entry points. In an open START menu an
 * entry is picked there (as its A or B would); from the field the request
 * runs where FireRed handles START/SELECT. Pokemon and the Bag open from the
 * field directly, without the START menu (CtrField_TryOpenParty/TryOpenBag). */
static void PokeTouchActivate(int id)
{
    int menu = CtrPokeTouch_StartMenuOpen();

    switch (id) {
    case PT_POKEMON:
        if (menu)
            CtrPokeTouch_Pick(id);
        else if (CtrField_TryOpenParty())
            BeginSessionPending(PT_POKEMON);
        break;
    case PT_BAG:
        if (menu)
            CtrPokeTouch_Pick(id);
        else if (CtrField_TryOpenBag())
            BeginSessionPending(PT_BAG);
        break;
    case PT_CARD:
        /* The real Trainer Card, opened directly (no START menu). From the native
         * OPTIONS the bottom keeps showing it until the card is ready. */
        if (!menu && CtrField_TryOpenCard())
            BeginSessionPending(PT_CARD);
        break;
    case PT_OPTIONS:
        /* The native screen, never FireRed's own Options menu. */
        if (!menu && sScreen != id) {
            sScreen = id;
            sOpt.sel = 0;
            sOpt.down = sOpt.inside = 0;
            CtrLog_Write(CTR_LOG_VIDEO, "bottom: %s opened", RootName(id));
        }
        break;
    case PT_SAVE:
        /* FireRed's save, never its START entry or original dialogue; the field is locked first. */
        if (!menu && CtrSave_Begin())
            OpenSave();
        break;
    case PT_POKEDEX:
        /* The real Pokedex, opened directly (no START menu); from the native OPTIONS the bottom keeps
         * showing it until the Pokedex is ready. */
        if (menu)
            CtrPokeTouch_Pick(id);
        else if (CtrField_TryOpenDex())
            BeginSessionPending(PT_POKEDEX);
        break;
    case PT_BIKE:
    case PT_REGISTERED:
        CtrPokeTouch_RequestFieldCommand((unsigned)id);
        break;
    case PT_RUN:
        CtrPokeTouch_ToggleRun();
        break;
    }
}

/* What the field's sidebar can do now. OPTIONS is the native screen: never
 * with FireRed's own START menu up (its entry would open FireRed's Options),
 * and no-op once open; while it is open the entries that have no destination
 * of their own yet are off. */
static int FieldEnabled(int id)
{
    /* While SAVE is up the field is locked (so FireRed's own checks say no to everything) and the
     * sidebar is off: SAVE's NO, its B and its OK are the ways out. */
    if (SaveActive() || !CtrPokeTouch_Enabled(id))
        return 0;
    if (id == PT_OPTIONS || id == PT_CARD || id == PT_SAVE)
        return !CtrPokeTouch_StartMenuOpen() && !(id == PT_OPTIONS && OptionsActive());
    return 1;
}

/* A touch on an OPTIONS row (the viewport shows no map then): selected on
 * touch-down, one step on release over the same zone ("<" back, the value or
 * ">" forward; the label only selects). Moving off cancels it; a held
 * physical button cancels it too. */
static void ProcessOptionsTouch(const CtrInput *in)
{
    int row, zone, on = in->touchActive && CtrPokeTouch_OptionHit((int)in->touchX, (int)in->touchY,
                                                               CtrOptions_RowCount(), &row, &zone);

    if (in->held & 0x3ffu) {
        sOpt.down = sOpt.inside = 0;
        return;
    }
    if (in->touchDown) {
        sOpt.down = sOpt.inside = on;
        if (on) {
            sOpt.row = row;
            sOpt.zone = zone;
            sOpt.sel = row;
        }
        return;
    }
    if (!sOpt.down)
        return;
    if (in->touchActive) {
        sOpt.inside = on && row == sOpt.row && zone == sOpt.zone;
        return;
    }
    if (sOpt.inside && sOpt.zone != PT_OPT_ZONE_LABEL)
        CtrOptions_Step(sOpt.row, sOpt.zone == PT_OPT_ZONE_PREV ? -1 : 1);
    sOpt.down = sOpt.inside = 0;
}

/* SAVE opens on its confirmation (YES first; NO first when it would replace another game file). */
static void OpenSave(void)
{
    sScreen = PT_SAVE;
    sSave.state = SAVE_CONFIRM;
    sSave.sel = CtrSave_IsDifferentFile() ? 1 : 0;
    sSave.down = sSave.inside = sSave.wait = 0;
    CtrLog_Write(CTR_LOG_VIDEO, "bottom: save opened");
}

/* SAVE's buttons: YES (PT_SAVE_FIRST) and NO while confirming, OK after a result. Pressing is a touch that
 * began on a button and is still on it; releasing there acts once. */
static void SaveActivate(int which)
{
    if (sSave.state == SAVE_CONFIRM) {
        if (which == PT_SAVE_FIRST) {
            sSave.state = SAVE_WRITING;     /* shown now; the save runs on the next frame */
            sSave.wait = 2;
            CtrLog_Write(CTR_LOG_VIDEO, "bottom: save requested");
        } else {
            CloseNative();                  /* NO: back to HOME */
        }
    } else if (sSave.state == SAVE_SUCCESS) {
        CloseNative();
    } else if (sSave.state == SAVE_ERROR) {
        sSave.state = SAVE_CONFIRM;         /* try again, or cancel */
        sSave.sel = 0;
    }
}

/* Physical input first: UP and DOWN choose, A acts, B cancels (OK on a result acts on A or B). A held
 * button ignores the touch; no key or touch does anything while the save is being written. */
static void ProcessSaveInput(const CtrInput *in)
{
    int hit = in->touchActive ? CtrPokeTouch_SaveHit((int)in->touchX, (int)in->touchY, sSave.state) : PT_SAVE_NONE;

    if (sSave.state == SAVE_WRITING) {
        sSave.down = sSave.inside = 0;
        return;
    }
    if (in->down & (K_UP | K_DOWN | K_A | K_B)) {
        sSave.down = sSave.inside = 0;
        if (sSave.state == SAVE_CONFIRM && (in->down & (K_UP | K_DOWN)))
            sSave.sel = !sSave.sel;
        else if (in->down & K_A)
            SaveActivate(sSave.state == SAVE_CONFIRM ? PT_SAVE_FIRST + sSave.sel : PT_SAVE_FIRST);
        else if (in->down & K_B)
            SaveActivate(sSave.state == SAVE_CONFIRM ? PT_SAVE_SECOND : PT_SAVE_FIRST);
        return;
    }
    if (in->held & 0x3ffu) {
        sSave.down = sSave.inside = 0;
        return;
    }
    if (in->touchDown) {
        sSave.down = hit;
        sSave.inside = hit != PT_SAVE_NONE;
        if (hit != PT_SAVE_NONE && sSave.state == SAVE_CONFIRM)
            sSave.sel = hit - PT_SAVE_FIRST;
        return;
    }
    if (!sSave.down)
        return;
    if (in->touchActive) {
        sSave.inside = hit == sSave.down;
        return;
    }
    {
        int which = sSave.down, inside = sSave.inside;

        sSave.down = sSave.inside = 0;
        if (inside)
            SaveActivate(which);
    }
}

/* Every frame while SAVE is up: the write itself happens here, one frame after YES (so the writing
 * dialogue is on the screen), and what it reports, FireRed's own result, decides what is shown next. */
static void UpdateSave(void)
{
    if (!SaveActive())
        return;
    if (sSave.state == SAVE_WRITING && sSave.wait > 0 && --sSave.wait == 0) {
        int ok = CtrSave_Run();

        CtrLog_Write(CTR_LOG_VIDEO, "bottom: save %s", ok ? "written" : "FAILED");
        sSave.state = ok ? SAVE_SUCCESS : SAVE_ERROR;
    }
}

/* Touch-down presses an enabled element, moving off releases the highlight,
 * release over it acts once. Holding never repeats. */
static void ProcessPokeTouch(const CtrInput *in)
{
    if (OptionsActive())
        ProcessOptionsTouch(in);
    if (SaveActive())
        ProcessSaveInput(in);
    if (in->touchDown) {
        int id = CtrPokeTouch_HitTest((int)in->touchX, (int)in->touchY), gx, gy;
        sPoke.id = (uint8_t)(id != PT_NONE && FieldEnabled(id) ? id : PT_NONE);
        sPoke.inside = sPoke.id != PT_NONE;
        /* A touch on the map (the viewport): a tap when released near where it began.
         * The OPTIONS rows are there instead while that screen is up. */
        sHome.down = !NativeActive() && id == PT_NONE && CtrPokeTouch_ViewportToGba((int)in->touchX, (int)in->touchY, &gx, &gy);
        if (sHome.down) {
            sHome.downX = sHome.lastX = gx;
            sHome.downY = sHome.lastY = gy;
        }
        return;
    }
    if (sHome.down) {
        int gx, gy;
        if (in->touchActive) {
            if (CtrPokeTouch_ViewportToGba((int)in->touchX, (int)in->touchY, &gx, &gy)) {
                sHome.lastX = gx;
                sHome.lastY = gy;
            } else {
                sHome.lastX = sHome.lastY = -1000;    /* left the map */
            }
            return;
        }
        sHome.down = 0;
        if (sHome.lastX - sHome.downX <= MAP_TAP_SLOP && sHome.downX - sHome.lastX <= MAP_TAP_SLOP
            && sHome.lastY - sHome.downY <= MAP_TAP_SLOP && sHome.downY - sHome.lastY <= MAP_TAP_SLOP)
            HomeTap(sHome.downX, sHome.downY);
        return;
    }
    if (sPoke.id == PT_NONE)
        return;
    if (in->touchActive) {
        sPoke.inside = CtrPokeTouch_HitTest((int)in->touchX, (int)in->touchY) == sPoke.id;
        return;
    }
    int id = sPoke.id, inside = sPoke.inside;
    sPoke.id = PT_NONE;
    sPoke.inside = 0;
    if (inside && FieldEnabled(id))
        PokeTouchActivate(id);
}

/* The battle screen's touch: press, track, release, one action. A button walks the controller's own cursor to
 * it with its own D-pad and then sends its A; CANCEL sends its B. The buttons exist only while the controller
 * waits at the screen they belong to, so nothing is taken during a message, an animation, a command being taken or the battle's end; a held
 * physical button cancels it. */
static struct {
    int active, target, state, steps, release;
    uint16_t press;                 /* one key to send as it is (CANCEL's B), instead of walking */
} sBatPlan;

static void BatPlanClear(void)
{
    memset(&sBatPlan, 0, sizeof(sBatPlan));
}

/* The next D-pad key from slot `cur` toward `target`. The action menu follows the graph of 3ds_battle_nav.h
 * (the one the controller's own D-pad uses); the move list is the controller's 2x2 grid, without stepping onto
 * a slot that has nothing in it (the controller would refuse). 0 if there is no way. */
static uint16_t BatActionKey(int cur, int target)
{
    static const uint16_t keys[CTR_NAV_DIRS] = {K_UP, K_DOWN, K_LEFT, K_RIGHT};
    int dir = CtrBattleActionDir(cur, target);

    return dir < 0 ? 0 : keys[dir];
}

static uint16_t BatKey(int cur, int target, int count)
{
    int diff = cur ^ target;

    if ((diff & 1) && (cur ^ 1) < count)
        return (cur & 1) ? K_LEFT : K_RIGHT;
    if ((diff & 2) && (cur ^ 2) < count)
        return (cur & 2) ? K_UP : K_DOWN;
    return 0;
}

static int BatCount(int state)
{
    int n = 0;

    if (state == BATTLE_ACTION || state == BATTLE_SAFARI)
        return 4;
    for (int i = 0; i < 4; ++i)
        n += sShown.battle.move[i].present;
    return n;
}

static void RunBatPlan(void)
{
    int state = CtrBattle_State(), cur;

    if (!sBatPlan.active)
        return;
    if ((CtrInput_Get()->held & 0x3ffu) || state != sBatPlan.state || !sHaveShown || sShown.mode != MODE_BATTLE) {
        BatPlanClear();
        return;
    }
    if (sBatPlan.release) {
        sBatPlan.release = 0;
        return;
    }
    if (++sBatPlan.steps > 10) {
        BatPlanClear();
        return;
    }
    if (sBatPlan.press) {
        sInjected = sBatPlan.press;
        BatPlanClear();
        return;
    }
    if (state == BATTLE_TARGET) {
        /* The controller's target cursor goes round the legal battlers with RIGHT; A when it is on the one asked. */
        if (CtrBattle_TargetCursor() == sBatPlan.target) {
            sInjected = K_A;
            BatPlanClear();
            return;
        }
        sInjected = K_RIGHT;
        sBatPlan.release = 1;
        return;
    }
    cur = state == BATTLE_ACTION || state == BATTLE_SAFARI ? CtrBattle_ActionCursor() : CtrBattle_MoveCursor();
    if (cur == sBatPlan.target) {
        sInjected = K_A;
        BatPlanClear();
        return;
    }
    sInjected = state == BATTLE_ACTION ? BatActionKey(cur, sBatPlan.target) : BatKey(cur, sBatPlan.target, BatCount(state));
    if (sInjected == 0) {
        BatPlanClear();
        return;
    }
    sBatPlan.release = 1;
}

static void ProcessBattleTouch(const CtrInput *in)
{
    int state = CtrBattle_State(), btn = -1;
    int shows = state == BATTLE_ACTION ? BUI_ACTION : state == BATTLE_MOVE ? BUI_MOVE
              : state == BATTLE_TARGET ? BUI_TARGET : state == BATTLE_SAFARI ? BUI_SAFARI : BUI_IDLE;
    int ready = shows != BUI_IDLE && sHaveShown && sShown.mode == MODE_BATTLE && sShown.battle.state == shows;

    if (in->touchDown)
        sBatTouchLast = 1;
    else if (in->held & 0x3ffu)
        sBatTouchLast = 0;
    if (in->touchActive && sHaveShown)
        btn = CtrBattleUi_Hit(&sShown.battle, (int)in->touchX, (int)in->touchY);
    if ((in->held & 0x3ffu) || !ready || (sBat.down && sBat.state != state)) {
        sBat.down = sBat.inside = 0;
        return;
    }
    if (in->touchDown) {
        BatPlanClear();
        sBat.down = sBat.inside = btn >= 0;
        sBat.btn = btn;
        sBat.state = state;
        return;
    }
    if (!sBat.down)
        return;
    if (in->touchActive) {
        sBat.inside = btn == sBat.btn;
        return;
    }
    if (sBat.inside) {
        BatPlanClear();
        sBatPlan.active = 1;
        sBatPlan.state = state;
        if (sBat.btn == BUI_CANCEL)
            sBatPlan.press = K_B;               /* the controller's own cancel: back to the action menu, no choice made */
        else if (state == BATTLE_TARGET)
            sBatPlan.target = sShown.battle.target[sBat.btn].battler;    /* the battler the card stands for */
        else
            sBatPlan.target = sBat.btn;
    }
    sBat.down = sBat.inside = 0;
}

/* The battle Bag's touch: the same press, track, release, and the same Bag plans as every Bag (walk the Bag's own
 * list cursor to an item, send A / B, walk its menu), so FireRed's Bag decides everything: a tap on another item
 * selects it, a tap on the selected one sends A and then A again on the first row of the menu that opens (USE, or
 * CANCEL for an item with no use here); a tab presses LEFT / RIGHT until the Bag is in that pocket; CANCEL is B.
 * An invalid use stays in the Bag with the Bag's own message (shown, A to continue). A held physical button
 * cancels it all, except the horizontal D-pad that shares the pocket router. */
static struct {
    int pocketTarget, pocketWait;   /* the pocket the last tab tap or D-pad press asked for (-1 none), frames left to get there */
    int autoWait;                   /* frames left to confirm USE once the item's menu opens */
} sBatBag = {-1, 0, 0};

uint16_t CtrBottom_FilterGameKeys(uint16_t held)
{
    /* Keep a held direction out during the router's busy frames too: it must neither
     * cancel its own plan nor leak FireRed's internal pocket order to the game. */
    if (CurrentMode() == MODE_BAG && CtrBottomBag_InBattle()
        && (CtrBottomBag_State() == BAG_LIST || sBatBag.pocketTarget >= 0))
        held &= ~(K_LEFT | K_RIGHT);
    return held;
}

static int BattleBagVisiblePocketStep(int pocket, uint16_t keys)
{
    if (keys == K_RIGHT)
        return pocket == 0 ? 2 : pocket == 2 ? 1 : pocket;
    if (keys == K_LEFT)
        return pocket == 1 ? 2 : pocket == 2 ? 0 : pocket;
    return pocket;
}

static void ProcessBattleBagTouch(const CtrInput *in)
{
    const ListScreen *list = &sBagScreen;
    int state = CtrBottomBag_State(), btn = -1;
    uint16_t physical = CtrBottom_FilterGameKeys(in->held) & 0x3ffu;
    int ready = sHaveShown && sShown.mode == MODE_BAG && sShown.battleUi && sShown.battle.state == BUI_BAG
             && state != BAG_BUSY && sShown.battle.bag.bagState == state;

    if (in->touchDown)
        sBatTouchLast = 1;
    else if (in->held & 0x3ffu)
        sBatTouchLast = 0;
    if (physical) {
        sBatBag.pocketTarget = -1;
        sBatBag.autoWait = 0;
    }
    if (!physical && ready && state == BAG_LIST && !NavPending(MODE_BAG)
        && (in->down & (K_LEFT | K_RIGHT))) {
        int pocket = CtrBottomBag_Pocket();
        int target = BattleBagVisiblePocketStep(pocket, in->held & (K_LEFT | K_RIGHT));

        if (target != pocket) {
            BagPlanClear();
            sBatBag.autoWait = 0;
            sBatBag.pocketTarget = target;
            sBatBag.pocketWait = 40;
        }
    }
    /* What an earlier tap or D-pad press still has to do, never over a running plan. */
    if (!physical && sBagPlan.kind == BAGPLAN_NONE) {
        if (sBatBag.autoWait > 0) {
            --sBatBag.autoWait;
            if (state == BAG_ACTIONS) {
                BagPlanStart(list, BAGPLAN_MENU, 0, 1, 0);
                sBatBag.autoWait = 0;
            }
        } else if (sBatBag.pocketTarget >= 0) {
            int pocket = CtrBottomBag_Pocket();

            if (pocket == sBatBag.pocketTarget || --sBatBag.pocketWait <= 0)
                sBatBag.pocketTarget = -1;
            else if (state == BAG_LIST)
                BagPlanStart(list, BAGPLAN_PRESS, 0, 0, pocket < sBatBag.pocketTarget ? K_RIGHT : K_LEFT);
        }
    }
    if (NavPending(MODE_BAG)) {               /* a pocket is being walked to: its tap is not answered twice */
        sBat.down = sBat.inside = 0;
        return;
    }
    if (in->touchActive && sHaveShown)
        btn = CtrBattleUi_Hit(&sShown.battle, (int)in->touchX, (int)in->touchY);
    if ((in->held & 0x3ffu) || !ready || (sBat.down && sBat.state != state)) {
        sBat.down = sBat.inside = 0;
        return;
    }
    if (in->touchDown) {
        sBagPlan.kind = BAGPLAN_NONE;
        sBatBag.pocketTarget = -1;
        sBatBag.autoWait = 0;
        sBat.down = sBat.inside = btn >= 0;
        sBat.btn = btn;
        sBat.state = state;
        return;
    }
    if (!sBat.down)
        return;
    if (in->touchActive) {
        sBat.inside = btn == sBat.btn;
        return;
    }
    if (sBat.inside) {
        const CtrBattleBag *b = &sShown.battle.bag;
        int code = sBat.btn;

        if (code >= BUI_BAG_ROW && code < BUI_BAG_ROW + BUI_BAG_ROWS) {
            int index = b->first + code - BUI_BAG_ROW, choose = index == b->cursor;

            BagPlanStart(list, BAGPLAN_LIST, index, choose, 0);
            if (choose)
                sBatBag.autoWait = 90;
        } else if (code >= BUI_BAG_TAB && code < BUI_BAG_TAB + 3) {
            sBatBag.pocketTarget = CtrBattleUi_TabPocket(code - BUI_BAG_TAB);
            sBatBag.pocketWait = 40;
        } else if (code >= BUI_BAG_MENU && code < BUI_BAG_MENU + 3)
            BagPlanStart(list, BAGPLAN_MENU, code - BUI_BAG_MENU, 1, 0);
        else if (code == BUI_BAG_CANCEL)
            BagPlanStart(list, BAGPLAN_PRESS, 0, 0, state == BAG_MESSAGE ? K_A : K_B);
        else if (code == BUI_BAG_MESSAGE)
            BagPlanStart(list, BAGPLAN_PRESS, 0, 0, K_A);
        else if (code == BUI_BAG_UP)
            BagPlanStart(list, BAGPLAN_LIST, b->cursor - (BUI_BAG_ROWS - 1) < 0 ? 0 : b->cursor - (BUI_BAG_ROWS - 1), 0, 0);
        else if (code == BUI_BAG_DOWN)
            BagPlanStart(list, BAGPLAN_LIST, b->cursor + (BUI_BAG_ROWS - 1) > b->count - 1 ? b->count - 1
                                                                                        : b->cursor + (BUI_BAG_ROWS - 1), 0, 0);
    }
    sBat.down = sBat.inside = 0;
}

/* The battle Party's touch: press, track, release, one action. A slot is the Party's own touch (it selects that slot and takes
 * its A, so FireRed's Party decides what A means here: its menu, or the item's use, or the switch), CANCEL its B; a row of the
 * menu or the YES / NO it has open is walked to with the Party's plans, and a message it waits on takes A. Nothing is taken while
 * the Party fades, sets up or runs a text of its own. A held physical button cancels it. */
static void ProcessBattlePartyTouch(const CtrInput *in)
{
    int btn = -1;
    int ready = sHaveShown && sShown.mode == MODE_PARTY && sShown.battleUi && sShown.battle.state == BUI_PARTY
             && !CtrBottom_IsFading() && sShown.serial == CtrBottomParty_MenuSerial();

    if (in->touchActive && sHaveShown)
        btn = CtrBattleUi_Hit(&sShown.battle, (int)in->touchX, (int)in->touchY);
    if ((in->held & 0x3ffu) || !ready) {
        sBat.down = sBat.inside = 0;
        return;
    }
    if (in->touchDown) {
        sBat.down = sBat.inside = btn >= 0;
        sBat.btn = btn;
        return;
    }
    if (!sBat.down)
        return;
    if (in->touchActive) {
        sBat.inside = btn == sBat.btn;
        return;
    }
    if (sBat.inside) {
        int code = sBat.btn, open = CtrBottomParty_SelectionOpen() || CtrBottomParty_YesNoOpen();

        if (code >= BUI_PTY_SLOT && code < BUI_PTY_SLOT + 6) {
            if (!open && CtrBottomParty_IsOpen() && !CtrBottomParty_CanAdvanceText())
                CtrBottomParty_TouchSlot(code - BUI_PTY_SLOT);
        } else if (code == BUI_PTY_CANCEL) {
            if (!open && CtrBottomParty_IsOpen() && !CtrBottomParty_CanAdvanceText())
                CtrBottomParty_TouchSlot(PARTY_SIZE + 1);
        } else if (code >= BUI_PTY_MENU && code < BUI_PTY_MENU + 6) {
            if (open)
                PlanStart(PLAN_MENU, code - BUI_PTY_MENU);
        } else if (code == BUI_PTY_MESSAGE && CtrBottomParty_CanAdvanceText())
            PlanPress(K_A);
    }
    sBat.down = sBat.inside = 0;
}

/* A tap on the real Trainer Card (anywhere in the viewport): touch-down arms it,
 * moving off cancels, release over it sends the card's own key once: A on its
 * front, which flips it (FireRed's own flip animation, not ours), B on its back,
 * which flips it back. Only while the card takes input, so never during the flip
 * or a fade; a held physical button wins. */
static struct {
    int down, inside;
} sCardTap;

static void ProcessCardTouch(const CtrInput *in)
{
    int gx, gy, on = in->touchActive && CtrPokeTouch_ViewportToGba((int)in->touchX, (int)in->touchY, &gx, &gy);
    int ready = CtrBottomCard_State() != CARD_BUSY && !CtrBottom_IsFading() && !(sHaveShown && sShown.mode != MODE_CARD);

    if ((in->held & 0x3ffu) || !ready) {
        sCardTap.down = sCardTap.inside = 0;
        return;
    }
    if (in->touchDown) {
        sCardTap.down = sCardTap.inside = on;
        return;
    }
    if (!sCardTap.down)
        return;
    if (in->touchActive) {
        sCardTap.inside = on;
        return;
    }
    if (sCardTap.inside)
        sRouteKey = CtrBottomCard_State() == CARD_BACK ? K_B : K_A;
    sCardTap.down = sCardTap.inside = 0;
}

/* The real Pokedex's lists (its top menu, the numerical and search lists). Rows are FireRed's own
 * (CtrBottomDex_RowAt): a tap on one moves the list's cursor there with the list's own UP/DOWN presses,
 * and a tap on the row that is already selected sends its A, so the Pokedex opens it. The scroll arrows
 * send UP/DOWN once on a tap, and repeat while held. Nothing happens on touch-down; a release off the
 * row (or arrow) it began on cancels; a held physical button or a state change drops it. */
#define DEX_REPEAT_DELAY 20
#define DEX_REPEAT_EVERY 6
static struct {
    int down, inside, row, arrow, held;
} sDexTap;
static void DexPlanClear(void)
{
    memset(&sDexPlan, 0, sizeof(sDexPlan));
}

static void RunDexPlan(void)
{
    int cur;

    if (!sDexPlan.active)
        return;
    if ((CtrInput_Get()->held & 0x3ffu) || sShown.mode != MODE_DEX) {
        DexPlanClear();
        return;
    }
    if (sDexPlan.release) {
        sDexPlan.release = 0;
        return;
    }
    if (CtrBottomDex_State() != sDexPlan.state || ++sDexPlan.steps > 64) {
        DexPlanClear();
        return;
    }
    if (sDexPlan.kind == DEXPLAN_FLIP) {
        /* FireRed's own LEFT/RIGHT: it steps the cursor along the page, and at the page's edge flips it. The
         * flip ends the plan (the screen is no longer the one it began on). */
        if (CtrBottomDex_Page() != sDexPlan.page) {
            DexPlanClear();
            return;
        }
        sInjected = sDexPlan.target < 0 ? K_LEFT : K_RIGHT;
        sDexPlan.release = 1;
        return;
    }
    cur = sDexPlan.kind == DEXPLAN_SLOT ? CtrBottomDex_PageSlot() : CtrBottomDex_Cursor();
    if (cur == sDexPlan.target) {
        DexPlanClear();
        return;
    }
    sInjected = sDexPlan.kind == DEXPLAN_SLOT ? (cur < sDexPlan.target ? K_RIGHT : K_LEFT)
              : cur < sDexPlan.target ? K_DOWN : K_UP;
    sDexPlan.release = 1;
}

static void ProcessDexTouch(const CtrInput *in)
{
    int state = CtrBottomDex_State();
    int gx = 0, gy = 0, on = in->touchActive && CtrPokeTouch_ViewportToGba((int)in->touchX, (int)in->touchY, &gx, &gy);
    int row = on ? CtrBottomDex_RowAt(gx, gy) : -1, arrow = on ? CtrBottomDex_ArrowAt(gx, gy) : 0;
    int ready = (state == DEX_MENU || state == DEX_LIST) && !CtrBottom_IsFading()
             && !(sHaveShown && sShown.mode != MODE_DEX);

    if ((in->held & 0x3ffu) || !ready) {
        sDexTap.down = sDexTap.inside = 0;
        return;
    }
    if (in->touchDown) {
        DexPlanClear();
        sDexTap.down = sDexTap.inside = row >= 0 || arrow != 0;
        sDexTap.row = row;
        sDexTap.arrow = arrow;
        sDexTap.held = 0;
        return;
    }
    if (!sDexTap.down)
        return;
    if (in->touchActive) {
        sDexTap.inside = on && row == sDexTap.row && arrow == sDexTap.arrow;
        if (sDexTap.inside && sDexTap.arrow != 0 && ++sDexTap.held >= DEX_REPEAT_DELAY
            && (sDexTap.held - DEX_REPEAT_DELAY) % DEX_REPEAT_EVERY == 0)
            sRouteKey = sDexTap.arrow < 0 ? K_UP : K_DOWN;
        return;
    }
    if (sDexTap.inside) {
        if (sDexTap.arrow != 0) {
            if (sDexTap.held < DEX_REPEAT_DELAY)
                sRouteKey = sDexTap.arrow < 0 ? K_UP : K_DOWN;
        } else if (sDexTap.row == CtrBottomDex_Cursor())
            sRouteKey = K_A;
        else {
            DexPlanClear();
            sDexPlan.active = 1;
            sDexPlan.target = sDexTap.row;
            sDexPlan.state = state;
        }
    }
    sDexTap.down = sDexTap.inside = 0;
}

/* The habitat/group page (up to four Pokemon, "PAGE 01/10"): a tap on a slot (its picture, number and name)
 * walks FireRed's cursor to it with its own LEFT/RIGHT, a tap on the selected slot sends its A (CHECK); a
 * swipe across the viewport flips the page with the same LEFT/RIGHT at the edge of the page (the page
 * flips by LEFT/RIGHT whatever the button mode, and L/R only in some modes), once, and only toward a page
 * that exists. A tap moves at most DEX_TAP_SLOP pixels; a swipe at least DEX_SWIPE_MIN, mostly sideways.
 * Nothing on touch-down or while held; sliding off the slot cancels; physical input wins. */
#define DEX_TAP_SLOP 8
#define DEX_SWIPE_MIN 24
static struct {
    int down, x0, y0, x1, y1, slot;
} sDexPage;

static void ProcessDexPageTouch(const CtrInput *in)
{
    int gx = 0, gy = 0, on = in->touchActive && CtrPokeTouch_ViewportToGba((int)in->touchX, (int)in->touchY, &gx, &gy);
    int ready = CtrBottomDex_State() == DEX_CATEGORY && !CtrBottom_IsFading() && !(sHaveShown && sShown.mode != MODE_DEX);

    if ((in->held & 0x3ffu) || !ready) {
        sDexPage.down = 0;
        return;
    }
    if (in->touchDown) {
        DexPlanClear();
        sDexPage.down = on;
        sDexPage.x0 = sDexPage.x1 = gx;
        sDexPage.y0 = sDexPage.y1 = gy;
        sDexPage.slot = on ? CtrBottomDex_PageSlotAt(gx, gy) : -1;
        return;
    }
    if (!sDexPage.down)
        return;
    if (in->touchActive) {
        if (on) {
            sDexPage.x1 = gx;
            sDexPage.y1 = gy;
        } else
            sDexPage.down = 0;              /* left the viewport: nothing */
        return;
    }
    {
        int dx = sDexPage.x1 - sDexPage.x0, dy = sDexPage.y1 - sDexPage.y0, ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;

        sDexPage.down = 0;
        if (ax >= DEX_SWIPE_MIN && ax >= 2 * ay) {
            int dir = dx < 0 ? 1 : -1;      /* swipe left: the next page */

            if (CtrBottomDex_PageCanFlip(dir)) {
                DexPlanClear();
                sDexPlan.active = 1;
                sDexPlan.kind = DEXPLAN_FLIP;
                sDexPlan.target = dir;
                sDexPlan.state = DEX_CATEGORY;
                sDexPlan.page = CtrBottomDex_Page();
            }
        } else if (ax <= DEX_TAP_SLOP && ay <= DEX_TAP_SLOP && sDexPage.slot >= 0
                   && CtrBottomDex_PageSlotAt(sDexPage.x1, sDexPage.y1) == sDexPage.slot) {
            if (sDexPage.slot == CtrBottomDex_PageSlot())
                sRouteKey = K_A;
            else {
                DexPlanClear();
                sDexPlan.active = 1;
                sDexPlan.kind = DEXPLAN_SLOT;
                sDexPlan.target = sDexPage.slot;
                sDexPlan.state = DEX_CATEGORY;
            }
        }
    }
}


/* Touch of a storage screen never calls any Pokemon mutation function. Every
 * operation (including RELEASE, DEPOSIT and item changes) remains owned by
 * FireRed's task/controller; a touch yields only one real D-pad/A/B key. */
static uint16_t StoreDirKey(int dir)
{
    switch (dir) {
    case ST_DIR_UP: return K_UP;
    case ST_DIR_DOWN: return K_DOWN;
    case ST_DIR_LEFT: return K_LEFT;
    case ST_DIR_RIGHT: return K_RIGHT;
    case ST_DIR_A: return K_A;
    case ST_DIR_B: return K_B;
    case ST_DIR_START: return K_START;
    }
    return 0;
}
static int StoreHit(int mode, int gx, int gy)
{
    if (mode == MODE_STORAGE_PC)
        return CtrStorage_PcHit(gx, gy, CtrBottomStorage_PCMenuCount());
    if (mode != MODE_STORAGE_BOX)
        return ST_HIT_NONE;
    if (CtrBottomStorage_YesNo())
        return CtrStorage_MenuHit(gx, gy, 192, 88, 40, 2);
    if (CtrBottomStorage_ContextMenu())
        return CtrStorage_MenuHit(gx, gy, CtrBottomStorage_ContextX(),
                CtrBottomStorage_ContextY(), CtrBottomStorage_ContextW(),
                CtrBottomStorage_ContextCount());
    if (CtrBottomStorage_BoxChoose())
        return CtrStorage_ChooseHit(gx, gy);
    if (!CtrBottomStorage_BoxReady() || CtrBottomStorage_MultiMove())
        return ST_HIT_NONE;
    return CtrStorage_BoxHit(gx, gy, CtrBottomStorage_CursorArea() == ST_AREA_PARTY);
}
static void StoreBeginTouch(int mode, int hit)
{
    StorePlanClear();
    sStorePlan.context = mode == MODE_STORAGE_PC ? CtrBottomStorage_PCInputContext()
                                                : CtrBottomStorage_InputContext();
    if (mode == MODE_STORAGE_PC && hit >= ST_HIT_PC_BASE) {
        sStorePlan.kind = ST_PLAN_PC;
        sStorePlan.target = hit - ST_HIT_PC_BASE;
    } else if (mode == MODE_STORAGE_BOX && (CtrBottomStorage_ContextMenu() || CtrBottomStorage_YesNo())
               && hit >= ST_HIT_MENU_BASE && hit < ST_HIT_PC_BASE) {
        sStorePlan.kind = ST_PLAN_CONTEXT;
        sStorePlan.target = hit - ST_HIT_MENU_BASE;
    } else if (mode == MODE_STORAGE_BOX && CtrBottomStorage_BoxChoose()) {
        sRouteKey = hit == ST_HIT_PREV ? K_LEFT : hit == ST_HIT_NEXT ? K_RIGHT
                  : hit == ST_HIT_TITLE ? K_A : 0;
        return;
    } else if (mode == MODE_STORAGE_BOX && CtrBottomStorage_BoxReady()) {
        sStorePlan.kind = ST_PLAN_BOX;
        sStorePlan.targetArea = ST_AREA_BOX;
        sStorePlan.targetPos = hit;
        if (hit >= ST_HIT_PARTY_BASE && hit <= ST_HIT_PARTY_BASE + 6) {
            sStorePlan.targetArea = ST_AREA_PARTY;
            sStorePlan.targetPos = hit - ST_HIT_PARTY_BASE;
        } else if (hit == ST_HIT_TITLE || hit == ST_HIT_PREV || hit == ST_HIT_NEXT) {
            sStorePlan.targetArea = ST_AREA_TITLE;
            sStorePlan.targetPos = 0;
            if (hit == ST_HIT_PREV) sStorePlan.postDir = ST_DIR_LEFT;
            else if (hit == ST_HIT_NEXT) sStorePlan.postDir = ST_DIR_RIGHT;
        } else if (hit == ST_HIT_BUTTON_PARTY || hit == ST_HIT_BUTTON_CLOSE) {
            sStorePlan.targetArea = ST_AREA_BUTTONS;
            sStorePlan.targetPos = hit == ST_HIT_BUTTON_PARTY ? 0 : 1;
        } else if (hit < 0 || hit >= 30) {
            StorePlanClear();
        }
    }
}
static void ProcessStorageTouch(int mode, const CtrInput *in)
{
    int gx = 0, gy = 0;
    int on = in->touchActive && CtrPokeTouch_ViewportToGba(
                        (int)in->touchX, (int)in->touchY, &gx, &gy);
    int hit = on ? StoreHit(mode, gx, gy) : ST_HIT_NONE;
    if ((in->held & 0x3ffu) || (sHaveShown && sShown.mode != mode)) {
        sStoreTouch.active = sStoreTouch.inside = 0;
        StorePlanClear();
        return;
    }
    if (in->touchDown) {
        sStoreTouch.active = hit != ST_HIT_NONE;
        sStoreTouch.inside = sStoreTouch.active;
        sStoreTouch.mode = mode;
        sStoreTouch.hit = hit;
        sStoreTouch.context = mode == MODE_STORAGE_PC ? CtrBottomStorage_PCInputContext()
                                                      : CtrBottomStorage_InputContext();
        StorePlanClear();
        return;
    }
    if (!sStoreTouch.active)
        return;
    if (sStoreTouch.context != (mode == MODE_STORAGE_PC ? CtrBottomStorage_PCInputContext()
                                                       : CtrBottomStorage_InputContext()))
        sStoreTouch.inside = 0;
    if (in->touchActive) {
        /* Once a finger leaves its original hit, returning over it does not
         * restore the arm. A dragged touch can never activate a PC action. */
        sStoreTouch.inside = sStoreTouch.inside
                         && hit == sStoreTouch.hit && mode == sStoreTouch.mode;
        return;
    }
    /* Release coordinates are not guaranteed to remain valid after touch-up.
     * Use the last held sample, armed only while staying inside the first hit. */
    if (sStoreTouch.inside && sStoreTouch.mode == mode)
        StoreBeginTouch(mode, sStoreTouch.hit);
    sStoreTouch.active = sStoreTouch.inside = 0;
}
static void RunStoragePlan(int mode)
{
    int cur, dir;
    if (!sStorePlan.kind)
        return;
    if ((CtrInput_Get()->held & 0x3ffu) || (mode != MODE_STORAGE_BOX && mode != MODE_STORAGE_PC)
        || ++sStorePlan.wait > 360) {
        StorePlanClear();
        return;
    }
    if (sStorePlan.release) {
        sStorePlan.release = 0;
        return;
    }
    /* A zero context is an animation: wait. A different live controller
     * cancels, even if the new popup occupies the same pixels. */
    int context = mode == MODE_STORAGE_PC ? CtrBottomStorage_PCInputContext()
                                         : CtrBottomStorage_InputContext();
    if (context && context != sStorePlan.context) { StorePlanClear(); return; }
    if (sStorePlan.kind == ST_PLAN_PC) {
        if (!CtrBottomStorage_PCMenuReady()) return;
        cur = CtrBottomStorage_PCMenuCursor();
        if (cur < 0 || sStorePlan.target >= CtrBottomStorage_PCMenuCount()) {
            StorePlanClear();
            return;
        }
        dir = cur == sStorePlan.target ? ST_DIR_A : cur < sStorePlan.target ? ST_DIR_DOWN : ST_DIR_UP;
    } else if (sStorePlan.kind == ST_PLAN_CONTEXT) {
        if (!CtrBottomStorage_ContextMenu() && !CtrBottomStorage_YesNo()) { StorePlanClear(); return; }
        cur = CtrBottomStorage_ContextCursor();
        if (cur < 0 || sStorePlan.target >= CtrBottomStorage_ContextCount()) {
            StorePlanClear();
            return;
        }
        dir = cur == sStorePlan.target ? ST_DIR_A : cur < sStorePlan.target ? ST_DIR_DOWN : ST_DIR_UP;
    } else {
        if (!CtrBottomStorage_BoxOpen()) { StorePlanClear(); return; }
        if (!CtrBottomStorage_BoxReady()) return; /* wait for cursor animations */
        dir = CtrStorage_Next(CtrBottomStorage_CursorArea(), CtrBottomStorage_CursorPos(),
                              sStorePlan.targetArea, sStorePlan.targetPos);
        if (dir == ST_DIR_A && sStorePlan.postDir)
            dir = sStorePlan.postDir;
    }
    if (dir == ST_DIR_NONE || ++sStorePlan.steps > 40) {
        StorePlanClear();
        return;
    }
    sRouteKey = StoreDirKey(dir);
    if (!sRouteKey || dir == ST_DIR_A || sStorePlan.postDir == dir)
        StorePlanClear();
    else
        sStorePlan.release = 1;
}

static int ProcessTouch(int mode)
{
    const CtrInput *in = CtrInput_Get();

    /* The picture kept between two screens belongs to neither: a tap on it would act on the screen that is gone. */
    if (sBridging) {
        sBat.down = sBat.inside = 0;
        sSum.down = sSum.inside = 0;
        sPoke.id = PT_NONE;
        sPoke.inside = 0;
        sTouch.active = 0;
        sBack.down = sBack.inside = 0;
        return HIT_NONE;
    }

    /* PokeTouch works alongside the buttons (walking while tapping is fine):
     * what it starts runs under FireRed's own field checks. */
    if (sPendingScreen != PT_NONE && (mode == MODE_FIELD || mode == MODE_OFF)) {
        /* A route passes through the field: nothing there is touchable. */
        sPoke.id = PT_NONE;
        sPoke.inside = 0;
        sHome.down = 0;
        sTouch.active = 0;
        return HIT_NONE;
    }
    if (mode == MODE_FIELD) {
        sTouch.active = 0;
        ProcessBack(mode, in);          /* only on the native OPTIONS screen */
        ProcessPokeTouch(in);
        return HIT_NONE;
    }
    if (mode == MODE_STORAGE_PC || mode == MODE_STORAGE_BOX || mode == MODE_STORAGE_CHILD) {
        sPoke.id = PT_NONE;
        sPoke.inside = 0;
        sTouch.active = 0;
        ProcessBack(mode, in);
        ProcessStorageTouch(mode, in);
        return HIT_NONE;
    }
    if (mode == MODE_BATTLE) {
        sPoke.id = PT_NONE;
        sPoke.inside = 0;
        sTouch.active = 0;
        ProcessBattleTouch(in);
        return HIT_NONE;
    }
    if (mode == MODE_PARTY && CtrBottomParty_InBattle()) {
        sPoke.id = PT_NONE;
        sPoke.inside = 0;
        sTouch.active = 0;
        ProcessBattlePartyTouch(in);
        return HIT_NONE;
    }
    if (mode == MODE_BAG && CtrBottomBag_InBattle()) {
        /* The battle's own Bag screen has its own CANCEL: PokeTouch's BACK is not in it. */
        sPoke.id = PT_NONE;
        sPoke.inside = 0;
        sTouch.active = 0;
        ProcessBattleBagTouch(in);
        return HIT_NONE;
    }
    if (mode == MODE_SUMMARY && SummaryPremium()) {
        /* The battle Summary's own screen: no sidebar to route from, its own BACK. */
        sPoke.id = PT_NONE;
        sPoke.inside = 0;
        sTouch.active = 0;
        sBack.down = sBack.inside = 0;
        return ProcessSummaryTouch(in);
    }
    if (sStorageSession && (mode == MODE_SUMMARY || mode == MODE_BAG || IS_CASE_MODE(mode))) {
        /* Keep the real GBA child touchscreen handler and BACK. Prevent the
         * field sidebar from unwinding the still-active PC context. */
        sPoke.id = PT_NONE;
        sPoke.inside = 0;
        ProcessBack(mode, in);
        if (in->held & 0x3ffu)
            return HIT_NONE;
        if (ListFor(mode) != NULL)
            return ProcessBagTouch(ListFor(mode), in);
        if (mode == MODE_SUMMARY)
            return ProcessSummaryGbaTouch(in);
        return HIT_NONE;
    }
    ProcessRouteTouch(mode, in);
    ProcessBack(mode, in);
    if (mode == MODE_CARD) {
        ProcessCardTouch(in);
        return HIT_NONE;
    }
    if (mode == MODE_DEX) {
        if (CtrBottomDex_State() == DEX_CATEGORY || sDexPage.down)
            ProcessDexPageTouch(in);
        else
            ProcessDexTouch(in);
        return HIT_NONE;
    }
    if (in->held & 0x3ffu || (sHaveShown && mode != sShown.mode))
        return HIT_NONE;
    if (ListFor(mode) != NULL)
        return ProcessBagTouch(ListFor(mode), in);
    if (mode == MODE_SUMMARY)
        return ProcessSummaryGbaTouch(in);
    if (mode != MODE_PARTY) {
        sTouch.active = 0;
        return HIT_NONE;
    }
    if (!in->touchDown || !sHaveShown || sShown.serial != CtrBottomParty_MenuSerial())
        return HIT_NONE;
    /* The side column and shortcuts are off in Party: only the viewport. */
    int gx, gy;
    if (!CtrPokeTouch_ViewportToGba((int)in->touchX, (int)in->touchY, &gx, &gy))
        return HIT_NONE;
    int hit = CtrBottomParty_HitAt(gx, gy);
    /* Rows of the selection or yes/no menu drive the live game menu. */
    if (hit >= PARTY_HIT_MENU) {
        int idx = hit - PARTY_HIT_MENU;
        if ((!CtrBottomParty_SelectionOpen() && !CtrBottomParty_YesNoOpen()) || CtrBottom_IsFading())
            return HIT_NONE;
        PlanStart(PLAN_MENU, idx);
        return HIT_ACTION + idx;
    }
    if (hit >= 0 && !CtrBottomParty_SelectionOpen() && !CtrBottomParty_YesNoOpen()
        && CtrBottomParty_IsOpen() && !CtrBottom_IsFading()) {
        /* Slots, Cancel and Confirm: the original Party handler decides. */
        if (hit < PARTY_SIZE && CtrBottomParty_SlotEmpty(hit))
            return HIT_NONE;
        if (CtrBottomParty_TouchSlot(hit))
            return hit < PARTY_SIZE ? HIT_SLOT + hit : hit == PARTY_SIZE ? HIT_CONFIRM : HIT_BACK;
        return HIT_NONE;
    }
    if (CtrBottomParty_CanAdvanceText()) {
        PlanPress(K_A);
        return HIT_PANEL;
    }
    return HIT_NONE;
}

/* Opt-in profiling (sdmc:/3ds/twinembers/bottom.perf, optionally holding a
 * slow-frame threshold in microseconds): the time of the bottom UI's three
 * parts, summed over a window of frames and logged once per window, plus one
 * line (at most every 120 frames) for a frame over the threshold. Off, it
 * costs one test per frame. */
#define PROF_WINDOW 600
#define PROF_TICKS_PER_US 268u          /* the ARM11 system clock, 268 MHz */
#define PROF_DEFAULT_SLOW_US 3000u
static struct {
    int on;
    unsigned slowUs;
    uint64_t snapshot, ui, present;     /* ticks in the window */
    uint32_t frames, active, rects, pixels;
    uint32_t sinceSlow;
    uint64_t frameSnapshot, frameUi, framePresent;
} sProf;

static uint64_t ProfNow(void)
{
    return sProf.on ? CtrPerf_PlatformClock() : 0;
}

static void ProfSpan(uint64_t *into, uint64_t start)
{
    if (sProf.on)
        *into += CtrPerf_PlatformClock() - start;
}

static void ProfFrameEnd(const CtrDirtyList *dirty)
{
    uint32_t upPixels;
    uint64_t upTicks;

    if (!sProf.on)
        return;
    CtrVideo_BottomUploadStats(&upPixels, &upTicks);
    sProf.snapshot += sProf.frameSnapshot;
    sProf.ui += sProf.frameUi;
    sProf.present += sProf.framePresent + upTicks;
    ++sProf.frames;
    ++sProf.sinceSlow;
    if (dirty != NULL && dirty->count > 0) {
        ++sProf.active;
        sProf.rects += (uint32_t)dirty->count;
        sProf.pixels += (uint32_t)CtrDirty_Pixels(dirty);
    }
    {
        uint64_t total = sProf.frameSnapshot + sProf.frameUi + sProf.framePresent + upTicks;

        if (total / PROF_TICKS_PER_US > sProf.slowUs && sProf.sinceSlow >= 120) {
            sProf.sinceSlow = 0;
            CtrLog_Write(CTR_LOG_VIDEO, "BOTTOM_PERF slow frame: snapshot=%luus ui=%luus present=%luus upload=%luus",
                         (unsigned long)(sProf.frameSnapshot / PROF_TICKS_PER_US),
                         (unsigned long)(sProf.frameUi / PROF_TICKS_PER_US),
                         (unsigned long)(sProf.framePresent / PROF_TICKS_PER_US),
                         (unsigned long)(upTicks / PROF_TICKS_PER_US));
        }
    }
    sProf.frameSnapshot = sProf.frameUi = sProf.framePresent = 0;
    if (sProf.frames >= PROF_WINDOW) {
        CtrLog_Write(CTR_LOG_VIDEO,
                     "BOTTOM_PERF frames=%lu snapshot=%luus ui=%luus present=%luus per frame, "
                     "%lu frames drew %lu rects %lu px",
                     (unsigned long)sProf.frames,
                     (unsigned long)(sProf.snapshot / PROF_TICKS_PER_US / sProf.frames),
                     (unsigned long)(sProf.ui / PROF_TICKS_PER_US / sProf.frames),
                     (unsigned long)(sProf.present / PROF_TICKS_PER_US / sProf.frames),
                     (unsigned long)sProf.active, (unsigned long)sProf.rects, (unsigned long)sProf.pixels);
        sProf.snapshot = sProf.ui = sProf.present = 0;
        sProf.frames = sProf.active = sProf.rects = sProf.pixels = 0;
    }
}

static void ProfInit(void)
{
#ifndef CTR_BOTTOM_PERF_PATH
#define CTR_BOTTOM_PERF_PATH "sdmc:/3ds/twinembers/bottom.perf"
#endif
    FILE *file = fopen(CTR_BOTTOM_PERF_PATH, "rb");
    unsigned slow = 0;

    memset(&sProf, 0, sizeof(sProf));
    if (file == NULL)
        return;
    sProf.on = 1;
    sProf.slowUs = fscanf(file, "%u", &slow) == 1 && slow ? slow : PROF_DEFAULT_SLOW_US;
    fclose(file);
    CtrLog_Write(CTR_LOG_BOOT, "bottom: profiling on, slow frame over %u us", sProf.slowUs);
}

/* Whether what the lower screen is to show for `mode` exists yet: the mode is known from the first frame of its setup, its
 * picture only later. A Party is its task, a Bag its list (not its setup), a Summary its first report of a Pokemon, the
 * battle a state of its own (not the idle one between its screens). */
static int ModePresentationReady(int mode)
{
    switch (mode) {
    case MODE_PARTY:
        return CtrBottomParty_IsOpen() != 0;
    case MODE_BAG:
        return CtrBottomBag_IsOpen() && CtrBottomBag_State() != BAG_BUSY;
    case MODE_SUMMARY: {
        CtrSummaryView now;

        CtrBottomSummary_GetView(&now);
        return now.species[0] != 0;
    }
    case MODE_BATTLE:
        return CtrBattle_State() != BATTLE_BUSY;
    default:
        return 1;
    }
}

/* Between two screens of one session FireRed shows neither: the Party closing for its Summary (its exit callback), the
 * Summary returning to the Party (CB2_ReturnToPartyMenuFromSummaryScreen), a Bag, Party or Summary that is still setting
 * up, the battle that has not taken its action menu back. Those frames belong to the session, so the lower screen keeps
 * what it showed (the battle's UI, the Party, the Summary) rather than the PokeTouch frame, an empty screen or black, until
 * the screen that follows can be presented. The battle keeps its Party, Bag or Summary past the session's end (TOP is
 * the battle's again by then) until it has a state to show. One bound for all of it: BRIDGE_MAX frames, the session's own
 * grace; a field transition shows HOME as ever, and the field's own menus keep their setups as they were. */
/* The PC has a separate lifetime from the field Party/Bag session. Keep
 * its last lower picture through loading callbacks, fades and null VBlank;
 * display only the child whose own controller has finished setup. */
static int StorageBridge(int mode)
{
    if (!sStorageSession) return 0;
    if (CtrBottom_IsFading()) return 1;
    if (mode == MODE_STORAGE_PC) return !CtrBottomStorage_PCMenuReady();
    if (mode == MODE_STORAGE_BOX) return !CtrBottomStorage_BoxPresented();
    if (mode == MODE_STORAGE_CHILD) return !CtrBottomStorage_NamingPresented();
    if (mode == MODE_SUMMARY) return !CtrBottomSummary_Ready();
    if (ListFor(mode) != NULL) return ListFor(mode)->state() == BAG_BUSY;
    return 0;
}

static void UpdateStorageOwner(int mode)
{
    if (mode == MODE_STORAGE_BOX)
        sStorageSession = 1;
    else if (mode == MODE_FIELD)
        sStorageSession = 0; /* Includes returning to Withdraw/Deposit/Move. */
}

static int StorageHoldTop(void)
{
    /* Keep the last TOP choice frame through entry's fade, while HOME stays
     * below. The lower viewport is activated only by the real box controller. */
    /* STATE_ENTER_PC survives one frame after the palette fade completes.
     * Never release TOP in that gap: it would present black, then freeze it
     * when the box callback starts. Loading the root also keeps its last frame. */
    return sStorageSession || (CtrBottomStorage_PCMenu()
        && !CtrBottomStorage_PCMenuReady());
}

static int SessionBridge(int mode)
{
    int bridge = 0;

    if (sHaveShown && sShown.mode != MODE_OFF) {
        if (mode == MODE_BATTLE && sShown.battleUi && sShown.mode != MODE_BATTLE)
            bridge = !ModePresentationReady(MODE_BATTLE);
        else if (sSession.active && sHeldTop) {
            if (mode == MODE_OFF)
                bridge = !CtrBottom_IsFieldTransition();
            else if (mode != sShown.mode && sShown.mode != MODE_FIELD)
                bridge = !ModePresentationReady(mode);
        }
    }
    if (!bridge || ++sBridgeFrames > BRIDGE_MAX)
        return sBridging = 0;
    return sBridging = 1;
}

void CtrBottom_Init(void)
{
    FILE *off;

    memset(sCanvas, 0, sizeof(sCanvas));
    memset(&sShown, 0, sizeof(sShown));
    memset(&sSession, 0, sizeof(sSession));
    sPartyMenuCallback = NULL;
    sLatchCount = 0;
    memset(sLatchIds, 0, sizeof(sLatchIds));
    memset(sLatchNames, 0, sizeof(sLatchNames));
    PlanClear();
    BagPlanClear();
    SumPlanClear();
    memset(&sHome, 0, sizeof(sHome));
    memset(&sBack, 0, sizeof(sBack));
    memset(&sBat, 0, sizeof(sBat));
    sBatTouchLast = 0;
    sBatHold = sBatHoldFrames = 0;
    sBatBag.pocketTarget = -1;
    sBatBag.pocketWait = sBatBag.autoWait = 0;
    BatPlanClear();
    memset(&sOpt, 0, sizeof(sOpt));
    memset(&sSave, 0, sizeof(sSave));
    memset(&sStoreTouch, 0, sizeof(sStoreTouch));
    StorePlanClear();
    sStorageSession = 0;
    memset(&sCardTap, 0, sizeof(sCardTap));
    memset(&sDexTap, 0, sizeof(sDexTap));
    memset(&sDexPage, 0, sizeof(sDexPage));
    DexPlanClear();
    sInjected = 0;
    memset(&sReveal, 0, sizeof(sReveal));
    memset(&sSum, 0, sizeof(sSum));
    sBridgeFrames = 0;
    sHeldTop = 0;
    sScreen = sPendingScreen = PT_NONE;
    sRouteHold = 0;
    sTapped = SLOT_NONE;
    sHaveShown = 0;
    sForceRedraw = 1;
    sEnabled = 1;
    sInitDone = 1;
    /* Opt-out fallback: creating sdmc:/3ds/twinembers/bottom.off disables
     * the bottom UI; TOP fallback (centred Party) keeps working. */
    off = fopen("sdmc:/3ds/twinembers/bottom.off", "rb");
    if (off != NULL) {
        fclose(off);
        sEnabled = 0;
    }
    CtrLog_Write(CTR_LOG_BOOT, "bottom: init %s", sEnabled ? "enabled" : "disabled");
    ProfInit();
}

/* FireRed reaches some screens through others: the Bag's BALLS from ITEMS through KEY ITEMS, a Summary's TRAINER from MOVES
 * through STATS, a move's row through the details. The lower screen shows where a touch asked to go, never the way: while the
 * backend is on its way (the plan a tap started has not arrived and is still walking) the last complete picture stays, and the
 * destination is presented in one update. Only what a finger pressed is let go, so the control does not stay held down. */
static int NavPending(int mode)
{
    if (!sHaveShown || mode != sShown.mode)
        return 0;
    if (sShown.mode == MODE_BAG && sShown.battleUi && CtrBottomBag_InBattle()) {
        if (sBatBag.pocketTarget < 0)
            return 0;
        return CtrBottomBag_Pocket() != sBatBag.pocketTarget || CtrBottomBag_State() == BAG_BUSY;
    }
    if (sShown.mode == MODE_SUMMARY && sShown.summaryUi) {
        if (sSumPlan.kind == SUMPLAN_PAGE)
            return !(CtrBottomSummary_Ready() && CtrBottomSummary_Page() == sSumPlan.target);
        return sSumPlan.kind == SUMPLAN_MOVE;
    }
    return 0;
}

static int NavPressedShown(void)
{
    return (sShown.summaryUi && sShown.sum.pressed != -1) || (sShown.battleUi && sShown.battle.pressed != -1);
}

void CtrBottom_Frame(void)
{
    int mode;
    int pressed;
    int hold, nav;
    int storageBridge;
    uint64_t profT = 0;
    Snapshot state;

    if (!sInitDone)
        return;
    /* Physical Y and X: the Bicycle and registered-item shortcuts, the same
     * field requests as the Y and X panels (and kept with the panels off). */
    if (CtrInput_Get()->physicalDown & CTR_KEY_Y)
        CtrPokeTouch_RequestFieldCommand(PT_BIKE);
    if (CtrInput_Get()->physicalDown & CTR_KEY_X)
        CtrPokeTouch_RequestFieldCommand(PT_REGISTERED);
    if (!sEnabled)
        return;
    /* The FPS COUNTER option (a save field): the top screen draws the counter only while it is ON. */
    if (CtrBottom_HasSave())
        CtrVideo_SetFpsCounter(CtrPokeTouch_OptionGet(PT_OPT_FPS) == 0);
    mode = CurrentMode();
    if (mode != MODE_STORAGE_PC && mode != MODE_STORAGE_BOX && mode != MODE_STORAGE_CHILD) {
        sStoreTouch.active = sStoreTouch.inside = 0;
        StorePlanClear();
    }
    UpdateStorageOwner(mode);
    if (mode != MODE_FIELD && !(mode == MODE_OFF && CtrBottom_IsFieldTransition())) {
        sHome.valid = 0;                /* a menu or a battle has the screen: no selection */
        sHome.down = 0;
    } else {
        ++sHome.frames;
    }
    /* Native OPTIONS lasts while the field is the screen's owner: a menu, a
     * battle or FireRed's START menu takes over, so it is left for HOME. */
    if (NativeActive() && sPendingScreen == PT_NONE
        && ((mode != MODE_FIELD && !(mode == MODE_OFF && CtrBottom_IsFieldTransition())) || CtrPokeTouch_StartMenuOpen()))
        CloseNative();
    /* Hits from the last render are used; render rebuilds them. */
    storageBridge = StorageBridge(mode);
    if (storageBridge) {
        sStoreTouch.active = sStoreTouch.inside = 0;
        StorePlanClear();
        sBack.down = sBack.inside = 0;
        pressed = HIT_NONE;
    } else
        pressed = ProcessTouch(mode);
    UpdateSave();
    UpdateDestinationRoute(mode);
    RunPlan();
    if (mode == MODE_DEX)
        RunDexPlan();
    else
        DexPlanClear();
    if (ListFor(mode) != NULL)
        RunBagPlan(ListFor(mode));
    else
        BagPlanClear();
    if (mode == MODE_SUMMARY)
        RunSumPlan();
    else
        SumPlanClear();
    if (mode == MODE_BATTLE)
        RunBatPlan();
    else
        BatPlanClear();
    RunStoragePlan(mode);
    if (mode != MODE_BATTLE && !(mode == MODE_BAG && CtrBottomBag_InBattle())
        && !(mode == MODE_PARTY && CtrBottomParty_InBattle()))
        sBat.down = sBat.inside = 0;
    /* The battle keeps the upper screen from the pick of BAG or POKeMON (or a replacement), before the Bag or Party exists (its fade and setup are never shown). */
    if (mode == MODE_BATTLE)
        sBatHold = CtrBattle_BagPending() || CtrBattle_PartyPending() ? 1 : 0;
    else if (mode == MODE_BAG || mode == MODE_PARTY)
        sBatHold = 0;
    else if (sBatHold && ++sBatHoldFrames > 150)
        sBatHold = 0;
    if (!sBatHold)
        sBatHoldFrames = 0;
    if (sRouteKey && !(CtrInput_Get()->held & 0x3ffu))
        sInjected = sRouteKey;
    sRouteKey = 0;
    hold = UpdateSession(mode, sPlan.kind != PLAN_NONE) || SaveActive() || sBatHold || StorageHoldTop();   /* SAVE keeps TOP on the frozen frame */
    if (hold != sHeldTop) {
        CtrVideo_HoldTop(hold != 0);
        sHeldTop = hold;
        if (!hold)
            sTapped = SLOT_NONE;
    }
    /* A route in flight: the last presentation stays (no HOME, no black, no
     * viewport sampling the unwinding GBA screens) until the requested root
     * is up. */
    if (storageBridge || (!sRouteHold && SessionBridge(mode))) {
        /* Between two screens of one session: what was shown stays (see SessionBridge). */
        CtrVideo_HoldBottom(true);
        RevealFlush();
        ProfFrameEnd(NULL);
        return;
    }
    sBridgeFrames = 0;
    nav = NavPending(mode);
    if (nav && !NavPressedShown()) {
        CtrVideo_HoldBottom(true);
        RevealFlush();
        ProfFrameEnd(NULL);
        return;
    }
    CtrVideo_HoldBottom(sRouteHold != 0);
    if (sRouteHold) {
        RevealFlush();
        return;
    }
    /* The callbacks that load the field (a warp, the return from a menu or a
     * battle) show HOME too: the map never goes blank between a menu and the
     * field. */
    profT = ProfNow();
    if (nav) {
        state = sShown;                 /* the picture stays, its pressed control let go */
        state.sum.pressed = -1;
        state.battle.pressed = -1;
    } else
        SnapshotState(&state, mode == MODE_OFF && CtrBottom_HasSave() && sEnabled && CtrBottom_IsFieldTransition()
                              ? MODE_FIELD : mode, pressed);
    ProfSpan(&sProf.frameSnapshot, profT);
    /* Party's and the Bag's viewport is drawn by the GPU, sampling the GBA
     * picture: the bottom screen goes through it while one is shown. */
    {
        /* The initial PC menu is a field BG0 window 80px down on the native
         * canvas. The box/summary are authored at the centred 240x160 stage. */
        CtrVideo_SetBottomViewportPcField(mode == MODE_STORAGE_PC);
        int gpu = CtrVideo_SetBottomViewport((mode == MODE_STORAGE_PC
                                               || mode == MODE_STORAGE_BOX || mode == MODE_STORAGE_CHILD
                                               || mode == MODE_PARTY || mode == MODE_SUMMARY || mode == MODE_PARTY_CHILD || mode == MODE_BAG
                                              || IS_CASE_MODE(mode) || mode == MODE_CARD || mode == MODE_DEX) && state.touch.viewport);
        if (gpu != sGpu) {
            sGpu = gpu;
            sForceRedraw = 1;
        }
        if (!gpu)
            state.touch.viewport = 0;
    }
    if (sForceRedraw || !sHaveShown || memcmp(&state, &sShown, sizeof(state)) != 0) {
        int screenChange = sHaveShown && state.battleUi && sShown.battleUi && state.battle.state != sShown.battle.state
                        && state.battle.state != BUI_IDLE;

        RevealFlush();
        /* PokeTouch to PokeTouch (a press, a state, the bar): only the
         * elements that changed are redrawn and only their rectangles presented. */
        if (!sForceRedraw && sHaveShown && state.mode == sShown.mode && state.mode != MODE_OFF
            && state.battleUi == sShown.battleUi && state.summaryUi == sShown.summaryUi) {
            CtrDirtyList dirty;

            CtrDirty_Clear(&dirty);
            profT = ProfNow();
            if (state.summaryUi)
                CtrSummaryUi_Update(sCanvas, &sShown.sum, &state.sum, &dirty);
            else if (state.battleUi)
                CtrBattleUi_Update(sCanvas, &sShown.battle, &state.battle, &dirty);
            else
                CtrPokeTouch_Update(sCanvas, &sShown.touch, &state.touch, &dirty);
            ProfSpan(&sProf.frameUi, profT);
            sShown = state;
            profT = ProfNow();
            if (screenChange)
                PresentRevealed(&dirty);
            else
                Present(&dirty);
            ProfSpan(&sProf.framePresent, profT);
            ProfFrameEnd(&dirty);
            return;
        }
        sShown = state;
        sHaveShown = 1;
        sForceRedraw = 0;
        profT = ProfNow();
        Render(&sShown);
        ProfSpan(&sProf.frameUi, profT);
        profT = ProfNow();
        PresentRect(0, 0, CTR_BOTTOM_WIDTH, CTR_BOTTOM_HEIGHT);
        ProfSpan(&sProf.framePresent, profT);
        {
            CtrDirtyList full;

            CtrDirty_Clear(&full);
            CtrDirty_Add(&full, 0, 0, CTR_BOTTOM_WIDTH, CTR_BOTTOM_HEIGHT);
            ProfFrameEnd(&full);
        }
        return;
    }
    RevealStep();
    ProfFrameEnd(NULL);
}

uint16_t CtrBottom_InjectedKeys(void)
{
    return sInjected;
}

bool CtrBottom_IsEnabled(void)
{
    return sEnabled != 0;
}

/* The turbo runs in two places, each only while FireRed says it is stable. The plain field (its own CB2, nothing fading,
 * no battle or battle transition) with PokeTouch having no screen of its own up: no hidden session, no native OPTIONS or
 * SAVE (SAVE also locks the field), no route in flight. And the battle itself (its own main callback, nothing fading, no
 * transition) with the lower screen on the battle's own UI: its messages, animations, turns and choices; never the Bag,
 * the Party or the Summary opened from it (a session), a loading screen or a fade. The chosen multiplier is kept
 * everywhere else and comes back with the battle. */
bool CtrBottom_TurboApplies(void)
{
    int battle = CtrTurbo_BattleStable();

    if (!battle && !CtrTurbo_FieldStable())
        return false;
    if (!sEnabled || !sInitDone || !CtrBottom_HasSave())
        return true;
    if (battle)
        return CurrentMode() == MODE_BATTLE && !sSession.active && !sRouteHold && !SaveActive() && !sBatHold;
    return CurrentMode() == MODE_FIELD && !sSession.active && sScreen == PT_NONE && sPendingScreen == PT_NONE
        && !sRouteHold && !SaveActive();
}

/* START cycles the multiplier in the same two places: in the battle at any moment of its stable core (a message, an
 * animation, a choice), with no menu of the original opened. */
bool CtrBottom_TurboCanCycle(void)
{
    if (CtrTurbo_BattleStable())
        return CtrBottom_TurboApplies();
    return CtrBottom_TurboApplies() && CtrTurbo_CanCycle();
}
