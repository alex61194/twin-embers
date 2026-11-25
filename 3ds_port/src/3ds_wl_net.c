/* WlNet: reliable, ordered messages between the nodes of a session over a radio that may lose frames (3ds_wl.h).
 *
 * A radio frame is: 'W', a kind (1 data, 2 acknowledgement), the sequence number of the message (data), the number of
 * the next message expected from the receiver (always: it acknowledges everything before it), the length (data) and
 * the message. A node keeps its messages for each peer in a small ring until the peer's number moves past them, and
 * sends again any that has waited WL_RESEND_MS. A peer sends only the message it expects next into the inbox; one
 * that is ahead is dropped (the sender will send it again) and one that is old is answered with an acknowledgement. */
#include <string.h>

#include "3ds_wl.h"

enum { KIND_DATA = 1, KIND_ACK = 2, HEADER = 8 };

static void Put16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static uint16_t Get16(const uint8_t *p)
{
    return (uint16_t)(p[0] | p[1] << 8);
}

void WlNet_Init(WlNet *net, const WlBackend *io, uint16_t self)
{
    memset(net, 0, sizeof(*net));
    net->io = *io;
    net->self = self;
}

static WlPeer *Peer(WlNet *net, uint16_t node)
{
    if (node < 1 || node > WL_MAX_NODES || node == net->self)
        return NULL;
    if (!net->peer[node].known)
    {
        memset(&net->peer[node], 0, sizeof(net->peer[node]));
        net->peer[node].known = true;
        net->peer[node].lastHeard = net->io.now(net->io.user);
    }
    return &net->peer[node];
}

bool WlNet_Send(WlNet *net, uint16_t dest, const void *data, uint16_t len)
{
    WlPeer *p = Peer(net, dest);
    WlMessage *m;
    unsigned at;

    if (!p || len > WL_MAX_PAYLOAD || p->count >= WL_WINDOW)
        return false;
    at = (p->head + p->count) % WL_WINDOW;
    m = &p->window[at];
    m->len = len;
    if (len)
        memcpy(m->data, data, len);
    p->sentOnce[at] = false;
    if (p->count == 0)
        p->lastProgress = net->io.now(net->io.user);
    ++p->count;
    ++p->nextSeq;
    return true;
}

static void SendData(WlNet *net, uint16_t node, WlPeer *p, unsigned at, uint16_t seq)
{
    uint8_t frame[HEADER + WL_MAX_PAYLOAD];
    const WlMessage *m = &p->window[at];

    frame[0] = 'W';
    frame[1] = KIND_DATA;
    Put16(frame + 2, seq);
    Put16(frame + 4, p->expect);
    Put16(frame + 6, m->len);
    if (m->len)
        memcpy(frame + HEADER, m->data, m->len);
    net->io.send(net->io.user, node, frame, (uint16_t)(HEADER + m->len));
    p->ackOwed = false;
}

static void SendAck(WlNet *net, uint16_t node, WlPeer *p)
{
    uint8_t frame[HEADER];

    frame[0] = 'W';
    frame[1] = KIND_ACK;
    Put16(frame + 2, 0);
    Put16(frame + 4, p->expect);
    Put16(frame + 6, 0);
    net->io.send(net->io.user, node, frame, HEADER);
    p->ackOwed = false;
}

static void Lose(WlNet *net, uint16_t node)
{
    if (net->lostCount < WL_MAX_NODES)
        net->lost[net->lostCount++] = node;
    memset(&net->peer[node], 0, sizeof(net->peer[node]));
}

void WlNet_Step(WlNet *net)
{
    uint8_t frame[HEADER + WL_MAX_PAYLOAD];
    uint16_t src, len;
    uint32_t now = net->io.now(net->io.user);

    for (int budget = 0; budget < 64 && net->io.recv(net->io.user, &src, frame, sizeof(frame), &len); ++budget)
    {
        WlPeer *p;
        uint16_t ack, seq, size, moved;

        if (len < HEADER || frame[0] != 'W' || (frame[1] != KIND_DATA && frame[1] != KIND_ACK))
            continue;
        p = Peer(net, src);
        if (!p)
            continue;
        p->lastHeard = now;
        ack = Get16(frame + 4);
        moved = (uint16_t)(ack - p->sendBase);
        if (moved > 0 && moved <= p->count)
        {
            p->head = (uint8_t)((p->head + moved) % WL_WINDOW);
            p->sendBase = (uint16_t)(p->sendBase + moved);
            p->count = (uint16_t)(p->count - moved);
            p->lastProgress = now;
        }
        if (frame[1] != KIND_DATA)
        {
            /* An acknowledgement that does not move on while messages wait means the oldest one is missing at the
             * other end: send it again now, not after the timer. */
            if (moved == 0 && p->count > 0)
                p->sentOnce[p->head] = false;
            continue;
        }
        seq = Get16(frame + 2);
        size = Get16(frame + 6);
        if (size > WL_MAX_PAYLOAD || (uint16_t)(HEADER + size) > len)
            continue;
        if (seq == p->expect && net->inboxCount < (uint8_t)(sizeof(net->inbox) / sizeof(net->inbox[0])))
        {
            unsigned at = (net->inboxHead + net->inboxCount) % (sizeof(net->inbox) / sizeof(net->inbox[0]));

            net->inbox[at].src = src;
            net->inbox[at].msg.len = size;
            memcpy(net->inbox[at].msg.data, frame + HEADER, size);
            ++net->inboxCount;
            ++p->expect;
        }
        p->ackOwed = true;      /* also for one that is old or ahead: the sender learns where we are */
    }
    for (uint16_t node = 1; node <= WL_MAX_NODES; ++node)
    {
        WlPeer *p = &net->peer[node];

        if (!p->known)
            continue;
        for (unsigned i = 0; i < p->count; ++i)
        {
            unsigned at = (p->head + i) % WL_WINDOW;

            if (!p->sentOnce[at] || now - p->lastSent[at] >= WL_RESEND_MS)
            {
                SendData(net, node, p, at, (uint16_t)(p->sendBase + i));
                p->sentOnce[at] = true;
                p->lastSent[at] = now;
            }
        }
        if (p->ackOwed)
            SendAck(net, node, p);
        if (p->count > 0 && now - p->lastProgress >= WL_SILENT_MS)
            Lose(net, node);
    }
}

bool WlNet_Pop(WlNet *net, uint16_t *src, WlMessage *msg)
{
    if (net->inboxCount == 0)
        return false;
    *src = net->inbox[net->inboxHead].src;
    *msg = net->inbox[net->inboxHead].msg;
    net->inboxHead = (uint8_t)((net->inboxHead + 1) % (sizeof(net->inbox) / sizeof(net->inbox[0])));
    --net->inboxCount;
    return true;
}

bool WlNet_PopLost(WlNet *net, uint16_t *node)
{
    if (net->lostCount == 0)
        return false;
    *node = net->lost[0];
    memmove(net->lost, net->lost + 1, (size_t)(net->lostCount - 1) * sizeof(net->lost[0]));
    --net->lostCount;
    return true;
}

void WlNet_Forget(WlNet *net, uint16_t node)
{
    if (node >= 1 && node <= WL_MAX_NODES)
        memset(&net->peer[node], 0, sizeof(net->peer[node]));
}

void WlNet_Lose(WlNet *net, uint16_t node)
{
    if (node >= 1 && node <= WL_MAX_NODES)
        Lose(net, node);
}

uint16_t WlNet_Pending(const WlNet *net, uint16_t dest)
{
    return dest >= 1 && dest <= WL_MAX_NODES ? net->peer[dest].count : 0;
}
