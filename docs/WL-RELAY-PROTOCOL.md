# Twin Embers relay protocol, version 1

Status: written 2026-11-18 together with the reference relay (`tools/wl_relay.py`) and the console's client
(`3ds_port/src/3ds_wl_inet.c`, sealing in `3ds_wl_crypto.c`). Tried on a computer only (`tools/test_wl_inet.py`,
`tools/test_wl_crypto.py`); nothing here has run on a console or
inside the game yet. It carries the frames of the local-wireless layer (`WlNet`, see `3ds_port/include/3ds_wl.h`)
over the internet, so people far apart can meet in a *room* as if they stood side by side. Anyone may run a relay:
this file is all a compatible one needs.

## Model

- A **room** is named by a short name (1 to 16 characters from `A-Z a-z 0-9 - _`). The first console that asks for a name
  creates the room; the room is gone when its last console leaves. A console does not choose the name: it is made from the
  players' secret code (see *Sealing*), so the relay never sees the code.
- A room holds up to **16 nodes**, numbered 1 to 16 as `WlNet` expects (the lowest free number is given).
- The relay **forwards** frames to the node they are addressed to and tells the others when somebody joins or leaves.
  It never reads, repairs or reorders what a frame carries: reliability is `WlNet`'s job (it numbers, acknowledges and
  resends), so a relay may be as simple as it likes.
- A room has a **build id** (16 bytes) that the first console sets and every later one must repeat. It stops two
  different builds, versions or languages from linking (the link code sends raw structures). The relay only compares
  it.
- There are no accounts and the relay stores nothing about people. The secret code is the only access control, so it
  should be long enough to guess badly and shared only with friends.
- The relay forwards **sealed frames**: it sees who talks to whom, when, and how much, but not what is said, and it
  cannot change a frame without the receiver noticing (see *Sealing*).

## Transport

TCP, no TLS (the console's TLS is limited and old). The console connects out, so no port forwarding is needed. The
default port is **47311**. Everything is little-endian.

A message on the stream is a 16-bit length `L` (1 to 2048) followed by `L` bytes; the first of them is the type.

| Type | Name | From | Body |
|---|---|---|---|
| 0x01 | HELLO | console | version `u8` (1), build id `u8[16]`, room length `u8` (1 to 16), room code bytes |
| 0x02 | WELCOME | relay | node `u8` (1 to 16), members `u32` (bit n set: node n is in the room, not this one) |
| 0x03 | REJECT | relay | reason `u8`; the relay then closes the connection |
| 0x04 | JOINED | relay | node `u8` |
| 0x05 | LEFT | relay | node `u8` |
| 0x06 | DATA | both | console to relay: destination node `u8`, bytes. Relay to console: source node `u8`, bytes |
| 0x07 | PING | console | (empty) |
| 0x08 | PONG | relay | (empty) |

REJECT reasons: 1 malformed HELLO or room code, 2 version not supported, 3 room full, 4 build id differs from the
room's, 5 the relay is full.

DATA carries 1 to 1500 bytes; `WlNet` sends at most 160 (`WL_MAX_PAYLOAD`) plus its 8-byte header. A frame for a node
that is not in the room is dropped without a word (a radio frame to nobody is lost the same way). A console never
receives its own frames.

## Sealing

Everything a console sends in a DATA message is sealed between consoles with ChaCha20-Poly1305 (RFC 8439). The relay holds
no key. `WlInet_Derive` (`3ds_port/src/3ds_wl_inet.c`) makes both the room name and the key from the players' secret code:

- `key = PBKDF2-HMAC-SHA-256(code, "TwinEmbers relay v1" + build id, 4096 iterations, 32 bytes)` (a code is 1 to 64 bytes);
- `room name = first 16 hex characters of SHA-256("TwinEmbers room" + key)`: a hash of the key, so the name tells the relay
  nothing it can use to read the room, and it is the name sent in HELLO.

The build id is in the key, so two different builds, versions or languages derive different rooms and never meet by mistake.

A sealed DATA body (after the destination node, or the source node from the relay) is `nonce[12] ciphertext tag[16]`:

- the nonce is a random 8-byte session value chosen by the console for each connection, then a 32-bit little-endian count
  of frames sealed so far (a console stops sending before the count wraps);
- the associated data is two bytes, the sender's node and the destination node, so the relay cannot move a frame to
  another node or give it another sender: a frame that does not open is dropped and counted (`WlInet.dropped`).

What this does not do: a relay can still drop, delay or repeat frames (`WlNet` numbers, acknowledges and resends, so
repeats and gaps are survived, and a silent relay ends the session); anyone who learns the code can join and read the
room; a short or common code can be guessed offline by whoever sees a room name, at the cost of the PBKDF2 rounds for each
guess (about a second on a console, far less on a computer), so use a long random code. The same code with a different
build is simply another room.

## Rules

- The first message on a connection must be a HELLO within 10 seconds, or the relay closes it.
- A console that has sent nothing for 15 seconds sends a PING; the relay answers with a PONG. A relay closes a
  connection that has sent nothing for 60 seconds.
- When a connection closes the relay sends LEFT for its node to the others and frees the number. A console that
  reconnects with the same room code gets a new node (and `WlNet` of the others forgets the old one on LEFT).
- A relay should keep the number of rooms and connections bounded and may refuse more with reason 5.
- Unknown types are ignored; a different version in HELLO is refused with reason 2.

## What a console does with it

`WlInet` (`3ds_port/src/3ds_wl_inet.c`) is a `WlBackend`: its `send` seals a frame and queues a DATA message to a node, its
`recv` hands back the DATA messages that arrived and opened, `now` is a millisecond clock. `WlNet` sits on top unchanged, and the
adapter emulation (`WlRfu`) on top of that, so the Union Room sees the room's players as people standing nearby.
The set of nodes comes from WELCOME, JOINED and LEFT (`WlInet_Members`, `WlInet_PopEvent`).

## Latency

The link layer exchanges a frame about every 17 ms. Trades, the Union Room and turn-based battles stay usable at
100 to 150 ms round trips; the real-time minigames do not and should not be offered over the internet.

## Using it

Run a relay anywhere with Python 3 (it needs nothing else): `python3 tools/wl_relay.py --host 0.0.0.0 --port 4700`.
It prints `LISTEN <port>` once it is ready. It sees only sealed frames and a room name derived from the code, never the code itself.

Each console reads `sdmc:/3ds/twinembers/online.cfg`, two lines:

```
relay=example.org:4700
code=a-room-code-both-players-know
```

Both players use the same relay, the same code and the same build of the game (the build is part of the key, so different builds never
meet). Without that file the console uses its own radio (UDS) to find people nearby.

Limits: a room holds at most 16 nodes; a relay refuses more rooms or connections than it was started with; the code is 1 to 64
characters. The sealing is a construction written for this project (PBKDF2-HMAC-SHA256, ChaCha20-Poly1305) from the RFCs and checked
against their test vectors, but it has not been reviewed by a cryptographer: treat a room code like a shared password for a game, not for
anything that matters.

`tools/test_wl_e2e.py` runs two consoles' worth of FireRed's own wireless library through a relay on a computer (it needs a workspace made
by `tools/bootstrap.py`).
