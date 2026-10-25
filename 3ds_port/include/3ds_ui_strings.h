#ifndef CTR_UI_STRINGS_H
#define CTR_UI_STRINGS_H

/* The lower screen's own words (UTF-8, in the game's capitals where FireRed writes capitals), kept together so that a
 * translation replaces this table and nothing else. Text that comes from FireRed (item names, menus, messages) is the
 * game's own and is not here. The buttons leave room for longer words than these. */
typedef enum {
    UIS_BAG_ITEMS,
    UIS_BAG_KEY_ITEMS,
    UIS_BAG_BALLS,
    UIS_BAG_CLOSE,
    UIS_BAG_TOSS_HOW_MANY,
    UIS_BAG_EMPTY,
    UIS_YES,
    UIS_NO,
    UIS_OK,
    UIS_CANCEL,
    UIS_TAP_TO_CONTINUE,
    UIS_COUNT
} CtrUiString;

static const char *const kCtrUiStrings[UIS_COUNT] = {
    [UIS_BAG_ITEMS] = "ITEMS",
    [UIS_BAG_KEY_ITEMS] = "KEY ITEMS",
    [UIS_BAG_BALLS] = "POK\xC3\xA9 BALLS",
    [UIS_BAG_CLOSE] = "CLOSE BAG",
    [UIS_BAG_TOSS_HOW_MANY] = "Toss out how many?",
    [UIS_BAG_EMPTY] = "There is nothing here.",
    [UIS_YES] = "YES",
    [UIS_NO] = "NO",
    [UIS_OK] = "OK",
    [UIS_CANCEL] = "CANCEL",
    [UIS_TAP_TO_CONTINUE] = "Tap to continue",
};

static inline const char *CtrUiText(CtrUiString id)
{
    return id < UIS_COUNT ? kCtrUiStrings[id] : "";
}

#endif
