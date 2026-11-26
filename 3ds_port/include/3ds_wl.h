#ifndef CTR_WL_H
#define CTR_WL_H

/*
 * Local wireless between consoles, written for Twin Embers (nothing here comes from another project's code).
 *
 * Two layers, both free of the game's types and of the 3DS libraries so a computer can test them:
 *
 *   WlNet  a reliable, ordered message stream between the members of a session. The radio underneath
 *          (WlBackend) may lose, repeat or delay frames; WlNet numbers every message, acknowledges, and sends again
 *          what was not acknowledged.
 *   WlRfu  the Game Boy Advance Wireless Adapter as FireRed's librfu sees it: librfu sends a command (a word
 *          0x9966LLCC and LL words of parameters) and gets an acknowledgement (0x9966LL(CC|0x80) and LL words); here
 *          each command is answered at once, and what the other consoles say comes as WlNet messages. Link
 *          layer frames (the slots, NI and UNI transfers) are librfu's own: the adapter only carries their bytes.
 *
 * The console's UDS radio is the WlBackend of 3ds_wl_uds.c; the tests use a simulated one.
 */
#include <stdbool.h>
#include <stdint.h>

/* ---- WlNet ---- */

#define WL_MAX_NODES 16             /* UDS: a host and up to 15 others; a node's id is 1 to 16 */
#define WL_MAX_PAYLOAD 160          /* the largest message: an adapter frame is at most 91 bytes (a UDS frame holds 1478) */
#define WL_WINDOW 24                /* messages to one node that may wait for their acknowledgement */
#define WL_RESEND_MS 60             /* an unacknowledged message is sent again after this long */
#define WL_SILENT_MS 6000           /* a node that has acknowledged nothing for this long is lost */

typedef struct WlBackend {
    void *user;
    /* One radio frame to node `dest` (1 to 16); false if it could not be queued. */
    bool (*send)(void *user, uint16_t dest, const void *data, uint16_t len);
    /* The next radio frame received: its sender and bytes (at most `max`); false when there is none. */
    bool (*recv)(void *user, uint16_t *src, void *data, uint16_t max, uint16_t *len);
    uint32_t (*now)(void *user);    /* milliseconds, any origin */
} WlBackend;

typedef struct WlMessage {
    uint16_t len;
    uint8_t data[WL_MAX_PAYLOAD];
} WlMessage;

typedef struct WlPeer {
    bool known;
    uint16_t nextSeq;               /* the sequence number of the next message to this node */
    uint16_t sendBase;              /* the oldest not acknowledged */
    uint16_t expect;                /* the next one to arrive from it */
    uint32_t lastHeard;             /* when anything last came from it */
    uint32_t lastProgress;          /* when it last acknowledged something (or when a first message was queued) */
    uint32_t lastSent[WL_WINDOW];
    bool sentOnce[WL_WINDOW];
    uint16_t count;                 /* queued, not acknowledged */
    uint8_t head;                   /* where the oldest sits in the ring */
    WlMessage window[WL_WINDOW];    /* a ring from sendBase */
    bool ackOwed;
} WlPeer;

typedef struct WlNet {
    WlBackend io;
    uint16_t self;
    WlPeer peer[WL_MAX_NODES + 1];
    /* messages that arrived, in order for each sender, for WlNet_Pop */
    struct { uint16_t src; WlMessage msg; } inbox[64];
    uint8_t inboxHead, inboxCount;
    uint16_t lost[WL_MAX_NODES + 1];/* nodes found silent, for WlNet_PopLost */
    uint8_t lostCount;
} WlNet;

void WlNet_Init(WlNet *net, const WlBackend *io, uint16_t self);
/* Queue a message for node `dest`; false when its window is full or the message is too long. */
bool WlNet_Send(WlNet *net, uint16_t dest, const void *data, uint16_t len);
/* Receive what the radio has, send what is due again, acknowledge. Call often (every frame or whenever the radio wakes). */
void WlNet_Step(WlNet *net);
/* The next message that arrived (in order for its sender); false when none. */
bool WlNet_Pop(WlNet *net, uint16_t *src, WlMessage *msg);
/* A node found silent for WL_SILENT_MS with messages unacknowledged (or one that WlNet_Lose was told of); false when none. */
bool WlNet_PopLost(WlNet *net, uint16_t *node);
/* Its queue and numbers are cleared (a console that joins starts from nothing); nobody is told the node is gone. */
void WlNet_Forget(WlNet *net, uint16_t node);
/* The node left: cleared as by WlNet_Forget, and reported by WlNet_PopLost so the link above frees its slot at once. Without the report
 * a dead partner's slot stays linked, for good when a newcomer is given the same seat and acknowledges in its place. */
void WlNet_Lose(WlNet *net, uint16_t node);
/* How many messages to `dest` still wait for their acknowledgement. */
uint16_t WlNet_Pending(const WlNet *net, uint16_t dest);

/* ---- WlRfu ---- */

#define WL_RFU_CHILD_MAX 4
#define WL_RFU_GAME_BYTES 16        /* serial number (2), game name (13), checksum (1) */
#define WL_RFU_USER_BYTES 8
#define WL_RFU_ACK_WORDS 29         /* an acknowledgement or a request: the header and up to 28 words */
#define WL_RFU_PARENT_FRAME 87      /* the most a parent sends in one frame, in bytes */
#define WL_RFU_RX_QUEUE 8           /* frames a slot keeps behind the one the game has not read yet */
#define WL_RFU_CHILD_FRAME 31       /* and a child */

enum { WL_STATE_IDLE = 0, WL_STATE_HOST_CLOSED = 1, WL_STATE_HOST_OPEN = 2, WL_STATE_SEARCHING = 3, WL_STATE_CONNECTING = 4, WL_STATE_CLIENT = 5 };

typedef struct WlRfuPeer {
    bool used;
    uint16_t id;                    /* the adapter id the other console has */
    uint16_t node;                  /* its node in the session */
    uint8_t game[WL_RFU_GAME_BYTES];
    uint8_t user[WL_RFU_USER_BYTES];
    bool hosting;                   /* heard as a host in the session */
    bool open;                      /* accepting children */
    uint8_t slotsFree;
    uint32_t heardMs;
} WlRfuPeer;

typedef struct WlRfu {
    WlNet *net;
    uint16_t myId;                  /* this adapter's id (never 0) */
    uint8_t state;                  /* WL_STATE_* */
    uint8_t game[WL_RFU_GAME_BYTES];
    uint8_t user[WL_RFU_USER_BYTES];
    bool configured;
    uint8_t availSlots;             /* 1 to 4: how many children a host takes */
    /* host */
    bool childUsed[WL_RFU_CHILD_MAX];
    uint16_t childId[WL_RFU_CHILD_MAX];
    uint16_t childNode[WL_RFU_CHILD_MAX];
    uint32_t childHeard[WL_RFU_CHILD_MAX];
    /* child */
    uint16_t parentNode, parentId;
    uint8_t mySlot;
    uint8_t connectStatus;          /* 0 done, 1 in process, 2 slot closed, 3 disconnected */
    uint16_t tryId;
    bool linkLost;
    /* what the other consoles are */
    WlRfuPeer seen[WL_MAX_NODES + 1];
    /* data: what each remote end last sent, waiting for DataRx */
    bool slave;                     /* the console is the clock slave (after a command that changes it) */
    uint8_t rx[WL_RFU_CHILD_MAX][WL_RFU_PARENT_FRAME + 4];
    uint8_t rxLen[WL_RFU_CHILD_MAX];
    bool rxNew[WL_RFU_CHILD_MAX];
    uint32_t rxHeader[WL_RFU_CHILD_MAX];
    /* Frames that came while the one above was still unread, in order (a burst of the radio, as TCP delivers them over the internet):
     * the next one takes its place when it is read, so none is overwritten. The oldest is dropped only past WL_RFU_RX_QUEUE. */
    uint8_t rxq[WL_RFU_CHILD_MAX][WL_RFU_RX_QUEUE][WL_RFU_PARENT_FRAME + 4];
    uint8_t rxqLen[WL_RFU_CHILD_MAX][WL_RFU_RX_QUEUE];
    uint32_t rxqHeader[WL_RFU_CHILD_MAX][WL_RFU_RX_QUEUE];
    uint8_t rxqHead[WL_RFU_CHILD_MAX], rxqCount[WL_RFU_CHILD_MAX];
    uint32_t rxDropped;                 /* frames dropped from a full queue (none expected) */
    uint8_t txFrame[WL_RFU_PARENT_FRAME + 4];
    uint8_t txLen;
    uint32_t txHeader;
    bool txOwed;                    /* a frame was sent by the console and the exchange has not completed */
    bool dataReady;                 /* something to tell a slave console (command 0x28) */
    uint8_t ackBits;                /* children that answered, for the host's data ready */
    uint8_t lostSlots;              /* slots to report lost (command 0x29) */
    uint32_t nowMs;
    uint32_t txMs;                  /* when the console last sent a frame and handed over the clock */
    uint32_t members;               /* the nodes of the session, a bit for each (set by the layer above) */
    uint32_t lastBeacon;
} WlRfu;

void WlRfu_Init(WlRfu *rfu, WlNet *net, uint16_t myId);
/* The nodes that are in the session now (bit n for node n, not this one). Hosts tell them they are hosting. */
void WlRfu_SetMembers(WlRfu *rfu, uint32_t members);
/* Answer one command. req[0] is the command word (0x9966LLCC), req[1..LL] its parameters. The answer goes to ack[]
 * (ack[0] the header, then its parameter words); the return value is the number of words in all (at least 1). A
 * command that is not known or not allowed now is rejected: 0x996601EE and a reason word. */
int WlRfu_Command(WlRfu *rfu, const uint32_t *req, uint32_t *ack);
/* Pump the radio's messages (call once a frame, before WlRfu_PollSlave). */
void WlRfu_Step(WlRfu *rfu);
/* For a console that is the clock slave: what the adapter says to it by itself, as a request in `req` (0x9966LLCC and
 * parameters), the return value its length in words; 0 when there is nothing. The console becomes the master again. */
int WlRfu_PollSlave(WlRfu *rfu, uint32_t *req);
bool WlRfu_IsSlave(const WlRfu *rfu);

/* Command ids the adapter answers, as librfu names them. */
enum {
    WLC_RESET = 0x10, WLC_LINK_STATUS = 0x11, WLC_VERSION = 0x12, WLC_SYSTEM_STATUS = 0x13, WLC_SLOT_STATUS = 0x14,
    WLC_CONFIG_STATUS = 0x15, WLC_GAME_CONFIG = 0x16, WLC_SYSTEM_CONFIG = 0x17, WLC_SC_START = 0x19, WLC_SC_POLL = 0x1a,
    WLC_SC_END = 0x1b, WLC_SP_START = 0x1c, WLC_SP_POLL = 0x1d, WLC_SP_END = 0x1e, WLC_CP_START = 0x1f, WLC_CP_POLL = 0x20,
    WLC_CP_END = 0x21, WLC_DATA_TX = 0x24, WLC_DATA_TX_AND_CHANGE = 0x25, WLC_DATA_RX = 0x26, WLC_MS_CHANGE = 0x27,
    WLC_DATA_READY_AND_CHANGE = 0x28, WLC_DISCONNECTED_AND_CHANGE = 0x29, WLC_DISCONNECT = 0x30, WLC_TEST_MODE = 0x31,
    WLC_CPR_START = 0x32, WLC_CPR_POLL = 0x33, WLC_CPR_END = 0x34, WLC_RESUME_RETRANSMIT_AND_CHANGE = 0x37, WLC_STOP_MODE = 0x3d
};

#endif
