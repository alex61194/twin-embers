# Local wireless between two 3DS: research (2026-11-17)

Status: research only. Nothing here is implemented in Twin Embers, nothing was compiled for FireRed and nothing was
tried on a console. The facts about Emerald 3DS come from its public pull request, release notes and source files at
tag `v0.4.0`; the FireRed facts come from this repository's recipes and a bootstrapped workspace.

## Short answer

It can be done, and most of the hard part already exists. Emerald 3DS (ZallaxDev/emerald-3ds, the project this port
inherits its backend from) shipped experimental local wireless in v0.4.0 (PR #76 by eathanpage): two to four 3DS
systems meet in the Union Room, trade, battle, and use the wireless minigames, through an **emulated GBA Wireless
Adapter whose radio is the 3DS's own local wireless (UDS)**. The adapter and the UDS transport are game-independent
C files; only the game-side patch (about 55 KB for Emerald) has to be redone for FireRed.

Recommended: reuse those two files (MIT, with attribution), redo the game patch for FireRed in stages, test with two
Azahar instances first and with two consoles last. Estimated effort: one focused feature, comparable to the Nuzlocke
and visible-wilds work together, and the one with the most risk because it needs two real consoles to be sure.

## What the 3DS offers

- **UDS** (`<3ds.h>`, libctru `uds.h`): a local wireless network with one host and up to 15 other nodes, data frames of
  at most 0x5C6 (1478) bytes on a channel, encrypted by the system. It works only between consoles that are near each
  other: there is no internet in it.
- Players must have wireless turned on. A session is found by its beacon (the host puts a few bytes of application
  data in it) and joined with a passphrase.
- Azahar (and Citra before it) emulates UDS, so two or more emulator instances can link on one computer. Emerald 3DS's
  author tested two to four Azahar instances, with simulated frame loss, and two real consoles (a New 3DS XL and an Old
  3DS).

## How Emerald 3DS did it

| Piece | What it is |
|---|---|
| `3ds_port/src/3ds_local.c` (43 KB) | The transport. A thread of its own makes the UDS calls (searching takes a second or two). Players meet in a *session* per link counter kind; sessions started at once merge. Every pair of systems keeps a numbered, acknowledged, resent (go-back-N) message stream, because the adapter code expects data to arrive in order. A partner silent for 6 s after an unacknowledged message is dropped. The thread sleeps until a frame arrives (polling every 2 ms had dropped a link battle to 30 fps on an Old 3DS). |
| `3ds_port/src/3ds_rfu_adapter.c` (39 KB) | The emulated Wireless Adapter. `librfu` talks to the adapter with commands; here `STWI_start_Command` hands the whole command to the emulator, which answers at once, as the interrupt handler would. Everything above librfu (`AgbRfu_LinkManager.c`, `link_rfu_2.c`, the Union Room) runs unchanged. Link battles also get a direct channel for whole battle messages. |
| `patches/pokeemerald/0055-local-wireless.patch` (55 KB, about 30 files) | The game side: librfu hooks, `cable_club.c`, `link.c`, `trade.c`, `union_room.c`, contest and minigame files, a session script (`local_session.c`) and the counters' scripts (`cable_club.inc`) so that a link counter joins or hosts a session. |
| `scripts/gen_build_id.py` | A SHA-256 of the executable and the game data, put in the session's beacon and passphrase, so only identical builds can see each other (the link code exchanges structures raw). |

What worked in the author's tests: the Union Room, Trade Center trades (by button and by touch), Colosseum link
battles, Berry Blender, Pokémon Jump, Dodrio Berry-Picking, link contests, a Battle Tower link multi battle and a
four-player multi battle, and sessions that survive closing the lid. Known limits: builds of different languages do
not link (the build id covers all text), and in Azahar three players joining at the very same moment get the same
player number. Reviewers noted a few non-blocking issues in the PR (a silent drop in one client-to-client case, a
missing link-lost error in one path).

## Licence

The v0.4.x line of Emerald 3DS is under the MIT licence ("Copyright (c) 2026 Zallax and Emerald 3DS contributors"),
and that grant covers the contributors' code in `3ds_port/`. From v0.5.0 on, upstream relicensed to CC BY-NC-SA 4.0
(non-commercial and share-alike). Twin Embers is MIT for its own code, so **only the files as they are at tag
`v0.4.0` (or earlier) may be taken, with the MIT notice kept** (`licenses/ZallaxDev-MIT.txt` already is in this
repository); anything copied from a newer upstream revision may not. The PR's author is a contributor named in that
copyright line, but it is worth asking Zallax and eathanpage for a word before the code lands, as was done for the
looks.

## What Twin Embers has today

- No serial or wireless hardware: `3ds_link_peripherals.c` answers `MultiBoot` and the GameCube boot with "not
  there", and three recipes make the game skip the adapter (`0006` link peripherals, `0018` the Pokémon Center's
  resume script returns before searching, `0147` no adapter in the soft reset). A link counter therefore ends in the
  game's own communication error.
- The FireRed librfu and wireless files are already compiled and linked: `librfu_*.c`, `AgbRfu_LinkManager.c`,
  `link_rfu_2.c`, `link_rfu_3.c`, `union_room*.c`, `trade.c`, `cable_club.c`, `link.c`, the link battle controllers.
  The overworld code already treats the Union Room as a place with no visible wilds.

## What it would take for FireRed

1. **Take the two native files** (`3ds_local.c`, `3ds_rfu_adapter.c`, their headers) from `v0.4.0`, with the MIT notice,
   and adapt them to this port's GBA shadow memory and frame hook (the adapter is called once a frame from the
   port's VBlank).
2. **Redo the game patch for FireRed** as new recipes: remove the three guards above, hook `STWI_start_Command` in
   FireRed's `librfu_stwi.c` (the library is the same family as Emerald's), and write the counters' scripts for
   FireRed's own Cable Club and Pokémon Center 2F (Trade Center, Colosseum, the Union Room and the Direct Corner;
   FireRed uses the cable for the first two, so they too must be sent through the adapter, as Emerald 3DS did).
3. **Fix the build id for Twin Embers.** Emerald 3DS hashes the executable *and* the game data. A Twin Embers pack
   differs from player to player (HeartGold sprites, looks, HD scenes, new people), so two friends with different
   Builder choices would never find each other. The id has to cover only what changes the link structures: the
   engine and the ROM-derived tables, not the optional pictures. The three languages stay apart.
4. **The lower screen**: trade and battle menus on it, as Emerald 3DS did, in the ember look (a mockup first, as
   always).
5. **Interactions to settle:** Nuzlocke (a traded Pokémon, release rules), Randomlocke (species are random per
   Trainer ID, so two players would see different Pokémon for the same one), the companion Pokémon and the visible
   wilds in the Union Room, LEVEL CAPS in link battles. The simplest rule is that link battles ignore all of them.
6. **Save safety:** a trade changes the party and the save; the new safe-write code (`.new` then rename, `.bak.prev`)
   already protects the file, and the first release of the feature should still say "back up your save".

## Suggested stages

| Stage | Result | How it is checked |
|---|---|---|
| 0 | Permission from Zallax and the PR's author; decide the build id rule | a message |
| 1 | The adapter and transport compile in this port; the Union Room opens and lists a second player | two Azahar instances |
| 2 | Trade and a single battle through the Union Room | Azahar, then host tests of the adapter's command answers |
| 3 | Cable Club counters (Trade Center, Colosseum) and the Direct Corner | Azahar |
| 4 | Two real consoles (New and Old 3DS), battery and frame-rate checks; the minigames | hardware, port.log |
| 5 | Lower-screen menus for trade and battle | mockup, then Azahar and hardware |

Host tests can cover the transport's message stream (loss, order, resend, a silent partner) and the adapter's command
answers without any radio, as `tests/` does for the other modules.

## Risks

- Two real consoles are needed to be sure; Azahar proves the logic, not the radio or the Old 3DS's frame rate.
- Upstream's own feature is called experimental and asks for a save backup.
- Different builds, versions or languages cannot link; the Builder's choices must not change the id.
- FireRed's link code differs from Emerald's in many files, so the patch is a rewrite, not a copy.
- The licence rule above: only the `v0.4.0`-era files, with attribution.

## Internet play

Researched 2026-11-17 (public pages only; nothing here was tried).

**Two different things are meant by "reviving 3DS online", and only one helps this port.**

1. *Nintendo Network replacements (Pretendo).* Nintendo closed the 3DS and Wii U services on 2024-04-08, and Pretendo
   is the fan-run replacement. It is still in beta, covers a growing but partial list of games, needs a console
   with custom firmware, and gives no ETA per game. It serves games that had online play (Mario Kart 7, the Pokémon
   X/Y generation and so on). **FireRed never had online play**, and a homebrew `.3dsx` has no Nintendo Network
   account to log in with, so Pretendo does not give Twin Embers anything by itself.
2. *Our own relay on the internet.* A homebrew program may open ordinary sockets (libctru's `soc` service, up to 64
   at a time) over the console's normal Wi-Fi. That is enough to carry what the local wireless would carry.

**Why (2) is cheap here.** The two layers written for local play are already independent of the radio: `WlNet`
takes a `WlBackend` (send a frame to node N, receive a frame, tell the time), and `WlRfu` only talks to `WlNet`.
The UDS radio is one backend; an internet one is another backend with the same three functions. Nothing above
it changes: the Union Room, trades and battles would see the people of a *room* as if they stood next to them.

**Shape of the project** (a sketch, not built):

| Piece | What it is | Notes |
|---|---|---|
| Internet backend | The console connects out to a relay with TCP (or UDP with `WlNet`'s own resend), says its room code, gets a node id | An outgoing connection needs no port forwarding, which removes the usual NAT problem |
| Relay server | A small program (a few hundred lines) that keeps rooms of up to 16 nodes and forwards each frame to its destination | The same model as Azahar's multiplayer rooms and as relay-style projects for other consoles (for example ryu_ldn_nx on the Switch); anyone can host one, which is what "community servers" would be |
| Rooms | A short code (or a list) instead of "who is near me"; the room's players are the session | No accounts needed, so nothing personal is stored |
| Same build | The same rule as local play: same engine, tables and language | The server can refuse mismatches by the build id in the room's first message |

**What would be hard or limiting.**

- *Latency.* The link layer exchanges a frame about every 17 ms. Trades, the Union Room and turn-based battles
  stay usable at 100-150 ms round trips (each turn waits for the other side anyway); the real-time minigames
  (Pokémon Jump, Dodrio Berry-Picking) would not. A room could simply not offer them over the internet.
- *Trust and abuse.* Trade data crosses a server somebody runs. Without accounts there is no moderation: a room
  code is the only access control, so rooms should be private by default, and the protocol should say what the
  server can see (everything, unless the frames are encrypted between consoles, which a shared room secret could do).
- *TLS.* The console's TLS support is limited and old; a plain TCP protocol with its own checks is simpler than HTTPS.
  A relay for real people should still encrypt the frames end to end with the room's secret.
- *Cost and upkeep.* The traffic is tiny (a few kilobytes a second per player), so a cheap server holds many rooms,
  but somebody has to run it and keep the protocol versioned.
- *Fairness.* A trade over the internet can be interrupted midway by a dropped link; the game's own trade
  protocol has the same weakness on a cable, but the internet makes it likelier, so the first release should say so.

**Emulator players get online play almost free.** Azahar already lets people join each other's rooms over the
internet and carries UDS across them; once the UDS backend works, Twin Embers running in Azahar can link across the
internet with no code of ours. Only real consoles need the relay.

**Suggested order.** Finish local play (UDS) first, test it in Azahar, then add the internet backend and a reference
relay; the relay's wire format is worth writing down early so other people can host compatible ones.

## Status of the internet backend (2026-11-18)

The wire format is written down in [`WL-RELAY-PROTOCOL.md`](WL-RELAY-PROTOCOL.md). Three pieces exist and are tested on a computer only:

- `tools/wl_relay.py`, a reference relay (rooms of up to 16 nodes, a build id per room, joins and leaves, keep-alive; standard library only);
- `3ds_port/src/3ds_wl_inet.c`, the console side as a `WlBackend` over a TCP socket;
- `tools/test_wl_inet.py` with `3ds_port/tests/wl_inet_test.c`: the client against the relay (node numbers, build check, joins and leaves, a full room, and `WlNet`'s reliable stream across two real connections), plus byte-level checks of the relay.

Frames are sealed between consoles with ChaCha20-Poly1305 (`3ds_port/src/3ds_wl_crypto.c`, from RFC 8439, SHA-256 and PBKDF2 for the key from the room's secret code; checked against the RFC's test vector, Python's hashlib and an independent Python implementation, and a Python console that talks to the C one and is refused when it changes, moves or forges a frame). Not done: the UDS backend, the game side (librfu hook and the Cable Club and Pokémon Center scripts) and any menu to enter a room code. Nothing of it is linked into the game or has run on a console.

## Sources

- Emerald 3DS pull request #76, "Local wireless play between 3DS systems":
  https://github.com/ZallaxDev/emerald-3ds/pull/76
- Emerald 3DS release v0.4.0: https://github.com/ZallaxDev/emerald-3ds/releases/tag/v0.4.0
- libctru UDS reference: https://libctru.devkitpro.org/uds_8h.html
- 3dbrew, NWMUDS SendTo (frame size limit): https://3dbrew.org/wiki/NWMUDS:SendTo
- Pretendo Network (Nintendo Network replacement): https://en.wikipedia.org/wiki/Pretendo_Network
- libctru sockets (`soc`): https://libctru.devkitpro.org/soc_8h.html
- Azahar multiplayer rooms: https://citra.azahar-emu.org/help/feature/multiplayer/
