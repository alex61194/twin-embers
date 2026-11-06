#ifndef CTR_LOOKS_H
#define CTR_LOOKS_H

/* The alternative player looks the pack has (the Builder's --alt-looks): the list is the pack's graphics/gen4/look/index.bin
 * (b"G4L1", then a line "folder<TAB>name<TAB>boy or girl" for each), read the first time it is asked for once the pack is
 * open. The OPTIONS' look rows (3ds_poketouch_options.c) list them and keep the choice for Red, Leaf and the rival Blue in
 * the settings (CtrSettings_Look); the pictures themselves are 3ds_battle_gen4.c's. */
#define CTR_LOOKS_MAX 24
#define CTR_LOOK_ID_LEN 16          /* a folder's name and its end (CtrSettings_Look keeps 15 characters) */
#define CTR_LOOK_NAME_LEN 12        /* the name the options show, at most 11 characters and its end */

/* How many looks the pack has: none without the Builder's alternative looks (or before the pack is open). */
int CtrLooks_Count(void);
/* The look's folder name and the name the options show (index 0 to CtrLooks_Count() - 1), NULL out of range. */
const char *CtrLooks_Id(int index);
const char *CtrLooks_Name(int index);
/* The index of the look chosen for the slot (CTR_LOOK_*), or -1 for none (or one the pack does not have). */
int CtrLooks_Chosen(int slot);
/* Choose the look of `index` for the slot, or none with -1: kept in the settings, and the pictures already read are dropped. */
void CtrLooks_Choose(int slot, int index);

#endif
