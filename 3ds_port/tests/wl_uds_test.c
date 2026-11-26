/* 3ds_wl_uds.c on a computer: libctru's own uds.h (so the types and the signatures are the real ones) with a small stand-in for
 * the functions, which records what the backend asks of UDS and plays back beacons, connection changes and frames. What it
 * cannot show is the radio: that needs two consoles (or two Azahar instances). */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "3ds_wl_uds.h"

#include "uds_mock.h"

/* ---- the tests ---- */
static void Queue(u16 src, const void *data, size_t len)
{
    M.queue[M.queued].src = src;
    memcpy(M.queue[M.queued].data, data, len);
    M.queue[M.queued].len = len;
    ++M.queued;
}

static void Scene(WlUds *u, const uint8_t *build, int member2, int member3)
{
    (void)member2; (void)member3;
    memset(&M, 0, sizeof(M));
    assert(WlUds_Init(u, build, "Ale") && u->ready);
}

static int Expect(WlUds *u, int kind, uint16_t node)
{
    int k;
    uint16_t n;

    if (!WlUds_PopEvent(u, &k, &n))
        return 0;
    return k == kind && n == node;
}

int main(void)
{
    static WlUds u;
    WlBackend io;
    WlUdsHost hosts[WL_UDS_SCAN_HOSTS];
    uint8_t data[256];
    uint16_t src, len;

    /* a console whose wireless is off: Init fails and nothing else works */
    memset(&M, 0, sizeof(M));
    M.initResult = -1;
    assert(!WlUds_Init(&u, kBuild, NULL) && !u.ready);
    assert(!WlUds_Host(&u, 4, 1) && WlUds_Scan(&u, hosts, WL_UDS_SCAN_HOSTS) == 0 && !WlUds_Join(&u, &hosts[0]));

    /* hosting: a network of 5 (the host and 4), this build's passphrase and beacon, node 1 */
    Scene(&u, kBuild, 0, 0);
    assert(!WlUds_Host(&u, 1, 7) && !WlUds_Host(&u, 17, 7));
    assert(WlUds_Host(&u, 5, 0x04030201u) && u.active && u.hosting && u.node == 1 && u.members == 0);
    assert(M.created == 1 && M.maxNodes == 5);
    assert(M.passLen == 11 + 32 + 1 && !memcmp(M.pass, "twinembers-0102030405060708090a0b0c0d0e0f10", 43) && M.pass[43] == 0);
    assert(M.appdataLen == 22 && M.appdata[0] == 'T' && M.appdata[1] == 'E' && !memcmp(M.appdata + 2, kBuild, 16));
    assert(M.appdata[18] == 1 && M.appdata[19] == 2 && M.appdata[20] == 3 && M.appdata[21] == 4);   /* the token, little-endian */
    assert(!WlUds_Host(&u, 5, 1));                                             /* already in a network */

    /* two consoles join, one leaves: events in node order, members follow */
    SetStatus(1, 1 | 2 | 4);
    M.statusEvent = true;
    WlUds_Pump(&u);
    assert(Expect(&u, WL_UDS_EVENT_JOINED, 2) && Expect(&u, WL_UDS_EVENT_JOINED, 3) && !Expect(&u, 0, 0));
    assert(u.members == ((1u << 2) | (1u << 3)));
    SetStatus(1, 1 | 4);
    WlUds_Pump(&u);                                                         /* no event signalled: nothing is read */
    assert(u.members == ((1u << 2) | (1u << 3)));
    M.statusEvent = true;
    WlUds_Pump(&u);
    assert(Expect(&u, WL_UDS_EVENT_LEFT, 2) && u.members == (1u << 3));

    /* frames: in order, with their senders; a frame too long for the link layer or from no node is dropped */
    io = WlUds_Backend(&u);
    Queue(3, "first", 5);
    Queue(3, data, 400);                                                    /* whatever is in `data`: too long */
    Queue(0, "from nobody", 11);
    Queue(17, "from beyond", 11);
    Queue(3, "second", 6);
    assert(io.recv(io.user, &src, data, sizeof(data), &len) && src == 3 && len == 5 && !memcmp(data, "first", 5));
    assert(io.recv(io.user, &src, data, sizeof(data), &len) && src == 3 && len == 6 && !memcmp(data, "second", 6));
    assert(!io.recv(io.user, &src, data, sizeof(data), &len));

    /* sending: to a node UDS can reach; the error UDS allows is a lost frame, any other is a refusal */
    assert(io.send(io.user, 3, "hello", 5) && M.lastDest == 3 && M.lastLen == 5 && !memcmp(M.lastData, "hello", 5));
    M.sendResult = (Result)0xC86113F0u;
    assert(io.send(io.user, 3, "x", 1));
    M.sendResult = -5;
    assert(!io.send(io.user, 3, "x", 1));
    M.sendResult = 0;
    assert(!io.send(io.user, 0, "x", 1) && !io.send(io.user, 17, "x", 1) && !io.send(io.user, 3, "x", 0));
    (void)io.now(io.user);

    /* leaving a network the console made closes it */
    WlUds_Leave(&u);
    assert(M.unbound == 1 && M.destroyed == 1 && M.disconnected == 0 && !u.active && u.node == 0);
    assert(!io.send(io.user, 3, "x", 1));

    /* searching: only this build's beacons count */
    Scene(&u, kBuild, 0, 0);
    M.scanN = 3;
    for (int i = 0; i < 3; ++i)
    {
        memset(&M.scan[i], 0, sizeof(M.scan[i]));
        M.scan[i].network.total_nodes = (u8)(i + 1);
        M.scan[i].network.max_nodes = 5;
    }
    M.scanApp[0][0] = 'T'; M.scanApp[0][1] = 'E'; memcpy(M.scanApp[0] + 2, kBuild, 16); M.scanApp[0][18] = 0x78; M.scanApp[0][19] = 0x56; M.scanApp[0][20] = 0x34; M.scanApp[0][21] = 0x12; M.scanAppLen[0] = 22;
    M.scanApp[1][0] = 'T'; M.scanApp[1][1] = 'E'; memcpy(M.scanApp[1] + 2, kOther, 16); M.scanAppLen[1] = 22;     /* another build */
    M.scanApp[2][0] = 'X'; M.scanApp[2][1] = 'E'; memcpy(M.scanApp[2] + 2, kBuild, 16); M.scanAppLen[2] = 22;     /* another application */
    assert(WlUds_Scan(&u, hosts, WL_UDS_SCAN_HOSTS) == 1);
    assert(hosts[0].nodes == 1 && hosts[0].maxNodes == 5 && !strcmp(hosts[0].name, "Ale") && hosts[0].token == 0x12345678u);
    assert(WlUds_Scan(&u, hosts, 0) == 0);

    /* joining: the host is node 1 and becomes a member; the passphrase is the same */
    assert(WlUds_Join(&u, &hosts[0]) && u.active && !u.hosting && u.node == 2 && u.members == (1u << 1));
    assert(M.connected == 1 && M.connectRecv == UDS_BROADCAST_NETWORKNODEID && !memcmp(M.pass, "twinembers-0102", 15));
    assert(Expect(&u, WL_UDS_EVENT_JOINED, 1));
    assert(!WlUds_Join(&u, &hosts[0]));

    /* the host goes away: UDS reports no node for this console; every member is told lost, once */
    SetStatus(0, 0);
    M.statusEvent = true;
    WlUds_Pump(&u);
    assert(!u.active && u.members == 0 && Expect(&u, WL_UDS_EVENT_LEFT, 1) && !Expect(&u, 0, 0));
    WlUds_Pump(&u);
    assert(!Expect(&u, 0, 0));
    WlUds_Leave(&u);                                                        /* a console that was joined disconnects, it does not destroy */
    WlUds_Exit(&u);
    assert(M.exitCalls == 1 && !u.ready);

    puts("wl_uds_test ok");
    return 0;
}
