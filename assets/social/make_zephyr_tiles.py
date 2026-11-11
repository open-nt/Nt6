#!/usr/bin/env python3
"""Builds the two Nt on Zephyr social media tiles into the output directory given as argv[1].

See tilelib.py for what it needs.
"""
import sys

from tilelib import *  # noqa: F401,F403
import tilelib

tilelib.set_out(sys.argv[1])
PAD = 80


def zephyr_header(t):
    """The Nt header with the Zephyr logo (for dark backgrounds) at the right instead of the pill."""
    t.paste(mark(84), PAD, PAD - 6)
    t.text(PAD + 84 + 22, PAD + 6, "Nt", FONT("Bold", 44), WHITE)
    t.text(PAD + 84 + 22, PAD + 54, "the Qt 6 API, MIT licensed", FONT("Regular", 20), MUTED)
    h = 90
    logo = svg_to_image(open(f"{HERE}/zephyr-logo-dark.svg").read(), h * 360 / 185 * S)
    t.paste(logo, t.w - PAD - logo.width / S, PAD - 12)


def stat_tile():
    t = Tile(1080, 1080)
    zephyr_header(t)
    big = FONT("Bold", 124)
    t.text(PAD, 262, "Qt APIs.", big, WHITE)
    t.text(PAD, 262 + 132, "Now on Zephyr.", big, NEON)
    t.wrap(PAD, 570, "Nt Core and Nt Network build as a Zephyr module for headless devices: signals and slots, "
           "event loops, timers, TCP and TLS on an RTOS.", FONT("Regular", 34), 880, MUTED, 1.3)
    stats = (("4.5", "Zephyr version"), ("~920 KB", "code, i.MX RT1170"), ("MIT", "licensed"))
    w = (1080 - 2 * PAD - 2 * 24) / 3
    for i, (big_s, small) in enumerate(stats):
        x = PAD + i * (w + 24)
        t.rrect((x, 790, x + w, 930), 20, fill=PINE_DEEP, outline=(30, 90, 77), width=2)
        t.text(x + 26, 812, big_s, FONT("Bold", 50), NEON)
        t.text(x + 26, 880, small, FONT("Regular", 22), MUTED)
    t.footer(PAD)
    t.save("nt-social-9-zephyr-1.png")


def terminal_tile():
    t = Tile(1080, 1080)
    zephyr_header(t)
    t.text(PAD, 250, "From QObject", FONT("Bold", 96), WHITE)
    t.text(PAD, 250 + 100, "to firmware.", FONT("Bold", 96), NEON)
    t.rrect((PAD, 500, t.w - PAD, 910), 22, fill=PINE_DEEP, outline=(30, 90, 77), width=2)
    for i, c in enumerate(((255, 95, 86), (255, 189, 46), (39, 201, 63))):
        t.d.ellipse(((PAD + 28 + i * 30) * S, 526 * S, (PAD + 44 + i * 30) * S, 542 * S), fill=c)
    t.chip("mps2/an385 (QEMU)", FONT("SemiBold", 16), t.w - PAD - 24, 534, padx=14, height=26)
    f = MONO(27)
    lines = [
        ("$ west build -b mps2/an385 \\", (222, 240, 235)),
        ("    examples/zephyr/qemu -t run", (222, 240, 235)),
        ("", None),
        ("CONFIG_NT=y", NEON),
        ("CONFIG_NT_NETWORK=y", NEON),
        ("", None),
        ("Nt sample: PASS", (39, 201, 63)),
    ]
    y = 574
    for s, col in lines:
        if s:
            t.text(PAD + 36, y, s, MONO_B(27) if s.endswith("PASS") else f, col)
        y += 46
    t.footer(PAD)
    t.save("nt-social-10-zephyr-2.png")


stat_tile()
terminal_tile()
