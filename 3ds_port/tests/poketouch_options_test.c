/* The native OPTIONS rows (3ds_poketouch_options.c) against a stub game bridge: valid FireRed
 * values read and step as before, and an out-of-range value from a damaged or edited save is shown
 * as "?" without reading past the name tables or being written back; only a player's step
 * replaces it, with a valid value. Bad rows and small or missing output buffers are safe. */
#include <stdio.h>
#include <string.h>

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

static void Text(int row, const char *expect)
{
    char out[PT_OPT_VALUE_LEN];

    memset(out, 'x', sizeof(out));
    CtrOptions_ValueText(row, out, (int)sizeof(out));
    CHECK(strcmp(out, expect) == 0, "row %d shows \"%s\", expected \"%s\"", row, out, expect);
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

    remove(CTR_SETTINGS_PATH);      /* no EMBERS choice kept from an earlier run */

    CHECK(CtrOptions_RowCount() == 26, "the OPTIONS screen has %d rows", CtrOptions_RowCount());
    CHECK(CtrOptions_RowCount() <= PT_OPT_MAX * CtrOptions_PageCount(), "more rows than the pages hold");
    CHECK(strcmp(CtrOptions_Row(6)->label, "FPS counter") == 0, "FPS COUNTER is row 6");
    CHECK(strcmp(CtrOptions_Row(15)->label, "Type matchup") == 0, "TYPE MATCHUP is row 7");
    CHECK(strcmp(CtrOptions_Row(16)->label, "Move details") == 0, "MOVE DETAILS is row 8");
    CHECK(strcmp(CtrOptions_Row(7)->label, "Day/Night") == 0, "DAY/NIGHT is row 9");
    CHECK(strcmp(CtrOptions_Row(8)->label, "Hour") == 0, "HOUR is row 10");
    CHECK(strcmp(CtrOptions_Row(9)->label, "Embers") == 0, "EMBERS is row 11");
    CHECK(strcmp(CtrOptions_Row(17)->label, "Repel again") == 0, "REPEL AGAIN is row 12");
    CHECK(strcmp(CtrOptions_Row(10)->label, "Companion") == 0, "COMPANION is row 13");

    /* Valid values: the names FireRed uses, FRAME as Type1..Type10. */
    for (int row = 0; row < 7; ++row) {
        int game = CtrOptions_Row(row)->game;

        for (int v = 0; v < sCounts[game]; ++v) {
            char frame[16];

            sStored[game] = v;
            snprintf(frame, sizeof(frame), "Type%d", v + 1);
            Text(row, game == PT_OPT_FRAME ? frame : valid[game][v]);
        }
        sStored[game] = 0;
    }
    /* Page 2 port options. */
    CHECK(CtrOptions_TypeMatchup() == 1 && CtrOptions_MoveDetails() == 1
          && CtrOptions_DayNight() == 1
          && CtrOptions_Embers() == 1, "default port options");
    CtrOptions_Step(15, 1);
    Text(15, "Off");
    CHECK(CtrOptions_TypeMatchup() == 0, "TYPE MATCHUP stepped to OFF");
    CtrOptions_Step(15, -1);
    Text(15, "On");
    Text(7, "Clock");
    CHECK(CtrOptions_DayNightSecond() == -1, "DAY/NIGHT on CLOCK follows the 3DS clock");
    /* FIXED takes the HOUR row's half hour: 12:00 until changed, any of the 48 after. */
    CtrOptions_Step(7, 1);
    Text(7, "Fixed");
    Text(8, "12:00");
    CHECK(CtrOptions_DayNight() == 1 && CtrOptions_DayNightSecond() == 43200, "FIXED at 12:00 is second 43200");
    CtrOptions_Step(8, 1);
    Text(8, "12:30");
    CHECK(CtrOptions_DayNightSecond() == 45000, "HOUR stepped to 12:30");
    CtrOptions_Set(8, 47);
    Text(8, "23:30");
    CHECK(CtrOptions_DayNightSecond() == 84600, "HOUR 23:30 is second 84600");
    CtrOptions_Step(8, 1);
    Text(8, "00:00");
    CHECK(CtrOptions_DayNightSecond() == 0, "HOUR wraps to 00:00");
    CtrOptions_Set(8, 12);
    CHECK(CtrOptions_DayNightSecond() == 21600, "HOUR 06:00 is second 21600");
    CtrOptions_Step(7, 1);
    Text(7, "Off");
    CtrOptions_Set(8, 24);
    CHECK(CtrOptions_DayNight() == 0 && CtrOptions_DayNightSecond() == -1, "choosing an hour keeps OFF off");
    CtrOptions_Step(7, 1);
    Text(7, "Clock");
    CtrOptions_Set(8, 24);
    Text(7, "Fixed");
    CtrOptions_Step(7, 1);
    Text(7, "Off");
    CHECK(CtrOptions_DayNight() == 0 && CtrOptions_DayNightSecond() == -1, "DAY/NIGHT stepped to OFF");
    CtrOptions_Step(7, 1);
    Text(7, "Clock");
    CtrOptions_Step(7, -1);
    Text(7, "Off");
    CtrOptions_Step(7, 1);
    Text(7, "Clock");
    /* EMBERS: OFF / LOW / HIGH, HIGH until a console default or a choice says otherwise; a row of levels. */
    Text(9, "High");
    CHECK(CtrOptions_Embers() == 1 && CtrOptions_EmbersLow() == 0, "EMBERS starts on HIGH");
    CtrOptions_Step(9, 1);
    Text(9, "Off");
    CHECK(CtrOptions_Embers() == 0 && CtrOptions_EmbersLow() == 0, "EMBERS stepped to OFF");
    CtrOptions_Step(9, 1);
    Text(9, "Low");
    CHECK(CtrOptions_Embers() == 1 && CtrOptions_EmbersLow() == 1, "EMBERS stepped to LOW");
    CtrOptions_Step(9, -1);
    Text(9, "Off");
    CtrOptions_Set(9, CTR_EMBERS_HIGH);
    Text(9, "High");
    CtrOptions_Set(9, 3);
    Text(9, "High");
    memset(&view, 0, sizeof(view));
    CtrOptions_Fill(&view, 9);
    CHECK(view.optSegCount[4] == 3 && view.optSegValue[4] == CTR_EMBERS_HIGH && strcmp(view.optSegName[4][0], "Off") == 0
          && strcmp(view.optSegName[4][2], "High") == 0 && view.optSegCount[3] == 0, "EMBERS shown as levels");
    /* COMPANION: none until chosen, then TRAIL, then BE IT; a row of levels, kept in the settings. */
    Text(10, "Off");
    CHECK(CtrOptions_Companion() == 0 && CtrSettings_Companion() == 0, "COMPANION starts off");
    CtrOptions_Step(10, 1);
    Text(10, "Follow");
    CHECK(CtrOptions_Companion() == 1 && CtrSettings_Companion() == 1, "COMPANION stepped to TRAIL");
    CtrOptions_Step(10, 1);
    Text(10, "Be it");
    CHECK(CtrOptions_Companion() == 2 && CtrSettings_Companion() == 2, "COMPANION stepped to BE IT");
    CtrOptions_Step(10, 1);
    Text(10, "Off");
    CtrOptions_Set(10, 2);
    CtrOptions_Set(10, 3);
    CHECK(CtrOptions_Companion() == 2, "a value that is not a level is refused");
    /* EXP SHARE, alone on page 3: the game's own until chosen, then HALF, then FULL; levels, kept in the settings. */
    Text(18, "Off");
    CHECK(CtrOptions_ExpShare() == 0 && CtrSettings_ExpShare() == 0, "EXP SHARE starts off");
    CtrOptions_Step(18, 1);
    Text(18, "Half");
    CHECK(CtrOptions_ExpShare() == 1 && CtrSettings_ExpShare() == 1, "EXP SHARE stepped to HALF");
    CtrOptions_Step(18, 1);
    Text(18, "Full");
    CHECK(CtrOptions_ExpShare() == 2, "EXP SHARE stepped to FULL");
    CtrOptions_Set(18, 3);
    CHECK(CtrOptions_ExpShare() == 2 && CtrOptions_IsLevels(18), "a value that is not a level is refused; a row of levels");
    CtrOptions_Step(18, 1);
    CHECK(CtrOptions_ExpShare() == 0, "EXP SHARE wraps to OFF");
    memset(&view, 0, sizeof(view));
    CtrOptions_Fill(&view, 18);
    CHECK(view.optPage == 3 && view.optPageCount == 5 && view.optCount == 6 && view.optSection == 2 && view.optSectionCount == 4,
          "the QoL page holds six rows and is the third section of four");
    /* The pages: Settings (5 rows), Graphics (7: the screen; 3: the looks), QoL (6) and Nuzlocke (5), in four sections. */
    CHECK(CtrOptions_PageCount() == 5 && CtrOptions_SectionCount() == 4
          && CtrOptions_PageBase(0) == 0 && CtrOptions_PageRows(0) == 5 && CtrOptions_PageBase(1) == 5 && CtrOptions_PageRows(1) == 7
          && CtrOptions_PageBase(2) == 12 && CtrOptions_PageRows(2) == 3 && CtrOptions_PageBase(3) == 15 && CtrOptions_PageRows(3) == 6
          && CtrOptions_PageBase(4) == 21 && CtrOptions_PageRows(4) == 5, "five pages in four sections");
    CHECK(strcmp(CtrOptions_PageName(0), "Settings") == 0 && strcmp(CtrOptions_PageName(1), "Graphics") == 0
          && strcmp(CtrOptions_PageName(2), "Graphics") == 0 && strcmp(CtrOptions_PageName(3), "QoL") == 0
          && strcmp(CtrOptions_PageName(4), "Nuzlocke") == 0 && CtrOptions_PageName(-1)[0] == 0 && CtrOptions_PageName(5)[0] == 0,
          "each page carries its section's name, none past the last");
    {
        int of = 0;

        CHECK(CtrOptions_SectionPage(1, &of) == 1 && of == 2 && CtrOptions_SectionPage(2, &of) == 2 && of == 2
              && CtrOptions_SectionPage(0, &of) == 1 && of == 1 && CtrOptions_SectionPage(4, &of) == 1 && of == 1
              && CtrOptions_SectionOf(0) == 0 && CtrOptions_SectionOf(1) == 1 && CtrOptions_SectionOf(2) == 1
              && CtrOptions_SectionOf(3) == 2 && CtrOptions_SectionOf(4) == 3, "Graphics has two pages, the rest one");
    }
    memset(&view, 0, sizeof(view));
    CtrOptions_Fill(&view, 12);
    CHECK(strcmp(view.optPageName, "Graphics 2/2") == 0 && view.optPage == 2 && view.optSection == 1, "the view carries the section's name and its page");
    CtrOptions_Fill(&view, 21);
    CHECK(strcmp(view.optPageName, "Nuzlocke") == 0 && view.optPage == 4 && view.optSection == 3, "a one-page section is named alone");
    CHECK(CtrOptions_PageBase(-1) == 0 && CtrOptions_PageBase(5) == 0 && CtrOptions_PageRows(-1) == 0 && CtrOptions_PageRows(5) == 0,
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
    CtrOptions_Fill(&view, CtrOptions_RowCount() - 3);
    CHECK(view.optPage == 4 && view.optCount == 5 && view.optSelected == 2 && view.optLabel[5][0] == 0,
          "the last page: its five rows, the third selected, no sixth");
    CHECK(CtrOptions_PageOf(0) == 0 && CtrOptions_PageOf(4) == 0 && CtrOptions_PageOf(5) == 1 && CtrOptions_PageOf(11) == 1
          && CtrOptions_PageOf(12) == 2 && CtrOptions_PageOf(14) == 2 && CtrOptions_PageOf(15) == 3 && CtrOptions_PageOf(20) == 3
          && CtrOptions_PageOf(21) == 4 && CtrOptions_PageOf(25) == 4 && CtrOptions_PageOf(99) == 4, "a row's page");
    /* FREE HMS: off by default, a switch that is kept in the settings. */
    CHECK(CtrOptions_FreeHm() == 0 && CtrSettings_FreeHm() == 0 && CtrOptions_Count(20) == 2, "FREE HMS starts off");
    Text(20, "Off");
    CtrOptions_Step(20, 1);
    CHECK(CtrOptions_FreeHm() == 1 && CtrSettings_FreeHm() == 1, "FREE HMS on, kept in the settings");
    Text(20, "On");
    CtrOptions_Step(20, 1);
    CHECK(CtrOptions_FreeHm() == 0, "FREE HMS back off");
    /* RANDOMLOCKE: off by default, a switch that is kept in the settings. */
    CHECK(CtrOptions_Randomlocke() == 0 && CtrSettings_Randomlocke() == 0 && CtrOptions_Count(22) == 2, "RANDOMLOCKE starts off");
    Text(22, "Off");
    CtrOptions_Step(22, 1);
    CHECK(CtrOptions_Randomlocke() == 1 && CtrSettings_Randomlocke() == 1, "RANDOMLOCKE on, kept in the settings");
    Text(22, "On");
    CtrOptions_Step(22, 1);
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
    CHECK(CtrOptions_Nuzlocke() == 0 && CtrSettings_Nuzlocke() == 0 && CtrOptions_Count(21) == 2, "NUZLOCKE starts off");
    Text(21, "Off");
    CtrOptions_Step(21, 1);
    CHECK(CtrOptions_Nuzlocke() == 1 && CtrSettings_Nuzlocke() == 1, "NUZLOCKE on, kept in the settings");
    Text(21, "On");
    CtrOptions_Step(21, 1);
    CHECK(CtrOptions_Nuzlocke() == 0, "NUZLOCKE back off");
    /* The three optional rules (page 5, rows 22 to 24): off by default, each its own switch kept in the settings. */
    CHECK(CtrOptions_NuzDuplicates() == 0 && CtrOptions_NuzShiny() == 0 && CtrOptions_NuzLevelCap() == 0
          && CtrOptions_Count(23) == 2 && CtrOptions_Count(24) == 2 && CtrOptions_Count(25) == 2, "the NUZLOCKE options start off");
    Text(23, "Off");
    CtrOptions_Step(23, 1);
    CtrOptions_Step(25, 1);
    CHECK(CtrOptions_NuzDuplicates() == 1 && CtrSettings_NuzDuplicates() == 1 && CtrOptions_NuzShiny() == 0
          && CtrOptions_NuzLevelCap() == 1 && CtrSettings_NuzLevelCap() == 1, "DUPLICATES and LEVEL CAPS on, SHINY CLAUSE still off");
    Text(23, "On");
    CtrOptions_Step(24, 1);
    CHECK(CtrOptions_NuzShiny() == 1 && CtrSettings_NuzShiny() == 1, "SHINY CLAUSE on, kept in the settings");
    CtrOptions_Step(23, 1);
    CtrOptions_Step(24, 1);
    CtrOptions_Step(25, 1);
    CHECK(CtrOptions_NuzDuplicates() == 0 && CtrOptions_NuzShiny() == 0 && CtrOptions_NuzLevelCap() == 0, "the NUZLOCKE options back off");
    memset(&view, 0, sizeof(view));
    CtrOptions_Fill(&view, 23);
    CHECK(view.optPage == 4 && view.optCount == 5 && view.optLabel[0][0] && view.optLabel[4][0] && view.optLabel[5][0] == 0, "the Nuzlocke page holds five rows");
    /* VISIBLE WILDS (page 6, row 25): off by default, a switch kept in the settings. */
    CHECK(CtrOptions_VisibleWilds() == 0 && CtrSettings_VisibleWilds() == 0 && CtrOptions_Count(11) == 2, "VISIBLE WILDS starts off");
    Text(11, "Off");
    CtrOptions_Step(11, 1);
    Text(11, "On");
    CHECK(CtrOptions_VisibleWilds() == 1 && CtrSettings_VisibleWilds() == 1, "VISIBLE WILDS on, kept in the settings");
    CtrOptions_Step(11, 1);
    CHECK(CtrOptions_VisibleWilds() == 0, "VISIBLE WILDS back off");
    CHECK(strcmp(CtrOptions_Row(11)->label, "Visible wilds") == 0, "the row's label");
    /* REUSABLE TMS: off by default, a switch that is kept in the settings. */
    CHECK(CtrOptions_ReusableTm() == 0 && CtrSettings_TmReuse() == 0 && CtrOptions_Count(19) == 2, "REUSABLE TMS starts off");
    Text(19, "Off");
    CtrOptions_Step(19, 1);
    CHECK(CtrOptions_ReusableTm() == 1 && CtrSettings_TmReuse() == 1, "REUSABLE TMS on, kept in the settings");
    Text(19, "On");
    CtrOptions_Set(19, 2);
    CHECK(CtrOptions_ReusableTm() == 1, "a value that is not a switch position is refused");
    CtrOptions_Step(19, 1);
    CHECK(CtrOptions_ReusableTm() == 0 && CtrSettings_TmReuse() == 0, "REUSABLE TMS back off");
    /* The looks of Red, Leaf and the rival: DEFAULT, then the pack's (here three); the choice goes to the game's looks. */
    Text(12, "Default");
    CHECK(CtrOptions_Count(12) == 4 && CtrOptions_Count(14) == 4, "the look rows have DEFAULT and three looks");
    CtrOptions_Step(12, 1);
    Text(12, "ASH");
    CtrOptions_Step(12, 1);
    Text(12, "GARY");
    CtrOptions_Set(12, 3);
    Text(12, "GREEN");
    CtrOptions_Step(12, 1);
    Text(12, "Default");
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
    CHECK(view.optLook.on && !view.optLook.has && strcmp(view.optLook.name, "Default") == 0 && view.optLook.credit[0],
          "the default: its name and no picture");
    memset(&view, 0, sizeof(view));
    CtrOptions_Fill(&view, 18);
    CHECK(!view.optLook.on && view.optLook.name[0] == 0, "no preview on another row");
    CtrOptions_Fill(&view, 3);
    CHECK(!view.optLook.on, "no preview on the first page");
    memset(&view, 0, sizeof(view));
    CtrOptions_Fill(&view, 10);
    CHECK(view.optSegCount[5] == 3 && view.optSegValue[5] == 2 && strcmp(view.optSegName[5][1], "Follow") == 0
          && strcmp(view.optSegName[5][2], "Be it") == 0, "COMPANION shown as levels");
    CtrOptions_Set(10, 0);
    /* The choice outlives the session (settings.cfg); without one the console decides. */
    CHECK(CtrSettings_Embers() == CTR_EMBERS_HIGH, "EMBERS kept in the settings");
    CtrOptions_EmbersInit(0);
    CHECK(CtrOptions_Embers() == 1 && CtrOptions_EmbersLow() == 0, "a kept HIGH wins over the Old 3DS default");
    CtrOptions_Set(9, CTR_EMBERS_LOW);
    CtrOptions_EmbersInit(1);
    CHECK(CtrOptions_EmbersLow() == 1, "a kept LOW wins over the New 3DS default");
    CtrOptions_Set(9, CTR_EMBERS_HIGH);
    memset(&view, 0, sizeof(view));
    CtrOptions_Fill(&view, 5);
    CHECK(view.optFrameShow == 1 && view.optFrameType == 0, "FRAME selected shows its sample (TYPE1)");
    sStored[PT_OPT_FRAME] = 3;
    CtrOptions_Fill(&view, 5);
    CHECK(view.optFrameShow == 1 && view.optFrameType == 3, "the sample follows the type");
    sStored[PT_OPT_FRAME] = 31;
    CtrOptions_Fill(&view, 5);
    CHECK(view.optFrameShow == 0, "no sample for a frame type the menu does not have");
    sStored[PT_OPT_FRAME] = 0;
    CtrOptions_Fill(&view, 0);
    CHECK(view.optFrameShow == 0, "no sample on other rows");
    CtrOptions_Fill(&view, 16);
    CHECK(view.optPage == 3 && view.optPageCount == 5 && view.optCount == 6 && view.optSelected == 1
          && strcmp(view.optLabel[1], "Move details") == 0, "QoL Fill");
    /* ON/OFF rows are switches (on, off), the others text: EXP SHARE's levels are words, not a switch. */
    CHECK(view.optSwitch[0] == 2 && view.optSwitch[1] == 2 && view.optSwitch[2] == 2 && view.optSwitch[3] == 0, "QoL switches");
    CtrOptions_Set(16, 1);
    CtrOptions_Step(7, -1);
    CtrOptions_Fill(&view, 16);
    CHECK(view.optSwitch[1] == 1, "MOVE DETAILS off is an off switch");
    CtrOptions_Fill(&view, 7);
    CHECK(view.optSwitch[2] == 0 && strcmp(view.optValue[2], "Off") == 0, "DAY/NIGHT OFF stays text");
    CtrOptions_Set(16, 0);
    CtrOptions_Step(7, 1);
    /* REPEL AGAIN: on by default, kept in settings.cfg. */
    Text(17, "On");
    CHECK(CtrOptions_RepelAgain() == 1, "REPEL AGAIN starts on");
    CtrOptions_Step(17, 1);
    Text(17, "Off");
    CHECK(CtrOptions_RepelAgain() == 0 && CtrSettings_RepelAgain() == 0, "REPEL AGAIN off, kept in the settings");
    CtrOptions_Step(17, 1);
    CHECK(CtrOptions_RepelAgain() == 1 && CtrSettings_RepelAgain() == 1, "REPEL AGAIN back on");

    /* Stepping wraps both ways through the valid values. */
    sStored[PT_OPT_TEXT_SPEED] = 2;
    CtrOptions_Step(0, 1);
    CHECK(sStored[PT_OPT_TEXT_SPEED] == 0, "TEXT SPEED forward from FAST");
    CtrOptions_Step(0, -1);
    CHECK(sStored[PT_OPT_TEXT_SPEED] == 2, "TEXT SPEED back from SLOW");
    sStored[PT_OPT_FRAME] = 9;
    CtrOptions_Step(5, 1);
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
    Text(5, "?");
    Text(6, "?");
    memset(&view, 0, sizeof(view));
    CtrOptions_Fill(&view, 0);
    CHECK(strcmp(view.optValue[0], "?") == 0 && strcmp(view.optValue[4], "?") == 0 && strcmp(view.optValue[1], "On") == 0,
          "Fill with bad values");
    CtrOptions_Fill(&view, 5);
    CHECK(strcmp(view.optValue[0], "?") == 0 && strcmp(view.optValue[1], "?") == 0, "Fill with bad values (the Graphics page)");
    CtrOptions_Fill(&view, 0);
    CHECK(sSets == 0 && sStored[PT_OPT_TEXT_SPEED] == 7 && sStored[PT_OPT_FRAME] == 31,
          "showing an invalid value changed the save");

    /* A player's step from an invalid value lands on a valid one: forward the first, back the last. */
    CtrOptions_Step(0, 1);
    CHECK(sStored[PT_OPT_TEXT_SPEED] == 0, "forward from an invalid TEXT SPEED gave %d", sStored[PT_OPT_TEXT_SPEED]);
    CtrOptions_Step(4, -1);
    CHECK(sStored[PT_OPT_BUTTON_MODE] == 2, "back from an invalid BUTTON MODE gave %d", sStored[PT_OPT_BUTTON_MODE]);
    CtrOptions_Step(5, -1);
    CHECK(sStored[PT_OPT_FRAME] == 9, "back from an invalid FRAME gave %d", sStored[PT_OPT_FRAME]);
    CtrOptions_Step(6, 1);
    CHECK(sStored[PT_OPT_FPS] == 0, "forward from a negative FPS gave %d", sStored[PT_OPT_FPS]);

    /* Bad rows and buffers. */
    Text(-1, "");
    Text(CtrOptions_RowCount(), "");
    CHECK(CtrOptions_Row(-1) == NULL && CtrOptions_Row(99) == NULL, "rows out of range");
    CHECK(CtrOptions_Count(99) == 0 && CtrOptions_Get(99) == 0, "count/get of a bad row");
    sSets = 0;
    CtrOptions_Step(99, 1);
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
    CHECK(strcmp(small, "Shi") == 0, "a short buffer holds \"%s\"", small);
    memset(&view, 0, sizeof(view));
    CtrOptions_Fill(&view, 42);
    CHECK(view.optSelected == 0 && view.optCount == 5, "Fill with a bad selection");
    CtrOptions_Fill(NULL, 0);

    if (failures) { printf("%d failures\n", failures); return 1; }
    puts("PASS poketouch options");
    return 0;
}
