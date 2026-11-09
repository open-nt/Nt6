#!/usr/bin/env python3
"""Builds the Snap Store images of Nt into the output directory given as argv[1] (default: this directory).

The Snap Store accepts GIF, JPEG and PNG files of 480 x 480 to 3840 x 2160 pixels, with an aspect ratio between
1:2 and 2:1, at most 2 MB each. An animation runs at 1 to 30 fps for at most 40 seconds. The script checks all of
this before it finishes and exits non-zero if an image does not comply. The banner is a JPEG or PNG file of
720 x 240 to 4320 x 1440 pixels at exactly 3:1, at most 2 MB.

Needs Pillow, rsvg-convert and the Titillium Web fonts in TITILLIUM_DIR; see ../social/tilelib.py.
The widget screenshot comes from doc/images.
"""
import math
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "social"))

from PIL import Image, ImageDraw  # noqa: E402

import tilelib  # noqa: E402
from tilelib import FONT, MONO, MUTED, NEON, PINE_DEEP, REPO, S, WHITE, Tile, github_icon, mark  # noqa: E402

tilelib.set_out(sys.argv[1] if len(sys.argv) > 1 else HERE)
OUT = tilelib.OUT

W, H = 1920, 1080
EDGE = (30, 90, 77)
CODE = (222, 240, 235)
STRING = (255, 214, 140)
COMMENT = (110, 160, 148)
CALL = (140, 200, 255)
DOTS = ((255, 95, 86), (255, 189, 46), (39, 201, 63))

KEYWORDS = {"class", "public", "private", "signals", "slots", "const", "int", "void", "include", "pragma", "once",
            "find_package", "target_link_libraries", "add_executable", "REQUIRED", "PRIVATE"}
MACROS = {"Q_OBJECT", "Q_PROPERTY", "QObject", "READ", "WRITE", "NOTIFY", "Counter"}

MAX_BYTES = 2 * 1024 * 1024


def header(t, pad=110, label="FOR DEVELOPERS"):
    t.paste(mark(92), pad, pad - 16)
    t.text(pad + 92 + 24, pad - 2, "Nt", FONT("Bold", 48), WHITE)
    t.text(pad + 92 + 24, pad + 50, "open-nt6 on the Snap Store", FONT("Regular", 22), MUTED)
    t.chip(label, FONT("SemiBold", 20), t.w - pad, pad + 30)


def footer(t, pad=110):
    y = t.h - 70
    t.paste(github_icon(40), pad, y - 20)
    t.text(pad + 56, y, "github.com/open-nt/Nt6", FONT("SemiBold", 30), WHITE, "lm")
    t.text(t.w - pad, y, "MIT  ·  C++17  ·  Linux first", FONT("Regular", 24), MUTED, "rm")
    t.text(t.w / 2, t.h - 22, "Nt is a clean-room implementation of the Qt 6 API and is not affiliated with The Qt Company.",
           FONT("Regular", 14), (110, 150, 140), "mm")


def title(t, x, y, white, neon, size):
    t.text(x, y, white, FONT("Bold", size), WHITE)
    t.text(x, y + size * 1.08, neon, FONT("Bold", size), NEON)


def tokens(line):
    return re.findall(r'"[^"]*"|<[\w/.]+>|//.*|# .*|/\*!|\*/|\w+|\s+|.', line)


def token_color(tok):
    if tok.startswith('"') or (tok.startswith("<") and tok.endswith(">") and len(tok) > 2):
        return STRING
    if tok.startswith("//") or tok.startswith("# "):
        return COMMENT
    if tok in MACROS or tok == "$":
        return NEON
    if tok in KEYWORDS:
        return CALL
    return CODE


def window(t, box, caption, radius=24):
    x0, y0, x1, y1 = box
    t.rrect(box, radius, fill=PINE_DEEP, outline=EDGE, width=2)
    for i, c in enumerate(DOTS):
        t.d.ellipse(((x0 + 30 + i * 34) * S, (y0 + 26) * S, (x0 + 48 + i * 34) * S, (y0 + 44) * S), fill=c)
    t.text(x1 - 30, y0 + 35, caption, FONT("SemiBold", 20), MUTED, "rm")


def code_lines(t, x, y, lines, size, step=1.5):
    f = MONO(size)
    for line in lines:
        cx = x
        for tok in tokens(line):
            t.text(cx, y, tok, f, token_color(tok))
            cx += t.width(tok, f)
        y += size * step
    return y


def rule(t, y):
    t.d.line((870 * S, y * S, 1800 * S, y * S), fill=EDGE, width=2 * S)


def screenshot(t, name, x, y, w, h):
    shot = Image.open(f"{REPO}/doc/images/{name}").convert("RGBA")
    shot = shot.resize((w * S, int(shot.height * w / shot.width * S)), Image.LANCZOS)
    h = min(h, shot.height // S)
    shot = shot.crop((0, 0, w * S, h * S))
    mask = Image.new("L", shot.size, 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, shot.width, shot.height + 40 * S), 26 * S, fill=255)
    shot.putalpha(mask)
    t.rrect((x - 3, y - 3, x + w + 3, y + h + 3), 28, fill=EDGE)
    t.paste(shot, x, y)


def bullets(t, x, y, items, size, step):
    for it in items:
        cy = y + size * 0.62
        pts = [((x + 14 + 14 * math.cos(math.radians(60 * i + 30))) * S, (cy + 14 * math.sin(math.radians(60 * i + 30))) * S) for i in range(6)]
        t.d.polygon(pts, fill=NEON)
        t.text(x + 46, y, it, FONT("Regular", size), WHITE)
        y += step


def overview():
    t = Tile(W, H)
    pad = 110
    header(t, pad)
    title(t, pad, 250, "Write Qt.", "Ship MIT.", 150)
    t.wrap(pad, 610, "Nt is a small, clean-room implementation of the Qt 6 API. This snap brings ntmoc, ndoc, the headers and the CMake package.",
           FONT("Regular", 38), 800, MUTED, 1.3)
    x = pad
    f = FONT("SemiBold", 26)
    for label in ("ntmoc", "ndoc", "CMake package"):
        right = x + t.width(label, f) + 44
        t.chip(label, f, right, 830, padx=22, height=52)
        x = right + 18
    screenshot(t, "hello-widgets-window.png", 1010, 280, 800, 760)
    footer(t, pad)
    t.save("nt-snap-1-overview.png")


def tool_slide(white, neon, text, caption, lines, command, name):
    t = Tile(W, H)
    pad = 110
    header(t, pad)
    title(t, pad, 250, white, neon, 118)
    t.wrap(pad, 560, text, FONT("Regular", 36), 640, MUTED, 1.3)
    window(t, (860, 190, 1810, 960), caption)
    y = code_lines(t, 900, 270, lines, 29, 1.42)
    rule(t, y + 14)
    code_lines(t, 900, y + 40, command, 29, 1.42)
    footer(t, pad)
    t.save(name)


def ntmoc():
    tool_slide("A moc for", "Nt classes.",
               "ntmoc reads Q_OBJECT and Q_GADGET headers and writes the meta-object, with signals, slots and properties as in Qt.",
               "counter.h", [
                   "#pragma once",
                   "#include <QtCore/QObject>",
                   "",
                   "class Counter : public QObject {",
                   "    Q_OBJECT",
                   "    Q_PROPERTY(int value READ value",
                   "        WRITE setValue NOTIFY valueChanged)",
                   "public:",
                   "    int value() const;",
                   "public slots:",
                   "    void setValue(int v);",
                   "signals:",
                   "    void valueChanged(int v);",
                   "};",
               ], ["$ ntmoc -o moc_counter.cpp counter.h"], "nt-snap-2-ntmoc.png")


def ndoc():
    tool_slide("Docs from", "comments.",
               "ndoc turns /*! */ comments into HTML reference pages, and fails the build when public API is left undocumented.",
               "counter.cpp", [
                   "/*!",
                   "    \\class Counter",
                   "    \\brief Counts things.",
                   "",
                   "    \\differences",
                   "    There is no undo stack.",
                   "    \\enddifferences",
                   "*/",
                   "",
                   "/*!",
                   "    Sets the value to \\a v.",
                   "",
                   "    \\sa value()",
                   "*/",
               ], ["$ ndoc --strict --require-complete \\", "       doc/ndoc.conf"], "nt-snap-3-ndoc.png")


def cmake():
    t = Tile(W, H)
    pad = 110
    header(t, pad)
    title(t, pad, 250, "Build against", "the snap.", 118)
    bullets(t, pad, 580, ["Headers and static libraries", "A CMake package, Nt6", "amd64 and arm64"], 38, 72)
    window(t, (860, 190, 1810, 960), "terminal")
    y = code_lines(t, 900, 270, [
        "$ sudo snap install open-nt6",
        "",
        "$ cmake -S . -B build \\",
        "    -DCMAKE_PREFIX_PATH=\\",
        "      /snap/open-nt6/current/usr",
    ], 30)
    rule(t, y + 14)
    code_lines(t, 900, y + 40, [
        "# CMakeLists.txt",
        "find_package(Nt6 6 REQUIRED)",
        "add_executable(app main.cpp)",
        "target_link_libraries(app PRIVATE",
        "    Nt6::Network)",
    ], 30)
    footer(t, pad)
    t.save("nt-snap-4-cmake.png")


def modules():
    t = Tile(W, H)
    pad = 110
    header(t, pad)
    title(t, pad, 220, "Nine modules.", "One familiar API.", 108)
    mods = [("Core", True), ("Network", True), ("Serial Port", False), ("D-Bus", False), ("HTTP Server", False),
            ("WebSockets", False), ("GUI", True), ("Widgets", True), ("Test", False)]
    gap = 24
    cw = (t.w - 2 * pad - 2 * gap) / 3
    ch = 110
    for i, (name, partial) in enumerate(mods):
        x = pad + (i % 3) * (cw + gap)
        y = 520 + (i // 3) * (ch + gap)
        t.rrect((x, y, x + cw, y + ch), 20, fill=PINE_DEEP, outline=EDGE, width=2)
        t.paste(mark(52), x + 26, y + 34)
        t.text(x + 98, y + ch / 2, "Nt " + name, FONT("SemiBold", 38), WHITE, "lm")
        if partial:
            t.text(x + cw - 26, y + ch / 2, "partial", FONT("Regular", 22), MUTED, "rm")
    footer(t, pad)
    t.save("nt-snap-5-modules.png")


def banner():
    t = Tile(2160, 720)  # 3:1; the store crops it to the page header, so the content stays clear of the edges
    pad = 150
    t.paste(mark(300), pad, 150)
    x = pad + 300 + 70
    t.text(x, 130, "Nt", FONT("Bold", 190), WHITE)
    t.text(x, 130 + 205, "The Qt 6 API. MIT licensed.", FONT("SemiBold", 72), NEON)
    t.text(x, 130 + 205 + 92, "ntmoc, ndoc, headers and CMake package.", FONT("Regular", 46), MUTED)
    t.paste(github_icon(52), x, 600)
    t.text(x + 70, 626, "github.com/open-nt/Nt6", FONT("SemiBold", 40), WHITE, "lm")
    pw, ph = 600, 520
    page = Image.open(f"{REPO}/doc/images/hello-widgets-nt.png").convert("RGBA")  # the first tab, Nt
    page = page.resize((pw * S, int(page.height * pw / page.width * S)), Image.LANCZOS).crop((0, 0, pw * S, ph * S))
    mask = Image.new("L", page.size, 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, page.width, page.height), 26 * S, fill=255)
    page.putalpha(mask)
    px = t.w - pad - pw
    t.rrect((px - 3, 100 - 3, px + pw + 3, 100 + ph + 3), 28, fill=EDGE)
    t.paste(page, px, 100)
    t.text(t.w / 2, t.h - 26, "Nt is a clean-room implementation of the Qt 6 API and is not affiliated with The Qt Company.",
           FONT("Regular", 18), (110, 150, 140), "mm")
    t.save("nt-snap-banner.png")


# ---- animation: a terminal session typed out ----
GW, GH = 1280, 720
FPS = 10
LINE_H = 38
FIRST_Y = 190


def terminal_frame(base, lines, cursor):
    img = base.copy()
    d = ImageDraw.Draw(img)
    f = MONO(14)  # tilelib sizes fonts for its 2x canvas; this frame is drawn at 1x
    x, y = 100, FIRST_Y
    for line in lines:
        x = 100
        for tok in tokens(line):
            d.text((x, y), tok, font=f, fill=token_color(tok))
            x += d.textlength(tok, font=f)
        y += LINE_H
    if cursor:
        y -= LINE_H
        d.rectangle((x + 3, y + 3, x + 17, y + 31), fill=NEON)
    return img


def animation():
    t = Tile(GW, GH)
    t.paste(mark(64), 60, 40)
    t.text(60 + 64 + 18, 44, "Nt", FONT("Bold", 36), WHITE)
    t.text(60 + 64 + 18, 88, "open-nt6 on the Snap Store", FONT("Regular", 18), MUTED)
    t.chip("ntmoc  ·  ndoc", FONT("SemiBold", 16), GW - 60, 72, padx=16, height=34)
    window(t, (50, 140, GW - 50, GH - 80), "terminal", 20)
    t.text(GW / 2, GH - 38, "github.com/open-nt/Nt6   ·   MIT   ·   Not affiliated with The Qt Company", FONT("Regular", 16), MUTED, "mm")
    base = t.img.convert("RGB").resize((GW, GH), Image.LANCZOS)

    script = [  # (text, typed)
        ("$ sudo snap install open-nt6", True),
        ("$ head -n 5 counter.h", True),
        ("#pragma once", False),
        ("#include <QtCore/QObject>", False),
        ("", False),
        ("class Counter : public QObject {", False),
        ("    Q_OBJECT", False),
        ("$ ntmoc -o moc_counter.cpp counter.h", True),
        ("$ head -n 2 moc_counter.cpp", True),
        ("// Generated by ntmoc. Do not edit.", False),
        ('#include "counter.h"', False),
    ]
    frames, seconds = [], []
    shown = []
    for text, typed in script:
        if not typed:
            shown.append(text)
            frames.append(terminal_frame(base, shown, True))
            seconds.append(0.1)
            continue
        for n in range(1, len(text) + 1):
            frames.append(terminal_frame(base, shown + [text[:n]], True))
            seconds.append(0.08)
        shown.append(text)
        frames.append(terminal_frame(base, shown, True))
        seconds.append(0.9)
    for i in range(8):  # blink the cursor before the loop restarts
        frames.append(terminal_frame(base, shown, i % 2 == 0))
        seconds.append(0.5)
    save_gif(frames, seconds, "nt-snap-demo.gif")


def save_gif(frames, seconds, name):
    out, ms = [], []
    for fr, s in zip(frames, seconds):
        d = max(round(s * FPS), 1) * 1000 // FPS  # whole ticks of 1/FPS s
        if out and fr.tobytes() == out[-1].tobytes() and ms[-1] + d <= 1000:  # a frame held longer than 1 s is below 1 fps
            ms[-1] += d
        else:
            out.append(fr)
            ms.append(d)
    pal = [f.convert("P", palette=Image.ADAPTIVE, colors=128) for f in out]
    pal[0].save(os.path.join(OUT, name), save_all=True, append_images=pal[1:], duration=ms, loop=0, optimize=True)


def check():
    ok = True
    for n in sorted(os.listdir(OUT)):
        if not n.startswith("nt-snap-") or n.rsplit(".", 1)[-1] not in ("png", "gif", "jpg", "jpeg"):
            continue
        path = os.path.join(OUT, n)
        im = Image.open(path)
        w, h = im.size
        size = os.path.getsize(path)
        problems = []
        if "banner" in n:  # JPEG or PNG, 720 x 240 to 4320 x 1440, exactly 3:1
            if im.format not in ("PNG", "JPEG"):
                problems.append("format")
            if w < 720 or h < 240 or w > 4320 or h > 1440:
                problems.append("resolution")
            if w != 3 * h:
                problems.append("aspect ratio")
        else:
            if im.format not in ("PNG", "GIF", "JPEG"):
                problems.append("format")
            if w < 480 or h < 480 or w > 3840 or h > 2160:
                problems.append("resolution")
            if not 0.5 <= w / h <= 2:
                problems.append("aspect ratio")
        if size > MAX_BYTES:
            problems.append("size")
        info = f"{n}: {im.format} {w}x{h} ratio {w / h:.2f} {size / 1024:.0f} KB"
        if getattr(im, "n_frames", 1) > 1:
            delays = []
            for i in range(im.n_frames):
                im.seek(i)
                delays.append(im.info["duration"])
            total = sum(delays) / 1000
            # the lowest rate is the longest-held frame, the highest the shortest
            info += f", {im.n_frames} frames, {total:.1f} s, {1000 / max(delays):.1f} to {1000 / min(delays):.1f} fps"
            if total > 40:
                problems.append("length")
            if 1000 / min(delays) > 30:
                problems.append("fps above 30")
            if 1000 / max(delays) < 1:
                problems.append("fps below 1")
        print(info, "OK" if not problems else "FAIL: " + ", ".join(problems))
        ok = ok and not problems
    return ok


for fn in (overview, ntmoc, ndoc, cmake, modules, banner, animation):
    fn()

sys.exit(0 if check() else 1)
