#!/usr/bin/env python3
# MIT License - see LICENSE
"""Turns text into SVG path data from a TrueType font, so that a generated SVG carries its lettering as outlines and
looks the same wherever it is opened, with or without the font installed. Reads the glyf, loca, cmap, hmtx and kern
tables; enough for the Latin text on the wallpapers."""
import struct


class Font:
    def __init__(self, path):
        self.data = data = open(path, "rb").read()
        count = struct.unpack(">H", data[4:6])[0]
        self.tables = {}
        for i in range(count):
            tag, _, offset, length = struct.unpack(">4sIII", data[12 + 16 * i:28 + 16 * i])
            self.tables[tag.decode("latin-1")] = (offset, length)
        head = self.tables["head"][0]
        self.units = struct.unpack(">H", data[head + 18:head + 20])[0]
        long_loca = struct.unpack(">h", data[head + 50:head + 52])[0] == 1
        maxp = self.tables["maxp"][0]
        glyphs = struct.unpack(">H", data[maxp + 4:maxp + 6])[0]
        loca = self.tables["loca"][0]
        if long_loca:
            self.loca = struct.unpack(f">{glyphs + 1}I", data[loca:loca + 4 * (glyphs + 1)])
        else:
            self.loca = [2 * v for v in struct.unpack(f">{glyphs + 1}H", data[loca:loca + 2 * (glyphs + 1)])]
        hhea = self.tables["hhea"][0]
        metrics = struct.unpack(">H", data[hhea + 34:hhea + 36])[0]
        hmtx = self.tables["hmtx"][0]
        self.advances = [struct.unpack(">H", data[hmtx + 4 * i:hmtx + 4 * i + 2])[0] for i in range(metrics)]
        self.cmap = self._cmap()
        self.kern = self._kern()

    def _cmap(self):
        data, base = self.data, self.tables["cmap"][0]
        count = struct.unpack(">H", data[base + 2:base + 4])[0]
        for i in range(count):
            platform, encoding, offset = struct.unpack(">HHI", data[base + 4 + 8 * i:base + 12 + 8 * i])
            sub = base + offset
            if struct.unpack(">H", data[sub:sub + 2])[0] != 4 or (platform, encoding) not in ((3, 1), (0, 3), (0, 4)):
                continue
            seg = struct.unpack(">H", data[sub + 6:sub + 8])[0] // 2
            ends = struct.unpack(f">{seg}H", data[sub + 14:sub + 14 + 2 * seg])
            starts = struct.unpack(f">{seg}H", data[sub + 16 + 2 * seg:sub + 16 + 4 * seg])
            deltas = struct.unpack(f">{seg}h", data[sub + 16 + 4 * seg:sub + 16 + 6 * seg])
            range_base = sub + 16 + 6 * seg
            ranges = struct.unpack(f">{seg}H", data[range_base:range_base + 2 * seg])
            out = {}
            for s in range(seg):
                for c in range(starts[s], min(ends[s], 0xFFFE) + 1):
                    if ranges[s] == 0:
                        g = (c + deltas[s]) & 0xFFFF
                    else:
                        at = range_base + 2 * s + ranges[s] + 2 * (c - starts[s])
                        g = struct.unpack(">H", data[at:at + 2])[0]
                        g = (g + deltas[s]) & 0xFFFF if g else 0
                    if g:
                        out[c] = g
            return out
        raise ValueError("no usable cmap")

    def _kern(self):
        """The pairs of the old-style kern table, if the font has one (format 0)."""
        pairs = {}
        if "kern" not in self.tables:
            return pairs
        data, base = self.data, self.tables["kern"][0]
        count = struct.unpack(">H", data[base + 2:base + 4])[0]
        at = base + 4
        for _ in range(count):
            length, coverage = struct.unpack(">HH", data[at + 2:at + 6])
            if coverage & 0xFF == 0:
                n = struct.unpack(">H", data[at + 6:at + 8])[0]
                for i in range(n):
                    left, right, value = struct.unpack(">HHh", data[at + 14 + 6 * i:at + 20 + 6 * i])
                    pairs[(left, right)] = value
            at += length
        return pairs

    def advance(self, glyph):
        return self.advances[min(glyph, len(self.advances) - 1)]

    def contours(self, glyph, dx=0, dy=0):
        """The glyph's contours as lists of (x, y, on_curve) points in font units."""
        start, end = self.loca[glyph], self.loca[glyph + 1]
        if end <= start:
            return []
        data = self.data
        at = self.tables["glyf"][0] + start
        n = struct.unpack(">h", data[at:at + 2])[0]
        at += 10
        if n < 0:
            return self._composite(at, dx, dy)
        ends = struct.unpack(f">{n}H", data[at:at + 2 * n])
        at += 2 * n
        points = ends[-1] + 1 if n else 0
        at += 2 + struct.unpack(">H", data[at:at + 2])[0]
        flags = []
        while len(flags) < points:
            f = data[at]
            at += 1
            flags.append(f)
            if f & 8:
                flags.extend([f] * data[at])
                at += 1
        xs, ys = [], []
        for coords, short, same in ((xs, 2, 16), (ys, 4, 32)):
            v = 0
            for f in flags:
                if f & short:
                    d = data[at]
                    at += 1
                    v += d if f & same else -d
                elif not f & same:
                    v += struct.unpack(">h", data[at:at + 2])[0]
                    at += 2
                coords.append(v)
        out, first = [], 0
        for last in ends:
            out.append([(xs[i] + dx, ys[i] + dy, bool(flags[i] & 1)) for i in range(first, last + 1)])
            first = last + 1
        return out

    def _composite(self, at, dx, dy):
        data, out = self.data, []
        while True:
            flags, glyph = struct.unpack(">HH", data[at:at + 4])
            at += 4
            if flags & 1:
                a, b = struct.unpack(">hh", data[at:at + 4])
                at += 4
            else:
                a, b = struct.unpack(">bb", data[at:at + 2])
                at += 2
            at += 2 if flags & 8 else 4 if flags & 0x40 else 8 if flags & 0x80 else 0
            if flags & 2:  # ARGS_ARE_XY_VALUES; the wallpapers' glyphs never need the point-matching form
                out.extend(self.contours(glyph, dx + a, dy + b))
            if not flags & 0x20:
                return out

    def path(self, text, x, y, size, spacing=0.0):
        """SVG path data for `text` with its baseline at (x, y), `size` tall in user units (y down), with `spacing`
        extra between letters; and the width it takes."""
        scale = size / self.units
        parts, pen, previous = [], 0.0, None
        for ch in text:
            glyph = self.cmap.get(ord(ch), 0)
            if previous is not None:
                pen += self.kern.get((previous, glyph), 0) * scale
            for contour in self.contours(glyph):
                parts.append(self._contour(contour, x + pen, y, scale))
            pen += self.advance(glyph) * scale + spacing
            previous = glyph
        return " ".join(parts), pen

    @staticmethod
    def _contour(points, ox, oy, scale):
        def fmt(px, py):
            return f"{ox + px * scale:.1f} {oy - py * scale:.1f}"
        # Start on an on-curve point, inserting the implied one if every point is off the curve.
        on = [i for i, p in enumerate(points) if p[2]]
        if on:
            points = points[on[0]:] + points[:on[0]]
        else:
            mid = ((points[0][0] + points[-1][0]) / 2, (points[0][1] + points[-1][1]) / 2, True)
            points = [mid] + points
        out = [f"M {fmt(points[0][0], points[0][1])}"]
        control = None
        for px, py, is_on in points[1:] + [points[0]]:
            if is_on:
                out.append(f"L {fmt(px, py)}" if control is None else f"Q {fmt(*control)} {fmt(px, py)}")
                control = None
            elif control is None:
                control = (px, py)
            else:
                mid = ((control[0] + px) / 2, (control[1] + py) / 2)
                out.append(f"Q {fmt(*control)} {fmt(*mid)}")
                control = (px, py)
        out.append("Z")
        return " ".join(out)
