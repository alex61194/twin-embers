"""Release gate: the finished 3DSX must carry the relay the release is meant to use, or the owner must have said it carries none.

Called by `make verify-clean-release` after the build. Two things go wrong silently otherwise:
  - TWIN_RELAY was forgotten: the console then has no relay and plays Online with nobody to reach (an earlier test build did exactly that);
  - TWIN_RELAY was given but an object compiled earlier without it was reused (the Makefile does not rebuild an object when the flag changes).

    check_release_relay.py FILE.3dsx --relay host:port     the file must contain exactly that "host:port"
    check_release_relay.py FILE.3dsx --allow-none           the owner chose a build with no relay (ALLOW_NO_RELAY=1 in make)
    check_release_relay.py FILE.3dsx                        refused: no relay and no decision to ship without one

The address is only compared with the file, never printed in a message that is written anywhere else.
"""
import argparse
import re
import sys
from pathlib import Path

ADDRESS = re.compile(r'^[A-Za-z0-9.-]{1,253}:[0-9]{1,5}$')


def check(data, relay, allow_none):
    """Returns (ok, message)."""
    if relay:
        if not ADDRESS.match(relay) or not 0 < int(relay.rsplit(':', 1)[1]) < 65536:
            return False, 'TWIN_RELAY must be host:port'
        if relay.encode() not in data:
            return False, ('the 3DSX does not contain the relay it was told to carry: an object built earlier without TWIN_RELAY was reused. '
                           'Delete build/main_game_full.o (or the whole build folder) and build again')
        return True, 'the 3DSX carries the relay of this release'
    if allow_none:
        return True, 'no relay in this build, as the owner chose (ALLOW_NO_RELAY): Online needs sdmc:/3ds/twinembers/online.cfg'
    return False, 'no relay for this release: pass TWIN_RELAY=host:port, or ALLOW_NO_RELAY=1 to ship a build without one on purpose'


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('image')
    ap.add_argument('--relay', default='')
    ap.add_argument('--allow-none', action='store_true')
    args = ap.parse_args()
    ok, message = check(Path(args.image).read_bytes(), args.relay, args.allow_none)
    print(('OK: ' if ok else 'RELEASE BLOCKED: ') + message)
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
