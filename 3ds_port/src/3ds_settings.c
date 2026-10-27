/* See 3ds_settings.h. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "3ds_settings.h"

#ifdef __3DS__
#include <3ds.h>
#endif

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

static void WriteText(const char *text)
{
    FILE *file = fopen(CTR_SETTINGS_PATH, "wb");

    if (file == NULL)
        return;
    fputs(text, file);
    fclose(file);
}

static void Format(char *text, size_t size)
{
    int length = snprintf(text, size, "turbo=%u\nautorun=%d\n", sTurbo, sAutoRun);

    if (sEmbers >= 0 && length > 0 && (size_t)length < size)
        snprintf(text + length, size - (size_t)length, "embers=%d\n", sEmbers);
}

#ifdef __3DS__
/* Creating the file on the SD card stalled the game thread for 3.4 s a time (the 0.2.6 Old 3DS port.log: each START
 * that cycled the turbo, each EMBERS change in OPTIONS). The text is worked out at once and written by a thread of
 * its own, half a second later so a run of presses is one write; what is still waiting is written on exit. */
#define SETTINGS_DELAY_NS 500000000LL
static LightEvent sWake;
static LightLock sLock;
static Thread sThread;
static char sPending[96];
static int sHavePending, sStarted;
static volatile int sStop;

static void FlushPending(void)
{
    char text[sizeof(sPending)];
    int have;

    LightLock_Lock(&sLock);
    have = sHavePending;
    memcpy(text, sPending, sizeof(text));
    sHavePending = 0;
    LightLock_Unlock(&sLock);
    if (have)
        WriteText(text);
}

static void WorkerMain(void *arg)
{
    (void)arg;
    while (!sStop)
    {
        LightEvent_Wait(&sWake);
        if (!sStop)
            svcSleepThread(SETTINGS_DELAY_NS);
        FlushPending();
    }
}

/* The thread's stack is heap memory, which libctru unmaps at exit: stop it first, then write what is left. */
static void Shutdown(void)
{
    sStop = 1;
    LightEvent_Signal(&sWake);
    threadJoin(sThread, U64_MAX);
    threadFree(sThread);
    sThread = NULL;
    FlushPending();
}

static void Save(void)
{
    if (!sStarted)
    {
        sStarted = 1;
        LightLock_Init(&sLock);
        LightEvent_Init(&sWake, RESET_ONESHOT);
        sThread = threadCreate(WorkerMain, NULL, 8 * 1024, 0x3F, -2, false);
        if (sThread != NULL)
            atexit(Shutdown);
    }
    if (sThread == NULL)
    {
        char text[sizeof(sPending)];

        Format(text, sizeof(text));
        WriteText(text);
        return;
    }
    LightLock_Lock(&sLock);
    Format(sPending, sizeof(sPending));
    sHavePending = 1;
    LightLock_Unlock(&sLock);
    LightEvent_Signal(&sWake);
}
#else
static void Save(void)
{
    char text[96];

    Format(text, sizeof(text));
    WriteText(text);
}
#endif

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
