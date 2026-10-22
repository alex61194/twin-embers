/* The native OPTIONS rows (3ds_poketouch_options.c) against a stub game bridge: valid FireRed
 * values read and step as before, and an out-of-range value from a damaged or edited save is shown
 * as "?" without reading past the name tables or being written back; only a player's step
 * replaces it, with a valid value. Bad rows and small or missing output buffers are safe. */
#include <stdio.h>
#include <string.h>

#include "3ds_poketouch_options.h"

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

static void Text(int row, const char *expect)
{
    char out[PT_OPT_VALUE_LEN];

    memset(out, 'x', sizeof(out));
    CtrOptions_ValueText(row, out, (int)sizeof(out));
    CHECK(strcmp(out, expect) == 0, "row %d shows \"%s\", expected \"%s\"", row, out, expect);
}

int main(void)
{
    static const char *const valid[][3] = {{"SLOW", "MID", "FAST"}, {"ON", "OFF"}, {"SHIFT", "SET"},
                                           {"MONO", "STEREO"}, {"HELP", "LR", "L=A"}, {0}, {"ON", "OFF"}};
    CtrPokeTouchView view;
    char small[4];

    CHECK(CtrOptions_RowCount() == 12, "the OPTIONS screen has %d rows", CtrOptions_RowCount());
    CHECK(CtrOptions_RowCount() <= PT_OPT_MAX * 2, "more rows than two pages hold");
    CHECK(strcmp(CtrOptions_Row(6)->label, "FPS COUNTER") == 0, "FPS COUNTER is row 6");
    CHECK(strcmp(CtrOptions_Row(7)->label, "TYPE MATCHUP") == 0, "TYPE MATCHUP is row 7");
    CHECK(strcmp(CtrOptions_Row(8)->label, "MOVE DETAILS") == 0, "MOVE DETAILS is row 8");
    CHECK(strcmp(CtrOptions_Row(9)->label, "FIELD SUMMARY") == 0, "FIELD SUMMARY is row 9");
    CHECK(strcmp(CtrOptions_Row(10)->label, "DAY/NIGHT") == 0, "DAY/NIGHT is row 10");
    CHECK(strcmp(CtrOptions_Row(11)->label, "EMBERS") == 0, "EMBERS is row 11");

    /* Valid values: the names FireRed uses, FRAME as TYPE1..TYPE10. */
    for (int row = 0; row < 7; ++row) {
        int game = CtrOptions_Row(row)->game;

        for (int v = 0; v < sCounts[game]; ++v) {
            char frame[16];

            sStored[game] = v;
            snprintf(frame, sizeof(frame), "TYPE%d", v + 1);
            Text(row, game == PT_OPT_FRAME ? frame : valid[game][v]);
        }
        sStored[game] = 0;
    }
    /* Page 2 port options. */
    CHECK(CtrOptions_TypeMatchup() == 1 && CtrOptions_MoveDetails() == 1 && CtrOptions_FieldSummary() == 0
          && CtrOptions_DayNight() == 1
          && CtrOptions_Embers() == 1, "default port options");
    CtrOptions_Step(7, 1);
    Text(7, "OFF");
    CHECK(CtrOptions_TypeMatchup() == 0, "TYPE MATCHUP stepped to OFF");
    CtrOptions_Step(7, -1);
    Text(7, "ON");
    CtrOptions_Step(9, 1);
    Text(9, "GBA");
    CHECK(CtrOptions_FieldSummary() == 1, "FIELD SUMMARY stepped to GBA");
    CtrOptions_Step(9, 1);
    Text(9, "3DS");
    Text(10, "CLOCK");
    CtrOptions_Step(10, 1);
    Text(10, "OFF");
    CHECK(CtrOptions_DayNight() == 0, "DAY/NIGHT stepped to OFF");
    CtrOptions_Step(10, 1);
    Text(10, "CLOCK");
    Text(11, "ON");
    CtrOptions_Step(11, 1);
    Text(11, "OFF");
    CHECK(CtrOptions_Embers() == 0, "EMBERS stepped to OFF");
    CtrOptions_Step(11, 1);
    Text(11, "ON");
    memset(&view, 0, sizeof(view));
    CtrOptions_Fill(&view, 8);
    CHECK(view.optPage == 1 && view.optPageCount == 2 && view.optCount == 5 && view.optSelected == 1
          && strcmp(view.optLabel[1], "MOVE DETAILS") == 0, "Page 2 Fill");

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
    CHECK(strcmp(view.optValue[0], "?") == 0 && strcmp(view.optValue[4], "?") == 0
          && strcmp(view.optValue[5], "?") == 0 && strcmp(view.optValue[1], "ON") == 0, "Fill with bad values");
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
    CHECK(strcmp(small, "SHI") == 0, "a short buffer holds \"%s\"", small);
    memset(&view, 0, sizeof(view));
    CtrOptions_Fill(&view, 42);
    CHECK(view.optSelected == 0 && view.optCount == 7, "Fill with a bad selection");
    CtrOptions_Fill(NULL, 0);

    if (failures) { printf("%d failures\n", failures); return 1; }
    puts("PASS poketouch options");
    return 0;
}
