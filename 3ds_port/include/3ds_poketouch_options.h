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
void CtrOptions_ValueText(int row, char *out, int size);
/* The view's rows, their values and the bar for the selected row. */
void CtrOptions_Fill(CtrPokeTouchView *view, int selected);

/* The game bridge (patches/pokefirered/0068): FireRed's own option fields. */
extern int CtrPokeTouch_OptionCount(int option);
extern int CtrPokeTouch_OptionGet(int option);
extern void CtrPokeTouch_OptionSet(int option, int value);

#endif
