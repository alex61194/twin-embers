# Running a Twin Embers relay

Online play needs one relay that every console can reach: `tools/wl_relay.py`, a small Python 3 program with no
dependencies (protocol: [WL-RELAY-PROTOCOL.md](WL-RELAY-PROTOCOL.md)). It forwards sealed frames between the consoles of a room
and never sees a room code, only a hash of it. A build made with `TWIN_RELAY=host:port` connects to that relay by default
and puts everyone in its public room; the MULTIPLAYER option (OPTIONS, Settings page) switches between Online and Local.

## What it costs to run

Each player sends about 60 small frames a second while linked: roughly 4 KB/s, 15 MB an hour, and the relay sends the same
amount on to the others. A machine with one small CPU and 256 MB of memory for the relay is plenty for hundreds of players;
the traffic is what decides the plan.

## A free machine that stays on: Oracle Cloud "Always Free"

Oracle's always-free tier includes small virtual machines with a public address and 10 TB of outgoing traffic a month, which
is far more than a relay uses. Signing up asks for a payment card to check the identity; the always-free resources are not
charged. Oracle may reclaim an always-free machine that stays almost idle for a long time; upgrading the account to
pay-as-you-go (still free while only always-free resources are used) avoids that.

1. Create the account at <https://www.oracle.com/cloud/free/> and pick a home region near the players.
2. Create an instance (Compute > Instances > Create): image **Ubuntu**, shape **VM.Standard.E2.1.Micro** (AMD) or
   **VM.Standard.A1.Flex** (Arm, 1 OCPU and 6 GB are enough), both marked "Always Free eligible". Keep "Assign a public IPv4
   address" on and save the SSH key it offers.
3. Open the relay's port in the cloud's network rules: the instance's subnet > Security List > Add Ingress Rule:
   source `0.0.0.0/0`, protocol TCP, destination port `47311`.
4. Connect with SSH (`ssh -i the-key ubuntu@PUBLIC-ADDRESS`), copy `tools/wl_relay.py` and `tools/relay-server/install.py`
   to it (for instance with `scp`, or `git clone` the repository), and run:

   ```sh
   sudo python3 install.py 47311
   ```

   The script installs the relay as the service `twinembers-relay` (it restarts by itself and starts with the machine), opens
   the port in the machine's own firewall (Oracle's Ubuntu images refuse new ports until told), and shows its status.
5. Check it from another computer: `python3 -c "import socket; socket.create_connection(('PUBLIC-ADDRESS', 47311), 5); print('open')"`.

A fixed name is easier to keep than an address: a free dynamic-DNS name (DuckDNS, for instance) pointed at the machine's address
can go into the build instead.

## Pointing the game at it

- For everyone: build with `make ... TWIN_RELAY=PUBLIC-ADDRESS:47311`. Online is then the default and nothing has to be set up
  on the console.
- For one console, or another relay: `sdmc:/3ds/twinembers/online.cfg` with `relay=host:port` and, for a private room,
  `code=a-shared-code` (without `code=` the console joins the relay's public room).

## Looking after it

- `journalctl -u twinembers-relay -f` shows a line per console joining or leaving (rooms by their hashed name, no addresses).
- `sudo python3 install.py 47311` again installs a newer `wl_relay.py`.
- Limits (arguments in the service file): 256 rooms, 1024 connections, 8 connections per address, 16 consoles per room; a
  console that stops reading is dropped before its queue grows past 256 KB, and one that sends nothing for 60 seconds is closed.
- The public room's code is known to everyone, so its frames are sealed but not secret. A private room (OPTIONS, Online room, PRIVATE: a 16-digit code made on the console, or `code=` in online.cfg) is sealed with its code; the relay only sees a hash of its name.
- Before a release: build the 3DSX with `TWIN_RELAY=PUBLIC-ADDRESS:47311` (add `REQUIRE_RELAY=1` and the build stops when it is missing), check the port from another computer (step 5 above) and from a console that is not on your network, and keep the address in the release notes.
