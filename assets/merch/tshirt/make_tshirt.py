#!/usr/bin/env python3
# MIT License - see LICENSE
"""The Nt 6 "Acorn" T-shirt: print files and mockups.

Prints for a dark pine (bottle green) or black shirt in three flat spot inks, Nut Neon, Nut Teal and White, so the
same files work for screen printing and DTG. Gaps between shapes are knock-outs (the shirt shows through) rather
than pine ink. Lettering is outlined Titillium Web (SIL OFL 1.1, via fontconfig).

    python3 make_tshirt.py            # writes the SVGs and renders the PNGs with rsvg-convert

Outputs, next to this script:
    nt-tee-front.svg / -4500.png    left-chest mark, 4 in wide
    nt-tee-back.svg  / -4500x5400.png  full back print, 15 x 18 in at 300 dpi (POD standard)
    nt-tee-mockup.svg / .png        front and back on a pine shirt, for showing people
"""

import math
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "acorn", "wallpapers"))
import make_wallpaper as mw  # noqa: E402  (palette, the mark, outlined text)

NEON, TEAL, WHITE, PINE, MUTED = mw.NEON, mw.TEAL, mw.WHITE, mw.PINE, mw.MUTED
SHIRT = "#23372E"  # the AS Colour dark green chosen for the order, for the mockup only
GOLDEN = mw.GOLDEN


def mark_knockout(x, y, size, fill, uid):
    """The Nt mark in one ink with the letters knocked out to the shirt (transparent)."""
    s = size / 240.0
    return (f'<mask id="{uid}" maskUnits="userSpaceOnUse" x="-10000" y="-10000" width="20000" height="20000">'
            f'<rect x="-10000" y="-10000" width="20000" height="20000" fill="#fff"/>'
            f'<path fill="#000" transform="translate({x} {y}) scale({s:.5f}) translate(-8 -8)" d="{mw.LETTERS}"/></mask>'
            f'<g mask="url(#{uid})"><polygon transform="translate({x} {y}) scale({s:.5f}) translate(-8 -8)" '
            f'points="128,24 218,76 218,180 128,232 38,180 38,76" fill="{fill}" stroke="{fill}" stroke-width="24" '
            f'stroke-linejoin="round"/></g>')


def flat_crown(cx, cy, diameter, cells=233):
    """The Acorn crown in three flat inks: hexagonal scales on the golden-angle spiral, white at the heart, neon,
    then teal at the rim. Each scale is shrunk so a knock-out gap separates it from its neighbours."""
    radius = diameter / 2.0
    k = radius / math.sqrt(cells)
    out = []
    for i in range(1, cells + 1):
        r = k * math.sqrt(i)
        a = i * GOLDEN
        size = (0.46 * k + 0.05 * r) * 0.9
        x, y = cx + r * math.cos(a), cy + r * math.sin(a)
        t = r / radius
        fill = WHITE if t < 0.3 else NEON if t < 0.7 else TEAL
        pts = " ".join(f"{x + size * math.cos(a + math.radians(60 * j - 30)):.1f},"
                       f"{y + size * math.sin(a + math.radians(60 * j - 30)):.1f}" for j in range(6))
        out.append(f'<polygon points="{pts}" fill="{fill}" stroke="{fill}" stroke-width="{k * 0.06:.1f}" '
                   f'stroke-linejoin="round"/>')
    return "\n".join(out)


def centred(cy, size, weight, fill, content, width, spacing=0):
    _, w = mw.font(weight).path(content, 0, 0, size, spacing)
    return mw.text((width - w) / 2, cy, size, weight, fill, content, spacing)


def back(w=4500, h=5400):
    """Full back print: crown, Nt 6 · Acorn and the platforms."""
    cx = w / 2
    parts = [flat_crown(cx, 1900, 3300)]
    parts.append(centred(4250, 620, 700, NEON, "Nt 6  ·  Acorn", w, spacing=6))
    parts.append(centred(4650, 150, 600, WHITE, "LINUX  ·  ANDROID  ·  WINDOWS  ·  MACOS  ·  ZEPHYR", w, spacing=18))
    parts.append(centred(5000, 210, 600, TEAL, "open-nt.org", w, spacing=12))
    return parts


def front(w=1200, h=1200):
    """Left-chest mark: the nut, letters knocked out, with no lettering beside it."""
    return [mark_knockout(80, 80, 1040, NEON, "fm")]


def svg(w, h, body, bg=None):
    rect = f'<rect width="{w}" height="{h}" fill="{bg}"/>' if bg else ""
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} {h}" width="{w}" height="{h}">'
            f'{rect}\n' + "\n".join(body) + "\n</svg>\n")


def shirt_path(ox, oy, s):
    """A plain crew-neck T-shirt silhouette, 1000 units wide before scaling by `s`."""
    d = ("M330 40 Q500 120 670 40 L870 110 L1000 360 L850 430 L800 330 L800 1080 Q500 1110 200 1080 "
         "L200 330 L150 430 L0 360 L130 110 Z")
    return f'<path transform="translate({ox} {oy}) scale({s})" d="{d}" fill="{SHIRT}"/>'


def mockup():
    W, H = 2400, 1400
    out = [f'<rect width="{W}" height="{H}" fill="#E9F2EF"/>']
    for i, (label, ox) in enumerate((("FRONT", 100), ("BACK", 1250))):
        out.append(shirt_path(ox, 140, 1.05))
        # collar
        out.append(f'<path transform="translate({ox} 140) scale(1.05)" d="M330 40 Q500 150 670 40 Q500 100 330 40Z" '
                   f'fill="#0A2620"/>')
        out.append(mw.text(ox + 470, 110, 44, 600, PINE, label, spacing=6))
    # front: left chest (wearer's left = viewer's right), about 4 in on a 20 in chest
    out.append(f'<g transform="translate({100 + 600} {140 + 250}) scale(0.16)">{"".join(front())}</g>')
    # back: 12 in wide print on the same shirt scale
    s = 0.125
    out.append(f'<g transform="translate({1250 + 525 - 4500 * s / 2:.0f} {140 + 220}) scale({s})">{"".join(back())}</g>')
    return svg(W, H, out)


def render(name, width):
    subprocess.run(["rsvg-convert", "-w", str(width), "-o", f"{HERE}/{name}.png", f"{HERE}/{name}.svg"], check=True)


def main():
    files = {"nt-tee-back": (back(), 4500, 5400), "nt-tee-front": (front(), 1200, 1200)}
    for name, (body, w, h) in files.items():
        open(f"{HERE}/{name}.svg", "w").write(svg(w, h, body))
    open(f"{HERE}/nt-tee-mockup.svg", "w").write(mockup())
    render("nt-tee-back", 4500)
    render("nt-tee-front", 1200)
    render("nt-tee-mockup", 2400)


if __name__ == "__main__":
    main()
