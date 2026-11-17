/* Host test of the local wireless layers (src/3ds_wl_net.c, src/3ds_wl_rfu.c) over a simulated radio that loses frames. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "3ds_wl.h"

#define QUEUE 512

typedef struct {
    uint16_t src;
    uint16_t len;
    uint32_t at;
    uint8_t data[1500];
    bool used;
} Frame;

typedef struct {
    uint32_t now;
    unsigned lossPercent;
    unsigned delayMs;
    uint32_t seed;
    Frame inbox[WL_MAX_NODES + 1][QUEUE];
} Air;

typedef struct {
    Air *air;
    uint16_t node;
} Radio;

static unsigned Rand(Air *air)
{
    air->seed = air->seed * 1664525u + 1013904223u;
    return air->seed >> 16;
}

static bool RadioSend(void *user, uint16_t dest, const void *data, uint16_t len)
{
    Radio *r = user;
    Air *air = r->air;

    if (Rand(air) % 100 < air->lossPercent)
        return true;                            /* lost on the way: the sender cannot tell */
    for (unsigned i = 0; i < QUEUE; ++i)
        if (!air->inbox[dest][i].used)
        {
            Frame *f = &air->inbox[dest][i];

            f->used = true;
            f->src = r->node;
            f->len = len;
            f->at = air->now + air->delayMs;
            memcpy(f->data, data, len);
            return true;
        }
    return false;
}

static bool RadioRecv(void *user, uint16_t *src, void *data, uint16_t max, uint16_t *len)
{
    Radio *r = user;
    Air *air = r->air;
    Frame *best = NULL;

    for (unsigned i = 0; i < QUEUE; ++i)
    {
        Frame *f = &air->inbox[r->node][i];

        if (f->used && f->at <= air->now && (!best || f->at < best->at))
            best = f;
    }
    if (!best || best->len > max)
        return false;
    *src = best->src;
    *len = best->len;
    memcpy(data, best->data, best->len);
    best->used = false;
    return true;
}

static uint32_t RadioNow(void *user)
{
    return ((Radio *)user)->air->now;
}

typedef struct {
    Radio radio;
    WlNet net;
    WlRfu rfu;
} Node;

static void NodeInit(Node *n, Air *air, uint16_t node, uint16_t id)
{
    WlBackend io = {&n->radio, RadioSend, RadioRecv, RadioNow};

    n->radio.air = air;
    n->radio.node = node;
    WlNet_Init(&n->net, &io, node);
    WlRfu_Init(&n->rfu, &n->net, id);
}

static void Tick(Air *air, Node **nodes, int count, unsigned ms)
{
    for (unsigned t = 0; t < ms; ++t)
    {
        ++air->now;
        for (int i = 0; i < count; ++i)
            WlRfu_Step(&nodes[i]->rfu);
    }
}

#define CHECK(cond, ...) do { if (!(cond)) { printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); exit(1); } } while (0)

static void TestNet(unsigned loss)
{
    static Air air;
    Node a, b;
    uint32_t sent = 0, got = 0;

    memset(&air, 0, sizeof(air));
    air.lossPercent = loss;
    air.delayMs = 3;
    air.seed = 12345;
    NodeInit(&a, &air, 1, 0x100);
    NodeInit(&b, &air, 2, 0x200);
    for (unsigned ms = 0; ms < 60000; ++ms)
    {
        uint16_t src;
        WlMessage msg;

        ++air.now;
        if (ms % 7 == 0 && sent < 600 && WlNet_Pending(&a.net, 2) < WL_WINDOW)
        {
            uint8_t body[8];

            memcpy(body, &sent, 4);
            memset(body + 4, (int)(sent & 0xFF), 4);
            CHECK(WlNet_Send(&a.net, 2, body, 8), "send");
            ++sent;
        }
        WlNet_Step(&a.net);
        WlNet_Step(&b.net);
        while (WlNet_Pop(&b.net, &src, &msg))
        {
            uint32_t number;

            memcpy(&number, msg.data, 4);
            CHECK(src == 1 && msg.len == 8 && number == got && msg.data[4] == (number & 0xFF), "message %u arrived as %u", got, number);
            ++got;
        }
        while (WlNet_Pop(&a.net, &src, &msg))
            CHECK(false, "a stream nobody sends on");
    }
    CHECK(got == 600, "with %u%% loss %u of 600 messages arrived", loss, got);
}

static void TestSilent(void)
{
    static Air air;
    Node a, b;
    uint16_t lost = 0;
    uint8_t m = 1;

    memset(&air, 0, sizeof(air));
    air.delayMs = 1;
    NodeInit(&a, &air, 1, 1);
    NodeInit(&b, &air, 2, 2);
    CHECK(WlNet_Send(&a.net, 2, &m, 1), "send");
    for (unsigned ms = 0; ms < WL_SILENT_MS + 500; ++ms)
    {
        ++air.now;
        WlNet_Step(&a.net);         /* b never steps: it is gone */
        if (WlNet_PopLost(&a.net, &lost))
            break;
    }
    CHECK(lost == 2, "a silent node is reported lost (%u)", lost);
    CHECK(!WlNet_Send(&a.net, 1, &m, 1) && WlNet_Pending(&a.net, 1) == 0, "no sending to oneself");
    for (unsigned i = 0; i < WL_WINDOW + 4; ++i)
    {
        bool ok = WlNet_Send(&a.net, 2, &m, 1);

        CHECK(ok == (i < WL_WINDOW), "the window holds %u messages (message %u %s)", WL_WINDOW, i, ok ? "taken" : "refused");
    }
}

static int Cmd(WlRfu *rfu, unsigned command, const uint32_t *params, unsigned count, uint32_t *ack)
{
    uint32_t req[WL_RFU_ACK_WORDS + 4];

    req[0] = 0x99660000u | count << 8 | command;
    for (unsigned i = 0; i < count; ++i)
        req[1 + i] = params[i];
    return WlRfu_Command(rfu, req, ack);
}

static void Configure(Node *n, const char *game, const char *user, unsigned slots)
{
    uint32_t p[6] = {0}, ack[WL_RFU_ACK_WORDS];
    uint8_t *b = (uint8_t *)p;
    uint8_t sum = 0;

    b[0] = 0x34;
    b[1] = 0x12;
    memcpy(b + 2, game, strlen(game) < 13 ? strlen(game) : 13);
    memcpy(b + 16, user, strlen(user) < 8 ? strlen(user) : 8);
    for (int i = 0; i < 8; ++i)
        sum = (uint8_t)(sum + b[2 + i] + b[16 + i]);
    b[15] = (uint8_t)~sum;
    CHECK(Cmd(&n->rfu, WLC_RESET, NULL, 0, ack) == 1 && ack[0] == 0x99660090u, "reset acknowledged");
    CHECK(Cmd(&n->rfu, WLC_GAME_CONFIG, p, 6, ack) == 1 && ack[0] == 0x99660096u, "game config acknowledged");
    p[0] = ((uint32_t)(0x3C | (4 - slots)) << 16) | 0x0200u;
    CHECK(Cmd(&n->rfu, WLC_SYSTEM_CONFIG, p, 1, ack) == 1 && ack[0] == 0x99660097u, "system config acknowledged");
}

static void TestUnknown(void)
{
    static Air air;
    Node a;
    uint32_t ack[WL_RFU_ACK_WORDS];

    memset(&air, 0, sizeof(air));
    NodeInit(&a, &air, 1, 7);
    CHECK(Cmd(&a.rfu, 0x3A, NULL, 0, ack) == 2 && ack[0] == 0x996601EEu && ack[1] == 1, "an unused command in range is refused with 1");
    CHECK(Cmd(&a.rfu, 0x77, NULL, 0, ack) == 2 && ack[0] == 0x996601EEu && ack[1] == 2, "an unknown command is refused with 2");
    CHECK(Cmd(&a.rfu, WLC_SC_START, NULL, 0, ack) == 2 && ack[0] == 0x996601EEu, "a host cannot start before it has its game data");
    {
        uint32_t bad[2] = {0x12340000u | WLC_RESET, 0};

        CHECK(WlRfu_Command(&a.rfu, bad, ack) == 2 && ack[0] == 0x996601EEu, "a request without the 0x9966 mark is refused");
    }
}

/* One host (node 1) and `children` children (nodes 2...): they find each other, connect, and trade frames. */
static void TestLink(unsigned children, unsigned loss)
{
    static Air air;
    Node host, kid[3];
    Node *all[4];
    uint32_t ack[WL_RFU_ACK_WORDS], p[8] = {0}, req[WL_RFU_ACK_WORDS];
    unsigned words;

    memset(&air, 0, sizeof(air));
    air.delayMs = 2;
    air.lossPercent = loss;
    air.seed = 99 + children + loss;
    NodeInit(&host, &air, 1, 0x1111);
    all[0] = &host;
    for (unsigned i = 0; i < children; ++i)
    {
        NodeInit(&kid[i], &air, (uint16_t)(2 + i), (uint16_t)(0x2000 + i));
        all[1 + i] = &kid[i];
    }
    Configure(&host, "POKEMON FIRE", "ALEX", 4);
    for (unsigned i = 0; i < children; ++i)
        Configure(&kid[i], "POKEMON FIRE", i ? "BEA" : "CARLOS", 4);
    {
        uint32_t members = 0;

        for (unsigned i = 0; i < 1 + children; ++i)
            members |= 1u << (1 + i);
        WlRfu_SetMembers(&host.rfu, members & ~2u);
        for (unsigned i = 0; i < children; ++i)
            WlRfu_SetMembers(&kid[i].rfu, members & ~(1u << (2 + i)));
    }
    CHECK(Cmd(&host.rfu, WLC_SC_START, NULL, 0, ack) == 1 && ack[0] == 0x99660099u, "host starts");
    CHECK(((Cmd(&host.rfu, WLC_SYSTEM_STATUS, NULL, 0, ack), ack[1]) >> 24) == WL_STATE_HOST_OPEN, "state: host open");

    for (unsigned i = 0; i < children; ++i)
    {
        uint32_t id;
        const uint8_t *e = (const uint8_t *)(ack + 1);
        unsigned tries = 0;
        uint8_t sum = 0;

        CHECK(Cmd(&kid[i].rfu, WLC_SP_START, NULL, 0, ack) == 1, "child searches");
        do
        {
            Tick(&air, all, 1 + (int)children, 20);
            words = (unsigned)Cmd(&kid[i].rfu, WLC_SP_POLL, NULL, 0, ack);
        } while (words == 1 && ++tries < 200);
        CHECK(words == 8 && (ack[0] >> 8 & 0xFF) == 7, "the child lists one host: %u words", words);
        id = (uint32_t)(e[0] | e[1] << 8);
        CHECK(id == 0x1111, "the host's id (0x%X)", id);
        CHECK(e[4] == 0x34 && e[5] == 0x12 && memcmp(e + 6, "POKEMON FIRE", 12) == 0 && memcmp(e + 20, "ALEX", 4) == 0, "game data and name as set");
        for (int k = 0; k < 8; ++k)
            sum = (uint8_t)(sum + e[6 + k] + e[20 + k]);
        CHECK((uint8_t)~sum == e[19], "the checksum librfu verifies (%02X vs %02X)", (uint8_t)~sum, e[19]);
        CHECK(Cmd(&kid[i].rfu, WLC_SP_END, NULL, 0, ack) == 1, "search ends");
        p[0] = id;
        CHECK(Cmd(&kid[i].rfu, WLC_CP_START, p, 1, ack) == 1 && ack[0] == 0x9966009Fu, "connect starts");
        tries = 0;
        do
        {
            Tick(&air, all, 1 + (int)children, 20);
            Cmd(&kid[i].rfu, WLC_CP_POLL, NULL, 0, ack);
        } while ((ack[1] >> 24) == 1 && ++tries < 200);
        CHECK((ack[1] >> 24) == 0 && (ack[1] >> 16 & 0xFF) == i && (ack[1] & 0xFFFF) == 0x1111, "child %u connected in slot %u (word 0x%08X)", i, i, ack[1]);
        CHECK(Cmd(&kid[i].rfu, WLC_CP_END, NULL, 0, ack) == 2 && (ack[1] >> 24) == 0, "connect ends");
    }
    Tick(&air, all, 1 + (int)children, 50);
    words = (unsigned)Cmd(&host.rfu, WLC_SC_POLL, NULL, 0, ack);
    CHECK(words == 1 + children, "the host lists %u children (%u words)", children, words);
    for (unsigned i = 0; i < children; ++i)
        CHECK((ack[1 + i] & 0xFFFF) == 0x2000 + i && (ack[1 + i] >> 16 & 0xFF) == i, "child %u as the host lists it (0x%08X)", i, ack[1 + i]);
    Cmd(&host.rfu, WLC_LINK_STATUS, NULL, 0, ack);
    CHECK((ack[1] & 0xFF) != 0 && (children < 2 || (ack[1] >> 8 & 0xFF) != 0) && (children >= 2 || (ack[1] >> 8 & 0xFF) == 0), "link status shows the slots used");

    /* A frame cycle: the host sends 9 bytes and changes the clock; each child sees it, answers with its own, and the host
     * learns they all did. */
    for (unsigned round = 0; round < 3; ++round)
    {
        uint8_t hostBody[12];

        /* Each child sends a frame of 3 + its slot bytes and hands the clock over. */
        for (unsigned i = 0; i < children; ++i)
        {
            uint8_t kidBody[8];

            memset(kidBody, 0x40 + (int)(round * 16 + i), sizeof(kidBody));
            p[0] = (uint32_t)(3 + i) << (8 + 5 * i);
            memcpy(&p[1], kidBody, 8);
            CHECK(Cmd(&kid[i].rfu, WLC_DATA_TX_AND_CHANGE, p, 3, ack) == 1 && ack[0] == 0x996600A5u, "child sends and changes");
            CHECK(WlRfu_IsSlave(&kid[i].rfu), "the child console is the clock slave now");
        }
        memset(hostBody, 0xA0 + (int)round, sizeof(hostBody));
        p[0] = 9;                                           /* the header: the host's size in bytes */
        memcpy(&p[1], hostBody, 12);
        CHECK(Cmd(&host.rfu, WLC_DATA_TX_AND_CHANGE, p, 4, ack) == 1 && ack[0] == 0x996600A5u, "host sends and changes");
        CHECK(WlRfu_IsSlave(&host.rfu), "the host console is the clock slave now");

        for (unsigned i = 0; i < children; ++i)
        {
            int n = 0;
            unsigned tries = 0;

            while (!(n = WlRfu_PollSlave(&kid[i].rfu, req)) && tries++ < 600)
                Tick(&air, all, 1 + (int)children, 5);
            CHECK(n == 1 && req[0] == 0x99660028u, "child %u is told the host's frame is there (%d words, 0x%08X)", i, n, req[0]);
            CHECK(!WlRfu_IsSlave(&kid[i].rfu), "and is the master again");
            words = (unsigned)Cmd(&kid[i].rfu, WLC_DATA_RX, NULL, 0, ack);
            CHECK(words == 1 + 1 + 3 && (ack[0] >> 8 & 0xFF) == 4 && (ack[1] & 0x7F) == 9, "child %u reads %u words, size %u", i, words, ack[1] & 0x7F);
            CHECK(memcmp(ack + 2, hostBody, 9) == 0, "the host's bytes arrive whole");
            CHECK(Cmd(&kid[i].rfu, WLC_DATA_RX, NULL, 0, ack) == 1, "and only once");
        }
        {
            unsigned seen = 0, turns = 0;

            /* With lost frames some children answer later: the host takes what is there, waits again, takes the rest. */
            while (seen != (1u << children) - 1 && turns++ < 20)
            {
                int n = 0;
                unsigned tries = 0, at = 0;
                const uint8_t *got;

                while (!(n = WlRfu_PollSlave(&host.rfu, req)) && tries++ < 600)
                    Tick(&air, all, 1 + (int)children, 5);
                CHECK(n >= 1 && (req[0] & 0xFF) == 0x28, "the host is told data is ready (%d words, 0x%08X)", n, req[0]);
                words = (unsigned)Cmd(&host.rfu, WLC_DATA_RX, NULL, 0, ack);
                got = (const uint8_t *)(ack + 2);
                for (unsigned i = 0; words > 1 && i < children; ++i)
                {
                    unsigned size = ack[1] >> (8 + 5 * i) & 0x1F;

                    if (!size)
                        continue;
                    CHECK(size == 3 + i && !(seen >> i & 1), "child %u's size in the host's header (0x%08X)", i, ack[1]);
                    for (unsigned k = 0; k < size; ++k)
                        CHECK(got[at + k] == 0x40 + round * 16 + i, "child %u's byte %u", i, k);
                    at += size;
                    seen |= 1u << i;
                }
                if (seen != (1u << children) - 1)
                    CHECK(Cmd(&host.rfu, WLC_MS_CHANGE, NULL, 0, ack) == 1, "the host waits again");
            }
            CHECK(seen == (1u << children) - 1, "the host got every child's frame (mask %u)", seen);
        }
    }

    /* A child leaves: the host, waiting as the slave, is told which slot and that it is gone. */
    {
        uint32_t mask[1] = {1};
        int n = 0;
        unsigned tries = 0;

        CHECK(Cmd(&kid[0].rfu, WLC_DISCONNECT, mask, 1, ack) == 1 && ack[0] == 0x996600B0u, "the child disconnects");
        CHECK(Cmd(&host.rfu, WLC_MS_CHANGE, NULL, 0, ack) == 1 && WlRfu_IsSlave(&host.rfu), "the host waits");
        while (!(n = WlRfu_PollSlave(&host.rfu, req)) && tries++ < 600)
            Tick(&air, all, 1 + (int)children, 5);
        CHECK(n == 2 && req[0] == 0x99660129u && (req[1] & 0xFF) == 1 && (req[1] >> 8 & 0xFF) == 0,
              "the host hears slot 0 left (%d words, 0x%08X 0x%08X)", n, req[0], req[1]);
        Cmd(&host.rfu, WLC_SLOT_STATUS, NULL, 0, ack);
        CHECK(ack[1] == children - 1, "the host has %u children now", children - 1);
    }
    printf("link with %u children and %u%% loss: ok\n", children, loss);
}

int main(void)
{
    TestNet(0);
    TestNet(20);
    TestNet(30);
    TestSilent();
    TestUnknown();
    TestLink(1, 0);
    TestLink(1, 15);
    TestLink(2, 10);
    puts("wl_test ok");
    return 0;
}
