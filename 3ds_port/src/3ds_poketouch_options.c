/* The native OPTIONS screen's rows (see 3ds_poketouch_options.h). */
#ifdef __DEVKITARM__
#include <3ds.h>                                  /* ps:ps, the console's random generator, for NEW */
#endif
#include <stdio.h>
#include <string.h>

#include "3ds_poketouch_options.h"
#include "3ds_looks.h"
#include "3ds_settings.h"
#include "3ds_wilds_core.h"
#include "3ds_lang.h"

static const char *const sTextSpeed[] = {CTR_TR("Slow", "Lenta", "Langsam", "Lent"), CTR_TR("Mid", "Media", "Mittel", "Moyen"), CTR_TR("Fast", "R\xC3\xA1pida", "Schnell", "Rapide")};
static const char *const sBattleScene[] = {CTR_TR("On", "S\xC3\xAD", "Ein", "Oui"), CTR_TR("Off", "No", "Aus", "Non")};
static const char *const sBattleStyle[] = {CTR_TR("Shift", "Cambio", "Wechsel", "\xC3\x89" "change"), CTR_TR("Set", "Fijo", "Folge", "D\xC3\xA9" "fini")};
static const char *const sSound[] = {CTR_TR("Mono", "Mono", "Mono", "Mono"), CTR_TR("Stereo", "Est\xC3\xA9reo", "Stereo", "St\xC3\xA9r\xC3\xA9o")};
static const char *const sButtonMode[] = {CTR_TR("Help", "Ayuda", "Hilfe", "Aide"), "LR", "L=A"};
static const char *const sFps[] = {CTR_TR("On", "S\xC3\xAD", "Ein", "Oui"), CTR_TR("Off", "No", "Aus", "Non")};
static const char *const sDayNight[] = {CTR_TR("Clock", "Reloj", "Uhrzeit", "Horloge"), CTR_TR("Fixed", "Fija", "Fest", "Fixe"), CTR_TR("Off", "Off", "Aus", "Non")};
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
/* EXP SHARE: who gets experience (HALF: the whole team shares as if each held an EXP. SHARE; FULL: every Pokemon that can
 * still fight gets the share of one that fought). */
static const char *const sRandTargetName[] = {CTR_TR("Both", "Ambos", "Beide", "Les deux"), CTR_TR("Wild", "Salvajes", "Wilde", "Sauvages"), CTR_TR("Trainers", "Rivales", "Trainer", "Dresseurs")};
static const char *const sExpShare[] = {CTR_TR("Off", "No", "Aus", "Non"), CTR_TR("Half", "Mitad", "Halb", "Moiti\xC3\xA9"), CTR_TR("Full", "Toda", "Voll", "Total")};
/* COMPANION: the first Pokemon walks behind the player (FOLLOW), or the player is that Pokemon (BE IT). */
/* MULTIPLAYER: online through the relay, or the console's own radio with someone nearby. */
static const char *const sMultiplayer[] = {CTR_TR("Online", "Online", "Online", "En ligne"), CTR_TR("Local", "Local", "Lokal", "Local")};
/* ONLINE ROOM: the relay's public room, or the private room of a six-digit code (typed on the keypad). */
static const char *const sRoomKind[] = {CTR_TR("Public", "P\xC3\xBA" "blica", "\xC3\x96" "ffentlich", "Publique"), CTR_TR("Private", "Privada", "Privat", "Priv\xC3\xA9" "e")};
static const char *const sCompanion[] ={CTR_TR("Off", "No", "Aus", "Non"), CTR_TR("Follow", "Sigue", "Folgt", "\xC3\x8Atre suivi"), CTR_TR("Be it", "Ser", "Selbst", "Incarner")};

/* WILD POKEMON, WILD DISPLAY and AGGRESSIVE (the Wilds page): the visible wild Pokemon of the overworld (3ds_wilds_core.h). */
static const char *const sWildDensity[] = {CTR_TR("Off", "No", "Aus", "Non"), CTR_TR("Few", "Pocos", "Wenig", "Peu"), CTR_TR("Normal", "Normal", "Normal", "Normal"), CTR_TR("Many", "Muchos", "Viele", "Beaucoup")};
static const char *const sWildDisplay[] = {CTR_TR("Pokemon", "Pok\xC3\xA9mon", "Pok\xC3\xA9mon", "Pok\xC3\xA9mon"), CTR_TR("Silhouette", "Silueta", "Schatten", "Silhouette")};
static const char *const sWildAggro[] = {CTR_TR("Off", "No", "Aus", "Non"), CTR_TR("Rare", "Raro", "Selten", "Rare"), CTR_TR("Low", "Bajo", "Wenig", "Faible"), CTR_TR("Moderate", "Medio", "Mittel", "Mod\xC3\xA9r\xC3\xA9"),
                                         CTR_TR("High", "Alto", "Viel", "\xC3\x89lev\xC3\xA9")};

/* TRUCOS: the page of cheats. Each row is a small number kept by CtrSettings_Cheat (off until chosen). */
static const char *const sShinyOdds[] = {CTR_TR("Normal", "Normal", "Normal", "Normal"), "1/4096", "1/1024", "1/256", "1/64", "1/16", CTR_TR("Always", "Siempre", "Immer", "Toujours")};
static const char *const sExpRate[] = {"x1", "x2", "x4", "x8"};
/* The shiny odds a SHINY row value stands for, as the denominator of "1 in N" (the game's own are 8 in 65536, which is 1 in 8192). */
static const unsigned kShinyDenominator[] = {0, 4096, 1024, 256, 64, 16, 1};

static int sPortEmbers = CTR_EMBERS_HIGH;

static int TwoCount(void) { return 2; }
/* TYPE MATCHUP, MOVE DETAILS, DAY/NIGHT and HOUR are kept in settings.cfg as the other port rows are: a player who turned them off or
 * fixed the hour must not find them back at their defaults on the next start. The values are the rows' own (0 is ON, as in the switches). */
static int GetTypeMatchup(void) { return CtrSettings_TypeMatchup(); }
static void SetTypeMatchup(int v) { CtrSettings_SetTypeMatchup(v & 1); }
static int GetMoveDetails(void) { return CtrSettings_MoveDetails(); }
static void SetMoveDetails(int v) { CtrSettings_SetMoveDetails(v & 1); }
static int GetDayNight(void) { return CtrSettings_DayNight(); }
static int DayNightCount(void) { return DAY_NIGHT_COUNT; }
static void SetDayNight(int v) { CtrSettings_SetDayNight(v < 0 || v >= DAY_NIGHT_COUNT ? DAY_NIGHT_CLOCK : v); }
static int GetHour(void) { return CtrSettings_Hour(); }
static int HourCount(void) { return HOUR_COUNT; }
/* Choosing an hour is choosing to use it: DAY/NIGHT moves to FIXED unless it is OFF. */
static void SetHour(int v)
{
    CtrSettings_SetHour(v < 0 || v >= HOUR_COUNT ? HOUR_DEFAULT : v);
    if (CtrSettings_DayNight() == DAY_NIGHT_CLOCK)
        CtrSettings_SetDayNight(DAY_NIGHT_FIXED);
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

    sLookNames[0] = CTR_TR("Default", "Normal", "Standard", "Par d\xC3\xA9" "faut");
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
/* The Pokemon to surf on (the same Builder download): DEFAULT is FireRed's own, then each one the pack has. */
static const char *sSurfNames[1 + CTR_SURFMONS_MAX];
static void SurfNamesFill(void)
{
    int n = CtrSurf_Count();

    /* A pack made without the Builder's surf blobs has none to choose: the row says so instead of looking as if a touch did nothing. */
    sSurfNames[0] = n ? CTR_TR("Default", "Normal", "Standard", "Par d\xC3\xA9" "faut") : CTR_TR("No data", "Sin datos", "Keine Daten", "Aucune donn\xC3\xA9" "e");
    for (int i = 0; i < n; ++i)
        sSurfNames[1 + i] = CtrSurf_Name(i);
}
static int SurfCount(void) { SurfNamesFill(); return 1 + CtrSurf_Count(); }
static int GetSurf(void) { SurfNamesFill(); return CtrSurf_Chosen() + 1; }
static void SetSurf(int v) { CtrSurf_Choose(v - 1); }
static void (*sLocalPlayHook)(int);
void CtrOptions_SetLocalPlayHook(void (*hook)(int local)) { sLocalPlayHook = hook; }
static int GetLocalPlay(void) { return CtrSettings_LocalPlay(); }
static void SetLocalPlay(int v)
{
    CtrSettings_SetLocalPlay(v & 1);
    if (sLocalPlayHook)
        sLocalPlayHook(v & 1);              /* the radio takes it from the next session on */
}
/* The keypad of the private room's code (CtrPokeTouchPad): what is typed and the key under a held touch. */
static struct {
    int on, len, down, key, inside;
    char code[PT_PAD_DIGITS + 1];
} sPad;

static void MultiplayerChanged(void)
{
    if (sLocalPlayHook)
        sLocalPlayHook(CtrSettings_LocalPlay());    /* the radio takes the room (and the radio) from the next session on */
}

static void PadOpen(void)
{
    memset(&sPad, 0, sizeof(sPad));
    sPad.on = 1;
    sPad.key = -1;
}

static int GetRoomKind(void) { return CtrSettings_PrivateRoom(); }
/* PRIVATE with no code yet asks for one first (the keypad; the room is private once it is OK'd); PUBLIC is at once. */
static void SetRoomKind(int v)
{
    if ((v & 1) && !CtrSettings_RoomCode()[0]) {
        PadOpen();
        return;
    }
    CtrSettings_SetPrivateRoom(v & 1);
    if (!(v & 1))
        CtrSettings_AckRoomCodeDropped();       /* PUBLIC chosen on purpose: the note about the dropped code has been read */
    MultiplayerChanged();
}

int CtrOptions_PadOn(void) { return sPad.on; }

/* OPTIONS left without OK or CANCEL (the sidebar, a battle): the keypad must not come back with the old digits. */
void CtrOptions_PadClose(void)
{
    memset(&sPad, 0, sizeof(sPad));
    sPad.key = -1;
}

/* The random bytes of NEW: the console's generator (ps:ps); a test gives its own. None: no code is made (false). */
static int (*sRandomSource)(uint8_t *out, size_t n);

void CtrOptions_SetRandomSource(int (*source)(uint8_t *out, size_t n))
{
    sRandomSource = source;
}

static int RandomBytes(uint8_t *out, size_t n)
{
    if (sRandomSource)
        return sRandomSource(out, n);
#ifdef __DEVKITARM__
    if (R_SUCCEEDED(psInit())) {
        int ok = R_SUCCEEDED(PS_GenerateRandomBytes(out, n));

        psExit();
        return ok;
    }
#endif
    return 0;
}

/* NEW: a code made of random bytes, put in the boxes (a few tries: the bytes can make a code that is refused, as rarely as one in a few hundred). */
static void PadNewCode(void)
{
    for (int tries = 0; tries < 8; ++tries) {
        uint8_t bytes[48];
        char code[PT_PAD_DIGITS + 1];

        if (!RandomBytes(bytes, sizeof(bytes)))
            return;                                 /* no generator: the boxes stay as they were */
        if (CtrSettings_MakeRoomCode(code, bytes, sizeof(bytes))) {
            memcpy(sPad.code, code, sizeof(code));
            sPad.len = PT_PAD_DIGITS;
            memset(bytes, 0, sizeof(bytes));
            return;
        }
    }
}

static void PadKey(int key)
{
    if (key >= 0 && key <= 9 && sPad.len < PT_PAD_DIGITS) {
        sPad.code[sPad.len++] = (char)('0' + key);
        sPad.code[sPad.len] = 0;
    } else if (key == PT_PAD_KEY_DELETE && sPad.len > 0)
        sPad.code[--sPad.len] = 0;
    else if (key == PT_PAD_KEY_NEW)
        PadNewCode();
    else if (key == PT_PAD_KEY_OK && sPad.len == PT_PAD_DIGITS && CtrSettings_RoomCodeOk(sPad.code)) {
        CtrSettings_SetRoomCode(sPad.code);
        CtrSettings_SetPrivateRoom(1);
        sPad.on = 0;
        MultiplayerChanged();
    } else if (key == PT_PAD_KEY_CANCEL)
        sPad.on = 0;
}

void CtrOptions_PadTouch(int touchDown, int touchActive, int key)
{
    if (!sPad.on)
        return;
    if (touchDown) {
        sPad.down = 1;
        sPad.key = key;
        sPad.inside = key >= 0;
        return;
    }
    if (!sPad.down)
        return;
    if (touchActive) {
        sPad.inside = key >= 0 && key == sPad.key;
        return;
    }
    if (sPad.inside)
        PadKey(sPad.key);           /* on release over the key that was touched, as the rows do */
    sPad.down = sPad.inside = 0;
    sPad.key = -1;
}

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
static int GetNuzSet(void) { return !CtrSettings_NuzSet(); }
static void SetNuzSet(int v) { CtrSettings_SetNuzSet(!(v & 1)); }
static int GetRandTarget(void) { return CtrSettings_RandomTarget(); }
static void SetRandTarget(int v) { CtrSettings_SetRandomTarget(v); }
static int GetRandLegend(void) { return CtrSettings_RandomNoLegendary(); }   /* On (allowed) is the first choice */
static void SetRandLegend(int v) { CtrSettings_SetRandomNoLegendary(v & 1); }
static int GetWildDensity(void) { return CtrSettings_WildDensity(); }
static int WildDensityCount(void) { return CTRW_DENSITIES; }
static void SetWildDensity(int v) { CtrSettings_SetWildDensity(v); }
static int GetWildDisplay(void) { return CtrSettings_WildSilhouette(); }
static void SetWildDisplay(int v) { CtrSettings_SetWildSilhouette(v); }
static int GetWildAggro(void) { return CtrSettings_WildAggro(); }
static int WildAggroCount(void) { return CTRW_AGGROS; }
static void SetWildAggro(int v) { CtrSettings_SetWildAggro(v); }
static int CheatCount(int id) { return CtrSettings_CheatMax(id) + 1; }
static int GetCheatCatch(void) { return !CtrSettings_Cheat(CTR_CHEAT_CATCH); }      /* 0 is ON, as in the other switches */
static void SetCheatCatch(int v) { CtrSettings_SetCheat(CTR_CHEAT_CATCH, !(v & 1)); }
static int CheatCatchCount(void) { return CheatCount(CTR_CHEAT_CATCH); }
static int GetCheatShiny(void) { return CtrSettings_Cheat(CTR_CHEAT_SHINY); }
static void SetCheatShiny(int v) { CtrSettings_SetCheat(CTR_CHEAT_SHINY, v); }
static int CheatShinyCount(void) { return CheatCount(CTR_CHEAT_SHINY); }
static int GetCheatNoEnc(void) { return !CtrSettings_Cheat(CTR_CHEAT_NOENC); }      /* 0 is ON, as in the other switches */
static void SetCheatNoEnc(int v) { CtrSettings_SetCheat(CTR_CHEAT_NOENC, !(v & 1)); }
static int CheatNoEncCount(void) { return CheatCount(CTR_CHEAT_NOENC); }
static int GetCheatWalls(void) { return !CtrSettings_Cheat(CTR_CHEAT_WALLS); }      /* 0 is ON, as in the other switches */
static void SetCheatWalls(int v) { CtrSettings_SetCheat(CTR_CHEAT_WALLS, !(v & 1)); }
static int CheatWallsCount(void) { return CheatCount(CTR_CHEAT_WALLS); }
static int GetCheatExp(void) { return CtrSettings_Cheat(CTR_CHEAT_EXP); }
static void SetCheatExp(int v) { CtrSettings_SetCheat(CTR_CHEAT_EXP, v); }
static int CheatExpCount(void) { return CheatCount(CTR_CHEAT_EXP); }
static int GetCheatPp(void) { return !CtrSettings_Cheat(CTR_CHEAT_PP); }      /* 0 is ON, as in the other switches */
static void SetCheatPp(int v) { CtrSettings_SetCheat(CTR_CHEAT_PP, !(v & 1)); }
static int CheatPpCount(void) { return CheatCount(CTR_CHEAT_PP); }
static int GetCheatFlee(void) { return !CtrSettings_Cheat(CTR_CHEAT_FLEE); }      /* 0 is ON, as in the other switches */
static void SetCheatFlee(int v) { CtrSettings_SetCheat(CTR_CHEAT_FLEE, !(v & 1)); }
static int CheatFleeCount(void) { return CheatCount(CTR_CHEAT_FLEE); }
static int GetTmReuse(void) { return !CtrSettings_TmReuse(); }
static void SetTmReuse(int v) { CtrSettings_SetTmReuse(!(v & 1)); }
static int GetExpShare(void) { return CtrSettings_ExpShare(); }
static void SetExpShare(int v) { CtrSettings_SetExpShare(v); }

int CtrOptions_TypeMatchup(void) { return CtrSettings_TypeMatchup() == 0; }
int CtrOptions_MoveDetails(void) { return CtrSettings_MoveDetails() == 0; }
int CtrOptions_DayNight(void) { return CtrSettings_DayNight() != DAY_NIGHT_OFF; }
int CtrOptions_DayNightSecond(void) { return CtrSettings_DayNight() == DAY_NIGHT_FIXED ? CtrSettings_Hour() * HOUR_STEP_SECONDS : -1; }
int CtrOptions_Embers(void) { return sPortEmbers != CTR_EMBERS_OFF; }
int CtrOptions_EmbersLow(void) { return sPortEmbers == CTR_EMBERS_LOW; }
int CtrOptions_RepelAgain(void) { return CtrSettings_RepelAgain(); }
int CtrOptions_Companion(void) { return CtrSettings_Companion(); }
int CtrOptions_ExpShare(void) { return CtrSettings_ExpShare(); }
int CtrOptions_ReusableTm(void) { return CtrSettings_TmReuse(); }
int CtrOptions_Nuzlocke(void) { return CtrSettings_Nuzlocke(); }
int CtrOptions_NuzDuplicates(void) { return CtrSettings_NuzDuplicates(); }
int CtrOptions_NuzShiny(void) { return CtrSettings_NuzShiny(); }
int CtrOptions_WildDensity(void) { return CtrSettings_WildDensity(); }
int CtrOptions_WildSilhouette(void) { return CtrSettings_WildSilhouette(); }
int CtrOptions_WildAggro(void) { return CtrSettings_WildAggro(); }
int CtrOptions_NuzLevelCap(void) { return CtrSettings_NuzLevelCap(); }
int CtrOptions_Randomlocke(void) { return CtrSettings_Randomlocke(); }
int CtrOptions_RandomWild(void) { return CtrSettings_Randomlocke() && CtrSettings_RandomTarget() != 2; }
int CtrOptions_RandomTrainers(void) { return CtrSettings_Randomlocke() && CtrSettings_RandomTarget() != 1; }
int CtrOptions_NuzSetMode(void) { return CtrSettings_Nuzlocke() && CtrSettings_NuzSet(); }
int CtrOptions_FreeHm(void) { return CtrSettings_FreeHm(); }
int CtrOptions_SureCatch(void) { return CtrSettings_Cheat(CTR_CHEAT_CATCH); }
int CtrOptions_NoEncounters(void) { return CtrSettings_Cheat(CTR_CHEAT_NOENC); }
int CtrOptions_WalkThroughWalls(void) { return CtrSettings_Cheat(CTR_CHEAT_WALLS); }
int CtrOptions_InfinitePp(void) { return CtrSettings_Cheat(CTR_CHEAT_PP); }
int CtrOptions_AlwaysFlee(void) { return CtrSettings_Cheat(CTR_CHEAT_FLEE); }
int CtrOptions_ShinyOdds(void) { return CtrSettings_Cheat(CTR_CHEAT_SHINY); }

unsigned CtrOptions_ScaleExp(unsigned exp)
{
    unsigned scaled = exp << CtrSettings_Cheat(CTR_CHEAT_EXP);      /* x1, x2, x4, x8 */

    return scaled > 0xFFFFu ? 0xFFFFu : scaled;                     /* the game keeps experience gains in 16 bits */
}

/* SHINY ODDS: a creature that was not shiny gets a personality that is, with the chance the row says. The game's own odds stay: a shiny one is
 * left as it is. The new personality keeps the low byte (the gender and the ability's bit) and the nature (personality mod 25) of the old one, and
 * is found by trying the 65536 high halves of the personality with the 8 low halves that make a shiny one of each; about 80 of the 524288 fit, so
 * the first one is found after a few thousand tries. `roll` is 32 random bits, used both for the chance and for where the search starts. */
uint32_t CtrOptions_ShinyPersonality(uint32_t personality, uint32_t otId, uint32_t roll)
{
    int odds = CtrSettings_Cheat(CTR_CHEAT_SHINY);
    uint32_t ids = ((otId >> 16) ^ otId) & 0xFFFFu;

    if (odds < 1 || odds >= (int)(sizeof(kShinyDenominator) / sizeof(kShinyDenominator[0])))
        return personality;
    if (((((personality >> 16) ^ personality) & 0xFFFFu) ^ ids) < 8)
        return personality;                         /* already shiny */
    if ((roll >> 16) % kShinyDenominator[odds] != 0)
        return personality;
    for (uint32_t n = 0; n < 0x10000u; ++n) {
        uint32_t high = ((roll & 0xFFFFu) + n) & 0xFFFFu;

        for (uint32_t k = 0; k < 8; ++k) {
            uint32_t candidate = (high << 16) | ((high ^ ids ^ k) & 0xFFFFu);

            if ((candidate & 0xFFu) == (personality & 0xFFu) && candidate % 25u == personality % 25u)
                return candidate;
        }
    }
    return personality;
}

/* The legendary and mythical species of the pool (FireRed's numbers): the birds, Mewtwo, Mew, the beasts, Lugia, Ho-Oh, Celebi and Hoenn's
 * Regis, Kyogre, Groudon, Rayquaza, Latias, Latios, Jirachi and Deoxys. */
static int IsLegendary(unsigned s)
{
    return (s >= 144 && s <= 146) || s == 150 || s == 151 || (s >= 243 && s <= 245) || (s >= 249 && s <= 251) || (s >= 401 && s <= 410);
}

unsigned CtrOptions_RandomSpecies(unsigned seed, unsigned place, unsigned species)
{
    uint32_t h = (uint32_t)seed ^ ((uint32_t)place * 0x9E3779B1u) ^ ((uint32_t)species * 0x85EBCA6Bu);
    int noLegend = CtrSettings_RandomNoLegendary();
    unsigned pick;

    for (int tries = 0;; ++tries) {
        h ^= h >> 16;
        h *= 0x7FEB352Du;
        h ^= h >> 15;
        h *= 0x846CA68Bu;
        h ^= h >> 16;
        pick = h % 386;                         /* 251 of Kanto and Johto (1-251), 135 of Hoenn (277-411) */
        pick = pick < 251 ? pick + 1 : pick - 251 + 277;
        if (!noLegend || !IsLegendary(pick) || tries >= 64)
            return pick;
        h += 0x9E3779B9u;                       /* the same seed and place always walk the same way, so the choice stays fixed */
    }
}

void CtrOptions_EmbersInit(int isNew3ds)
{
    int stored = CtrSettings_Embers();

    sPortEmbers = stored >= 0 && stored < CTR_EMBERS_COUNT ? stored : isNew3ds ? CTR_EMBERS_HIGH : CTR_EMBERS_LOW;
}

/* The rows by section: Settings, Graphics, QoL, Nuzlocke. */
static const CtrOptionRow sRows[] = {
    /* Settings: how the game plays and sounds. */
    {CTR_TR("Text speed", "Vel. texto", "Texttempo", "Vitesse texte"), {CTR_TR("How fast dialogue", "La velocidad a la que", "Geschwindigkeit der", "Vitesse d'affichage"), CTR_TR("text is printed.", "aparece el texto.", "Textanzeige.", "du dialogue.")}, PT_OPT_TEXT_SPEED, sTextSpeed, NULL, NULL, NULL, NULL},
    {CTR_TR("Battle scene", "Animaci\xC3\xB3n", "Kampfanimation", "Anim. combat"), {CTR_TR("Show or skip the move", "Ver o saltar las", "Attackenanimationen im", "Afficher ou ignorer"), CTR_TR("animations in battle.", "animaciones de ataques.", "Kampf zeigen oder nicht.", "les animations.")}, PT_OPT_BATTLE_SCENE, sBattleScene, NULL,
     NULL, NULL, NULL},
    {CTR_TR("Battle style", "Estilo combate", "Kampfstil", "Style combat"), {CTR_TR("Shift offers a switch", "Cambio te deja cambiar", "Wechsel erlaubt Tausch", "Changement facultatif"), CTR_TR("after a foe faints.", "si el rival se debilita.", "nach besiegtem Gegner.", "apr\xC3\xA8s un K.O. adverse.")}, PT_OPT_BATTLE_STYLE, sBattleStyle, NULL,
     NULL, NULL, NULL},
    {CTR_TR("Sound", "Sonido", "Ton", "Son"), {CTR_TR("Mono or stereo sound.", "Sonido mono o est\xC3\xA9reo.", "Mono oder Stereo.", "Son mono ou st\xC3\xA9r\xC3\xA9o."), ""}, PT_OPT_SOUND, sSound, NULL, NULL, NULL, NULL},
    {CTR_TR("Button mode", "Modo botones", "Tastenmodus", "Mode boutons"), {CTR_TR("What L and R do:", "Qu\xC3\xA9 hacen L y R:", "Funktion von L und R:", "R\xC3\xB4le de L et R :"), CTR_TR("Help, LR or L=A.", "Ayuda, LR o L=A.", "Hilfe, LR oder L=A.", "Aide, LR ou L pour A.")}, PT_OPT_BUTTON_MODE, sButtonMode, NULL,
     NULL, NULL, NULL},
    {CTR_TR("Multiplayer", "Multijugador", "Mehrspieler", "Multijoueur"), {CTR_TR("Online: on the internet.", "Online: por internet.", "Online: \xC3\xBC" "bers Internet.", "En ligne : internet."),
     CTR_TR("Local: someone nearby.", "Local: alguien cerca.", "Lokal: jemand nahebei.", "Local : pr\xC3\xA8s de vous.")}, 0, sMultiplayer, NULL, GetLocalPlay, TwoCount, SetLocalPlay},
    {CTR_TR("Online room", "Sala online", "Online-Raum", "Salon en ligne"), {CTR_TR("Public: everyone online.", "P\xC3\xBA" "blica: todos online.", "\xC3\x96" "ffentlich: alle online.", "Publique : pour tous."),
     CTR_TR("Private: a shared code.", "Privada: con un c\xC3\xB3" "digo.", "Privat: mit einem Code.", "Priv\xC3\xA9" "e : code partag\xC3\xA9.")}, 0, sRoomKind, NULL, GetRoomKind, TwoCount, SetRoomKind},
    /* Graphics, page 1: the screen and what moves on it (the Poke Ball style is the only one: no STYLE row, and no EMBERS, which only it had). */
    {CTR_TR("Frame", "Marco", "Fenster", "Cadre"), {CTR_TR("Menus, Yes/No", "Men\xC3\xBAs y S\xC3\xAD/No", "Men\xC3\xBC-Rahmen", "Cadre"), CTR_TR("box frame.", "su marco.", "und JA/NEIN.", "des menus.")}, PT_OPT_FRAME, NULL, CTR_TR("Type", "Tipo", "Typ", "Type"), NULL, NULL, NULL},
    {CTR_TR("FPS counter", "Contador FPS", "FPS-Anzeige", "Cadence vid\xC3\xA9o"), {CTR_TR("Show the frames per", "Muestra las im\xC3\xA1genes por", "Bilder pro Sekunde", "Images par seconde"), CTR_TR("second up top.", "segundo arriba.", "oben anzeigen.", "en haut de l'\xC3\xA9" "cran.")}, PT_OPT_FPS, sFps, NULL, NULL, NULL, NULL},
    {CTR_TR("Day/Night", "D\xC3\xAD" "a/Noche", "Tag/Nacht", "Jour/Nuit"), {CTR_TR("The 3DS clock, the Hour", "Hora de la 3DS, la Hora", "3DS-Uhrzeit, die Stunde", "Utilise l'horloge 3DS,"), CTR_TR("below, or Off.", "de abajo, u Off.", "unten, oder Aus.", "une heure fixe ou Non.")}, 0, sDayNight, NULL,
     GetDayNight, DayNightCount, SetDayNight},
    {CTR_TR("Hour", "Hora", "Stunde", "Heure"), {CTR_TR("The hour outdoors when", "La hora fuera si", "Uhrzeit in der Oberwelt,", "Heure en ext\xC3\xA9rieur"), CTR_TR("Day/Night is Fixed.", "D\xC3\xAD" "a/Noche est\xC3\xA1 en Fija.", "bei TAG/NACHT FEST.", "si Jour/Nuit est fixe.")}, 0, sHourName, NULL,
     GetHour, HourCount, SetHour},
    {CTR_TR("Companion", "Acompa\xC3\xB1" "ante", "Begleiter", "Compagnon"), {CTR_TR("A Pokemon follows you,", "Un Pok\xC3\xA9mon te sigue,", "Ein Pok\xC3\xA9mon folgt dir,", "Un Pok\xC3\xA9mon vous suit,"), CTR_TR("or you become one.", "o t\xC3\xBA eres ese Pok\xC3\xA9mon.", "oder du bist es selbst.", "ou vous l'incarnez.")}, 0, sCompanion, NULL,
     GetCompanion, ThreeCount, SetCompanion},
    /* Graphics, page 2: the looks (a preview card takes the rows it does not use). */
    {CTR_TR("Red look", "Aspecto rojo", "Rot-Look", "Style de Red"), {CTR_TR("Red's look, from the", "El aspecto de Rojo, de", "Rots Aussehen, aus", "Choisir l'apparence"), CTR_TR("alternative looks.", "los aspectos extra.", "den Builder-Looks.", "de Red.")}, 0, sLookNames, NULL,
     GetLookRed, LookCount, SetLookRed},
    {CTR_TR("Leaf look", "Aspecto hoja", "Blatt-Look", "Style de Leaf"), {CTR_TR("Leaf's look, from the", "El aspecto de Hoja, de", "Aussehen von Blatt, aus", "Choisir l'apparence"), CTR_TR("alternative looks.", "los aspectos extra.", "den Builder-Looks.", "de Red.")}, 0, sLookNames, NULL,
     GetLookLeaf, LookCount, SetLookLeaf},
    {CTR_TR("Rival look", "Aspecto rival", "Rivalen-Look", "Style de Blue"), {CTR_TR("Blue's look, from the", "El aspecto de Azul, de", "Blaus Aussehen, aus", "Choisir l'apparence"), CTR_TR("alternative looks.", "los aspectos extra.", "aus den Builder-Looks.", "de Red.")}, 0, sLookNames, NULL,
     GetLookBlue, LookCount, SetLookBlue},
    {CTR_TR("Surf Pokemon", "Pok\xC3\xA9mon surf", "Surf-Pok\xC3\xA9mon", "Monture"), {CTR_TR("The Pokemon you surf on,", "El Pok\xC3\xA9mon sobre el que", "Das Pok\xC3\xA9mon, auf dem du", "Choisir le Pok\xC3\xA9mon"), CTR_TR("from the extra ones.", "surfeas, de los extra.", "surfst, aus den Extras.", "parmi les autres.")}, 0, sSurfNames, NULL,
     GetSurf, SurfCount, SetSurf},
    /* Graphics, page 3: the wild Pokemon that are seen in the overworld. */
    {CTR_TR("Wild Pokemon", "Salvajes", "Wilde Pok\xC3\xA9mon", "Sauvages"), {CTR_TR("How many wild Pokemon", "Cu\xC3\xA1ntos Pok\xC3\xA9mon salvajes", "Wie viele wilde Pok\xC3\xA9mon", "Nombre de Pok\xC3\xA9mon"), CTR_TR("walk about the map.", "andan por el mapa.", "auf der Karte laufen.", "sauvages sur la carte.")}, 0,
     sWildDensity, NULL, GetWildDensity, WildDensityCount, SetWildDensity},
    {CTR_TR("Wild display", "Ver salvajes", "Anzeige", "Affichage"), {CTR_TR("Show them as Pokemon", "Se ven como Pok\xC3\xA9mon", "Als Pok\xC3\xA9mon oder", "Afficher les Pok\xC3\xA9mon"), CTR_TR("or as silhouettes.", "o como siluetas.", "als Schatten zeigen.", "ou des silhouettes.")}, 0,
     sWildDisplay, NULL, GetWildDisplay, TwoCount, SetWildDisplay},
    {CTR_TR("Aggressive", "Agresivos", "Aggressiv", "Agressifs"), {CTR_TR("Share of wild Pokemon", "La parte de salvajes que", "Anteil wilder Pok\xC3\xA9mon,", "Part de Pok\xC3\xA9mon"), CTR_TR("that chase you.", "te ve y te persigue.", "die dich jagen.", "qui vous poursuivent.")}, 0,
     sWildAggro, NULL, GetWildAggro, WildAggroCount, SetWildAggro},
    /* QoL: conveniences. */
    {CTR_TR("Type matchup", "Eficacia tipos", "Typ-Effekt.", "Efficacit\xC3\xA9"), {CTR_TR("Show type effectiveness", "Muestra la eficacia de", "Typenvor-/nachteile bei", "Afficher l'efficacit\xC3\xA9"), CTR_TR("on battle move cards.", "tipos en los ataques.", "Attacken zeigen.", "sur les cartes.")}, 0, sFps, NULL,
     GetTypeMatchup, TwoCount, SetTypeMatchup},
    {CTR_TR("Move details", "Datos ataques", "Attacken-Infos", "D\xC3\xA9tails"), {CTR_TR("Show live move details", "Muestra datos y clase", "Werte und Schadensklasse", "D\xC3\xA9tails des capacit\xC3\xA9s"), CTR_TR("and category in battle.", "de los ataques.", "der Attacken anzeigen.", "et cat\xC3\xA9gorie en jeu.")}, 0, sFps, NULL,
     GetMoveDetails, TwoCount, SetMoveDetails},
    {CTR_TR("Repel again", "Repelente", "Schutz erneut", "Repousse auto"), {CTR_TR("When a repel wears off,", "Al acabar un repelente,", "Wenn ein Schutz endet,", "Proposer un Repousse"), CTR_TR("offer to use another.", "pregunta si usar otro.", "einen weiteren anbieten.", "\xC3\xA0 la fin de l'effet.")}, 0, sFps, NULL,
     GetRepelAgain, TwoCount, SetRepelAgain},
    {CTR_TR("EXP share", "Repartir EXP", "EP-Teiler", "Partage d'EXP"), {CTR_TR("Share EXP with your", "Reparte la EXP con el", "EP an das ganze Team:", "Moiti\xC3\xA9 ou totalit\xC3\xA9"), CTR_TR("team: Half or Full.", "equipo: Mitad o Toda.", "Halb oder Voll.", "de l'EXP \xC3\xA0 l'\xC3\xA9quipe.")}, 0, sExpShare, NULL,
     GetExpShare, ThreeCount, SetExpShare},
    {CTR_TR("Reusable TMs", "MT infinitas", "TMs mehrfach", "CT illimit\xC3\xA9" "es"), {CTR_TR("A TM is not used up", "Una MT no se gasta al", "Eine TM wird beim Lehren", "Conserver les CT"), CTR_TR("by teaching a move.", "ense\xC3\xB1" "ar un ataque.", "nicht verbraucht.", "apr\xC3\xA8s usage.")}, 0, sFps, NULL,
     GetTmReuse, TwoCount, SetTmReuse},
    {CTR_TR("Free HMs", "MO libres", "VMs frei", "CS directes"), {CTR_TR("An HM in the Bag works", "Las MO de la Mochila", "Eine VM im Beutel wirkt", "Utiliser les CS du Sac"), CTR_TR("in the field, no move.", "sirven sin ense\xC3\xB1" "arlas.", "ohne sie zu lehren.", "sans capacit\xC3\xA9 apprise.")}, 0, sFps, NULL,
     GetFreeHm, TwoCount, SetFreeHm},
    /* Nuzlocke: the mode and its optional rules (they count only while it is on). */
    {CTR_TR("Nuzlocke", "Nuzlocke", "Nuzlocke", "Nuzlocke"), {CTR_TR("One catch per area;", "Un salvaje por zona;", "Ein Fang pro Gebiet;", "Une capture par zone ;"), CTR_TR("fainted ones go free.", "los ca\xC3\xAD" "dos se liberan.", "Besiegte werden frei.", "Pok\xC3\xA9mon K.O. rel\xC3\xA2" "ch\xC3\xA9s.")}, 0, sFps, NULL,
     GetNuzlocke, TwoCount, SetNuzlocke},
    {CTR_TR("Duplicates", "Repetidos", "Duplikate", "Doublons"), {CTR_TR("A species you caught", "Una especie capturada", "Eine gefangene Art", "Une esp\xC3\xA8" "ce captur\xC3\xA9" "e"), CTR_TR("does not use the area.", "no gasta la zona.", "z\xC3\xA4hlt nicht f\xC3\xBCrs Gebiet.", "ne compte pas.")}, 0, sFps, NULL,
     GetNuzDupes, TwoCount, SetNuzDupes},
    {CTR_TR("Shiny clause", "Cl\xC3\xA1usula shiny", "Shiny-Regel", "Chromatique"), {CTR_TR("A shiny Pokemon does not", "Un Pok\xC3\xA9mon shiny no", "Ein Shiny-Pok\xC3\xA9mon z\xC3\xA4hlt", "Un Pok\xC3\xA9mon chromatique"), CTR_TR("use up the area.", "gasta la zona.", "nicht f\xC3\xBCr das Gebiet.", "ne compte pas.")}, 0, sFps, NULL,
     GetNuzShiny, TwoCount, SetNuzShiny},
    {CTR_TR("Level caps", "Nivel m\xC3\xA1ximo", "Level-Limit", "Niveau max"), {CTR_TR("No EXP at or above the", "Sin EXP al nivel del", "Keine EP ab dem Level", "Pas d'exp\xC3\xA9rience d\xC3\xA8s"), CTR_TR("next Gym Leader's level.", "pr\xC3\xB3ximo l\xC3\xAD" "der.", "des n\xC3\xA4" "chsten Leiters.", "le niveau du Champion.")}, 0, sFps, NULL,
     GetNuzCap, TwoCount, SetNuzCap},
    {CTR_TR("Set mode", "Modo fijo", "Folgen-Modus", "Mode d\xC3\xA9" "fini"), {CTR_TR("The battle style is Set:", "El estilo es Fijo: no", "Kampfstil FEST: kein", "Pas d'\xC3\xA9" "change propos\xC3\xA9"), CTR_TR("no free switch.", "hay cambio gratis.", "freier Wechsel.", "apr\xC3\xA8s un K.O. adverse.")}, 0, sFps, NULL,
     GetNuzSet, TwoCount, SetNuzSet},
    {CTR_TR("Randomlocke", "Randomlocke", "Randomlocke", "Randomlocke"), {CTR_TR("Wild and trainer Pokemon", "Salvajes y entrenadores", "Wilde und gegnerische", "Pok\xC3\xA9mon sauvages"), CTR_TR("are random species.", "con especies al azar.", "Pok\xC3\xA9mon sind zuf\xC3\xA4llig.", "et adverses au hasard.")}, 0, sFps, NULL,
     GetRandomlocke, TwoCount, SetRandomlocke},
    {CTR_TR("Random for", "Azar en", "Zufall f\xC3\xBCr", "Au hasard"), {CTR_TR("Who is random: both,", "Qui\xC3\xA9n es aleatorio:", "Wer zuf\xC3\xA4llig ist: beide,", "Sauvages, dresseurs"), CTR_TR("wild or trainers.", "ambos, salvajes, rivales", "Wilde oder Trainer.", "ou les deux.")}, 0, sRandTargetName, NULL,
     GetRandTarget, ThreeCount, SetRandTarget},
    {CTR_TR("Legendaries", "Legendarios", "Legend\xC3\xA4re", "L\xC3\xA9gendaires"), {CTR_TR("Allow legendary species", "Permite especies", "Legend\xC3\xA4re Arten sind", "Pok\xC3\xA9mon l\xC3\xA9gendaires"), CTR_TR("among the random ones.", "legendarias al azar.", "unter den zuf\xC3\xA4lligen.", "admis au tirage.")}, 0, sFps, NULL,
     GetRandLegend, TwoCount, SetRandLegend},
    /* Cheats: a page of its own, off until chosen. */
    {CTR_TR("Sure catch", "Captura segura", "Sicherer Fang", "Capture s\xC3\xBBre"), {CTR_TR("Every Poke Ball catches", "Toda bola captura al", "Jeder Ball f\xC3\xA4ngt das", "Toute Ball capture"), CTR_TR("the wild Pokemon.", "Pok\xC3\xA9mon salvaje.", "wilde Pok\xC3\xA9mon.", "le Pok\xC3\xA9mon sauvage.")}, 0, sFps, NULL,
     GetCheatCatch, CheatCatchCount, SetCheatCatch},
    {CTR_TR("Shiny odds", "Prob. shiny", "Shiny-Chance", "Taux chroma."), {CTR_TR("The chance that a new", "La probabilidad de que", "Chance, dass ein neues", "La chance d'un nouveau"), CTR_TR("Pokemon is shiny.", "un Pok\xC3\xA9mon sea shiny.", "Pok\xC3\xA9mon schillert.", "Pok\xC3\xA9mon chromatique.")}, 0, sShinyOdds, NULL,
     GetCheatShiny, CheatShinyCount, SetCheatShiny},
    {CTR_TR("No encounters", "Sin encuentros", "Keine Begegn.", "Sans sauvages"), {CTR_TR("No wild Pokemon appear", "No aparecen Pok\xC3\xA9mon", "Keine wilden Pok\xC3\xA9mon", "Rien dans l'herbe"), CTR_TR("in grass or caves.", "en hierba ni cuevas.", "in Gras und H\xC3\xB6hlen.", "ni dans les grottes.")}, 0, sFps, NULL,
     GetCheatNoEnc, CheatNoEncCount, SetCheatNoEnc},
    {CTR_TR("Through walls", "Atrav. paredes", "Durch W\xC3\xA4nde", "Traverser"), {CTR_TR("Walk through walls and", "Camina a trav\xC3\xA9s de", "Durch W\xC3\xA4nde und", "Traverser les murs et"), CTR_TR("objects, not the edge.", "paredes y objetos.", "Objekte laufen.", "objets, pas le bord.")}, 0, sFps, NULL,
     GetCheatWalls, CheatWallsCount, SetCheatWalls},
    {CTR_TR("EXP rate", "Ritmo EXP", "EP-Rate", "Taux d'EXP"), {CTR_TR("The experience of a won", "La EXP de un combate", "Die EP eines Kampfes", "L'EXP d'un combat"), CTR_TR("battle, times 1 to 8.", "ganado, por 1 a 8.", "mal 1 bis 8.", "gagn\xC3\xA9, fois 1 \xC3\xA0 8.")}, 0, sExpRate, NULL,
     GetCheatExp, CheatExpCount, SetCheatExp},
    {CTR_TR("Infinite PP", "PP infinitos", "Unendl. PP", "PP infinis"), {CTR_TR("Your moves never lose", "Tus ataques nunca", "Deine Attacken verlieren", "Vos capacit\xC3\xA9s gardent"), CTR_TR("PP in battle.", "pierden PP.", "keine AP.", "leur PP en combat.")}, 0, sFps, NULL,
     GetCheatPp, CheatPpCount, SetCheatPp},
    {CTR_TR("Always flee", "Huida segura", "Immer fliehen", "Fuite assur\xC3\xA9" "e"), {CTR_TR("Running away from a wild", "Huir de un Pok\xC3\xA9mon", "Die Flucht vor einem", "Fuir un sauvage"), CTR_TR("Pokemon always works.", "salvaje siempre sale.", "wilden Pok\xC3\xA9mon klappt.", "r\xC3\xA9ussit toujours.")}, 0, sFps, NULL,
     GetCheatFlee, CheatFleeCount, SetCheatFlee},
};

#define ROWS ((int)(sizeof(sRows) / sizeof(sRows[0])))

/* The pages: the first row of each, then the end. The looks page is short on purpose: the look preview is a card in the rows a page
 * does not use. Four sections hold them: Settings, Graphics (three pages: the screen, the looks, the wild Pokemon), QoL and Nuzlocke (two pages: the rules and the Randomlocke). */
enum { PAGE1 = 0, PAGE2 = 7, PAGE3 = 12, PAGE4 = 16, PAGE5 = 19, PAGE6 = 25, PAGE7 = 30, PAGE8 = 33 };
static const uint8_t sPageStart[] = {PAGE1, PAGE2, PAGE3, PAGE4, PAGE5, PAGE6, PAGE7, PAGE8, ROWS};
static const uint8_t sPageSection[] = {0, 1, 1, 1, 2, 3, 3, 4};
#define PAGES ((int)(sizeof(sPageStart) / sizeof(sPageStart[0])) - 1)
#define SECTIONS 5
_Static_assert(PAGE8 < ROWS, "the last page has rows");
_Static_assert(sizeof(sPageSection) == PAGES, "a section for each page");
_Static_assert(PAGE2 - PAGE1 <= PT_OPT_MAX && PAGE3 - PAGE2 <= PT_OPT_MAX && PAGE4 - PAGE3 <= PT_OPT_MAX && PAGE5 - PAGE4 <= PT_OPT_MAX
               && PAGE6 - PAGE5 <= PT_OPT_MAX && PAGE7 - PAGE6 <= PT_OPT_MAX && PAGE8 - PAGE7 <= PT_OPT_MAX && ROWS - PAGE8 <= PT_OPT_MAX,
               "a page holds up to PT_OPT_MAX rows");

int CtrOptions_PageCount(void) { return PAGES; }

const char *CtrOptions_PageName(int page)
{
    /* A page's own short name (it has to fit the PAGE button's 64 px): the looks page is a page of the Graphics section, and says so with its dot. */
    static const char *const names[] = {CTR_TR("Settings", "Ajustes", "Optionen", "R\xC3\xA9glages"), CTR_TR("Graphics", "Gr\xC3\xA1" "ficos", "Grafik", "Graphismes"),
                                        CTR_TR("Looks", "Aspectos", "Looks", "Apparences"), CTR_TR("Wilds", "Salvajes", "Wilde", "Sauvages"), CTR_TR("QoL", "QoL", "QoL", "Confort"),
                                        CTR_TR("Nuzlocke", "Nuzlocke", "Nuzlocke", "Nuzlocke"), CTR_TR("Random", "Azar", "Zufall", "Al\xC3\xA9" "atoire"), CTR_TR("Cheats", "Trucos", "Cheats", "Triches")};

    _Static_assert(sizeof(names) / sizeof(names[0]) == PAGES, "one name for each page");
    return page >= 0 && page < PAGES ? names[page] : "";
}

int CtrOptions_SectionCount(void) { return SECTIONS; }

int CtrOptions_SectionOf(int page)
{
    return page < 0 || page >= PAGES ? 0 : sPageSection[page];
}

/* A section's page `page` is in (1 based) and how many pages the section has: "Graphics 1/2". */
int CtrOptions_SectionPage(int page, int *of)
{
    int n = 0, at = 0, sec = CtrOptions_SectionOf(page);

    for (int i = 0; i < PAGES; ++i)
        if (sPageSection[i] == sec) {
            ++n;
            if (i <= page)
                at = n;
        }
    if (of)
        *of = n;
    return at;
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

    return r != NULL && (r->names == sCompanion || r->names == sExpShare);
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
    if (selected >= 0 && selected < ROWS && sRows[selected].names == sSurfNames)
    {
        /* The Pokemon to surf on: its name, the artist and the blob facing south. */
        l->on = 1;
        index = CtrSurf_Chosen();
        text = index >= 0 ? CtrSurf_Name(index) : CTR_TR("Default", "Normal", "Standard", "Par d\xC3\xA9" "faut");
        snprintf(l->name, sizeof(l->name), "%s", text ? text : "");
        if (index < 0)
        {
            snprintf(l->credit, sizeof(l->credit), "%s", CTR_TR("The one FireRed has.", "El que tiene FireRed.", "Das von FireRed.", "Celui de Rouge Feu."));
            return;
        }
        text = CtrSurf_Credit(index);
        snprintf(l->credit, sizeof(l->credit), "%s", text ? text : "");
        l->has = (uint8_t)CtrSurf_Picture(index, l->tiles, l->palette);
        return;
    }
    if (slot < 0 || slot > 2)
        return;
    l->on = 1;
    index = CtrLooks_Chosen(slot);
    text = index >= 0 ? CtrLooks_Name(index) : CTR_TR("Default", "Normal", "Standard", "Par d\xC3\xA9" "faut");
    snprintf(l->name, sizeof(l->name), "%s", text ? text : "");
    if (index < 0)
    {
        snprintf(l->credit, sizeof(l->credit), "%s", CTR_TR("The look the game had before.", "El aspecto que ten\xC3\xAD" "a el juego.", "Das Aussehen, das das Spiel hatte.", "L'apparence d'origine du jeu."));
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
    for (int i = 0; i < PAGES && i < (int)sizeof(v->optDotSection); ++i)
        v->optDotSection[i] = sPageSection[i];
    {
        v->optSection = (uint8_t)CtrOptions_SectionOf(page);
        v->optSectionCount = SECTIONS;
        snprintf(v->optPageName, sizeof(v->optPageName), "%.19s", CtrOptions_PageName(page));
    }
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
    /* ONLINE ROOM in a private room: the note says its code, and that a tap on the row changes it. */
    if (sRows[selected].get == GetRoomKind && CtrSettings_PrivateRoom()) {
        const char *code = CtrSettings_RoomCode();

        snprintf(v->bar[1], PT_BAR_LEN, CTR_TR("Code %.4s %.4s %.4s %.4s.", "C\xC3\xB3" "digo %.4s %.4s %.4s %.4s.", "Code %.4s %.4s %.4s %.4s.", "Code %.4s %.4s %.4s %.4s."), code, code + 4, code + 8, code + 12);
        snprintf(v->bar[2], PT_BAR_LEN, "%s", CTR_TR("Tap the row to change it.", "Toca la fila para cambiarlo.", "Zeile antippen: neu.", "Touchez pour changer."));
    }
    /* ONLINE ROOM after an older build's code was dropped: say so, so that the public room is never entered without the player knowing. */
    if (sRows[selected].get == GetRoomKind && !CtrSettings_PrivateRoom() && CtrSettings_RoomCodeDropped()) {
        snprintf(v->bar[1], PT_BAR_LEN, "%s", CTR_TR("Old code removed: now PUBLIC.", "C\xC3\xB3" "digo viejo quitado: P\xC3\x9A" "BLICA.", "Alter Code entfernt: \xC3\x96" "FFENTLICH.", "Code retir\xC3\xA9 : PUBLIC."));
        snprintf(v->bar[2], PT_BAR_LEN, "%s", CTR_TR("Pick PRIVATE for a new code.", "Elige PRIVADA: c\xC3\xB3" "digo nuevo.", "PRIVAT w\xC3\xA4" "hlen: neuer Code.", "Choisissez PRIV\xC3\x89" "E."));
    }
    memset(&v->optPad, 0, sizeof(v->optPad));
    if (sPad.on) {
        v->optPad.on = 1;
        v->optPad.len = (uint8_t)sPad.len;
        v->optPad.ok = (uint8_t)CtrSettings_RoomCodeOk(sPad.code);
        v->optPad.pressed = (int8_t)(sPad.down && sPad.inside ? sPad.key : -1);
        memcpy(v->optPad.code, sPad.code, sizeof(v->optPad.code));
        snprintf(v->bar[0], PT_BAR_LEN, "%s", CTR_TR("Private room", "Sala privada", "Privater Raum", "Salon priv\xC3\xA9"));
        snprintf(v->bar[1], PT_BAR_LEN, "%s", CTR_TR("NEW makes a code; your friends", "NUEVO crea un c\xC3\xB3" "digo; tus amigos", "NEU macht einen Code; Freunde", "NOUVEAU fait un code ;"));
        snprintf(v->bar[2], PT_BAR_LEN, "%s", CTR_TR("type the same 16 digits.", "ponen las mismas 16 cifras.", "tippen dieselben 16 Ziffern.", "vos amis ont le m\xC3\xAAme."));
    }
}

/* A tap on a row's label (it only selects it): ONLINE ROOM in a private room opens the keypad for a new code. */
void CtrOptions_Activate(int row)
{
    if (row >= 0 && row < ROWS && sRows[row].get == GetRoomKind && CtrSettings_PrivateRoom())
        PadOpen();
}
