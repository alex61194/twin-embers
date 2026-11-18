/* The 3DS's local wireless (UDS) as a WlBackend (3ds_wl_uds.h). */
#include <string.h>

#include "3ds_wl_uds.h"

#ifdef __DEVKITARM__
#include <3ds/os.h>
#else
#include <time.h>
#endif

#define BEACON_MAGIC0 'T'
#define BEACON_MAGIC1 'E'
#define BEACON_BYTES (2 + WL_UDS_BUILD_BYTES + 4)
#define SCAN_BYTES 0x4000
#define EVENT_MAX ((unsigned)(sizeof(((WlUds *)0)->events) / sizeof(((WlUds *)0)->events[0])))
#define IN_MAX ((unsigned)(sizeof(((WlUds *)0)->in) / sizeof(((WlUds *)0)->in[0])))

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

/* The passphrase of this build's networks: the build id in hex, so another build cannot join even by finding the network. */
static size_t Passphrase(const WlUds *uds, char out[64])
{
    static const char kPrefix[] = "twinembers-";
    size_t n = sizeof(kPrefix) - 1;

    memcpy(out, kPrefix, n);
    for (int i = 0; i < WL_UDS_BUILD_BYTES; ++i)
    {
        out[n++] = "0123456789abcdef"[uds->build[i] >> 4];
        out[n++] = "0123456789abcdef"[uds->build[i] & 15];
    }
    out[n++] = 0;
    return n;                               /* the terminator counts: UDS takes the size of the buffer */
}

static void Event(WlUds *uds, uint8_t kind, uint8_t node)
{
    unsigned at;

    if (uds->eventCount >= EVENT_MAX)
        return;
    at = (uds->eventHead + uds->eventCount++) % EVENT_MAX;
    uds->events[at].kind = kind;
    uds->events[at].node = node;
}

/* Who is in the network now, from UDS's connection status; the differences become events. A status that has no node for this
 * console is a lost link. */
static void Refresh(WlUds *uds)
{
    udsConnectionStatus st;
    uint32_t members = 0;

    if (R_FAILED(udsGetConnectionStatus(&st)) || st.cur_NetworkNodeID < 1 || st.cur_NetworkNodeID > WL_MAX_NODES || st.node_bitmask == 0)
    {
        if (uds->active)
        {
            for (unsigned n = 1; n <= WL_MAX_NODES; ++n)
                if (uds->members >> n & 1)
                    Event(uds, WL_UDS_EVENT_LEFT, (uint8_t)n);
            uds->members = 0;
            uds->active = false;
        }
        return;
    }
    uds->node = st.cur_NetworkNodeID;
    for (unsigned n = 1; n <= WL_MAX_NODES; ++n)
        if ((st.node_bitmask >> (n - 1) & 1) && n != uds->node)
            members |= 1u << n;
    for (unsigned n = 1; n <= WL_MAX_NODES; ++n)
    {
        uint32_t bit = 1u << n;

        if ((members & bit) && !(uds->members & bit))
            Event(uds, WL_UDS_EVENT_JOINED, (uint8_t)n);
        else if (!(members & bit) && (uds->members & bit))
            Event(uds, WL_UDS_EVENT_LEFT, (uint8_t)n);
    }
    uds->members = members;
}

static void Reset(WlUds *uds)
{
    uds->active = false;
    uds->hosting = false;
    uds->node = 0;
    uds->members = 0;
    uds->eventHead = uds->eventCount = 0;
    uds->inHead = uds->inCount = 0;
}

bool WlUds_Init(WlUds *uds, const uint8_t build[WL_UDS_BUILD_BYTES], const char *username)
{
    memset(uds, 0, sizeof(*uds));
    memcpy(uds->build, build, WL_UDS_BUILD_BYTES);
    if (R_FAILED(udsInit(WL_UDS_SHARED_BYTES, username)))
        return false;
    uds->ready = true;
    return true;
}

void WlUds_Exit(WlUds *uds)
{
    if (!uds->ready)
        return;
    WlUds_Leave(uds);
    udsExit();
    uds->ready = false;
}

bool WlUds_Host(WlUds *uds, uint8_t maxNodes, uint32_t token)
{
    udsNetworkStruct network;
    char pass[64];
    uint8_t beacon[BEACON_BYTES];
    size_t passLen;

    if (!uds->ready || uds->active || maxNodes < 2 || maxNodes > WL_MAX_NODES)
        return false;
    Reset(uds);
    passLen = Passphrase(uds, pass);
    udsGenerateDefaultNetworkStruct(&network, WL_UDS_WLANCOMM_ID, WL_UDS_ID8, maxNodes);
    if (R_FAILED(udsCreateNetwork(&network, pass, passLen, &uds->bind, WL_UDS_CHANNEL, UDS_DEFAULT_RECVBUFSIZE)))
        return false;
    beacon[0] = BEACON_MAGIC0;
    beacon[1] = BEACON_MAGIC1;
    memcpy(beacon + 2, uds->build, WL_UDS_BUILD_BYTES);
    for (int i = 0; i < 4; ++i)
        beacon[2 + WL_UDS_BUILD_BYTES + i] = (uint8_t)(token >> (8 * i));
    udsSetApplicationData(beacon, sizeof(beacon));          /* what a scan checks: a failure here only hides the host */
    uds->active = true;
    uds->hosting = true;
    Refresh(uds);
    return uds->active;
}

int WlUds_Scan(WlUds *uds, WlUdsHost *out, int max)
{
    static uint8_t outbuf[SCAN_BYTES];          /* UDS wants a word-aligned buffer that outlives the call */
    udsNetworkScanInfo *networks = NULL;
    size_t total = 0;
    int found = 0;

    if (!uds->ready || max <= 0 || R_FAILED(udsScanBeacons(outbuf, sizeof(outbuf), &networks, &total, WL_UDS_WLANCOMM_ID, WL_UDS_ID8, NULL, uds->active)))
        return 0;
    for (size_t i = 0; i < total && found < max; ++i)
    {
        uint8_t data[BEACON_BYTES + 8];
        size_t actual = 0;

        if (R_FAILED(udsGetNetworkStructApplicationData(&networks[i].network, data, sizeof(data), &actual)) || actual != BEACON_BYTES
            || data[0] != BEACON_MAGIC0 || data[1] != BEACON_MAGIC1 || memcmp(data + 2, uds->build, WL_UDS_BUILD_BYTES) != 0)
            continue;                           /* another application, or another build */
        memset(&out[found], 0, sizeof(out[found]));
        out[found].network = networks[i].network;
        out[found].nodes = networks[i].network.total_nodes;
        out[found].maxNodes = networks[i].network.max_nodes;
        out[found].token = (uint32_t)(data[18] | data[19] << 8 | data[20] << 16 | (uint32_t)data[21] << 24);
        if (R_FAILED(udsGetNodeInfoUsername(&networks[i].nodes[0], out[found].name)))
            out[found].name[0] = 0;
        out[found].name[sizeof(out[found].name) - 1] = 0;
        ++found;
    }
    return found;
}

bool WlUds_Join(WlUds *uds, const WlUdsHost *host)
{
    char pass[64];
    size_t passLen;

    if (!uds->ready || uds->active)
        return false;
    Reset(uds);
    passLen = Passphrase(uds, pass);
    if (R_FAILED(udsConnectNetwork(&host->network, pass, passLen, &uds->bind, UDS_BROADCAST_NETWORKNODEID, UDSCONTYPE_Client,
                                   WL_UDS_CHANNEL, UDS_DEFAULT_RECVBUFSIZE)))
        return false;
    uds->active = true;
    Refresh(uds);
    return uds->active;
}

void WlUds_Leave(WlUds *uds)
{
    if (!uds->ready || !(uds->active || uds->node))
        return;
    udsUnbind(&uds->bind);
    if (uds->hosting)
        udsDestroyNetwork();
    else
        udsDisconnectNetwork();
    Reset(uds);
}

void WlUds_Pump(WlUds *uds)
{
    size_t actual;
    uint16_t src;

    if (!uds->ready || !uds->active)
        return;
    if (udsWaitConnectionStatusEvent(false, false))
        Refresh(uds);
    if (!uds->active)
        return;
    while (uds->inCount < IN_MAX)
    {
        unsigned at = (uds->inHead + uds->inCount) % IN_MAX;

        actual = 0;
        if (R_FAILED(udsPullPacket(&uds->bind, uds->buffer, sizeof(uds->buffer), &actual, &src)) || actual == 0)
            break;
        if (actual > sizeof(uds->in[at].data) || src < 1 || src > WL_MAX_NODES)
            continue;                           /* not one of ours (too long for the link layer): dropped */
        uds->in[at].src = src;
        uds->in[at].len = (uint16_t)actual;
        memcpy(uds->in[at].data, uds->buffer, actual);
        ++uds->inCount;
    }
}

bool WlUds_PopEvent(WlUds *uds, int *kind, uint16_t *node)
{
    if (!uds->eventCount)
        return false;
    *kind = uds->events[uds->eventHead].kind;
    *node = uds->events[uds->eventHead].node;
    uds->eventHead = (uint8_t)((uds->eventHead + 1) % EVENT_MAX);
    --uds->eventCount;
    return true;
}

static bool BackendSend(void *user, uint16_t dest, const void *data, uint16_t len)
{
    WlUds *uds = user;
    Result r;

    if (!uds->active || dest < 1 || dest > WL_MAX_NODES || len == 0 || len > UDS_DATAFRAME_MAXSIZE)
        return false;
    r = udsSendTo(dest, WL_UDS_CHANNEL, UDS_SENDFLAG_Default, data, len);
    return !(R_FAILED(r) && (uint32_t)r != 0xC86113F0u);     /* a frame UDS does not send for a reason it allows is lost, as on a radio */
}

static bool BackendRecv(void *user, uint16_t *src, void *data, uint16_t max, uint16_t *len)
{
    WlUds *uds = user;
    uint16_t n;

    WlUds_Pump(uds);
    if (!uds->inCount)
        return false;
    n = uds->in[uds->inHead].len;
    if (n > max)
        n = max;
    *src = uds->in[uds->inHead].src;
    memcpy(data, uds->in[uds->inHead].data, n);
    *len = n;
    uds->inHead = (uint8_t)((uds->inHead + 1) % IN_MAX);
    --uds->inCount;
    return true;
}

static uint32_t BackendNow(void *user)
{
    (void)user;
    return NowMs();
}

WlBackend WlUds_Backend(WlUds *uds)
{
    WlBackend io = {uds, BackendSend, BackendRecv, BackendNow};

    return io;
}
