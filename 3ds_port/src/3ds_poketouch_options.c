/* The native OPTIONS screen's rows (see 3ds_poketouch_options.h). */
#include <stdio.h>
#include <string.h>

#include "3ds_poketouch_options.h"
#include "3ds_looks.h"
#include "3ds_settings.h"
#include "3ds_lang.h"

static const char *const sTextSpeed[] = {CTR_TR("Slow", "Lenta", "Langsam"), CTR_TR("Mid", "Media", "Mittel"), CTR_TR("Fast", "R\xC3\xA1pida", "Schnell")};
static const char *const sBattleScene[] = {CTR_TR("On", "S\xC3\xAD", "Ein"), CTR_TR("Off", "No", "Aus")};
static const char *const sBattleStyle[] = {CTR_TR("Shift", "Cambio", "Wechsel"), CTR_TR("Set", "Fijo", "Folge")};
static const char *const sSound[] = {CTR_TR("Mono", "Mono", "Mono"), CTR_TR("Stereo", "Est\xC3\xA9reo", "Stereo")};
static const char *const sButtonMode[] = {CTR_TR("Help", "Ayuda", "Hilfe"), "LR", "L=A"};
static const char *const sFps[] = {CTR_TR("On", "S\xC3\xAD", "Ein"), CTR_TR("Off", "No", "Aus")};
static const char *const sDayNight[] = {CTR_TR("Clock", "Reloj", "Uhrzeit"), CTR_TR("Fixed", "Fija", "Fest"), CTR_TR("Off", "Off", "Aus")};
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
static const char *const sEmbers[] = {CTR_TR("Off", "No", "Aus"), CTR_TR("Low", "Bajo", "Wenig"), CTR_TR("High", "Alto", "Viel")};     /* CTR_EMBERS_* */
/* EXP SHARE: who gets experience (HALF: the whole team shares as if each held an EXP. SHARE; FULL: every Pokemon that can
 * still fight gets the share of one that fought). */
static const char *const sExpShare[] = {CTR_TR("Off", "No", "Aus"), CTR_TR("Half", "Mitad", "Halb"), CTR_TR("Full", "Toda", "Voll")};
/* COMPANION: the first Pokemon walks behind the player (FOLLOW), or the player is that Pokemon (BE IT). */
static const char *const sCompanion[] = {CTR_TR("Off", "No", "Aus"), CTR_TR("Follow", "Sigue", "Folgt"), CTR_TR("Be it", "Ser", "Selbst")};

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

    sLookNames[0] = CTR_TR("Default", "Normal", "Standard");
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
static int GetFreeHm(void) { return !CtrSettings_FreeHm(); }
static void SetFreeHm(int v) { CtrSettings_SetFreeHm(!(v & 1)); }
static int GetRandomlocke(void) { return !CtrSettings_Randomlocke(); }
static void SetRandomlocke(int v) { CtrSettings_SetRandomlocke(!(v & 1)); }
static int GetNuzlocke(void) { return !CtrSettings_Nuzlocke(); }
static void SetNuzlocke(int v) { CtrSettings_SetNuzlocke(!(v & 1)); }
static int GetNuzDupes(void) { return !CtrSettings_NuzDuplicates(); }
static void SetNuzDupes(int v) { CtrSettings_SetNuzDuplicates(!(v & 1)); }
static int GetNuzShiny(void) { return !CtrSettings_NuzShiny(); }
static void SetNuzShiny(int v) { CtrSettings_SetNuzShiny(!(v & 1)); }
static int GetNuzCap(void) { return !CtrSettings_NuzLevelCap(); }
static void SetNuzCap(int v) { CtrSettings_SetNuzLevelCap(!(v & 1)); }
static int GetVisibleWilds(void) { return !CtrSettings_VisibleWilds(); }
static void SetVisibleWilds(int v) { CtrSettings_SetVisibleWilds(!(v & 1)); }
static int GetTmReuse(void) { return !CtrSettings_TmReuse(); }
static void SetTmReuse(int v) { CtrSettings_SetTmReuse(!(v & 1)); }
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
int CtrOptions_ReusableTm(void) { return CtrSettings_TmReuse(); }
int CtrOptions_Nuzlocke(void) { return CtrSettings_Nuzlocke(); }
int CtrOptions_NuzDuplicates(void) { return CtrSettings_NuzDuplicates(); }
int CtrOptions_NuzShiny(void) { return CtrSettings_NuzShiny(); }
int CtrOptions_VisibleWilds(void) { return CtrSettings_VisibleWilds(); }
int CtrOptions_NuzLevelCap(void) { return CtrSettings_NuzLevelCap(); }
int CtrOptions_Randomlocke(void) { return CtrSettings_Randomlocke(); }
int CtrOptions_FreeHm(void) { return CtrSettings_FreeHm(); }

unsigned CtrOptions_RandomSpecies(unsigned seed, unsigned place, unsigned species)
{
    uint32_t h = (uint32_t)seed ^ ((uint32_t)place * 0x9E3779B1u) ^ ((uint32_t)species * 0x85EBCA6Bu);

    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    h *= 0x846CA68Bu;
    h ^= h >> 16;
    h %= 386;                                   /* 251 of Kanto and Johto (1-251), 135 of Hoenn (277-411) */
    return h < 251 ? h + 1 : h - 251 + 277;
}

void CtrOptions_EmbersInit(int isNew3ds)
{
    int stored = CtrSettings_Embers();

    sPortEmbers = stored >= 0 && stored < CTR_EMBERS_COUNT ? stored : isNew3ds ? CTR_EMBERS_HIGH : CTR_EMBERS_LOW;
}

/* FireRed's menu order (Page 1), followed by 3DS port QoL options (Page 2). */
static const CtrOptionRow sRows[] = {
    {CTR_TR("Text speed", "Vel. texto", "Texttempo"), {CTR_TR("How fast dialogue", "La velocidad a la que", "Geschwindigkeit der"), CTR_TR("text is printed.", "aparece el texto.", "Textanzeige.")}, PT_OPT_TEXT_SPEED, sTextSpeed, NULL, NULL, NULL, NULL},
    {CTR_TR("Battle scene", "Animaci\xC3\xB3n", "Kampfanimation"), {CTR_TR("Show or skip the move", "Ver o saltar las", "Kampfanimationen im Kampf"), CTR_TR("animations in battle.", "animaciones de ataques.", "anzeigen oder nicht.")}, PT_OPT_BATTLE_SCENE, sBattleScene, NULL,
     NULL, NULL, NULL},
    {CTR_TR("Battle style", "Estilo combate", "Kampfstil"), {CTR_TR("Shift offers a switch", "Cambio te deja cambiar", "Wechsel erlaubt Tausch"), CTR_TR("after a foe faints.", "si el rival se debilita.", "nach besiegtem Gegner.")}, PT_OPT_BATTLE_STYLE, sBattleStyle, NULL,
     NULL, NULL, NULL},
    {CTR_TR("Sound", "Sonido", "Ton"), {CTR_TR("Mono or stereo sound.", "Sonido mono o est\xC3\xA9reo.", "Mono- oder Stereo-Klang."), ""}, PT_OPT_SOUND, sSound, NULL, NULL, NULL, NULL},
    {CTR_TR("Button mode", "Modo botones", "Tastenmodus"), {CTR_TR("What the L and R buttons", "Qu\xC3\xA9 hacen L y R:", "Funktion von L und R:"), CTR_TR("do: Help, LR or L=A.", "Ayuda, LR o L=A.", "Hilfe, LR oder L=A.")}, PT_OPT_BUTTON_MODE, sButtonMode, NULL,
     NULL, NULL, NULL},
    {CTR_TR("Frame", "Marco", "Fenster"), {CTR_TR("Menu and Yes/No", "Marco de men\xC3\xBAs", "Rahmen der"), CTR_TR("box frame.", "y cajas S\xC3\xAD/No.", "Men\xC3\xBCs, JA/NEIN.")}, PT_OPT_FRAME, NULL, CTR_TR("Type", "Tipo", "Typ"), NULL, NULL, NULL},
    /* The 3DS's own: the counter in the top-right corner of the top screen (stored in the save's spare bit). */
    {CTR_TR("FPS counter", "Contador FPS", "FPS-Anzeige"), {CTR_TR("Show the frames per", "Muestra las im\xC3\xA1genes por", "Bilder pro Sekunde"), CTR_TR("second on the top screen.", "segundo arriba.", "oben anzeigen.")}, PT_OPT_FPS, sFps, NULL, NULL, NULL, NULL},
    /* Page 2: 3DS Port QoL options. */
    {CTR_TR("Type matchup", "Eficacia tipos", "Typ-Effekt."), {CTR_TR("Show type effectiveness", "Muestra la eficacia de", "Typenvor-/nachteile bei"), CTR_TR("on battle move cards.", "tipos en los ataques.", "Attacken im Kampf zeigen.")}, 0, sFps, NULL,
     GetTypeMatchup, TwoCount, SetTypeMatchup},
    {CTR_TR("Move details", "Datos ataques", "Attacken-Infos"), {CTR_TR("Show live move details", "Muestra datos y clase", "Werte und Schadensklasse"), CTR_TR("and category in battle.", "de los ataques.", "der Attacken anzeigen.")}, 0, sFps, NULL,
     GetMoveDetails, TwoCount, SetMoveDetails},
    {CTR_TR("Day/Night", "D\xC3\xAD" "a/Noche", "Tag/Nacht"), {CTR_TR("The 3DS clock's hour, the", "Hora de la 3DS, la Hora", "3DS-Uhrzeit, die Stunde"), CTR_TR("Hour below, or Off.", "de abajo, u Off.", "unten, oder Aus.")}, 0, sDayNight, NULL,
     GetDayNight, DayNightCount, SetDayNight},
    {CTR_TR("Hour", "Hora", "Stunde"), {CTR_TR("The hour outdoors when", "La hora en el exterior si", "Uhrzeit in der Oberwelt,"), CTR_TR("Day/Night is Fixed.", "D\xC3\xAD" "a/Noche est\xC3\xA1 en Fija.", "wenn TAG/NACHT FEST ist.")}, 0, sHourName, NULL,
     GetHour, HourCount, SetHour},
    {CTR_TR("Embers", "Ascuas", "Funken"), {CTR_TR("Sparks behind the menus.", "Chispas que suben tras los", "Funken hinter den Men\xC3\xBCs."), CTR_TR("Low suits an Old 3DS.", "men\xC3\xBAs. Bajo para Old 3DS.", "Wenig passt f\xC3\xBCr Old 3DS.")}, 0, sEmbers, NULL,
     GetEmbers, ThreeCount, SetEmbers},
    {CTR_TR("Repel again", "Repelente", "Schutz erneut"), {CTR_TR("When a repel wears off,", "Al acabarse un repelente,", "Wenn ein Schutz endet,"), CTR_TR("offer to use another.", "pregunta si usar otro.", "einen weiteren anbieten.")}, 0, sFps, NULL,
     GetRepelAgain, TwoCount, SetRepelAgain},
    {CTR_TR("Companion", "Acompa\xC3\xB1" "ante", "Begleiter"), {CTR_TR("A Pokemon follows you,", "Un Pok\xC3\xA9mon te sigue,", "Ein Pok\xC3\xA9mon folgt dir,"), CTR_TR("or you are that Pokemon.", "o t\xC3\xBA eres ese Pok\xC3\xA9mon.", "oder du bist das Pok\xC3\xA9mon.")}, 0, sCompanion, NULL,
     GetCompanion, ThreeCount, SetCompanion},
    /* Page 3. */
    {CTR_TR("EXP share", "Repartir EXP", "EP-Teiler"), {CTR_TR("Share EXP with the whole", "Reparte la EXP con todo", "EP an das ganze Team:"), CTR_TR("team: Half or Full.", "el equipo: Mitad o Toda.", "Halb oder Voll.")}, 0, sExpShare, NULL,
     GetExpShare, ThreeCount, SetExpShare},
    {CTR_TR("Reusable TMs", "MT infinitas", "TMs mehrfach"), {CTR_TR("A TM is not used up when", "Una MT no se gasta al", "Eine TM wird beim Lehren"), CTR_TR("it teaches a move.", "ense\xC3\xB1" "ar un ataque.", "nicht verbraucht.")}, 0, sFps, NULL,
     GetTmReuse, TwoCount, SetTmReuse},
    {CTR_TR("Nuzlocke", "Nuzlocke", "Nuzlocke"), {CTR_TR("One wild catch per area;", "Un salvaje por zona;", "Ein Fang pro Gebiet;"), CTR_TR("fainted ones are released.", "los debilitados se liberan.", "Besiegte werden entlassen.")}, 0, sFps, NULL,
     GetNuzlocke, TwoCount, SetNuzlocke},
    {CTR_TR("Randomlocke", "Randomlocke", "Randomlocke"), {CTR_TR("Wild and trainer Pokemon", "Salvajes y entrenadores", "Wilde und Trainer-Pok\xC3\xA9mon"), CTR_TR("are random species.", "con especies al azar.", "mit zuf\xC3\xA4lligen Arten.")}, 0, sFps, NULL,
     GetRandomlocke, TwoCount, SetRandomlocke},
    {CTR_TR("Free HMs", "MO libres", "VMs frei"), {CTR_TR("An HM in the Bag works in", "Una MO de la Mochila sirve", "Eine VM im Beutel wirkt im"), CTR_TR("the field: no move needed.", "en el campo sin ense\xC3\xB1" "arla.", "Feld, ohne sie zu lehren.")}, 0, sFps, NULL,
     GetFreeHm, TwoCount, SetFreeHm},
    {CTR_TR("Red look", "Aspecto rojo", "Rot-Look"), {CTR_TR("The look of Red, from the", "El aspecto de Rojo, de", "Das Aussehen von Rot, aus"), CTR_TR("alternative looks.", "los aspectos del Builder.", "den Builder-Looks.")}, 0, sLookNames, NULL,
     GetLookRed, LookCount, SetLookRed},
    {CTR_TR("Leaf look", "Aspecto hoja", "Blatt-Look"), {CTR_TR("The look of Leaf, from the", "El aspecto de Hoja, de", "Aussehen von Blatt, aus"), CTR_TR("alternative looks.", "los aspectos del Builder.", "den Builder-Looks.")}, 0, sLookNames, NULL,
     GetLookLeaf, LookCount, SetLookLeaf},
    {CTR_TR("Rival look", "Aspecto rival", "Rivalen-Look"), {CTR_TR("Blue's look, from the", "El aspecto de Azul, de", "Blaus Aussehen, aus"), CTR_TR("alternative looks.", "los aspectos del Builder.", "aus den Builder-Looks.")}, 0, sLookNames, NULL,
     GetLookBlue, LookCount, SetLookBlue},
    /* Page 5: the optional rules of the NUZLOCKE row (they count only while it is on). */
    {CTR_TR("Duplicates", "Repetidos", "Duplikate"), {CTR_TR("A species you already caught", "Una especie ya capturada", "Eine schon gefangene Art"), CTR_TR("does not use the area.", "no gasta la zona.", "z\xC3\xA4hlt nicht f\xC3\xBCr das Gebiet.")}, 0, sFps, NULL,
     GetNuzDupes, TwoCount, SetNuzDupes},
    {CTR_TR("Shiny clause", "Cl\xC3\xA1usula shiny", "Shiny-Regel"), {CTR_TR("A shiny Pokemon does not", "Un Pok\xC3\xA9mon shiny no gasta", "Ein Shiny-Pok\xC3\xA9mon z\xC3\xA4hlt"), CTR_TR("use up the area.", "la zona.", "nicht f\xC3\xBCr das Gebiet.")}, 0, sFps, NULL,
     GetNuzShiny, TwoCount, SetNuzShiny},
    {CTR_TR("Level caps", "Nivel m\xC3\xA1ximo", "Level-Limit"), {CTR_TR("No EXP at or above the", "Sin EXP al llegar al nivel", "Keine EP ab dem Level-Limit"), CTR_TR("next Gym Leader's level.", "del pr\xC3\xB3ximo L\xC3\xAD" "der.", "des n\xC3\xA4" "chsten Arenaleiters.")}, 0, sFps, NULL,
     GetNuzCap, TwoCount, SetNuzCap},
    /* Page 6: the experiments. */
    {CTR_TR("Visible wilds", "Salvajes visibles", "Sichtbare Wilde"), {CTR_TR("Route 1 only: wild Pokemon", "Solo Ruta 1: los Pok\xC3\xA9mon", "Nur Route 1: wilde Pok\xC3\xA9mon"), CTR_TR("walk in the grass; touch one.", "salvajes andan; t\xC3\xB3" "calos.", "laufen im Gras; ber\xC3\xBChre eins.")}, 0, sFps, NULL,
     GetVisibleWilds, TwoCount, SetVisibleWilds},
};

#define ROWS ((int)(sizeof(sRows) / sizeof(sRows[0])))

/* The pages: the first row of each, then the end. Page 3 (the game's rules) and page 4 (the looks) are short on purpose: the look
 * preview is a card in the rows a page does not use. */
enum { PAGE1 = 0, PAGE2 = 7, PAGE3 = 14, PAGE4 = 19, PAGE5 = 22, PAGE6 = 25 };
static const uint8_t sPageStart[] = {PAGE1, PAGE2, PAGE3, PAGE4, PAGE5, PAGE6, ROWS};
#define PAGES ((int)(sizeof(sPageStart) / sizeof(sPageStart[0])) - 1)
_Static_assert(PAGE6 < ROWS, "the last page has rows");
_Static_assert(PAGE2 - PAGE1 <= PT_OPT_MAX && PAGE3 - PAGE2 <= PT_OPT_MAX && PAGE4 - PAGE3 <= PT_OPT_MAX && PAGE5 - PAGE4 <= PT_OPT_MAX && PAGE6 - PAGE5 <= PT_OPT_MAX && ROWS - PAGE6 <= PT_OPT_MAX,
               "a page holds up to PT_OPT_MAX rows");

int CtrOptions_PageCount(void) { return PAGES; }

const char *CtrOptions_PageName(int page)
{
    static const char *const names[] = {CTR_TR("Game", "Juego", "Spiel"), CTR_TR("Extras", "Extras", "Extras"),
                                        CTR_TR("Rules", "Reglas", "Regeln"), CTR_TR("Looks", "Aspectos", "Looks"),
                                        CTR_TR("Nuzlocke", "Nuzlocke", "Nuzlocke"), CTR_TR("Wilds", "Salvajes", "Wilde")};

    _Static_assert(sizeof(names) / sizeof(names[0]) == PAGES, "one name for each page");
    return page >= 0 && page < PAGES ? names[page] : "";
}

int CtrOptions_PageOf(int row)
{
    int page = 0;

    while (page + 1 < PAGES && row >= sPageStart[page + 1])
        ++page;
    return page;
}

int CtrOptions_PageBase(int page)
{
    return page < 0 || page >= PAGES ? 0 : sPageStart[page];
}

int CtrOptions_PageRows(int page)
{
    return page < 0 || page >= PAGES ? 0 : sPageStart[page + 1] - sPageStart[page];
}

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
    text = index >= 0 ? CtrLooks_Name(index) : CTR_TR("Default", "Normal", "Standard");
    snprintf(l->name, sizeof(l->name), "%s", text ? text : "");
    if (index < 0)
    {
        snprintf(l->credit, sizeof(l->credit), "%s", CTR_TR("The look the game had before.", "El aspecto que ten\xC3\xAD" "a el juego.", "Das Aussehen, das das Spiel hatte."));
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
    page = CtrOptions_PageOf(selected);
    base = CtrOptions_PageBase(page);
    count = CtrOptions_PageRows(page);
    v->options = 1;
    v->optPage = (uint8_t)page;
    v->optPageCount = (uint8_t)PAGES;
    snprintf(v->optPageName, sizeof(v->optPageName), "%s", CtrOptions_PageName(page));
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
