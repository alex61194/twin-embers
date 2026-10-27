/* See 3ds_settings.h. */
#include <stdio.h>
#include <string.h>

#include "3ds_settings.h"

static unsigned sTurbo;
static int sAutoRun;
static int sEmbers = -1;
static int sLoaded;

static void Load(void)
{
    char line[64];
    FILE *file;

    if (sLoaded)
        return;
    sLoaded = 1;
    file = fopen(CTR_SETTINGS_PATH, "rb");
    if (file == NULL)
        return;
    while (fgets(line, sizeof(line), file) != NULL)
    {
        unsigned value;

        if (sscanf(line, "turbo=%u", &value) == 1)
            sTurbo = value == 0 || (value >= 2 && value <= 4) ? value : 0;
        else if (sscanf(line, "autorun=%u", &value) == 1)
            sAutoRun = value != 0;
        else if (sscanf(line, "embers=%u", &value) == 1)
            sEmbers = value <= 2 ? (int)value : -1;
    }
    fclose(file);
}

static void Save(void)
{
    FILE *file = fopen(CTR_SETTINGS_PATH, "wb");

    if (file == NULL)
        return;
    fprintf(file, "turbo=%u\nautorun=%d\n", sTurbo, sAutoRun);
    if (sEmbers >= 0)
        fprintf(file, "embers=%d\n", sEmbers);
    fclose(file);
}

unsigned CtrSettings_Turbo(void)
{
    Load();
    return sTurbo;
}

void CtrSettings_SetTurbo(unsigned turbo)
{
    Load();
    if (turbo != 0 && (turbo < 2 || turbo > 4))
        turbo = 0;
    if (turbo == sTurbo)
        return;
    sTurbo = turbo;
    Save();
}

int CtrSettings_AutoRun(void)
{
    Load();
    return sAutoRun;
}

void CtrSettings_SetAutoRun(int on)
{
    Load();
    on = on != 0;
    if (on == sAutoRun)
        return;
    sAutoRun = on;
    Save();
}

int CtrSettings_Embers(void)
{
    Load();
    return sEmbers;
}

void CtrSettings_SetEmbers(int level)
{
    Load();
    if (level < 0 || level > 2 || level == sEmbers)
        return;
    sEmbers = level;
    Save();
}
