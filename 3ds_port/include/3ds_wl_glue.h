#ifndef CTR_WL_GLUE_H
#define CTR_WL_GLUE_H

/*
 * The meeting point of the game and the radio: FireRed's librfu (patched, recipe 0272) hands every command it would have sent to
 * the Wireless Adapter to CtrRfu_Command, which the emulated adapter (WlRfu, 3ds_wl.h) answers at once; what the other consoles
 * say travels over WlNet on the 3DS's local wireless (WlUds, 3ds_wl_uds.h). This file owns the one adapter and the one radio and
 * decides when the radio is used:
 *
 *   - CtrWl_Available: the radio can be used (UDS started: the console's wireless switch is on). Asked when the game looks
 *     for the adapter (a link counter, the Union Room's attendant). It starts UDS the first time and tries again every few seconds
 *     while it fails.
 *   - With the internet radio the session connects to the relay (and again every few seconds if it cannot, or if the connection drops).
 *   - A session starts when the game starts to search or to host (the adapter's SC/SP/CP start commands): the console looks for a
 *     host of its build (a second or two, on a thread of its own on the console); if there is one it joins, else it hosts. A host
 *     alone keeps looking, and when another host shows up with a smaller token the two merge (the one with the larger token joins
 *     the other), so two consoles that started together still end up in one network.
 *   - The session ends when the game stops the adapter (stop mode) and when the link is lost, which the game sees as the adapter
 *     losing its slots.
 *
 * CtrWl_Tick runs once a frame (VBlankIntrWait in 3ds_game_bridge.c). Nothing here is the game's type.
 */
#include <stdbool.h>
#include <stdint.h>

#include "3ds_wl.h"

#define CTR_WL_NODES_MAX 5              /* the host and four others: what the adapter's slots allow */
#define CTR_WL_RESCAN_MS 4000           /* a host that is alone looks again this often */
#define CTR_WL_RETRY_MS 5000            /* UDS that failed to start is tried again this often */

#define CTR_WL_RECONNECT_MS 3000        /* the internet radio tries again this often while the relay is out of reach */

enum { CTR_WL_OFF = 0, CTR_WL_SEARCH, CTR_WL_HOST, CTR_WL_JOINED, CTR_WL_ONLINE };

/* The build id every console of a session shares (the engine and the tables, not the optional pictures), and a seed of randomness for
 * the tokens and the adapter id. Call once at start-up. */
void CtrWl_Setup(const uint8_t build[16], uint32_t seed);
/* The internet radio instead of the local one: the relay (docs/WL-RELAY-PROTOCOL.md) and the secret code of the room. A session then connects
 * to the relay instead of looking for a host nearby, and everyone who uses the same code, relay and build is in one room. NULL or an empty
 * string for either turns it off. CtrWl_LoadConfig reads `relay=host[:port]` and `code=...` lines from a text file (false when there is none).
 * The caller starts the console's sockets (socInit) before a session. */
void CtrWl_SetInternet(const char *host, uint16_t port, const char *code);
bool CtrWl_LoadConfig(const char *path);
bool CtrWl_IsInternet(void);
/* The player's MULTIPLAYER choice: local play uses the console's own radio even when a relay is known. Like the relay itself, it counts
 * from the next session on (a link that is up keeps its radio). CtrWl_HasRelay: a relay and a room code are known. */
void CtrWl_SetLocal(bool local);
bool CtrWl_HasRelay(void);
/* The room everyone meets in when a relay is given without a code. */
#define CTR_WL_PUBLIC_ROOM "twinembers-public"
/* A private room: the room of a six-digit code everyone in it types (the OPTIONS keypad), instead of the relay's public room or online.cfg's
 * code; NULL or "" for none. It counts from the next session on, like the rest. */
#define CTR_WL_PRIVATE_ROOM "twinembers-private-"
void CtrWl_SetPrivateRoom(const char *digits);
/* The relay a build connects to by default, "host:port" (make TWIN_RELAY=...); empty: none, online play needs online.cfg. */
#ifndef CTR_WL_DEFAULT_RELAY
#define CTR_WL_DEFAULT_RELAY ""
#endif
/* Whether the radio can be used now (starts UDS if it is not started; false while it will not start). */
uint8_t CtrWl_Available(void);
/* One frame: radio, adapter, session. Cheap when nothing is going on. */
void CtrWl_Tick(void);
/* The session's state (CTR_WL_*) and how many other consoles are in it. */
int CtrWl_State(void);
int CtrWl_Others(void);
/* Leave the session and stop UDS (the game is closing, or the player turned the radio off). */
void CtrWl_Shutdown(void);

/* What the patched librfu calls (see 3ds_wl.h WlRfu_Command / WlRfu_PollSlave for the words). */
int CtrRfu_Command(const uint32_t *request, uint32_t *answer);
int CtrRfu_PollSlave(uint32_t *request);
void CtrRfu_Reset(void);
/* The game waits in a loop for the adapter's request (it came by interrupt on the GBA): give the radio and the adapter a turn. */
void CtrRfu_Wait(void);
/* One turn of the radio and the adapter (and the adapter's request to the game when it has one), for the time between frames; nothing
 * when no session is on. */
void CtrWl_Idle(void);

#endif
