/* The letter being read as the lower screen's own view (included by 3ds_bottom.c after RelearnSnapshot; drawn by 3ds_field_mail_impl.h).
 * Its data is recipe 0243's: the stationery's name and the writer. The item (for the icon) is the one of the mail items whose name that is:
 * FireRed's twelve, ITEM_ORANGE_MAIL..ITEM_RETRO_MAIL, are consecutive. BACK and a touch of the letter close it as before (ProcessTouch). */

#define MAIL_FIRST_ITEM 121u
#define MAIL_ITEM_COUNT 12u

static unsigned MailItemOf(const char *name)
{
    static char sKey[PT_BAR_LEN];
    static unsigned sItem;
    char probe[PT_BAR_LEN], cased[PT_BAR_LEN];

    if (strcmp(sKey, name) == 0)
        return sItem;
    snprintf(cased, sizeof(cased), "%s", name);
    CtrTextCase(cased);
    sItem = 0;
    for (unsigned i = 0; i < MAIL_ITEM_COUNT && !sItem; ++i) {
        CtrBottomBag_ItemName(MAIL_FIRST_ITEM + i, probe, sizeof(probe));
        if (strcmp(probe, cased) == 0)
            sItem = MAIL_FIRST_ITEM + i;
    }
    snprintf(sKey, sizeof(sKey), "%s", name);
    return sItem;
}

static void MailSnapshot(Snapshot *s, int mode)
{
    CtrPokeTouchMail *m = &s->touch.mail;
    char name[PT_BAR_LEN];

    memset(m, 0, sizeof(*m));
    m->on = 1;
    if (CtrBottomView_MailName(name, sizeof(name))) {
        snprintf(m->name, sizeof(m->name), "%s", name);
        m->item = (uint16_t)MailItemOf(name);
    } else
        snprintf(m->name, sizeof(m->name), "%s", GbaViewTitle());
    CtrBottomView_MailFrom(m->from, sizeof(m->from));
    s->touch.viewport = 0;
    s->touch.backVisible = 1;
    s->touch.backEnabled = (uint8_t)BackEnabled(mode);
    s->touch.backPressed = (uint8_t)(sBack.inside != 0);
}
