/* The alternative player looks the pack has: the list (3ds_looks.h) and the choice for Red, Leaf and Blue. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "3ds_battle_gen4.h"
#include "3ds_data.h"
#include "3ds_looks.h"
#include "3ds_settings.h"

static struct {
    char id[CTR_LOOK_ID_LEN];
    char name[CTR_LOOK_NAME_LEN];
} sLooks[CTR_LOOKS_MAX];
static int sCount;
static bool sRead;

/* The list, read once the pack is open (before that, the options show none and ask again). */
static void Read(void)
{
    uint32_t size = 0;
    uint8_t *data;
    char *line, *end;

    if (sRead || CtrData_GetBackend() == CTR_DATA_NONE)
        return;
    sRead = true;
    data = CtrData_Load("graphics/gen4/look/index.bin", &size);
    if (!data)
        return;
    if (size < 5 || memcmp(data, "G4L1\n", 5) != 0)
    {
        free(data);
        return;
    }
    {
        char *text = malloc(size + 1);

        if (!text)
        {
            free(data);
            return;
        }
        memcpy(text, data, size);
        text[size] = 0;
        free(data);
        for (line = text + 5; *line && sCount < CTR_LOOKS_MAX; line = end ? end + 1 : line + strlen(line))
        {
            char *tab1, *tab2;

            end = strchr(line, '\n');
            if (end)
                *end = 0;
            tab1 = strchr(line, '\t');
            tab2 = tab1 ? strchr(tab1 + 1, '\t') : NULL;
            if (tab1 && tab2)
            {
                *tab1 = 0;
                *tab2 = 0;
                if (line[0] && strlen(line) < CTR_LOOK_ID_LEN && tab1[1] && strlen(tab1 + 1) < CTR_LOOK_NAME_LEN)
                {
                    snprintf(sLooks[sCount].id, sizeof(sLooks[sCount].id), "%s", line);
                    snprintf(sLooks[sCount].name, sizeof(sLooks[sCount].name), "%s", tab1 + 1);
                    ++sCount;
                }
            }
            if (!end)
                break;
        }
        free(text);
    }
}

int CtrLooks_Count(void)
{
    Read();
    return sCount;
}

const char *CtrLooks_Id(int index)
{
    Read();
    return index >= 0 && index < sCount ? sLooks[index].id : NULL;
}

const char *CtrLooks_Name(int index)
{
    Read();
    return index >= 0 && index < sCount ? sLooks[index].name : NULL;
}

int CtrLooks_Chosen(int slot)
{
    const char *id = CtrSettings_Look(slot);

    Read();
    if (!id || !id[0])
        return -1;
    for (int i = 0; i < sCount; ++i)
        if (strcmp(sLooks[i].id, id) == 0)
            return i;
    return -1;
}

void CtrLooks_Choose(int slot, int index)
{
    Read();
    CtrSettings_SetLook(slot, index >= 0 && index < sCount ? sLooks[index].id : "");
    CtrGen4_LooksChanged();
}
