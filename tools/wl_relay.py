#!/usr/bin/env python3
"""Reference relay for Twin Embers internet play (docs/WL-RELAY-PROTOCOL.md, version 1).

    python3 tools/wl_relay.py [--host 0.0.0.0] [--port 47311] [--max-rooms 256] [--max-connections 1024]

It keeps rooms of up to 16 nodes and forwards each DATA message to its destination. It never looks inside a frame and
stores nothing about people. Standard library only.
"""
import argparse
import asyncio
import re
import struct
import sys
import time

VERSION = 1
HELLO, WELCOME, REJECT, JOINED, LEFT, DATA, PING, PONG = 1, 2, 3, 4, 5, 6, 7, 8
BAD_HELLO, BAD_VERSION, ROOM_FULL, BUILD_DIFFERS, RELAY_FULL = 1, 2, 3, 4, 5
MAX_NODES = 16
MAX_MESSAGE = 2048
MAX_DATA = 1500
HELLO_TIMEOUT = 10.0
IDLE_TIMEOUT = 60.0
ROOM_RE = re.compile(rb'^[A-Za-z0-9_-]{1,16}$')


def frame(kind, body=b''):
    payload = bytes([kind]) + body
    return struct.pack('<H', len(payload)) + payload


class Room:
    def __init__(self, code, build):
        self.code = code
        self.build = build
        self.nodes = {}            # node number -> Client

    def free_node(self):
        for n in range(1, MAX_NODES + 1):
            if n not in self.nodes:
                return n
        return 0

    def mask_without(self, node):
        mask = 0
        for n in self.nodes:
            if n != node:
                mask |= 1 << n
        return mask


class Relay:
    def __init__(self, max_rooms, max_connections):
        self.rooms = {}
        self.connections = 0
        self.max_rooms = max_rooms
        self.max_connections = max_connections

    async def handle(self, reader, writer):
        if self.connections >= self.max_connections:
            writer.write(frame(REJECT, bytes([RELAY_FULL])))
            await self.close(writer)
            return
        self.connections += 1
        room = None
        node = 0
        try:
            hello = await asyncio.wait_for(self.read_message(reader), HELLO_TIMEOUT)
            if hello is None or hello[0] != HELLO or len(hello) < 19:
                await self.reject(writer, BAD_HELLO)
                return
            version, build, rlen = hello[1], hello[2:18], hello[18]
            code = hello[19:19 + rlen]
            if version != VERSION:
                await self.reject(writer, BAD_VERSION)
                return
            if len(code) != rlen or not ROOM_RE.match(code):
                await self.reject(writer, BAD_HELLO)
                return
            room = self.rooms.get(code)
            if room is None:
                if len(self.rooms) >= self.max_rooms:
                    await self.reject(writer, RELAY_FULL)
                    room = None
                    return
                room = Room(code, bytes(build))
                self.rooms[code] = room
            elif room.build != bytes(build):
                await self.reject(writer, BUILD_DIFFERS)
                room = None
                return
            node = room.free_node()
            if node == 0:
                await self.reject(writer, ROOM_FULL)
                room = None
                return
            writer.write(frame(WELCOME, struct.pack('<BI', node, room.mask_without(node))))
            others = list(room.nodes.values())
            room.nodes[node] = writer
            for other in others:
                self.send(other, frame(JOINED, bytes([node])))
            while True:
                message = await asyncio.wait_for(self.read_message(reader), IDLE_TIMEOUT)
                if message is None:
                    break
                kind = message[0]
                if kind == PING:
                    self.send(writer, frame(PONG))
                elif kind == DATA and 2 <= len(message) <= 2 + MAX_DATA:
                    target = room.nodes.get(message[1])
                    if target is not None and message[1] != node:
                        self.send(target, frame(DATA, bytes([node]) + message[2:]))
        except (asyncio.TimeoutError, asyncio.IncompleteReadError, ConnectionError, OSError):
            pass
        finally:
            self.connections -= 1
            if room is not None and node and room.nodes.get(node) is writer:
                del room.nodes[node]
                for other in list(room.nodes.values()):
                    self.send(other, frame(LEFT, bytes([node])))
                if not room.nodes and self.rooms.get(room.code) is room:
                    del self.rooms[room.code]
            await self.close(writer)

    @staticmethod
    async def read_message(reader):
        header = await reader.readexactly(2)
        (length,) = struct.unpack('<H', header)
        if length < 1 or length > MAX_MESSAGE:
            return None
        return await reader.readexactly(length)

    @staticmethod
    def send(writer, data):
        if writer.is_closing():
            return
        try:
            writer.write(data)
        except (ConnectionError, OSError):
            pass

    async def reject(self, writer, reason):
        writer.write(frame(REJECT, bytes([reason])))
        try:
            await writer.drain()
        except (ConnectionError, OSError):
            pass

    @staticmethod
    async def close(writer):
        try:
            writer.close()
            await writer.wait_closed()
        except (ConnectionError, OSError):
            pass


async def serve(args):
    relay = Relay(args.max_rooms, args.max_connections)
    server = await asyncio.start_server(relay.handle, args.host, args.port)
    port = server.sockets[0].getsockname()[1]
    print('LISTEN %d' % port, flush=True)
    async with server:
        await server.serve_forever()


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--host', default='0.0.0.0')
    ap.add_argument('--port', type=int, default=47311, help='0 picks a free port (printed as LISTEN <port>)')
    ap.add_argument('--max-rooms', type=int, default=256)
    ap.add_argument('--max-connections', type=int, default=1024)
    args = ap.parse_args()
    try:
        asyncio.run(serve(args))
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == '__main__':
    sys.exit(main())
