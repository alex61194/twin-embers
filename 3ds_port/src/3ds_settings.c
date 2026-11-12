/* See 3ds_settings.h. */
#define _POSIX_C_SOURCE 200809L   /* fileno, fsync */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "3ds_settings.h"

/* __3DS__ comes from <3ds.h> itself, so it cannot choose whether to include it: the 0.2.8 builds tested __3DS__ here,
 * never had the writer thread and wrote settings.cfg on the game thread (5-16 ms per turbo change, no "written in place"
 * line in the Old 3DS port.log). __arm__, as in 3ds_flash.c, is the console; the host tests build the #else. */
#ifdef __arm__
#include <3ds.h>
#include "3ds_log.h"
#endif

static unsigned sTurbo;
static int sAutoRun;
static int sEmbers = -1;
static int sRepelAgain = 1;
static int sCompanion;
static char sLook[3][16];     /* look_red, look_leaf, look_blue */
static int sExpShare;
static int sTmReuse;
static int sNuzlocke;
static int sRandomlocke;
static int sNuzDupes;       /* NUZLOCKE clauses: a species already caught does not use the area's catch */
static int sNuzShiny;       /* a shiny does not use it either */
static int sNuzCap;         /* LEVEL CAPS: no experience at or above the badge's cap */
static int sFreeHm;
static int sVisibleWilds;   /* VISIBLE WILDS (prototype): wild Pokemon walk about on one route */
static unsigned sBall;          /* the ball the battle's ball button last threw (0: none yet) */
static int sLoaded;

/* The file stays open from the first read to the exit and is rewritten in place, padded to a fixed size: creating or
 * truncating it on the SD card is what took 3.4 to 6.9 s (0.2.6 and 0.2.7 Old 3DS port.log, one freeze per START that
 * cycled the turbo), and the game froze for the whole of it even with the write on a thread of its own. A missing file
 * is created here, on the first read, which happens while the game starts. */
#define SETTINGS_FILE_BYTES 512     /* was 256: the file grew to about 232 bytes in the worst case; a shorter old file reads as before */
static FILE *sFile;

static const char *const kLookKey[3] = {"look_red=", "look_leaf=", "look_blue="};

/* A look's name is its folder's: only [a-z0-9_], at most 15 characters, up to the first other one (the line's end). */
static void LookSet(int slot, const char *text)
{
    size_t n = 0;

    while (n < sizeof(sLook[slot]) - 1
           && ((text[n] >= 'a' && text[n] <= 'z') || (text[n] >= '0' && text[n] <= '9') || text[n] == '_'))
    {
        sLook[slot][n] = text[n];
        ++n;
    }
    sLook[slot][n] = 0;
}

static int LookLine(const char *line)
{
    for (int slot = 0; slot < 3; ++slot)
        if (strncmp(line, kLookKey[slot], strlen(kLookKey[slot])) == 0)
        {
            LookSet(slot, line + strlen(kLookKey[slot]));
            return 1;
        }
    return 0;
}

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
        else if (sscanf(line, "repel=%u", &value) == 1)
            sRepelAgain = value != 0;
        else if (sscanf(line, "companion=%u", &value) == 1)
            sCompanion = value <= 2 ? (int)value : 0;
        else if (sscanf(line, "expshare=%u", &value) == 1)
            sExpShare = value <= 2 ? (int)value : 0;
        else if (sscanf(line, "tmreuse=%u", &value) == 1)
            sTmReuse = value != 0;
        else if (sscanf(line, "nuzlocke=%u", &value) == 1)
            sNuzlocke = value != 0;
        else if (sscanf(line, "randomlocke=%u", &value) == 1)
            sRandomlocke = value != 0;
        else if (sscanf(line, "nuzdupes=%u", &value) == 1)
            sNuzDupes = value != 0;
        else if (sscanf(line, "nuzshiny=%u", &value) == 1)
            sNuzShiny = value != 0;
        else if (sscanf(line, "nuzcap=%u", &value) == 1)
            sNuzCap = value != 0;
        else if (sscanf(line, "freehm=%u", &value) == 1)
            sFreeHm = value != 0;
        else if (sscanf(line, "visiblewilds=%u", &value) == 1)
            sVisibleWilds = value != 0;
        else if (LookLine(line))
            ;
        else if (sscanf(line, "ball=%u", &value) == 1)
            sBall = value <= 0xffff ? value : 0;
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
    int length = snprintf(text, size, "turbo=%u\nautorun=%d\nrepel=%d\n", sTurbo, sAutoRun, sRepelAgain);

    if (sEmbers >= 0 && length > 0 && (size_t)length < size)
    {
        length += snprintf(text + length, size - (size_t)length, "embers=%d\n", sEmbers);
    }
    if (sCompanion && length > 0 && (size_t)length < size)
        length += snprintf(text + length, size - (size_t)length, "companion=%d\n", sCompanion);
    if (sExpShare && length > 0 && (size_t)length < size)
        length += snprintf(text + length, size - (size_t)length, "expshare=%d\n", sExpShare);
    if (sTmReuse && length > 0 && (size_t)length < size)
        length += snprintf(text + length, size - (size_t)length, "tmreuse=1\n");
    if (sNuzlocke && length > 0 && (size_t)length < size)
        length += snprintf(text + length, size - (size_t)length, "nuzlocke=1\n");
    if (sRandomlocke && length > 0 && (size_t)length < size)
        length += snprintf(text + length, size - (size_t)length, "randomlocke=1\n");
    if (sNuzDupes && length > 0 && (size_t)length < size)
        length += snprintf(text + length, size - (size_t)length, "nuzdupes=1\n");
    if (sNuzShiny && length > 0 && (size_t)length < size)
        length += snprintf(text + length, size - (size_t)length, "nuzshiny=1\n");
    if (sNuzCap && length > 0 && (size_t)length < size)
        length += snprintf(text + length, size - (size_t)length, "nuzcap=1\n");
    if (sFreeHm && length > 0 && (size_t)length < size)
        length += snprintf(text + length, size - (size_t)length, "freehm=1\n");
    if (sVisibleWilds && length > 0 && (size_t)length < size)
        length += snprintf(text + length, size - (size_t)length, "visiblewilds=1\n");
    for (int slot = 0; slot < 3; ++slot)
        if (sLook[slot][0] && length > 0 && (size_t)length < size)
            length += snprintf(text + length, size - (size_t)length, "%s%s\n", kLookKey[slot], sLook[slot]);
    if (sBall && length > 0 && (size_t)length < size)
        snprintf(text + length, size - (size_t)length, "ball=%u\n", sBall);
}

#ifdef __arm__
/* The text is worked out at once and written by a thread of its own, half a second later so a run of presses is one
 * write; what is still waiting is written on exit. Each write logs how long it took. */
#define SETTINGS_DELAY_NS 500000000LL
static LightEvent sWake;
static LightLock sLock;
static Thread sThread;
static char sPending[SETTINGS_FILE_BYTES];
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
        /* 16 KiB, as the log writer: this thread formats a log line and goes through stdio and the SD card. */
        sThread = threadCreate(WorkerMain, NULL, 16 * 1024, 0x3F, -2, false);
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
    char text[SETTINGS_FILE_BYTES];

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

int CtrSettings_RepelAgain(void)
{
    Load();
    return sRepelAgain;
}

void CtrSettings_SetRepelAgain(int on)
{
    Load();
    on = on != 0;
    if (on == sRepelAgain)
        return;
    sRepelAgain = on;
    Save();
}

int CtrSettings_Companion(void)
{
    Load();
    return sCompanion;
}

void CtrSettings_SetCompanion(int mode)
{
    Load();
    if (mode < 0 || mode > 2 || mode == sCompanion)
        return;
    sCompanion = mode;
    Save();
}

int CtrSettings_ExpShare(void)
{
    Load();
    return sExpShare;
}

void CtrSettings_SetExpShare(int mode)
{
    Load();
    if (mode < 0 || mode > 2 || mode == sExpShare)
        return;
    sExpShare = mode;
    Save();
}

int CtrSettings_TmReuse(void)
{
    Load();
    return sTmReuse;
}

void CtrSettings_SetTmReuse(int on)
{
    Load();
    on = on != 0;
    if (on == sTmReuse)
        return;
    sTmReuse = on;
    Save();
}

int CtrSettings_FreeHm(void)
{
    Load();
    return sFreeHm;
}

void CtrSettings_SetFreeHm(int on)
{
    Load();
    on = on != 0;
    if (on == sFreeHm)
        return;
    sFreeHm = on;
    Save();
}

int CtrSettings_Randomlocke(void)
{
    Load();
    return sRandomlocke;
}

void CtrSettings_SetRandomlocke(int on)
{
    Load();
    on = on != 0;
    if (on == sRandomlocke)
        return;
    sRandomlocke = on;
    Save();
}

int CtrSettings_Nuzlocke(void)
{
    Load();
    return sNuzlocke;
}

void CtrSettings_SetNuzlocke(int on)
{
    Load();
    on = on != 0;
    if (on == sNuzlocke)
        return;
    sNuzlocke = on;
    Save();
}

int CtrSettings_NuzDuplicates(void)
{
    Load();
    return sNuzDupes;
}

void CtrSettings_SetNuzDuplicates(int on)
{
    Load();
    on = on != 0;
    if (on == sNuzDupes)
        return;
    sNuzDupes = on;
    Save();
}

int CtrSettings_NuzShiny(void)
{
    Load();
    return sNuzShiny;
}

void CtrSettings_SetNuzShiny(int on)
{
    Load();
    on = on != 0;
    if (on == sNuzShiny)
        return;
    sNuzShiny = on;
    Save();
}

int CtrSettings_VisibleWilds(void)
{
    Load();
    return sVisibleWilds;
}

void CtrSettings_SetVisibleWilds(int on)
{
    Load();
    on = on != 0;
    if (on == sVisibleWilds)
        return;
    sVisibleWilds = on;
    Save();
}

int CtrSettings_NuzLevelCap(void)
{
    Load();
    return sNuzCap;
}

void CtrSettings_SetNuzLevelCap(int on)
{
    Load();
    on = on != 0;
    if (on == sNuzCap)
        return;
    sNuzCap = on;
    Save();
}

const char *CtrSettings_Look(int slot)
{
    Load();
    return slot >= 0 && slot < 3 ? sLook[slot] : "";
}

void CtrSettings_SetLook(int slot, const char *id)
{
    char before[16];

    Load();
    if (slot < 0 || slot >= 3)
        return;
    memcpy(before, sLook[slot], sizeof(before));
    LookSet(slot, id ? id : "");
    if (memcmp(before, sLook[slot], sizeof(before)))
        Save();
}

void CtrSettings_SetEmbers(int level)
{
    Load();
    if (level < 0 || level > 2 || level == sEmbers)
        return;
    sEmbers = level;
    Save();
}

unsigned CtrSettings_LastBall(void)
{
    Load();
    return sBall;
}

void CtrSettings_SetLastBall(unsigned item)
{
    Load();
    if (item == sBall || item > 0xffff)
        return;
    sBall = item;
    Save();
}
