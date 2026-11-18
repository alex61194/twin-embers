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

int main(int argc, char **argv)
{
    uint16_t port;
    static WlInet a, b, c, extra[14];
    WlNet na, nb;
    WlBackend ioA, ioB;
    int gotA = 0, gotB = 0;

    assert(argc >= 2);
    port = (uint16_t)atoi(argv[1]);
#ifdef _WIN32
    {
        WSADATA wsa;

        assert(WSAStartup(MAKEWORD(2, 2), &wsa) == 0);
    }
#endif

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

    /* the build id of the room decides */
    assert(!WlInet_Connect(&c, "127.0.0.1", port, "TESTROOM1", kBuildB, 3000) && c.reject == WL_INET_REJECT_BUILD);
    assert(!WlInet_Connect(&c, "127.0.0.1", port, "", kBuildA, 3000));
    assert(!WlInet_Connect(&c, "127.0.0.1", port, "A-ROOM-NAME-THAT-IS-TOO-LONG", kBuildA, 3000));
    assert(!WlInet_Connect(&c, "127.0.0.1", port, "BAD ROOM", kBuildA, 3000) && c.reject == WL_INET_REJECT_HELLO);

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
