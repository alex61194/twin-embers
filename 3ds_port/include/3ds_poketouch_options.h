#ifndef CTR_POKETOUCH_OPTIONS_H
#define CTR_POKETOUCH_OPTIONS_H

#include "3ds_poketouch.h"

/*
 * The rows of the native OPTIONS screen. A row is a static descriptor: its
 * label, a two-line description for the bar, and how its value is read, stepped
 * and shown. FireRed's own options are read and written through the game
 * bridge (ctr_poketouch_options.c, the save fields themselves: nothing is
 * copied here); a port-specific option adds a row whose get/step are its own.
 */
typedef struct {
    const char *label;
    const char *description[2];     /* the bar's second and third lines */
    int game;                       /* the bridge's PT_OPT_* option, when get is NULL */
    const char *const *names;       /* the value's names, or NULL: "<prefix><number>" */
    const char *prefix;
    int (*get)(void);               /* a port option's own value ... */
    int (*count)(void);             /* ... how many values it has ... */
    void (*set)(int value);         /* ... and how it takes one */
} CtrOptionRow;

int CtrOptions_RowCount(void);
const CtrOptionRow *CtrOptions_Row(int row);
int CtrOptions_Count(int row);                  /* values of a row */
int CtrOptions_Get(int row);
/* One step, `dir` +1 or -1, wrapping; the game's own side effect (SOUND's mono or stereo) applies. */
void CtrOptions_Step(int row, int dir);
/* Whether a row's values are levels (EMBERS), shown side by side and picked with a touch. */
int CtrOptions_IsLevels(int row);
/* A value picked outright (a touch on one of a row of levels); out of range does nothing. */
void CtrOptions_Set(int row, int value);
void CtrOptions_ValueText(int row, char *out, int size);
/* The view's rows, their values and the bar for the selected row. */
void CtrOptions_Fill(CtrPokeTouchView *view, int selected);

/* 3DS port QoL options (Page 2 of OPTIONS). */
int CtrOptions_TypeMatchup(void);
int CtrOptions_MoveDetails(void);
/* The day/night tint of the outdoor field (3ds_day_night.h): on unless set to OFF. */
int CtrOptions_DayNight(void);
/* ... the second of the day it shows: the one chosen (DAWN, DAY, DUSK, NIGHT), -1 for CLOCK (the 3DS clock's). */
int CtrOptions_DayNightSecond(void);
/* The embers rising behind the lower screen's menus: OFF, LOW or HIGH (CTR_EMBERS_*). LOW is a third of the
 * sparks, moved every third frame. The choice is kept in settings.cfg (3ds_settings.h); until the player makes
 * one, CtrOptions_EmbersInit gives the console's: HIGH on a New 3DS, LOW on an Old 3DS. */
enum { CTR_EMBERS_OFF, CTR_EMBERS_LOW, CTR_EMBERS_HIGH, CTR_EMBERS_COUNT };
void CtrOptions_EmbersInit(int isNew3ds);
int CtrOptions_Embers(void);        /* on unless OFF */
int CtrOptions_EmbersLow(void);
int CtrOptions_RepelAgain(void);    /* a Repel that wears off offers another (REPEL AGAIN) */
int CtrOptions_Companion(void);     /* 0 none, 1 the first Pokemon follows, 2 the player is that Pokemon (COMPANION) */
int CtrOptions_FreeHm(void);         /* FREE HMS: an HM in the Bag works in the field (with its badge) with no Pokemon that knows the move */
/* TRUCOS (the Cheats page): what the game code asks. Each is off until chosen. */
int CtrOptions_SureCatch(void);          /* every ball catches */
int CtrOptions_NoEncounters(void);       /* no random wild encounters */
int CtrOptions_WalkThroughWalls(void);   /* the player passes walls and objects (not the map edge) */
int CtrOptions_InfinitePp(void);         /* the player's moves keep their PP */
int CtrOptions_AlwaysFlee(void);         /* running away from a wild battle always works */
int CtrOptions_ShinyOdds(void);          /* 0 the game's own, 1..6: 1/4096, 1/1024, 1/256, 1/64, 1/16, always */
unsigned CtrOptions_ScaleExp(unsigned exp);   /* a won battle's experience times 1, 2, 4 or 8, at most 65535 */
/* A shiny personality that keeps the gender, the ability's bit and the nature, with the chance the row says; else `personality` itself. */
uint32_t CtrOptions_ShinyPersonality(uint32_t personality, uint32_t otId, uint32_t roll);
int CtrOptions_Randomlocke(void);    /* RANDOMLOCKE: wild and trainer Pokemon are random species */
int CtrOptions_RandomWild(void);     /* RANDOMLOCKE on and its target is not TRAINERS: wild Pokemon, gifts and the starter are random */
int CtrOptions_RandomTrainers(void); /* RANDOMLOCKE on and its target is not WILD: the trainers' Pokemon are random */
int CtrOptions_NuzSetMode(void);     /* NUZLOCKE on with SET MODE: the battle style is Set whatever OPTIONS says */
/* The species (FireRed's internal number) that stands in for one of the game's: always the same for a seed (the save's Trainer ID),
 * a place (a map, or a trainer's party slot) and the species it replaces, so an area keeps its Pokemon through a game. Never the
 * unused numbers between Celebi and Treecko, the Egg or nothing. */
unsigned CtrOptions_RandomSpecies(unsigned seed, unsigned place, unsigned species);
int CtrOptions_Nuzlocke(void);       /* the Nuzlocke rules (NUZLOCKE): one wild encounter to catch in each area, a fainted Pokemon is released */
int CtrOptions_NuzDuplicates(void);   /* NUZLOCKE clause (with NUZLOCKE on): a species already caught does not use the area's catch */
int CtrOptions_NuzShiny(void);        /* NUZLOCKE clause: a shiny does not use the area's catch */
int CtrOptions_WildDensity(void);     /* WILD POKEMON: 0 off, 1 few (2), 2 normal (4), 3 many (6) wild Pokemon seen in the overworld; walking into one starts its battle */
int CtrOptions_WildSilhouette(void);  /* WILD DISPLAY: 1 draws them as silhouettes */
int CtrOptions_WildAggro(void);       /* AGGRESSIVE: 0 off, 1 rare 10% ... 4 high 40% of the wild Pokemon notice the player and chase */
int CtrOptions_NuzLevelCap(void);     /* NUZLOCKE level caps: no experience at or above the next Gym Leader's level (by badges) */
/* The pages of OPTIONS: not a fixed number of rows each (a page may end early to keep a card under its rows). */
const char *CtrOptions_PageName(int page);   /* a page's short name in the game's language, "" past the last */
int CtrOptions_PageCount(void);
int CtrOptions_SectionCount(void);
int CtrOptions_SectionOf(int page);                 /* the section a page belongs to */
int CtrOptions_SectionPage(int page, int *of);      /* a page's place (1 based) in its section, and the section's pages */
int CtrOptions_PageOf(int row);
int CtrOptions_PageBase(int page);   /* the first row of a page */
int CtrOptions_PageRows(int page);
int CtrOptions_ReusableTm(void);    /* a TM is not used up when it teaches a move (REUSABLE TMS) */
int CtrOptions_ExpShare(void);      /* 0 the game's own, 1 the team shares by halves, 2 everyone gets the full share (EXP SHARE) */

/* The game bridge (patches/pokefirered/0068): FireRed's own option fields. */
extern int CtrPokeTouch_OptionCount(int option);
extern int CtrPokeTouch_OptionGet(int option);
extern void CtrPokeTouch_OptionSet(int option, int value);

/* Told when the player changes MULTIPLAYER (1: local play), so the radio takes it from the next session on (main_game_full.c sets it). */
void CtrOptions_SetLocalPlayHook(void (*hook)(int local));
/* The keypad of the private room's code (ONLINE ROOM): open, and a touch on it (the phase of the touch and the key under it, from
 * CtrPokeTouch_PadHit, -1 for none): a key acts on release over it. A tap on a row's label: CtrOptions_Activate. */
/* The private room's keypad's NEW key asks for random bytes: the console's generator by default; a test (or a host) gives a source, which returns 0
 * when it has none. */
void CtrOptions_SetRandomSource(int (*source)(uint8_t *out, size_t n));
int CtrOptions_PadOn(void);
/* Closes the keypad and forgets what was typed: OPTIONS opens without it, however it was last left. */
void CtrOptions_PadClose(void);
void CtrOptions_PadTouch(int touchDown, int touchActive, int key);
void CtrOptions_Activate(int row);

#endif
