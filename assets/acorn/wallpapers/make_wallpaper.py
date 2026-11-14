#!/usr/bin/env python3
# MIT License - see LICENSE
"""Makes the release wallpapers: the Acorn crown, the acorn's cup seen from above as hexagonal scales on the golden
spiral, in the brand colours (see doc/brand.h); on its own in the clean version and with the Nt mark, the version and
the release name in the full one. Writes an SVG per variant and orientation and renders the PNG sizes with
rsvg-convert. The lettering is set in Titillium Web (SIL OFL 1.1), which fontconfig must know, as in ~/.fonts; it
goes into the SVG as outlines (see ttfpaths.py), so that the files look the same wherever they are opened.

    assets/acorn/wallpapers/make_wallpaper.py [--version 6.0.0] [--name Acorn] [--out assets/acorn/wallpapers]

A later release can keep this layout and put its own artwork in the crown's place with --image (a square SVG or PNG,
generated with the prompt in PROMPT.md or drawn); the nt-icon-style skill describes the look it should have.
"""
import argparse
import math
import os
import re
import subprocess

import ttfpaths

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))

PINE = "#0B2E27"
PINE_DEEP = "#06201B"
PINE_LINE = "#1E5A4D"
NEON = "#3FD9B4"
NEON_DEEP = "#2BB894"
TEAL = "#13977F"
TEAL_DEEP = "#0C6655"
WHITE = "#FFFFFF"
MUTED = "#A6CDC3"

LETTERS = re.search(r'<path[^>]*d="([^"]+)"', open(f"{REPO}/assets/nt-logo.svg").read()).group(1)

# The PNG sizes rendered from each orientation.
LANDSCAPE_SIZES = [(3840, 2160), (2560, 1440), (1920, 1080)]
PORTRAIT_SIZES = [(1440, 3120), (1080, 2340)]


def hexagon(cx, cy, r):
    """A pointy-top hexagon."""
    points = []
    for i in range(6):
        a = math.radians(60 * i - 30)
        points.append(f"{cx + r * math.cos(a):.2f},{cy + r * math.sin(a):.2f}")
    return " ".join(points)


def hex_pattern(name, r, fill, stroke, width, shift=0):
    """A seamless tiling of pointy-top hexagons: a tile two hexagons wide and one and a half tall, with a hexagon at
    each corner and one in the middle."""
    w = math.sqrt(3) * r
    tile_w, tile_h = 2 * w, 3 * r
    cells = [(0, 0), (tile_w, 0), (w, 1.5 * r), (0, tile_h), (tile_w, tile_h)]
    polys = "".join(f'<polygon points="{hexagon(x, y, r)}" fill="{fill}" stroke="{stroke}" stroke-width="{width}" '
                    f'stroke-linejoin="round"/>' for x, y in cells)
    return (f'<pattern id="{name}" width="{tile_w:.3f}" height="{tile_h:.3f}" patternUnits="userSpaceOnUse" '
            f'patternTransform="translate(0 {shift})">{polys}</pattern>')


def mark(x, y, size):
    """The Nt mark in neon with the letters cut to pine, placed with its top-left corner at (x, y)."""
    scale = size / 240.0
    return (f'<g transform="translate({x} {y}) scale({scale:.5f}) translate(-8 -8)">'
            f'<polygon points="128,24 218,76 218,180 128,232 38,180 38,76" fill="{NEON}" stroke="{NEON}" '
            f'stroke-width="24" stroke-linejoin="round"/><path fill="{PINE}" d="{LETTERS}"/></g>')


GENERATED = None  # the path of an image given with --image, which takes the crown's place
GOLDEN = math.radians(137.50776)


def crown(cx, cy, diameter, cells=190, glow=True):
    """The Acorn crown: hexagonal scales set on the golden-angle spiral, as the scales of a real cup are, growing from
    the stem at the centre to the rim. Each scale turns with its angle and carries a small bevel, lit from the upper
    left, so the field reads as a surface. `cells` thins it for small sizes."""
    radius = diameter / 2.0
    k = radius / math.sqrt(cells)
    out = []
    if glow:
        out.append(f'<circle cx="{cx}" cy="{cy}" r="{radius * 1.25:.0f}" fill="url(#glow)"/>')
    for i in range(1, cells + 1):
        r = k * math.sqrt(i)
        a = i * GOLDEN
        size = 0.33 * k + 0.055 * r
        x, y = cx + r * math.cos(a), cy + r * math.sin(a)
        t = r / radius
        fill = mix(NEON, TEAL_DEEP, t * t * 0.85 + t * 0.15)
        points = " ".join(f"{x + size * math.cos(a + math.radians(60 * j - 30)):.1f},"
                          f"{y + size * math.sin(a + math.radians(60 * j - 30)):.1f}" for j in range(6))
        out.append(f'<polygon points="{points}" fill="{fill}" stroke="{PINE}" stroke-width="{max(1.0, k * 0.09):.1f}" '
                   f'stroke-linejoin="round"/>')
        out.append(f'<polygon points="{points}" fill="url(#bevel)"/>')
    out.append(f'<circle cx="{cx}" cy="{cy}" r="{k * 0.75:.1f}" fill="{TEAL_DEEP}"/>'
               f'<circle cx="{cx}" cy="{cy}" r="{k * 0.38:.1f}" fill="{PINE_DEEP}"/>')
    return "\n".join(out)


def mix(a, b, t):
    """A colour between two hex colours."""
    t = max(0.0, min(1.0, t))
    ra, ga, ba = int(a[1:3], 16), int(a[3:5], 16), int(a[5:7], 16)
    rb, gb, bb = int(b[1:3], 16), int(b[3:5], 16), int(b[5:7], 16)
    return "#%02X%02X%02X" % (round(ra + (rb - ra) * t), round(ga + (gb - ga) * t), round(ba + (bb - ba) * t))


def artwork(cx, cy, size):
    """The artwork, `size` across and centred on (cx, cy): the crown, or the image given with --image."""
    if not GENERATED:
        return crown(cx, cy, size)
    side = size * 1.15
    return (f'<circle cx="{cx}" cy="{cy}" r="{size * 0.66:.0f}" fill="url(#glow)"/>\n'
            f'<image href="{os.path.basename(GENERATED)}" x="{cx - side / 2:.0f}" y="{cy - side / 2:.0f}" '
            f'width="{side:.0f}" height="{side:.0f}" preserveAspectRatio="xMidYMid meet"/>')


def defs():
    return f'''<defs>
  <radialGradient id="vignette" cx="0.62" cy="0.42" r="0.78">
    <stop offset="0" stop-color="{PINE}"/><stop offset="1" stop-color="{PINE_DEEP}"/>
  </radialGradient>
  <radialGradient id="glow">
    <stop offset="0" stop-color="{NEON}" stop-opacity="0.26"/><stop offset="0.5" stop-color="{NEON}" stop-opacity="0.06"/>
    <stop offset="1" stop-color="{NEON}" stop-opacity="0"/>
  </radialGradient>
  <linearGradient id="bevel" x1="0" y1="0" x2="1" y2="1">
    <stop offset="0" stop-color="{WHITE}" stop-opacity="0.22"/><stop offset="0.5" stop-color="{WHITE}" stop-opacity="0"/>
    <stop offset="1" stop-color="{PINE_DEEP}" stop-opacity="0.35"/>
  </linearGradient>
  {hex_pattern("hexgrid", 60, "none", PINE_LINE, 3)}
  <linearGradient id="gridfade" x1="0" y1="0" x2="1" y2="1">
    <stop offset="0" stop-color="{WHITE}" stop-opacity="0.14"/><stop offset="1" stop-color="{WHITE}" stop-opacity="0.02"/>
  </linearGradient>
  <mask id="gridmask"><rect width="100%" height="100%" fill="url(#gridfade)"/></mask>
</defs>'''


def scatter(w, h, avoid, keep_clear=None, seed=7):
    """Small nuts scattered over the background, away from the artwork and from the text block."""
    out = []
    x, y = seed * 97.0, seed * 31.0
    for i in range(28):
        x = (x * 1103515245 + 12345) % 2147483648
        y = (y * 1103515245 + 54321) % 2147483648
        px, py = (x / 2147483648.0) * w, (y / 2147483648.0) * h
        ax, ay, ar = avoid
        if (px - ax) ** 2 + (py - ay) ** 2 < ar * ar:
            continue
        if keep_clear and px < keep_clear[0] and py > keep_clear[1]:
            continue
        r = 10 + (i % 5) * 7
        fill = NEON if i % 3 else TEAL
        out.append(f'<polygon points="{hexagon(px, py, r)}" fill="{fill}" opacity="{0.05 + (i % 4) * 0.03:.2f}"/>')
    return "\n".join(out)


FONTS = {}


def font(weight):
    """The Titillium Web face of a CSS `weight` (400, 600 or 700), through fontconfig."""
    if weight not in FONTS:
        fc_weight = {400: 80, 600: 180, 700: 200}[weight]
        path = subprocess.run(["fc-match", "-f", "%{file}", f"Titillium Web:weight={fc_weight}"],
                              capture_output=True, text=True, check=True).stdout
        if "Titillium" not in os.path.basename(path):
            raise SystemExit(f"fontconfig has no Titillium Web (weight {weight}); install it, as in ~/.fonts")
        FONTS[weight] = ttfpaths.Font(path)
    return FONTS[weight]


def text(x, y, size, weight, fill, content, spacing=0):
    """`content` set as outlines with its baseline at (x, y)."""
    d, _ = font(weight).path(content, x, y, size, spacing)
    return f'<path d="{d}" fill="{fill}"/>'


FOOTER = ("github.com/open-nt/Nt6   ·   Nt is a clean-room implementation of the Qt 6 API "
          "and is not affiliated with The Qt Company.")


def background(w, h, center, radius, keep_clear=None, seed=7):
    """The vignette, the faint hexagon grid and the scattered nuts, kept out of the circle of `radius` at `center`."""
    cx, cy = center
    return (f'<rect width="{w}" height="{h}" fill="url(#vignette)"/>\n'
            f'<rect width="{w}" height="{h}" fill="url(#hexgrid)" mask="url(#gridmask)"/>\n'
            f'{scatter(w, h, (cx, cy, radius), keep_clear, seed)}')


def caption(x, baseline, name, version, name_size, mark_size, version_size):
    """The release name, and under it the mark with the version to its right, centred on the mark."""
    row_top = baseline + name_size * 0.28
    version_baseline = row_top + mark_size * 0.5 + version_size * 0.36
    return (f'{text(x - name_size * 0.05, baseline, name_size, 700, NEON, name, spacing=-name_size * 0.018)}\n'
            f'{mark(x, row_top, mark_size)}\n'
            f'{text(x + mark_size * 1.22, version_baseline, version_size, 600, WHITE, version)}')


def landscape(version, name, clean):
    w, h = 3840, 2160
    body = (f'{background(w, h, (1920, 1080), 1100)}\n{artwork(1920, 1080, 1640)}' if clean else
            f'{background(w, h, (2640, 1080), 1050, keep_clear=(0.38 * w, 0.45 * h))}\n{artwork(2640, 1080, 1540)}\n'
            f'{caption(300, 1380, name, version, 340, 230, 150)}\n{text(300, 2050, 42, 400, MUTED, FOOTER)}')
    return f'''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} {h}" width="{w}" height="{h}">
<title>Nt {version} {name}</title>
{defs()}
{body}
</svg>
'''


def portrait(version, name, clean):
    w, h = 1440, 3120
    body = (f'{background(w, h, (720, 1450), 900, seed=11)}\n{artwork(720, 1450, 1240)}' if clean else
            f'{background(w, h, (720, 1130), 820, keep_clear=(w, 0.66 * h), seed=11)}\n{artwork(720, 1130, 1180)}\n'
            f'{caption(120, 2500, name, version, 250, 170, 110)}\n{text(120, 2990, 26, 400, MUTED, FOOTER)}')
    return f'''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} {h}" width="{w}" height="{h}">
<title>Nt {version} {name}</title>
{defs()}
{body}
</svg>
'''


def render(svg_path, sizes):
    for w, h in sizes:
        png = re.sub(r"\.svg$", f"-{w}x{h}.png", svg_path)
        subprocess.run(["rsvg-convert", "-w", str(w), "-h", str(h), "-o", png, svg_path], check=True)
        print("wrote", png)


def main():
    parser = argparse.ArgumentParser(description="Makes the Nt release wallpapers.")
    parser.add_argument("--version", default=open(f"{REPO}/VERSION").read().strip())
    parser.add_argument("--name", default="Acorn")
    parser.add_argument("--out", default=HERE)
    parser.add_argument("--image", help="a square image (SVG or PNG) to show instead of the crown; see PROMPT.md")
    args = parser.parse_args()
    global GENERATED
    if args.image:
        GENERATED = os.path.abspath(args.image)
        if os.path.dirname(GENERATED) != os.path.abspath(args.out):
            raise SystemExit("--image: put the image in the output directory, where rsvg-convert may read it")
    os.makedirs(args.out, exist_ok=True)
    stem = f"nt-{args.version}-{args.name.lower()}"
    for clean in (False, True):
        for suffix, maker, sizes in (("", landscape, LANDSCAPE_SIZES), ("-portrait", portrait, PORTRAIT_SIZES)):
            path = os.path.join(args.out, f"{stem}{'-clean' if clean else ''}{suffix}.svg")
            with open(path, "w") as f:
                f.write(maker(args.version, args.name, clean))
            print("wrote", path)
            render(path, sizes)


if __name__ == "__main__":
    main()
