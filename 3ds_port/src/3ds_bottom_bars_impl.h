/* The bars under the Party and the Summary screens (included by 3ds_bottom.c after the Snapshot). */

/* The bar under the Party screen: the Pokemon under the cursor. */
static void PartyBar(const Snapshot *s, CtrPokeTouchView *v)
{
    /* GetMonAilment's AILMENT_* (constants/party_menu.h). */
    static const char *const sStatus[] = {"", CTR_TR("Poisoned", "Envenenado", "Vergiftet"), CTR_TR("Paralyzed", "Paralizado", "Paralysiert"), CTR_TR("Asleep", "Dormido", "Schl\xC3\xA4" "ft"), CTR_TR("Frozen", "Congelado", "Eingefroren"), CTR_TR("Burned", "Quemado", "Verbrannt"),
                                          "Pok\xc3\xa9rus", CTR_TR("Fainted", "Debilitado", "Besiegt")};
    int slot = s->cursor;

    if (slot < 0 || slot >= PARTY_SIZE || !((s->presentMask >> slot) & 1)) {
        snprintf(v->bar[0], PT_BAR_LEN, CTR_TR("Choose a Pok\xc3\xa9mon.", "Elige un Pok\xc3\xa9mon.", "W\xC3\xA4hle ein Pok\xC3\xA9mon."));
        return;
    }
    if (s->isEgg[slot]) {
        snprintf(v->bar[0], PT_BAR_LEN, CTR_TR("Egg", "Huevo", "Ei"));
        return;
    }
    snprintf(v->bar[0], PT_BAR_LEN, "%s  Lv%u", s->nick[slot], s->level[slot]);
    snprintf(v->bar[1], PT_BAR_LEN, CTR_TR("HP %u/%u", "PS %u/%u", "KP %u/%u"), s->hp[slot], s->maxHp[slot]);
    if (s->hp[slot] == 0)
        snprintf(v->bar[2], PT_BAR_LEN, CTR_TR("Fainted", "Debilitado", "Besiegt"));
    else if (s->status[slot] < sizeof(sStatus) / sizeof(sStatus[0]))
        snprintf(v->bar[2], PT_BAR_LEN, "%s", sStatus[s->status[slot]]);
}

/* The bar under the Summary screen: the Pokemon it shows. */
static void SummaryBar(const CtrSummaryView *v, CtrPokeTouchView *t)
{
    /* The bridge's status codes are English in every language. */
    static const char *const sCodes[] = {"PSN", "PRZ", "SLP", "FRZ", "BRN", "PKR", "FNT"};
    static const char *const sNames[] = {CTR_TR("Poisoned", "Envenenado", "Vergiftet"), CTR_TR("Paralyzed", "Paralizado", "Paralysiert"), CTR_TR("Asleep", "Dormido", "Schl\xC3\xA4" "ft"), CTR_TR("Frozen", "Congelado", "Eingefroren"), CTR_TR("Burned", "Quemado", "Verbrannt"),
                                         "Pok\xc3\xa9rus", CTR_TR("Fainted", "Debilitado", "Besiegt")};

    if (!v->species[0])
        return;
    if (v->egg) {
        snprintf(t->bar[0], PT_BAR_LEN, CTR_TR("Egg", "Huevo", "Ei"));
        return;
    }
    const char *level = v->level;
    while (*level == ' ')       /* the Summary's level is right-aligned */
        ++level;
    snprintf(t->bar[0], PT_BAR_LEN, "%s  Lv%s", v->nickname, level);
    snprintf(t->bar[1], PT_BAR_LEN, "No.%s %s", v->dex, v->species);
    for (unsigned i = 0; i < sizeof(sCodes) / sizeof(sCodes[0]); ++i)
        if (CtrTextCase_Eq(v->status, sCodes[i]))
            snprintf(t->bar[2], PT_BAR_LEN, "%s", sNames[i]);
}
