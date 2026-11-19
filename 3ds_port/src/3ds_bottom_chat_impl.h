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
