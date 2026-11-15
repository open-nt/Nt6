#!/usr/bin/env python3
"""Builds the 1200x630 link preview of the landing page (site/index.html) into the output directory given as argv[1].
packaging/docs-site.sh copies it to the root of the documentation site, where the Open Graph and Twitter card tags of
the landing page point.

The picture on the right is the path tracer example of github.com/open-nt/nt6-examples; give a local copy of
raytracing/pathtracer.png as argv[2], or it is downloaded. See tilelib.py for what else it needs.
"""
import io
import sys
import urllib.request

from PIL import Image, ImageDraw

from tilelib import *  # noqa: F401,F403
import tilelib

PATHTRACER = "https://github.com/open-nt/nt6-examples/raw/main/raytracing/pathtracer.png"

tilelib.set_out(sys.argv[1])

LINE = (30, 90, 77)
DIM = (110, 150, 140)
CHIP = (22, 72, 61)


def render():
    if len(sys.argv) > 2:
        return Image.open(sys.argv[2]).convert("RGBA")
    with urllib.request.urlopen(PATHTRACER) as reply:
        return Image.open(io.BytesIO(reply.read())).convert("RGBA")


def framed(t, im, x, y, w):
    """The render scaled to width w, with rounded corners and a thin frame, its top left corner at (x, y)."""
    h = round(w * im.height / im.width)
    t.rrect((x - 3, y - 3, x + w + 3, y + h + 3), 18, fill=LINE)
    pic = im.resize((w * S, h * S), Image.LANCZOS)
    mask = Image.new("L", pic.size, 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, pic.width - 1, pic.height - 1), 16 * S, fill=255)
    pic.putalpha(mask)
    t.paste(pic, x, y)
    return h


def landing_card():
    t = Tile(1200, 630)
    pad = 64
    t.paste(mark(84), pad, 52)
    t.text(pad + 84 + 22, 52 + 4, "Nt", FONT("Bold", 44), WHITE)
    t.text(pad + 84 + 22, 52 + 52, "the Qt 6 API, MIT licensed", FONT("Regular", 20), MUTED)
    t.text(pad, 192, "Write Qt 6.", FONT("Bold", 76), WHITE)
    t.text(pad, 192 + 80, "Ship anywhere.", FONT("Bold", 76), NEON)
    t.wrap(pad, 396, "Static, MIT-licensed libraries for embedded Linux, Arm and Zephyr microcontrollers.",
           FONT("Regular", 24), 500, MUTED, 1.3)
    x = pad
    for label in ("Embedded", "arm64", "Zephyr", "Vulkan", "Snap"):
        f = FONT("SemiBold", 17)
        w = t.width(label, f) + 30
        t.chip(label, f, x + w, 504, fg=NEON, bg=CHIP, padx=15, height=32)
        x += w + 10
    t.text(pad, 562, "open-nt.github.io/Nt6", FONT("SemiBold", 26), WHITE, "lm")

    w = 480
    x = t.w - pad - w
    h = framed(t, render(), x, 160, w)
    t.text(x, 160 + h + 28, "Path traced with Nt Vulkan and VK_KHR_ray_tracing_pipeline", FONT("Regular", 16), MUTED, "lm")
    t.text(pad, t.h - 22, "Nt is a clean-room implementation of the Qt 6 API and is not affiliated with The Qt Company.",
           FONT("Regular", 13), DIM, "lm")
    t.save("nt-social-14-landing-1200x630.png")


landing_card()
