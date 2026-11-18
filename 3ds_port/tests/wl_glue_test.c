/* 3ds_wl_glue.c on a computer, over the UDS stand-in (tests/uds_mock.h): when the radio is started and when a session begins and
 * ends, hosting or joining by what a scan finds, two consoles that began together merging, a lost link, and the adapter answering the
 * game's commands with or without a radio. The file is included so the test can set the clock of the next rescan. */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "uds_mock.h"
#include "../src/3ds_wl_glue.c"

static void Fresh(void)
{
    CtrWl_Shutdown();
    memset(&M, 0, sizeof(M));
    CtrWl_Setup(kBuild, 12345);
}

static uint32_t Cmd(uint32_t command, const uint32_t *params, int count, uint32_t *ack)
{
    uint32_t req[WL_RFU_ACK_WORDS + 1] = {0x99660000u | (uint32_t)count << 8 | command};

    for (int i = 0; i < count; ++i)
        req[1 + i] = params[i];
    return (uint32_t)CtrRfu_Command(req, ack);
}

static void SetHost(int i, uint8_t nodes, uint8_t maxNodes, uint32_t token, const uint8_t *build)
{
    memset(&M.scan[i], 0, sizeof(M.scan[i]));
    M.scan[i].network.total_nodes = nodes;
    M.scan[i].network.max_nodes = maxNodes;
    M.scanApp[i][0] = 'T';
    M.scanApp[i][1] = 'E';
    memcpy(M.scanApp[i] + 2, build, 16);
    for (int k = 0; k < 4; ++k)
        M.scanApp[i][18 + k] = (uint8_t)(token >> (8 * k));
    M.scanAppLen[i] = 22;
    if (M.scanN <= i)
        M.scanN = i + 1;
}

int main(void)
{
    uint32_t ack[WL_RFU_ACK_WORDS + 2];

    /* before it is set up, the adapter rejects whatever it is asked */
    {
        uint32_t req = 0x99660011u;

        assert(CtrRfu_Command(&req, ack) == 2 && ack[0] == 0x996601EEu && !CtrWl_Available() && CtrRfu_PollSlave(&req) == 0);
        CtrWl_Tick();
    }

    /* no radio (the wireless switch is off): the adapter still answers, and no session starts */
    memset(&M, 0, sizeof(M));
    M.initResult = -1;
    CtrWl_Setup(kBuild, 777);
    assert(!CtrWl_Available());
    assert(Cmd(WLC_LINK_STATUS, NULL, 0, ack) >= 1 && (ack[0] >> 16) == 0x9966 && (ack[0] & 0xffu) == (0x80u | WLC_LINK_STATUS));
    assert(Cmd(WLC_SP_START, NULL, 0, ack) >= 1 && CtrWl_State() == CTR_WL_OFF && M.created == 0);
    CtrWl_Tick();
    assert(CtrWl_State() == CTR_WL_OFF);

    /* nobody around: the search finds nothing and this console hosts, with a token in its beacon */
    Fresh();
    assert(CtrWl_Available() && M.initCalls == 1);
    assert(Cmd(WLC_SP_START, NULL, 0, ack) >= 1 && CtrWl_State() == CTR_WL_SEARCH);
    CtrWl_Tick();
    assert(CtrWl_State() == CTR_WL_HOST && M.created == 1 && M.maxNodes == CTR_WL_NODES_MAX && M.appdataLen == 22);
    assert(CtrWl_Others() == 0);
    assert(Cmd(WLC_SP_START, NULL, 0, ack) >= 1 && M.created == 1);                 /* asking again does not make another network */

    /* somebody joins: the adapter knows the member (it will list it when the game polls) */
    SetStatus(1, 1 | 2);
    M.statusEvent = true;
    CtrWl_Tick();
    assert(CtrWl_Others() == 1 && sRfu.members == (1u << 2));

    /* the game stops the adapter: the network is closed */
    assert(Cmd(WLC_STOP_MODE, NULL, 0, ack) >= 1);
    assert(CtrWl_State() == CTR_WL_OFF && M.unbound == 1 && M.destroyed == 1 && sRfu.members == 0);
    assert(Cmd(WLC_SC_START, NULL, 0, ack) >= 1 && CtrWl_State() == CTR_WL_SEARCH);   /* and a later search starts a new session */
    Cmd(WLC_STOP_MODE, NULL, 0, ack);

    /* a host of this build is there: join it (the one with company first, and not another build's, and not a full one) */
    Fresh();
    assert(CtrWl_Available());
    SetHost(0, 1, 5, 900, kOther);                                                   /* another build: the scan will not list it */
    M.scanN = 0;
    SetHost(0, 1, 5, 900, kBuild);
    SetHost(1, 3, 5, 800, kBuild);                                                   /* more company: preferred */
    SetHost(2, 5, 5, 100, kBuild);                                                   /* full: never */
    Cmd(WLC_SC_START, NULL, 0, ack);
    CtrWl_Tick();
    assert(CtrWl_State() == CTR_WL_JOINED && M.connected == 1 && M.created == 0);
    assert(CtrWl_Others() == 1 && sNet.self == 2);                                   /* the host is node 1, this console node 2 */

    /* the host goes away: the link is lost, the console looks again (and hosts when there is nobody) */
    SetStatus(0, 0);
    M.statusEvent = true;
    M.scanN = 0;
    CtrWl_Tick();
    assert(CtrWl_State() == CTR_WL_SEARCH && M.disconnected == 1 && sRfu.members == 0);
    CtrWl_Tick();
    assert(CtrWl_State() == CTR_WL_HOST && M.created == 1);

    /* hosting alone, another host with a smaller token appears: merge (join it), after the rescan interval and not before */
    {
        uint32_t mine = sToken;

        M.scanN = 0;
        SetHost(0, 1, 5, mine - 1, kBuild);
        sScanAt = NowMs();
        CtrWl_Tick();
        assert(CtrWl_State() == CTR_WL_HOST && sJobState == 0);                     /* too soon to look again */
        sScanAt = NowMs() - CTR_WL_RESCAN_MS - 10;
        CtrWl_Tick();                                                                /* the scan runs */
        assert(CtrWl_State() == CTR_WL_HOST && sJobState == 2);
        CtrWl_Tick();                                                                /* and its result is taken */
        assert(CtrWl_State() == CTR_WL_JOINED && M.connected == 2 && M.destroyed == 1);   /* the second join of this scene */
    }

    /* hosting alone, another host alone with a larger token: it is the one that will give way; this console stays */
    Fresh();
    Cmd(WLC_SP_START, NULL, 0, ack);
    CtrWl_Tick();
    assert(CtrWl_State() == CTR_WL_HOST);
    M.scanN = 0;
    SetHost(0, 1, 5, sToken + 1, kBuild);
    sScanAt = NowMs() - CTR_WL_RESCAN_MS - 10;
    CtrWl_Tick();
    CtrWl_Tick();
    assert(CtrWl_State() == CTR_WL_HOST && M.connected == 0);

    /* hosting with company: no looking about */
    SetStatus(1, 1 | 2);
    M.statusEvent = true;
    CtrWl_Tick();
    SetHost(0, 1, 5, 0, kBuild);
    sScanAt = NowMs() - CTR_WL_RESCAN_MS - 10;
    CtrWl_Tick();
    assert(CtrWl_State() == CTR_WL_HOST && sJobState == 0 && M.connected == 0);

    /* the game can reset the adapter at any time, and the answer to a reset is the usual one */
    CtrRfu_Reset();
    assert(Cmd(WLC_RESET, NULL, 0, ack) >= 1 && (ack[0] & 0xffu) == (0x80u | WLC_RESET));

    CtrWl_Shutdown();
    assert(M.exitCalls >= 1);
    puts("wl_glue_test ok");
    return 0;
}
