/* The alternative player looks the pack has: the list (3ds_looks.h), the choice for Red, Leaf and Blue, and the preview's picture. */
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
    char credit[CTR_LOOK_CREDIT_LEN];
} sLooks[CTR_LOOKS_MAX];
static int sCount;
static bool sRead;

/* The list, read once the pack is open (before that, the options show none and ask again). A line is
 * "folder<TAB>name<TAB>gender<TAB>credit"; one without a name, with a folder or a name too long, or without the gender's tab
 * is left out, and a missing credit is empty. */
static void Read(void)
{
    uint32_t size = 0;
    uint8_t *data;
    char *text, *line, *end;

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
    text = malloc(size + 1);
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
        char *tab1, *tab2, *tab3;

        end = strchr(line, '\n');
        if (end)
            *end = 0;
        tab1 = strchr(line, '\t');
        tab2 = tab1 ? strchr(tab1 + 1, '\t') : NULL;
        if (tab1 && tab2)
        {
            tab3 = strchr(tab2 + 1, '\t');
            *tab1 = 0;
            *tab2 = 0;
            if (tab3)
                *tab3 = 0;
            if (line[0] && strlen(line) < CTR_LOOK_ID_LEN && tab1[1] && strlen(tab1 + 1) < CTR_LOOK_NAME_LEN)
            {
                snprintf(sLooks[sCount].id, sizeof(sLooks[sCount].id), "%s", line);
                snprintf(sLooks[sCount].name, sizeof(sLooks[sCount].name), "%s", tab1 + 1);
                snprintf(sLooks[sCount].credit, sizeof(sLooks[sCount].credit), "%s", tab3 ? tab3 + 1 : "");
                ++sCount;
            }
        }
        if (!end)
            break;
    }
    free(text);
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

const char *CtrLooks_Credit(int index)
{
    Read();
    return index >= 0 && index < sCount ? sLooks[index].credit : NULL;
}

/* The preview's picture, read once for the look shown and kept while it is (the options ask for it every frame). */
int CtrLooks_Walk(int index, uint8_t tiles[CTR_LOOK_WALK_BYTES], uint16_t palette[16])
{
    static int shown = -1;
    static bool ok;
    static uint8_t keptTiles[CTR_LOOK_WALK_BYTES];
    static uint16_t keptPalette[16];

    Read();
    if (index < 0 || index >= sCount)
        return 0;
    if (shown != index)
    {
        char path[64];
        uint32_t size = 0;
        uint8_t *entry;

        shown = index;
        ok = false;
        snprintf(path, sizeof(path), "graphics/gen4/look/%s/walk.bin", sLooks[index].id);
        entry = CtrData_Load(path, &size);
        if (entry)
        {
            /* G4N1, the frame count, 32, two bytes, 16 colours, then the frames (the first faces down). */
            if (size >= 40 + CTR_LOOK_WALK_BYTES && memcmp(entry, "G4N1", 4) == 0 && entry[4] >= 1 && entry[5] == 32)
            {
                memcpy(keptTiles, entry + 40, sizeof(keptTiles));
                for (int i = 0; i < 16; ++i)
                    keptPalette[i] = (uint16_t)(entry[8 + i * 2] | entry[9 + i * 2] << 8);
                ok = true;
            }
            free(entry);
        }
    }
    if (!ok)
        return 0;
    memcpy(tiles, keptTiles, sizeof(keptTiles));
    memcpy(palette, keptPalette, sizeof(keptPalette));
    return 1;
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
