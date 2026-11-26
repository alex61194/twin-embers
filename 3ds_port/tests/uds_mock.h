/* A stand-in for libctru's UDS functions, with the real uds.h's types and signatures: it records what the backend asks of UDS and plays
 * back beacons, connection changes and frames. Included by tests/wl_uds_test.c and tests/wl_glue_test.c (one program has one copy). */
#ifndef CTR_UDS_MOCK_H
#define CTR_UDS_MOCK_H
#include <assert.h>
#include <string.h>

#include "3ds_wl_uds.h"

static const uint8_t kBuild[WL_UDS_BUILD_BYTES] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
static const uint8_t kOther[WL_UDS_BUILD_BYTES] = {9, 9, 9};

static struct {
    int initCalls, exitCalls;
    Result initResult;
    int created, connected, destroyed, disconnected, unbound;
    char pass[80];
    size_t passLen;
    uint8_t appdata[40];
    size_t appdataLen;
    uint8_t maxNodes;
    u16 connectRecv;
    udsConnectionStatus status;
    bool statusEvent;
    int scanN;
    udsNetworkScanInfo scan[3];
    uint8_t scanApp[3][32];
    size_t scanAppLen[3];
    struct { u16 src; uint8_t data[2000]; size_t len; } queue[8];
    int queued, queueHead;
    u16 lastDest;
    uint8_t lastData[256];
    size_t lastLen;
    Result sendResult;
} M;

/* ---- the stand-in for libctru's UDS ---- */
Result udsInit(size_t sharedmem_size, const char *username)
{
    (void)sharedmem_size; (void)username;
    ++M.initCalls;
    return M.initResult;
}

void udsExit(void)
{
    ++M.exitCalls;
}

void udsGenerateDefaultNetworkStruct(udsNetworkStruct *network, u32 wlancommID, u8 id8, u8 max_nodes)
{
    memset(network, 0, sizeof(*network));
    network->wlancommID = wlancommID;
    network->id8 = id8;
    network->max_nodes = max_nodes;
}

static void SetStatus(u16 node, u16 mask)
{
    memset(&M.status, 0, sizeof(M.status));
    M.status.cur_NetworkNodeID = node;
    M.status.node_bitmask = mask;
}

Result udsCreateNetwork(const udsNetworkStruct *network, const void *passphrase, size_t passphrase_size, udsBindContext *context, u8 data_channel,
                        u32 recv_buffer_size)
{
    (void)context; (void)data_channel; (void)recv_buffer_size;
    ++M.created;
    M.maxNodes = network->max_nodes;
    M.passLen = passphrase_size;
    memcpy(M.pass, passphrase, passphrase_size);
    SetStatus(1, 1);
    return 0;
}

Result udsSetApplicationData(const void *buf, size_t size)
{
    M.appdataLen = size;
    memcpy(M.appdata, buf, size);
    return 0;
}

Result udsScanBeacons(void *outbuf, size_t maxsize, udsNetworkScanInfo **networks, size_t *total_networks, u32 wlancommID, u8 id8,
                      const u8 *host_macaddress, bool connected)
{
    (void)outbuf; (void)maxsize; (void)host_macaddress; (void)connected;
    assert(wlancommID == WL_UDS_WLANCOMM_ID && id8 == WL_UDS_ID8);
    *networks = M.scan;
    *total_networks = (size_t)M.scanN;
    return 0;
}

Result udsGetNetworkStructApplicationData(const udsNetworkStruct *network, void *buf, size_t size, size_t *actual_size)
{
    for (int i = 0; i < M.scanN; ++i)
        if (network == &M.scan[i].network)
        {
            assert(size >= M.scanAppLen[i]);
            memcpy(buf, M.scanApp[i], M.scanAppLen[i]);
            *actual_size = M.scanAppLen[i];
            return 0;
        }
    return -1;
}

Result udsGetNodeInfoUsername(const udsNodeInfo *nodeinfo, char *username)
{
    (void)nodeinfo;
    strcpy(username, "Ale");
    return 0;
}

Result udsConnectNetwork(const udsNetworkStruct *network, const void *passphrase, size_t passphrase_size, udsBindContext *context,
                         u16 recv_NetworkNodeID, udsConnectionType connection_type, u8 data_channel, u32 recv_buffer_size)
{
    (void)network; (void)context; (void)data_channel; (void)recv_buffer_size;
    assert(connection_type == UDSCONTYPE_Client);
    ++M.connected;
    M.connectRecv = recv_NetworkNodeID;
    M.passLen = passphrase_size;
    memcpy(M.pass, passphrase, passphrase_size);
    SetStatus(2, 3);
    return 0;
}

Result udsGetConnectionStatus(udsConnectionStatus *output)
{
    *output = M.status;
    return 0;
}

bool udsWaitConnectionStatusEvent(bool nextEvent, bool wait)
{
    bool r = M.statusEvent;

    (void)nextEvent; (void)wait;
    M.statusEvent = false;
    return r;
}

Result udsPullPacket(const udsBindContext *bindcontext, void *buf, size_t size, size_t *actual_size, u16 *src_NetworkNodeID)
{
    (void)bindcontext;
    if (M.queueHead >= M.queued)
    {
        *actual_size = 0;
        return 0;
    }
    *src_NetworkNodeID = M.queue[M.queueHead].src;
    *actual_size = M.queue[M.queueHead].len < size ? M.queue[M.queueHead].len : size;
    memcpy(buf, M.queue[M.queueHead].data, *actual_size);
    ++M.queueHead;
    return 0;
}

Result udsSendTo(u16 dst_NetworkNodeID, u8 data_channel, u8 flags, const void *buf, size_t size)
{
    (void)data_channel; (void)flags;
    M.lastDest = dst_NetworkNodeID;
    M.lastLen = size;
    memcpy(M.lastData, buf, size < sizeof(M.lastData) ? size : sizeof(M.lastData));
    return M.sendResult;
}

Result udsUnbind(udsBindContext *bindcontext)
{
    (void)bindcontext;
    ++M.unbound;
    return 0;
}

Result udsDestroyNetwork(void)
{
    ++M.destroyed;
    return 0;
}

Result udsDisconnectNetwork(void)
{
    ++M.disconnected;
    return 0;
}


#endif
