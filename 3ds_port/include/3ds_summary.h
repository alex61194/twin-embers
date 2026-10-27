#ifndef CTR_SUMMARY_H
#define CTR_SUMMARY_H

/* Where a Summary the lower screen shows was opened from: FireRed's normal Summary of the player's party, from the
 * field's Party or from the battle's Party (it returns there through CB2_ReturnToPartyMenuFromSummaryScreen), or a PC
 * box's SUMMARY (of a Pokemon in the box, in the Party beside it or in hand; it returns through
 * CB2_ReturnToPokeStorage). Fixed when ShowPokemonSummaryScreen starts, before any callback changes, and kept for the
 * Summary's whole life: a battle's Summary is the battle's even though BattleMainCB2 is not running. Every other
 * Summary (learning or forgetting a move, a trade, ...) is NONE and stays FireRed's own picture. */
enum CtrSummaryOrigin {
    CTR_SUMMARY_NONE = 0,
    CTR_SUMMARY_FIELD_PARTY,
    CTR_SUMMARY_BATTLE_PARTY,
    CTR_SUMMARY_PC_BOX,
};

/* Display snapshot only; the FireRed Summary allocation owns all state. */
typedef struct {
    unsigned serial;
    int origin;                     /* CTR_SUMMARY_* */
    int ready, page, mon, egg, cursor, swapping, swapCursor;
    unsigned short moves[4];
    char nickname[12], species[12], level[8], gender[2], status[8];
    char dex[8], ot[16], id[8], item[20], types[2][12];
    char hp[12], stats[5][8], exp[12], nextExp[12];
    char ability[16], abilityDesc[64], memo[256], hatch[128];
    char moveNames[4][16], pp[4][12], moveTypes[4][12];
    char power[4][8], accuracy[4][8], moveDesc[256];
    /* What the lower screen's own design reads besides the Summary's strings: the type numbers (its colours), the
     * nature's name, the ribbons earned and the HP as numbers (its bar); all read from the Pokemon shown. */
    unsigned char typeIds[2], moveTypeIds[4];
    char nature[12];
    unsigned short hpCur, hpMax;
    unsigned char ribbons;
} CtrSummaryView;

int CtrBottomSummary_IsActive(void);
void CtrBottomSummary_GetView(CtrSummaryView *out);
/* The origin of the Summary that exists now (allocated, from its setup to its close), CTR_SUMMARY_NONE without one. */
int CtrBottomSummary_Origin(void);

/* Touch on the real Summary: its live input state, and what it shows under a
 * GBA pixel (CtrBottomSummary_HitAt, -1 for nothing). */
#define CTR_SUMMARY_HIT_PAGE 0x10      /* + page: the page-progress tiles */
#define CTR_SUMMARY_HIT_PAGE_NAME 0x20 /* the page name */
#define CTR_SUMMARY_HIT_CONTROLS 0x21  /* the controls hint, its page part */
#define CTR_SUMMARY_HIT_BUTTON_A 0x22  /* the controls hint, its A part */
#define CTR_SUMMARY_HIT_MOVE 0x30      /* + row on the moves pages, 4 CANCEL */
int CtrBottomSummary_Ready(void);
int CtrBottomSummary_Page(void);
int CtrBottomSummary_Mon(void);
unsigned CtrBottomSummary_Serial(void);
int CtrBottomSummary_MoveCursor(void);
int CtrBottomSummary_HitAt(int x, int y);
/* The Pokemon shown, its icon (FireRed's, the first frame, as CtrPokeTouch_MonIcon gives it): its icon species, 0 with
 * none, and the picture. A box's Pokemon are not the party's, so the Summary's own Pokemon is asked, not a slot. */
unsigned CtrBottomSummary_IconSpecies(void);
int CtrBottomSummary_Icon(unsigned char *tiles, unsigned short *palette);
/* The Pokemon shown, its picture as FireRed's Summary has it: the front picture (64x64 4bpp, 8x8 tiles in rows of
 * eight; an egg's is the egg) and palette (shiny or not), and whether FireRed flips it. 0 when there is none. */
int CtrBottomSummary_Pic(unsigned char *tiles, unsigned short *palette, int *flip);

#endif
