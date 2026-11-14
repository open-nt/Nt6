#!/usr/bin/env python3
# MIT License - see LICENSE
"""Reports how much of an image lies outside the Nt palette, to catch artwork that drifted to brown or grey.

    check_palette.py image.png|image.svg [--tolerance 28]

The image is scaled down and read through ImageMagick (an SVG is rasterised first), so Pillow is not needed. Every
pixel is matched to the nearest palette colour; a pixel farther than the tolerance (in RGB units) from all of them is
off-palette. Fully transparent pixels are ignored. Exit status 1 when more than 5% is off-palette.
"""
import argparse
import subprocess
import sys

import numpy as np

PALETTE = {
    "Pine": (0x0B, 0x2E, 0x27),
    "Deep Pine": (0x06, 0x20, 0x1B),
    "Nut Teal": (0x13, 0x97, 0x7F),
    "Deep Teal": (0x0C, 0x66, 0x55),
    "Nut Neon": (0x3F, 0xD9, 0xB4),
    "Neon Deep": (0x2B, 0xB8, 0x94),
    "White": (0xFF, 0xFF, 0xFF),
    "Muted": (0xA6, 0xCD, 0xC3),
}


def load(path, size=256):
    """The image as an (n, 4) array of RGBA bytes, resampled to `size` by `size`; the proportions do not matter for
    colour statistics."""
    raw = subprocess.run(["magick", path, "-resize", f"{size}x{size}!", "-depth", "8", "rgba:-"], capture_output=True,
                         check=True)
    return np.frombuffer(raw.stdout, dtype=np.uint8).reshape(-1, 4)


def main():
    parser = argparse.ArgumentParser(description="Checks an image against the Nt palette.")
    parser.add_argument("image")
    parser.add_argument("--tolerance", type=float, default=28.0,
                        help="how far (RGB distance) a pixel may be from the nearest palette colour; gradients "
                             "between palette colours pass, as do highlights, because white is in the palette")
    args = parser.parse_args()

    pixels = load(args.image)
    opaque = pixels[pixels[:, 3] > 16][:, :3].astype(np.float32)
    if opaque.size == 0:
        print("the image is empty")
        return 1
    colours = np.array(list(PALETTE.values()), dtype=np.float32)
    # The distance to the nearest palette colour, and to the segment between any two of them (a gradient).
    distances = np.full(len(opaque), np.inf, dtype=np.float32)
    nearest = np.zeros(len(opaque), dtype=np.int64)
    for i, c in enumerate(colours):
        d = np.linalg.norm(opaque - c, axis=1)
        closer = d < distances
        distances[closer] = d[closer]
        nearest[closer] = i
    for i in range(len(colours)):
        for j in range(i + 1, len(colours)):
            a, b = colours[i], colours[j]
            ab = b - a
            t = np.clip(((opaque - a) @ ab) / float(ab @ ab), 0.0, 1.0)
            d = np.linalg.norm(opaque - (a + t[:, None] * ab), axis=1)
            distances = np.minimum(distances, d)
    off = distances > args.tolerance
    share = float(off.mean()) * 100.0

    names = list(PALETTE.keys())
    counts = np.bincount(nearest, minlength=len(names)) / len(opaque) * 100.0
    print(f"{args.image}: {share:.1f}% of the opaque pixels are off the palette")
    for name, pct in sorted(zip(names, counts), key=lambda p: -p[1]):
        if pct >= 0.5:
            print(f"  {name:10s} {pct:5.1f}%")
    if off.any():
        stray = opaque[off].mean(axis=0)
        print(f"  off-palette pixels average #{int(stray[0]):02X}{int(stray[1]):02X}{int(stray[2]):02X}")
    return 1 if share > 5.0 else 0


if __name__ == "__main__":
    sys.exit(main())
