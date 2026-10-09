/* The native OPTIONS screen's rows (see 3ds_poketouch_options.h). */
#include <stdio.h>
#include <string.h>

#include "3ds_poketouch_options.h"

static const char *const sTextSpeed[] = {"SLOW", "MID", "FAST"};
static const char *const sBattleScene[] = {"ON", "OFF"};
static const char *const sBattleStyle[] = {"SHIFT", "SET"};
static const char *const sSound[] = {"MONO", "STEREO"};
static const char *const sButtonMode[] = {"HELP", "LR", "L=A"};
static const char *const sFps[] = {"ON", "OFF"};

/* FireRed's menu order. The names are the ones of its own strings (strings.c). */
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
};

#define ROWS ((int)(sizeof(sRows) / sizeof(sRows[0])))
_Static_assert(sizeof(sRows) / sizeof(sRows[0]) <= PT_OPT_MAX, "the OPTIONS screen holds PT_OPT_MAX rows");

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
    if (v == NULL)
        return;
    v->options = 1;
    v->optCount = (uint8_t)ROWS;
    if (selected < 0 || selected >= ROWS)
        selected = 0;
    v->optSelected = (uint8_t)selected;
    for (int i = 0; i < ROWS; ++i) {
        snprintf(v->optLabel[i], PT_OPT_LABEL_LEN, "%s", sRows[i].label);
        CtrOptions_ValueText(i, v->optValue[i], PT_OPT_VALUE_LEN);
    }
    snprintf(v->bar[0], PT_BAR_LEN, "%s", sRows[selected].label);
    snprintf(v->bar[1], PT_BAR_LEN, "%s", sRows[selected].description[0]);
    snprintf(v->bar[2], PT_BAR_LEN, "%s", sRows[selected].description[1]);
}
