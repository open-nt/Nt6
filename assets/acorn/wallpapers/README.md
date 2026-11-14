# Acorn wallpapers

Each Nt release is named after a nut; 6.0.0 is **Acorn**. Its artwork is the *Acorn crown*: the acorn's cup seen
from above, its scales drawn as hexagons set on the golden-angle spiral that real cups and seed heads follow, growing
from the stem at the centre to the rim. It is drawn in the brand colours of the [brand guide](../../../doc/brand.h):
Pine `#0B2E27`, Nut Teal `#13977F` and Nut Neon `#3FD9B4`, with the text set in Titillium Web.

| File | For |
|---|---|
| `nt-6.0.0-acorn.svg` | The landscape source: the crown with the release name, the mark and the version |
| `nt-6.0.0-acorn-3840x2160.png`, `-2560x1440.png`, `-1920x1080.png` | 4K, 1440p and 1080p screens |
| `nt-6.0.0-acorn-clean.svg` and its PNGs | The same without text, for a desktop with its own icons |
| `nt-6.0.0-acorn-portrait.svg`, `-clean-portrait.svg` and their `1440x3120`, `1080x2340` PNGs | Phones |
| [`../icon/`](../icon/) | The crown as an icon: SVG and PNGs from 16 to 1024 px |

## Making them

`make_wallpaper.py` writes the SVG files and renders the PNG sizes with `rsvg-convert`; `make_icon.py` makes the icon
set, thinning the spiral as the size drops so that it stays legible. Both need the Titillium Web fonts (SIL Open Font
License 1.1, `../../brand/OFL-TitilliumWeb.txt`) known to fontconfig, for example in `~/.fonts`:

    assets/acorn/wallpapers/make_wallpaper.py
    assets/acorn/wallpapers/make_icon.py

The mark is embedded from `assets/nt-logo.svg`, unchanged, as the brand guide asks.

## The next release

Copy this directory to `assets/<nut>/wallpapers/` and give the script the release's own artwork, a square SVG or PNG
generated from a prompt written as `PROMPT.md` and the `nt-icon-style` skill describe:

    assets/hazelnut/wallpapers/make_wallpaper.py --version 6.1.0 --name Hazelnut --image hazelnut.svg --out assets/hazelnut/wallpapers

Without `--image` the script draws the crown, which stays Acorn's.
