/* See 3ds_settings.h. */
#define _POSIX_C_SOURCE 200809L   /* fileno, fsync */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "3ds_settings.h"
#include "3ds_wilds_core.h"

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
static int sStyle = 1;       /* the lower screen's look: 1 Pokeball (the default), 0 Ascuas; only the choice of Ascuas is written (style=0) */
static int sRepelAgain = 1;
static int sCompanion;
static char sLook[4][16];     /* look_red, look_leaf, look_blue, look_surf (the Pokemon to surf on) */
static int sExpShare;
static int sTmReuse;
static int sNuzlocke;
static int sRandomlocke;
static int sNuzDupes;       /* NUZLOCKE clauses: a species already caught does not use the area's catch */
static int sNuzShiny;       /* a shiny does not use it either */
static int sNuzCap;         /* LEVEL CAPS: no experience at or above the badge's cap */
static int sNuzSet;         /* SET MODE: the battle style is Set while NUZLOCKE is on */
static int sRandTarget;     /* RANDOM: 0 wild and trainers, 1 wild only, 2 trainers only */
static int sRandNoLegend;   /* NO LEGENDARIES: the random species are never legendary */
static int sFreeHm;
static int sDayNight;       /* DAY/NIGHT: 0 CLOCK, 1 FIXED, 2 OFF (daynight=) */
static int sHour = 24;      /* HOUR: FIXED's half hour, 24 is 12:00 (hour=) */
static int sTypeMatchup;    /* TYPE MATCHUP: 0 shown, 1 hidden (typematchup=) */
static int sMoveDetails;    /* MOVE DETAILS: 0 shown, 1 hidden (movedetails=) */
static int sCheat[CTR_CHEAT_COUNT];      /* TRUCOS: cheatN= */
static const uint8_t kCheatMax[CTR_CHEAT_COUNT] = {1, 6, 1, 1, 3, 1, 1};
static int sPrivateRoom;    /* ONLINE ROOM: 1 private (the room of the code below) (privroom=) */
static int sRoomNote;       /* a code from an older build was dropped and the player was not told yet (roomnote=) */
static char sRoomCode[CTR_ROOM_CODE_DIGITS + 1];   /* the private room's code: its digits, "" when none was set (roomcode=) */
static int sLocalPlay;      /* MULTIPLAYER: 0 Online (the relay), 1 Local (the console's own radio) (localplay=) */
static int sWildDensity;    /* WILD POKEMON: 0 OFF, 1 FEW, 2 NORMAL, 3 MANY (visiblewilds=; an old 1 meant on) */
static int sWildSil;        /* WILD DISPLAY: 0 Pokemon, 1 silhouette (wildsil=) */
static int sWildAggro;      /* AGGRESSIVE: 0 OFF to 4 HIGH (wildaggro=) */
static unsigned sBall;          /* the ball the battle's ball button last threw (0: none yet) */
static int sLoaded;

/* The file stays open from the first read to the exit and is rewritten in place, padded to a fixed size: creating or
 * truncating it on the SD card is what took 3.4 to 6.9 s (0.2.6 and 0.2.7 Old 3DS port.log, one freeze per START that
 * cycled the turbo), and the game froze for the whole of it even with the write on a thread of its own. A missing file
 * is created here, on the first read, which happens while the game starts. */
#define SETTINGS_FILE_BYTES 640     /* was 256, then 512: the file grew to about 232 bytes in the worst case, then 430 with the cheats, about 480 with DAY/NIGHT and its rows; a shorter old file reads as before */
static FILE *sFile;

static const char *const kLookKey[4] = {"look_red=", "look_leaf=", "look_blue=", "look_surf="};

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
    for (int slot = 0; slot < 4; ++slot)
        if (strncmp(line, kLookKey[slot], strlen(kLookKey[slot])) == 0)
        {
            LookSet(slot, line + strlen(kLookKey[slot]));
            return 1;
        }
    return 0;
}

/* A room code is CTR_ROOM_CODE_DIGITS digits, at least six of them different; anything else (an older six-digit code too) is no code. */
int CtrSettings_RoomCodeOk(const char *code)
{
    unsigned seen = 0, different = 0;
    size_t n = 0;

    if (!code)
        return 0;
    while (n < CTR_ROOM_CODE_DIGITS && code[n] >= '0' && code[n] <= '9')
        seen |= 1u << (code[n++] - '0');
    for (; seen; seen >>= 1)
        different += seen & 1;
    return n == CTR_ROOM_CODE_DIGITS && code[n] == 0 && different >= 6;
}

int CtrSettings_MakeRoomCode(char out[CTR_ROOM_CODE_DIGITS + 1], const uint8_t *random, size_t n)
{
    char code[CTR_ROOM_CODE_DIGITS + 1];
    size_t made = 0;

    for (size_t i = 0; i < n && made < CTR_ROOM_CODE_DIGITS; ++i)
        if (random[i] < 250)
            code[made++] = (char)('0' + random[i] % 10);
    code[made] = 0;
    if (made < CTR_ROOM_CODE_DIGITS || !CtrSettings_RoomCodeOk(code))
        return 0;
    memcpy(out, code, sizeof(code));
    return 1;
}

static void RoomCodeSet(const char *text)
{
    if (CtrSettings_RoomCodeOk(text))
        memcpy(sRoomCode, text, CTR_ROOM_CODE_DIGITS + 1);
    else
        sRoomCode[0] = 0;
}

static void Save(void);

static void Load(void)
{
    char line[64];
    FILE *file;
    int wildRaw = 0, haveSil = 0, haveAggro = 0, sawBadCode = 0;

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
        else if (sscanf(line, "style=%u", &value) == 1)
            sStyle = value <= 1 ? (int)value : 1;           /* a damaged value is the default */
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
        else if (sscanf(line, "nuzset=%u", &value) == 1)
            sNuzSet = value != 0;
        else if (sscanf(line, "randtarget=%u", &value) == 1)
            sRandTarget = value <= 2 ? (int)value : 0;
        else if (sscanf(line, "randnolegend=%u", &value) == 1)
            sRandNoLegend = value != 0;
        else if (sscanf(line, "freehm=%u", &value) == 1)
            sFreeHm = value != 0;
        else if (sscanf(line, "daynight=%u", &value) == 1)
            sDayNight = value <= 2 ? (int)value : 0;
        else if (sscanf(line, "hour=%u", &value) == 1)
            sHour = value < 48 ? (int)value : 24;
        else if (sscanf(line, "typematchup=%u", &value) == 1)
            sTypeMatchup = value != 0;
        else if (sscanf(line, "movedetails=%u", &value) == 1)
            sMoveDetails = value != 0;
        else if (strncmp(line, "cheat", 5) == 0 && line[5] >= '0' && line[5] < '0' + CTR_CHEAT_COUNT && sscanf(line + 6, "=%u", &value) == 1)
            sCheat[line[5] - '0'] = value <= kCheatMax[line[5] - '0'] ? (int)value : 0;
        else if (sscanf(line, "localplay=%u", &value) == 1)
            sLocalPlay = value != 0;
        else if (sscanf(line, "privroom=%u", &value) == 1)
            sPrivateRoom = value != 0;
        else if (strncmp(line, "roomcode=", 9) == 0)
        {
            /* fgets keeps the line end: LF, or CR LF in a file that was edited on a computer. Those are not part of the code. */
            size_t n = strlen(line + 9);

            while (n > 0 && (line[9 + n - 1] == '\n' || line[9 + n - 1] == '\r'))
                line[9 + --n] = 0;
            RoomCodeSet(line + 9);
            if (n > 0 && !sRoomCode[0])
                sawBadCode = 1;     /* a code was there and it is not one (the six digits of an older build, or damage) */
        }
        else if (sscanf(line, "roomnote=%u", &value) == 1)
            sRoomNote = value != 0;
        else if (sscanf(line, "visiblewilds=%u", &value) == 1)
            wildRaw = value < 1000 ? (int)value : -1;       /* settled below: an old file's 1 is NORMAL */
        else if (sscanf(line, "wildsil=%u", &value) == 1)
        {
            sWildSil = value == 1;
            haveSil = 1;
        }
        else if (sscanf(line, "wildaggro=%u", &value) == 1)
        {
            sWildAggro = value <= 4 ? (int)value : 0;
            haveAggro = 1;
        }
        else if (LookLine(line))
            ;
        else if (sscanf(line, "ball=%u", &value) == 1)
            sBall = value <= 0xffff ? value : 0;
    }
    sWildDensity = CtrWildsCore_MigrateDensity(wildRaw, haveSil, haveAggro);
    /* A private room whose code is gone (an older six-digit code, or a damaged line) must not turn into the public room unseen: the
     * private mode is switched off in the file itself, and the OPTIONS screen says why until the player chooses a room again. */
    if (sawBadCode || (sPrivateRoom && !sRoomCode[0]))
    {
        sPrivateRoom = 0;
        sRoomNote = 1;
        Save();
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
    if (!sStyle && length > 0 && (size_t)length < size)      /* Poke Ball is the default: only the choice of Ascuas is written */
        length += snprintf(text + length, size - (size_t)length, "style=0\n");
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
    if (sNuzSet && length > 0 && (size_t)length < size)
        length += snprintf(text + length, size - (size_t)length, "nuzset=1\n");
    if (sRandTarget && length > 0 && (size_t)length < size)
        length += snprintf(text + length, size - (size_t)length, "randtarget=%d\n", sRandTarget);
    if (sRandNoLegend && length > 0 && (size_t)length < size)
        length += snprintf(text + length, size - (size_t)length, "randnolegend=1\n");
    if (sFreeHm && length > 0 && (size_t)length < size)
        length += snprintf(text + length, size - (size_t)length, "freehm=1\n");
    if (sDayNight && length > 0 && (size_t)length < size)
        length += snprintf(text + length, size - (size_t)length, "daynight=%d\n", sDayNight);
    if (sHour != 24 && length > 0 && (size_t)length < size)
        length += snprintf(text + length, size - (size_t)length, "hour=%d\n", sHour);
    if (sTypeMatchup && length > 0 && (size_t)length < size)
        length += snprintf(text + length, size - (size_t)length, "typematchup=1\n");
    if (sMoveDetails && length > 0 && (size_t)length < size)
        length += snprintf(text + length, size - (size_t)length, "movedetails=1\n");
    for (int i = 0; i < CTR_CHEAT_COUNT; ++i)
        if (sCheat[i] && length > 0 && (size_t)length < size)
            length += snprintf(text + length, size - (size_t)length, "cheat%d=%d\n", i, sCheat[i]);
    if (sLocalPlay && length > 0 && (size_t)length < size)
        length += snprintf(text + length, size - (size_t)length, "localplay=1\n");
    if (sPrivateRoom && length > 0 && (size_t)length < size)
        length += snprintf(text + length, size - (size_t)length, "privroom=1\n");
    if (sRoomCode[0] && length > 0 && (size_t)length < size)
        length += snprintf(text + length, size - (size_t)length, "roomcode=%s\n", sRoomCode);
    if (sRoomNote && length > 0 && (size_t)length < size)
        length += snprintf(text + length, size - (size_t)length, "roomnote=1\n");
    /* The wild keys come before the looks and the ball, which are the first to be cut if the text ever outgrows the file; wildsil and
     * wildaggro come first so that a visiblewilds that was read always had them (a lone 1 is the old format, NORMAL). */
    if ((sWildDensity || sWildSil) && length > 0 && (size_t)length < size)
        length += snprintf(text + length, size - (size_t)length, "wildsil=%d\n", sWildSil);
    if ((sWildDensity || sWildAggro) && length > 0 && (size_t)length < size)
        length += snprintf(text + length, size - (size_t)length, "wildaggro=%d\n", sWildAggro);
    if (sWildDensity && length > 0 && (size_t)length < size)
        length += snprintf(text + length, size - (size_t)length, "visiblewilds=%d\n", sWildDensity);
    for (int slot = 0; slot < 4; ++slot)
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

int CtrSettings_DayNight(void)
{
    Load();
    return sDayNight;
}

void CtrSettings_SetDayNight(int mode)
{
    Load();
    if (mode < 0 || mode > 2)
        mode = 0;
    if (mode == sDayNight)
        return;
    sDayNight = mode;
    Save();
}

int CtrSettings_Hour(void)
{
    Load();
    return sHour;
}

void CtrSettings_SetHour(int half)
{
    Load();
    if (half < 0 || half >= 48)
        half = 24;
    if (half == sHour)
        return;
    sHour = half;
    Save();
}

int CtrSettings_TypeMatchup(void)
{
    Load();
    return sTypeMatchup;
}

void CtrSettings_SetTypeMatchup(int hidden)
{
    Load();
    hidden = hidden != 0;
    if (hidden == sTypeMatchup)
        return;
    sTypeMatchup = hidden;
    Save();
}

int CtrSettings_MoveDetails(void)
{
    Load();
    return sMoveDetails;
}

void CtrSettings_SetMoveDetails(int hidden)
{
    Load();
    hidden = hidden != 0;
    if (hidden == sMoveDetails)
        return;
    sMoveDetails = hidden;
    Save();
}

int CtrSettings_CheatMax(int id)
{
    return id >= 0 && id < CTR_CHEAT_COUNT ? kCheatMax[id] : 0;
}

int CtrSettings_Cheat(int id)
{
    Load();
    return id >= 0 && id < CTR_CHEAT_COUNT ? sCheat[id] : 0;
}

void CtrSettings_SetCheat(int id, int value)
{
    Load();
    if (id < 0 || id >= CTR_CHEAT_COUNT)
        return;
    value = value < 0 ? 0 : value > kCheatMax[id] ? kCheatMax[id] : value;
    if (value == sCheat[id])
        return;
    sCheat[id] = value;
    Save();
}

int CtrSettings_PrivateRoom(void)
{
    Load();
    return sPrivateRoom && sRoomCode[0];
}

void CtrSettings_SetPrivateRoom(int on)
{
    Load();
    on = on != 0;
    if (on == sPrivateRoom)
        return;
    sPrivateRoom = on;
    Save();
}

const char *CtrSettings_RoomCode(void)
{
    Load();
    return sRoomCode;
}

void CtrSettings_SetRoomCode(const char *code)
{
    char before[sizeof(sRoomCode)];
    int wasNote;

    Load();
    wasNote = sRoomNote;
    memcpy(before, sRoomCode, sizeof(before));
    RoomCodeSet(code ? code : "");
    if (sRoomCode[0])
        sRoomNote = 0;      /* a new code: the player has chosen a room again */
    if (strcmp(before, sRoomCode) || (!sRoomNote && wasNote))
        Save();
}

int CtrSettings_RoomCodeDropped(void)
{
    Load();
    return sRoomNote;
}

void CtrSettings_AckRoomCodeDropped(void)
{
    Load();
    if (sRoomNote)
    {
        sRoomNote = 0;
        Save();
    }
}

int CtrSettings_LocalPlay(void)
{
    Load();
    return sLocalPlay;
}

void CtrSettings_SetLocalPlay(int on)
{
    Load();
    on = on != 0;
    if (on == sLocalPlay)
        return;
    sLocalPlay = on;
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

int CtrSettings_WildDensity(void)
{
    Load();
    return sWildDensity;
}

void CtrSettings_SetWildDensity(int density)
{
    Load();
    density = CtrWildsCore_ClampDensity(density);
    if (density == sWildDensity)
        return;
    sWildDensity = density;
    Save();
}

int CtrSettings_WildSilhouette(void)
{
    Load();
    return sWildSil;
}

void CtrSettings_SetWildSilhouette(int on)
{
    Load();
    on = on != 0;
    if (on == sWildSil)
        return;
    sWildSil = on;
    Save();
}

int CtrSettings_WildAggro(void)
{
    Load();
    return sWildAggro;
}

void CtrSettings_SetWildAggro(int aggro)
{
    Load();
    aggro = CtrWildsCore_ClampAggro(aggro);
    if (aggro == sWildAggro)
        return;
    sWildAggro = aggro;
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

int CtrSettings_NuzSet(void)
{
    Load();
    return sNuzSet;
}

void CtrSettings_SetNuzSet(int on)
{
    Load();
    on = on != 0;
    if (on == sNuzSet)
        return;
    sNuzSet = on;
    Save();
}

int CtrSettings_RandomTarget(void)
{
    Load();
    return sRandTarget;
}

void CtrSettings_SetRandomTarget(int target)
{
    Load();
    target = target >= 0 && target <= 2 ? target : 0;
    if (target == sRandTarget)
        return;
    sRandTarget = target;
    Save();
}

int CtrSettings_RandomNoLegendary(void)
{
    Load();
    return sRandNoLegend;
}

void CtrSettings_SetRandomNoLegendary(int on)
{
    Load();
    on = on != 0;
    if (on == sRandNoLegend)
        return;
    sRandNoLegend = on;
    Save();
}

const char *CtrSettings_Look(int slot)
{
    Load();
    return slot >= 0 && slot < 4 ? sLook[slot] : "";
}

void CtrSettings_SetLook(int slot, const char *id)
{
    char before[16];

    Load();
    if (slot < 0 || slot >= 4)
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

int CtrSettings_Style(void)
{
    Load();
    return sStyle;
}

void CtrSettings_SetStyle(int style)
{
    Load();
    if (style < 0 || style > 1 || style == sStyle)
        return;
    sStyle = style;
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
