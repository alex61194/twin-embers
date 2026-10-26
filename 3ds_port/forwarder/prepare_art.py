"""The forwarder's icon and banner from the owner's own pictures (kept outside the repository).

    python3 prepare_art.py --icon ICON.jpg --banner LOGO.jpg OUT_DIR
    python3 prepare_art.py --icon ICON.jpg OUT_DIR          (icon.png only: the game's 3DSX)

Writes OUT_DIR/icon.png (48x48) and OUT_DIR/banner.png (256x128, transparent around the logo:
the HOME Menu's theme shows through), and the banner chime of make_art.py (banner.wav).
Needs Pillow. The pictures are not stored in the repository: its source audit admits only text.

A logo with real transparency (PNG, WebP) is used as it is. A JPEG logo whose "transparency" is a
grey checkerboard painted into the pixels is cleaned: grey or light pixels connected to the border,
and enclosed regions of both checkerboard tones, are background. Its subtitle can be cut away
(--cut-subtitle, fractions of the picture's width and height: top, left, right; 'auto' cuts the
first logo's "3Ds Dual Screen" from a JPEG only); the base of the title's letters under it is
redrawn from a column outside it (--base-column).
"""
import argparse
from collections import deque
from pathlib import Path

from PIL import Image

import make_art

WORK_WIDTH = 1000      # the logo is processed at this width, then fitted into 256x128


def grey(c, low):
    return max(c[:3]) - min(c[:3]) <= 22 and min(c[:3]) >= low


def clear_background(im):
    """Grey/light pixels connected to the border become transparent; then two passes take off the
    light fringe the JPEG left around the logo."""
    w, h = im.size
    p = im.load()
    seen = bytearray(w * h)
    removed = []
    queue = deque([(x, y) for x in range(w) for y in (0, h - 1)] + [(x, y) for y in range(h) for x in (0, w - 1)])
    while queue:
        x, y = queue.popleft()
        if seen[y * w + x] or not grey(p[x, y], 140):
            continue
        seen[y * w + x] = 1
        removed.append(p[x, y][0])
        p[x, y] = (0, 0, 0, 0)
        queue.extend(((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)) if 0 < x < w - 1 and 0 < y < h - 1 else
                     [(a, b) for a, b in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)) if 0 <= a < w and 0 <= b < h])
    clear_islands(im, removed)
    for _ in range(2):
        edge = []
        for y in range(1, h - 1):
            for x in range(1, w - 1):
                c = p[x, y]
                if c[3] and max(c[:3]) - min(c[:3]) <= 45 and min(c[:3]) >= 110 and \
                        any(p[a, b][3] == 0 for a, b in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1))):
                    edge.append((x, y))
        for x, y in edge:
            p[x, y] = (0, 0, 0, 0)


def checker_tones(values):
    """The checkerboard's two greys: the two most common levels of the background removed."""
    counts = {}
    for v in values:
        counts[v // 4] = counts.get(v // 4, 0) + 1
    ranked = sorted(counts, key=counts.get, reverse=True)
    first = ranked[0]
    second = next((b for b in ranked[1:] if abs(b - first) * 4 > 20), first)
    return first * 4 + 2, second * 4 + 2


def clear_islands(im, removed):
    """Checkerboard enclosed by the logo (between flames and the ball) is not connected to the
    border: grey regions made of both checkerboard tones go too. Light parts of the logo itself
    (the ball's button) are one shade, not the two tones, and stay."""
    if not removed:
        return
    tones = checker_tones(removed)
    w, h = im.size
    p = im.load()
    seen = bytearray(w * h)
    for y0 in range(h):
        for x0 in range(w):
            if seen[y0 * w + x0] or not (p[x0, y0][3] and grey(p[x0, y0], 140)):
                continue
            queue, region = deque([(x0, y0)]), []
            seen[y0 * w + x0] = 1
            while queue:
                x, y = queue.popleft()
                region.append((x, y))
                for a, b in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
                    if 0 <= a < w and 0 <= b < h and not seen[b * w + a] and p[a, b][3] and grey(p[a, b], 140):
                        seen[b * w + a] = 1
                        queue.append((a, b))
            if len(region) < 15:
                continue
            share = [sum(1 for x, y in region if abs(p[x, y][0] - t) <= 12) / len(region) for t in tones]
            if min(share) >= 0.15:
                for x, y in region:
                    p[x, y] = (0, 0, 0, 0)


def cut_subtitle(im, top, left, right, base_column):
    w, h = im.size
    p = im.load()
    cut, x0, x1, src = round(top * h), round(left * w), round(right * w), round(base_column * w)
    base = [p[src, y] for y in range(cut, h)]
    base = base[:next((i for i, c in enumerate(base) if c[3] == 0), len(base))]
    dark = base[min(2, len(base) - 1)] if base else (0, 0, 0, 255)
    for y in range(max(0, cut - 30), cut):          # the subtitle's tips above the cut
        for x in range(x0, x1):
            r, g, b, a = p[x, y]
            if a and r > 200 and g > 165 and b > 130:
                p[x, y] = dark
    for x in range(x0, x1):
        letter = p[x, cut - 1][3] != 0
        for y in range(cut, h):
            p[x, y] = (0, 0, 0, 0)
        if letter:
            for i, c in enumerate(base):
                p[x, cut + i] = c


def real_transparency(im):
    return im.mode in ('RGBA', 'LA', 'PA') or (im.mode == 'P' and 'transparency' in im.info)


def banner(path, subtitle, base_column):
    src = Image.open(path)
    if real_transparency(src) and src.convert('RGBA').getchannel('A').getextrema()[0] == 0:
        # A logo with real transparency (PNG, WebP) is used as it is; its nearly opaque body
        # (an encoder's alpha of 250-254) is made fully opaque, its soft edges kept.
        im = src.convert('RGBA')
        im.putalpha(im.getchannel('A').point(lambda a: 255 if a >= 250 else a))
        if subtitle == 'auto':
            subtitle = None
    else:
        im = src.convert('RGB')
        im = im.resize((WORK_WIDTH, round(im.height * WORK_WIDTH / im.width)), Image.LANCZOS).convert('RGBA')
        clear_background(im)
        if subtitle == 'auto':
            subtitle = (0.86, 0.236, 0.794)
    if subtitle:
        cut_subtitle(im, *subtitle, base_column)
    logo = im.crop(im.getbbox())
    scale = min(248 / logo.width, 122 / logo.height)
    logo = logo.resize((round(logo.width * scale), round(logo.height * scale)), Image.LANCZOS)
    out = Image.new('RGBA', (256, 128), (0, 0, 0, 0))
    out.alpha_composite(logo, ((256 - logo.width) // 2, (128 - logo.height) // 2))
    return out


def icon(path, crop):
    im = Image.open(path).convert('RGB')
    s = min(im.size)
    m = round(s * crop)
    im = im.crop((m, round(m * 0.78), s - m, s - m - round(m * 0.22)))   # the ball sits a little high
    return im.resize((48, 48), Image.LANCZOS)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--icon', type=Path, required=True)
    ap.add_argument('--banner', type=Path, help='left out: only icon.png (the 3DSX icon)')
    ap.add_argument('--icon-crop', type=float, default=0.09, help='fraction cut from each side of the icon')
    ap.add_argument('--cut-subtitle', default='auto',
                    help="top,left,right of the logo's subtitle as fractions, or 'none'")
    ap.add_argument('--base-column', type=float, default=0.2, help='a column of the title outside the subtitle')
    ap.add_argument('out', type=Path)
    a = ap.parse_args()
    a.out.mkdir(parents=True, exist_ok=True)
    subtitle = (None if a.cut_subtitle == 'none' else 'auto' if a.cut_subtitle == 'auto'
                else tuple(float(v) for v in a.cut_subtitle.split(',')))
    icon(a.icon, a.icon_crop).save(a.out / 'icon.png')
    if a.banner is None:
        return
    banner(a.banner, subtitle, a.base_column).save(a.out / 'banner.png')
    make_art.write_chime(a.out / 'banner.wav')


if __name__ == '__main__':
    main()
