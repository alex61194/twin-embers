#ifndef CTR_WL_UDS_H
#define CTR_WL_UDS_H

/*
 * The 3DS's own local wireless (UDS, libctru's uds.h) as a WlBackend (3ds_wl.h): consoles near each other meet in a network
 * of one host and up to 15 others, without the internet. The node numbers of UDS (1 for the host, 2 and up for the others) are
 * the node numbers WlNet uses. Only a console running the same build (the 16-byte build id every console passes to
 * WlUds_Init) can see or join a host: the id is in the beacon and in the network's passphrase.
 *
 * Searching takes a second or two and blocks (call WlUds_Scan from a thread of its own, as the frame loop must not stall);
 * everything else returns at once. The caller owns the radio: udsInit is called here and the console's wireless switch must
 * be on (the calls fail otherwise and say so with false).
 */
#include <3ds/types.h>
#include <3ds/result.h>
#include <3ds/services/uds.h>
#include <stdbool.h>
#include <stdint.h>

#include "3ds_wl.h"

#define WL_UDS_WLANCOMM_ID 0x00545745u      /* "TWE": this application's local-WLAN communications id */
#define WL_UDS_ID8 0x46
#define WL_UDS_CHANNEL 1                    /* the data channel of the frames */
#define WL_UDS_BUILD_BYTES 16
#define WL_UDS_SCAN_HOSTS 8
#define WL_UDS_SHARED_BYTES 0x3000          /* udsInit's shared memory (a multiple of 0x1000) */

enum { WL_UDS_EVENT_JOINED = 1, WL_UDS_EVENT_LEFT = 2 };

typedef struct WlUdsHost {
    udsNetworkStruct network;               /* what WlUds_Join needs */
    char name[32];                          /* the host player's name (UTF-8) */
    uint8_t nodes, maxNodes;                /* how many are in it and how many it takes */
} WlUdsHost;

typedef struct WlUds {
    bool ready;                             /* udsInit succeeded */
    bool active;                            /* hosting or joined, and not lost */
    bool hosting;
    uint8_t build[WL_UDS_BUILD_BYTES];
    udsBindContext bind;
    uint16_t node;                          /* this console's node (1 to 16), 0 when not in a network */
    uint32_t members;                       /* the other nodes in the network, a bit for each */
    struct { uint8_t kind, node; } events[32];
    uint8_t eventHead, eventCount;
    struct { uint16_t src, len; uint8_t data[WL_MAX_PAYLOAD + 16]; } in[32];
    uint8_t inHead, inCount;
    uint8_t buffer[UDS_DATAFRAME_MAXSIZE];
} WlUds;

/* Start UDS (shared memory, the name other consoles see: NULL for the system's, else up to 10 characters). */
bool WlUds_Init(WlUds *uds, const uint8_t build[WL_UDS_BUILD_BYTES], const char *username);
void WlUds_Exit(WlUds *uds);
/* Make a network that takes `maxNodes` consoles (2 to 16, the host counted); this console becomes node 1. */
bool WlUds_Host(WlUds *uds, uint8_t maxNodes);
/* Look for hosts of this build: how many were found (at most `max`, WL_UDS_SCAN_HOSTS suits). Blocks for a second or two. */
int WlUds_Scan(WlUds *uds, WlUdsHost *out, int max);
/* Join a host WlUds_Scan found; this console becomes the node UDS gives it. */
bool WlUds_Join(WlUds *uds, const WlUdsHost *host);
/* Leave (a host closes the network). */
void WlUds_Leave(WlUds *uds);
/* Take the frames that arrived and the changes of who is in the network. Cheap when nothing happened: call it every frame. */
void WlUds_Pump(WlUds *uds);
/* A change of the network's members not yet taken (WL_UDS_EVENT_*); false when none. A lost link is a LEFT for every node and
 * WlUds.active false. */
bool WlUds_PopEvent(WlUds *uds, int *kind, uint16_t *node);
/* The WlBackend over this network, for WlNet_Init. */
WlBackend WlUds_Backend(WlUds *uds);

#endif
