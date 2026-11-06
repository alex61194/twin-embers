/* The list of the alternative looks (src/3ds_looks.c) on a synthetic index entry, and the choice kept in the settings. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "3ds_battle_gen4.h"
#include "3ds_data.h"
#include "3ds_looks.h"
#include "3ds_settings.h"

static CtrDataBackend sBackend = CTR_DATA_NONE;
static const char *sIndex;
static unsigned sLoads, sChanged;

CtrDataBackend CtrData_GetBackend(void)
{
    return sBackend;
}

void *CtrData_Load(const char *path, uint32_t *outSize)
{
    char *copy;

    ++sLoads;
    if (strcmp(path, "graphics/gen4/look/index.bin") != 0 || !sIndex)
        return NULL;
    copy = malloc(strlen(sIndex));
    memcpy(copy, sIndex, strlen(sIndex));
    *outSize = (uint32_t)strlen(sIndex);
    return copy;
}

void CtrGen4_LooksChanged(void)
{
    ++sChanged;
}

int main(void)
{
    remove(CTR_SETTINGS_PATH);
    /* Before the pack is open there are none, and the entry is not asked for. */
    assert(CtrLooks_Count() == 0 && CtrLooks_Id(0) == NULL && sLoads == 0);
    assert(CtrLooks_Chosen(CTR_LOOK_RED) == -1);

    /* Open: the list is read once, in the entry's order; a bad line (no name, a name that is too long) is left out. */
    sBackend = CTR_DATA_PAK;
    sIndex = "G4L1\nash\tASH\tboy\nash_hgss\tASH HGSS\tboy\nbroken\nno_name\t\tboy\ntoo_long_name_x\tTWELVE CHARS\tgirl\ngreen\tGREEN\tgirl\n";
    assert(CtrLooks_Count() == 3 && sLoads == 1);
    assert(strcmp(CtrLooks_Id(0), "ash") == 0 && strcmp(CtrLooks_Name(1), "ASH HGSS") == 0);
    assert(strcmp(CtrLooks_Id(2), "green") == 0 && CtrLooks_Id(3) == NULL && CtrLooks_Name(-1) == NULL);
    assert(CtrLooks_Count() == 3 && sLoads == 1);

    /* The choice: kept in the settings by folder name, found again by it, and each change tells the pictures. */
    assert(CtrLooks_Chosen(CTR_LOOK_LEAF) == -1);
    CtrLooks_Choose(CTR_LOOK_LEAF, 2);
    assert(CtrLooks_Chosen(CTR_LOOK_LEAF) == 1 + 1 && strcmp(CtrSettings_Look(CTR_LOOK_LEAF), "green") == 0 && sChanged == 1);
    CtrLooks_Choose(CTR_LOOK_BLUE, 0);
    assert(CtrLooks_Chosen(CTR_LOOK_BLUE) == 0 && sChanged == 2);
    CtrLooks_Choose(CTR_LOOK_BLUE, -1);
    assert(CtrLooks_Chosen(CTR_LOOK_BLUE) == -1 && CtrSettings_Look(CTR_LOOK_BLUE)[0] == 0 && sChanged == 3);
    CtrLooks_Choose(CTR_LOOK_RED, 9);                                      /* not a look the pack has: none */
    assert(CtrLooks_Chosen(CTR_LOOK_RED) == -1);
    /* A look the settings name but the pack does not have is none. */
    CtrSettings_SetLook(CTR_LOOK_RED, "gone");
    assert(CtrLooks_Chosen(CTR_LOOK_RED) == -1 && strcmp(CtrSettings_Look(CTR_LOOK_RED), "gone") == 0);

    remove(CTR_SETTINGS_PATH);
    puts("looks_test ok");
    return 0;
}
