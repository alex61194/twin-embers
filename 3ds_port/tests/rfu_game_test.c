/* One console of an end-to-end test: FireRed's own librfu and Link Manager (the patched sources of a bootstrapped workspace, recipe 0272),
 * the adapter emulation and the glue (3ds_wl_glue.c), the internet radio to a relay; tools/test_wl_e2e.py runs two of these (a parent and a
 * child) against tools/wl_relay.py and checks they find each other and connect, as two players do in the Union Room.
 *
 *   rfu_game_test parent|child|pcparent|pcchild <relay port> <room code>   (pc: both in the parent-child switching mode)
 * Prints one line a step: "EVENT <name>"; ends with "DONE" (or "TIMEOUT"). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <unistd.h>
#endif

#include "global.h"
#include "gba_shadow.h"
#include "librfu.h"
#include "AgbRfu_LinkManager.h"

#include "3ds_wl_glue.h"
#include "3ds_wl_inet.h"

/* What the game provides around librfu: the interrupt table the library installs its handlers in, and the few routines it calls. */
IntrFunc gIntrTable[16];
void *CtrAssets_Memcpy(void *dest, const void *source, size_t bytes)
{
    return __builtin_memcpy(dest, source, bytes);
}
void IntrSIO32(void) {}

/* The library carves its tables out of this: sized for 32-bit pointers on the console (0xe64 bytes), too small for the same structures with 64-bit
 * pointers on a computer. */
static u32 sApiBuffer[0x4000 / 4];
static u8 sGameName[RFU_GAME_NAME_LENGTH + 2];
static u8 sUserName[RFU_USER_NAME_LENGTH];
static u16 sSerials[] = {0x0002, 0xFFFF};
static INIT_PARAM sConfig;
static int sIsParent, sSwitch;              /* sSwitch: both search as parent and child in turn, as in the Union Room */
static volatile int sConnected, sNameSent;

static void Say(const char *what)
{
    printf("EVENT %s%c", what, 10);
    fflush(stdout);
}

static volatile int sExchange;
static volatile int sParentFinished;
static int sParentWaits;
static unsigned sFrames;                         /* frames run, for the frame rate at the end */                        /* turns the parent waited for its children in the E2E_PARENT_CHANGE mode */           /* link_rfu_2.c's gRfu.parentFinished: set by the parent's callback when the adapter is done */

/* Data both ways once linked, as link_rfu_2.c sets it up: the child sends 14 bytes on its slot, the parent 70 to its children (UNI);
 * the first byte counts frames, and each side counts the new values it reads. */
static u8 sChildSend[14], sChildRecv[70], sParentSend[70], sParentRecv[4][14];
static int sUniReady, sUniSlot, sUniGot, sParentTurn;
static u8 sNiSend = 0x5A, sNiRecv;                 /* one status byte the child sends reliably (NI), as link_rfu_2.c's partner statuses */
static u8 sUniLast;

static int FirstSlot(u8 mask)
{
    int i = 0;

    while (i < 4 && !(mask & (1 << i)))
        ++i;
    return i < 4 ? i : 0;
}

static void UniCheck(const u8 *data)
{
    if (data[0] != sUniLast)
    {
        sUniLast = data[0];
        ++sUniGot;
    }
}                      /* a parent runs the data exchange once a child is there */

/* The child's answer to each exchange the parent opens (link_rfu_2.c, MscCallback_Child): read what came, ask for the clock back. */
static void Msc(u16 command)
{
    (void)command;
    if (sIsParent)
        sParentFinished = 1;                       /* link_rfu_2.c, MSCCallback_Parent */
    if (!sIsParent)
    {
        rfu_REQ_recvData();
        rfu_waitREQComplete();
        if (sUniReady && gRfuSlotStatusUNI[sUniSlot]->recv.newDataFlag)
        {
            UniCheck(sChildRecv);
            rfu_UNI_clearRecvNewDataFlag(sUniSlot);
            ++sChildSend[0];
            if (!getenv("E2E_NI"))
                rfu_UNI_readySendData(sUniSlot);    /* as link_rfu_2.c's child: it answers only what came */
        }
        rfu_LMAN_REQ_sendData(TRUE);
    }
}

static void Manager(u8 message, u8 param)
{
    switch (message)
    {
    case LMAN_MSG_INITIALIZE_COMPLETED:
        Say("initialized");
        rfu_LMAN_establishConnection(sSwitch ? MODE_P_C_SWITCH : sIsParent ? MODE_PARENT : MODE_CHILD, 0, 240, sSerials);
        break;
    case LMAN_MSG_NEW_CHILD_CONNECT_DETECTED:
        Say("child detected");
        sExchange = 1;
        break;
    case LMAN_MSG_NEW_CHILD_CONNECT_ACCEPTED:
        Say("child accepted");
        sConnected = 1;
        sUniSlot = FirstSlot(lman.acceptSlot_flag);
        rfu_setRecvBuffer(TYPE_UNI, sUniSlot, sParentRecv[sUniSlot], sizeof(sParentRecv[0]));
        rfu_UNI_setSendData(lman.acceptSlot_flag, sParentSend, sizeof(sParentSend));
        if (getenv("E2E_NI"))
            rfu_setRecvBuffer(TYPE_NI, sUniSlot, &sNiRecv, sizeof(sNiRecv));
        sUniReady = 1;
        break;
    case LMAN_MSG_PARENT_FOUND:
    {
        int i = 0;

        Say("parent found");
        if (sIsParent)
            break;                                  /* in the switching mode the other one is the one who talks */
        while (i < 4 && !(param & (1 << i)))
            ++i;
        rfu_LMAN_CHILD_connectParent(gRfuLinkStatus->partner[i].id, 240);
        break;
    }
    case LMAN_MSG_CONNECT_PARENT_SUCCESSED:
        Say("connected to parent");
        sConnected = 1;
        break;
    case LMAN_MSG_CHILD_NAME_SEND_COMPLETED:
        Say("name sent");
        sNameSent = 1;
        sUniSlot = FirstSlot(gRfuLinkStatus->connSlotFlag);
        rfu_setRecvBuffer(TYPE_UNI, sUniSlot, sChildRecv, sizeof(sChildRecv));
        if (getenv("E2E_NI"))
            printf("NI set %x%c", (unsigned)rfu_NI_setSendData(1 << sUniSlot, 8, &sNiSend, 1), 10);   /* a child's frame holds the UNI or this */
        else
            rfu_UNI_setSendData(1 << sUniSlot, sChildSend, sizeof(sChildSend));
        sUniReady = 1;
        break;
    case LMAN_MSG_CONNECT_PARENT_FAILED:
        Say("connect failed");
        break;
    case LMAN_MSG_REQ_API_ERROR:
        Say("req api error");
        break;
    default:
        break;
    }
}

static void Frame(void)
{
    static int n;

    if (getenv("E2E_TRACE") && ++n % 30 == 0)
        fprintf(stderr, "TICK %d state %d others %d ms %d%c", n, CtrWl_State(), CtrWl_Others(), (int)rfu_getMasterSlave(), 10);
    CtrWl_Tick();
    if (sIsParent && sUniReady && getenv("E2E_PARENT_CHANGE") && rfu_getMasterSlave() == AGB_CLK_MASTER
        && (gRfuLinkStatus->connSlotFlag >> sUniSlot & 1))
    {
        /* As link_rfu_2.c's linked parent: send and hand the clock over (RfuMain1_Parent), then wait in a loop for the adapter's answer
         * (RfuMain2_Parent); without CtrRfu_Wait that loop never ends here, as it froze the contacted console in the Union Room. */
        unsigned spins = 0;

        sParentFinished = 0;
        ++sParentSend[0];
        rfu_UNI_readySendData(sUniSlot);
        rfu_LMAN_REQ_sendData(TRUE);
        while (!sParentFinished && (gRfuLinkStatus->connSlotFlag >> sUniSlot & 1))
        {
            if (++spins > 2000000)
            {
                printf("PARENT WAIT STUCK ms %d stwi msMode %d sending %d state %d others %d%c", (int)rfu_getMasterSlave(), (int)gSTWIStatus->msMode,
                       (int)gSTWIStatus->sending, (int)gSTWIStatus->state, CtrWl_Others(), 10);
                fflush(stdout);
                exit(6);
            }
            if (!getenv("E2E_NO_WAIT"))
                CtrRfu_Wait();
        }
        ++sParentWaits;
        rfu_REQ_recvData();
        rfu_waitREQComplete();
        if (gRfuSlotStatusUNI[sUniSlot]->recv.newDataFlag)
        {
            UniCheck(sParentRecv[sUniSlot]);
            rfu_UNI_clearRecvNewDataFlag(sUniSlot);
        }
    }
    else if (sIsParent && sExchange && rfu_getMasterSlave() == AGB_CLK_MASTER)
    {
        /* the parent's turn, once a frame (link_rfu_2.c, RfuMain1_Parent): read, then send and hand the clock over */
        rfu_REQ_recvData();
        rfu_waitREQComplete();
        if (sUniReady && gRfuSlotStatusUNI[sUniSlot]->recv.newDataFlag)
        {
            UniCheck(sParentRecv[sUniSlot]);
            rfu_UNI_clearRecvNewDataFlag(sUniSlot);
        }
        if (sUniReady && (!getenv("E2E_SPARSE") || ++sParentTurn % 3 == 0))
        {
            ++sParentSend[0];
            rfu_UNI_readySendData(sUniSlot);    /* E2E_SPARSE: new data one turn in three, as the game's parent often has none */
        }
        rfu_LMAN_REQ_sendData(FALSE);
    }
    rfu_LMAN_manager_entity((u32)rand());
    rfu_LMAN_syncVBlank();
    /* The rest of a 60 Hz frame: idle (as the console's frame pacing does), or (E2E_IDLE) given to the radio a millisecond at a time, as
     * 3ds_game_bridge.c does while a session is on. */
    {
        static uint32_t frameStart;
        uint32_t now = (uint32_t)clock() * (1000u / CLOCKS_PER_SEC);

        if (getenv("E2E_IDLE"))
            while ((uint32_t)clock() * (1000u / CLOCKS_PER_SEC) - frameStart < 15)
            {
                CtrWl_Idle();
#ifdef _WIN32
                Sleep(1);
#else
                usleep(1000);
#endif
            }
        else
        {
#ifdef _WIN32
            Sleep(17);
#else
            usleep(17000);
#endif
        }
        (void)now;
        frameStart = (uint32_t)clock() * (1000u / CLOCKS_PER_SEC);
        ++sFrames;
    }
}

int main(int argc, char **argv)
{
    uint8_t build[16] = {7, 7, 7};
    int frames;

    if (argc < 4)
        return 2;
    sSwitch = !strncmp(argv[1], "pc", 2);
    sIsParent = !strcmp(argv[1] + (sSwitch ? 2 : 0), "parent");
#ifdef _WIN32
    {
        WSADATA wsa;

        WSAStartup(MAKEWORD(2, 2), &wsa);
    }
#endif
    srand((unsigned)atoi(argv[2]) + (sIsParent ? 1u : 2u));
    CtrWl_Setup(build, (uint32_t)rand());
    CtrWl_SetInternet("127.0.0.1", (uint16_t)atoi(argv[2]), argv[3]);

    REG_IME = 1;                                    /* the library will not send with interrupts off */
    gGbaShadow.regs[0x208] = 1;
    memset(sUserName, 0, sizeof(sUserName));
    memcpy(sUserName, sIsParent ? "PARENT" : "CHILD", sIsParent ? 6 : 5);
    memset(sGameName, 0, sizeof(sGameName));
    sConfig.maxMFrame = 4;
    sConfig.MC_TimerCount = 32;
    sConfig.availSlot_flag = AVAIL_SLOT4;
    sConfig.mboot_flag = 0;
    sConfig.serialNo = 0x0002;
    sConfig.gameName = sGameName;
    sConfig.userName = sUserName;
    sConfig.fastSearchParent_flag = 1;
    sConfig.linkRecovery_enable = 0;
    sConfig.linkRecovery_period = 600;
    sConfig.NI_failCounter_limit = 300;

    if (!CtrWl_Available())
    {
        Say("adapter not available");
        return 3;
    }
    if (rfu_initializeAPI(sApiBuffer, 0x4000, &gIntrTable[1], TRUE))
    {
        Say("initializeAPI failed");
        return 4;
    }
    rfu_setTimerInterrupt(3, &gIntrTable[2]);
    Say("api ready");
    if (rfu_LMAN_initializeManager(Manager, Msc))
    {
        Say("manager failed");
        return 5;
    }
    rfu_LMAN_initializeRFU(&sConfig);
    for (frames = 0; frames < 60 * 30 && !(sConnected && (sIsParent || sNameSent)); ++frames)
        Frame();
    if (!sConnected)
    {
        Say("state");
        printf("STATE %d others %d%c", CtrWl_State(), CtrWl_Others(), 10);
        puts("TIMEOUT");
        return 1;
    }
    {
        int hold = getenv("E2E_HOLD") ? atoi(getenv("E2E_HOLD")) : 1;
        clock_t start = clock();
        unsigned first = sFrames;

        for (frames = 0; frames < 60 * hold; ++frames)
            Frame();                                /* a while more (a second unless E2E_HOLD says): the link stays up */
        printf("FPS %.1f%c", (double)(sFrames - first) * CLOCKS_PER_SEC / (double)(clock() - start + 1), 10);
    }
    printf("STATE %d others %d%c", CtrWl_State(), CtrWl_Others(), 10);
    printf("UNI got %d NI %02x waits %d%c", sUniGot, sNiRecv, sParentWaits, 10);
    printf("NI send state %04x fail %u err %04x phase %u | recv state %04x fail %u err %04x%c", gRfuSlotStatusNI[sUniSlot]->send.state, gRfuSlotStatusNI[sUniSlot]->send.failCounter, gRfuSlotStatusNI[sUniSlot]->send.errorCode, gRfuSlotStatusNI[sUniSlot]->send.phase, gRfuSlotStatusNI[sUniSlot]->recv.state, gRfuSlotStatusNI[sUniSlot]->recv.failCounter, gRfuSlotStatusNI[sUniSlot]->recv.errorCode, 10);
    puts("DONE");
    CtrWl_Shutdown();
    return 0;
}
