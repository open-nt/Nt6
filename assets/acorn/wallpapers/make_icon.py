#!/usr/bin/env python3
# MIT License - see LICENSE
"""Makes the Acorn icon set from the crown: an SVG and PNGs at the usual sizes, with fewer scales as the size drops so
that it stays legible, in ../icon/.

    assets/acorn/wallpapers/make_icon.py
"""
import os
import subprocess

import make_wallpaper as mw

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "icon")
SIZES = (16, 32, 48, 64, 128, 256, 512, 1024)


def cells_for(px):
    return 19 if px <= 32 else 61 if px <= 64 else 190


def icon_svg(cells, background=True):
    side = 512
    body = mw.crown(side / 2, side / 2, side * 0.78, cells=cells, glow=False)
    bg = f'<rect width="{side}" height="{side}" rx="{side * 0.22:.0f}" fill="{mw.PINE}"/>' if background else ""
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {side} {side}" width="{side}" height="{side}">\n'
            f'<title>Nt Acorn</title>\n{mw.defs()}\n{bg}\n{body}\n</svg>\n')


def main():
    os.makedirs(OUT, exist_ok=True)
    for cells, name in ((190, "nt-acorn-crown"), (61, "nt-acorn-crown-medium"), (19, "nt-acorn-crown-small")):
        with open(os.path.join(OUT, f"{name}.svg"), "w") as f:
            f.write(icon_svg(cells))
    with open(os.path.join(OUT, "nt-acorn-crown-transparent.svg"), "w") as f:
        f.write(icon_svg(190, background=False))
    for px in SIZES:
        src = {19: "nt-acorn-crown-small", 61: "nt-acorn-crown-medium", 190: "nt-acorn-crown"}[cells_for(px)]
        subprocess.run(["rsvg-convert", "-w", str(px), "-h", str(px), "-o", os.path.join(OUT, f"nt-acorn-crown-{px}.png"),
                        os.path.join(OUT, f"{src}.svg")], check=True)
    print("wrote", os.path.normpath(OUT))


if __name__ == "__main__":
    main()
