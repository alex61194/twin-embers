/* A link battle's team on the lower screen (included by 3ds_bottom.c after the Snapshot). Choosing the team (the Union Room's battle, the
 * Cable Club's) is the Party's pick of several: its count and the order are drawn on the cards (BUI_PTY_MULTI). Before a multi battle starts
 * the game shows both teams (the player's three on the left, the partner's three on the right): BUI_PTY_SHOWCASE, nothing to touch. The
 * upper screen keeps the room the players are in for all of it (CtrLinkTeam_Hold). The game says what it has (recipe 0330). */

extern int CtrLinkTeam_Hold(void);
extern int CtrBottomParty_EntryLimit(void);
extern int CtrBottomParty_EntryCount(void);
extern int CtrBottomParty_Showcase(void);
extern int CtrBottomParty_ShowcasePresent(int card);
extern unsigned CtrBottomParty_ShowcaseLevel(int card);
extern unsigned CtrBottomParty_ShowcaseHP(int card);
extern unsigned CtrBottomParty_ShowcaseMaxHP(int card);
extern unsigned CtrBottomParty_ShowcaseStatus(int card);
extern unsigned CtrBottomParty_ShowcaseIconSpecies(int card);
extern void CtrBottomParty_ShowcaseName(int card, char *out);
extern void CtrBottomParty_ShowcasePartner(char *out);

/* The Party snapshot's team fields: how many the pick takes and has, and, in the showcase, the six cards (card 2n the player's n-th, 2n+1 the
 * partner's n-th) in place of the Party's own slots. */
static void TeamFill(Snapshot *s)
{
    s->entryLimit = (unsigned char)CtrBottomParty_EntryLimit();
    s->entryCount = (unsigned char)CtrBottomParty_EntryCount();
    s->showcase = (unsigned char)CtrBottomParty_Showcase();
    if (!s->showcase)
        return;
    CtrBottomParty_ShowcasePartner(s->partner);
    s->cursor = -1;
    s->message[0] = 0;
    s->presentMask = 0;
    for (int i = 0; i < PARTY_SIZE; ++i) {
        s->nick[i][0] = 0;
        s->description[i][0] = 0;
        s->isEgg[i] = 0;
        if (!CtrBottomParty_ShowcasePresent(i))
            continue;
        s->presentMask |= (unsigned char)(1u << i);
        s->hp[i] = (uint16_t)CtrBottomParty_ShowcaseHP(i);
        s->maxHp[i] = (uint16_t)CtrBottomParty_ShowcaseMaxHP(i);
        s->status[i] = (unsigned char)CtrBottomParty_ShowcaseStatus(i);
        s->level[i] = (unsigned char)CtrBottomParty_ShowcaseLevel(i);
        s->icon[i] = (uint16_t)CtrBottomParty_ShowcaseIconSpecies(i);
        CtrBottomParty_ShowcaseName(i, s->nick[i]);
        s->nick[i][12] = 0;
    }
}

/* The battle Party view's team parts: the counts the note says, and the showcase's mode and caption (who the other team is). */
static void TeamPartyView(const Snapshot *s, CtrBattleParty *p)
{
    p->entryLimit = s->entryLimit;
    p->entryCount = s->entryCount;
    if (!s->showcase)
        return;
    p->mode = BUI_PTY_SHOWCASE;
    snprintf(p->caption, sizeof(p->caption), CTR_TR("Your team and %s's.", "Tu equipo y el de %s.", "Dein Team und das von %s."),
             s->partner[0] ? s->partner : CTR_TR("your partner", "tu compa\xC3\xB1" "ero", "deinem Partner"));
}
