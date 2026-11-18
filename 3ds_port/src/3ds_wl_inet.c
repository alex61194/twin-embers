/* The internet radio of the local-wireless layer: a WlBackend over a TCP connection to a relay (3ds_wl_inet.h,
 * docs/WL-RELAY-PROTOCOL.md). The socket is non-blocking after the connection is made; Pump reads and writes what it can. */
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef int WlLen;
#define WL_WOULD_BLOCK (WSAGetLastError() == WSAEWOULDBLOCK)
#define SK(s) ((SOCKET)(s))
#define WL_CLOSE(s) closesocket((SOCKET)(s))
#else
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
typedef ssize_t WlLen;
#define WL_WOULD_BLOCK (errno == EAGAIN || errno == EWOULDBLOCK)
#define SK(s) ((int)(s))
#define WL_CLOSE(s) close((int)(s))
#endif

#include "3ds_wl_inet.h"

enum { T_HELLO = 1, T_WELCOME, T_REJECT, T_JOINED, T_LEFT, T_DATA, T_PING, T_PONG };
enum { VERSION = 1, KEEPALIVE_MS = 15000, MAX_MESSAGE = 2048 };

static uint32_t NowMs(void)
{
#ifdef _WIN32
    return (uint32_t)GetTickCount64();
#else
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000u + ts.tv_nsec / 1000000u);
#endif
}

static void SetNonBlocking(intptr_t s, bool on)
{
#ifdef _WIN32
    u_long mode = on ? 1 : 0;

    ioctlsocket(SK(s), FIONBIO, &mode);
#else
    int flags = fcntl(SK(s), F_GETFL, 0);

    fcntl(SK(s), F_SETFL, on ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK));
#endif
}

/* Queue one message (type and body) for sending. False when the queue cannot hold it. */
static bool Queue(WlInet *net, uint8_t type, const void *body, uint16_t len)
{
    uint16_t total = (uint16_t)(len + 1);

    if (net->txLen + 2u + total > sizeof(net->tx))
        return false;
    net->tx[net->txLen++] = (uint8_t)total;
    net->tx[net->txLen++] = (uint8_t)(total >> 8);
    net->tx[net->txLen++] = type;
    if (len)
        memcpy(net->tx + net->txLen, body, len);
    net->txLen = (uint16_t)(net->txLen + len);
    return true;
}

static void Flush(WlInet *net)
{
    while (net->sock >= 0 && net->txLen)
    {
        WlLen n = send(SK(net->sock), (const char *)net->tx, net->txLen, 0);

        if (n <= 0)
        {
            if (n < 0 && !WL_WOULD_BLOCK)
                WlInet_Close(net);
            return;
        }
        memmove(net->tx, net->tx + n, (size_t)(net->txLen - n));
        net->txLen = (uint16_t)(net->txLen - n);
        net->lastTx = NowMs();
    }
}

static void Event(WlInet *net, uint8_t kind, uint8_t node)
{
    unsigned at;

    if (net->eventCount >= sizeof(net->events) / sizeof(net->events[0]))
        return;                         /* the oldest stay: a console that never reads them has no use for more */
    at = (net->eventHead + net->eventCount++) % (sizeof(net->events) / sizeof(net->events[0]));
    net->events[at].kind = kind;
    net->events[at].node = node;
}

static void Handle(WlInet *net, const uint8_t *m, uint16_t len)
{
    switch (m[0])
    {
    case T_WELCOME:
        if (len >= 6 && m[1] >= 1 && m[1] <= WL_MAX_NODES)
        {
            net->node = m[1];
            net->members = (uint32_t)(m[2] | m[3] << 8 | m[4] << 16 | (uint32_t)m[5] << 24);
            net->welcomed = true;
        }
        break;
    case T_REJECT:
        net->reject = len >= 2 ? m[1] : WL_INET_REJECT_HELLO;
        break;
    case T_JOINED:
        if (len >= 2 && m[1] >= 1 && m[1] <= WL_MAX_NODES)
        {
            net->members |= 1u << m[1];
            Event(net, WL_INET_EVENT_JOINED, m[1]);
        }
        break;
    case T_LEFT:
        if (len >= 2 && m[1] >= 1 && m[1] <= WL_MAX_NODES)
        {
            net->members &= ~(1u << m[1]);
            Event(net, WL_INET_EVENT_LEFT, m[1]);
        }
        break;
    case T_DATA:
        if (len >= 2 && net->inCount < sizeof(net->in) / sizeof(net->in[0]))
        {
            unsigned at = (net->inHead + net->inCount++) % (sizeof(net->in) / sizeof(net->in[0]));
            uint16_t n = (uint16_t)(len - 2);

            if (n > sizeof(net->in[at].data))
                n = sizeof(net->in[at].data);
            net->in[at].src = m[1];
            net->in[at].len = n;
            memcpy(net->in[at].data, m + 2, n);
        }
        break;
    default:
        break;                          /* PONG and anything unknown */
    }
}

static void Parse(WlInet *net)
{
    uint16_t at = 0;

    while (net->rxLen - at >= 2)
    {
        uint16_t length = (uint16_t)(net->rx[at] | net->rx[at + 1] << 8);

        if (length < 1 || length > MAX_MESSAGE)
        {
            WlInet_Close(net);          /* a stream that is not the protocol */
            return;
        }
        if ((unsigned)(net->rxLen - at) < 2u + length)
            break;
        Handle(net, net->rx + at + 2, length);
        at = (uint16_t)(at + 2 + length);
    }
    if (at)
    {
        memmove(net->rx, net->rx + at, (size_t)(net->rxLen - at));
        net->rxLen = (uint16_t)(net->rxLen - at);
    }
}

void WlInet_Pump(WlInet *net)
{
    while (net->sock >= 0)
    {
        WlLen n;

        if (net->rxLen >= sizeof(net->rx))
        {
            WlInet_Close(net);
            return;
        }
        n = recv(SK(net->sock), (char *)net->rx + net->rxLen, sizeof(net->rx) - net->rxLen, 0);
        if (n == 0)
        {
            WlInet_Close(net);
            return;
        }
        if (n < 0)
        {
            if (!WL_WOULD_BLOCK)
                WlInet_Close(net);
            break;
        }
        net->rxLen = (uint16_t)(net->rxLen + n);
        Parse(net);
    }
    if (net->sock >= 0 && net->welcomed && NowMs() - net->lastTx >= KEEPALIVE_MS && !net->txLen)
        Queue(net, T_PING, NULL, 0);
    Flush(net);
}

bool WlInet_Connect(WlInet *net, const char *host, uint16_t port, const char *room, const uint8_t build[WL_INET_BUILD_BYTES],
                    uint32_t timeoutMs)
{
    struct addrinfo hints, *list = NULL, *a;
    char service[8];
    uint8_t hello[19 + WL_INET_ROOM_MAX];
    size_t roomLen = room ? strlen(room) : 0;
    uint32_t start = NowMs();

    memset(net, 0, sizeof(*net));
    net->sock = -1;
    if (roomLen < 1 || roomLen > WL_INET_ROOM_MAX)
        return false;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    {
        unsigned p = port, i = 0, digits[6], n = 0;

        do
            digits[n++] = p % 10;
        while ((p /= 10) && n < 5);
        while (n)
            service[i++] = (char)('0' + digits[--n]);
        service[i] = 0;
    }
    if (getaddrinfo(host, service, &hints, &list) != 0)
        return false;
    for (a = list; a && net->sock < 0; a = a->ai_next)
    {
        intptr_t s = (intptr_t)socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        fd_set wr;
        struct timeval tv;
        uint32_t left = timeoutMs > NowMs() - start ? timeoutMs - (NowMs() - start) : 0;

        if (s < 0)
            continue;
        SetNonBlocking(s, true);
        if (connect(SK(s), a->ai_addr, (int)a->ai_addrlen) != 0)
        {
#ifdef _WIN32
            bool pending = WSAGetLastError() == WSAEWOULDBLOCK;
#else
            bool pending = errno == EINPROGRESS;
#endif
            int err = 0;
            socklen_t errLen = sizeof(err);

            if (!pending)
            {
                WL_CLOSE(s);
                continue;
            }
            FD_ZERO(&wr);
            FD_SET(SK(s), &wr);
            tv.tv_sec = (long)(left / 1000);
            tv.tv_usec = (long)(left % 1000) * 1000;
            if (select((int)s + 1, NULL, &wr, NULL, &tv) <= 0 || getsockopt(SK(s), SOL_SOCKET, SO_ERROR, (char *)&err, &errLen) != 0
                || err != 0)
            {
                WL_CLOSE(s);
                continue;
            }
        }
        net->sock = s;
    }
    freeaddrinfo(list);
    if (net->sock < 0)
        return false;
    hello[0] = VERSION;
    memcpy(hello + 1, build, WL_INET_BUILD_BYTES);
    hello[17] = (uint8_t)roomLen;
    memcpy(hello + 18, room, roomLen);
    Queue(net, T_HELLO, hello, (uint16_t)(18 + roomLen));
    Flush(net);
    while (net->sock >= 0 && !net->welcomed && !net->reject)
    {
        fd_set rd;
        struct timeval tv = {0, 20000};

        if (NowMs() - start >= timeoutMs)
        {
            WlInet_Close(net);
            return false;
        }
        FD_ZERO(&rd);
        FD_SET(SK(net->sock), &rd);
        select((int)net->sock + 1, &rd, NULL, NULL, &tv);
        WlInet_Pump(net);
    }
    if (net->reject || !net->welcomed)
    {
        WlInet_Close(net);
        return false;
    }
    return true;
}

bool WlInet_PopEvent(WlInet *net, int *kind, uint16_t *node)
{
    if (!net->eventCount)
        return false;
    *kind = net->events[net->eventHead].kind;
    *node = net->events[net->eventHead].node;
    net->eventHead = (uint8_t)((net->eventHead + 1) % (sizeof(net->events) / sizeof(net->events[0])));
    --net->eventCount;
    return true;
}

bool WlInet_Connected(const WlInet *net)
{
    return net->sock >= 0 && net->welcomed;
}

void WlInet_Close(WlInet *net)
{
    if (net->sock >= 0)
        WL_CLOSE(net->sock);
    net->sock = -1;
}

static bool BackendSend(void *user, uint16_t dest, const void *data, uint16_t len)
{
    WlInet *net = user;
    uint8_t body[WL_MAX_PAYLOAD + 32];

    if (net->sock < 0 || !net->welcomed || dest < 1 || dest > WL_MAX_NODES || len > sizeof(body) - 1)
        return false;
    body[0] = (uint8_t)dest;
    memcpy(body + 1, data, len);
    if (!Queue(net, T_DATA, body, (uint16_t)(len + 1)))
        return false;
    Flush(net);
    return true;
}

static bool BackendRecv(void *user, uint16_t *src, void *data, uint16_t max, uint16_t *len)
{
    WlInet *net = user;
    uint16_t n;

    WlInet_Pump(net);
    if (!net->inCount)
        return false;
    n = net->in[net->inHead].len;
    if (n > max)
        n = max;
    *src = net->in[net->inHead].src;
    memcpy(data, net->in[net->inHead].data, n);
    *len = n;
    net->inHead = (uint8_t)((net->inHead + 1) % (sizeof(net->in) / sizeof(net->in[0])));
    --net->inCount;
    return true;
}

static uint32_t BackendNow(void *user)
{
    (void)user;
    return NowMs();
}

WlBackend WlInet_Backend(WlInet *net)
{
    WlBackend io = {net, BackendSend, BackendRecv, BackendNow};

    return io;
}
