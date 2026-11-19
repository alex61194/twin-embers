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
/* PBKDF2 rounds for a room's key, once per connection. The public room's key hides nothing (its code is in the open), so it keeps the quick 4096 (about
 * a second on the console). A private room's code is the only secret there is: its key takes 10000 rounds (a few seconds on the console, the time of
 * joining), which on top of a code that cannot be guessed (WL_INET_PRIVATE_DIGITS random digits) puts the key beyond an offline search. */
#define WL_INET_ITERATIONS 4096
#define WL_INET_ITERATIONS_PRIVATE 10000
/* A private room's code starts with this (the glue makes it from the digits the players share, CTR_WL_PRIVATE_ROOM) and has that many digits after it. */
#define WL_INET_PRIVATE_PREFIX "twinembers-private-"
#define WL_INET_PRIVATE_DIGITS 16

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
    uint32_t sentFrames, receivedFrames, lostFrames;   /* statistics: frames sent, frames opened, frames that found the queue full */
    struct { uint8_t kind, node; } events[32];
    uint8_t eventHead, eventCount;
    struct { uint16_t src, len; uint8_t data[WL_MAX_PAYLOAD + 16]; } in[48];   /* opened frames */
    uint8_t inHead, inCount;
    int trail[12][2], trailLen;    /* the steps of the last connection (code, value): 10 key made, 11 lookup, 12 socket, 13 connect error, 14 select,
                                       15 socket error, 16 hello sent (bytes left * 1000 + closed), 18 closed (error) */
    int failStep, failErrno;        /* why the last WlInet_Connect failed: 1 name lookup, 2 connect, 3 wait for the connection, 4 refused,
                                       5 no welcome in time, 6 the relay refused (failErrno: its reason), 7 closed before the welcome */
} WlInet;

/* Fresh randomness for the nonces of the connections that follow: the caller gives what the system has (on the console
 * PS_GenerateRandomBytes); time and addresses are mixed in too, but are not enough alone. Call it before WlInet_Connect. */
void WlInet_Seed(const void *entropy, size_t len);
/* Whether `code` is a private room's (it starts with WL_INET_PRIVATE_PREFIX), and the PBKDF2 rounds its key takes. */
bool WlInet_IsPrivateCode(const char *code);
uint32_t WlInet_Iterations(const char *code);
/* The room name and key a code makes with a build id (the same on every console): `room` gets 16 characters and a terminator. One PBKDF2 run
 * (WlInet_Iterations rounds, so the time of a connection) makes 64 bytes: the first 32 are the key, the others only the room's name (through a
 * hash), so that the name the relay sees is not made from the key that seals the frames. */
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
