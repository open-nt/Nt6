// MIT License - see LICENSE
#include "qrasterpaintengine_p.h"

#include "qrasterizer_p.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace {

uint *asPixels(uchar *p) { return reinterpret_cast<uint *>(p); }
const uint *asPixels(const uchar *p) { return reinterpret_cast<const uint *>(p); }

QRgb fromPremultiplied(QRgb p, QImage::Format f) {
    switch (f) {
    case QImage::Format_RGB32:
        return qUnpremultiply(p) | 0xff000000u;
    case QImage::Format_ARGB32:
        return qUnpremultiply(p);
    default:
        return p;
    }
}

QRgb toPremultiplied(QRgb stored, QImage::Format f) {
    switch (f) {
    case QImage::Format_RGB32:
        return stored | 0xff000000u;
    case QImage::Format_ARGB32:
        return qPremultiply(stored);
    default:
        return stored;
    }
}

QRgb scaleChannels(QRgb p, int factor255) {
    if (factor255 >= 255)
        return p;
    auto mul = [factor255](int c) { return (c * factor255 + 127) / 255; };
    return qRgba(mul(qRed(p)), mul(qGreen(p)), mul(qBlue(p)), mul(qAlpha(p)));
}

void subtractRect(const QRect &p, const QRect &o, std::vector<QRect> &out) {
    if (!p.intersects(o)) {
        out.push_back(p);
        return;
    }
    const QRect i = p.intersected(o);
    if (p.top() < i.top())
        out.emplace_back(p.left(), p.top(), p.width(), i.top() - p.top());
    if (i.bottom() < p.bottom())
        out.emplace_back(p.left(), i.bottom() + 1, p.width(), p.bottom() - i.bottom());
    if (p.left() < i.left())
        out.emplace_back(p.left(), i.top(), i.left() - p.left(), i.height());
    if (i.right() < p.right())
        out.emplace_back(i.right() + 1, i.top(), p.right() - i.right(), i.height());
}

// Splits overlapping rectangles into disjoint ones so that translucent fills do not blend twice.
std::vector<QRect> makeDisjoint(const std::vector<QRect> &in) {
    std::vector<QRect> out;
    for (const QRect &r : in) {
        std::vector<QRect> pieces{r};
        for (const QRect &o : out) {
            std::vector<QRect> next;
            for (const QRect &p : pieces)
                subtractRect(p, o, next);
            pieces.swap(next);
        }
        out.insert(out.end(), pieces.begin(), pieces.end());
    }
    return out;
}

} // namespace

QRasterPaintEngine::~QRasterPaintEngine() = default;

bool QRasterPaintEngine::begin(QPaintDevice *device) {
    auto *image = dynamic_cast<QImage *>(device);
    if (!image || image->isNull() || isActive())
        return false;
    image->detach();
    m_image = image;
    setPaintDevice(device);
    m_state = QPaintEngineState();
    m_clip.assign(1, image->rect());
    setActive(true);
    return true;
}

bool QRasterPaintEngine::end() {
    m_image = nullptr;
    setPaintDevice(nullptr);
    setActive(false);
    return true;
}

void QRasterPaintEngine::updateState(const QPaintEngineState &state) {
    m_state = state;
    const QRect bounds = m_image->rect();
    if (!state.hasClipping()) {
        m_clip.assign(1, bounds);
        return;
    }
    m_clip = makeDisjoint(state.clipRegion().intersected(bounds).rects());
}

QRgb QRasterPaintEngine::premultiplied(const QColor &color) const {
    const int a = qRound(color.alpha() * m_state.opacity());
    return qPremultiply(qRgba(color.red(), color.green(), color.blue(), std::clamp(a, 0, 255)));
}

void QRasterPaintEngine::blend(uint *dst, QRgb src) const {
    const QImage::Format f = m_image->format();
    const QPainter::CompositionMode mode = m_state.compositionMode();
    if (mode == QPainter::CompositionMode_Clear) {
        *dst = f == QImage::Format_RGB32 ? 0xff000000u : 0u;
        return;
    }
    const uint sa = static_cast<uint>(qAlpha(src));
    if (mode == QPainter::CompositionMode_Source || sa == 255u) {
        *dst = fromPremultiplied(src, f);
        return;
    }
    if (sa == 0u)
        return;
    const QRgb d = toPremultiplied(*dst, f);
    const int ia = static_cast<int>(255u - sa);
    auto mix = [ia](int s, int dc) { return std::min(255, s + (dc * ia + 127) / 255); };
    const QRgb out = qRgba(mix(qRed(src), qRed(d)), mix(qGreen(src), qGreen(d)), mix(qBlue(src), qBlue(d)),
                           mix(qAlpha(src), qAlpha(d)));
    *dst = fromPremultiplied(out, f);
}

void QRasterPaintEngine::fillRect(const QRect &rect, QRgb premult) {
    if (!m_image || rect.isEmpty())
        return;
    const bool plain = m_state.compositionMode() == QPainter::CompositionMode_Source ||
                       (m_state.compositionMode() == QPainter::CompositionMode_SourceOver && qAlpha(premult) == 255);
    const QRgb solid = fromPremultiplied(premult, m_image->format());
    for (const QRect &clip : m_clip) {
        const QRect r = rect.normalized().intersected(clip);
        if (r.isEmpty())
            continue;
        for (int y = r.top(); y <= r.bottom(); ++y) {
            uint *line = asPixels(m_image->scanLine(y)) + r.left();
            if (plain && m_state.compositionMode() != QPainter::CompositionMode_Clear)
                std::fill(line, line + r.width(), solid);
            else
                for (int x = 0; x < r.width(); ++x)
                    blend(line + x, premult);
        }
    }
}

void QRasterPaintEngine::fillSpan(int y, int x0, int x1, QRgb premult) {
    if (x1 > x0)
        fillRect(QRect(x0, y, x1 - x0, 1), premult);
}

void QRasterPaintEngine::strokeSquare(int cx, int cy, int width, QRgb premult) {
    const int half = (width - 1) / 2;
    fillRect(QRect(cx - half, cy - half, width, width), premult);
}

void QRasterPaintEngine::strokeRect(const QRect &rect, int width, QRgb premult) {
    const int half = (width - 1) / 2;
    const QRect outer(rect.x() - half, rect.y() - half, rect.width() + width, rect.height() + width);
    if (outer.width() <= 2 * width || outer.height() <= 2 * width) {
        fillRect(outer, premult);
        return;
    }
    fillRect(QRect(outer.x(), outer.y(), outer.width(), width), premult);
    fillRect(QRect(outer.x(), outer.bottom() - width + 1, outer.width(), width), premult);
    fillRect(QRect(outer.x(), outer.y() + width, width, outer.height() - 2 * width), premult);
    fillRect(QRect(outer.right() - width + 1, outer.y() + width, width, outer.height() - 2 * width), premult);
}

void QRasterPaintEngine::drawRects(const QRect *rects, int rectCount) {
    const QPen pen = m_state.pen();
    const QBrush brush = m_state.brush();
    const bool stroke = pen.style() != Qt::NoPen;
    const int pw = std::max(1, pen.width());
    for (int i = 0; i < rectCount; ++i) {
        const QRect r = rects[i].normalized();
        if (brush.style() != Qt::NoBrush) {
            QRect fillArea = r;
            if (stroke)
                fillArea = QRect(r.x() - (pw - 1) / 2 + pw, r.y() - (pw - 1) / 2 + pw, r.width() + pw - 2 * pw,
                                 r.height() + pw - 2 * pw);
            fillRect(fillArea, premultiplied(brush.color()));
        }
        if (stroke)
            strokeRect(r, pw, premultiplied(pen.color()));
    }
}

void QRasterPaintEngine::drawLines(const QLine *lines, int lineCount) {
    const QPen pen = m_state.pen();
    if (pen.style() == Qt::NoPen)
        return;
    const int pw = std::max(1, pen.width());
    const int half = (pw - 1) / 2;
    const QRgb color = premultiplied(pen.color());
    for (int i = 0; i < lineCount; ++i) {
        int x0 = lines[i].x1(), y0 = lines[i].y1();
        const int x1 = lines[i].x2(), y1 = lines[i].y2();
        if (y0 == y1) {
            fillRect(QRect(std::min(x0, x1) - half, y0 - half, std::abs(x1 - x0) + pw, pw), color);
            continue;
        }
        if (x0 == x1) {
            fillRect(QRect(x0 - half, std::min(y0, y1) - half, pw, std::abs(y1 - y0) + pw), color);
            continue;
        }
        const int dx = std::abs(x1 - x0), dy = -std::abs(y1 - y0);
        const int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
        int err = dx + dy;
        for (;;) {
            strokeSquare(x0, y0, pw, color);
            if (x0 == x1 && y0 == y1)
                break;
            const int e2 = 2 * err;
            if (e2 >= dy) {
                err += dy;
                x0 += sx;
            }
            if (e2 <= dx) {
                err += dx;
                y0 += sy;
            }
        }
    }
}

void QRasterPaintEngine::drawPoints(const QPoint *points, int pointCount) {
    const QPen pen = m_state.pen();
    if (pen.style() == Qt::NoPen)
        return;
    const QRgb color = premultiplied(pen.color());
    for (int i = 0; i < pointCount; ++i)
        strokeSquare(points[i].x(), points[i].y(), std::max(1, pen.width()), color);
}

void QRasterPaintEngine::drawEllipse(const QRect &rectIn) {
    const QRect rect = rectIn.normalized();
    if (rect.isEmpty())
        return;
    const QPen pen = m_state.pen();
    const QBrush brush = m_state.brush();
    const bool stroke = pen.style() != Qt::NoPen;
    const bool fill = brush.style() != Qt::NoBrush;
    if (!stroke && !fill)
        return;
    const int pw = std::max(1, pen.width());
    const QRgb penColor = premultiplied(pen.color());
    const QRgb brushColor = premultiplied(brush.color());
    const double cx = rect.x() + rect.width() / 2.0;
    const double cy = rect.y() + rect.height() / 2.0;
    const double rx = rect.width() / 2.0;
    const double ry = rect.height() / 2.0;
    const double irx = stroke ? rx - pw : rx;
    const double iry = stroke ? ry - pw : ry;
    for (int y = rect.top(); y <= rect.bottom(); ++y) {
        const double dy = y + 0.5 - cy;
        const double t = 1.0 - (dy * dy) / (ry * ry);
        if (t <= 0.0)
            continue;
        const double hx = rx * std::sqrt(t);
        const int oL = qRound(cx - hx), oR = qRound(cx + hx);
        int iL = 0, iR = 0;
        bool hasInner = false;
        if (irx > 0.0 && iry > 0.0) {
            const double ti = 1.0 - (dy * dy) / (iry * iry);
            if (ti > 0.0) {
                const double hi = irx * std::sqrt(ti);
                iL = qRound(cx - hi);
                iR = qRound(cx + hi);
                hasInner = iR > iL;
            }
        }
        if (stroke) {
            if (hasInner) {
                fillSpan(y, oL, iL, penColor);
                fillSpan(y, iR, oR, penColor);
                if (fill)
                    fillSpan(y, iL, iR, brushColor);
            } else {
                fillSpan(y, oL, oR, penColor);
            }
        } else {
            fillSpan(y, oL, oR, brushColor);
        }
    }
}

void QRasterPaintEngine::drawImage(const QRect &rect, const QImage &image, const QRect &sourceRect) {
    if (image.isNull())
        return;
    const QRect sr = sourceRect.isNull() ? image.rect() : sourceRect.normalized().intersected(image.rect());
    if (sr.isEmpty())
        return;
    const QRect target = rect.isNull() ? QRect(rect.topLeft(), sr.size()) : rect.normalized();
    if (target.isEmpty())
        return;
    const int opacity = std::clamp(qRound(m_state.opacity() * 255.0), 0, 255);
    const QImage::Format srcFormat = image.format();
    const bool opaqueFast = opacity == 255 && srcFormat != QImage::Format_ARGB32;
    for (const QRect &clip : m_clip) {
        const QRect r = target.intersected(clip);
        if (r.isEmpty())
            continue;
        for (int y = r.top(); y <= r.bottom(); ++y) {
            const qint64 sy = sr.top() + static_cast<qint64>(y - target.top()) * sr.height() / target.height();
            const uint *srcLine = asPixels(image.constScanLine(static_cast<int>(sy)));
            uint *dstLine = asPixels(m_image->scanLine(y));
            for (int x = r.left(); x <= r.right(); ++x) {
                const qint64 sx = sr.left() + static_cast<qint64>(x - target.left()) * sr.width() / target.width();
                QRgb p = toPremultiplied(srcLine[sx], srcFormat);
                if (!opaqueFast)
                    p = scaleChannels(p, opacity);
                blend(dstLine + x, p);
            }
        }
    }
}

void QRasterPaintEngine::drawAlphaMask(const QPoint &topLeft, const uchar *mask, int width, int height,
                                       qsizetype stride) {
    if (!m_image || width <= 0 || height <= 0)
        return;
    const QRgb color = premultiplied(m_state.pen().color());
    const QRect target(topLeft, QSize(width, height));
    for (const QRect &clip : m_clip) {
        const QRect r = target.intersected(clip);
        if (r.isEmpty())
            continue;
        for (int y = r.top(); y <= r.bottom(); ++y) {
            const uchar *coverage =
                mask + static_cast<qsizetype>(y - target.top()) * stride + (r.left() - target.left());
            uint *line = asPixels(m_image->scanLine(y)) + r.left();
            for (int x = 0; x < r.width(); ++x) {
                const int c = coverage[x];
                if (c != 0)
                    blend(line + x, c == 255 ? color : scaleChannels(color, c));
            }
        }
    }
}

// ---- Paths ----

namespace {

namespace raster = QtRasterPrivate;

raster::Mat toMat(const QTransform &t) { return {t.m11(), t.m12(), t.m21(), t.m22(), t.dx(), t.dy()}; }

raster::Path toRasterPath(const QPainterPath &path) {
    raster::Path out;
    QPointF start;
    bool open = false;
    for (int i = 0; i < path.elementCount(); ++i) {
        const QPainterPath::Element e = path.elementAt(i);
        switch (e.type) {
        case QPainterPath::MoveToElement:
            out.moveTo({e.x, e.y});
            start = QPointF(e.x, e.y);
            open = true;
            break;
        case QPainterPath::LineToElement:
            // A line back to the start that ends the subpath closes it, so that its corner gets a join.
            if (open && QPointF(e.x, e.y) == start &&
                (i + 1 == path.elementCount() || path.elementAt(i + 1).type == QPainterPath::MoveToElement)) {
                out.close();
                open = false;
            } else {
                out.lineTo({e.x, e.y});
            }
            break;
        case QPainterPath::CurveToElement:
            if (i + 2 < path.elementCount()) {
                const QPainterPath::Element c2 = path.elementAt(i + 1), end = path.elementAt(i + 2);
                out.cubicTo({e.x, e.y}, {c2.x, c2.y}, {end.x, end.y});
                i += 2;
            }
            break;
        case QPainterPath::CurveToDataElement:
            break;
        }
    }
    return out;
}

} // namespace

void QRasterPaintEngine::drawPath(const QPainterPath &path) {
    if (!m_image || path.isEmpty())
        return;
    const QTransform t = m_state.transform();
    const raster::Mat m = toMat(t);
    const double scale = std::max(1e-9, m.scale());
    const double tolerance = 0.2 / scale;
    const raster::Path p = toRasterPath(path);
    const std::vector<raster::Path::Contour> contours = p.flatten(tolerance);
    if (contours.empty())
        return;
    if (m_state.brush().style() != Qt::NoBrush) {
        raster::Polygons polygons;
        raster::fillContours(contours, m, &polygons);
        paintCoverage(&polygons, path.fillRule() == Qt::OddEvenFill, m_state.brush(), false, t);
    }
    const QPen pen = m_state.pen();
    if (pen.style() == Qt::NoPen || pen.color().alpha() == 0)
        return;
    raster::StrokeStyle style;
    const double width = pen.widthF() > 0.0 ? pen.widthF() : 1.0;
    style.width = pen.isCosmetic() ? width / scale : width;
    style.cap = pen.capStyle() == Qt::RoundCap    ? raster::LineCap::Round
                : pen.capStyle() == Qt::SquareCap ? raster::LineCap::Square
                                                  : raster::LineCap::Butt;
    style.join = pen.joinStyle() == Qt::RoundJoin   ? raster::LineJoin::Round
                 : pen.joinStyle() == Qt::BevelJoin ? raster::LineJoin::Bevel
                                                    : raster::LineJoin::Miter;
    style.miterLimit = pen.miterLimit();
    const QList<qreal> dashes = pen.dashPattern();
    const double unit = std::max(1.0, width);
    for (const qreal d : dashes)
        style.dashes.push_back(std::max(1e-3, d) * (pen.isCosmetic() ? unit / scale : unit));
    style.dashOffset = pen.dashOffset() * (pen.isCosmetic() ? unit / scale : unit);
    raster::Polygons polygons;
    raster::strokeContours(contours, style, m, tolerance, &polygons);
    // Aliased strokes are moved half a pixel, so that a line on whole coordinates covers whole pixels.
    if (!(m_state.renderHints() & QPainter::Antialiasing))
        for (raster::Pt &pt : polygons.points) {
            pt.x += 0.5;
            pt.y += 0.5;
        }
    paintCoverage(&polygons, false, pen.brush(), true, t);
}

void QRasterPaintEngine::paintCoverage(const void *polygonData, bool evenOdd, const QBrush &brush, bool pen,
                                       const QTransform &toDevice) {
    const auto &polygons = *static_cast<const raster::Polygons *>(polygonData);
    if (polygons.points.empty())
        return;
    // The pixels the polygons can touch, within the image and the clip.
    double x0 = polygons.points.front().x, x1 = x0, y0 = polygons.points.front().y, y1 = y0;
    for (const raster::Pt &pt : polygons.points) {
        if (!std::isfinite(pt.x) || !std::isfinite(pt.y))
            return;
        x0 = std::min(x0, pt.x);
        x1 = std::max(x1, pt.x);
        y0 = std::min(y0, pt.y);
        y1 = std::max(y1, pt.y);
    }
    QRect clipBounds;
    for (const QRect &c : m_clip)
        clipBounds = clipBounds.united(c);
    const double limit = 1e7;
    const QRect box = QRect(QPoint(static_cast<int>(std::floor(std::max(x0, -limit))),
                                   static_cast<int>(std::floor(std::max(y0, -limit)))),
                            QPoint(static_cast<int>(std::ceil(std::min(x1, limit))),
                                   static_cast<int>(std::ceil(std::min(y1, limit)))))
                          .intersected(clipBounds);
    if (box.isEmpty())
        return;
    // The coverage of each pixel, as the alpha of a white fill.
    raster::Surface coverage(box.width(), box.height());
    raster::Polygons shifted = polygons;
    for (raster::Pt &pt : shifted.points) {
        pt.x -= box.x();
        pt.y -= box.y();
    }
    raster::Shader white;
    white.color = 0xffffffffu;
    coverage.fill(shifted, evenOdd ? raster::FillRule::EvenOdd : raster::FillRule::NonZero, white);

    // The color of each pixel: one color, or the gradient of the brush.
    const QGradient *gradient = pen ? nullptr : brush.gradient();
    raster::Shader shader;
    const QRgb solid = premultiplied(gradient ? QColor(Qt::black) : brush.color());
    if (gradient) {
        shader.gradient = true;
        std::vector<raster::GradientStop> stops;
        for (const QGradientStop &s : gradient->stops())
            stops.push_back({s.first, s.second.redF(), s.second.greenF(), s.second.blueF(), s.second.alphaF()});
        shader.setStops(stops, m_state.opacity());
        bool invertible = false;
        const QTransform toGradient = (brush.transform() * toDevice).inverted(&invertible);
        if (!invertible)
            return;
        shader.toGradient = toMat(toGradient);
        shader.spread = gradient->spread() == QGradient::RepeatSpread    ? raster::Spread::Repeat
                        : gradient->spread() == QGradient::ReflectSpread ? raster::Spread::Reflect
                                                                         : raster::Spread::Pad;
        if (gradient->type() == QGradient::LinearGradient) {
            const auto *g = static_cast<const QLinearGradient *>(gradient);
            shader.p1 = {g->start().x(), g->start().y()};
            shader.p2 = {g->finalStop().x(), g->finalStop().y()};
        } else if (gradient->type() == QGradient::RadialGradient) {
            const auto *g = static_cast<const QRadialGradient *>(gradient);
            shader.radial = true;
            shader.p1 = {g->center().x(), g->center().y()};
            shader.p2 = {g->focalPoint().x(), g->focalPoint().y()};
            shader.radius = g->radius();
        } else {
            const auto *g = static_cast<const QConicalGradient *>(gradient);
            shader.conical = true;
            shader.p1 = {g->center().x(), g->center().y()};
            shader.angle = g->angle();
        }
    }
    const bool antialias = m_state.renderHints() & QPainter::Antialiasing;
    const QPainter::CompositionMode mode = m_state.compositionMode();
    const QImage::Format f = m_image->format();
    for (const QRect &clip : m_clip) {
        const QRect r = clip.intersected(box);
        for (int y = r.top(); y <= r.bottom(); ++y) {
            uint *line = asPixels(m_image->scanLine(y));
            const std::uint32_t *cov =
                coverage.pixels.data() + static_cast<std::size_t>(y - box.y()) * static_cast<std::size_t>(box.width());
            for (int x = r.left(); x <= r.right(); ++x) {
                unsigned c = cov[x - box.x()] >> 24u;
                if (!antialias)
                    c = c >= 128u ? 255u : 0u;
                if (c == 0u)
                    continue;
                const QRgb color = gradient ? shader.at(x + 0.5, y + 0.5) : solid;
                if (mode == QPainter::CompositionMode_SourceOver) {
                    blend(line + x, c == 255u ? color : scaleChannels(color, static_cast<int>(c)));
                    continue;
                }
                // Source and Clear replace the destination where the shape covers it.
                const QRgb src = mode == QPainter::CompositionMode_Clear ? 0u : color;
                const QRgb dst = toPremultiplied(line[x], f);
                const int k = static_cast<int>(c), ik = 255 - k;
                const auto mix = [k, ik](int s, int d) { return (s * k + d * ik + 127) / 255; };
                QRgb out = qRgba(mix(qRed(src), qRed(dst)), mix(qGreen(src), qGreen(dst)), mix(qBlue(src), qBlue(dst)),
                                 mix(qAlpha(src), qAlpha(dst)));
                if (f == QImage::Format_RGB32)
                    out |= 0xff000000u;
                line[x] = fromPremultiplied(out, f);
            }
        }
    }
}
