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
#elif defined(__DEVKITARM__)
#include <3ds/os.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
typedef ssize_t WlLen;
/* The console's socket service can answer a send right after a connect with "in progress" or "not connected yet": not an error. */
#define WL_WOULD_BLOCK (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS || errno == ENOTCONN)
#define SK(s) ((int)(s))
#define WL_CLOSE(s) close((int)(s))
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
/* The console's socket service can answer a send right after a connect with "in progress" or "not connected yet": not an error. */
#define WL_WOULD_BLOCK (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS || errno == ENOTCONN)
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
#elif defined(__DEVKITARM__)
    return (uint32_t)osGetTime();
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

static int LastError(void)
{
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

/* A connect that is still under way (each system says it its own way). */
static bool ConnectPending(void)
{
#ifdef _WIN32
    int e = WSAGetLastError();

    return e == WSAEWOULDBLOCK || e == WSAEINPROGRESS || e == WSAEALREADY;
#else
    return errno == EINPROGRESS || errno == EAGAIN || errno == EWOULDBLOCK || errno == EALREADY;
#endif
}

/* What a connection did, step by step (code, value), for a log: the console's socket service is not something a computer can show. */
static void Trail(WlInet *net, int code, int value)
{
    if (net->trailLen < (int)(sizeof(net->trail) / sizeof(net->trail[0])))
    {
        net->trail[net->trailLen][0] = code;
        net->trail[net->trailLen][1] = value;
        ++net->trailLen;
    }
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

static void HandleMessage(WlInet *net, const uint8_t *m, uint16_t len)
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
        if (len >= 2 + WL_INET_SEAL_BYTES && net->inCount < sizeof(net->in) / sizeof(net->in[0]))
        {
            unsigned at = (net->inHead + net->inCount) % (sizeof(net->in) / sizeof(net->in[0]));
            uint16_t n = (uint16_t)(len - 2 - WL_INET_SEAL_BYTES);
            uint8_t aad[2] = {m[1], (uint8_t)net->node};   /* the sender the relay names and this node: a frame cannot be moved */

            if (n > sizeof(net->in[at].data) || !WlAeadOpen(net->key, m + 2, aad, 2, m + 14, n, m + 14 + n, net->in[at].data))
            {
                ++net->dropped;
                break;
            }
            net->in[at].src = m[1];
            net->in[at].len = n;
            ++net->inCount;
            ++net->receivedFrames;
        }
        else if (len >= 2 + WL_INET_SEAL_BYTES)
            ++net->lostFrames;                  /* nobody was taking them: the queue of opened frames is full */
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
        HandleMessage(net, net->rx + at + 2, length);
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

static uint8_t sPool[32];
static uint32_t sConnections;

void WlInet_Seed(const void *entropy, size_t len)
{
    uint8_t mix[64];

    memcpy(mix, sPool, 32);
    WlSha256(entropy, len, mix + 32);
    WlSha256(mix, sizeof(mix), sPool);
}

void WlInet_Derive(const char *code, const uint8_t build[WL_INET_BUILD_BYTES], char room[WL_INET_ROOM_MAX + 1], uint8_t key[32])
{
    static const char kSalt[] = "TwinEmbers relay v1";
    static const char kRoom[] = "TwinEmbers room";
    uint8_t salt[sizeof(kSalt) - 1 + WL_INET_BUILD_BYTES], input[sizeof(kRoom) - 1 + 32], digest[32];

    memcpy(salt, kSalt, sizeof(kSalt) - 1);
    memcpy(salt + sizeof(kSalt) - 1, build, WL_INET_BUILD_BYTES);
    WlPbkdf2Sha256(code, strlen(code), salt, sizeof(salt), WL_INET_ITERATIONS, key, 32);
    memcpy(input, kRoom, sizeof(kRoom) - 1);
    memcpy(input + sizeof(kRoom) - 1, key, 32);
    WlSha256(input, sizeof(input), digest);
    for (int i = 0; i < 8; ++i)
    {
        room[i * 2] = "0123456789abcdef"[digest[i] >> 4];
        room[i * 2 + 1] = "0123456789abcdef"[digest[i] & 15];
    }
    room[16] = 0;
}

bool WlInet_Connect(WlInet *net, const char *host, uint16_t port, const char *code, const uint8_t build[WL_INET_BUILD_BYTES],
                    uint32_t timeoutMs)
{
    char room[WL_INET_ROOM_MAX + 1];
    uint8_t key[32];
    struct addrinfo hints, *list = NULL, *a;
    char service[8];
    uint8_t hello[19 + WL_INET_ROOM_MAX];
    size_t roomLen = WL_INET_ROOM_MAX, codeLen = code ? strlen(code) : 0;
    uint32_t start;

    memset(net, 0, sizeof(*net));
    net->sock = -1;
    if (codeLen < 1 || codeLen > WL_INET_CODE_MAX)
        return false;
    WlInet_Derive(code, build, room, key);
    Trail(net, 10, 0);
    start = NowMs();               /* after the key: the time it takes is not the connection's */
    {
        uint8_t mix[32 + 4 + 4 + sizeof(void *)], digest[32];
        uint32_t now = NowMs();
        void *where = net;

        memcpy(mix, sPool, 32);
        memcpy(mix + 32, &now, 4);
        ++sConnections;
        memcpy(mix + 36, &sConnections, 4);
        memcpy(mix + 40, &where, sizeof(where));
        WlSha256(mix, sizeof(mix), digest);
        memcpy(net->session, digest, sizeof(net->session));
    }
    memcpy(net->key, key, 32);
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
    {
        int rc = getaddrinfo(host, service, &hints, &list);

        Trail(net, 11, rc);
        if (rc != 0)
        {
            net->failStep = 1;
            return false;
        }
    }
    for (a = list; a && net->sock < 0; a = a->ai_next)
    {
        intptr_t s = (intptr_t)socket(a->ai_family, a->ai_socktype, a->ai_protocol);

        Trail(net, 12, (int)s);
        if (s < 0)
            continue;
        SetNonBlocking(s, true);
        if (connect(SK(s), a->ai_addr, (int)a->ai_addrlen) != 0)
        {
            Trail(net, 13, LastError());
            if (!ConnectPending())
            {
                net->failStep = 2;
                net->failErrno = LastError();
                WL_CLOSE(s);
                continue;
            }
#ifdef _WIN32
            {
                fd_set wr;
                struct timeval tv;
                int err = 0, ready, rc;
                int errLen = sizeof(err);
                uint32_t left = timeoutMs > NowMs() - start ? timeoutMs - (NowMs() - start) : 0;

                FD_ZERO(&wr);
                FD_SET(SK(s), &wr);
                tv.tv_sec = (long)(left / 1000);
                tv.tv_usec = (long)(left % 1000) * 1000;
                ready = select((int)s + 1, NULL, &wr, NULL, &tv);
                Trail(net, 14, ready);
                rc = ready > 0 ? getsockopt(SK(s), SOL_SOCKET, SO_ERROR, (char *)&err, &errLen) : -1;
                Trail(net, 15, err);
                if (ready <= 0 || rc != 0 || err != 0)
                {
                    net->failStep = ready <= 0 ? 3 : 4;
                    net->failErrno = ready <= 0 ? LastError() : err;
                    WL_CLOSE(s);
                    continue;
                }
            }
#else
            /* The console's socket service answers a connect that is under way with EAGAIN (not EINPROGRESS) and may not report the
             * outcome through select and SO_ERROR: ask again until the answer is "connected" (0 or EISCONN) or a real error. */
            {
                unsigned tries = 0;
                bool up = false;

                while (NowMs() - start < timeoutMs)
                {
                    fd_set wr;
                    struct timeval tv = {0, 20000};

                    FD_ZERO(&wr);
                    FD_SET(SK(s), &wr);
                    select((int)s + 1, NULL, &wr, NULL, &tv);
                    ++tries;
                    if (connect(SK(s), a->ai_addr, (int)a->ai_addrlen) == 0 || errno == EISCONN)
                    {
                        up = true;
                        break;
                    }
                    if (!ConnectPending())
                        break;
                }
                Trail(net, 14, (int)tries);
                Trail(net, 15, up ? 0 : errno);
                if (!up)
                {
                    net->failStep = NowMs() - start < timeoutMs ? 4 : 3;
                    net->failErrno = errno;
                    WL_CLOSE(s);
                    continue;
                }
            }
#endif
        }
        net->sock = s;
    }
    freeaddrinfo(list);
    if (net->sock < 0)
        return false;                   /* failStep says where */
    hello[0] = VERSION;
    memcpy(hello + 1, build, WL_INET_BUILD_BYTES);
    hello[17] = (uint8_t)roomLen;
    memcpy(hello + 18, room, roomLen);
    Queue(net, T_HELLO, hello, (uint16_t)(18 + roomLen));
    Flush(net);
    Trail(net, 16, net->txLen * 1000 + (net->sock < 0 ? 1 : 0));
    while (net->sock >= 0 && !net->welcomed && !net->reject)
    {
        fd_set rd;
        struct timeval tv = {0, 20000};

        if (NowMs() - start >= timeoutMs)
        {
            net->failStep = 5;
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
        net->failStep = net->reject ? 6 : 7;     /* 7: the connection ended before the welcome */
        net->failErrno = net->reject;
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
        Trail(net, 18, LastError());
    if (net->sock >= 0)
        WL_CLOSE(net->sock);
    net->sock = -1;
    memset(net->key, 0, sizeof(net->key));      /* the key is not kept once the connection is over */
}

static bool BackendSend(void *user, uint16_t dest, const void *data, uint16_t len)
{
    WlInet *net = user;
    uint8_t body[1 + WL_INET_SEAL_BYTES + WL_MAX_PAYLOAD + 16], aad[2];

    if (net->sock < 0 || !net->welcomed || dest < 1 || dest > WL_MAX_NODES || len > WL_MAX_PAYLOAD + 16 || net->counter == 0xffffffffu)
        return false;
    body[0] = (uint8_t)dest;
    memcpy(body + 1, net->session, 8);
    for (int i = 0; i < 4; ++i)
        body[9 + i] = (uint8_t)(net->counter >> (8 * i));
    aad[0] = (uint8_t)net->node;
    aad[1] = (uint8_t)dest;
    WlAeadSeal(net->key, body + 1, aad, 2, data, len, body + 13, body + 13 + len);
    if (!Queue(net, T_DATA, body, (uint16_t)(1 + WL_INET_SEAL_BYTES + len)))
        return false;
    ++net->counter;
    ++net->sentFrames;
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
