/* One console of an end-to-end test: FireRed's own librfu and Link Manager (the patched sources of a bootstrapped workspace, recipe 0272),
 * the adapter emulation and the glue (3ds_wl_glue.c), the internet radio to a relay; tools/test_wl_e2e.py runs two of these (a parent and a
 * child) against tools/wl_relay.py and checks they find each other and connect, as two players do in the Union Room.
 *
 *   rfu_game_test parent|child|pcparent|pcchild <relay port> <room code>   (pc: both in the parent-child switching mode)
 * Prints one line a step: "EVENT <name>"; ends with "DONE" (or "TIMEOUT"). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* Data both ways once linked, as link_rfu_2.c sets it up: the child sends 14 bytes on its slot, the parent 70 to its children (UNI);
 * the first byte counts frames, and each side counts the new values it reads. */
static u8 sChildSend[14], sChildRecv[70], sParentSend[70], sParentRecv[4][14];
static int sUniReady, sUniSlot, sUniGot;
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
    if (!sIsParent)
    {
        rfu_REQ_recvData();
        rfu_waitREQComplete();
        if (sUniReady && gRfuSlotStatusUNI[sUniSlot]->recv.newDataFlag)
        {
            UniCheck(sChildRecv);
            rfu_UNI_clearRecvNewDataFlag(sUniSlot);
        }
        if (sUniReady)
        {
            ++sChildSend[0];
            rfu_UNI_readySendData(sUniSlot);
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
    if (sIsParent && sExchange && rfu_getMasterSlave() == AGB_CLK_MASTER)
    {
        /* the parent's turn, once a frame (link_rfu_2.c, RfuMain1_Parent): read, then send and hand the clock over */
        rfu_REQ_recvData();
        rfu_waitREQComplete();
        if (sUniReady && gRfuSlotStatusUNI[sUniSlot]->recv.newDataFlag)
        {
            UniCheck(sParentRecv[sUniSlot]);
            rfu_UNI_clearRecvNewDataFlag(sUniSlot);
        }
        if (sUniReady)
        {
            ++sParentSend[0];
            rfu_UNI_readySendData(sUniSlot);
        }
        rfu_LMAN_REQ_sendData(FALSE);
    }
    rfu_LMAN_manager_entity((u32)rand());
    rfu_LMAN_syncVBlank();
#ifdef _WIN32
    Sleep(17);
#else
    usleep(17000);
#endif
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

        for (frames = 0; frames < 60 * hold; ++frames)
            Frame();                                /* a while more (a second unless E2E_HOLD says): the link stays up */
    }
    printf("STATE %d others %d%c", CtrWl_State(), CtrWl_Others(), 10);
    printf("UNI got %d%c", sUniGot, 10);
    puts("DONE");
    CtrWl_Shutdown();
    return 0;
}
