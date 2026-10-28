/* See 3ds_settings.h. */
#define _POSIX_C_SOURCE 200809L   /* fileno, fsync */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "3ds_settings.h"

#ifdef __3DS__
#include <3ds.h>
#include "3ds_log.h"
#endif

static unsigned sTurbo;
static int sAutoRun;
static int sEmbers = -1;
static int sLoaded;

/* The file stays open from the first read to the exit and is rewritten in place, padded to a fixed size: creating or
 * truncating it on the SD card is what took 3.4 to 6.9 s (0.2.6 and 0.2.7 Old 3DS port.log, one freeze per START that
 * cycled the turbo), and the game froze for the whole of it even with the write on a thread of its own. A missing file
 * is created here, on the first read, which happens while the game starts. */
#define SETTINGS_FILE_BYTES 64
static FILE *sFile;

static void Load(void)
{
    char line[64];
    FILE *file;

    if (sLoaded)
        return;
    sLoaded = 1;
    sFile = fopen(CTR_SETTINGS_PATH, "r+b");
    if (sFile == NULL && errno == ENOENT)   /* only a missing file is created: any other failure keeps the settings */
        sFile = fopen(CTR_SETTINGS_PATH, "w+b");
    file = sFile;
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
}

static void WriteText(const char *text)
{
    char block[SETTINGS_FILE_BYTES];
    size_t length = strlen(text);

    if (sFile == NULL)
        return;
    if (length > sizeof(block))
        length = sizeof(block);
    memset(block, '\n', sizeof(block));   /* blank lines, which the reader skips */
    memcpy(block, text, length);
    if (fseek(sFile, 0, SEEK_SET) == 0)
        fwrite(block, 1, sizeof(block), sFile);
    fflush(sFile);
    fsync(fileno(sFile));   /* committed now (FSFILE_Flush on the SD card), not only when the file is closed at exit */
}

static void Format(char *text, size_t size)
{
    int length = snprintf(text, size, "turbo=%u\nautorun=%d\n", sTurbo, sAutoRun);

    if (sEmbers >= 0 && length > 0 && (size_t)length < size)
        snprintf(text + length, size - (size_t)length, "embers=%d\n", sEmbers);
}

#ifdef __3DS__
/* The text is worked out at once and written by a thread of its own, half a second later so a run of presses is one
 * write; what is still waiting is written on exit. Each write logs how long it took. */
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
    {
        u64 start = osGetTime();
        WriteText(text);
        CtrLog_Write(CTR_LOG_FS, "settings: written in place in %lu ms", (unsigned long)(osGetTime() - start));
    }
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
    if (sFile != NULL)
        fclose(sFile);
    sFile = NULL;
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

        CtrLog_Write(CTR_LOG_FS, "settings: no writer thread, written on the game thread");

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
