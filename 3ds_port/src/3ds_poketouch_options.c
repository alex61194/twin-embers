/* The native OPTIONS screen's rows (see 3ds_poketouch_options.h). */
#include <stdio.h>
#include <string.h>

#include "3ds_poketouch_options.h"
#include "3ds_looks.h"
#include "3ds_settings.h"
#include "3ds_lang.h"

static const char *const sTextSpeed[] = {CTR_TR("SLOW", "LENTA", "Langsam"), CTR_TR("MID", "MEDIA", "Mittel"), CTR_TR("FAST", "R\xC3\x81PIDA", "Schnell")};
static const char *const sBattleScene[] = {CTR_TR("ON", "S\xC3\x8D", "Ein"), CTR_TR("OFF", "NO", "Aus")};
static const char *const sBattleStyle[] = {CTR_TR("SHIFT", "CAMBIO", "Wechsel"), CTR_TR("SET", "FIJO", "Folge")};
static const char *const sSound[] = {CTR_TR("MONO", "MONO", "Mono"), CTR_TR("STEREO", "EST\xC3\x89REO", "Stereo")};
static const char *const sButtonMode[] = {CTR_TR("HELP", "AYUDA", "Hilfe"), "LR", "L=A"};
static const char *const sFps[] = {CTR_TR("ON", "S\xC3\x8D", "Ein"), CTR_TR("OFF", "NO", "Aus")};
static const char *const sDayNight[] = {CTR_TR("CLOCK", "RELOJ", "Uhrzeit"), CTR_TR("FIXED", "FIJA", "Fest"), CTR_TR("OFF", "OFF", "Aus")};
enum { DAY_NIGHT_CLOCK, DAY_NIGHT_FIXED, DAY_NIGHT_OFF, DAY_NIGHT_COUNT };
/* FIXED's hour, picked on the HOUR row: any half hour of the 24, 00:00 to 23:30. */
#define HOUR_STEP_SECONDS 1800
#define HOUR_COUNT (86400 / HOUR_STEP_SECONDS)
#define HOUR_DEFAULT 24                     /* 12:00 */
static const char *const sHourName[HOUR_COUNT] = {
    "00:00", "00:30", "01:00", "01:30", "02:00", "02:30",
    "03:00", "03:30", "04:00", "04:30", "05:00", "05:30",
    "06:00", "06:30", "07:00", "07:30", "08:00", "08:30",
    "09:00", "09:30", "10:00", "10:30", "11:00", "11:30",
    "12:00", "12:30", "13:00", "13:30", "14:00", "14:30",
    "15:00", "15:30", "16:00", "16:30", "17:00", "17:30",
    "18:00", "18:30", "19:00", "19:30", "20:00", "20:30",
    "21:00", "21:30", "22:00", "22:30", "23:00", "23:30",
};
static const char *const sEmbers[] = {CTR_TR("OFF", "NO", "Aus"), CTR_TR("LOW", "BAJO", "Wenig"), CTR_TR("HIGH", "ALTO", "Viel")};     /* CTR_EMBERS_* */
/* EXP SHARE: who gets experience (HALF: the whole team shares as if each held an EXP. SHARE; FULL: every Pokemon that can
 * still fight gets the share of one that fought). */
static const char *const sExpShare[] = {CTR_TR("OFF", "NO", "Aus"), CTR_TR("HALF", "MITAD", "Halb"), CTR_TR("FULL", "TODA", "Voll")};
/* COMPANION: the first Pokemon walks behind the player (FOLLOW), or the player is that Pokemon (BE IT). */
static const char *const sCompanion[] = {CTR_TR("OFF", "NO", "Aus"), CTR_TR("FOLLOW", "SIGUE", "Folgt"), CTR_TR("BE IT", "SER", "Spielen")};

static int sPortTypeMatchup;
static int sPortMoveDetails;
static int sPortDayNight;
static int sPortHour = HOUR_DEFAULT;
static int sPortEmbers = CTR_EMBERS_HIGH;

static int TwoCount(void) { return 2; }
static int GetTypeMatchup(void) { return sPortTypeMatchup; }
static void SetTypeMatchup(int v) { sPortTypeMatchup = v & 1; }
static int GetMoveDetails(void) { return sPortMoveDetails; }
static void SetMoveDetails(int v) { sPortMoveDetails = v & 1; }
static int GetDayNight(void) { return sPortDayNight; }
static int DayNightCount(void) { return DAY_NIGHT_COUNT; }
static void SetDayNight(int v) { sPortDayNight = v < 0 || v >= DAY_NIGHT_COUNT ? DAY_NIGHT_CLOCK : v; }
static int GetHour(void) { return sPortHour; }
static int HourCount(void) { return HOUR_COUNT; }
/* Choosing an hour is choosing to use it: DAY/NIGHT moves to FIXED unless it is OFF. */
static void SetHour(int v)
{
    sPortHour = v < 0 || v >= HOUR_COUNT ? HOUR_DEFAULT : v;
    if (sPortDayNight == DAY_NIGHT_CLOCK)
        sPortDayNight = DAY_NIGHT_FIXED;
}
static int ThreeCount(void) { return 3; }
/* 0 is ON, as in the other switches' tables; kept in settings.cfg. */
static int GetRepelAgain(void) { return !CtrSettings_RepelAgain(); }
static void SetRepelAgain(int v) { CtrSettings_SetRepelAgain(!(v & 1)); }
static int GetCompanion(void) { return CtrSettings_Companion(); }
static void SetCompanion(int v) { CtrSettings_SetCompanion(v); }
/* The look of Red, Leaf and the rival Blue (the Builder's alternative looks): DEFAULT, then each look the pack has. */
static const char *sLookNames[1 + CTR_LOOKS_MAX];
static void LookNamesFill(void)
{
    int n = CtrLooks_Count();

    sLookNames[0] = CTR_TR("DEFAULT", "NORMAL", "Standard");
    for (int i = 0; i < n; ++i)
        sLookNames[1 + i] = CtrLooks_Name(i);
}
static int LookCount(void) { LookNamesFill(); return 1 + CtrLooks_Count(); }
static int GetLookRed(void) { LookNamesFill(); return CtrLooks_Chosen(CTR_LOOK_RED) + 1; }
static int GetLookLeaf(void) { LookNamesFill(); return CtrLooks_Chosen(CTR_LOOK_LEAF) + 1; }
static int GetLookBlue(void) { LookNamesFill(); return CtrLooks_Chosen(CTR_LOOK_BLUE) + 1; }
static void SetLookRed(int v) { CtrLooks_Choose(CTR_LOOK_RED, v - 1); }
static void SetLookLeaf(int v) { CtrLooks_Choose(CTR_LOOK_LEAF, v - 1); }
static void SetLookBlue(int v) { CtrLooks_Choose(CTR_LOOK_BLUE, v - 1); }
static int GetExpShare(void) { return CtrSettings_ExpShare(); }
static void SetExpShare(int v) { CtrSettings_SetExpShare(v); }
static int GetEmbers(void) { return sPortEmbers; }
static void SetEmbers(int v)
{
    sPortEmbers = v < 0 || v >= CTR_EMBERS_COUNT ? CTR_EMBERS_HIGH : v;
    CtrSettings_SetEmbers(sPortEmbers);     /* kept for the next session; the file is written only on a change */
}

int CtrOptions_TypeMatchup(void) { return sPortTypeMatchup == 0; }
int CtrOptions_MoveDetails(void) { return sPortMoveDetails == 0; }
int CtrOptions_DayNight(void) { return sPortDayNight != DAY_NIGHT_OFF; }
int CtrOptions_DayNightSecond(void) { return sPortDayNight == DAY_NIGHT_FIXED ? sPortHour * HOUR_STEP_SECONDS : -1; }
int CtrOptions_Embers(void) { return sPortEmbers != CTR_EMBERS_OFF; }
int CtrOptions_EmbersLow(void) { return sPortEmbers == CTR_EMBERS_LOW; }
int CtrOptions_RepelAgain(void) { return CtrSettings_RepelAgain(); }
int CtrOptions_Companion(void) { return CtrSettings_Companion(); }
int CtrOptions_ExpShare(void) { return CtrSettings_ExpShare(); }

void CtrOptions_EmbersInit(int isNew3ds)
{
    int stored = CtrSettings_Embers();

    sPortEmbers = stored >= 0 && stored < CTR_EMBERS_COUNT ? stored : isNew3ds ? CTR_EMBERS_HIGH : CTR_EMBERS_LOW;
}

/* FireRed's menu order (Page 1), followed by 3DS port QoL options (Page 2). */
static const CtrOptionRow sRows[] = {
    {CTR_TR("TEXT SPEED", "VEL. TEXTO", "Texttempo"), {CTR_TR("How fast dialogue", "La velocidad a la que", "Geschwindigkeit der"), CTR_TR("text is printed.", "aparece el texto.", "Textanzeige.")}, PT_OPT_TEXT_SPEED, sTextSpeed, NULL, NULL, NULL, NULL},
    {CTR_TR("BATTLE SCENE", "ANIMACI\xC3\x93N", "Kampfanimation"), {CTR_TR("Show or skip the move", "Ver o saltar las", "Kampfanimationen im Kampf"), CTR_TR("animations in battle.", "animaciones de ataques.", "anzeigen oder nicht.")}, PT_OPT_BATTLE_SCENE, sBattleScene, NULL,
     NULL, NULL, NULL},
    {CTR_TR("BATTLE STYLE", "ESTILO COMBATE", "Kampfstil"), {CTR_TR("SHIFT offers a switch", "CAMBIO te deja cambiar", "Wechsel erlaubt Tausch"), CTR_TR("after a foe faints.", "si el rival se debilita.", "nach besiegtem Gegner.")}, PT_OPT_BATTLE_STYLE, sBattleStyle, NULL,
     NULL, NULL, NULL},
    {CTR_TR("SOUND", "SONIDO", "Ton"), {CTR_TR("Mono or stereo sound.", "Sonido mono o est\xC3\xA9reo.", "Mono- oder Stereo-Klang."), ""}, PT_OPT_SOUND, sSound, NULL, NULL, NULL, NULL},
    {CTR_TR("BUTTON MODE", "MODO BOTONES", "Tastenmodus"), {CTR_TR("What the L and R buttons", "Qu\xC3\xA9 hacen L y R:", "Funktion der L- und R-Tasten:"), CTR_TR("do: HELP, LR or L=A.", "AYUDA, LR o L=A.", "Hilfe, LR oder L=A.")}, PT_OPT_BUTTON_MODE, sButtonMode, NULL,
     NULL, NULL, NULL},
    {CTR_TR("FRAME", "MARCO", "Fenster"), {CTR_TR("The frame of menu", "El marco de los men\xC3\xBAs", "Das Design der Men\xC3\xBCs"), CTR_TR("and YES/NO boxes.", "y de las cajas S\xC3\x8D/NO.", "und JA/NEIN-Fenster.")}, PT_OPT_FRAME, NULL, CTR_TR("TYPE", "TIPO", "Typ"), NULL, NULL, NULL},
    /* The 3DS's own: the counter in the top-right corner of the top screen (stored in the save's spare bit). */
    {CTR_TR("FPS COUNTER", "CONTADOR FPS", "FPS-Anzeige"), {CTR_TR("Show the frames per", "Muestra las im\xC3\xA1genes por", "Bilder pro Sekunde"), CTR_TR("second on the top screen.", "segundo arriba.", "auf dem Topscreen anzeigen.")}, PT_OPT_FPS, sFps, NULL, NULL, NULL, NULL},
    /* Page 2: 3DS Port QoL options. */
    {CTR_TR("TYPE MATCHUP", "EFICACIA TIPOS", "Typ-Effekt."), {CTR_TR("Show type effectiveness", "Muestra la eficacia de", "Typenvor-/nachteile bei"), CTR_TR("on battle move cards.", "tipos en los ataques.", "Attacken im Kampf anzeigen.")}, 0, sFps, NULL,
     GetTypeMatchup, TwoCount, SetTypeMatchup},
    {CTR_TR("MOVE DETAILS", "DATOS ATAQUES", "Attacken-Infos"), {CTR_TR("Show live move details", "Muestra datos y clase", "Werte und Schadensklasse"), CTR_TR("and category in battle.", "de los ataques.", "der Attacken anzeigen.")}, 0, sFps, NULL,
     GetMoveDetails, TwoCount, SetMoveDetails},
    {CTR_TR("DAY/NIGHT", "D\xC3\x8D" "A/NOCHE", "Tag/Nacht"), {CTR_TR("The 3DS clock's hour, the", "Hora de la 3DS, la HORA", "3DS-Uhrzeit, die Stunde"), CTR_TR("HOUR below, or OFF.", "de abajo, u OFF.", "unten, oder Aus.")}, 0, sDayNight, NULL,
     GetDayNight, DayNightCount, SetDayNight},
    {CTR_TR("HOUR", "HORA", "Stunde"), {CTR_TR("The hour outdoors when", "La hora en el exterior si", "Uhrzeit in der Oberwelt,"), CTR_TR("DAY/NIGHT is FIXED.", "D\xC3\x8D" "A/NOCHE est\xC3\xA1 en FIJA.", "wenn TAG/NACHT FEST ist.")}, 0, sHourName, NULL,
     GetHour, HourCount, SetHour},
    {CTR_TR("EMBERS", "ASCUAS", "Funken"), {CTR_TR("Sparks rising behind the", "Chispas que suben tras los", "Funken hinter den Men\xC3\xBCs."), CTR_TR("menus. LOW suits an Old 3DS.", "men\xC3\xBAs. BAJO para Old 3DS.", "Wenig eignet sich f\xC3\xBCr Old 3DS.")}, 0, sEmbers, NULL,
     GetEmbers, ThreeCount, SetEmbers},
    {CTR_TR("REPEL AGAIN", "REPELENTE", "Schutz erneut"), {CTR_TR("When a REPEL wears off,", "Al acabarse un REPELENTE,", "Wenn ein Schutz endet,"), CTR_TR("offer to use another.", "pregunta si usar otro.", "einen weiteren anbieten.")}, 0, sFps, NULL,
     GetRepelAgain, TwoCount, SetRepelAgain},
    {CTR_TR("COMPANION", "ACOMPA\xC3\x91" "NTE", "Begleiter"), {CTR_TR("A Pokemon walks behind you,", "Un Pok\xC3\xA9mon te sigue,", "Ein Pok\xC3\xA9mon folgt dir,"), CTR_TR("or you are that Pokemon.", "o t\xC3\xBA eres ese Pok\xC3\xA9mon.", "oder du bist das Pok\xC3\xA9mon.")}, 0, sCompanion, NULL,
     GetCompanion, ThreeCount, SetCompanion},
    /* Page 3. */
    {CTR_TR("EXP SHARE", "REPARTIR EXP", "EP-Teiler"), {CTR_TR("Share EXP with the whole", "Reparte la EXP con todo", "EP an das ganze Team:"), CTR_TR("team: HALF or FULL.", "el equipo: MITAD o TODA.", "Halb oder Voll.")}, 0, sExpShare, NULL,
     GetExpShare, ThreeCount, SetExpShare},
    {CTR_TR("RED LOOK", "ASPECTO ROJO", "Rot-Look"), {CTR_TR("The look of Red, from the", "El aspecto de Rojo, de los", "Das Aussehen von Rot, aus"), CTR_TR("Builder's alternative looks.", "aspectos del Builder.", "den Builder-Looks.")}, 0, sLookNames, NULL,
     GetLookRed, LookCount, SetLookRed},
    {CTR_TR("LEAF LOOK", "ASPECTO HOJA", "Blatt-Look"), {CTR_TR("The look of Leaf, from the", "El aspecto de Hoja, de los", "Das Aussehen von Blatt, aus"), CTR_TR("Builder's alternative looks.", "aspectos del Builder.", "den Builder-Looks.")}, 0, sLookNames, NULL,
     GetLookLeaf, LookCount, SetLookLeaf},
    {CTR_TR("RIVAL LOOK", "ASPECTO RIVAL", "Rivalen-Look"), {CTR_TR("The look of Blue, your rival,", "El aspecto de Azul, tu rival,", "Das Aussehen von Blau, dem Rivalen,"), CTR_TR("from the alternative looks.", "de los aspectos del Builder.", "aus den Builder-Looks.")}, 0, sLookNames, NULL,
     GetLookBlue, LookCount, SetLookBlue},
};

#define ROWS ((int)(sizeof(sRows) / sizeof(sRows[0])))
_Static_assert(sizeof(sRows) / sizeof(sRows[0]) <= PT_OPT_MAX * 3, "the OPTIONS screen holds up to three pages of PT_OPT_MAX rows");

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

    return r != NULL && (r->names == sEmbers || r->names == sCompanion || r->names == sExpShare);
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

/* The preview for the selected look row: the look chosen for it (its name, artists and walking picture), or the default's name. */
static void LookPreview(CtrPokeTouchView *v, int selected)
{
    CtrPokeTouchLook *l = &v->optLook;
    int first = -1, slot, index;
    const char *text;

    for (int i = 0; i < ROWS && first < 0; ++i)    /* the looks' rows: RED LOOK, LEAF LOOK, RIVAL LOOK in a run */
        if (sRows[i].names == sLookNames)
            first = i;
    slot = first >= 0 ? selected - first : -1;

    memset(l, 0, sizeof(*l));
    if (slot < 0 || slot > 2)
        return;
    l->on = 1;
    index = CtrLooks_Chosen(slot);
    text = index >= 0 ? CtrLooks_Name(index) : CTR_TR("DEFAULT", "NORMAL", "Standard");
    snprintf(l->name, sizeof(l->name), "%s", text ? text : "");
    if (index < 0)
    {
        snprintf(l->credit, sizeof(l->credit), "%s", CTR_TR("The look the game had before.", "El aspecto que ten\xC3\xADa el juego.", "Das Aussehen, das das Spiel hatte."));
        return;
    }
    text = CtrLooks_Credit(index);
    snprintf(l->credit, sizeof(l->credit), "%s", text ? text : "");
    l->has = (uint8_t)CtrLooks_Walk(index, l->tiles, l->palette);
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
        v->optSwitch[i] = 0;
        if (i < count && (sRows[base + i].names == sFps || sRows[base + i].names == sBattleScene)) {
            int value = CtrOptions_Get(base + i);   /* 0 is ON in both tables; a bad value stays the text "?" */

            if (value == 0 || value == 1)
                v->optSwitch[i] = (uint8_t)(value == 0 ? 2 : 1);
        }
        if (i < count && CtrOptions_IsLevels(base + i)) {
            int n = CtrOptions_Count(base + i), value = CtrOptions_Get(base + i);

            n = n > PT_OPT_SEGS ? PT_OPT_SEGS : n;
            v->optSegCount[i] = (uint8_t)n;
            v->optSegValue[i] = (uint8_t)(value >= 0 && value < n ? value : n);   /* n: none lit */
            for (int k = 0; k < n; ++k)
                snprintf(v->optSegName[i][k], PT_OPT_SEG_LEN, "%s", sRows[base + i].names[k]);
        }
    }
    v->optFrameShow = 0;
    if (sRows[selected].game == PT_OPT_FRAME && sRows[selected].get == NULL) {
        int type = CtrOptions_Get(selected);

        v->optFrameShow = type >= 0 && type < 10;
        v->optFrameType = (uint8_t)(v->optFrameShow ? type : 0);
    }
    LookPreview(v, selected);
    snprintf(v->bar[0], PT_BAR_LEN, "%s", sRows[selected].label);
    snprintf(v->bar[1], PT_BAR_LEN, "%s", sRows[selected].description[0]);
    snprintf(v->bar[2], PT_BAR_LEN, "%s", sRows[selected].description[1]);
}
