#!/usr/bin/env python3
"""Drawing helpers for the Nt social media tiles: the Tile canvas, the mark, the GitHub icon and the header.

Needs Pillow, rsvg-convert and the Titillium Web fonts (SemiBold, Regular, Bold; SIL OFL 1.1) in TITILLIUM_DIR.
The screenshots come from doc/images, so run it after the widget pictures are up to date.
"""
import io
import os
import re
import subprocess
import sys

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
OUT = "."


def set_out(path):
    global OUT
    OUT = path
    os.makedirs(OUT, exist_ok=True)

PINE = (11, 46, 39)
PINE_DEEP = (6, 32, 27)
NEON = (63, 217, 180)
TEAL = (19, 151, 127)
WHITE = (255, 255, 255)
MUTED = (166, 205, 195)
S = 2  # supersampling

FONT = lambda w, size: ImageFont.truetype(os.path.join(os.environ.get("TITILLIUM_DIR", HERE), f"TitilliumWeb-{w}.ttf"), int(size * S))
MONO = lambda size: ImageFont.truetype("/usr/share/fonts/truetype/noto/NotoSansMono-Regular.ttf", int(size * S))
MONO_B = lambda size: ImageFont.truetype("/usr/share/fonts/truetype/noto/NotoSansMono-Bold.ttf", int(size * S))

LETTERS = re.search(r'<path[^>]*d="([^"]+)"', open(f"{REPO}/assets/nt-logo.svg").read()).group(1)
GITHUB = open(f"{HERE}/github-fontawesome.svg").read()
GITHUB_PATH = re.search(r'<path d="([^"]+)"', GITHUB).group(1)


def rgb(c):
    return "#%02x%02x%02x" % c


def svg_to_image(svg, px):
    png = subprocess.run(["rsvg-convert", "-w", str(int(px)), "-f", "png"], input=svg.encode(), capture_output=True, check=True).stdout
    return Image.open(io.BytesIO(png)).convert("RGBA")


def mark(px, color=NEON, letters=PINE):
    """The Nt mark: a hexagon with the letters cut out in the color of what is behind it."""
    svg = f'''<svg xmlns="http://www.w3.org/2000/svg" viewBox="8 8 240 240">
<polygon points="128,24 218,76 218,180 128,232 38,180 38,76" fill="{rgb(color)}" stroke="{rgb(color)}" stroke-width="24" stroke-linejoin="round"/>
<path fill="{rgb(letters)}" d="{LETTERS}"/></svg>'''
    return svg_to_image(svg, px * S)


def github_icon(px, color=WHITE):
    svg = f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 496 512"><path fill="{rgb(color)}" d="{GITHUB_PATH}"/></svg>'
    img = svg_to_image(svg, px * S * 496 / 512)
    return img


class Tile:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.img = Image.new("RGBA", (w * S, h * S), PINE + (255,))
        self.d = ImageDraw.Draw(self.img)
        self.pattern()

    def pattern(self):
        """A faint field of hexagons, the nut of the mark, at the right and bottom."""
        layer = Image.new("RGBA", self.img.size, (0, 0, 0, 0))
        d = ImageDraw.Draw(layer)
        import math
        r = 70 * S
        for row in range(-1, int(self.h * S / (r * 1.5)) + 2):
            for col in range(-1, int(self.w * S / (r * math.sqrt(3))) + 2):
                cx = col * r * math.sqrt(3) + (r * math.sqrt(3) / 2 if row % 2 else 0)
                cy = row * r * 1.5
                # fade out away from the bottom right corner
                dist = math.hypot(self.w * S - cx, self.h * S - cy) / math.hypot(self.w * S, self.h * S)
                alpha = int(max(0, 26 * (1 - dist * 1.35)))
                if alpha <= 0:
                    continue
                pts = [(cx + r * 0.92 * math.cos(math.radians(60 * i + 30)), cy + r * 0.92 * math.sin(math.radians(60 * i + 30))) for i in range(6)]
                d.polygon(pts, outline=NEON + (alpha,))
        self.img.alpha_composite(layer)
        self.d = ImageDraw.Draw(self.img)

    def paste(self, im, x, y):
        self.img.alpha_composite(im, (int(x * S), int(y * S)))
        self.d = ImageDraw.Draw(self.img)

    def text(self, x, y, s, font, fill=WHITE, anchor="la"):
        self.d.text((x * S, y * S), s, font=font, fill=fill, anchor=anchor)

    def width(self, s, font):
        return self.d.textlength(s, font=font) / S

    def rrect(self, box, r, fill=None, outline=None, width=1):
        x0, y0, x1, y1 = box
        self.d.rounded_rectangle((x0 * S, y0 * S, x1 * S, y1 * S), r * S, fill=fill, outline=outline, width=width * S)

    def wrap(self, x, y, s, font, maxw, fill=WHITE, line=1.25):
        words, lines, cur = s.split(), [], ""
        for w in words:
            t = (cur + " " + w).strip()
            if self.width(t, font) <= maxw:
                cur = t
            else:
                lines.append(cur)
                cur = w
        lines.append(cur)
        size = font.size / S
        for i, l in enumerate(lines):
            self.text(x, y + i * size * line, l, font, fill)
        return y + len(lines) * size * line

    def chip(self, s, font, right, cy, fg=PINE, bg=NEON, padx=18, height=None):
        """A pill with its right edge at `right` and its middle at `cy`; the text is centered in it by its capital letters."""
        w = self.width(s, font) + 2 * padx
        h = height or font.size / S + 14
        x0, y0 = right - w, cy - h / 2
        self.rrect((x0, y0, right, y0 + h), h / 2, fill=bg)
        # "m" anchors at the middle of the x-height; the capitals are taller, so measure them
        top, bottom = font.getbbox("H")[1] / S, font.getbbox("H")[3] / S
        self.d.text(((x0 + w / 2) * S, (cy - (top + bottom) / 2) * S), s, font=font, fill=fg, anchor="ma")
        return x0

    def footer(self, pad, compact=False):
        """GitHub icon and address, the license line and the disclaimer the brand guide asks for."""
        y = self.h - pad
        icon = github_icon(40 if not compact else 34, WHITE)
        self.paste(icon, pad, y - icon.height / S)
        f = FONT("SemiBold", 28 if not compact else 24)
        self.text(pad + 40 + 16 if not compact else pad + 34 + 14, y - icon.height / S / 2, "github.com/open-nt/Nt6", f, WHITE, "lm")
        right = FONT("Regular", 20 if not compact else 18)
        self.text(self.w - pad, y - icon.height / S / 2, "MIT  ·  C++17  ·  Linux first", right, MUTED, "rm")

    def disclaimer(self, pad):
        self.text(self.w / 2, self.h - 22, "Nt is a clean-room implementation of the Qt 6 API and is not affiliated with The Qt Company.",
                  FONT("Regular", 13), (110, 150, 140), "mm")

    def save(self, name):
        out = self.img.convert("RGB").resize((self.w, self.h), Image.LANCZOS)
        path = os.path.join(OUT, name)
        out.save(path, optimize=True)
        print(path, out.size)


def header(t, pad, label="FOR DEVELOPERS"):  # the mark, the name and a pill on the right
    t.paste(mark(84), pad, pad - 6)
    t.text(pad + 84 + 22, pad + 6, "Nt", FONT("Bold", 44), WHITE)
    t.text(pad + 84 + 22, pad + 54, "the Qt 6 API, MIT licensed", FONT("Regular", 20), MUTED)
    t.chip(label, FONT("SemiBold", 18), t.w - pad, pad + 36)


