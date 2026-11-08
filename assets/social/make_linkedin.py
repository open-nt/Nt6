#!/usr/bin/env python3
"""Builds "Nt Explained", a series of eight LinkedIn posts (1080 x 1350, 4:5), into the directory given as argv[1].

Every post teaches one thing about Nt and ends with an "In short" line. See tilelib.py for what it needs.
"""
import math
import os
import re
import sys

from PIL import Image, ImageDraw

import tilelib
from tilelib import *  # noqa: F401,F403

tilelib.set_out(sys.argv[1])
OUT = tilelib.OUT
W, H, PAD = 1080, 1350, 80
LINE = (30, 90, 77)
TOTAL = 8

CPP_TYPES = {"QObject", "QHttpServer", "QTcpServer", "QString", "QHostAddress", "Counter", "int", "void", "auto"}
KEYWORDS = {"class", "public", "return", "const", "signals", "emit", "Q_OBJECT", "REQUIRED", "PRIVATE"}
CALLS = {"route", "bind", "setValue", "find_package", "target_link_libraries", "nt_enable_automoc", "listen"}


def lesson(n, title_lines, label=None):
    t = Tile(W, H)
    header(t, PAD, label or f"NT EXPLAINED  ·  {n:02d} / {TOTAL:02d}")
    t.text(PAD, 214, f"LESSON {n:02d}", FONT("SemiBold", 26), NEON)
    y = 256
    for i, line in enumerate(title_lines):
        t.text(PAD, y, line, FONT("Bold", 92), NEON if i == len(title_lines) - 1 and len(title_lines) > 1 else WHITE)
        y += 100
    return t, y


def takeaway(t, text):
    top, bottom = 1086, 1186
    t.rrect((PAD, top, W - PAD, bottom), 18, fill=PINE_DEEP)
    t.rrect((PAD, top, PAD + 10, bottom), 5, fill=NEON)
    t.text(PAD + 38, (top + bottom) / 2, "In short", FONT("SemiBold", 22), NEON, "lm")
    f = FONT("SemiBold", 28)
    t.text(PAD + 38 + 120, (top + bottom) / 2, text, f, WHITE, "lm")
    t.footer(PAD)


def bullet(t, x, y, text, size=32, maxw=860, color=WHITE):
    cy = y + size * 0.62
    t.d.polygon([((x + 11 * math.cos(math.radians(60 * i + 30)) + 11) * S, (cy + 11 * math.sin(math.radians(60 * i + 30))) * S) for i in range(6)], fill=NEON)
    return t.wrap(x + 38, y, text, FONT("Regular", size), maxw, color, 1.25)


def pills(t, x, y, items, size=24, gap=12, maxw=W - 2 * PAD, solid=False):
    f = FONT("SemiBold", size)
    cx, cy = x, y
    h = size + 22
    for it in items:
        w = t.width(it, f) + 40
        if cx + w > x + maxw:
            cx, cy = x, cy + h + gap
        if solid:
            t.rrect((cx, cy, cx + w, cy + h), h / 2, fill=NEON)
            t.text(cx + w / 2, cy + h / 2, it, f, PINE, "mm")
        else:
            t.rrect((cx, cy, cx + w, cy + h), h / 2, outline=NEON, width=2)
            t.text(cx + w / 2, cy + h / 2, it, f, WHITE, "mm")
        cx += w + gap
    return cy + h


def code_panel(t, y0, lines, size=31, tag=None, step=1.5):
    f = MONO(size)
    h = 70 + int(len(lines) * size * step) + 24
    t.rrect((PAD, y0, W - PAD, y0 + h), 22, fill=PINE_DEEP, outline=LINE, width=2)
    for i, c in enumerate(((255, 95, 86), (255, 189, 46), (39, 201, 63))):
        t.d.ellipse(((PAD + 28 + i * 30) * S, (y0 + 26) * S, (PAD + 44 + i * 30) * S, (y0 + 42) * S), fill=c)
    if tag:
        t.chip(tag, FONT("SemiBold", 16), W - PAD - 24, y0 + 34, padx=14, height=26)
    y = y0 + 70
    for l in lines:
        x = PAD + 36
        for tok in re.findall(r'"[^"]*"|//.*|#.*|\w+|\s+|.', l):
            col = (222, 240, 235)
            if tok.startswith('"'):
                col = (255, 214, 140)
            elif tok.startswith("//") or tok.startswith("#"):
                col = (110, 160, 148)
            elif tok in KEYWORDS:
                col = (200, 165, 255)
            elif tok in CPP_TYPES:
                col = NEON
            elif tok in CALLS:
                col = (140, 200, 255)
            t.text(x, y, tok, f, col)
            x += t.width(tok, f)
        y += size * step
    return y0 + h


# 1 ---------------------------------------------------------------
def p1():
    t, y = lesson(1, ["What is Nt?"])
    y = t.wrap(PAD, 390, 'Nt ("Nut") is a small, clean-room implementation of the Qt 6 API.', FONT("SemiBold", 54), W - 2 * PAD, WHITE, 1.2)
    y = t.wrap(PAD, y + 28, "You write the Qt code you already know: QObject, signals and slots, QString, QTimer, sockets, widgets. It builds with CMake and a C++17 compiler.",
               FONT("Regular", 36), W - 2 * PAD, MUTED, 1.35)
    y = pills(t, PAD, y + 40, ["Core", "Network", "Serial Port", "D-Bus", "HTTP Server", "WebSockets", "GUI", "Widgets", "Test"], size=26)
    gap = 20
    cw = (W - 2 * PAD - 2 * gap) / 3
    for i, (big, small) in enumerate((("9", "modules"), ("C++17", "to build it"), ("MIT", "license"))):
        x = PAD + i * (cw + gap)
        t.rrect((x, y + 44, x + cw, y + 44 + 150), 20, fill=PINE_DEEP, outline=LINE, width=2)
        t.text(x + cw / 2, y + 44 + 64, big, FONT("Bold", 64), NEON, "mm")
        t.text(x + cw / 2, y + 44 + 118, small, FONT("Regular", 26), MUTED, "mm")
    takeaway(t, "The Qt 6 API, module by module, under the MIT license.")
    t.save("nt-linkedin-1-what-is-nt.png")


# 2 ---------------------------------------------------------------
def p2():
    t, y = lesson(2, ["Clean-room.", "MIT."])
    blocks = [("Clean-room", "Nt is written without copying Qt's source code. It follows the public Qt 6 API, so your code keeps its shape."),
              ("MIT licensed", "A short, permissive license. Use, change and ship Nt in open or closed source products; keep the license notice."),
              ("Independent", "Nt is not affiliated with The Qt Company, and its name and logo are never to be mistaken for Qt's.")]
    y = 505
    for title, body in blocks:
        t.rrect((PAD, y, W - PAD, y + 168), 18, fill=PINE_DEEP, outline=LINE, width=2)
        t.text(PAD + 30, y + 22, title, FONT("Bold", 36), NEON)
        t.wrap(PAD + 30, y + 72, body, FONT("Regular", 27), W - 2 * PAD - 60, WHITE, 1.25)
        y += 168 + 20
    takeaway(t, "A familiar API, independent code, a permissive license.")
    t.save("nt-linkedin-2-clean-room-mit.png")


# 3 ---------------------------------------------------------------
def p3():
    t, y = lesson(3, ["Signals and slots,", "no moc."])
    y = code_panel(t, 510, [
        "class Counter : public QObject {",
        "    Q_OBJECT",
        "public:",
        "    void setValue(int v) { emit valueChanged(v); }",
        "signals:",
        "    void valueChanged(int value);  // ntmoc",
        "};",
    ], size=28, tag="counter.h")
    y = bullet(t, PAD, y + 30, "ntmoc, a small MIT tool, generates the signal bodies.", 30)
    y = bullet(t, PAD, y + 12, "signals:, slots:, emit and pointer-to-member connect() work as in Qt.", 30)
    takeaway(t, "Q_OBJECT, signals, slots and emit work as you know them.")
    t.save("nt-linkedin-3-signals-slots.png")


# 4 ---------------------------------------------------------------
def p4():
    t, y = lesson(4, ["Nine modules,", "built one by one."])
    mods = [("Core", True), ("Network", True), ("Serial Port", False), ("D-Bus", False), ("HTTP Server", False),
            ("WebSockets", False), ("GUI", True), ("Widgets", True), ("Test", False)]
    gx, gy, gap = PAD, 520, 20
    cw = (W - 2 * PAD - 2 * gap) / 3
    ch = 128
    for i, (name, partial) in enumerate(mods):
        x = gx + (i % 3) * (cw + gap)
        yy = gy + (i // 3) * (ch + gap)
        t.rrect((x, yy, x + cw, yy + ch), 18, fill=PINE_DEEP, outline=LINE, width=2)
        t.paste(mark(36), x + 22, yy + 22)
        t.text(x + 22, yy + 74, "Nt " + name, FONT("SemiBold", 30), WHITE)
        if partial:
            t.chip("partial", FONT("SemiBold", 15), x + cw - 18, yy + 40, fg=NEON, bg=(22, 70, 59), padx=12, height=24)
    t.wrap(PAD, gy + 3 * ch + 2 * gap + 30, "Partial means the classes Nt needs first are there and the rest is coming. Every class lists what differs from Qt.",
           FONT("Regular", 28), W - 2 * PAD, MUTED, 1.3)
    takeaway(t, "Each class documents exactly where it differs from Qt.")
    t.save("nt-linkedin-4-modules.png")


# 5 ---------------------------------------------------------------
def p5():
    t, y = lesson(5, ["Three lines", "of CMake."])
    y = code_panel(t, 510, [
        "find_package(Nt6 6 REQUIRED)",
        "target_link_libraries(app PRIVATE Nt6::Network)",
        "nt_enable_automoc(app)",
    ], size=29, tag="CMakeLists.txt")
    y = bullet(t, PAD, y + 44, "Nt:: and Qt6:: target names both work.", 34)
    y = bullet(t, PAD, y + 22, "Headers install to /usr/include/nt6, so Nt can sit next to Qt.", 34)
    y = bullet(t, PAD, y + 22, "Packages for Ubuntu, Enterprise Linux, Arch and snap.", 34)
    takeaway(t, "Find the package, link a module, and you are building.")
    t.save("nt-linkedin-5-cmake.png")


# 6 ---------------------------------------------------------------
def p6():
    t, y = lesson(6, ["A web service", "in six lines."])
    y = code_panel(t, 510, [
        "QHttpServer server;",
        'server.route("/hello/<arg>",',
        "    [](const QString &name) {",
        '        return "Hello, " + name;',
        "    });",
        "server.bind(tcp);  // a QTcpServer",
    ], size=29, tag="Nt::HttpServer")
    y = bullet(t, PAD, y + 44, "Typed routes: <arg> arrives as a QString, an int or a double.", 34)
    y = bullet(t, PAD, y + 22, "Serves TCP, TLS (HTTPS) and local sockets.", 34)
    takeaway(t, "QHttpServer and QTcpServer, the way Qt has them.")
    t.save("nt-linkedin-6-web-service.png")


# 7 ---------------------------------------------------------------
def p7():
    t, y = lesson(7, ["Widgets,", "the Qt way."])
    shot = Image.open(f"{tilelib.REPO}/doc/images/hello-widgets-window.png").convert("RGBA")  # the first tab, Nt, dark and light
    half = shot.width // 2
    gap = 22
    w = (W - 2 * PAD - gap) // 2
    top, room = 570, 1062 - 570
    for i in range(2):
        page = shot.crop((i * half, 0, (i + 1) * half, shot.height))
        page = page.resize((w * S, int(page.height * w / page.width * S)), Image.LANCZOS).crop((0, 0, w * S, room * S))
        mask = Image.new("L", page.size, 0)
        ImageDraw.Draw(mask).rounded_rectangle((0, 0, page.width, page.height + 30 * S), 20 * S, fill=255)
        page.putalpha(mask)
        x = PAD + i * (w + gap)
        t.rrect((x - 3, top - 3, x + w + 3, top + room + 3), 22, fill=LINE)
        t.paste(page, x, top)
    t.text(PAD, top - 40, "hello_widgets: one of every widget, live", FONT("Regular", 26), MUTED, "lm")
    takeaway(t, "Dark or light, following your desktop theme.")
    t.save("nt-linkedin-7-widgets.png")


# 8 ---------------------------------------------------------------
def p8():
    t = Tile(W, H)
    dev = svg_to_image(open(f"{tilelib.REPO}/assets/brand/nt-development-logo-neon.svg").read(), 600 * S)
    t.paste(dev, PAD - 34, PAD - 48)
    t.chip(f"NT EXPLAINED  ·  {TOTAL:02d} / {TOTAL:02d}", FONT("SemiBold", 18), W - PAD, PAD + 36)
    t.text(PAD, 214, f"LESSON {TOTAL:02d}", FONT("SemiBold", 26), NEON)
    t.text(PAD, 256, "Trust it.", FONT("Bold", 92), WHITE)
    t.text(PAD, 352, "Improve it.", FONT("Bold", 92), NEON)
    t.text(PAD, 500, "How Nt earns trust", FONT("SemiBold", 36), MUTED)
    y = 562
    for it in ["Every commit passes ./check.sh", "-Werror, clang-tidy and cppcheck", "ASan, UBSan and TSan in CI",
               "Built on Ubuntu 22.04 to 26.04, EL 9 and 10, Arch"]:
        y = bullet(t, PAD, y, it, 34) + 22
    # the call to action
    top = 905
    t.rrect((PAD, top, W - PAD, top + 130), 26, fill=NEON)
    icon = github_icon(64, PINE)
    t.paste(icon, PAD + 36, top + 33)
    t.text(PAD + 36 + 64 + 28, top + 65, "Star it. Fork it. Send a PR.", FONT("Bold", 44), PINE, "lm")
    takeaway(t, "Open source and welcoming. Pick an issue and say hello.")
    t.save("nt-linkedin-8-contribute.png")


for fn in (p1, p2, p3, p4, p5, p6, p7, p8):
    fn()

# a contact sheet to look at the whole series
names = sorted(n for n in os.listdir(OUT) if n.startswith("nt-linkedin-") and n.endswith(".png"))
cell_w, cell_h = 405, 506
sheet = Image.new("RGB", (4 * cell_w + 5 * 20, 2 * cell_h + 3 * 20), (20, 24, 24))
for i, n in enumerate(names[:8]):
    im = Image.open(os.path.join(OUT, n)).convert("RGB").resize((cell_w, cell_h), Image.LANCZOS)
    sheet.paste(im, (20 + (i % 4) * (cell_w + 20), 20 + (i // 4) * (cell_h + 20)))
sheet.save(os.path.join(OUT, "contact-sheet.png"))
