#!/usr/bin/env python3
"""Builds "Nt on Zephyr", a series of five LinkedIn posts (1080 x 1350, 4:5), into the directory given as argv[1].

It reuses the helpers of make_linkedin.py (everything above its first post), so the two series look alike.
Needs Pillow, rsvg-convert and the Titillium Web fonts (see tilelib.py).
"""
import sys

_src = open(__import__("os").path.join(__import__("os").path.dirname(__import__("os").path.abspath(__file__)), "make_linkedin.py")).read()
exec(compile(_src.split("# 1 ----")[0], "make_linkedin.py", "exec"))  # noqa: S102

TOTAL = 5
CALLS |= {"CONFIG_NT", "CONFIG_NT_NETWORK", "CONFIG_NT_TLS", "CONFIG_CPP", "CONFIG_STD_CPP17", "CONNECT", "connectToHostEncrypted",
          "setCaCertificates", "setSslConfiguration"}
CPP_TYPES |= {"QSslConfiguration", "QSslSocket", "QSslCertificate"}
SERIES = "NT ON ZEPHYR"


def zlesson(n, title_lines):
    t = Tile(W, H)
    t.paste(mark(84), PAD, PAD - 6)
    t.text(PAD + 84 + 22, PAD + 6, "Nt", FONT("Bold", 44), WHITE)
    t.text(PAD + 84 + 22, PAD + 54, "the Qt 6 API, MIT licensed", FONT("Regular", 20), MUTED)
    logo = svg_to_image(open(f"{HERE}/zephyr-logo-dark.svg").read(), 90 * 360 / 185 * S)
    t.paste(logo, W - PAD - logo.width / S, PAD - 12)
    t.text(PAD, 214, f"{SERIES}  ·  {n:02d} / {TOTAL:02d}", FONT("SemiBold", 26), NEON)
    y = 256
    for i, line in enumerate(title_lines):
        t.text(PAD, y, line, FONT("Bold", 92), NEON if i == len(title_lines) - 1 and len(title_lines) > 1 else WHITE)
        y += 100
    return t, y


def save(t, name):
    t.save(name)


# 1 ---------------------------------------------------------------
def z1():
    t, y = zlesson(1, ["Qt code,", "on an RTOS."])
    y = t.wrap(PAD, 500, "Nt Core and Nt Network now build as a Zephyr module, for headless devices with megabytes of flash and RAM.",
               FONT("SemiBold", 44), W - 2 * PAD, WHITE, 1.25)
    y = t.wrap(PAD, y + 24, "QObject, signals and slots, the event loop, timers, threads, JSON and TCP sockets: the Qt 6 API you know, MIT licensed.",
               FONT("Regular", 32), W - 2 * PAD, MUTED, 1.35)
    gap = 20
    cw = (W - 2 * PAD - 2 * gap) / 3
    for i, (big, small) in enumerate((("4.5", "Zephyr"), ("~920 KB", "RT1170 build"), ("2", "modules"))):
        x = PAD + i * (cw + gap)
        top = y + 50
        t.rrect((x, top, x + cw, top + 150), 20, fill=PINE_DEEP, outline=LINE, width=2)
        t.text(x + cw / 2, top + 64, big, FONT("Bold", 56), NEON, "mm")
        t.text(x + cw / 2, top + 118, small, FONT("Regular", 26), MUTED, "mm")
    takeaway(t, "Core and Network, cross built as a Zephyr module.")
    save(t, "nt-zephyr-1-qt-on-an-rtos.png")


# 2 ---------------------------------------------------------------
def z2():
    t, y = zlesson(2, ["Turn it on", "in prj.conf."])
    y = code_panel(t, 510, [
        "CONFIG_CPP=y",
        "CONFIG_STD_CPP17=y",
        "CONFIG_NT=y",
        "CONFIG_NT_NETWORK=y",
    ], size=30, tag="prj.conf")
    y = bullet(t, PAD, y + 40, "Add Nt to the west manifest, or to ZEPHYR_EXTRA_MODULES.", 32)
    y = bullet(t, PAD, y + 20, "Link the app with Nt::Core and Nt::Network.", 32)
    y = bullet(t, PAD, y + 20, "ntmoc and ntrcc are built for the host automatically.", 32)
    takeaway(t, "Nt is a Zephyr module: a few Kconfig lines and you build.")
    save(t, "nt-zephyr-2-prj-conf.png")


# 3 ---------------------------------------------------------------
def z3():
    t, y = zlesson(3, ["Try it in QEMU.", "No board."])
    top = 510
    rows = ["JSON round trip", "QTcpServer::listen", "QTimer ticks", "TCP echo over loopback", "QThread ran", "queued call from the thread"]
    h = 70 + 50 + len(rows) * 44 + 70
    t.rrect((PAD, top, W - PAD, top + h), 22, fill=PINE_DEEP, outline=LINE, width=2)
    for i, c in enumerate(((255, 95, 86), (255, 189, 46), (39, 201, 63))):
        t.d.ellipse(((PAD + 28 + i * 30) * S, (top + 26) * S, (PAD + 44 + i * 30) * S, (top + 42) * S), fill=c)
    t.chip("mps2/an385", FONT("SemiBold", 16), W - PAD - 24, top + 34, padx=14, height=26)
    f, fb = MONO(26), MONO_B(26)
    yy = top + 70
    t.text(PAD + 36, yy, "Nt 6.0.0 on Zephyr", f, (222, 240, 235))
    yy += 50
    for r in rows:
        t.text(PAD + 52, yy, r, f, (222, 240, 235))
        t.text(W - PAD - 36, yy, "ok", fb, (39, 201, 63), "ra")
        yy += 44
    t.text(PAD + 36, yy + 20, "Nt sample: PASS", fb, (39, 201, 63))
    y = top + h
    t.text(PAD, y + 34, "west build -b mps2/an385 examples/zephyr/qemu -t run", MONO(23), MUTED)
    takeaway(t, "A signal, a socket and a thread, checked on every build.")
    save(t, "nt-zephyr-3-qemu.png")


# 4 ---------------------------------------------------------------
def z4():
    t, y = zlesson(4, ["Sockets and", "TLS, the Qt way."])
    y = code_panel(t, 510, [
        "QSslConfiguration configuration;",
        "configuration.setCaCertificates(pem);",
        "QSslSocket socket;",
        "socket.setSslConfiguration(configuration);",
        'socket.connectToHostEncrypted("example.com", 443);',
    ], size=24, tag="CONFIG_NT_TLS=y", step=1.55)
    y = bullet(t, PAD, y + 34, "TLS 1.2 on Mbed TLS 4 and PSA cryptography.", 30)
    y = bullet(t, PAD, y + 16, "Name the certificates to trust: Zephyr has no CA store.", 30)
    y = bullet(t, PAD, y + 16, "Give the board the time of day, so dates can be checked.", 30)
    takeaway(t, "QTcpSocket, QTcpServer and QSslSocket on Zephyr's stack.")
    save(t, "nt-zephyr-4-tls.png")


# 5 ---------------------------------------------------------------
def z5():
    t, y = zlesson(5, ["Early days.", "Help shape it."])
    t.text(PAD, 500, "What to know before you start", FONT("SemiBold", 36), MUTED)
    y = 562
    for it in ["Run in QEMU and built for the i.MX RT1170; not yet run on a real board.",
               "Left out: QProcess, QLibrary, QSharedMemory, local sockets.",
               "Keep to one thread where you can; sharing objects between threads is not safe yet.",
               "Files need absolute paths; local time is UTC."]:
        y = bullet(t, PAD, y, it, 30, maxw=W - 2 * PAD - 40) + 18
    top = 930
    t.rrect((PAD, top, W - PAD, top + 110), 26, fill=NEON)
    icon = github_icon(54, PINE)
    t.paste(icon, PAD + 36, top + 28)
    t.text(PAD + 36 + 54 + 26, top + 55, "Try it on your board. Tell us.", FONT("Bold", 40), PINE, "lm")
    takeaway(t, "An honest first step. Boards and bug reports welcome.")
    save(t, "nt-zephyr-5-early-days.png")


for fn in (z1, z2, z3, z4, z5):
    fn()
