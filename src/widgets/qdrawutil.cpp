// MIT License - see LICENSE
#include <QtWidgets/qdrawutil.h>

#include <QtGui/qbrush.h>
#include <QtGui/qpainter.h>
#include <QtGui/qpalette.h>
#include <QtGui/qpixmap.h>

#include <algorithm>
#include <cmath>

namespace {

// Draws `width` lines of `top` along the top and left edges of `r`, and of `bottom` along the bottom and right edges,
// going inwards; returns what is left inside.
QRect drawRing(QPainter *p, QRect r, int width, const QColor &top, const QColor &bottom) {
    for (int i = 0; i < width && r.width() > 0 && r.height() > 0; ++i) {
        p->setPen(top);
        p->drawLine(r.left(), r.top(), r.right() - 1, r.top());
        p->drawLine(r.left(), r.top(), r.left(), r.bottom() - 1);
        p->setPen(bottom);
        p->drawLine(r.left(), r.bottom(), r.right(), r.bottom());
        p->drawLine(r.right(), r.top(), r.right(), r.bottom());
        r.adjust(1, 1, -1, -1);
    }
    return r;
}

} // namespace

// Draws a horizontal or vertical shaded line from (`x1`, `y1`) to (`x2`, `y2`) with painter `p` and the colors of
// `pal`: `lineWidth` pixels of the dark and the light color, sunken if `sunken` is true and raised otherwise, with
// `midLineWidth` pixels of the mid color between them.
void qDrawShadeLine(QPainter *p, int x1, int y1, int x2, int y2, const QPalette &pal, bool sunken, int lineWidth,
                    int midLineWidth) {
    if (!p || lineWidth < 0 || midLineWidth < 0)
        return;
    p->save();
    const QColor first = sunken ? pal.color(QPalette::Dark) : pal.color(QPalette::Light);
    const QColor second = sunken ? pal.color(QPalette::Light) : pal.color(QPalette::Dark);
    const bool horizontal = y1 == y2;
    const int total = 2 * lineWidth + midLineWidth;
    int offset = -(total / 2);
    const auto line = [&](const QColor &color, int count) {
        p->setPen(color);
        for (int i = 0; i < count; ++i, ++offset) {
            if (horizontal)
                p->drawLine(std::min(x1, x2), y1 + offset, std::max(x1, x2), y1 + offset);
            else
                p->drawLine(x1 + offset, std::min(y1, y2), x1 + offset, std::max(y1, y2));
        }
    };
    line(first, lineWidth);
    line(pal.color(QPalette::Mid), midLineWidth);
    line(second, lineWidth);
    p->restore();
}

// Draws a shaded line from `p1` to `p2` with painter `p`, the colors of `pal`, `sunken`, `lineWidth` and
// `midLineWidth`.
void qDrawShadeLine(QPainter *p, const QPoint &p1, const QPoint &p2, const QPalette &pal, bool sunken, int lineWidth,
                    int midLineWidth) {
    qDrawShadeLine(p, p1.x(), p1.y(), p2.x(), p2.y(), pal, sunken, lineWidth, midLineWidth);
}

// Draws a shaded rectangle, a groove if `sunken` is true and a ridge otherwise, at `x`, `y`, `w` by `h` with
// painter `p` and the colors of `pal`: `lineWidth` pixels of the dark and the light color on each side of
// `midLineWidth` pixels of the mid color. The inside is filled with `fill` if it is not nullptr.
void qDrawShadeRect(QPainter *p, int x, int y, int w, int h, const QPalette &pal, bool sunken, int lineWidth,
                    int midLineWidth, const QBrush *fill) {
    if (!p || w <= 0 || h <= 0)
        return;
    p->save();
    const QColor dark = pal.color(QPalette::Dark), light = pal.color(QPalette::Light);
    QRect r = drawRing(p, QRect(x, y, w, h), lineWidth, sunken ? dark : light, sunken ? light : dark);
    r = drawRing(p, r, midLineWidth, pal.color(QPalette::Mid), pal.color(QPalette::Mid));
    r = drawRing(p, r, lineWidth, sunken ? light : dark, sunken ? dark : light);
    if (fill && r.isValid())
        p->fillRect(r, *fill);
    p->restore();
}

// Draws a shaded rectangle at `r` with painter `p`, the colors of `pal`, `sunken`, `lineWidth`, `midLineWidth` and
// `fill`.
void qDrawShadeRect(QPainter *p, const QRect &r, const QPalette &pal, bool sunken, int lineWidth, int midLineWidth,
                    const QBrush *fill) {
    qDrawShadeRect(p, r.x(), r.y(), r.width(), r.height(), pal, sunken, lineWidth, midLineWidth, fill);
}

// Draws a shaded panel at `x`, `y`, `w` by `h` with painter `p` and the colors of `pal`: `lineWidth` pixels dark at
// the top and left and light at the bottom and right if `sunken` is true, the other way round otherwise. The inside
// is filled with `fill` if it is not nullptr.
void qDrawShadePanel(QPainter *p, int x, int y, int w, int h, const QPalette &pal, bool sunken, int lineWidth,
                     const QBrush *fill) {
    if (!p || w <= 0 || h <= 0)
        return;
    p->save();
    const QColor dark = pal.color(QPalette::Dark), light = pal.color(QPalette::Light);
    const QRect inner = drawRing(p, QRect(x, y, w, h), lineWidth, sunken ? dark : light, sunken ? light : dark);
    if (fill && inner.isValid())
        p->fillRect(inner, *fill);
    p->restore();
}

// Draws a shaded panel at `r` with painter `p`, the colors of `pal`, `sunken`, `lineWidth` and `fill`.
void qDrawShadePanel(QPainter *p, const QRect &r, const QPalette &pal, bool sunken, int lineWidth, const QBrush *fill) {
    qDrawShadePanel(p, r.x(), r.y(), r.width(), r.height(), pal, sunken, lineWidth, fill);
}

// Draws a button in the look of Windows, two pixels deep, at `x`, `y`, `w` by `h` with painter `p` and the colors
// of `pal`; pressed if `sunken` is true. The inside is filled with `fill`, or the button color.
void qDrawWinButton(QPainter *p, int x, int y, int w, int h, const QPalette &pal, bool sunken, const QBrush *fill) {
    if (!p || w <= 0 || h <= 0)
        return;
    p->save();
    QRect r(x, y, w, h);
    if (sunken) {
        r = drawRing(p, r, 1, pal.color(QPalette::Shadow), pal.color(QPalette::Light));
        r = drawRing(p, r, 1, pal.color(QPalette::Dark), pal.color(QPalette::Button));
    } else {
        r = drawRing(p, r, 1, pal.color(QPalette::Light), pal.color(QPalette::Shadow));
        r = drawRing(p, r, 1, pal.color(QPalette::Midlight), pal.color(QPalette::Dark));
    }
    if (r.isValid())
        p->fillRect(r, fill ? *fill : pal.brush(QPalette::Button));
    p->restore();
}

// Draws a Windows button at `r` with painter `p`, the colors of `pal`, `sunken` and `fill`.
void qDrawWinButton(QPainter *p, const QRect &r, const QPalette &pal, bool sunken, const QBrush *fill) {
    qDrawWinButton(p, r.x(), r.y(), r.width(), r.height(), pal, sunken, fill);
}

// Draws a panel in the look of Windows, two pixels deep, at `x`, `y`, `w` by `h` with painter `p` and the colors of
// `pal`; sunken if `sunken` is true. The inside is filled with `fill` if it is not nullptr.
void qDrawWinPanel(QPainter *p, int x, int y, int w, int h, const QPalette &pal, bool sunken, const QBrush *fill) {
    if (!p || w <= 0 || h <= 0)
        return;
    p->save();
    QRect r(x, y, w, h);
    if (sunken) {
        r = drawRing(p, r, 1, pal.color(QPalette::Dark), pal.color(QPalette::Light));
        r = drawRing(p, r, 1, pal.color(QPalette::Shadow), pal.color(QPalette::Midlight));
    } else {
        r = drawRing(p, r, 1, pal.color(QPalette::Light), pal.color(QPalette::Shadow));
        r = drawRing(p, r, 1, pal.color(QPalette::Midlight), pal.color(QPalette::Dark));
    }
    if (fill && r.isValid())
        p->fillRect(r, *fill);
    p->restore();
}

// Draws a Windows panel at `r` with painter `p`, the colors of `pal`, `sunken` and `fill`.
void qDrawWinPanel(QPainter *p, const QRect &r, const QPalette &pal, bool sunken, const QBrush *fill) {
    qDrawWinPanel(p, r.x(), r.y(), r.width(), r.height(), pal, sunken, fill);
}

// Draws a frame of `lineWidth` pixels of the color `c` at `x`, `y`, `w` by `h` with painter `p`, and fills the
// inside with `fill` if it is not nullptr.
void qDrawPlainRect(QPainter *p, int x, int y, int w, int h, const QColor &c, int lineWidth, const QBrush *fill) {
    if (!p || w <= 0 || h <= 0)
        return;
    p->save();
    const QRect inner = drawRing(p, QRect(x, y, w, h), lineWidth, c, c);
    if (fill && inner.isValid())
        p->fillRect(inner, *fill);
    p->restore();
}

// Draws a plain frame at `r` with painter `p`, the color `c`, `lineWidth` and `fill`.
void qDrawPlainRect(QPainter *p, const QRect &r, const QColor &c, int lineWidth, const QBrush *fill) {
    qDrawPlainRect(p, r.x(), r.y(), r.width(), r.height(), c, lineWidth, fill);
}

/*!
    \class QTileRules
    \inmodule NtWidgets
    \since 6.0
    \brief The QTileRules struct says how qDrawBorderPixmap() fills the edges and the middle of a frame.

    \differences
    None.
    \enddifferences
*/

namespace {

// Fills `target` with `source` of `pixmap` along one direction as `rule` says, stretched in the other.
void tile(QPainter *painter, const QRect &target, const QPixmap &pixmap, const QRect &source, Qt::TileRule across,
          Qt::TileRule down) {
    if (target.isEmpty() || source.isEmpty())
        return;
    // The size of one tile in each direction.
    const auto pieces = [](int targetLength, int sourceLength, Qt::TileRule rule, qreal *size, int *count) {
        if (rule == Qt::StretchTile) {
            *size = targetLength;
            *count = 1;
        } else if (rule == Qt::RoundTile) {
            *count = std::max(1, static_cast<int>(std::lround(qreal(targetLength) / sourceLength)));
            *size = qreal(targetLength) / *count;
        } else {
            *size = sourceLength;
            *count = (targetLength + sourceLength - 1) / sourceLength;
        }
    };
    qreal w = 0, h = 0;
    int nx = 0, ny = 0;
    pieces(target.width(), source.width(), across, &w, &nx);
    pieces(target.height(), source.height(), down, &h, &ny);
    painter->save();
    painter->setClipRect(target, Qt::IntersectClip);
    for (int j = 0; j < ny; ++j)
        for (int i = 0; i < nx; ++i)
            painter->drawPixmap(QRectF(target.left() + i * w, target.top() + j * h, w, h), pixmap, QRectF(source));
    painter->restore();
}

} // namespace

// Draws `pixmap` in `targetRect` with painter `painter` as a frame of nine parts: `sourceRect` of the pixmap is cut
// by `sourceMargins` into four corners, four edges and a middle, which go into the parts of `targetRect` that
// `targetMargins` cut. The corners are scaled to fit; the edges and the middle are filled as `rules` say.
void qDrawBorderPixmap(QPainter *painter, const QRect &targetRect, const QMargins &targetMargins,
                       const QPixmap &pixmap, const QRect &sourceRect, const QMargins &sourceMargins,
                       const QTileRules &rules) {
    if (!painter || pixmap.isNull() || targetRect.isEmpty())
        return;
    const QRect s = sourceRect.isValid() ? sourceRect : pixmap.rect();
    const QRect &t = targetRect;
    const QMargins &sm = sourceMargins;
    const QMargins &tm = targetMargins;
    // The columns and the rows of the nine parts, as x and y positions.
    const int sx[4] = {s.left(), s.left() + sm.left(), s.right() + 1 - sm.right(), s.right() + 1};
    const int sy[4] = {s.top(), s.top() + sm.top(), s.bottom() + 1 - sm.bottom(), s.bottom() + 1};
    const int tx[4] = {t.left(), t.left() + tm.left(), t.right() + 1 - tm.right(), t.right() + 1};
    const int ty[4] = {t.top(), t.top() + tm.top(), t.bottom() + 1 - tm.bottom(), t.bottom() + 1};
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            const QRect source(QPoint(sx[column], sy[row]), QPoint(sx[column + 1] - 1, sy[row + 1] - 1));
            const QRect target(QPoint(tx[column], ty[row]), QPoint(tx[column + 1] - 1, ty[row + 1] - 1));
            if (source.isEmpty() || target.isEmpty())
                continue;
            // Corners stretch; the edges follow the rule along them, the middle both rules.
            const Qt::TileRule across = column == 1 ? rules.horizontal : Qt::StretchTile;
            const Qt::TileRule down = row == 1 ? rules.vertical : Qt::StretchTile;
            tile(painter, target, pixmap, source, across, down);
        }
    }
}

// Draws `pixmap` in `target` with painter `painter` as a frame cut by `margins`, the same in the pixmap and in the
// target, with the edges and the middle stretched.
void qDrawBorderPixmap(QPainter *painter, const QRect &target, const QMargins &margins, const QPixmap &pixmap) {
    qDrawBorderPixmap(painter, target, margins, pixmap, pixmap.rect(), margins);
}
