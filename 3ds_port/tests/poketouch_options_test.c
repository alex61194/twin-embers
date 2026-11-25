/* The native OPTIONS rows (3ds_poketouch_options.c) against a stub game bridge: valid FireRed
 * values read and step as before, and an out-of-range value from a damaged or edited save is shown
 * as "?" without reading past the name tables or being written back; only a player's step
 * replaces it, with a valid value. Bad rows and small or missing output buffers are safe. */
#include <stdio.h>
#include <string.h>

#include "3ds_lang.h"
#include "3ds_poketouch_options.h"
#include "3ds_looks.h"
#include "3ds_settings.h"

static int failures;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); failures++; } } while (0)

/* The bridge's ranges (ctr_poketouch_options.c); the stored values may be anything the bits hold. */
static const int sCounts[PT_OPT_GAME_COUNT] = {3, 2, 2, 2, 3, 10, 2};
static int sStored[PT_OPT_GAME_COUNT];
static int sSets;

int CtrPokeTouch_OptionCount(int option)
{
    return option >= 0 && option < PT_OPT_GAME_COUNT ? sCounts[option] : 0;
}

int CtrPokeTouch_OptionGet(int option)
{
    return option >= 0 && option < PT_OPT_GAME_COUNT ? sStored[option] : 0;
}

void CtrPokeTouch_OptionSet(int option, int value)
{
    sSets++;
    if (option >= 0 && option < PT_OPT_GAME_COUNT && value >= 0 && value < sCounts[option])
        sStored[option] = value;
}

/* The looks the pack has (src/3ds_looks.c): three, and the choice for Red, Leaf and the rival (-1: none). */
static int sLookChoice[3] = {-1, -1, -1};
int CtrLooks_Count(void) { return 3; }
const char *CtrLooks_Id(int i) { static const char *const k[3] = {"ash", "gary_kanto", "green"}; return i >= 0 && i < 3 ? k[i] : NULL; }
const char *CtrLooks_Name(int i) { static const char *const k[3] = {"ASH", "GARY", "GREEN"}; return i >= 0 && i < 3 ? k[i] : NULL; }
int CtrLooks_Chosen(int slot) { return sLookChoice[slot]; }
void CtrLooks_Choose(int slot, int index) { sLookChoice[slot] = index; }
const char *CtrLooks_Credit(int i) { static const char *const k[3] = {"By Kalarie", "By Alan_Flygon", "By Poltergeist"}; return i >= 0 && i < 3 ? k[i] : NULL; }
int CtrLooks_Walk(int i, uint8_t tiles[CTR_LOOK_WALK_BYTES], uint16_t palette[16])
{
    if (i == 1)
        return 0;                                           /* this look has no picture in the pack */
    memset(tiles, 0x11 * (i + 1), CTR_LOOK_WALK_BYTES);
    for (int c = 0; c < 16; ++c)
        palette[c] = (uint16_t)c;
    return 1;
}

/* The Pokemon to surf on the pack has: two, the second without a picture; -1 is FireRed's own. */
static int sSurfChoice = -1;
int CtrSurf_Count(void) { return 2; }
const char *CtrSurf_Id(int i) { static const char *const k[2] = {"lapras", "latias"}; return i >= 0 && i < 2 ? k[i] : NULL; }
const char *CtrSurf_Name(int i) { static const char *const k[2] = {"LAPRAS", "LATIAS"}; return i >= 0 && i < 2 ? k[i] : NULL; }
const char *CtrSurf_Credit(int i) { static const char *const k[2] = {"By eMMe97", "By Nolo33LP"}; return i >= 0 && i < 2 ? k[i] : NULL; }
int CtrSurf_Picture(int i, uint8_t tiles[CTR_LOOK_WALK_BYTES], uint16_t palette[16])
{
    if (i == 1)
        return 0;
    memset(tiles, 0x22, CTR_LOOK_WALK_BYTES);
    for (int c = 0; c < 16; ++c)
        palette[c] = (uint16_t)c;
    return 1;
}
int CtrSurf_Chosen(void) { return sSurfChoice; }
void CtrSurf_Choose(int index) { sSurfChoice = index; }

/* The random sources of NEW: a fixed run (byte i is (37 i + 11) mod 250, so every byte makes a digit), one of zeros, and none. */
static int RunSource(uint8_t *out, size_t n)
{
    for (size_t i = 0; i < n; ++i)
        out[i] = (uint8_t)((37 * i + 11) % 250);
    return 1;
}
static int ZeroSource(uint8_t *out, size_t n)
{
    memset(out, 0, n);
    return 1;
}
static int NoSource(uint8_t *out, size_t n)
{
    (void)out;
    (void)n;
    return 0;
}
static const char *RunCode(void)
{
    static char code[17];

    for (int i = 0; i < 16; ++i)
        code[i] = (char)('0' + (37 * i + 11) % 250 % 10);
    code[16] = 0;
    return code;
}

static int sLocalPlayTold = -1;
static void LocalPlayHook(int local) { sLocalPlayTold = local; }

static void Text(int row, const char *expect)
{
    char out[PT_OPT_VALUE_LEN];

    memset(out, 'x', sizeof(out));
    CtrOptions_ValueText(row, out, (int)sizeof(out));
    CHECK(strcmp(out, expect) == 0, "row %d shows \"%s\", expected \"%s\"", row, out, expect);
}

/* A value that is a word of the port's (CTR_TR in 3ds_poketouch_options.c): the English build checks the very word; a Spanish
 * or German build (-DCTR_LANG_ES=1 / -DCTR_LANG_DE=1) has its own word there, so it checks only that the row shows a whole,
 * valid value, which every language must, and the English word stays in the message to name the value meant. */
static void TextWord(int row, const char *english)
{
#if !CTR_LANG_ES && !CTR_LANG_DE
    Text(row, english);
#else
    char out[PT_OPT_VALUE_LEN];

    memset(out, 'x', sizeof(out));
    CtrOptions_ValueText(row, out, (int)sizeof(out));
    CHECK(memchr(out, 0, sizeof(out)) != NULL && out[0] && strcmp(out, "?") != 0, "row %d shows no valid value for \"%s\"", row, english);
#endif
}

int main(void)
{
    static const char *const valid[][3] = {{"Slow", "Mid", "Fast"}, {"On", "Off"}, {"Shift", "Set"},
                                           {"Mono", "Stereo"}, {"Help", "LR", "L=A"}, {0}, {"On", "Off"}};
    CtrPokeTouchView view;
    char small[4];

    /* Every row's label fits the view's optLabel (PT_OPT_LABEL_LEN, the NUL included): Fill's snprintf would cut it without a
     * sign. Run for each build language (-DCTR_LANG_ES=1 / -DCTR_LANG_DE=1) to cover them all. */
    for (int i = 0; i < CtrOptions_RowCount(); ++i) {
        const CtrOptionRow *row = CtrOptions_Row(i);

        CHECK(row != NULL && row->label != NULL && strlen(row->label) < PT_OPT_LABEL_LEN, "row %d label \"%s\" is %d bytes, room for %d",
              i, row && row->label ? row->label : "?", row && row->label ? (int)strlen(row->label) : 0, PT_OPT_LABEL_LEN - 1);
    }

    {   /* An older build's settings: the private room on with a six-digit code. It is dropped as the settings are first read; the OPTIONS
         * screen says so on the ONLINE ROOM row (and the room is public on purpose), until PUBLIC or a new code is chosen. */
        FILE *old = fopen(CTR_SETTINGS_PATH, "wb");
        CtrPokeTouchView nv;
        int row;

        CHECK(old != NULL, "no settings file");
        if (old) {
            fputs("privroom=1\nroomcode=123456\n", old);
            fclose(old);
        }
        CHECK(!CtrSettings_PrivateRoom() && CtrSettings_RoomCodeDropped(), "the old six-digit code is dropped and the player is to be told");
        row = 6;                        /* ONLINE ROOM */
        memset(&nv, 0, sizeof(nv));
        CtrOptions_Fill(&nv, row);
        CHECK(nv.bar[1][0] && nv.bar[2][0] && nv.bar[1][strlen(nv.bar[1]) - 1] == '.' && nv.bar[2][strlen(nv.bar[2]) - 1] == '.',
              "the ONLINE ROOM row says the code was removed, whole (%s / %s)", nv.bar[1], nv.bar[2]);
        CtrOptions_Fill(&nv, 0);
        CHECK(!nv.bar[1][0] || strstr(nv.bar[1], "6") == NULL, "another row does not carry the note");
        CtrSettings_AckRoomCodeDropped();
    }
    remove(CTR_SETTINGS_PATH);      /* no EMBERS choice kept from an earlier run */

    CHECK(CtrOptions_RowCount() == 40, "the OPTIONS screen has %d rows", CtrOptions_RowCount());
    CHECK(CtrOptions_RowCount() <= PT_OPT_MAX * CtrOptions_PageCount(), "more rows than the pages hold");
    /* The rows' English labels name them; the other languages have their own words, so only the English build compares them. */
#if !CTR_LANG_ES && !CTR_LANG_DE
    CHECK(strcmp(CtrOptions_Row(8)->label, "FPS counter") == 0, "FPS COUNTER is row 6");
    CHECK(strcmp(CtrOptions_Row(19)->label, "Type matchup") == 0, "TYPE MATCHUP is row 7");
    CHECK(strcmp(CtrOptions_Row(20)->label, "Move details") == 0, "MOVE DETAILS is row 8");
    CHECK(strcmp(CtrOptions_Row(9)->label, "Day/Night") == 0, "DAY/NIGHT is row 9");
    CHECK(strcmp(CtrOptions_Row(10)->label, "Hour") == 0, "HOUR is row 10");
    CHECK(strcmp(CtrOptions_Row(21)->label, "Repel again") == 0, "REPEL AGAIN is row 12");
    CHECK(strcmp(CtrOptions_Row(11)->label, "Companion") == 0, "COMPANION is row 13");
#endif

    /* Valid values: the names FireRed uses, FRAME as Type1..Type10. */
    for (int row = 0; row < 9; ++row) {
        int game = CtrOptions_Row(row)->game;

        if (CtrOptions_Row(row)->get)
            continue;                               /* MULTIPLAYER (row 5) is the port's own, tested below */

        for (int v = 0; v < sCounts[game]; ++v) {
            char frame[16];

            sStored[game] = v;
            snprintf(frame, sizeof(frame), "Type%d", v + 1);
            TextWord(row, game == PT_OPT_FRAME ? frame : valid[game][v]);
        }
        sStored[game] = 0;
    }
    /* MULTIPLAYER (row 5): Online until chosen, then Local, kept in the settings; the radio is told through the hook. */
    CHECK(CtrOptions_Count(5) == 2 && CtrOptions_PageOf(5) == 0, "MULTIPLAYER is the last row of Settings");
#if !CTR_LANG_ES && !CTR_LANG_DE
    CHECK(strcmp(CtrOptions_Row(5)->label, "Multiplayer") == 0, "MULTIPLAYER is row 5");
#endif
    TextWord(5, "Online");
    CtrOptions_SetLocalPlayHook(LocalPlayHook);
    CtrOptions_Step(5, 1);
    TextWord(5, "Local");
    CHECK(CtrSettings_LocalPlay() == 1 && sLocalPlayTold == 1, "LOCAL is kept and the radio is told");
    CtrOptions_Step(5, 1);
    TextWord(5, "Online");
    CHECK(CtrSettings_LocalPlay() == 0 && sLocalPlayTold == 0, "back to ONLINE");
    /* ONLINE ROOM (row 6): Public; PRIVATE with no code opens the keypad and stays public until OK; the keys type, delete, and OK takes the
     * six digits (the room is private, the note says the code, the radio is told); a tap on the label changes the code; CANCEL leaves it. */
    {
        CtrPokeTouchView pv;

        CHECK(CtrOptions_PageOf(6) == 0, "ONLINE ROOM is the last row of Settings");
#if !CTR_LANG_ES && !CTR_LANG_DE
        CHECK(strcmp(CtrOptions_Row(6)->label, "Online room") == 0, "ONLINE ROOM is row 6");
#endif
        TextWord(6, "Public");
        sLocalPlayTold = -1;
        CtrOptions_Step(6, 1);
        CHECK(CtrOptions_PadOn() && !CtrSettings_PrivateRoom(), "PRIVATE with no code asks for one, still public");
        memset(&pv, 0, sizeof(pv));
        CtrOptions_Fill(&pv, 6);
        CHECK(pv.optPad.on && pv.optPad.len == 0 && pv.optPad.pressed == -1, "the keypad is in the view, empty");
        CtrOptions_PadTouch(1, 1, PT_PAD_KEY_OK);
        CtrOptions_PadTouch(0, 0, -1);
        CHECK(CtrOptions_PadOn(), "OK does nothing before sixteen digits");
        for (int k = 0; k < 17; ++k) {
            CtrOptions_PadTouch(1, 1, (4 + k) % 10);
            CtrOptions_Fill(&pv, 6);
            CHECK(pv.optPad.pressed == (4 + k) % 10, "the key under the touch is pressed");
            CtrOptions_PadTouch(0, 0, -1);
        }
        CtrOptions_Fill(&pv, 6);
        CHECK(pv.optPad.len == 16 && strcmp(pv.optPad.code, "4567890123456789") == 0 && pv.optPad.ok, "sixteen digits and no more (%s)", pv.optPad.code);
        CtrOptions_PadTouch(1, 1, PT_PAD_KEY_DELETE);
        CtrOptions_PadTouch(0, 0, -1);
        CtrOptions_PadTouch(1, 1, 3);
        CtrOptions_PadTouch(0, 1, 5);                   /* moved onto another key: let go there, nothing */
        CtrOptions_PadTouch(0, 0, -1);
        CtrOptions_PadTouch(1, 1, 1);
        CtrOptions_PadTouch(0, 0, -1);
        CtrOptions_Fill(&pv, 6);
        CHECK(strcmp(pv.optPad.code, "4567890123456781") == 0, "DELETE takes the last; a touch let go off its key types nothing (%s)", pv.optPad.code);
        CtrOptions_PadTouch(1, 1, PT_PAD_KEY_OK);
        CtrOptions_PadTouch(0, 0, -1);
        CHECK(!CtrOptions_PadOn() && CtrSettings_PrivateRoom() && strcmp(CtrSettings_RoomCode(), "4567890123456781") == 0 && sLocalPlayTold == 0,
              "OK: the private room of 4567890123456781, and the radio told");
        TextWord(6, "Private");
        CtrOptions_Fill(&pv, 6);
        CHECK(strstr(pv.bar[1], " 4567 8901 2345 6781.") != NULL && !pv.optPad.on, "the note says the code (%s)", pv.bar[1]);
#if !CTR_LANG_ES && !CTR_LANG_DE
        CHECK(strcmp(pv.bar[1], "Code 4567 8901 2345 6781.") == 0, "the note says the code in English (%s)", pv.bar[1]);
#endif
        CtrOptions_Activate(6);
        CHECK(CtrOptions_PadOn(), "a tap on the label opens the keypad again");
        CtrOptions_PadTouch(1, 1, PT_PAD_KEY_CANCEL);
        CtrOptions_PadTouch(0, 0, -1);
        CHECK(!CtrOptions_PadOn() && strcmp(CtrSettings_RoomCode(), "4567890123456781") == 0, "CANCEL keeps the code");
        CtrOptions_Activate(5);
        CHECK(!CtrOptions_PadOn(), "another row's label opens nothing");
        CtrOptions_Step(6, 1);
        CHECK(!CtrSettings_PrivateRoom() && strcmp(CtrSettings_RoomCode(), "4567890123456781") == 0, "back to PUBLIC; the code is kept");
        CtrOptions_Step(6, 1);
        CHECK(CtrSettings_PrivateRoom() && !CtrOptions_PadOn(), "PRIVATE again with a code: no keypad");
        CtrOptions_Step(6, 1);
        /* NEW makes a code at random (from the source's bytes, here a fixed run), which OK then takes; a source with nothing, or a run that makes a refused code,
         * leaves the boxes as they were. */
        CtrOptions_SetRandomSource(RunSource);
        CtrOptions_Step(6, 1);                                  /* PRIVATE again (the code is kept: no keypad) */
        CtrOptions_Activate(6);
        CtrOptions_PadTouch(1, 1, PT_PAD_KEY_NEW);
        CtrOptions_PadTouch(0, 0, -1);
        CtrOptions_Fill(&pv, 6);
        CHECK(pv.optPad.on && pv.optPad.len == 16 && strcmp(pv.optPad.code, RunCode()) == 0 && pv.optPad.ok, "NEW fills the boxes with the source's code (%s)", pv.optPad.code);
        CtrOptions_PadTouch(1, 1, PT_PAD_KEY_OK);
        CtrOptions_PadTouch(0, 0, -1);
        CHECK(!CtrOptions_PadOn() && strcmp(CtrSettings_RoomCode(), RunCode()) == 0, "OK takes it");
        CtrOptions_Activate(6);
        CtrOptions_SetRandomSource(ZeroSource);                 /* bytes that make 0000000000000000: refused, at every try */
        CtrOptions_PadTouch(1, 1, PT_PAD_KEY_NEW);
        CtrOptions_PadTouch(0, 0, -1);
        CtrOptions_Fill(&pv, 6);
        CHECK(pv.optPad.on && pv.optPad.len == 0 && !pv.optPad.ok, "a refused run leaves the boxes empty (%s)", pv.optPad.code);
        CtrOptions_SetRandomSource(NoSource);
        CtrOptions_PadTouch(1, 1, PT_PAD_KEY_NEW);
        CtrOptions_PadTouch(0, 0, -1);
        CtrOptions_Fill(&pv, 6);
        CHECK(pv.optPad.len == 0, "no generator: nothing is made");
        for (int k = 0; k < 4; ++k) {                           /* a weak code typed by hand is not taken: OK stays dead */
            CtrOptions_PadTouch(1, 1, 7);
            CtrOptions_PadTouch(0, 0, -1);
        }
        CtrOptions_Fill(&pv, 6);
        for (int k = 0; k < 12; ++k) {
            CtrOptions_PadTouch(1, 1, k % 2 ? 7 : 8);
            CtrOptions_PadTouch(0, 0, -1);
        }
        CtrOptions_Fill(&pv, 6);
        CHECK(pv.optPad.len == 16 && !pv.optPad.ok, "78787878... has too few different digits: OK is not lit");
        CtrOptions_PadTouch(1, 1, PT_PAD_KEY_OK);
        CtrOptions_PadTouch(0, 0, -1);
        CHECK(CtrOptions_PadOn() && strcmp(CtrSettings_RoomCode(), RunCode()) == 0, "and it takes nothing");
        CtrOptions_PadTouch(1, 1, PT_PAD_KEY_CANCEL);
        CtrOptions_PadTouch(0, 0, -1);
        CtrOptions_SetRandomSource(NULL);
    }
    CtrOptions_SetLocalPlayHook(NULL);
    /* Page 2 port options. */
    CHECK(CtrOptions_TypeMatchup() == 1 && CtrOptions_MoveDetails() == 1
          && CtrOptions_DayNight() == 1
          && CtrOptions_Embers() == 1, "default port options");
    CtrOptions_Step(19, 1);
    TextWord(19, "Off");
    CHECK(CtrOptions_TypeMatchup() == 0, "TYPE MATCHUP stepped to OFF");
    CtrOptions_Step(19, -1);
    TextWord(19, "On");
    TextWord(9, "Clock");
    CHECK(CtrOptions_DayNightSecond() == -1, "DAY/NIGHT on CLOCK follows the 3DS clock");
    /* FIXED takes the HOUR row's half hour: 12:00 until changed, any of the 48 after. */
    CtrOptions_Step(9, 1);
    TextWord(9, "Fixed");
    Text(10, "12:00");
    CHECK(CtrOptions_DayNight() == 1 && CtrOptions_DayNightSecond() == 43200, "FIXED at 12:00 is second 43200");
    CtrOptions_Step(10, 1);
    Text(10, "12:30");
    CHECK(CtrOptions_DayNightSecond() == 45000, "HOUR stepped to 12:30");
    CtrOptions_Set(10, 47);
    Text(10, "23:30");
    CHECK(CtrOptions_DayNightSecond() == 84600, "HOUR 23:30 is second 84600");
    CtrOptions_Step(10, 1);
    Text(10, "00:00");
    CHECK(CtrOptions_DayNightSecond() == 0, "HOUR wraps to 00:00");
    CtrOptions_Set(10, 12);
    CHECK(CtrOptions_DayNightSecond() == 21600, "HOUR 06:00 is second 21600");
    CtrOptions_Step(9, 1);
    TextWord(9, "Off");
    CtrOptions_Set(10, 24);
    CHECK(CtrOptions_DayNight() == 0 && CtrOptions_DayNightSecond() == -1, "choosing an hour keeps OFF off");
    CtrOptions_Step(9, 1);
    TextWord(9, "Clock");
    CtrOptions_Set(10, 24);
    TextWord(9, "Fixed");
    CtrOptions_Step(9, 1);
    TextWord(9, "Off");
    CHECK(CtrOptions_DayNight() == 0 && CtrOptions_DayNightSecond() == -1, "DAY/NIGHT stepped to OFF");
    CtrOptions_Step(9, 1);
    TextWord(9, "Clock");
    CtrOptions_Step(9, -1);
    TextWord(9, "Off");
    CtrOptions_Step(9, 1);
    TextWord(9, "Clock");
    /* COMPANION: none until chosen, then TRAIL, then BE IT; a row of levels, kept in the settings. */
    TextWord(11, "Off");
    CHECK(CtrOptions_Companion() == 0 && CtrSettings_Companion() == 0, "COMPANION starts off");
    CtrOptions_Step(11, 1);
    TextWord(11, "Follow");
    CHECK(CtrOptions_Companion() == 1 && CtrSettings_Companion() == 1, "COMPANION stepped to TRAIL");
    CtrOptions_Step(11, 1);
    TextWord(11, "Be it");
    CHECK(CtrOptions_Companion() == 2 && CtrSettings_Companion() == 2, "COMPANION stepped to BE IT");
    CtrOptions_Step(11, 1);
    TextWord(11, "Off");
    CtrOptions_Set(11, 2);
    CtrOptions_Set(11, 3);
    CHECK(CtrOptions_Companion() == 2, "a value that is not a level is refused");
    /* EXP SHARE, alone on page 3: the game's own until chosen, then HALF, then FULL; levels, kept in the settings. */
    TextWord(22, "Off");
    CHECK(CtrOptions_ExpShare() == 0 && CtrSettings_ExpShare() == 0, "EXP SHARE starts off");
    CtrOptions_Step(22, 1);
    TextWord(22, "Half");
    CHECK(CtrOptions_ExpShare() == 1 && CtrSettings_ExpShare() == 1, "EXP SHARE stepped to HALF");
    CtrOptions_Step(22, 1);
    TextWord(22, "Full");
    CHECK(CtrOptions_ExpShare() == 2, "EXP SHARE stepped to FULL");
    CtrOptions_Set(22, 3);
    CHECK(CtrOptions_ExpShare() == 2 && CtrOptions_IsLevels(22), "a value that is not a level is refused; a row of levels");
    CtrOptions_Step(22, 1);
    CHECK(CtrOptions_ExpShare() == 0, "EXP SHARE wraps to OFF");
    memset(&view, 0, sizeof(view));
    CtrOptions_Fill(&view, 22);
    CHECK(view.optPage == 4 && view.optPageCount == 8 && view.optCount == 6 && view.optSection == 2 && view.optSectionCount == 5,
          "the QoL page holds six rows and is the third section of five");
    /* The pages: Settings (7 rows), Graphics (6: the screen; 4: the looks; 3: the wild Pokemon), QoL (6) and Nuzlocke (6), Randomlocke (3), Cheats (7), in five sections. */
    CHECK(CtrOptions_PageCount() == 8 && CtrOptions_SectionCount() == 5
          && CtrOptions_PageBase(0) == 0 && CtrOptions_PageRows(0) == 7 && CtrOptions_PageBase(1) == 7 && CtrOptions_PageRows(1) == 5
          && CtrOptions_PageBase(2) == 12 && CtrOptions_PageRows(2) == 4 && CtrOptions_PageBase(3) == 16 && CtrOptions_PageRows(3) == 3
          && CtrOptions_PageBase(4) == 19 && CtrOptions_PageRows(4) == 6 && CtrOptions_PageBase(5) == 25 && CtrOptions_PageRows(5) == 5
          && CtrOptions_PageBase(6) == 30 && CtrOptions_PageRows(6) == 3 && CtrOptions_PageBase(7) == 33 && CtrOptions_PageRows(7) == 7,
          "eight pages in five sections");
    for (int page = 0; page < CtrOptions_PageCount(); ++page)
        CHECK(CtrOptions_PageName(page)[0] != 0, "page %d has a name", page);
    CHECK(CtrOptions_PageName(-1)[0] == 0 && CtrOptions_PageName(8)[0] == 0, "no page name past the last");
#if !CTR_LANG_ES && !CTR_LANG_DE
    CHECK(strcmp(CtrOptions_PageName(0), "Settings") == 0 && strcmp(CtrOptions_PageName(1), "Graphics") == 0
          && strcmp(CtrOptions_PageName(2), "Looks") == 0 && strcmp(CtrOptions_PageName(3), "Wilds") == 0 && strcmp(CtrOptions_PageName(4), "QoL") == 0
          && strcmp(CtrOptions_PageName(5), "Nuzlocke") == 0 && strcmp(CtrOptions_PageName(6), "Random") == 0
          && strcmp(CtrOptions_PageName(7), "Cheats") == 0,
          "each page has its own short name (the PAGE button is 64 px of text)");
#endif
    {
        int of = 0;

        CHECK(CtrOptions_SectionPage(1, &of) == 1 && of == 3 && CtrOptions_SectionPage(2, &of) == 2 && of == 3 && CtrOptions_SectionPage(3, &of) == 3 && of == 3
              && CtrOptions_SectionPage(0, &of) == 1 && of == 1 && CtrOptions_SectionPage(5, &of) == 1 && of == 2 && CtrOptions_SectionPage(6, &of) == 2 && of == 2
              && CtrOptions_SectionOf(0) == 0 && CtrOptions_SectionOf(1) == 1 && CtrOptions_SectionOf(2) == 1 && CtrOptions_SectionOf(3) == 1
              && CtrOptions_SectionOf(4) == 2 && CtrOptions_SectionOf(5) == 3 && CtrOptions_SectionOf(6) == 3 && CtrOptions_SectionOf(7) == 4
              && CtrOptions_SectionPage(7, &of) == 1 && of == 1, "Graphics has three pages, Nuzlocke two, the rest one");
    }
    memset(&view, 0, sizeof(view));
    CtrOptions_Fill(&view, 12);
    CHECK(view.optPage == 2 && view.optSection == 1, "the looks page is in the Graphics section");
#if !CTR_LANG_ES && !CTR_LANG_DE
    CHECK(strcmp(view.optPageName, "Looks") == 0, "the looks page is named Looks");
#endif
    /* TRUCOS: the Cheats page holds seven rows, all off at first; each is kept and read by the game's accessors. */
    memset(&view, 0, sizeof(view));
    CtrOptions_Fill(&view, 33);
    CHECK(view.optPage == 7 && view.optPageCount == 8 && view.optCount == 7 && view.optSection == 4 && view.optSectionCount == 5,
          "the Cheats page: seven rows, the fifth section");
#if !CTR_LANG_ES && !CTR_LANG_DE
    CHECK(strcmp(view.optPageName, "Cheats") == 0 && strcmp(view.optLabel[0], "Sure catch") == 0 && strcmp(view.optLabel[1], "Shiny odds") == 0,
          "the Cheats page's name and first rows");
#endif
    CHECK(!CtrOptions_SureCatch() && !CtrOptions_NoEncounters() && !CtrOptions_WalkThroughWalls() && !CtrOptions_InfinitePp()
          && !CtrOptions_AlwaysFlee() && CtrOptions_ShinyOdds() == 0 && CtrOptions_ScaleExp(100) == 100, "every cheat is off until chosen");
    for (int i = 0; i < CtrOptions_PageRows(7); ++i) {
        const CtrOptionRow *row = CtrOptions_Row(33 + i);

        CHECK(row->get != NULL && row->set != NULL && row->count != NULL && row->count() >= 2, "cheat row %d has its own accessors and two values or more", i);
    }
    CtrOptions_Row(33)->set(0);                                   /* 0 is ON in a switch row */
    CtrOptions_Row(35)->set(0);                                   /* 0 is ON in a switch row */
    CtrOptions_Row(36)->set(0);                                   /* 0 is ON in a switch row */
    CtrOptions_Row(38)->set(0);                                   /* 0 is ON in a switch row */
    CtrOptions_Row(39)->set(0);                                   /* 0 is ON in a switch row */
    CHECK(CtrOptions_SureCatch() && CtrOptions_NoEncounters() && CtrOptions_WalkThroughWalls() && CtrOptions_InfinitePp() && CtrOptions_AlwaysFlee(),
          "the switches reach the game's accessors");
    CtrOptions_Row(37)->set(2);
    CHECK(CtrOptions_ScaleExp(100) == 400 && CtrOptions_ScaleExp(30000) == 0xFFFF, "EXP x4, and no more than the 16 bits the game keeps");
    CtrOptions_Row(37)->set(9);
    CHECK(CtrOptions_Row(37)->get() == 3 && CtrOptions_ScaleExp(1) == 8, "a value past the last is the last");
    CtrOptions_Row(37)->set(0);
    /* SHINY ODDS: normal leaves every personality alone; "always" makes a shiny one that keeps the gender byte and the nature; a shiny one is not touched. */
    {
        const uint32_t otId = 0x1234ABCDu;
        int shinies = 0;

        CtrOptions_Row(34)->set(0);
        CHECK(CtrOptions_ShinyPersonality(0x89ABCDEFu, otId, 0) == 0x89ABCDEFu, "Normal odds keep the personality");
        CtrOptions_Row(34)->set(6);
        for (uint32_t pid = 0x01010101u, n = 0; n < 2000; ++n, pid = pid * 1664525u + 1013904223u) {
            uint32_t made = CtrOptions_ShinyPersonality(pid, otId, pid * 2654435761u);

            CHECK((((made >> 16) ^ made ^ (otId >> 16) ^ otId) & 0xFFFFu) < 8, "Always: %08X is shiny", (unsigned)made);
            CHECK((made & 0xFFu) == (pid & 0xFFu) && made % 25u == pid % 25u, "Always: gender byte and nature of %08X are kept", (unsigned)pid);
            ++shinies;
        }
        CHECK(shinies == 2000, "every one was made shiny");
        CHECK(CtrOptions_ShinyPersonality(0x0000B9F9u, otId, 77) == 0x0000B9F9u, "a personality that is already shiny is left as it is");
        /* 1/16: about one in sixteen rolls makes one. */
        CtrOptions_Row(34)->set(5);
        {
            int made = 0;

            for (uint32_t n = 0; n < 16000; ++n) {
                uint32_t pid = 0x01010101u + n * 7919u, roll = n * 2654435761u + 12345u;

                if (CtrOptions_ShinyPersonality(pid, otId, roll) != pid)
                    ++made;
            }
            CHECK(made > 800 && made < 1200, "1/16 makes about a sixteenth shiny (%d of 16000)", made);
        }
        CtrOptions_Row(34)->set(0);
    }
    /* SURF POKEMON: the looks page's fourth row: DEFAULT, then the pack's Pokemon; a preview like the looks'. */
    CHECK(CtrOptions_Count(15) == 3 && CtrOptions_PageOf(15) == 2, "SURF POKEMON is the fourth row of the looks page, with DEFAULT and two more");
#if !CTR_LANG_ES && !CTR_LANG_DE
    CHECK(strcmp(CtrOptions_Row(15)->label, "Surf Pokemon") == 0, "SURF POKEMON is row 15");
#endif
    TextWord(15, "Default");
    CtrOptions_Fill(&view, 15);
    CHECK(view.optPage == 2 && view.optCount == 4 && view.optLook.on == 1 && view.optLook.has == 0 && view.optLook.name[0],
          "DEFAULT: the preview names it and has no picture");
#if !CTR_LANG_ES && !CTR_LANG_DE
    CHECK(strcmp(view.optLook.name, "Default") == 0, "DEFAULT: the preview's name");
#endif
    CtrOptions_Step(15, 1);
    Text(15, "LAPRAS");
    CHECK(sSurfChoice == 0, "the first step chooses the first Pokemon");
    CtrOptions_Fill(&view, 15);
    CHECK(view.optLook.on == 1 && view.optLook.has == 1 && strcmp(view.optLook.name, "LAPRAS") == 0 && strcmp(view.optLook.credit, "By eMMe97") == 0
          && view.optLook.tiles[0] == 0x22, "the preview shows its picture, name and artist");
    CtrOptions_Step(15, 1);
    CtrOptions_Fill(&view, 15);
    CHECK(sSurfChoice == 1 && view.optLook.has == 0 && strcmp(view.optLook.credit, "By Nolo33LP") == 0, "a Pokemon without a picture shows its name and artist only");
    CtrOptions_Step(15, 1);
    CHECK(sSurfChoice == -1, "one more step is back to DEFAULT");
    CtrOptions_Fill(&view, 13);
    CHECK(view.optLook.on == 1 && view.optLook.name[0] && view.optLook.credit[0], "the other look rows still preview their own");
#if !CTR_LANG_ES && !CTR_LANG_DE
    CHECK(strcmp(view.optLook.name, "Default") == 0, "the other look rows' preview is DEFAULT");
#endif
    CtrOptions_Fill(&view, 25);
    CHECK(strcmp(view.optPageName, "Nuzlocke") == 0 && view.optPage == 5 && view.optSection == 3, "the Nuzlocke page is named alone");
    CHECK(CtrOptions_PageBase(-1) == 0 && CtrOptions_PageBase(8) == 0 && CtrOptions_PageRows(-1) == 0 && CtrOptions_PageRows(8) == 0,
          "a page that does not exist has no rows");
    {
        int total = 0;

        for (int page = 0; page < CtrOptions_PageCount(); ++page)
        {
            CHECK(CtrOptions_PageRows(page) >= 1 && CtrOptions_PageRows(page) <= PT_OPT_MAX, "page %d holds 1 to PT_OPT_MAX rows", page);
            CHECK(CtrOptions_PageBase(page) == total, "page %d starts where the one before ends", page);
            total += CtrOptions_PageRows(page);
        }
        CHECK(total == CtrOptions_RowCount(), "the pages hold every row once (%d of %d)", total, CtrOptions_RowCount());
    }
    memset(&view, 0, sizeof(view));
    CtrOptions_Fill(&view, CtrOptions_RowCount() - 2);
    CHECK(view.optPage == 7 && view.optCount == 7 && view.optSelected == 5,
          "the last page: its seven rows, the sixth selected");
    CHECK(CtrOptions_PageOf(0) == 0 && CtrOptions_PageOf(4) == 0 && CtrOptions_PageOf(7) == 1 && CtrOptions_PageOf(11) == 1
          && CtrOptions_PageOf(12) == 2 && CtrOptions_PageOf(15) == 2 && CtrOptions_PageOf(16) == 3 && CtrOptions_PageOf(18) == 3
          && CtrOptions_PageOf(19) == 4 && CtrOptions_PageOf(24) == 4 && CtrOptions_PageOf(25) == 5 && CtrOptions_PageOf(29) == 5
          && CtrOptions_PageOf(30) == 6 && CtrOptions_PageOf(32) == 6 && CtrOptions_PageOf(33) == 7 && CtrOptions_PageOf(98) == 7, "a row's page");
    /* FREE HMS: off by default, a switch that is kept in the settings. */
    CHECK(CtrOptions_FreeHm() == 0 && CtrSettings_FreeHm() == 0 && CtrOptions_Count(24) == 2, "FREE HMS starts off");
    TextWord(24, "Off");
    CtrOptions_Step(24, 1);
    CHECK(CtrOptions_FreeHm() == 1 && CtrSettings_FreeHm() == 1, "FREE HMS on, kept in the settings");
    TextWord(24, "On");
    CtrOptions_Step(24, 1);
    CHECK(CtrOptions_FreeHm() == 0, "FREE HMS back off");
    /* RANDOMLOCKE: off by default, a switch that is kept in the settings. */
    CHECK(CtrOptions_Randomlocke() == 0 && CtrSettings_Randomlocke() == 0 && CtrOptions_Count(30) == 2, "RANDOMLOCKE starts off");
    TextWord(30, "Off");
    CtrOptions_Step(30, 1);
    CHECK(CtrOptions_Randomlocke() == 1 && CtrSettings_Randomlocke() == 1, "RANDOMLOCKE on, kept in the settings");
    TextWord(30, "On");
    CtrOptions_Step(30, 1);
    CHECK(CtrOptions_Randomlocke() == 0, "RANDOMLOCKE back off");
    /* Its species: a real one always, the same for the same seed, place and species, and not the same for every seed or place. */
    {
        unsigned seen[412] = {0}, kinds = 0, moved = 0, bad = 0;

        for (unsigned place = 0; place < 400; ++place)
            for (unsigned species = 1; species < 40; ++species)
            {
                unsigned made = CtrOptions_RandomSpecies(0x12345678u, place, species);

                if (made < 1 || made > 411 || (made > 251 && made < 277))
                    ++bad;
                else if (!seen[made]++)
                    ++kinds;
                if (made != CtrOptions_RandomSpecies(0x12345678u, place, species))
                    ++bad;
                if (made != CtrOptions_RandomSpecies(0x12345679u, place, species))
                    ++moved;
            }
        CHECK(bad == 0, "a random species is one of the 386 and repeats for the same seed, place and species");
        CHECK(kinds == 386, "every species can come out (%u of 386)", kinds);
        CHECK(moved > 15000, "another Trainer ID gives another game (%u of 15600 differ)", moved);
    }
    /* NUZLOCKE: off by default, a switch that is kept in the settings. */
    CHECK(CtrOptions_Nuzlocke() == 0 && CtrSettings_Nuzlocke() == 0 && CtrOptions_Count(25) == 2, "NUZLOCKE starts off");
    TextWord(25, "Off");
    CtrOptions_Step(25, 1);
    CHECK(CtrOptions_Nuzlocke() == 1 && CtrSettings_Nuzlocke() == 1, "NUZLOCKE on, kept in the settings");
    TextWord(25, "On");
    CtrOptions_Step(25, 1);
    CHECK(CtrOptions_Nuzlocke() == 0, "NUZLOCKE back off");
    /* The three optional rules (page 6, rows 24 to 26): off by default, each its own switch kept in the settings. */
    CHECK(CtrOptions_NuzDuplicates() == 0 && CtrOptions_NuzShiny() == 0 && CtrOptions_NuzLevelCap() == 0
          && CtrOptions_Count(26) == 2 && CtrOptions_Count(27) == 2 && CtrOptions_Count(28) == 2, "the NUZLOCKE options start off");
    TextWord(26, "Off");
    CtrOptions_Step(26, 1);
    CtrOptions_Step(28, 1);
    CHECK(CtrOptions_NuzDuplicates() == 1 && CtrSettings_NuzDuplicates() == 1 && CtrOptions_NuzShiny() == 0
          && CtrOptions_NuzLevelCap() == 1 && CtrSettings_NuzLevelCap() == 1, "DUPLICATES and LEVEL CAPS on, SHINY CLAUSE still off");
    TextWord(26, "On");
    CtrOptions_Step(27, 1);
    CHECK(CtrOptions_NuzShiny() == 1 && CtrSettings_NuzShiny() == 1, "SHINY CLAUSE on, kept in the settings");
    CtrOptions_Step(26, 1);
    CtrOptions_Step(27, 1);
    CtrOptions_Step(28, 1);
    CHECK(CtrOptions_NuzDuplicates() == 0 && CtrOptions_NuzShiny() == 0 && CtrOptions_NuzLevelCap() == 0, "the NUZLOCKE options back off");
    memset(&view, 0, sizeof(view));
    CtrOptions_Fill(&view, 27);
    CHECK(view.optPage == 5 && view.optCount == 5 && view.optLabel[0][0] && view.optLabel[4][0] && view.optLabel[5][0] == 0, "the Nuzlocke page holds five rows");
    /* SET MODE (row 27): off, and only counts while NUZLOCKE is on. */
    CHECK(CtrOptions_NuzSetMode() == 0 && CtrOptions_Count(29) == 2, "SET MODE starts off");
    TextWord(29, "Off");
    CtrOptions_Step(29, 1);
    CHECK(CtrSettings_NuzSet() == 1 && CtrOptions_NuzSetMode() == 0, "SET MODE is kept but counts only with NUZLOCKE on");
    CtrOptions_Step(25, 1);
    CHECK(CtrOptions_NuzSetMode() == 1, "SET MODE counts with NUZLOCKE on");
    CtrOptions_Step(25, 1);
    CtrOptions_Step(29, 1);
    CHECK(CtrSettings_NuzSet() == 0 && CtrOptions_NuzSetMode() == 0, "SET MODE and NUZLOCKE back off");
    /* RANDOM FOR (row 29) and LEGENDARIES (row 30): both Wild and Trainers, legendaries allowed, until chosen. */
    CHECK(CtrOptions_Count(31) == 3 && CtrOptions_Count(32) == 2 && CtrSettings_RandomTarget() == 0 && CtrSettings_RandomNoLegendary() == 0,
          "RANDOM FOR has three values and starts at Both, LEGENDARIES starts allowed");
    CtrOptions_Step(30, 1);
    CHECK(CtrOptions_RandomWild() == 1 && CtrOptions_RandomTrainers() == 1, "RANDOMLOCKE on: wild and trainers");
    CtrOptions_Step(31, 1);
    CHECK(CtrSettings_RandomTarget() == 1 && CtrOptions_RandomWild() == 1 && CtrOptions_RandomTrainers() == 0, "WILD only");
    CtrOptions_Step(31, 1);
    CHECK(CtrSettings_RandomTarget() == 2 && CtrOptions_RandomWild() == 0 && CtrOptions_RandomTrainers() == 1, "TRAINERS only");
    CtrOptions_Step(31, 1);
    CHECK(CtrSettings_RandomTarget() == 0, "back to Both");
    CtrOptions_Step(30, 1);
    CHECK(CtrOptions_RandomWild() == 0 && CtrOptions_RandomTrainers() == 0, "RANDOMLOCKE off: nothing is random whatever the target");
    CtrSettings_SetRandomTarget(9);
    CHECK(CtrSettings_RandomTarget() == 0, "an unknown target is Both");
    CtrOptions_Step(32, 1);
    CHECK(CtrSettings_RandomNoLegendary() == 1, "LEGENDARIES off is kept in the settings");
    {
        unsigned legend = 0, bad = 0, kinds = 0, seen[412] = {0};

        for (unsigned place = 0; place < 600; ++place)
            for (unsigned species = 1; species < 40; ++species) {
                unsigned made = CtrOptions_RandomSpecies(0x12345678u, place, species);

                if ((made >= 144 && made <= 146) || made == 150 || made == 151 || (made >= 243 && made <= 245) || (made >= 249 && made <= 251)
                    || (made >= 401 && made <= 410))
                    ++legend;
                if (made < 1 || made > 411 || (made > 251 && made < 277) || made != CtrOptions_RandomSpecies(0x12345678u, place, species))
                    ++bad;
                else if (!seen[made]++)
                    ++kinds;
            }
        CHECK(legend == 0 && bad == 0, "no legendary comes out when they are off, and the choice repeats (%u legendary, %u bad)", legend, bad);
        CHECK(kinds == 386 - 21, "every other species still can (%u of %u)", kinds, 386u - 21u);
    }
    CtrOptions_Step(32, 1);
    CHECK(CtrSettings_RandomNoLegendary() == 0, "LEGENDARIES allowed again");
    /* The Wilds page (Graphics, page 3: rows 14 to 16): WILD POKEMON, WILD DISPLAY and AGGRESSIVE, all off or plain until chosen, kept in the settings. */
    CHECK(CtrOptions_WildDensity() == 0 && CtrSettings_WildDensity() == 0 && CtrOptions_Count(16) == 4, "WILD POKEMON starts off, with four values");
#if !CTR_LANG_ES && !CTR_LANG_DE
    CHECK(strcmp(CtrOptions_Row(16)->label, "Wild Pokemon") == 0 && strcmp(CtrOptions_Row(17)->label, "Wild display") == 0
          && strcmp(CtrOptions_Row(18)->label, "Aggressive") == 0, "the three rows of the Wilds page");
#endif
    TextWord(16, "Off");
    CtrOptions_Step(16, 1);
    TextWord(16, "Few");
    CtrOptions_Step(16, 1);
    TextWord(16, "Normal");
    CtrOptions_Step(16, 1);
    TextWord(16, "Many");
    CHECK(CtrOptions_WildDensity() == 3 && CtrSettings_WildDensity() == 3, "WILD POKEMON stepped to MANY, kept in the settings");
    CtrOptions_Step(16, 1);
    TextWord(16, "Off");
    CtrOptions_Set(16, 4);
    CHECK(CtrOptions_WildDensity() == 0, "a value that does not exist is refused");
    CtrOptions_Set(16, 2);
    CHECK(CtrOptions_WildDensity() == 2 && !CtrOptions_IsLevels(16), "NORMAL picked outright; the row is a value between arrows");
    CHECK(CtrOptions_WildSilhouette() == 0 && CtrOptions_Count(17) == 2, "WILD DISPLAY starts with Pokemon");
    TextWord(17, "Pokemon");
    CtrOptions_Step(17, 1);
    TextWord(17, "Silhouette");
    CHECK(CtrOptions_WildSilhouette() == 1 && CtrSettings_WildSilhouette() == 1, "WILD DISPLAY on silhouettes, kept in the settings");
    CtrOptions_Step(17, 1);
    CHECK(CtrOptions_WildSilhouette() == 0, "WILD DISPLAY back to Pokemon");
    CHECK(CtrOptions_WildAggro() == 0 && CtrOptions_Count(18) == 5, "AGGRESSIVE starts off, with five values");
    TextWord(18, "Off");
    CtrOptions_Step(18, 1);
    TextWord(18, "Rare");
    CtrOptions_Step(18, 1);
    TextWord(18, "Low");
    CtrOptions_Step(18, 1);
    TextWord(18, "Moderate");
    CtrOptions_Step(18, 1);
    TextWord(18, "High");
    CHECK(CtrOptions_WildAggro() == 4 && CtrSettings_WildAggro() == 4, "AGGRESSIVE stepped to HIGH, kept in the settings");
    CtrOptions_Step(18, 1);
    CHECK(CtrOptions_WildAggro() == 0, "AGGRESSIVE wraps to OFF");
    CtrOptions_Step(18, -1);
    CHECK(CtrOptions_WildAggro() == 4, "and back to HIGH");
    CtrOptions_Set(16, 0);
    CtrOptions_Set(18, 0);
    memset(&view, 0, sizeof(view));
    CtrOptions_Fill(&view, 17);
    CHECK(view.optPage == 3 && view.optPageCount == 8 && view.optCount == 3 && view.optSelected == 1 && view.optSection == 1 && view.optSectionCount == 5
          && view.optLabel[0][0] && view.optLabel[3][0] == 0,
          "the Wilds page: three rows, the third page of Graphics");
#if !CTR_LANG_ES && !CTR_LANG_DE
    CHECK(strcmp(view.optPageName, "Wilds") == 0 && strcmp(view.optLabel[0], "Wild Pokemon") == 0, "the Wilds page's name and first row");
#endif
    /* REUSABLE TMS: off by default, a switch that is kept in the settings. */
    CHECK(CtrOptions_ReusableTm() == 0 && CtrSettings_TmReuse() == 0 && CtrOptions_Count(23) == 2, "REUSABLE TMS starts off");
    TextWord(23, "Off");
    CtrOptions_Step(23, 1);
    CHECK(CtrOptions_ReusableTm() == 1 && CtrSettings_TmReuse() == 1, "REUSABLE TMS on, kept in the settings");
    TextWord(23, "On");
    CtrOptions_Set(23, 2);
    CHECK(CtrOptions_ReusableTm() == 1, "a value that is not a switch position is refused");
    CtrOptions_Step(23, 1);
    CHECK(CtrOptions_ReusableTm() == 0 && CtrSettings_TmReuse() == 0, "REUSABLE TMS back off");
    /* The looks of Red, Leaf and the rival: DEFAULT, then the pack's (here three); the choice goes to the game's looks. */
    TextWord(12, "Default");
    CHECK(CtrOptions_Count(12) == 4 && CtrOptions_Count(14) == 4, "the look rows have DEFAULT and three looks");
    CtrOptions_Step(12, 1);
    Text(12, "ASH");
    CtrOptions_Step(12, 1);
    Text(12, "GARY");
    CtrOptions_Set(12, 3);
    Text(12, "GREEN");
    CtrOptions_Step(12, 1);
    TextWord(12, "Default");
    CtrOptions_Step(12, -1);
    Text(12, "GREEN");
    CtrOptions_Set(13, 1);
    CtrOptions_Set(14, 2);
    CHECK(sLookChoice[CTR_LOOK_RED] == 2 && sLookChoice[CTR_LOOK_LEAF] == 0 && sLookChoice[CTR_LOOK_BLUE] == 1, "each row sets its own slot");
    Text(13, "ASH");
    Text(14, "GARY");
    CtrOptions_Set(14, 9);
    Text(14, "GARY");
    CHECK(!CtrOptions_IsLevels(12), "a look row is a value between arrows");
    /* The preview: on while a look row is selected, for the look chosen for that row (GREEN for Red here, ASH for Leaf, GARY for
     * the rival), the default's name with no picture, and nothing on another row. */
    memset(&view, 0, sizeof(view));
    CtrOptions_Fill(&view, 12);
    CHECK(view.optLook.on && view.optLook.has && strcmp(view.optLook.name, "GREEN") == 0
          && strcmp(view.optLook.credit, "By Poltergeist") == 0 && view.optLook.tiles[0] == 0x33 && view.optLook.palette[5] == 5,
          "the preview of the look chosen for Red");
    memset(&view, 0, sizeof(view));
    CtrOptions_Fill(&view, 14);
    CHECK(view.optLook.on && !view.optLook.has && strcmp(view.optLook.name, "GARY") == 0
          && strcmp(view.optLook.credit, "By Alan_Flygon") == 0, "a look without a picture: its name and artists only");
    CtrOptions_Set(13, 0);
    memset(&view, 0, sizeof(view));
    CtrOptions_Fill(&view, 13);
    CHECK(view.optLook.on && !view.optLook.has && view.optLook.name[0] && view.optLook.credit[0],
          "the default: its name and no picture");
#if !CTR_LANG_ES && !CTR_LANG_DE
    CHECK(strcmp(view.optLook.name, "Default") == 0, "the default's name");
#endif
    memset(&view, 0, sizeof(view));
    CtrOptions_Fill(&view, 22);
    CHECK(!view.optLook.on && view.optLook.name[0] == 0, "no preview on another row");
    CtrOptions_Fill(&view, 3);
    CHECK(!view.optLook.on, "no preview on the first page");
    memset(&view, 0, sizeof(view));
    CtrOptions_Fill(&view, 11);
    CHECK(view.optSegCount[4] == 3 && view.optSegValue[4] == 2 && view.optSegName[4][1][0] && view.optSegName[4][2][0],
          "COMPANION shown as levels (the fifth row of Graphics)");
#if !CTR_LANG_ES && !CTR_LANG_DE
    CHECK(strcmp(view.optSegName[4][1], "Follow") == 0 && strcmp(view.optSegName[4][2], "Be it") == 0, "COMPANION's level names");
#endif
    CtrOptions_Set(11, 0);
    /* The choice outlives the session (settings.cfg); without one the console decides. */
    CtrSettings_SetEmbers(CTR_EMBERS_HIGH);
    CHECK(CtrSettings_Embers() == CTR_EMBERS_HIGH, "EMBERS kept in the settings");
    CtrOptions_EmbersInit(0);
    CHECK(CtrOptions_Embers() == 1 && CtrOptions_EmbersLow() == 0, "a kept HIGH wins over the Old 3DS default");
    CtrSettings_SetEmbers(CTR_EMBERS_LOW);
    CtrOptions_EmbersInit(1);
    CHECK(CtrOptions_EmbersLow() == 1, "a kept LOW wins over the New 3DS default");
    CtrSettings_SetEmbers(CTR_EMBERS_HIGH);
    memset(&view, 0, sizeof(view));
    CtrOptions_Fill(&view, 7);
    CHECK(view.optFrameShow == 1 && view.optFrameType == 0, "FRAME selected shows its sample (TYPE1)");
    sStored[PT_OPT_FRAME] = 3;
    CtrOptions_Fill(&view, 7);
    CHECK(view.optFrameShow == 1 && view.optFrameType == 3, "the sample follows the type");
    sStored[PT_OPT_FRAME] = 31;
    CtrOptions_Fill(&view, 7);
    CHECK(view.optFrameShow == 0, "no sample for a frame type the menu does not have");
    sStored[PT_OPT_FRAME] = 0;
    CtrOptions_Fill(&view, 0);
    CHECK(view.optFrameShow == 0, "no sample on other rows");
    CtrOptions_Fill(&view, 20);
    CHECK(view.optPage == 4 && view.optPageCount == 8 && view.optCount == 6 && view.optSelected == 1 && view.optLabel[1][0], "QoL Fill");
#if !CTR_LANG_ES && !CTR_LANG_DE
    CHECK(strcmp(view.optLabel[1], "Move details") == 0, "QoL Fill: MOVE DETAILS is its second row");
#endif
    /* ON/OFF rows are switches (on, off), the others text: EXP SHARE's levels are words, not a switch. */
    CHECK(view.optSwitch[0] == 2 && view.optSwitch[1] == 2 && view.optSwitch[2] == 2 && view.optSwitch[3] == 0, "QoL switches");
    CtrOptions_Set(20, 1);
    CtrOptions_Step(9, -1);
    CtrOptions_Fill(&view, 20);
    CHECK(view.optSwitch[1] == 1, "MOVE DETAILS off is an off switch");
    CtrOptions_Fill(&view, 9);
    CHECK(view.optSwitch[2] == 0 && view.optValue[2][0] && strcmp(view.optValue[2], "?") != 0, "DAY/NIGHT OFF stays text");
#if !CTR_LANG_ES && !CTR_LANG_DE
    CHECK(strcmp(view.optValue[2], "Off") == 0, "DAY/NIGHT OFF reads Off");
#endif
    CtrOptions_Set(20, 0);
    CtrOptions_Step(9, 1);
    /* REPEL AGAIN: on by default, kept in settings.cfg. */
    TextWord(21, "On");
    CHECK(CtrOptions_RepelAgain() == 1, "REPEL AGAIN starts on");
    CtrOptions_Step(21, 1);
    TextWord(21, "Off");
    CHECK(CtrOptions_RepelAgain() == 0 && CtrSettings_RepelAgain() == 0, "REPEL AGAIN off, kept in the settings");
    CtrOptions_Step(21, 1);
    CHECK(CtrOptions_RepelAgain() == 1 && CtrSettings_RepelAgain() == 1, "REPEL AGAIN back on");

    /* Stepping wraps both ways through the valid values. */
    sStored[PT_OPT_TEXT_SPEED] = 2;
    CtrOptions_Step(0, 1);
    CHECK(sStored[PT_OPT_TEXT_SPEED] == 0, "TEXT SPEED forward from FAST");
    CtrOptions_Step(0, -1);
    CHECK(sStored[PT_OPT_TEXT_SPEED] == 2, "TEXT SPEED back from SLOW");
    sStored[PT_OPT_FRAME] = 9;
    CtrOptions_Step(7, 1);
    CHECK(sStored[PT_OPT_FRAME] == 0, "FRAME forward from TYPE10");

    /* Out-of-range saved values (the bits hold up to 7 for TEXT SPEED, 31 for FRAME, 255 for BUTTON
     * MODE): shown as "?", never written back by showing them. */
    sStored[PT_OPT_TEXT_SPEED] = 7;
    sStored[PT_OPT_BUTTON_MODE] = 255;
    sStored[PT_OPT_FRAME] = 31;
    sStored[PT_OPT_FPS] = -1;
    sSets = 0;
    Text(0, "?");
    Text(4, "?");
    Text(7, "?");
    Text(8, "?");
    memset(&view, 0, sizeof(view));
    CtrOptions_Fill(&view, 0);
    CHECK(strcmp(view.optValue[0], "?") == 0 && strcmp(view.optValue[4], "?") == 0 && view.optValue[1][0] && strcmp(view.optValue[1], "?") != 0,
          "Fill with bad values");
#if !CTR_LANG_ES && !CTR_LANG_DE
    CHECK(strcmp(view.optValue[1], "On") == 0, "Fill with bad values: the good row beside them reads On");
#endif
    CtrOptions_Fill(&view, 7);
    CHECK(strcmp(view.optValue[0], "?") == 0 && strcmp(view.optValue[1], "?") == 0, "Fill with bad values (the Graphics page)");
    CtrOptions_Fill(&view, 0);
    CHECK(sSets == 0 && sStored[PT_OPT_TEXT_SPEED] == 7 && sStored[PT_OPT_FRAME] == 31,
          "showing an invalid value changed the save");

    /* A player's step from an invalid value lands on a valid one: forward the first, back the last. */
    CtrOptions_Step(0, 1);
    CHECK(sStored[PT_OPT_TEXT_SPEED] == 0, "forward from an invalid TEXT SPEED gave %d", sStored[PT_OPT_TEXT_SPEED]);
    CtrOptions_Step(4, -1);
    CHECK(sStored[PT_OPT_BUTTON_MODE] == 2, "back from an invalid BUTTON MODE gave %d", sStored[PT_OPT_BUTTON_MODE]);
    CtrOptions_Step(7, -1);
    CHECK(sStored[PT_OPT_FRAME] == 9, "back from an invalid FRAME gave %d", sStored[PT_OPT_FRAME]);
    CtrOptions_Step(8, 1);
    CHECK(sStored[PT_OPT_FPS] == 0, "forward from a negative FPS gave %d", sStored[PT_OPT_FPS]);

    /* Bad rows and buffers. */
    Text(-1, "");
    Text(CtrOptions_RowCount(), "");
    CHECK(CtrOptions_Row(-1) == NULL && CtrOptions_Row(101) == NULL, "rows out of range");
    CHECK(CtrOptions_Count(101) == 0 && CtrOptions_Get(100) == 0, "count/get of a bad row");
    sSets = 0;
    CtrOptions_Step(101, 1);
    CtrOptions_Step(-5, -1);
    CHECK(sSets == 0, "stepping a bad row wrote the save");
    CtrOptions_ValueText(0, NULL, 8);
    memset(small, 'x', sizeof(small));
    CtrOptions_ValueText(1, small, 0);
    CHECK(small[0] == 'x', "a zero-size buffer was written");
    CtrOptions_ValueText(-1, small, 0);
    CHECK(small[0] == 'x', "a zero-size buffer was written for a bad row");
    sStored[PT_OPT_BATTLE_STYLE] = 0;
    CtrOptions_ValueText(2, small, (int)sizeof(small));
    CHECK(strlen(small) == sizeof(small) - 1, "a short buffer is filled and ends in its NUL (\"%s\")", small);
#if !CTR_LANG_ES && !CTR_LANG_DE
    CHECK(strcmp(small, "Shi") == 0, "a short buffer holds \"%s\"", small);
#endif
    memset(&view, 0, sizeof(view));
    CtrOptions_Fill(&view, 44);
    CHECK(view.optSelected == 0 && view.optCount == 7, "Fill with a bad selection");
    CtrOptions_Fill(NULL, 0);

    if (failures) { printf("%d failures\n", failures); return 1; }
    puts("PASS poketouch options");
    return 0;
}
