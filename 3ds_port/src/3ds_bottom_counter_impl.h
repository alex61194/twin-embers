/* The Cable Club counter's wait for the other players on the lower screen (included by 3ds_bottom.c). Between the counter's "Please wait" and
 * the link being made (recipe 0341, cable_club.c) FireRed shows its messages and a player count in boxes above; here the field is held, the
 * boxes are left out of it, and the lower screen shows the choice's look: FireRed's message in the note (or how many are linked) and CANCEL,
 * which sends the game its own B. A confirms the link with the console's A, as FireRed asks in its message. */

extern int CtrLinkup_Active(void);
extern int CtrLinkup_Players(void);

static void LinkupFill(CtrPokeTouchChoice *c)
{
    int players = CtrLinkup_Players();

    memset(c->label, 0, sizeof(c->label));
    snprintf(c->label[0], sizeof(c->label[0]), "%s", CTR_TR("CANCEL", "CANCELAR", "ABBRECHEN"));
    c->count = 1;
    c->cols = 1;
    c->cursor = 0;
    if (c->prompt[0] != 0)
        return;                             /* FireRed's own message is up: its words are the note */
    if (players >= 2)
        snprintf(c->prompt, sizeof(c->prompt), CTR_TR("%d players are linked.", "%d jugadores conectados.", "%d Spieler sind verbunden."), players);
    else
        snprintf(c->prompt, sizeof(c->prompt), "%s", CTR_TR("Please wait for the other players.", "Espera a los otros jugadores.",
                                                            "Warte auf die anderen Spieler."));
}
