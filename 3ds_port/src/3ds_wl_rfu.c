/* WlRfu: the Wireless Adapter as librfu sees it, over WlNet messages (3ds_wl.h).
 *
 * What the formats are comes from librfu's own code (what it reads out of each acknowledgement and writes into each
 * request); the message protocol between consoles is this file's own:
 *
 *   BEACON      host to every member, now and then: open?, free slots, adapter id, game data (16 bytes), user name (8)
 *   CONNECT     child to host: adapter id
 *   ACCEPT      host to child: the slot it has
 *   REJECT      host to child
 *   DISCONNECT  either way
 *   FRAME       a link frame: the slot, librfu's 32-bit header, the bytes
 *
 * The console's librfu is the clock master except after a command that changes it (DataTxAndChange, MS_Change,
 * ResumeRetransmitAndChange); then the adapter talks first, through WlRfu_PollSlave: "data ready" (0x28) when what the
 * console is waiting for has come, or "disconnected" (0x29).
 */
#include <string.h>

#include "3ds_wl.h"

enum { M_BEACON = 1, M_CONNECT, M_ACCEPT, M_REJECT, M_DISCONNECT, M_FRAME };
enum { BEACON_EVERY_MS = 200, HOST_LISTED_MS = 2000, EXCHANGE_WAIT_MS = 8, EMPTY_WAIT_MS = 16, SIGNAL = 0xA0 };
enum { MAX_PARAM_WORDS = WL_RFU_ACK_WORDS - 1, RX_BYTES_MAX = MAX_PARAM_WORDS * 4 };

static uint32_t Header(unsigned words, unsigned command)
{
    return 0x99660000u | (uint32_t)words << 8 | command;
}

static unsigned CountChildren(const WlRfu *rfu)
{
    unsigned n = 0;

    for (unsigned i = 0; i < WL_RFU_CHILD_MAX; ++i)
        n += rfu->childUsed[i] ? 1u : 0u;
    return n;
}

static uint8_t ChildMask(const WlRfu *rfu)
{
    uint8_t mask = 0;

    for (unsigned i = 0; i < WL_RFU_CHILD_MAX; ++i)
        if (rfu->childUsed[i])
            mask |= (uint8_t)(1u << i);
    return mask;
}

void WlRfu_Init(WlRfu *rfu, WlNet *net, uint16_t myId)
{
    memset(rfu, 0, sizeof(*rfu));
    rfu->net = net;
    rfu->myId = myId ? myId : 1;
    rfu->availSlots = WL_RFU_CHILD_MAX;
    rfu->connectStatus = 3;
}

void WlRfu_SetMembers(WlRfu *rfu, uint32_t members)
{
    rfu->members = members;
}

bool WlRfu_IsSlave(const WlRfu *rfu)
{
    return rfu->slave;
}

static void Send(WlRfu *rfu, uint16_t node, const uint8_t *data, uint16_t len)
{
    WlNet_Send(rfu->net, node, data, len);
}

static void SendType(WlRfu *rfu, uint16_t node, uint8_t type, uint8_t arg)
{
    uint8_t m[2] = {type, arg};

    Send(rfu, node, m, 2);
}

/* Tell a node we are leaving and let the radio have it at once: forgetting the node would drop it from the queue. */
static void Farewell(WlRfu *rfu, uint16_t node)
{
    SendType(rfu, node, M_DISCONNECT, 0);
    WlNet_Step(rfu->net);
    WlNet_Forget(rfu->net, node);
}

/* Leave whatever the link was: tell the other side, clear the slots. */
static void DropLink(WlRfu *rfu)
{
    if (rfu->state == WL_STATE_CLIENT || rfu->state == WL_STATE_CONNECTING)
    {
        if (rfu->parentNode)
            Farewell(rfu, rfu->parentNode);
    }
    else
    {
        for (unsigned i = 0; i < WL_RFU_CHILD_MAX; ++i)
            if (rfu->childUsed[i])
                Farewell(rfu, rfu->childNode[i]);
    }
    memset(rfu->childUsed, 0, sizeof(rfu->childUsed));
    memset(rfu->rxNew, 0, sizeof(rfu->rxNew));          /* and what came on it */
    memset(rfu->rxqCount, 0, sizeof(rfu->rxqCount));
    rfu->parentNode = 0;
    rfu->parentId = 0;
    rfu->mySlot = 0;
    rfu->connectStatus = 3;
    memset(rfu->rxNew, 0, sizeof(rfu->rxNew));
    memset(rfu->rxqCount, 0, sizeof(rfu->rxqCount));
    rfu->txOwed = false;
    rfu->dataReady = false;
}

static void Reject(uint32_t *ack, uint32_t reason, int *words)
{
    ack[0] = 0x996601EEu;
    ack[1] = reason;
    *words = 2;
}

static int Answer(uint32_t *ack, unsigned command, unsigned words)
{
    ack[0] = Header(words, command | 0x80);
    return (int)words + 1;
}

static void SeenAdd(WlRfu *rfu, uint16_t node, const uint8_t *m, uint16_t len)
{
    WlRfuPeer *p;

    if (len < 29 || node < 1 || node > WL_MAX_NODES)
        return;
    p = &rfu->seen[node];
    p->used = true;
    p->node = node;
    p->hosting = true;
    p->open = m[1] != 0;
    p->slotsFree = m[2];
    p->id = (uint16_t)(m[3] | m[4] << 8);
    memcpy(p->game, m + 5, WL_RFU_GAME_BYTES);
    memcpy(p->user, m + 5 + WL_RFU_GAME_BYTES, WL_RFU_USER_BYTES);
    p->heardMs = rfu->nowMs;
}

/* A slot's received frames: the one the game reads next (rx, rxNew) and those behind it (rxq). */
static void RxClear(WlRfu *rfu, unsigned slot)
{
    rfu->rxNew[slot] = false;
    rfu->rxqCount[slot] = 0;
    rfu->rxqHead[slot] = 0;
}

static void RxPut(WlRfu *rfu, unsigned slot, uint32_t header, const uint8_t *data, uint16_t bytes)
{
    if (!rfu->rxNew[slot]) {
        rfu->rxHeader[slot] = header;
        memcpy(rfu->rx[slot], data, bytes);
        rfu->rxLen[slot] = (uint8_t)bytes;
        rfu->rxNew[slot] = true;
        return;
    }
    if (rfu->rxqCount[slot] == WL_RFU_RX_QUEUE) {
        rfu->rxqHead[slot] = (uint8_t)((rfu->rxqHead[slot] + 1) % WL_RFU_RX_QUEUE);
        --rfu->rxqCount[slot];
        ++rfu->rxDropped;
    }
    {
        unsigned at = (rfu->rxqHead[slot] + rfu->rxqCount[slot]) % WL_RFU_RX_QUEUE;

        rfu->rxqHeader[slot][at] = header;
        memcpy(rfu->rxq[slot][at], data, bytes);
        rfu->rxqLen[slot][at] = (uint8_t)bytes;
        ++rfu->rxqCount[slot];
    }
}

/* The game read the slot's frame: the next one that came, if any, is now the one to read. */
static void RxTaken(WlRfu *rfu, unsigned slot)
{
    rfu->rxNew[slot] = false;
    if (rfu->rxqCount[slot]) {
        unsigned at = rfu->rxqHead[slot];

        rfu->rxHeader[slot] = rfu->rxqHeader[slot][at];
        memcpy(rfu->rx[slot], rfu->rxq[slot][at], rfu->rxqLen[slot][at]);
        rfu->rxLen[slot] = rfu->rxqLen[slot][at];
        rfu->rxNew[slot] = true;
        rfu->rxqHead[slot] = (uint8_t)((at + 1) % WL_RFU_RX_QUEUE);
        --rfu->rxqCount[slot];
    }
}

static void OnMessage(WlRfu *rfu, uint16_t src, const WlMessage *msg)
{
    const uint8_t *m = msg->data;

    if (msg->len < 1)
        return;
    switch (m[0])
    {
    case M_BEACON:
        SeenAdd(rfu, src, m, msg->len);
        break;
    case M_CONNECT:
    {
        int slot = -1;

        if (msg->len < 3 || (rfu->state != WL_STATE_HOST_OPEN && rfu->state != WL_STATE_HOST_CLOSED))
        {
            SendType(rfu, src, M_REJECT, 0);
            break;
        }
        for (unsigned i = 0; i < WL_RFU_CHILD_MAX; ++i)
            if (rfu->childUsed[i] && rfu->childNode[i] == src)
                slot = (int)i;                      /* asked again: the same slot */
        if (slot < 0 && rfu->state == WL_STATE_HOST_OPEN && CountChildren(rfu) < rfu->availSlots)
            for (unsigned i = 0; i < WL_RFU_CHILD_MAX && slot < 0; ++i)
                if (!rfu->childUsed[i])
                {
                    slot = (int)i;
                    rfu->childUsed[i] = true;
                    rfu->childNode[i] = src;
                    rfu->childId[i] = (uint16_t)(m[1] | m[2] << 8);
                    RxClear(rfu, i);
                }
        if (slot < 0)
            SendType(rfu, src, M_REJECT, 0);
        else
        {
            rfu->childHeard[slot] = rfu->nowMs;
            SendType(rfu, src, M_ACCEPT, (uint8_t)slot);
        }
        break;
    }
    case M_ACCEPT:
        if (rfu->state == WL_STATE_CONNECTING && src == rfu->parentNode && msg->len >= 2 && m[1] < WL_RFU_CHILD_MAX)
        {
            rfu->mySlot = m[1];
            RxClear(rfu, 0);                        /* nothing of an earlier link */
            rfu->state = WL_STATE_CLIENT;
            rfu->connectStatus = 0;
        }
        break;
    case M_REJECT:
        if (rfu->state == WL_STATE_CONNECTING && src == rfu->parentNode)
        {
            rfu->state = WL_STATE_IDLE;
            rfu->connectStatus = 2;
            rfu->parentNode = 0;
        }
        break;
    case M_DISCONNECT:
        if ((rfu->state == WL_STATE_CLIENT || rfu->state == WL_STATE_CONNECTING) && src == rfu->parentNode)
        {
            if (rfu->state == WL_STATE_CLIENT)
                rfu->lostSlots |= (uint8_t)(1u << rfu->mySlot);
            else
                rfu->connectStatus = 3;
            rfu->state = WL_STATE_IDLE;
            rfu->parentNode = 0;
        }
        else
            for (unsigned i = 0; i < WL_RFU_CHILD_MAX; ++i)
                if (rfu->childUsed[i] && rfu->childNode[i] == src)
                {
                    rfu->childUsed[i] = false;
                    RxClear(rfu, i);
                    rfu->lostSlots |= (uint8_t)(1u << i);
                }
        break;
    case M_FRAME:
    {
        uint16_t bytes;

        if (msg->len < 6)
            break;
        bytes = (uint16_t)(msg->len - 6);
        if (bytes > WL_RFU_PARENT_FRAME + 4 || bytes > sizeof(rfu->rx[0]))
            break;
        if (rfu->state == WL_STATE_CLIENT && src == rfu->parentNode)
        {
            RxPut(rfu, 0, (uint32_t)(m[2] | m[3] << 8 | m[4] << 16 | (uint32_t)m[5] << 24), m + 6, bytes);
        }
        else if (rfu->state == WL_STATE_HOST_OPEN || rfu->state == WL_STATE_HOST_CLOSED)
            for (unsigned i = 0; i < WL_RFU_CHILD_MAX; ++i)
                if (rfu->childUsed[i] && rfu->childNode[i] == src)
                {
                    RxPut(rfu, i, (uint32_t)(m[2] | m[3] << 8 | m[4] << 16 | (uint32_t)m[5] << 24), m + 6, bytes);
                    rfu->childHeard[i] = rfu->nowMs;
                }
        break;
    }
    default:
        break;
    }
}

static void SendBeacons(WlRfu *rfu)
{
    uint8_t m[29];

    m[0] = M_BEACON;
    m[1] = rfu->state == WL_STATE_HOST_OPEN ? 1 : 0;
    m[2] = (uint8_t)(rfu->availSlots > CountChildren(rfu) ? rfu->availSlots - CountChildren(rfu) : 0);
    m[3] = (uint8_t)rfu->myId;
    m[4] = (uint8_t)(rfu->myId >> 8);
    memcpy(m + 5, rfu->game, WL_RFU_GAME_BYTES);
    memcpy(m + 5 + WL_RFU_GAME_BYTES, rfu->user, WL_RFU_USER_BYTES);
    for (uint16_t node = 1; node <= WL_MAX_NODES; ++node)
        if (rfu->members >> node & 1)
            Send(rfu, node, m, sizeof(m));
}

void WlRfu_Step(WlRfu *rfu)
{
    uint16_t src, lost;
    WlMessage msg;

    rfu->nowMs = rfu->net->io.now(rfu->net->io.user);
    WlNet_Step(rfu->net);
    while (WlNet_Pop(rfu->net, &src, &msg))
        OnMessage(rfu, src, &msg);
    while (WlNet_PopLost(rfu->net, &lost))
    {
        WlMessage gone = {1, {M_DISCONNECT}};

        OnMessage(rfu, lost, &gone);
    }
    if ((rfu->state == WL_STATE_HOST_OPEN || rfu->state == WL_STATE_HOST_CLOSED) && rfu->configured
        && rfu->nowMs - rfu->lastBeacon >= BEACON_EVERY_MS)
    {
        rfu->lastBeacon = rfu->nowMs;
        SendBeacons(rfu);
    }
}

static uint8_t Strength(const WlRfu *rfu, unsigned slot)
{
    if (rfu->state == WL_STATE_CLIENT)
        return slot == rfu->mySlot ? SIGNAL : 0;
    return (rfu->state == WL_STATE_HOST_OPEN || rfu->state == WL_STATE_HOST_CLOSED) && rfu->childUsed[slot] ? SIGNAL : 0;
}

static unsigned ListChildren(const WlRfu *rfu, uint32_t *out)
{
    unsigned n = 0;

    for (unsigned i = 0; i < WL_RFU_CHILD_MAX; ++i)
        if (rfu->childUsed[i])
            out[n++] = rfu->childId[i] | (uint32_t)i << 16;
    return n;
}

/* The words of a connect poll: the parent's id, the slot, and the status in the top byte. */
static uint32_t ConnectWord(const WlRfu *rfu)
{
    return (uint32_t)rfu->tryId | (uint32_t)rfu->mySlot << 16 | (uint32_t)rfu->connectStatus << 24;
}

static void StartSearch(WlRfu *rfu)
{
    memset(rfu->seen, 0, sizeof(rfu->seen));
}

int WlRfu_Command(WlRfu *rfu, const uint32_t *req, uint32_t *ack)
{
    unsigned command = req[0] & 0xFF, count = req[0] >> 8 & 0xFF, n;
    int words = 0;

    rfu->nowMs = rfu->net->io.now(rfu->net->io.user);
    if ((req[0] >> 16) != 0x9966 || count > MAX_PARAM_WORDS)
    {
        Reject(ack, 1, &words);
        return words;
    }
    switch (command)
    {
    case WLC_RESET:
        DropLink(rfu);
        rfu->state = WL_STATE_IDLE;
        rfu->configured = false;
        rfu->slave = false;
        rfu->lostSlots = 0;
        memset(rfu->seen, 0, sizeof(rfu->seen));
        return Answer(ack, command, 0);
    case WLC_LINK_STATUS:
        ack[1] = 0;
        for (unsigned i = 0; i < WL_RFU_CHILD_MAX; ++i)
            ack[1] |= (uint32_t)Strength(rfu, i) << (8 * i);
        return Answer(ack, command, 1);
    case WLC_VERSION:
        ack[1] = 0x00830117u;
        return Answer(ack, command, 1);
    case WLC_SYSTEM_STATUS:
        ack[1] = (uint32_t)rfu->myId | (uint32_t)(rfu->state == WL_STATE_CLIENT ? 1u << rfu->mySlot : ChildMask(rfu)) << 16
               | (uint32_t)rfu->state << 24;
        return Answer(ack, command, 1);
    case WLC_SLOT_STATUS:
        n = ListChildren(rfu, ack + 2);
        ack[1] = n;
        return Answer(ack, command, 1 + n);
    case WLC_CONFIG_STATUS:
        ack[1] = ack[2] = ack[3] = 0;
        return Answer(ack, command, 3);
    case WLC_GAME_CONFIG:
        if (count < 6)
        {
            Reject(ack, 1, &words);
            return words;
        }
        memcpy(rfu->game, req + 1, WL_RFU_GAME_BYTES);
        memcpy(rfu->user, (const uint8_t *)(req + 1) + WL_RFU_GAME_BYTES, WL_RFU_USER_BYTES);
        rfu->configured = true;
        return Answer(ack, command, 0);
    case WLC_SYSTEM_CONFIG:
        if (count < 1)
        {
            Reject(ack, 1, &words);
            return words;
        }
        rfu->availSlots = (uint8_t)(WL_RFU_CHILD_MAX - ((req[1] >> 16) & 3));
        return Answer(ack, command, 0);
    case WLC_SC_START:
        if (!rfu->configured || rfu->state == WL_STATE_CLIENT || rfu->state == WL_STATE_CONNECTING || rfu->state == WL_STATE_SEARCHING)
        {
            Reject(ack, 1, &words);
            return words;
        }
        rfu->state = WL_STATE_HOST_OPEN;
        rfu->lastBeacon = rfu->nowMs - BEACON_EVERY_MS;
        return Answer(ack, command, 0);
    case WLC_SC_POLL:
    case WLC_SC_END:
        if (rfu->state != WL_STATE_HOST_OPEN && rfu->state != WL_STATE_HOST_CLOSED)
        {
            ack[0] = Header(0, command | 0x80);
            return 1;
        }
        n = ListChildren(rfu, ack + 1);
        if (command == WLC_SC_END)
            rfu->state = n ? WL_STATE_HOST_CLOSED : WL_STATE_IDLE;
        return Answer(ack, command, n);
    case WLC_SP_START:
        if (rfu->state == WL_STATE_CLIENT || rfu->state == WL_STATE_CONNECTING || rfu->state == WL_STATE_HOST_OPEN
            || rfu->state == WL_STATE_HOST_CLOSED)
        {
            Reject(ack, 1, &words);
            return words;
        }
        rfu->state = WL_STATE_SEARCHING;
        StartSearch(rfu);
        return Answer(ack, command, 0);
    case WLC_SP_POLL:
    {
        unsigned found = 0;

        for (uint16_t node = 1; node <= WL_MAX_NODES && found < WL_RFU_CHILD_MAX; ++node)
        {
            const WlRfuPeer *p = &rfu->seen[node];
            uint8_t *e;

            if (!p->used || !p->hosting || !p->open || rfu->nowMs - p->heardMs > HOST_LISTED_MS)
                continue;
            e = (uint8_t *)(ack + 1 + found * 7);
            e[0] = (uint8_t)p->id;
            e[1] = (uint8_t)(p->id >> 8);
            e[2] = 0;                                   /* the slot the parent would give: the parent chooses */
            e[3] = 0;
            memcpy(e + 4, p->game, WL_RFU_GAME_BYTES);
            memcpy(e + 4 + WL_RFU_GAME_BYTES, p->user, WL_RFU_USER_BYTES);
            ++found;
        }
        return Answer(ack, command, found * 7);
    }
    case WLC_SP_END:
        if (rfu->state == WL_STATE_SEARCHING)
            rfu->state = WL_STATE_IDLE;
        return Answer(ack, command, 0);
    case WLC_CP_START:
    {
        uint16_t id = count >= 1 ? (uint16_t)req[1] : 0;

        for (uint16_t node = 1; node <= WL_MAX_NODES; ++node)
            if (rfu->seen[node].used && rfu->seen[node].id == id && rfu->seen[node].hosting)
            {
                uint8_t m[3] = {M_CONNECT, (uint8_t)rfu->myId, (uint8_t)(rfu->myId >> 8)};

                rfu->tryId = id;
                rfu->parentNode = node;
                rfu->parentId = id;
                rfu->state = WL_STATE_CONNECTING;
                rfu->connectStatus = 1;
                Send(rfu, node, m, sizeof(m));
                return Answer(ack, command, 0);
            }
        Reject(ack, 1, &words);
        return words;
    }
    case WLC_CP_POLL:
        ack[1] = ConnectWord(rfu);
        return Answer(ack, command, 1);
    case WLC_CP_END:
        if (rfu->state == WL_STATE_CONNECTING)
        {
            Farewell(rfu, rfu->parentNode);
            rfu->state = WL_STATE_IDLE;
            rfu->connectStatus = 3;
            rfu->parentNode = 0;
        }
        ack[1] = ConnectWord(rfu);
        return Answer(ack, command, 1);
    case WLC_DATA_TX:
    case WLC_DATA_TX_AND_CHANGE:
    case WLC_RESUME_RETRANSMIT_AND_CHANGE:
        if (command != WLC_RESUME_RETRANSMIT_AND_CHANGE && count >= 1)
        {
            uint16_t bytes = (uint16_t)((count - 1) * 4);
            uint8_t m[6 + RX_BYTES_MAX];

            if (bytes > WL_RFU_PARENT_FRAME + 4)
                bytes = WL_RFU_PARENT_FRAME + 4;
            m[0] = M_FRAME;
            m[5] = (uint8_t)(req[1] >> 24);
            m[2] = (uint8_t)req[1];
            m[3] = (uint8_t)(req[1] >> 8);
            m[4] = (uint8_t)(req[1] >> 16);
            memcpy(m + 6, req + 2, bytes);
            if (rfu->state == WL_STATE_CLIENT)
            {
                m[1] = rfu->mySlot;
                Send(rfu, rfu->parentNode, m, (uint16_t)(6 + bytes));
            }
            else if (rfu->state == WL_STATE_HOST_OPEN || rfu->state == WL_STATE_HOST_CLOSED)
                for (unsigned i = 0; i < WL_RFU_CHILD_MAX; ++i)
                    if (rfu->childUsed[i])
                    {
                        m[1] = (uint8_t)i;
                        Send(rfu, rfu->childNode[i], m, (uint16_t)(6 + bytes));
                    }
            rfu->txOwed = true;
            rfu->txMs = rfu->nowMs;
        }
        if (command != WLC_DATA_TX)
            rfu->slave = true;
        return Answer(ack, command, 0);
    case WLC_DATA_RX:
        if (rfu->state == WL_STATE_CLIENT)
        {
            if (!rfu->rxNew[0])
                return Answer(ack, command, 0);
            ack[1] = rfu->rxHeader[0];
            n = (rfu->rxLen[0] + 3u) / 4u;
            if (n > MAX_PARAM_WORDS - 1)
                n = MAX_PARAM_WORDS - 1;
            memset(ack + 2, 0, n * 4);
            memcpy(ack + 2, rfu->rx[0], rfu->rxLen[0] < n * 4 ? rfu->rxLen[0] : n * 4);
            RxTaken(rfu, 0);
            rfu->txOwed = false;
            return Answer(ack, command, 1 + n);
        }
        else if (rfu->state == WL_STATE_HOST_OPEN || rfu->state == WL_STATE_HOST_CLOSED)
        {
            uint32_t sizes = 0;
            unsigned bytes = 0;
            uint8_t *out = (uint8_t *)(ack + 2);

            for (unsigned i = 0; i < WL_RFU_CHILD_MAX; ++i)
            {
                unsigned len;

                if (!rfu->childUsed[i] || !rfu->rxNew[i])
                    continue;
                len = rfu->rxHeader[i] >> (8 + 5 * i) & 0x1F;
                if (len > rfu->rxLen[i])
                    len = rfu->rxLen[i];
                if (bytes + len > RX_BYTES_MAX - 4)
                    break;                              /* the rest waits for the next DataRx */
                memcpy(out + bytes, rfu->rx[i], len);
                bytes += len;
                sizes |= (uint32_t)len << (8 + 5 * i);
                RxTaken(rfu, i);
            }
            if (sizes == 0 && bytes == 0)
                return Answer(ack, command, 0);
            n = (bytes + 3u) / 4u;
            memset(out + bytes, 0, n * 4 - bytes);
            ack[1] = sizes;
            rfu->txOwed = false;
            return Answer(ack, command, 1 + n);
        }
        return Answer(ack, command, 0);
    case WLC_MS_CHANGE:
        rfu->slave = true;
        rfu->txMs = rfu->nowMs;                 /* a host waits its turn from now: others may still be answering */
        return Answer(ack, command, 0);
    case WLC_DISCONNECT:
    {
        uint32_t mask = count >= 1 ? req[1] & 0xFF : 0;

        if (rfu->state == WL_STATE_CLIENT || rfu->state == WL_STATE_CONNECTING)
        {
            DropLink(rfu);
            rfu->state = WL_STATE_IDLE;
        }
        else
            for (unsigned i = 0; i < WL_RFU_CHILD_MAX; ++i)
                if ((mask >> i & 1) && rfu->childUsed[i])
                {
                    Farewell(rfu, rfu->childNode[i]);
                    rfu->childUsed[i] = false;
                    RxClear(rfu, i);
                }
        if (rfu->state == WL_STATE_HOST_CLOSED && CountChildren(rfu) == 0)
            rfu->state = WL_STATE_IDLE;
        return Answer(ack, command, 0);
    }
    case WLC_STOP_MODE:
        DropLink(rfu);
        rfu->state = WL_STATE_IDLE;
        rfu->slave = false;
        return Answer(ack, command, 0);
    case WLC_TEST_MODE:
        return Answer(ack, command, 0);
    case WLC_CPR_START:
        rfu->connectStatus = 1;
        return Answer(ack, command, 0);
    case WLC_CPR_POLL:
    case WLC_CPR_END:
        ack[1] = (uint32_t)rfu->tryId | 1u << 24;       /* recovery is not offered: it has failed */
        return Answer(ack, command, 1);
    default:
        Reject(ack, (command >= 0x10 && command <= 0x3D) ? 1 : 2, &words);
        return words;
    }
}

int WlRfu_PollSlave(WlRfu *rfu, uint32_t *req)
{
    bool host = rfu->state == WL_STATE_HOST_OPEN || rfu->state == WL_STATE_HOST_CLOSED;

    if (!rfu->slave)
        return 0;
    if (rfu->lostSlots)
    {
        req[0] = Header(1, WLC_DISCONNECTED_AND_CHANGE);
        req[1] = rfu->lostSlots | 0u << 8;              /* slots, then the reason: 0 disconnected */
        rfu->lostSlots = 0;
        rfu->slave = false;
        return 2;
    }
    if (host)
    {
        uint8_t answered = 0, connected = ChildMask(rfu);

        for (unsigned i = 0; i < WL_RFU_CHILD_MAX; ++i)
            if (rfu->childUsed[i] && rfu->rxNew[i])
                answered |= (uint8_t)(1u << i);
        if (connected ? (answered != connected && rfu->nowMs - rfu->txMs < EXCHANGE_WAIT_MS)
                      : rfu->nowMs - rfu->txMs < EMPTY_WAIT_MS)
            return 0;
        rfu->slave = false;
        if (connected == 0 || answered == connected)
        {
            req[0] = Header(0, WLC_DATA_READY_AND_CHANGE);  /* everyone answered */
            return 1;
        }
        req[0] = Header(1, WLC_DATA_READY_AND_CHANGE);
        req[1] = answered;
        return 2;
    }
    if (rfu->state == WL_STATE_CLIENT && rfu->rxNew[0])
    {
        rfu->slave = false;
        req[0] = Header(0, WLC_DATA_READY_AND_CHANGE);
        return 1;
    }
    if (rfu->state != WL_STATE_CLIENT && !host)
    {
        /* the link ended while the console waited: say so once */
        req[0] = Header(1, WLC_DISCONNECTED_AND_CHANGE);
        req[1] = 0xFu;
        rfu->slave = false;
        return 2;
    }
    return 0;
}
