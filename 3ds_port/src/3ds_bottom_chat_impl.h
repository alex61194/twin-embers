/* The Union Room chat on the lower screen (included by 3ds_bottom.c). While the chat waits for input the lower screen shows its keyboard
 * (3ds_field_chat_impl.h): a tap on a key, Delete, Send or a page tab is handed to the game as a command (CtrChat_Tap*, recipe 0276), which
 * does what the D-pad and buttons would; Quit presses B, which asks FireRed's own question (only with nothing typed: B deletes otherwise). */

extern int CtrChat_Active(void);
extern int CtrChat_PageNow(void);
extern int CtrChat_Length(void);
extern void CtrChat_Message(uint8_t *out);
extern int CtrChat_KeyBytes(int page, int row, int col, uint8_t *out);
extern void CtrChat_Registered(int row, uint8_t *out);
extern void CtrChat_TapKey(int col, int row);
extern void CtrChat_TapDelete(void);
extern void CtrChat_TapSend(void);
extern void CtrChat_TapPage(int page);

static void ChatFill(CtrPokeTouchView *v)
{
    CtrPokeTouchChat *t = &v->chat;

    memset(t, 0xff, sizeof(*t));
    t->on = 1;
    t->page = (uint8_t)CtrChat_PageNow();
    t->length = (uint8_t)CtrChat_Length();
    CtrChat_Message(t->msg);
    for (int row = 0; row < 10; ++row) {
        if (t->page == 3) {
            CtrChat_Registered(row, t->reg[row]);
            if (t->reg[row][0] == 0xff || t->reg[row][0] == 0)
                t->reg[row][0] = 0xff;
        } else {
            for (int col = 0; col < 5; ++col) {
                uint8_t b[2] = {0xff, 0xff};
                int n = CtrChat_KeyBytes(t->page, row, col, b);

                t->key[row][col][0] = n > 0 ? b[0] : 0xff;
                t->key[row][col][1] = n > 1 ? b[1] : 0xff;
                t->key[row][col][2] = 0xff;
            }
        }
    }
    v->choice.count = 0;
    snprintf(v->choice.prompt, sizeof(v->choice.prompt), "%s", CTR_TR("Chat with the others in the room.", "Chatea con los de la sala.",
                                                                      "Chatte mit den anderen im Raum."));
}

static uint16_t ChatTap(int code)
{
    int k = code - PT_CHOICE_HIT_ROW;

    if (k >= 0 && k < 50) {
        CtrChat_TapKey(k % 5, k / 5);
        return 0;
    }
    if (k >= PT_CHAT_TAB && k < PT_CHAT_TAB + PT_CHAT_PAGES) {
        CtrChat_TapPage(k - PT_CHAT_TAB);
        return 0;
    }
    if (k == PT_CHAT_DELETE)
        CtrChat_TapDelete();
    else if (k == PT_CHAT_SEND)
        CtrChat_TapSend();
    else if (k == PT_CHAT_QUIT && CtrChat_Length() == 0)
        return K_B;
    return 0;
}

/* The wireless screens (the leader's wait, the trade's cards, the chat's keyboard) have gaps of a few frames while the game waits for the
 * link, with none of them up: the lower screen went to the title's logo and came back (blank embers first). The mode and the last picture
 * are held across a gap of up to WL_HOLD_FRAMES frames; a choice of the game's own (a menu, its rows) ends the hold. */
#define WL_HOLD_FRAMES 45

static int sWlHold;

extern int CtrTrade_InMenu(void);
extern void CtrWl_Trace(const char *what, int a, int b, int c, int d, int e);

/* Written to the net log when it changes: the choice open, the hold left, the leader's wait, the trade (active, in its menu, the cursor), the chat. */
static void WlTrace(int open)
{
    static int last[6] = {-9, -9, -9, -9, -9, -9}, lines;
    int now[6] = {open, sWlHold, CtrUnionLeader_Active(), CtrTrade_Active() | CtrTrade_InMenu() << 1, CtrTrade_Cursor(), CtrChat_Active()};

    if (!CtrUnionLeader_Active() && !CtrTrade_InMenu() && !CtrChat_Active() && last[2] == 0 && last[3] == 0 && last[5] == 0)
        return;                             /* no wireless screen now or just before: nothing to say */
    if (memcmp(last, now, sizeof(last)) == 0)
        return;
    memcpy(last, now, sizeof(last));
    if (lines++ >= 400)
        return;                             /* a flicker every frame would fill the log the link lines share */
    CtrWl_Trace("open/hold/leader/trade/cursor/chat", now[0], now[1], now[2], now[3] * 100 + now[4], now[5]);
}

/* A field choice (the Cable Club's rooms, its YES/NO, the Union Room's) is reported closed for a frame now and then while the link runs
 * (the game redraws the window or changes its callback for a frame), and the lower screen showed the map for that frame in the middle of
 * the choice. The choice stays up for WL_BLIP_FRAMES frames after the game last had it open. */
#define WL_BLIP_FRAMES 4
static int sWlBlip;

/* `mode` (the field) unless the choice that was up a frame ago is only blipping. */
static int WlBlipMode(int mode)
{
    static unsigned long lastFrame = (unsigned long)-1;
    unsigned long frame;

    if (sWlBlip <= 0 || !sHaveShown || sShown.mode != MODE_CHOICE || CtrBattle_InBattle())
        return mode;
    frame = (unsigned long)CtrVideo_GetStats()->frames;
    if (frame != lastFrame) {
        lastFrame = frame;
        --sWlBlip;
    }
    return MODE_CHOICE;
}

static int WlHoldMode(int open)
{
    WlTrace(open);
    if (open) {
        sWlBlip = WL_BLIP_FRAMES;
        if (CtrUnionLeader_Active() || CtrTrade_Active() || CtrChat_Active())
            sWlHold = WL_HOLD_FRAMES;
        else
            sWlHold = 0;
        return MODE_CHOICE;
    }
    if (sWlHold > 0 && sHaveShown && sShown.mode == MODE_CHOICE) {
        /* CurrentMode runs several times in a frame: the hold counts frames, not calls (counted per call it ran out in about a
         * fifth of its time, and the upper and lower screens flashed through the gap, worst online where the gaps are longer). */
        static unsigned long lastFrame = (unsigned long)-1;
        unsigned long frame = (unsigned long)CtrVideo_GetStats()->frames;

        if (frame != lastFrame) {
            lastFrame = frame;
            --sWlHold;
        }
        return MODE_CHOICE;
    }
    sWlHold = 0;
    return MODE_OFF;
}

/* The held frame of the snapshot: what was shown last, taps ignored (the game's choice is not open, so nothing is ready). */
static void WlHoldSnapshot(Snapshot *s, int rows)
{
    if (rows == 0 && (sWlHold > 0 || sWlBlip > 0) && sHaveShown && sShown.mode == MODE_CHOICE) {
        s->touch.choice = sShown.touch.choice;
        s->touch.trade = sShown.touch.trade;
        s->touch.chat = sShown.touch.chat;
        s->touch.choice.pressed = -1;
    }
}
