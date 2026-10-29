"""Original artwork for the HOME Menu forwarder, drawn here: no game or third-party art.

Writes icon.png (48x48), banner.png (256x128, the HOME Menu's upper-screen banner) and
banner.wav (a short synthesized chime) into the given folder. Two flames, the "twin
embers", on a dark red ground; the banner adds the name in a 5x7 pixel font drawn
below. Pure Python (zlib, struct, math): nothing to install.
"""
import math
import struct
import sys
import zlib
from pathlib import Path

# A 5x7 pixel font, only the letters of the name.
FONT = {
    'T': ['#####', '..#..', '..#..', '..#..', '..#..', '..#..', '..#..'],
    'W': ['#...#', '#...#', '#...#', '#.#.#', '#.#.#', '##.##', '#...#'],
    'I': ['#####', '..#..', '..#..', '..#..', '..#..', '..#..', '#####'],
    'N': ['#...#', '##..#', '#.#.#', '#.#.#', '#..##', '#...#', '#...#'],
    'E': ['#####', '#....', '#....', '####.', '#....', '#....', '#####'],
    'M': ['#...#', '##.##', '#.#.#', '#.#.#', '#...#', '#...#', '#...#'],
    'B': ['####.', '#...#', '#...#', '####.', '#...#', '#...#', '####.'],
    'R': ['####.', '#...#', '#...#', '####.', '#.#..', '#..#.', '#...#'],
    'S': ['.####', '#....', '#....', '.###.', '....#', '....#', '####.'],
    ' ': ['.....'] * 7,
}


def mix(a, b, t):
    t = max(0.0, min(1.0, t))
    return tuple(a[i] + (b[i] - a[i]) * t for i in range(len(a)))


def flame(u, t, sway):
    """How far inside a flame (0 outside .. 1 at its heart). u: -1..1 across, t: 0 tip .. 1 base."""
    if not 0.0 <= t <= 1.0:
        return 0.0
    u -= sway * (1.0 - t) ** 2            # the tip leans
    bulb, radius = 0.66, 0.34
    if t >= bulb:                         # the round base
        d = math.hypot(u, (t - bulb) / radius)
    else:                                 # the curved sides up to the tip
        width = (t / bulb) ** 1.4
        d = abs(u) / max(width, 1e-6)
    return max(0.0, 1.0 - d)


def flames(x, y, box):
    """Colour and coverage of the two flames at pixel (x, y); box: their square (left, top, size)."""
    best = None
    left, top, size = box
    for cx, base, height, half, sway in ((0.40, 0.92, 0.80, 0.21, 0.35), (0.62, 0.90, 0.62, 0.16, -0.30)):
        t = ((y - top) / size - (base - height)) / height
        u = ((x - left) / size - cx) / half
        heat = flame(u, t, sway)
        if heat > 0 and (best is None or heat > best):
            best = heat
    if best is None:
        return None
    outer, middle, core = (214, 40, 18), (255, 140, 30), (255, 244, 190)
    colour = mix(outer, middle, best / 0.45) if best < 0.45 else mix(middle, core, (best - 0.45) / 0.55)
    return colour, min(1.0, best * 6.0)


def ground(x, y, w, h):
    """Dark red, lit from the bottom centre, with a few small sparks."""
    d = math.hypot((x - w / 2) / h * 0.9, y / h - 0.95)
    colour = mix((120, 18, 12), (28, 4, 6), d / 0.95)
    for i in range(14):
        sx = (math.sin(i * 12.9898) * 43758.5453) % 1.0 * w
        sy = (math.sin(i * 78.233) * 12543.123) % 1.0 * 0.75 * h
        r = math.hypot(x - sx, y - sy) / h
        if r < 0.010 + 0.004 * (i % 3):
            colour = mix(colour, (255, 170, 60), 0.75)
    return colour


def picture(w, h, rounded, box):
    pixels = []
    for y in range(h):
        row = []
        for x in range(w):
            colour = ground(x + 0.5, y + 0.5, w, h)
            lit = flames(x + 0.5, y + 0.5, box)
            if lit:
                colour = mix(colour, lit[0], lit[1])
            alpha = 255
            if rounded:
                r = 7.0
                dx = max(r - x - 0.5, 0, x + 0.5 - (w - r))
                dy = max(r - y - 0.5, 0, y + 0.5 - (h - r))
                if math.hypot(dx, dy) > r:
                    alpha = 0
            row.append(tuple(int(round(c)) for c in colour) + (alpha,))
        pixels.append(row)
    return pixels


def text(pixels, words, top, scale):
    w = len(pixels[0])
    width = len(words) * 6 * scale - scale
    left = (w - width) // 2
    for n, ch in enumerate(words):
        glyph = FONT[ch]
        for gy, line in enumerate(glyph):
            for gx, bit in enumerate(line):
                if bit != '#':
                    continue
                for sy in range(scale):
                    for sx in range(scale):
                        x = left + (n * 6 + gx) * scale + sx
                        y = top + gy * scale + sy
                        if 0 <= y + 1 < len(pixels) and 0 <= x + 1 < w:
                            pixels[y + 1][x + 1] = (40, 6, 4, 255)       # shadow
                        if 0 <= y < len(pixels) and 0 <= x < w:
                            pixels[y][x] = (255, 236, 200, 255)


def write_png(path, pixels):
    h, w = len(pixels), len(pixels[0])
    raw = b''.join(b'\0' + bytes(c for px in row for c in px) for row in pixels)

    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0))
    png += chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b'')
    Path(path).write_bytes(png)


def write_chime(path, rate=22050):
    """A rising arpeggio of soft bell tones (C, E, G, high C), about 2 s: mono, 16-bit."""
    notes = ((0.00, 523.25), (0.11, 659.25), (0.22, 783.99), (0.36, 1046.50))
    length = int(rate * 2.0)
    samples = []
    for i in range(length):
        t = i / rate
        v = 0.0
        for start, freq in notes:
            if t >= start:
                s = t - start
                env = math.exp(-3.2 * s) * min(1.0, s / 0.01)
                v += env * (math.sin(2 * math.pi * freq * s) + 0.3 * math.sin(4 * math.pi * freq * s))
        samples.append(int(max(-1.0, min(1.0, v * 0.22)) * 32767))
    data = struct.pack('<%dh' % length, *samples)
    header = b'RIFF' + struct.pack('<I', 36 + len(data)) + b'WAVE'
    header += b'fmt ' + struct.pack('<IHHIIHH', 16, 1, 1, rate, rate * 2, 2, 16)
    Path(path).write_bytes(header + b'data' + struct.pack('<I', len(data)) + data)


def main(out):
    out = Path(out)
    out.mkdir(parents=True, exist_ok=True)
    write_png(out / 'icon.png', picture(48, 48, rounded=True, box=(0, 0, 48)))
    banner = picture(256, 128, rounded=False, box=(128 - 46, 2, 92))
    text(banner, 'TWIN EMBERS', 104, 2)
    write_png(out / 'banner.png', banner)
    write_chime(out / 'banner.wav')


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else 'build')
