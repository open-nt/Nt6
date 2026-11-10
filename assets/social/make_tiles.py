#!/usr/bin/env python3
"""Builds the Nt social media tiles into the output directory given as argv[1].

See tilelib.py for what it needs.
"""
import math
import os
import re
import sys

from PIL import Image, ImageDraw

from tilelib import *  # noqa: F401,F403
import tilelib

tilelib.set_out(sys.argv[1])
OUT = tilelib.OUT

# ---- 1. Hero ----
def hero():
    t = Tile(1080, 1080)
    pad = 80
    header(t, pad)
    big = FONT("Bold", 132)
    t.text(pad, 270, "Write Qt.", big, WHITE)
    t.text(pad, 270 + 140, "Ship MIT.", big, NEON)
    t.wrap(pad, 610, "Nt is a small, clean-room implementation of the Qt 6 API: Core, Network, HTTP Server, WebSockets, GUI, Widgets and more.",
           FONT("Regular", 36), 880, MUTED, 1.3)
    t.footer(pad + 0)
    t.save("nt-social-1-hero.png")


# ---- 2. Code ----
KEYWORDS = {"QHttpServer", "QTcpServer", "QHostAddress", "QString", "auto", "const", "return"}


def code_tile():
    t = Tile(1080, 1080)
    pad = 80
    header(t, pad)
    t.text(pad, 250, "A web service", FONT("Bold", 86), WHITE)
    t.text(pad, 250 + 92, "in six lines.", FONT("Bold", 86), NEON)
    t.rrect((pad, 480, t.w - pad, 900), 22, fill=PINE_DEEP, outline=(30, 90, 77), width=2)
    for i, c in enumerate(((255, 95, 86), (255, 189, 46), (39, 201, 63))):
        t.d.ellipse(((pad + 28 + i * 30) * S, 506 * S, (pad + 44 + i * 30) * S, 522 * S), fill=c)
    t.chip("Nt::HttpServer", FONT("SemiBold", 16), t.w - pad - 24, 514, padx=14, height=26)
    lines = [
        'QHttpServer server;',
        'server.route("/hello/<arg>",',
        '    [](const QString &name) {',
        '        return "Hello, " + name;',
        '    });',
        'server.bind(tcp);  // a QTcpServer',
    ]
    f = MONO(31)
    y = 556
    for l in lines:
        x = pad + 36
        for tok in re.findall(r'"[^"]*"|//.*|\w+|\s+|.', l):
            col = (222, 240, 235)
            if tok.startswith('"'):
                col = (255, 214, 140)
            elif tok.startswith("//"):
                col = (110, 160, 148)
            elif tok in KEYWORDS:
                col = NEON
            elif tok in ("route", "bind"):
                col = (140, 200, 255)
            t.text(x, y, tok, f, col)
            x += t.width(tok, f)
        y += 52
    t.footer(pad)
    t.save("nt-social-2-code.png")


# ---- 3. Modules ----
def modules_tile():
    t = Tile(1080, 1080)
    pad = 80
    header(t, pad)
    t.text(pad, 215, "Nine modules.", FONT("Bold", 96), WHITE)
    t.text(pad, 215 + 100, "One familiar API.", FONT("Bold", 96), NEON)
    mods = [("Core", True), ("Network", True), ("Serial Port", False), ("D-Bus", False), ("HTTP Server", False),
            ("WebSockets", False), ("GUI", True), ("Widgets", True), ("Test", False)]
    gx, gy, gap = pad, 500, 22
    cw = (t.w - 2 * pad - 2 * gap) / 3
    ch = 120
    for i, (name, partial) in enumerate(mods):
        x = gx + (i % 3) * (cw + gap)
        y = gy + (i // 3) * (ch + gap)
        t.rrect((x, y, x + cw, y + ch), 18, fill=PINE_DEEP, outline=(30, 90, 77), width=2)
        small = mark(34)
        t.paste(small, x + 22, y + 22)
        t.text(x + 22, y + 66, "Nt " + name, FONT("SemiBold", 30), WHITE)
        if partial:
            t.text(x + cw - 20, y + 36, "partial", FONT("Regular", 17), MUTED, "rm")
    t.footer(pad)
    t.save("nt-social-3-modules.png")


# ---- 4. Widgets ----
def widgets_tile():
    t = Tile(1080, 1080)
    pad = 80
    header(t, pad)
    t.text(pad, 205, "Widgets,", FONT("Bold", 96), WHITE)
    t.text(pad, 205 + 100, "the Qt way.", FONT("Bold", 96), NEON)
    shot = Image.open(f"{REPO}/doc/images/hello-widgets-window.png").convert("RGBA")  # the first tab, Nt, dark and light
    w = t.w - 2 * pad
    h = int(shot.height * w / shot.width)
    shot = shot.resize((w * S, h * S), Image.LANCZOS)
    # round the corners of the screenshot and cut it where the footer starts
    top = 500
    room = t.h - 140 - top
    shot = shot.crop((0, 0, shot.width, min(shot.height, room * S)))
    mask = Image.new("L", shot.size, 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, shot.width, shot.height + 40 * S), 22 * S, fill=255)
    shot.putalpha(mask)
    t.rrect((pad - 3, top - 3, pad + w + 3, top + shot.height / S + 3), 24, fill=(30, 90, 77))
    t.paste(shot, pad, top)
    t.footer(pad)
    t.save("nt-social-4-widgets.png")


# ---- 5. Contribute ----
def contribute_tile():
    t = Tile(1080, 1080)
    pad = 80
    dev = svg_to_image(open(f"{REPO}/assets/brand/nt-development-logo-neon.svg").read(), 640 * S)
    t.paste(dev, pad - 32, pad - 46)
    t.chip("CONTRIBUTORS WELCOME", FONT("SemiBold", 18), t.w - pad, pad + 38)
    t.text(pad, 330, "Build Nt", FONT("Bold", 120), WHITE)
    t.text(pad, 330 + 126, "with us.", FONT("Bold", 120), NEON)
    items = ["Linux first, portable by design", "Every commit passes ./check.sh", "Tests, docs and sanitizers in CI", "MIT licensed, clean-room"]
    y = 650
    for it in items:
        t.d.polygon([((pad + 14 * math.cos(math.radians(60 * i + 30)) + 14) * S, (y + 25 + 14 * math.sin(math.radians(60 * i + 30))) * S) for i in range(6)], fill=NEON)
        t.text(pad + 44, y, it, FONT("Regular", 34), WHITE)
        y += 62
    t.footer(pad)
    t.save("nt-social-5-contribute.png")


# ---- 6. Link card 1200x630 ----
def card():
    t = Tile(1200, 630)
    pad = 64
    t.paste(mark(104), pad, 48)
    t.text(pad, 165, "Write Qt.", FONT("Bold", 104), WHITE)
    t.text(pad, 165 + 100, "Ship MIT.", FONT("Bold", 104), NEON)
    t.wrap(pad, 408, "A small, clean-room implementation of the Qt 6 API: Core, Network, HTTP Server, WebSockets, GUI, Widgets and more.",
           FONT("Regular", 25), 560, MUTED, 1.3)
    icon = github_icon(34)
    t.paste(icon, pad, 548)
    t.text(pad + 34 + 14, 548 + 17, "github.com/open-nt/Nt6", FONT("SemiBold", 26), WHITE, "lm")
    page = Image.open(f"{REPO}/doc/images/hello-widgets-nt.png").convert("RGBA")  # the first tab, Nt
    pw_t = 470
    ph_t = 490
    page = page.resize((pw_t * S, int(page.height * pw_t / page.width * S)), Image.LANCZOS).crop((0, 0, pw_t * S, ph_t * S))
    mask = Image.new("L", page.size, 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, page.width, page.height), 20 * S, fill=255)
    page.putalpha(mask)
    x = t.w - pad - pw_t
    y = 70
    t.rrect((x - 3, y - 3, x + pw_t + 3, y + ph_t + 3), 22, fill=(30, 90, 77))
    t.paste(page, x, y)
    t.text(pad, t.h - 24, "Nt is a clean-room implementation of the Qt 6 API and is not affiliated with The Qt Company.", FONT("Regular", 13), (110, 150, 140), "lm")
    t.save("nt-social-6-card-1200x630.png")


# ---- 7. Banner 1500x500 ----
def banner():
    t = Tile(1500, 500)
    pad = 90
    t.paste(mark(190), pad, 100)
    x = pad + 190 + 50
    t.text(x, 100, "Nt", FONT("Bold", 120), WHITE)
    t.text(x, 100 + 132, "The Qt 6 API. MIT licensed.", FONT("SemiBold", 48), NEON)
    t.text(x, 100 + 132 + 64, "Clean-room. Linux first. Built for developers.", FONT("Regular", 32), MUTED)
    icon = github_icon(40)
    t.paste(icon, x, 410)
    t.text(x + 40 + 16, 430, "github.com/open-nt/Nt6", FONT("SemiBold", 30), WHITE, "lm")
    page = Image.open(f"{REPO}/doc/images/hello-widgets-nt.png").convert("RGBA")  # the first tab, Nt
    pw_t, ph_t = 330, 400
    page = page.resize((pw_t * S, int(page.height * pw_t / page.width * S)), Image.LANCZOS).crop((0, 0, pw_t * S, ph_t * S))
    mask = Image.new("L", page.size, 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, page.width, page.height + 30 * S), 20 * S, fill=255)
    page.putalpha(mask)
    x = t.w - 110 - pw_t
    t.rrect((x - 3, 100 - 3, x + pw_t + 3, 520), 22, fill=(30, 90, 77))
    t.paste(page, x, 100)
    t.text(30, t.h - 16, "Not affiliated with The Qt Company.", FONT("Regular", 13), (110, 150, 140), "lm")
    t.save("nt-social-7-banner-1500x500.png")


# ---- 8. GitHub social preview 1280x640 ----
def github_preview():
    """The repository's social preview. GitHub asks for 1280x640 and crops the edges in some places, so everything
    that matters stays 80 px inside."""
    t = Tile(1280, 640)
    pad = 80
    t.paste(mark(92), pad, 70)
    t.text(pad + 92 + 22, 70 + 46, "Nt", FONT("Bold", 56), WHITE, "lm")
    t.text(pad, 186, "Write Qt.", FONT("Bold", 108), WHITE)
    t.text(pad, 186 + 104, "Ship MIT.", FONT("Bold", 108), NEON)
    t.wrap(pad, 448, "A small, clean-room implementation of the Qt 6 API, built module by module.",
           FONT("Regular", 27), 560, MUTED, 1.3)
    x = pad
    for label in ("MIT", "C++17", "Linux first"):
        f = FONT("SemiBold", 19)
        w = t.width(label, f) + 36
        t.chip(label, f, x + w, 552, fg=NEON, bg=(22, 72, 61), height=36)
        x += w + 12
    page = Image.open(f"{REPO}/doc/images/hello-widgets-nt.png").convert("RGBA")  # the first tab, Nt
    pw_t, ph_t = 460, 480
    page = page.resize((pw_t * S, int(page.height * pw_t / page.width * S)), Image.LANCZOS).crop((0, 0, pw_t * S, ph_t * S))
    mask = Image.new("L", page.size, 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, page.width, page.height), 20 * S, fill=255)
    page.putalpha(mask)
    x = t.w - pad - pw_t
    y = (t.h - ph_t) / 2
    t.rrect((x - 3, y - 3, x + pw_t + 3, y + ph_t + 3), 22, fill=(30, 90, 77))
    t.paste(page, x, y)
    t.text(pad, t.h - 30, "Nt is a clean-room implementation of the Qt 6 API and is not affiliated with The Qt Company.",
           FONT("Regular", 13), (110, 150, 140), "lm")
    t.save("nt-social-8-github-1280x640.png")


for fn in (hero, code_tile, modules_tile, widgets_tile, contribute_tile, card, banner, github_preview):
    fn()

# a contact sheet for a quick look
names = sorted(n for n in os.listdir(OUT) if n.startswith("nt-social-") and n.endswith(".png"))
sheet = Image.new("RGB", (1620, 1280), (20, 24, 24))
x = y = 20
for n in names[:5]:
    im = Image.open(os.path.join(OUT, n)).convert("RGB").resize((500, 500), Image.LANCZOS)
    sheet.paste(im, (x, y))
    x += 520
    if x > 1100:
        x, y = 20, y + 520
im = Image.open(os.path.join(OUT, names[5])).convert("RGB").resize((540, 283), Image.LANCZOS)
sheet.paste(im, (1060, 560))
im = Image.open(os.path.join(OUT, names[6])).convert("RGB").resize((540, 180), Image.LANCZOS)
sheet.paste(im, (1060, 870))
sheet.save(os.path.join(OUT, "contact-sheet.png"))
