/* The internet radio (3ds_wl_inet.c) against the reference relay: tools/test_wl_inet.py starts tools/wl_relay.py on a free port
 * and runs this program with that port. Rooms, node numbers, the build id check, joins and leaves, a full room, and WlNet's
 * reliable stream over two real connections. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <unistd.h>
#endif

#include "3ds_wl_inet.h"

static const uint8_t kBuildA[WL_INET_BUILD_BYTES] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
static const uint8_t kBuildB[WL_INET_BUILD_BYTES] = {9, 9, 9};

static void Sleep10(void)
{
#ifdef _WIN32
    Sleep(10);
#else
    usleep(10000);
#endif
}

static void Pump(WlInet *a, WlInet *b, int rounds)
{
    for (int i = 0; i < rounds; ++i)
    {
        if (a)
            WlInet_Pump(a);
        if (b)
            WlInet_Pump(b);
        Sleep10();
    }
}

static int Expect(WlInet *net, int kind, uint16_t node)
{
    for (int tries = 0; tries < 200; ++tries)
    {
        int k;
        uint16_t n;

        WlInet_Pump(net);
        while (WlInet_PopEvent(net, &k, &n))
            if (k == kind && n == node)
                return 1;
        Sleep10();
    }
    return 0;
}

/* "listen <port> <code>": for tools/test_wl_inet.py, which plays the other console in Python. Joins, prints its node, waits for one
 * frame, prints it, answers it, then listens a while longer and prints how many frames did not open. */
static int Listen(uint16_t port, const char *code)
{
    static WlInet net;
    WlBackend io;
    uint16_t src = 0, len = 0;
    uint8_t data[256];
    unsigned waited = 0;

#ifdef _WIN32
    WSADATA wsa;

    assert(WSAStartup(MAKEWORD(2, 2), &wsa) == 0);
#endif
    assert(WlInet_Connect(&net, "127.0.0.1", port, code, kBuildA, 5000));
    printf("READY %u%c", net.node, 10);
    fflush(stdout);
    io = WlInet_Backend(&net);
    while (!io.recv(io.user, &src, data, sizeof(data), &len) && waited++ < 800)
        Sleep10();
    assert(len > 0);
    printf("GOT %u ", src);
    for (unsigned i = 0; i < len; ++i)
        printf("%02x", data[i]);
    printf("%c", 10);
    assert(io.send(io.user, src, "reply from c", 12));
    for (unsigned i = 0; i < 150; ++i)
    {
        uint16_t s2, l2;

        WlInet_Pump(&net);
        while (io.recv(io.user, &s2, data, sizeof(data), &l2))
            printf("MORE %u%c", s2, 10);
        Sleep10();
    }
    printf("DROPPED %u%c", (unsigned)net.dropped, 10);
    fflush(stdout);
    WlInet_Close(&net);
    return 0;
}

int main(int argc, char **argv)
{
    uint16_t port;
    static WlInet a, b, c, extra[14];
    WlNet na, nb;
    WlBackend ioA, ioB;
    int gotA = 0, gotB = 0;

    assert(argc >= 2);
    if (argc >= 4 && !strcmp(argv[1], "listen"))
        return Listen((uint16_t)atoi(argv[2]), argv[3]);
    port = (uint16_t)atoi(argv[1]);
#ifdef _WIN32
    {
        WSADATA wsa;

        assert(WSAStartup(MAKEWORD(2, 2), &wsa) == 0);
    }
#endif

    /* the room name is a hash of the key: it differs with the code and with the build, is 16 hex characters, and is not the code */
    {
        char r1[WL_INET_ROOM_MAX + 1], r2[WL_INET_ROOM_MAX + 1], r3[WL_INET_ROOM_MAX + 1], r4[WL_INET_ROOM_MAX + 1];
        uint8_t k1[32], k2[32], k3[32], k4[32];

        WlInet_Derive("TESTROOM1", kBuildA, r1, k1);
        WlInet_Derive("TESTROOM2", kBuildA, r2, k2);
        WlInet_Derive("TESTROOM1", kBuildB, r3, k3);
        WlInet_Derive("TESTROOM1", kBuildA, r4, k4);
        assert(strlen(r1) == 16 && strcmp(r1, r2) && strcmp(r1, r3) && !strcmp(r1, r4));
        assert(memcmp(k1, k2, 32) && memcmp(k1, k3, 32) && !memcmp(k1, k4, 32));
        for (int i = 0; i < 16; ++i)
            assert((r1[i] >= '0' && r1[i] <= '9') || (r1[i] >= 'a' && r1[i] <= 'f'));
    }

    /* the first console makes the room and is node 1; the second is node 2 and finds the first */
    assert(WlInet_Connect(&a, "127.0.0.1", port, "TESTROOM1", kBuildA, 3000));
    assert(a.node == 1 && a.members == 0 && WlInet_Connected(&a));
    assert(WlInet_Connect(&b, "127.0.0.1", port, "TESTROOM1", kBuildA, 3000));
    assert(b.node == 2 && b.members == (1u << 1));
    assert(Expect(&a, WL_INET_EVENT_JOINED, 2) && a.members == (1u << 2));

    /* a room of another name is another room: node 1 again */
    {
        WlInet other;

        assert(WlInet_Connect(&other, "127.0.0.1", port, "OTHER", kBuildB, 3000));
        assert(other.node == 1 && other.members == 0);
        WlInet_Close(&other);
    }

    /* the build id is part of the key and of the room's name: the same code with another build is another room, which cannot be
     * entered by mistake (the relay's own build check is tried on the wire by test_wl_inet.py) */
    assert(WlInet_Connect(&c, "127.0.0.1", port, "TESTROOM1", kBuildB, 3000) && c.node == 1 && c.members == 0);
    WlInet_Close(&c);
    assert(!WlInet_Connect(&c, "127.0.0.1", port, "", kBuildA, 3000));
    assert(!WlInet_Connect(&c, "127.0.0.1", port, "0123456789012345678901234567890123456789012345678901234567890123456789", kBuildA, 3000));

    /* WlNet over the two connections: 40 messages each way, in order, whole */
    ioA = WlInet_Backend(&a);
    ioB = WlInet_Backend(&b);
    WlNet_Init(&na, &ioA, 1);
    WlNet_Init(&nb, &ioB, 2);
    for (int i = 0; i < 40; ++i)
    {
        uint8_t m[24];

        memset(m, i, sizeof(m));
        while (!WlNet_Send(&na, 2, m, sizeof(m)))
        {
            WlNet_Step(&na);
            WlNet_Step(&nb);
        }
        m[0] = (uint8_t)(100 + i);
        while (!WlNet_Send(&nb, 1, m, 12))
        {
            WlNet_Step(&na);
            WlNet_Step(&nb);
        }
    }
    for (int rounds = 0; rounds < 600 && (gotA < 40 || gotB < 40); ++rounds)
    {
        uint16_t src;
        WlMessage msg;

        WlNet_Step(&na);
        WlNet_Step(&nb);
        while (WlNet_Pop(&nb, &src, &msg))
        {
            assert(src == 1 && msg.len == 24 && msg.data[0] == gotB && msg.data[23] == gotB);
            ++gotB;
        }
        while (WlNet_Pop(&na, &src, &msg))
        {
            assert(src == 2 && msg.len == 12 && msg.data[0] == 100 + gotA);
            ++gotA;
        }
        Sleep10();
    }
    assert(gotA == 40 && gotB == 40);

    /* a frame to a node that is not there is dropped without harm */
    assert(ioA.send(ioA.user, 9, "x", 1));
    Pump(&a, &b, 5);
    assert(WlInet_Connected(&a) && WlInet_Connected(&b));

    /* a third console joins and leaves; the others are told */
    assert(WlInet_Connect(&c, "127.0.0.1", port, "TESTROOM1", kBuildA, 3000));
    assert(c.node == 3 && c.members == ((1u << 1) | (1u << 2)));
    assert(Expect(&a, WL_INET_EVENT_JOINED, 3) && Expect(&b, WL_INET_EVENT_JOINED, 3));
    WlInet_Close(&c);
    assert(Expect(&a, WL_INET_EVENT_LEFT, 3) && Expect(&b, WL_INET_EVENT_LEFT, 3));
    assert(!(a.members & (1u << 3)));

    /* sixteen nodes fill the room: the seventeenth is refused, and a freed number is given again */
    for (int i = 0; i < 14; ++i)
    {
        assert(WlInet_Connect(&extra[i], "127.0.0.1", port, "TESTROOM1", kBuildA, 3000));
        assert(extra[i].node == 3 + i);
    }
    assert(!WlInet_Connect(&c, "127.0.0.1", port, "TESTROOM1", kBuildA, 3000) && c.reject == WL_INET_REJECT_FULL);
    WlInet_Close(&extra[5]);
    assert(Expect(&a, WL_INET_EVENT_LEFT, 8));
    assert(WlInet_Connect(&c, "127.0.0.1", port, "TESTROOM1", kBuildA, 3000) && c.node == 8);

    WlInet_Close(&c);
    for (int i = 0; i < 14; ++i)
        WlInet_Close(&extra[i]);
    WlInet_Close(&a);
    WlInet_Close(&b);
    puts("wl_inet_test ok");
    return 0;
}
