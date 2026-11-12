#ifndef CTR_TEXT_CASE_VIEWS_H
#define CTR_TEXT_CASE_VIEWS_H

#include "3ds_text_case.h"
#include "3ds_poketouch.h"
#include "3ds_summary.h"

/*
 * The names FireRed's text gives the lower screen's views, in proper case (3ds_text_case.h). Nicknames, the player's and
 * the rival's names, a mail's writer and the box names are what the player typed and stay as they are; the Bag's item
 * names (and a case's moves) are done where they are read (3ds_bottom.c, CtrBottomBag_ItemName), because a mailbox's
 * rows share the Bag's list and hold the writers' names.
 */
#define CTR_CASE(f) CtrTextCase(f)
#define CTR_CASE_ALL(a) do { for (size_t i_ = 0; i_ < sizeof(a) / sizeof((a)[0]); ++i_) CtrTextCase((a)[i_]); } while (0)

static inline void CtrTextCase_Summary(CtrSummaryView *v)
{
    CTR_CASE(v->species);
    CTR_CASE(v->item);
    CTR_CASE_ALL(v->types);
    CTR_CASE(v->ability);
    CTR_CASE(v->nature);
    CTR_CASE_ALL(v->moveNames);
    CTR_CASE_ALL(v->moveTypes);
    CTR_CASE(v->hpTypeName);
}

/* The field's views (the Bag's rows and selected name are left, see above). */
static inline void CtrTextCase_Touch(CtrPokeTouchView *v)
{
    CtrPokeTouchBag *b = &v->bag;
    CtrPokeTouchDex *d = &v->dex;

    CTR_CASE_ALL(v->bar);
    CTR_CASE(b->title);
    CTR_CASE(b->closeName);
    CTR_CASE(b->typeName);
    CTR_CASE_ALL(b->menuName);
    for (int i = 0; i < PT_DEX_MENU_MAX; ++i)
        CTR_CASE(d->menu[i].name);
    for (int i = 0; i < PT_DEX_ROWS; ++i)
        CTR_CASE(d->row[i].name);
    for (int i = 0; i < 4; ++i)
        CTR_CASE(d->mon[i].name);
    CTR_CASE(d->sel.name);
    CTR_CASE(d->kind);
    CTR_CASE_ALL(d->typeName);
    for (int i = 0; i < PT_LEARN_ROWS; ++i) {
        CTR_CASE(v->learn.row[i].name);
        CTR_CASE(v->learn.row[i].typeName);
    }
    /* The move relearner: its moves (and CANCEL), their types, the question and the description; the Pokemon is a nickname. */
    CTR_CASE(v->relearn.question);
    CTR_CASE(v->relearn.desc);
    for (int i = 0; i < PT_RL_ROWS; ++i) {
        CTR_CASE(v->relearn.row[i].name);
        CTR_CASE(v->relearn.row[i].typeName);
    }
    for (int i = 0; i < 6; ++i)
        CTR_CASE(v->card.label[i]);
    for (int i = 0; i < PT_CARD_ROWS; ++i)
        CTR_CASE(v->card.row[i].label);
    for (int i = 0; i < PT_CHOICE_MAX; ++i)
        CTR_CASE(v->choice.label[i]);
}

static inline void CtrTextCase_Battle(CtrBattleView *v)
{
    for (int i = 0; i < 4; ++i) {
        CTR_CASE(v->move[i].name);
        CTR_CASE(v->move[i].typeName);
    }
    CTR_CASE_ALL(v->activeTypeName);
    CTR_CASE_ALL(v->bag.menuName);
    CTR_CASE_ALL(v->pty.menuName);
}

#undef CTR_CASE
#undef CTR_CASE_ALL

#endif
