/* The game and the radio meet here (3ds_wl_glue.h). */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "3ds_wl_glue.h"
#ifdef __DEVKITARM__
#include "3ds_log.h"
#endif
#include "3ds_wl_inet.h"
#include "3ds_wl_uds.h"

#ifdef __DEVKITARM__
#include <3ds/os.h>
#include <3ds/thread.h>
#include <3ds/svc.h>
#include <3ds/synchronization.h>
#else
#include <time.h>
#endif

/* The game's own half (librfu_stwi.c, recipe 0272): a computer's test does not have it. */
extern void CtrStwi_SlaveTick(void) __attribute__((weak));

static WlUds sUds;
static WlInet sInet;
static WlNet sNet;
static WlRfu sRfu;
static bool sSetup;
static uint8_t sBuild[WL_UDS_BUILD_BYTES];
static uint32_t sRand;
static uint32_t sToken;                     /* what this console put in its beacon while it hosts */
static int sState = CTR_WL_OFF;
static bool sRetryArmed;
static uint32_t sRetryAt;
static WlUdsHost sHosts[WL_UDS_SCAN_HOSTS];
static int sFound;
static volatile int sJobState;              /* 0 none, 1 running, 2 done and not yet looked at */
static volatile int sConnected;             /* the internet job's result */
static uint32_t sScanAt;
static uint32_t sInetRetryAt;
static bool sInternet;                  /* this session's radio: the relay (true) or the console's own (false) */
static bool sHaveRelay;                 /* a relay and a room code are known */
static bool sLocal;                     /* the player chose local play (MULTIPLAYER: Local) */

static char sRelayHost[64], sCode[WL_INET_CODE_MAX + 1];
static char sBaseCode[WL_INET_CODE_MAX + 1];    /* the room of online.cfg or the relay's public room */
static char sPrivateCode[8];                    /* the private room's six digits, "" for none (CtrWl_SetPrivateRoom) */

/* The radio the next session uses: the relay when one is known and the player did not choose local play. Changed only between sessions:
 * a link that is up keeps its radio. */
static void ApplyMode(void)
{
    if (sState != CTR_WL_OFF)
        return;
    sInternet = sHaveRelay && !sLocal;
    if (sPrivateCode[0])
        snprintf(sCode, sizeof(sCode), "%s%s", CTR_WL_PRIVATE_ROOM, sPrivateCode);
    else
        memcpy(sCode, sBaseCode, sizeof(sCode));
}
static uint16_t sRelayPort = WL_INET_PORT;

static uint32_t NowMs(void)
{
#ifdef __DEVKITARM__
    return (uint32_t)osGetTime();
#else
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000u + ts.tv_nsec / 1000000u);
#endif
}

static uint32_t Rand32(void)
{
    sRand = sRand * 1664525u + 1013904223u;
    return sRand ^ (sRand << 13) ^ (sRand >> 7);
}

/* The radio that is not there: a session that has not started, or is over, sends and hears nothing. */
static bool NullSend(void *user, uint16_t dest, const void *data, uint16_t len)
{
    (void)user; (void)dest; (void)data; (void)len;
    return false;
}

static bool NullRecv(void *user, uint16_t *src, void *data, uint16_t max, uint16_t *len)
{
    (void)user; (void)src; (void)data; (void)max; (void)len;
    return false;
}

static uint32_t NullNow(void *user)
{
    (void)user;
    return NowMs();
}

static WlBackend NullBackend(void)
{
    WlBackend io = {NULL, NullSend, NullRecv, NullNow};

    return io;
}

void CtrWl_Setup(const uint8_t build[16], uint32_t seed)
{
    WlBackend none = NullBackend();

    memcpy(sBuild, build, sizeof(sBuild));
    sRand = seed ? seed : 0x5eedu;
    WlInet_Seed(&seed, sizeof(seed));
    WlNet_Init(&sNet, &none, 0);
    WlRfu_Init(&sRfu, &sNet, (uint16_t)(Rand32() | 1u));
    sState = CTR_WL_OFF;
    sRetryArmed = false;
    sSetup = true;
}

void CtrWl_SetInternet(const char *host, uint16_t port, const char *code)
{
    if (!host || !code || !host[0] || !code[0] || strlen(host) >= sizeof(sRelayHost) || strlen(code) > WL_INET_CODE_MAX)
    {
        sHaveRelay = false;
        ApplyMode();
        return;
    }
    memcpy(sRelayHost, host, strlen(host) + 1);
    memcpy(sBaseCode, code, strlen(code) + 1);
    sRelayPort = port ? port : WL_INET_PORT;
    sHaveRelay = true;
    ApplyMode();
}

void CtrWl_SetPrivateRoom(const char *digits)
{
    size_t n = 0;

    while (digits && n < 6 && digits[n] >= '0' && digits[n] <= '9')
        ++n;
    if (n == 6)
        memcpy(sPrivateCode, digits, 6);
    sPrivateCode[n == 6 ? 6 : 0] = 0;
    ApplyMode();
}

void CtrWl_SetLocal(bool local)
{
    sLocal = local;
    ApplyMode();
}

bool CtrWl_HasRelay(void)
{
    return sHaveRelay;
}

/* `src` into `dst`, or an empty string when it does not fit (a value that is cut is a wrong value). */
static void CopyOrEmpty(char *dst, size_t size, const char *src)
{
    size_t n = strlen(src);

    if (n >= size)
        dst[0] = 0;
    else
        memcpy(dst, src, n + 1);
}

bool CtrWl_LoadConfig(const char *path)
{
    FILE *f = fopen(path, "rb");
    char line[160], host[64] = "", code[WL_INET_CODE_MAX + 1] = "";
    unsigned port = 0;

    if (!f)
        return false;
    while (fgets(line, sizeof(line), f))
    {
        size_t n = strlen(line);

        while (n && (line[n - 1] == '\n' || line[n - 1] == '\r' || line[n - 1] == ' '))
            line[--n] = 0;
        if (!strncmp(line, "relay=", 6))
        {
            char *colon = strrchr(line + 6, ':');

            if (colon)
            {
                port = (unsigned)strtoul(colon + 1, NULL, 10);
                *colon = 0;
            }
            CopyOrEmpty(host, sizeof(host), line + 6);
        }
        else if (!strncmp(line, "code=", 5))
            CopyOrEmpty(code, sizeof(code), line + 5);
    }
    fclose(f);
    if (host[0] && !code[0])
        CopyOrEmpty(code, sizeof(code), CTR_WL_PUBLIC_ROOM);      /* a relay without a code: its public room */
    CtrWl_SetInternet(host, (uint16_t)port, code);
    return sHaveRelay;
}

bool CtrWl_IsInternet(void)
{
    return sInternet;
}

/* What the adapter does, for port.log: the first few of each kind. */
#ifdef __DEVKITARM__
#define WL_NOTE(counter, ...) do { if ((counter)++ < 40) CtrLog_Write(CTR_LOG_GAME, __VA_ARGS__); } while (0)
#else
#define WL_NOTE(counter, ...) do { (void)(counter); } while (0)
#endif
/* A log of its own per node of the room (sdmc:/3ds/twinembers/online-<node>.log): two emulators on one computer share port.log. */
#ifdef __DEVKITARM__
static FILE *sNetLog;
static unsigned sNetLogLines;
static LightLock sNetLogLock;

static void NetLogOpen(unsigned node)
{
    char path[64];

    static bool lockReady;

    if (!CtrLog_Enabled())
        return;                             /* as port.log: only with sdmc:/3ds/twinembers/debug.txt (or an Old 3DS build) */
    if (!lockReady)
    {
        LightLock_Init(&sNetLogLock);
        lockReady = true;
    }
    LightLock_Lock(&sNetLogLock);
    if (sNetLog)
        fclose(sNetLog);
    snprintf(path, sizeof(path), "sdmc:/3ds/twinembers/online-%u.log", node);
    sNetLog = fopen(path, "w");
    sNetLogLines = 0;
    LightLock_Unlock(&sNetLogLock);
}

static void NetLog(const char *format, ...) __attribute__((format(printf, 1, 2)));
static void NetLog(const char *format, ...)
{
    va_list args;

    if (!sNetLog || sNetLogLines++ > 40000)
        return;
    LightLock_Lock(&sNetLogLock);
    fprintf(sNetLog, "[%10.3f] ", (double)(uint32_t)osGetTime() / 1000.0);
    va_start(args, format);
    vfprintf(sNetLog, format, args);
    va_end(args);
    fputc(10, sNetLog);
    LightLock_Unlock(&sNetLogLock);
}

/* Written out every two seconds and when the link ends (each flush is a write to the card, too slow for every line). */
static void NetLogFlush(void)
{
    if (!sNetLog)
        return;
    LightLock_Lock(&sNetLogLock);
    fflush(sNetLog);
    LightLock_Unlock(&sNetLogLock);
}

/* A watch on the game thread while in a room: when no frame has gone through CtrWl_Tick for a second, it writes down where the game was
 * (WL_WHERE_*: in which of this file's calls, after which adapter command) and flushes the log, which a frozen game never would. It runs
 * at a higher priority than the game so that a game thread that spins still lets it in. */
enum { WL_WHERE_GAME, WL_WHERE_TICK, WL_WHERE_PUMP, WL_WHERE_NET, WL_WHERE_RFU, WL_WHERE_SLAVE, WL_WHERE_COMMAND, WL_WHERE_POLL, WL_WHERE_WAIT };
static volatile uint32_t sWatchTicks;
static volatile int sWhere;
static volatile unsigned sWhereCommand;
static Thread sWatchThread;

static void WatchMain(void *unused)
{
    uint32_t last = sWatchTicks, stalledMs = 0;

    (void)unused;
    for (;;)
    {
        svcSleepThread(250000000LL);
        if (sWatchTicks != last)
        {
            if (stalledMs >= 1000)
            {
                NetLog("game going again after %lu ms", (unsigned long)stalledMs);
                NetLogFlush();
            }
            last = sWatchTicks;
            stalledMs = 0;
            continue;
        }
        stalledMs += 250;
        if (stalledMs == 1000 || stalledMs % 10000 == 0)
        {
            NetLog("STALL: no frame for %lu ms; where %d, last adapter command %02x, frames %lu", (unsigned long)stalledMs, sWhere,
                   sWhereCommand, (unsigned long)sWatchTicks);
            NetLogFlush();
        }
    }
}

static void WatchStart(void)
{
    if (!sWatchThread && sNetLog)
        sWatchThread = threadCreate(WatchMain, NULL, 0x2000, 0x28, -2, true);
}
#define WL_WHERE(place) (sWhere = (place))
#define WL_TICKED() (++sWatchTicks)
#define WL_COMMAND(c) (sWhereCommand = (c))
static int sWhereSaved(void) { return sWhere; }
#else
#define NetLogOpen(node) ((void)(node))
#define NetLog(...) ((void)0)
#define NetLogFlush() ((void)0)
#define WatchStart() ((void)0)
#define WL_WHERE(place) ((void)(place))
#define WL_TICKED() ((void)0)
#define WL_COMMAND(c) ((void)(c))
enum { WL_WHERE_GAME, WL_WHERE_TICK, WL_WHERE_PUMP, WL_WHERE_NET, WL_WHERE_RFU, WL_WHERE_SLAVE, WL_WHERE_COMMAND, WL_WHERE_POLL, WL_WHERE_WAIT };
static int sWhereSaved(void) { return WL_WHERE_GAME; }
#endif

static int sNoteAvailable;

uint8_t CtrWl_Available(void)
{
    uint32_t now;

    WL_NOTE(sNoteAvailable, "wireless: adapter asked for (setup %d, internet %d)", (int)sSetup, (int)sInternet);
    if (!sSetup)
        return 0;
    if (sInternet)
        return 1;                           /* the connection itself is made when a session starts */
    if (sUds.ready)
        return 1;
    now = NowMs();
    if (sRetryArmed && (int32_t)(now - sRetryAt) < 0)
        return 0;
    if (WlUds_Init(&sUds, sBuild, NULL))
    {
        sRetryArmed = false;
        return 1;
    }
    sRetryArmed = true;
    sRetryAt = now + CTR_WL_RETRY_MS;
    return 0;
}

/* ---- the scan and the connection, on a thread of its own on the console (each blocks for seconds) ---- */
static int sNoteSession, sNoteCommand, sNoteState;

static int sFailStep, sFailErrno, sFailReported;
static uint32_t sStatAt;
static char sTrail[160];

static void JobBody(void)
{
    if (sInternet)
    {
        char code[sizeof(sCode)];

        memcpy(code, sCode, sizeof(code));
        for (int room = 1;; ++room)
        {
            sConnected = WlInet_Connect(&sInet, sRelayHost, sRelayPort, code, sBuild, 8000);
            /* The public room is full: the next one (twinembers-public-2, -3...), so the rooms fill in order. */
            if (sConnected || sInet.reject != WL_INET_REJECT_FULL || strcmp(sCode, CTR_WL_PUBLIC_ROOM) || room >= CTR_WL_PUBLIC_ROOMS)
                break;
            snprintf(code, sizeof(code), "%s-%d", CTR_WL_PUBLIC_ROOM, room + 1);
        }
        sFailStep = sInet.failStep;
        sFailErrno = sInet.failErrno;
        sTrail[0] = 0;
        for (int i = 0; i < sInet.trailLen; ++i)
        {
            size_t used = strlen(sTrail);

            snprintf(sTrail + used, sizeof(sTrail) - used, "%d:%d ", sInet.trail[i][0], sInet.trail[i][1]);
        }
    }
    else
        sFound = WlUds_Scan(&sUds, sHosts, WL_UDS_SCAN_HOSTS);
    sJobState = 2;
}

#ifdef __DEVKITARM__
static Thread sThread;

static void JobMain(void *unused)
{
    (void)unused;
    JobBody();
}

static void Reap(void)
{
    if (sThread)
    {
        threadJoin(sThread, U64_MAX);
        threadFree(sThread);
        sThread = NULL;
    }
}

static void StartJob(void)
{
    Reap();
    sJobState = 1;
    sThread = threadCreate(JobMain, NULL, 0x10000, 0x31, -2, false);
    if (!sThread)
        JobBody();                          /* no thread to be had: the frame waits */
}
#else
static void Reap(void)
{
}

static void StartJob(void)
{
    sJobState = 1;
    JobBody();
}
#endif

static void Open(int state)
{
    WL_NOTE(sNoteState, "wireless: open state %d", state);
    WlBackend io = sInternet ? WlInet_Backend(&sInet) : WlUds_Backend(&sUds);

    sState = state;
    WlNet_Init(&sNet, &io, sInternet ? sInet.node : sUds.node);   /* the adapter keeps what it was told; only its link to the radio is new */
}

static void Close(void)
{
    NetLog("session closed by the game");
    NetLogFlush();
    WlBackend none = NullBackend();

    Reap();
    sJobState = 0;
    if (sInternet)
        WlInet_Close(&sInet);
    else if (sUds.ready)
        WlUds_Leave(&sUds);
    sState = CTR_WL_OFF;
    WlNet_Init(&sNet, &none, 0);
    WlRfu_SetMembers(&sRfu, 0);
}

/* The host to join among the ones found: one that has company first, then the smallest token; never one at the end of its room. */
static const WlUdsHost *Best(uint32_t belowToken, bool onlyBelow)
{
    const WlUdsHost *best = NULL;

    for (int i = 0; i < sFound; ++i)
    {
        const WlUdsHost *h = &sHosts[i];

        if (h->nodes < 1 || h->nodes >= h->maxNodes || h->token == sToken)
            continue;
        if (onlyBelow && h->nodes < 2 && h->token >= belowToken)
            continue;                       /* a host alone as we are: the smaller token wins */
        if (!best || h->nodes > best->nodes || (h->nodes == best->nodes && h->token < best->token))
            best = h;
    }
    return best;
}

static void BecomeHost(void)
{
    sToken = Rand32();
    if (WlUds_Host(&sUds, CTR_WL_NODES_MAX, sToken))
        Open(CTR_WL_HOST);
    else
        sState = CTR_WL_OFF;                /* the game will try again at its next search */
}

static void StartSession(void)
{
    WL_NOTE(sNoteSession, "wireless: session start asked (state %d, internet %d, setup %d)", sState, (int)sInternet, (int)sSetup);
    ApplyMode();                            /* the MULTIPLAYER choice counts from here */
    if (sState != CTR_WL_OFF || !CtrWl_Available())
        return;
    sState = CTR_WL_SEARCH;
    sJobState = 0;
    sInetRetryAt = NowMs();                 /* not 0: the console clock counts from 1900, and a deadline far behind it reads as one ahead */
}

static void EndSession(void)
{
    WL_NOTE(sNoteSession, "wireless: session end asked (state %d)", sState);
    if (sState != CTR_WL_OFF)
        Close();
}

void CtrWl_Shutdown(void)
{
    Close();
    if (sUds.ready)
        WlUds_Exit(&sUds);
}

int CtrWl_State(void)
{
    return sState;
}

int CtrWl_Others(void)
{
    uint32_t members = sInternet ? sInet.members : sUds.members;
    int n = 0;

    for (unsigned i = 1; i <= WL_MAX_NODES; ++i)
        n += (members >> i) & 1;
    return n;
}

/* The link is gone: the adapter is told every other console left, and the session starts over (the search, or the connection again after a pause). */
static void LostLink(uint32_t now)
{
    NetLog("link lost (socket %d, welcomed %d, sent %lu got %lu)", (int)sInet.sock, (int)sInet.welcomed, (unsigned long)sInet.sentFrames, (unsigned long)sInet.receivedFrames);
    NetLogFlush();
    WlBackend none = NullBackend();

    if (sInternet)
        WlInet_Close(&sInet);
    else
        WlUds_Leave(&sUds);
    WlNet_Init(&sNet, &none, 0);
    WlRfu_SetMembers(&sRfu, 0);
    sState = CTR_WL_SEARCH;
    sJobState = 0;
    sInetRetryAt = now + CTR_WL_RECONNECT_MS;
}

void CtrWl_Tick(void)
{
    uint32_t now;
    int kind;
    uint16_t node;

    if (!sSetup)
        return;
    WL_TICKED();
    WL_WHERE(WL_WHERE_TICK);
    now = NowMs();
    if (sState == CTR_WL_SEARCH)
    {
        if (sJobState == 0 && (!sInternet || (int32_t)(now - sInetRetryAt) >= 0))
            StartJob();
        if (sJobState == 2)
        {
            Reap();
            sJobState = 0;
            sScanAt = now;
            if (sInternet)
            {
                if (sConnected)
                {
                    Open(CTR_WL_ONLINE);
#ifdef __DEVKITARM__
                    CtrLog_Write(CTR_LOG_GAME, "internet play: in the room as node %u (%s)", (unsigned)sInet.node, sTrail);
                    NetLogOpen(sInet.node);
                    WatchStart();
                    NetLog("in the room as node %u, members %lx", (unsigned)sInet.node, (unsigned long)sInet.members);
#endif
                }
                else
                {
                    sInetRetryAt = now + CTR_WL_RECONNECT_MS;
#ifdef __DEVKITARM__
                    if (sFailReported < 5)      /* the first few only: a relay that is down would fill the log */
                        CtrLog_Write(CTR_LOG_GAME, "internet play: no connection (step %d, error %d; %s)", sFailStep, sFailErrno, sTrail);
#endif
                    ++sFailReported;
                }       /* the relay is out of reach: again in a while, for as long as the game wants the adapter */
            }
            else
            {
                const WlUdsHost *host = Best(0, false);

                if (host && WlUds_Join(&sUds, host))
                    Open(CTR_WL_JOINED);
                else
                    BecomeHost();
            }
        }
    }
    else if (sState == CTR_WL_ONLINE)
    {
        WL_WHERE(WL_WHERE_PUMP);
        WlInet_Pump(&sInet);
        WL_WHERE(WL_WHERE_TICK);
        while (WlInet_PopEvent(&sInet, &kind, &node))
        {
            NetLog("node %u %s", (unsigned)node, kind == WL_INET_EVENT_LEFT ? "left" : "joined");
            if (kind == WL_INET_EVENT_LEFT)
                WlNet_Forget(&sNet, node);
        }
        if (!WlInet_Connected(&sInet))
            LostLink(now);
        else
        {
            WlRfu_SetMembers(&sRfu, sInet.members);
            WL_WHERE(WL_WHERE_NET);
            WlNet_Step(&sNet);
            WL_WHERE(WL_WHERE_RFU);
            WlRfu_Step(&sRfu);
            WL_WHERE(WL_WHERE_TICK);
            if (now - sStatAt >= 2000)
            {
                sStatAt = now;
                NetLog("members %lx sent %lu got %lu full %lu bad %lu tx %u in %u pending %u,%u,%u rfu %d slave %d",
                       (unsigned long)sInet.members, (unsigned long)sInet.sentFrames, (unsigned long)sInet.receivedFrames,
                       (unsigned long)sInet.lostFrames, (unsigned long)sInet.dropped, (unsigned)sInet.txLen, (unsigned)sInet.inCount,
                       (unsigned)WlNet_Pending(&sNet, 1), (unsigned)WlNet_Pending(&sNet, 2), (unsigned)WlNet_Pending(&sNet, 3),
                       (int)sRfu.state, (int)sRfu.slave);
                NetLogFlush();
            }
        }
    }
    else if (sState == CTR_WL_HOST || sState == CTR_WL_JOINED)
    {
        WlUds_Pump(&sUds);
        while (WlUds_PopEvent(&sUds, &kind, &node))
            if (kind == WL_UDS_EVENT_LEFT)
                WlNet_Forget(&sNet, node);
        if (!sUds.active)
            LostLink(now);                  /* the host left, or the radio was lost */
        else
        {
            WlRfu_SetMembers(&sRfu, sUds.members);
            WlNet_Step(&sNet);
            WlRfu_Step(&sRfu);
            if (sState == CTR_WL_HOST && sUds.members == 0)
            {
                /* Alone: look again now and then; a host with a smaller token (or one that already has company) takes us in. */
                if (sJobState == 2)
                {
                    const WlUdsHost *host;

                    Reap();
                    sJobState = 0;
                    sScanAt = now;
                    host = Best(sToken, true);
                    if (host)
                    {
                        WlUdsHost chosen = *host;

                        WlUds_Leave(&sUds);
                        if (WlUds_Join(&sUds, &chosen))
                            Open(CTR_WL_JOINED);
                        else
                            BecomeHost();
                    }
                }
                else if (sJobState == 0 && (int32_t)(now - sScanAt) >= (int32_t)CTR_WL_RESCAN_MS)
                    StartJob();
            }
        }
    }
    WL_WHERE(WL_WHERE_SLAVE);
    if (CtrStwi_SlaveTick)
        CtrStwi_SlaveTick();
    WL_WHERE(WL_WHERE_GAME);
}

/* ---- what the game's librfu calls ---- */
int CtrRfu_Command(const uint32_t *request, uint32_t *answer)
{
    if (!sSetup)
    {
        answer[0] = 0x996601EEu;            /* a rejection: the adapter is not set up (reason 1: the command is not allowed now) */
        answer[1] = 1;
        return 2;
    }
    WL_NOTE(sNoteCommand, "wireless: command %02x", (unsigned)(request[0] & 0xffu));
    {
        static unsigned last = 0x100, repeats;
        unsigned c = request[0] & 0xffu;

        static unsigned noted;

        if ((c != last || ++repeats % 300 == 0) && noted++ < 3000)
            NetLog("command %02x (x%u)", c, repeats + 1);   /* the first commands only: a link's every frame would fill the log */
        if (c != last)
            repeats = 0;
        last = c;
    }
    switch (request[0] & 0xffu)
    {
    case WLC_SC_START:
    case WLC_SP_START:
    case WLC_CP_START:
        StartSession();
        break;
    case WLC_STOP_MODE:
        EndSession();
        break;
    default:
        break;
    }
    {
        int words, before = sWhereSaved();

        WL_WHERE(WL_WHERE_COMMAND);
        WL_COMMAND(request[0] & 0xffu);
        words = WlRfu_Command(&sRfu, request, answer);
        WL_WHERE(before);
        return words;
    }
}

int CtrRfu_PollSlave(uint32_t *request)
{
    int words, before = sWhereSaved();

    WL_WHERE(WL_WHERE_POLL);
    words = sSetup ? WlRfu_PollSlave(&sRfu, request) : 0;
    WL_WHERE(before);
    return words;
}

/* The game waits in a loop for the adapter (link_rfu_2.c, a parent that handed the clock over waits for its children's answers): on the
 * GBA the adapter's request came by interrupt during that loop. One turn of the radio and the adapter, and the request if there is one;
 * a millisecond's sleep lets the system's threads bring what the other consoles sent. */
void CtrRfu_Wait(void)
{
    CtrWl_Idle();
#ifdef __DEVKITARM__
    svcSleepThread(1000000LL);
#endif
}

/* One turn of the radio and the adapter between frames, while the console has time to spare (3ds_game_bridge.c): the GBA's interrupts
 * came while the game waited for the next frame, so a child answers its parent at once instead of at its next frame. */
void CtrWl_Idle(void)
{
    int before = sWhereSaved();

    if (!sSetup || sState == CTR_WL_OFF)
        return;
    WL_WHERE(WL_WHERE_WAIT);
    if (sState == CTR_WL_ONLINE && WlInet_Connected(&sInet))
    {
        WlInet_Pump(&sInet);
        WlRfu_SetMembers(&sRfu, sInet.members);
        WlRfu_Step(&sRfu);
    }
    else if ((sState == CTR_WL_HOST || sState == CTR_WL_JOINED) && sUds.active)
    {
        WlUds_Pump(&sUds);
        WlRfu_SetMembers(&sRfu, sUds.members);
        WlRfu_Step(&sRfu);
    }
    else
        WlRfu_Step(&sRfu);                  /* no link: the adapter still answers (the slots lost) */
    if (CtrStwi_SlaveTick)
        CtrStwi_SlaveTick();
    WL_WHERE(before);
}

void CtrRfu_Reset(void)
{
    uint32_t request = 0x99660010u, answer[WL_RFU_ACK_WORDS + 2];

    if (sSetup)
        (void)WlRfu_Command(&sRfu, &request, answer);
}
