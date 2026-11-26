/* The Union Room's wait for the others on the lower screen (included by 3ds_bottom.c). A link leader waits for players (Trade, battles,
 * the minigames): the game reads START to begin when enough have come (CtrUnionLeader_CanStart) and B to give up. Here it is a choice of
 * its own, drawn with the field choice's buttons: START (only when the game would take it) and CANCEL, and what is missing in the note.
 * The words are the port's (CTR_TR), not FireRed's; a tap sends the game its START or its B. */

extern int CtrUnionLeader_Active(void);
extern int CtrUnionLeader_Needed(void);
extern int CtrUnionLeader_CanStart(void);

static void UnionLeaderFill(CtrPokeTouchChoice *c)
{
    int needed = CtrUnionLeader_Needed(), rows = 0;

    memset(c->label, 0, sizeof(c->label));
    if (CtrUnionLeader_CanStart())
        snprintf(c->label[rows++], sizeof(c->label[0]), "%s", CTR_TR("START", "EMPEZAR", "START"));
    snprintf(c->label[rows++], sizeof(c->label[0]), "%s", CTR_TR("CANCEL", "CANCELAR", "ABBRECHEN"));
    c->count = (uint8_t)rows;
    c->cols = 1;
    c->cursor = 0;
    if (needed > 1)
        snprintf(c->prompt, sizeof(c->prompt), CTR_TR("%d players are missing.", "Faltan %d jugadores.", "Es fehlen %d Spieler."), needed);
    else if (needed == 1)
        snprintf(c->prompt, sizeof(c->prompt), "%s", CTR_TR("1 player is missing.", "Falta 1 jugador.", "Es fehlt 1 Spieler."));
    else
        snprintf(c->prompt, sizeof(c->prompt), "%s", CTR_TR("Everyone is here. START begins.", "Ya est\xC3\xA1n todos. EMPEZAR comienza.",
                                                            "Alle sind da. START beginnt."));
}

/* The key a row sends: START for the first row while the game takes it, B (the game's own CANCEL) for the last. */
static uint16_t UnionLeaderKey(int row)
{
    return CtrUnionLeader_CanStart() && row == 0 ? K_START : K_B;
}
