/* The game and the radio meet here (3ds_wl_glue.h). */
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
static bool sInternet;
static char sRelayHost[64], sCode[WL_INET_CODE_MAX + 1];
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
        sInternet = false;
        return;
    }
    memcpy(sRelayHost, host, strlen(host) + 1);
    memcpy(sCode, code, strlen(code) + 1);
    sRelayPort = port ? port : WL_INET_PORT;
    sInternet = true;
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
    CtrWl_SetInternet(host, (uint16_t)port, code);
    return sInternet;
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
static char sTrail[160];

static void JobBody(void)
{
    if (sInternet)
    {
        sConnected = WlInet_Connect(&sInet, sRelayHost, sRelayPort, sCode, sBuild, 8000);
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
    if (sState != CTR_WL_OFF || !CtrWl_Available())
        return;
    sState = CTR_WL_SEARCH;
    sJobState = 0;
    sInetRetryAt = 0;
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
        WlInet_Pump(&sInet);
        while (WlInet_PopEvent(&sInet, &kind, &node))
            if (kind == WL_INET_EVENT_LEFT)
                WlNet_Forget(&sNet, node);
        if (!WlInet_Connected(&sInet))
            LostLink(now);
        else
        {
            WlRfu_SetMembers(&sRfu, sInet.members);
            WlNet_Step(&sNet);
            WlRfu_Step(&sRfu);
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
    if (CtrStwi_SlaveTick)
        CtrStwi_SlaveTick();
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
    return WlRfu_Command(&sRfu, request, answer);
}

int CtrRfu_PollSlave(uint32_t *request)
{
    return sSetup ? WlRfu_PollSlave(&sRfu, request) : 0;
}

void CtrRfu_Reset(void)
{
    uint32_t request = 0x99660010u, answer[WL_RFU_ACK_WORDS + 2];

    if (sSetup)
        (void)WlRfu_Command(&sRfu, &request, answer);
}
