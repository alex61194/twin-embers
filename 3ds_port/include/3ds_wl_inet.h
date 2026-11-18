#ifndef CTR_WL_INET_H
#define CTR_WL_INET_H

/*
 * The internet radio of the local-wireless layer (3ds_wl.h): a WlBackend that carries the frames over a TCP connection
 * to a relay (docs/WL-RELAY-PROTOCOL.md, version 1), so consoles far apart meet in a room. Plain BSD sockets: on the
 * console libctru's soc service must be running (the caller's business); on a computer the same file is tested
 * against tools/wl_relay.py. Nothing here knows the game.
 */
#include <stdbool.h>
#include <stdint.h>

#include "3ds_wl.h"

#define WL_INET_BUILD_BYTES 16
#define WL_INET_ROOM_MAX 16
#define WL_INET_PORT 47311

enum { WL_INET_EVENT_JOINED = 1, WL_INET_EVENT_LEFT = 2 };
enum { WL_INET_REJECT_NONE = 0, WL_INET_REJECT_HELLO = 1, WL_INET_REJECT_VERSION = 2, WL_INET_REJECT_FULL = 3,
       WL_INET_REJECT_BUILD = 4, WL_INET_REJECT_RELAY_FULL = 5 };

typedef struct WlInet {
    intptr_t sock;                  /* -1 when not connected */
    bool welcomed;                  /* the relay accepted this console */
    uint8_t reject;                 /* WL_INET_REJECT_*, 0 when none */
    uint16_t node;                  /* this console's node in the room (1 to 16) */
    uint32_t members;               /* the other nodes in the room, a bit for each */
    uint8_t rx[2064];
    uint16_t rxLen;
    uint8_t tx[4096];
    uint16_t txLen;
    uint32_t lastTx;                /* when something was last sent, for the keep-alive */
    struct { uint8_t kind, node; } events[32];
    uint8_t eventHead, eventCount;
    struct { uint16_t src, len; uint8_t data[WL_MAX_PAYLOAD + 16]; } in[48];
    uint8_t inHead, inCount;
} WlInet;

/* Connect to the relay and join (or make) `room`; `build` must be what every console of the room has. Waits up to
 * `timeoutMs` for the welcome. False on failure: WlInet.reject says why when the relay refused (0 when it could not be
 * reached). */
bool WlInet_Connect(WlInet *net, const char *host, uint16_t port, const char *room, const uint8_t build[WL_INET_BUILD_BYTES],
                    uint32_t timeoutMs);
/* Read what has arrived, send what waits, keep the connection alive. Cheap when idle: call it every frame. */
void WlInet_Pump(WlInet *net);
/* A room event (joined or left) not yet taken; false when none. */
bool WlInet_PopEvent(WlInet *net, int *kind, uint16_t *node);
bool WlInet_Connected(const WlInet *net);
void WlInet_Close(WlInet *net);
/* The WlBackend over this connection, for WlNet_Init. */
WlBackend WlInet_Backend(WlInet *net);

#endif
