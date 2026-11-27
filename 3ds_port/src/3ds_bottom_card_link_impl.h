/* Another player's Trainer Card on the lower screen (included by 3ds_bottom.c). FireRed shows it (ShowTrainerCardInLink: the Union Room, the Cable
 * Club, the link START menu) and the port draws it like the player's own card (FieldCardFill); the upper screen keeps the room the players are in
 * through CtrBottomCard_LinkHold (recipe 0340, trainer_card.c). In a link the back is left with a closing key (it never turns back to the front),
 * and in the Union Room the card waits for the partner with FireRed's own text, which the note shows. */

extern int CtrBottomCard_IsLink(void);                   /* another player's card (or the own one in a link) */
extern int CtrBottomCard_BackCloses(void);               /* in a link the back is left, not turned back */
extern void CtrBottomCard_Wait(char *out, int size);     /* FireRed's "waiting for the trainer to finish reading", or empty */
extern int CtrBottomCard_LinkHold(void);                 /* the upper screen keeps the room around a link card */

/* The link part of the card's picture: whether it is a link card and the wait note (one line). */
static void CardLinkFill(CtrPokeTouchCard *c)
{
    c->link = (uint8_t)(CtrBottomCard_IsLink() != 0);
    CtrBottomCard_Wait(c->note, sizeof(c->note));
    for (char *q = c->note; *q; ++q)
        if (*q == '\n')
            *q = ' ';
}

/* The card's hint in the bar: how to turn it, or how to close it from a link card's back. */
static int CardHintId(void)
{
    return CtrBottomCard_IsLink() && CtrBottomCard_OnBack() ? UIS_CARD_CLOSE : UIS_CARD_TURN;
}

/* The key a tap on the card sends: A on the front (flips it), B on the back (flips it back), but A in a link (closes it). */
static uint16_t CardTapKey(void)
{
    return CtrBottomCard_State() == CARD_BACK && !CtrBottomCard_BackCloses() ? K_B : K_A;
}

/* The Trainer Card (the player's own or another player's) as the card shows it: its side, FireRed's labels and numbers, and its back's records. */
static void FieldCardFill(CtrPokeTouchView *v, int pressed)
{
    CtrPokeTouchCard *c = &v->card;
    unsigned n[8];
    int rows = 0;

    memset(c, 0, sizeof(*c));
    c->on = 1;
    c->back = (uint8_t)(CtrBottomCard_OnBack() != 0);
    c->pressed = (uint8_t)(pressed != 0);
    CardLinkFill(c);
    CtrBottomCard_Name(c->name, sizeof(c->name));
    for (int i = 0; i < 6; ++i)
        CtrBottomCard_Label(i, c->label[i], sizeof(c->label[i]));
    CtrBottomCard_Numbers(n);
    c->id = n[0];
    c->money = n[1];
    c->hasDex = (uint8_t)n[2];
    c->caught = (uint16_t)n[3];
    c->hours = (uint16_t)n[4];
    c->minutes = (uint8_t)n[5];
    c->stars = (uint8_t)n[6];
    c->badges = (uint8_t)n[7];
    while (rows < PT_CARD_ROWS && CtrBottomCard_BackRow(rows, c->row[rows].label, sizeof(c->row[rows].label),
                                                         c->row[rows].value, sizeof(c->row[rows].value)))
        ++rows;
    c->rows = (uint8_t)rows;
}
