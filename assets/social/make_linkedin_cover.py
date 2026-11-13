#!/usr/bin/env python3
"""Builds the LinkedIn cover images into the output directory given as argv[1]:

- nt-social-12-linkedin-cover-1584x396.png, the background of a personal profile or group. The profile photo sits over
  its bottom left corner and phones crop its sides, so the text starts 400 px in and nothing that matters is near an edge.
- nt-social-13-linkedin-page-1128x191.png, the cover of a company or showcase page, whose logo overlaps the bottom left.

See tilelib.py for what it needs.
"""
import sys

from PIL import Image, ImageDraw

from tilelib import *  # noqa: F401,F403
import tilelib

tilelib.set_out(sys.argv[1])

LINE = (30, 90, 77)
DIM = (110, 150, 140)
CHIP = (22, 72, 61)


def screenshot(t, x, y, w, h):
    """The first tab of the widgets example, cut off at the bottom edge of the cover."""
    page = Image.open(f"{REPO}/doc/images/hello-widgets-nt.png").convert("RGBA")
    page = page.resize((w * S, int(page.height * w / page.width * S)), Image.LANCZOS).crop((0, 0, w * S, h * S))
    mask = Image.new("L", page.size, 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, page.width, page.height + 30 * S), 18 * S, fill=255)
    page.putalpha(mask)
    t.rrect((x - 3, y - 3, x + w + 3, y + h + 40), 21, fill=LINE)
    t.paste(page, x, y)


def chips(t, x, cy, labels, size, height, gap):
    f = FONT("SemiBold", size)
    for label in labels:
        w = t.width(label, f) + 2 * 14
        t.chip(label, f, x + w, cy, fg=NEON, bg=CHIP, padx=14, height=height)
        x += w + gap


def profile_cover():
    t = Tile(1584, 396)
    x = 420
    t.paste(mark(132), x, 70)
    tx = x + 132 + 34
    t.text(tx, 62, "Nt", FONT("Bold", 84), WHITE)
    t.text(tx, 62 + 98, "The Qt 6 API. MIT licensed.", FONT("SemiBold", 40), NEON)
    t.text(tx, 62 + 98 + 52, "Clean-room C++17. Linux first, Zephyr too.", FONT("Regular", 26), MUTED)
    chips(t, x, 290, ("Core", "Network", "Widgets", "Qml", "Zephyr"), 17, 34, 10)
    icon = github_icon(30)
    gy = 290
    gx = 880
    t.paste(icon, gx, gy - icon.height / S / 2)
    t.text(gx + 30 + 12, gy, "github.com/open-nt/Nt6", FONT("SemiBold", 22), WHITE, "lm")
    screenshot(t, t.w - 80 - 300, 56, 300, 340)
    t.text(t.w - 80 - 300 - 30, t.h - 18, "Not affiliated with The Qt Company.", FONT("Regular", 13), DIM, "rm")
    t.save("nt-social-12-linkedin-cover-1584x396.png")


def page_cover():
    t = Tile(1128, 191)
    x = 250
    t.paste(mark(84), x, 36)
    tx = x + 84 + 24
    t.text(tx, 26, "Nt", FONT("Bold", 52), WHITE)
    t.text(tx, 26 + 64, "The Qt 6 API. MIT licensed.", FONT("SemiBold", 26), NEON)
    icon = github_icon(22)
    t.paste(icon, tx, 140 - icon.height / S / 2)
    t.text(tx + 22 + 10, 140, "github.com/open-nt/Nt6", FONT("SemiBold", 17), MUTED, "lm")
    screenshot(t, t.w - 60 - 220, 28, 220, 163)
    t.text(t.w - 60 - 220 - 24, t.h - 14, "Not affiliated with The Qt Company.", FONT("Regular", 11), DIM, "rm")
    t.save("nt-social-13-linkedin-page-1128x191.png")


profile_cover()
page_cover()
