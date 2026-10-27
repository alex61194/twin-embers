/* The native OPTIONS screen's rows (see 3ds_poketouch_options.h). */
#include <stdio.h>
#include <string.h>

#include "3ds_poketouch_options.h"
#include "3ds_settings.h"

static const char *const sTextSpeed[] = {"SLOW", "MID", "FAST"};
static const char *const sBattleScene[] = {"ON", "OFF"};
static const char *const sBattleStyle[] = {"SHIFT", "SET"};
static const char *const sSound[] = {"MONO", "STEREO"};
static const char *const sButtonMode[] = {"HELP", "LR", "L=A"};
static const char *const sFps[] = {"ON", "OFF"};
static const char *const sSummaryMode[] = {"3DS", "GBA"};
static const char *const sDayNight[] = {"CLOCK", "OFF"};
static const char *const sEmbers[] = {"OFF", "LOW", "HIGH"};     /* CTR_EMBERS_* */

static int sPortTypeMatchup;
static int sPortMoveDetails;
static int sPortFieldSummary;
static int sPortDayNight;
static int sPortEmbers = CTR_EMBERS_HIGH;

static int TwoCount(void) { return 2; }
static int GetTypeMatchup(void) { return sPortTypeMatchup; }
static void SetTypeMatchup(int v) { sPortTypeMatchup = v & 1; }
static int GetMoveDetails(void) { return sPortMoveDetails; }
static void SetMoveDetails(int v) { sPortMoveDetails = v & 1; }
static int GetFieldSummary(void) { return sPortFieldSummary; }
static void SetFieldSummary(int v) { sPortFieldSummary = v & 1; }
static int GetDayNight(void) { return sPortDayNight; }
static void SetDayNight(int v) { sPortDayNight = v & 1; }
static int ThreeCount(void) { return 3; }
static int GetEmbers(void) { return sPortEmbers; }
static void SetEmbers(int v)
{
    sPortEmbers = v < 0 || v >= CTR_EMBERS_COUNT ? CTR_EMBERS_HIGH : v;
    CtrSettings_SetEmbers(sPortEmbers);     /* kept for the next session; the file is written only on a change */
}

int CtrOptions_TypeMatchup(void) { return sPortTypeMatchup == 0; }
int CtrOptions_MoveDetails(void) { return sPortMoveDetails == 0; }
int CtrOptions_FieldSummary(void) { return sPortFieldSummary; }
int CtrOptions_DayNight(void) { return sPortDayNight == 0; }
int CtrOptions_Embers(void) { return sPortEmbers != CTR_EMBERS_OFF; }
int CtrOptions_EmbersLow(void) { return sPortEmbers == CTR_EMBERS_LOW; }

void CtrOptions_EmbersInit(int isNew3ds)
{
    int stored = CtrSettings_Embers();

    sPortEmbers = stored >= 0 && stored < CTR_EMBERS_COUNT ? stored : isNew3ds ? CTR_EMBERS_HIGH : CTR_EMBERS_LOW;
}

/* FireRed's menu order (Page 1), followed by 3DS port QoL options (Page 2). */
static const CtrOptionRow sRows[] = {
    {"TEXT SPEED", {"How fast dialogue", "text is printed."}, PT_OPT_TEXT_SPEED, sTextSpeed, NULL, NULL, NULL, NULL},
    {"BATTLE SCENE", {"Show or skip the move", "animations in battle."}, PT_OPT_BATTLE_SCENE, sBattleScene, NULL,
     NULL, NULL, NULL},
    {"BATTLE STYLE", {"SHIFT offers a switch", "after a foe faints."}, PT_OPT_BATTLE_STYLE, sBattleStyle, NULL,
     NULL, NULL, NULL},
    {"SOUND", {"Mono or stereo sound.", ""}, PT_OPT_SOUND, sSound, NULL, NULL, NULL, NULL},
    {"BUTTON MODE", {"What the L and R buttons", "do: HELP, LR or L=A."}, PT_OPT_BUTTON_MODE, sButtonMode, NULL,
     NULL, NULL, NULL},
    {"FRAME", {"The style of the text", "box's frame."}, PT_OPT_FRAME, NULL, "TYPE", NULL, NULL, NULL},
    /* The 3DS's own: the counter in the top-right corner of the top screen (stored in the save's spare bit). */
    {"FPS COUNTER", {"Show the frames per", "second on the top screen."}, PT_OPT_FPS, sFps, NULL, NULL, NULL, NULL},
    /* Page 2: 3DS Port QoL options. */
    {"TYPE MATCHUP", {"Show type effectiveness", "on battle move cards."}, 0, sFps, NULL,
     GetTypeMatchup, TwoCount, SetTypeMatchup},
    {"MOVE DETAILS", {"Show live move details", "and category in battle."}, 0, sFps, NULL,
     GetMoveDetails, TwoCount, SetMoveDetails},
    {"FIELD SUMMARY", {"Use the 3DS Summary", "screen from field Party."}, 0, sSummaryMode, NULL,
     GetFieldSummary, TwoCount, SetFieldSummary},
    {"DAY/NIGHT", {"Tint the outdoors by the", "hour of the 3DS clock."}, 0, sDayNight, NULL,
     GetDayNight, TwoCount, SetDayNight},
    {"EMBERS", {"Sparks rising behind the", "menus. LOW suits an Old 3DS."}, 0, sEmbers, NULL,
     GetEmbers, ThreeCount, SetEmbers},
};

#define ROWS ((int)(sizeof(sRows) / sizeof(sRows[0])))
_Static_assert(sizeof(sRows) / sizeof(sRows[0]) <= PT_OPT_MAX * 2, "the OPTIONS screen holds up to two pages of PT_OPT_MAX rows");

int CtrOptions_RowCount(void)
{
    return ROWS;
}

const CtrOptionRow *CtrOptions_Row(int row)
{
    return row >= 0 && row < ROWS ? &sRows[row] : NULL;
}

int CtrOptions_Count(int row)
{
    const CtrOptionRow *r = CtrOptions_Row(row);

    if (r == NULL)
        return 0;
    return r->count != NULL ? r->count() : CtrPokeTouch_OptionCount(r->game);
}

int CtrOptions_Get(int row)
{
    const CtrOptionRow *r = CtrOptions_Row(row);

    if (r == NULL)
        return 0;
    return r->get != NULL ? r->get() : CtrPokeTouch_OptionGet(r->game);
}

/* A save's bits can hold more than the menu's values (TEXT SPEED 0..7 of 3, FRAME 0..31 of 10, BUTTON
 * MODE 0..255 of 3): such a value is shown as "?" and left in the save; a step replaces it. */
void CtrOptions_Step(int row, int dir)
{
    const CtrOptionRow *r = CtrOptions_Row(row);
    int count = CtrOptions_Count(row), value;

    if (r == NULL || count <= 0)
        return;
    value = CtrOptions_Get(row);
    if (value < 0 || value >= count)
        value = dir < 0 ? count - 1 : 0;
    else
        value = (value + (dir < 0 ? count - 1 : 1)) % count;
    if (r->set != NULL)
        r->set(value);
    else
        CtrPokeTouch_OptionSet(r->game, value);
}

int CtrOptions_IsLevels(int row)
{
    const CtrOptionRow *r = CtrOptions_Row(row);

    return r != NULL && r->names == sEmbers;
}

void CtrOptions_Set(int row, int value)
{
    const CtrOptionRow *r = CtrOptions_Row(row);

    if (r == NULL || value < 0 || value >= CtrOptions_Count(row))
        return;
    if (r->set != NULL)
        r->set(value);
    else
        CtrPokeTouch_OptionSet(r->game, value);
}

void CtrOptions_ValueText(int row, char *out, int size)
{
    const CtrOptionRow *r = CtrOptions_Row(row);
    int value;

    if (out == NULL || size <= 0)
        return;
    out[0] = 0;
    if (r == NULL)
        return;
    value = CtrOptions_Get(row);
    if (value < 0 || value >= CtrOptions_Count(row))
        snprintf(out, (size_t)size, "?");
    else if (r->names != NULL)
        snprintf(out, (size_t)size, "%s", r->names[value]);
    else
        snprintf(out, (size_t)size, "%s%d", r->prefix != NULL ? r->prefix : "", value + 1);
}

void CtrOptions_Fill(CtrPokeTouchView *v, int selected)
{
    int page, base, count;

    if (v == NULL)
        return;
    if (selected < 0 || selected >= ROWS)
        selected = 0;
    page = selected / PT_OPT_MAX;
    base = page * PT_OPT_MAX;
    count = ROWS - base > PT_OPT_MAX ? PT_OPT_MAX : ROWS - base;
    v->options = 1;
    v->optPage = (uint8_t)page;
    v->optPageCount = (uint8_t)((ROWS + PT_OPT_MAX - 1) / PT_OPT_MAX);
    v->optCount = (uint8_t)count;
    v->optSelected = (uint8_t)(selected - base);
    for (int i = 0; i < PT_OPT_MAX; ++i) {
        if (i < count) {
            snprintf(v->optLabel[i], PT_OPT_LABEL_LEN, "%s", sRows[base + i].label);
            CtrOptions_ValueText(base + i, v->optValue[i], PT_OPT_VALUE_LEN);
        } else {
            v->optLabel[i][0] = 0;
            v->optValue[i][0] = 0;
        }
        v->optSegCount[i] = 0;
        if (i < count && CtrOptions_IsLevels(base + i)) {
            int n = CtrOptions_Count(base + i), value = CtrOptions_Get(base + i);

            n = n > PT_OPT_SEGS ? PT_OPT_SEGS : n;
            v->optSegCount[i] = (uint8_t)n;
            v->optSegValue[i] = (uint8_t)(value >= 0 && value < n ? value : n);   /* n: none lit */
            for (int k = 0; k < n; ++k)
                snprintf(v->optSegName[i][k], PT_OPT_SEG_LEN, "%s", sRows[base + i].names[k]);
        }
    }
    snprintf(v->bar[0], PT_BAR_LEN, "%s", sRows[selected].label);
    snprintf(v->bar[1], PT_BAR_LEN, "%s", sRows[selected].description[0]);
    snprintf(v->bar[2], PT_BAR_LEN, "%s", sRows[selected].description[1]);
}
