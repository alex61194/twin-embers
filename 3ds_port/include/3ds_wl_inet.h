#ifndef CTR_WL_INET_H
#define CTR_WL_INET_H

/*
 * The internet radio of the local-wireless layer (3ds_wl.h): a WlBackend that carries the frames over a TCP connection
 * to a relay (docs/WL-RELAY-PROTOCOL.md, version 1), so consoles far apart meet in a room. Plain BSD sockets: on the
 * console libctru's soc service must be running (the caller's business); on a computer the same file is tested
 * against tools/wl_relay.py. Nothing here knows the game.
 *
 * Every frame is sealed between the consoles (ChaCha20-Poly1305, 3ds_wl_crypto.h) with a key made from the room's secret
 * code, so the relay carries what it cannot read or change. The relay is never given the code: it sees a room name
 * that is a hash of the key. Anyone who knows the code can join, so a code should be long and shared only with friends.
 */
#include <stdbool.h>
#include <stdint.h>

#include "3ds_wl.h"
#include "3ds_wl_crypto.h"

#define WL_INET_BUILD_BYTES 16
#define WL_INET_ROOM_MAX 16
#define WL_INET_PORT 47311
#define WL_INET_CODE_MAX 64
#define WL_INET_SEAL_BYTES 28       /* what sealing adds to a frame: 12 for the nonce, 16 for the tag */
#define WL_INET_ITERATIONS 4096     /* of PBKDF2 for the key: about a second on the console, once per connection */

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
    uint8_t key[32];                /* the room's key */
    uint8_t session[8];             /* this connection's random half of every nonce */
    uint32_t counter;               /* the other half: frames sealed so far */
    uint32_t dropped;               /* frames that came and did not open (changed, or sealed with another key) */
    struct { uint8_t kind, node; } events[32];
    uint8_t eventHead, eventCount;
    struct { uint16_t src, len; uint8_t data[WL_MAX_PAYLOAD + 16]; } in[48];   /* opened frames */
    uint8_t inHead, inCount;
} WlInet;

/* Fresh randomness for the nonces of the connections that follow: the caller gives what the system has (on the console
 * PS_GenerateRandomBytes); time and addresses are mixed in too, but are not enough alone. Call it before WlInet_Connect. */
void WlInet_Seed(const void *entropy, size_t len);
/* The room name and key a code makes with a build id (the same on every console): `room` gets 16 characters and a
 * terminator. Takes WL_INET_ITERATIONS rounds of PBKDF2, so the time of a connection. */
void WlInet_Derive(const char *code, const uint8_t build[WL_INET_BUILD_BYTES], char room[WL_INET_ROOM_MAX + 1], uint8_t key[32]);
/* Connect to the relay and join (or make) the room of `code` (1 to WL_INET_CODE_MAX characters); `build` must be what every
 * console of the room has. Waits up to `timeoutMs` for the welcome. False on failure: WlInet.reject says why when the relay
 * refused (0 when it could not be reached). */
bool WlInet_Connect(WlInet *net, const char *host, uint16_t port, const char *code, const uint8_t build[WL_INET_BUILD_BYTES],
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
