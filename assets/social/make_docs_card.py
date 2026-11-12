#!/usr/bin/env python3
"""Builds the 1200x630 link preview for the documentation site (https://open-nt.github.io/Nt6/) into the output
directory given as argv[1]. doc/ndoc.conf passes it to ndoc as `social_image`, which puts it in the Open Graph and
Twitter card tags of every page.

See tilelib.py for what it needs.
"""
import sys

from tilelib import *  # noqa: F401,F403
import tilelib

tilelib.set_out(sys.argv[1])

LINE = (30, 90, 77)
CODE = (222, 240, 235)
DIM = (110, 150, 140)


def page_mock(t, x, y, w, h):
    """A stylised ndoc reference page for QTimer: header with search, crumbs, title, brief and members."""
    t.rrect((x - 3, y - 3, x + w + 3, y + h + 3), 22, fill=LINE)
    t.rrect((x, y, x + w, y + h), 20, fill=PINE_DEEP)
    # header bar: the mark, the version and the search box
    t.d.rounded_rectangle((x * S, y * S, (x + w) * S, (y + 58) * S), 20 * S, fill=(9, 40, 34), corners=(True, True, False, False))
    t.paste(mark(30), x + 20, y + 14)
    t.text(x + 60, y + 29, "6.0", FONT("SemiBold", 16), MUTED, "lm")
    t.rrect((x + 110, y + 14, x + w - 20, y + 44), 15, fill=PINE, outline=LINE, width=1)
    t.text(x + 128, y + 29, "Search (press /)", FONT("Regular", 15), DIM, "lm")
    # crumbs
    cy = y + 84
    cx = x + 24
    for i, s in enumerate(("Nt", "NtCore", "QTimer")):
        f = FONT("SemiBold" if i == 2 else "Regular", 16)
        t.text(cx, cy, s, f, WHITE if i == 2 else NEON, "lm")
        cx += t.width(s, f)
        if i < 2:
            t.text(cx + 10, cy, "›", FONT("Regular", 16), DIM, "lm")
            cx += 28
    # title and brief
    t.text(x + 24, y + 108, "QTimer Class", FONT("Bold", 38), WHITE)
    t.wrap(x + 24, y + 164, "The QTimer class provides repetitive and single-shot timers.", FONT("Regular", 18), w - 48, MUTED, 1.3)
    t.d.line(((x + 24) * S, (y + 224) * S, (x + w - 24) * S, (y + 224) * S), fill=LINE, width=S)
    t.text(x + 24, y + 240, "Public Functions", FONT("SemiBold", 19), WHITE)
    members = (
        ("void", "start(int msec)"),
        ("void", "stop()"),
        ("bool", "isActive() const"),
        ("int", "interval() const"),
        ("void", "setSingleShot(bool)"),
    )
    my = y + 280
    for ret, sig in members:
        t.text(x + 24, my, ret, MONO(16), DIM)
        name, rest = sig.split("(", 1)
        t.text(x + 84, my, name, MONO_B(16), NEON)
        t.text(x + 84 + t.width(name, MONO_B(16)), my, "(" + rest, MONO(16), CODE)
        my += 30
    t.text(x + 24, my + 12, "Signals", FONT("SemiBold", 19), WHITE)
    t.text(x + 24, my + 50, "void", MONO(16), DIM)
    t.text(x + 84, my + 50, "timeout", MONO_B(16), NEON)
    t.text(x + 84 + t.width("timeout", MONO_B(16)), my + 50, "()", MONO(16), CODE)


def docs_card():
    t = Tile(1200, 630)
    pad = 64
    t.paste(mark(84), pad, 52)
    t.text(pad + 84 + 22, 52 + 4, "Nt", FONT("Bold", 44), WHITE)
    t.text(pad + 84 + 22, 52 + 52, "the Qt 6 API, MIT licensed", FONT("Regular", 20), MUTED)
    t.text(pad, 190, "The Nt 6", FONT("Bold", 92), WHITE)
    t.text(pad, 190 + 94, "Reference.", FONT("Bold", 92), NEON)
    t.wrap(pad, 410, "Every class, function, signal and enum, with guides and examples. Searchable in the browser.",
           FONT("Regular", 24), 540, MUTED, 1.3)
    x = pad
    for label in ("Core", "Network", "Widgets", "Charts", "Qml"):
        f = FONT("SemiBold", 17)
        w = t.width(label, f) + 30
        t.chip(label, f, x + w, 512, fg=NEON, bg=(22, 72, 61), padx=15, height=32)
        x += w + 10
    t.text(pad, 568, "open-nt.github.io/Nt6", FONT("SemiBold", 26), WHITE, "lm")
    page_mock(t, t.w - pad - 470, 56, 470, 510)
    t.text(pad, t.h - 22, "Nt is a clean-room implementation of the Qt 6 API and is not affiliated with The Qt Company.",
           FONT("Regular", 13), DIM, "lm")
    t.save("nt-social-11-docs-1200x630.png")


docs_card()
