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
int CtrOptions_Randomlocke(void);    /* RANDOMLOCKE: wild and trainer Pokemon are random species */
/* The species (FireRed's internal number) that stands in for one of the game's: always the same for a seed (the save's Trainer ID),
 * a place (a map, or a trainer's party slot) and the species it replaces, so an area keeps its Pokemon through a game. Never the
 * unused numbers between Celebi and Treecko, the Egg or nothing. */
unsigned CtrOptions_RandomSpecies(unsigned seed, unsigned place, unsigned species);
int CtrOptions_Nuzlocke(void);       /* the Nuzlocke rules (NUZLOCKE): one wild encounter to catch in each area, a fainted Pokemon is released */
/* The pages of OPTIONS: not a fixed number of rows each (a page may end early to keep a card under its rows). */
int CtrOptions_PageCount(void);
int CtrOptions_PageOf(int row);
int CtrOptions_PageBase(int page);   /* the first row of a page */
int CtrOptions_PageRows(int page);
int CtrOptions_ReusableTm(void);    /* a TM is not used up when it teaches a move (REUSABLE TMS) */
int CtrOptions_ExpShare(void);      /* 0 the game's own, 1 the team shares by halves, 2 everyone gets the full share (EXP SHARE) */

/* The game bridge (patches/pokefirered/0068): FireRed's own option fields. */
extern int CtrPokeTouch_OptionCount(int option);
extern int CtrPokeTouch_OptionGet(int option);
extern void CtrPokeTouch_OptionSet(int option, int value);

#endif
