// MIT License - see LICENSE
#include <QtCharts/qabstractbarseries.h>
#include <QtCharts/qareaseries.h>
#include <QtCharts/qbarcategoryaxis.h>
#include <QtCharts/qbarset.h>
#include <QtCharts/qchart.h>
#include <QtCharts/qdatetimeaxis.h>
#include <QtCharts/qlegend.h>
#include <QtCharts/qlineseries.h>
#include <QtCharts/qlogvalueaxis.h>
#include <QtCharts/qpieseries.h>
#include <QtCharts/qpieslice.h>
#include <QtCharts/qscatterseries.h>
#include <QtCharts/qvalueaxis.h>
#include <QtCharts/qxyseries.h>
#include <QtCore/qalgorithms.h>
#include <QtCore/qdatetime.h>
#include <QtGui/qfontmetrics.h>
#include <QtGui/qimage.h>
#include <QtGui/qpaintdevice.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qwidget.h>

#include "qchartengine_p.h"
#include "qrasterizer_p.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>

namespace svg = QtRasterPrivate;

namespace QtChartsPrivate {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTick = 5.0;  // length of the ticks
constexpr double kGap = 4.0;   // between ticks, labels and titles
constexpr double kStack = 8.0; // between axes on the same side
constexpr int kMaxTicks = 1000;

Theme makeTheme(const char *background, const char *title, const char *labels, const char *axisLine, const char *grid,
                const char *minorGrid, const char *shades, std::initializer_list<const char *> palette) {
    Theme t;
    t.background = QColor(background);
    t.title = QColor(title);
    t.labels = QColor(labels);
    t.axisLine = QColor(axisLine);
    t.grid = QColor(grid);
    t.minorGrid = QColor(minorGrid);
    t.shades = QColor(shades);
    t.legendBackground = QColor(background);
    t.legendBorder = QColor(grid);
    for (const char *color : palette)
        t.palette.emplace_back(color);
    return t;
}

bool isBarType(QAbstractSeries::SeriesType type) {
    switch (type) {
    case QAbstractSeries::SeriesTypeBar:
    case QAbstractSeries::SeriesTypeStackedBar:
    case QAbstractSeries::SeriesTypePercentBar:
    case QAbstractSeries::SeriesTypeHorizontalBar:
    case QAbstractSeries::SeriesTypeHorizontalStackedBar:
    case QAbstractSeries::SeriesTypeHorizontalPercentBar:
        return true;
    default:
        return false;
    }
}

bool isHorizontalBar(QAbstractSeries::SeriesType type) {
    return type == QAbstractSeries::SeriesTypeHorizontalBar ||
           type == QAbstractSeries::SeriesTypeHorizontalStackedBar ||
           type == QAbstractSeries::SeriesTypeHorizontalPercentBar;
}

bool isXYType(QAbstractSeries::SeriesType type) {
    return type == QAbstractSeries::SeriesTypeLine || type == QAbstractSeries::SeriesTypeSpline ||
           type == QAbstractSeries::SeriesTypeScatter;
}

// Premultiplied ARGB of `color` with an extra opacity.
svg::Shader solid(const QColor &color, double opacity = 1.0) {
    const double a = std::clamp(color.alphaF() * opacity, 0.0, 1.0);
    const auto channel = [a](int v) { return static_cast<std::uint32_t>(std::lround(v * a)); };
    svg::Shader shader;
    shader.color = (static_cast<std::uint32_t>(std::lround(a * 255.0)) << 24) | (channel(color.red()) << 16) |
                   (channel(color.green()) << 8) | channel(color.blue());
    return shader;
}

double clampCoord(double v) { return std::clamp(v, -1e6, 1e6); }

// Shapes in the coordinates of the chart, drawn on a surface at the resolution of the device.
struct Canvas {
    svg::Surface *surface = nullptr;
    svg::Mat m;
    double ratio = 1.0;

    void fill(const svg::Path &path, const QColor &color, double opacity = 1.0) const {
        if (path.isEmpty() || color.alpha() == 0)
            return;
        svg::Polygons polygons;
        svg::fillContours(path.flatten(0.2 / ratio), m, &polygons);
        surface->fill(polygons, svg::FillRule::NonZero, solid(color, opacity));
    }

    void fill(const svg::Path &path, const QBrush &brush, double opacity = 1.0) const {
        if (brush.style() != Qt::NoBrush)
            fill(path, brush.color(), opacity);
    }

    void stroke(const svg::Path &path, const QPen &pen, double opacity = 1.0, bool round = false) const {
        if (path.isEmpty() || pen.style() == Qt::NoPen || pen.color().alpha() == 0)
            return;
        svg::StrokeStyle style;
        style.width = pen.widthF() > 0.0 ? pen.widthF() : 1.0 / ratio;
        style.cap = round ? svg::LineCap::Round : svg::LineCap::Butt;
        style.join = round ? svg::LineJoin::Round : svg::LineJoin::Miter;
        svg::Polygons polygons;
        svg::strokeContours(path.flatten(0.2 / ratio), style, m, 0.2 / ratio, &polygons);
        surface->fill(polygons, svg::FillRule::NonZero, solid(pen.color(), opacity));
    }

    // A line of width `w` along a whole number of device pixels, so that thin lines stay sharp.
    double snap(double v, double w) const {
        const double px = std::max(1.0, std::round(w * ratio));
        const double odd = std::fmod(px, 2.0) == 1.0 ? 0.5 : 0.0;
        return (std::floor(v * ratio) + odd) / ratio;
    }

    void line(QPointF a, QPointF b, const QPen &pen) const {
        svg::Path path;
        path.moveTo({a.x(), a.y()});
        path.lineTo({b.x(), b.y()});
        stroke(path, pen);
    }

    void rect(const QRectF &r, const QBrush &brush, const QPen &pen, double radius = 0.0) const {
        svg::Path path;
        path.addRect(r.x(), r.y(), r.width(), r.height(), radius, radius);
        fill(path, brush);
        stroke(path, pen);
    }
};

struct TextItem {
    QRectF rect;
    int flags = 0;
    QString text;
    QFont font;
    QColor color;
};

QString number(double v) { return QString::number(v, 'g', 6); }

// The marker of a scatter series, or of a legend entry, centered at `c`.
svg::Path markerPath(QScatterSeries::MarkerShape shape, QPointF c, double size) {
    svg::Path path;
    const double r = size / 2.0;
    const auto polygon = [&](int corners, double start, double inner) {
        const int n = inner > 0.0 ? corners * 2 : corners;
        for (int i = 0; i < n; ++i) {
            const double radius = inner > 0.0 && (i % 2) ? r * inner : r;
            const double angle = start + 2.0 * kPi * i / n;
            const svg::Pt p{c.x() + radius * std::sin(angle), c.y() - radius * std::cos(angle)};
            if (i == 0)
                path.moveTo(p);
            else
                path.lineTo(p);
        }
        path.close();
    };
    switch (shape) {
    case QScatterSeries::MarkerShapeRectangle:
        path.addRect(c.x() - r, c.y() - r, size, size, 0, 0);
        break;
    case QScatterSeries::MarkerShapeRotatedRectangle:
        polygon(4, 0.0, 0.0);
        break;
    case QScatterSeries::MarkerShapeTriangle:
        polygon(3, 0.0, 0.0);
        break;
    case QScatterSeries::MarkerShapeStar:
        polygon(5, 0.0, 0.45);
        break;
    case QScatterSeries::MarkerShapePentagon:
        polygon(5, 0.0, 0.0);
        break;
    case QScatterSeries::MarkerShapeCircle:
    default:
        path.addEllipse(c.x(), c.y(), r, r);
        break;
    }
    return path;
}

QPointF onCircle(QPointF c, double radius, double degrees) {
    const double a = degrees * kPi / 180.0;
    return QPointF(c.x() + radius * std::sin(a), c.y() - radius * std::cos(a));
}

svg::Path slicePath(const SliceGeometry &g) {
    svg::Path path;
    const int steps = std::max(2, static_cast<int>(std::ceil(std::fabs(g.span) / 2.0)));
    for (int i = 0; i <= steps; ++i) {
        const QPointF p = onCircle(g.center, g.outer, g.start + g.span * i / steps);
        if (i == 0)
            path.moveTo({p.x(), p.y()});
        else
            path.lineTo({p.x(), p.y()});
    }
    if (g.inner > 0.0) {
        for (int i = steps; i >= 0; --i) {
            const QPointF p = onCircle(g.center, g.inner, g.start + g.span * i / steps);
            path.lineTo({p.x(), p.y()});
        }
    } else {
        path.lineTo({g.center.x(), g.center.y()});
    }
    path.close();
    return path;
}

double segmentDistance(QPointF p, QPointF a, QPointF b) {
    const double dx = b.x() - a.x(), dy = b.y() - a.y();
    const double len = dx * dx + dy * dy;
    double t = len > 0.0 ? ((p.x() - a.x()) * dx + (p.y() - a.y()) * dy) / len : 0.0;
    t = std::clamp(t, 0.0, 1.0);
    return std::hypot(p.x() - (a.x() + t * dx), p.y() - (a.y() + t * dy));
}

bool insidePolygon(const std::vector<QPointF> &polygon, QPointF p) {
    bool inside = false;
    for (std::size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
        const QPointF &a = polygon[i], &b = polygon[j];
        if ((a.y() > p.y()) != (b.y() > p.y()) && p.x() < (b.x() - a.x()) * (p.y() - a.y()) / (b.y() - a.y()) + a.x())
            inside = !inside;
    }
    return inside;
}

std::vector<QPointF> mapPoints(const QAbstractSeries *series, const QList<QPointF> &points, const QRectF &plot) {
    const Scale sx = ChartEngine::seriesScale(series, Qt::Horizontal);
    const Scale sy = ChartEngine::seriesScale(series, Qt::Vertical);
    std::vector<QPointF> out;
    out.reserve(static_cast<std::size_t>(points.size()));
    if (!sx.isValid() || !sy.isValid())
        return out;
    for (const QPointF &p : points) {
        if (!std::isfinite(p.x()) || !std::isfinite(p.y()) || (sx.log && p.x() <= 0.0) || (sy.log && p.y() <= 0.0))
            continue;
        out.emplace_back(clampCoord(sx.toPixel(p.x(), plot.left(), plot.right())),
                         clampCoord(sy.toPixel(p.y(), plot.bottom(), plot.top())));
    }
    return out;
}

svg::Path polyline(const std::vector<QPointF> &points, bool spline) {
    svg::Path path;
    if (points.empty())
        return path;
    path.moveTo({points[0].x(), points[0].y()});
    for (std::size_t i = 1; i < points.size(); ++i) {
        if (!spline) {
            path.lineTo({points[i].x(), points[i].y()});
            continue;
        }
        // Catmull-Rom tangents, as cubic Bézier control points.
        const QPointF p0 = points[i >= 2 ? i - 2 : i - 1], p1 = points[i - 1], p2 = points[i];
        const QPointF p3 = points[i + 1 < points.size() ? i + 1 : i];
        const QPointF c1 = p1 + (p2 - p0) / 6.0, c2 = p2 - (p3 - p1) / 6.0;
        path.cubicTo({c1.x(), c1.y()}, {c2.x(), c2.y()}, {p2.x(), p2.y()});
    }
    return path;
}

QString pointLabel(const QString &format, QPointF p) {
    QString text = format;
    text.replace(QStringLiteral("@xPoint"), number(p.x()));
    text.replace(QStringLiteral("@yPoint"), number(p.y()));
    return text;
}

QList<QPointF> areaUpper(const QAreaSeries *area) {
    return area->upperSeries() ? area->upperSeries()->points() : QList<QPointF>();
}

} // namespace

// ----------------------------------------------------------------------------------------------------------------
// Themes and formatting

const Theme &theme(QChart::ChartTheme which) {
    static const Theme themes[] = {
        makeTheme("#ffffff", "#1f2328", "#4a5058", "#9aa1a9", "#e4e7eb", "#f1f3f5", "#f5f7f9",
                  {"#2f7ed8", "#f28f43", "#3ba272", "#d64e4e", "#8e6bc9", "#a0785a", "#e377c2", "#7f8c8d"}),
        makeTheme("#0f2a44", "#eef5fb", "#b8cde0", "#5f7e9c", "#22405f", "#1a3652", "#13304d",
                  {"#53c5f5", "#f6d55c", "#9ae17b", "#f5925c", "#c39bf0", "#7fe0d0", "#f28cb1", "#c8d6e5"}),
        makeTheme("#1f2124", "#f1f3f4", "#c4c7c5", "#6b6f75", "#34373c", "#2a2d31", "#27292d",
                  {"#5aa9f0", "#ffaa5c", "#5cc98d", "#f07070", "#b393ea", "#c9a283", "#f29bd4", "#aab4bb"}),
        makeTheme("#f3ebdd", "#4a3b2a", "#6b5a45", "#a8977e", "#e1d5c0", "#ebe2d2", "#ede4d4",
                  {"#b5651d", "#6b8e23", "#c19a6b", "#8b4513", "#d2a24c", "#556b2f", "#a0522d", "#8f7a55"}),
        makeTheme("#f5f9fc", "#10324d", "#2d4f6b", "#8aa5bb", "#dde8f0", "#ebf1f6", "#eaf1f7",
                  {"#1d71b8", "#0aa5a5", "#5b9bd5", "#2f4b7c", "#7ec8e3", "#3d85a8", "#14507a", "#9cc3dd"}),
        makeTheme("#ffffff", "#000000", "#000000", "#000000", "#b0b0b0", "#d8d8d8", "#ececec",
                  {"#000000", "#e69f00", "#0072b2", "#d55e00", "#009e73", "#cc79a7", "#56b4e9", "#f0e442"}),
        makeTheme("#eef6fb", "#1b3a4b", "#36596b", "#93b3c4", "#d6e7f0", "#e4eff5", "#e3eff6",
                  {"#3a8fc5", "#6cc3e8", "#1f5f8b", "#9fd6ef", "#4e7fa8", "#b9e4f5", "#2b4f6e", "#7fb2d4"}),
        makeTheme("#ffffff", "#2e3436", "#4c5154", "#a0a5a8", "#e6e8ea", "#f2f3f4", "#f4f6f4",
                  {"#3fb950", "#1f883d", "#8dd35f", "#2e7d32", "#a5d6a7", "#56a05d", "#c5e1a5", "#1b5e20"}),
    };
    const int i = static_cast<int>(which);
    return themes[i >= 0 && i < 8 ? i : 0];
}

QString formatNumber(const QString &format, double value) {
    QString out;
    bool done = false;
    const qsizetype size = format.size();
    qsizetype i = 0;
    while (i < size) {
        const QChar c = format.at(i);
        if (c != u'%') {
            out += c;
            ++i;
            continue;
        }
        if (i + 1 < size && format.at(i + 1) == u'%') {
            out += u'%';
            i += 2;
            continue;
        }
        if (done) {
            out += c;
            ++i;
            continue;
        }
        qsizetype j = i + 1;
        bool left = false, plus = false, space = false, zero = false, alt = false;
        for (; j < size; ++j) {
            const char f = format.at(j).toLatin1();
            if (f == '-')
                left = true;
            else if (f == '+')
                plus = true;
            else if (f == ' ')
                space = true;
            else if (f == '0')
                zero = true;
            else if (f == '#')
                alt = true;
            else
                break;
        }
        int width = 0, precision = -1;
        for (; j < size && format.at(j).isDigit(); ++j)
            width = std::min(64, width * 10 + format.at(j).digitValue());
        if (j < size && format.at(j) == u'.') {
            precision = 0;
            for (++j; j < size && format.at(j).isDigit(); ++j)
                precision = std::min(32, precision * 10 + format.at(j).digitValue());
        }
        while (j < size && QStringLiteral("hlLqjzt").contains(format.at(j)))
            ++j;
        if (j >= size) {
            out += format.mid(i);
            break;
        }
        const char conv = format.at(j).toLatin1();
        QString digits, prefix;
        bool negative = false, numeric = true;
        switch (conv) {
        case 'd':
        case 'i':
        case 'u':
        case 'o':
        case 'x':
        case 'X': {
            const double r = std::isfinite(value) ? std::clamp(std::round(value), -9.2e18, 9.2e18) : 0.0;
            const auto n = static_cast<qlonglong>(r);
            negative = n < 0;
            const qulonglong magnitude = negative ? 0ULL - static_cast<qulonglong>(n) : static_cast<qulonglong>(n);
            const int base = conv == 'o' ? 8 : (conv == 'x' || conv == 'X') ? 16 : 10;
            digits = QString::number(magnitude, base);
            if (conv == 'X')
                digits = digits.toUpper();
            if (alt && base == 16 && magnitude != 0)
                prefix = conv == 'X' ? QStringLiteral("0X") : QStringLiteral("0x");
            if (alt && base == 8)
                prefix = QStringLiteral("0");
            if (precision >= 0 && digits.size() < precision)
                digits = QString(precision - digits.size(), u'0') + digits;
            break;
        }
        case 'f':
        case 'F':
        case 'e':
        case 'E':
        case 'g':
        case 'G': {
            negative = value < 0.0;
            const char lower = static_cast<char>(conv == 'F' ? 'f' : (conv | 0x20));
            if (std::isnan(value))
                digits = QStringLiteral("nan");
            else if (std::isinf(value))
                digits = QStringLiteral("inf");
            else
                digits = QString::number(std::fabs(value), lower, precision < 0 ? 6 : precision);
            if (conv == 'F' || conv == 'E' || conv == 'G')
                digits = digits.toUpper();
            break;
        }
        case 'c':
            numeric = false;
            if (std::isfinite(value) && value >= 32.0 && value < 0x10000)
                digits = QChar(static_cast<char16_t>(value));
            break;
        case 's':
            numeric = false;
            digits = number(value);
            break;
        default:
            out += format.mid(i, j - i + 1);
            i = j + 1;
            continue;
        }
        const QString sign = negative ? QStringLiteral("-")
                             : plus   ? QStringLiteral("+")
                             : space  ? QStringLiteral(" ")
                                      : QString();
        QString body = sign + prefix + digits;
        if (body.size() < width) {
            const qsizetype pad = width - body.size();
            if (left)
                body += QString(pad, u' ');
            else if (zero && numeric)
                body = sign + prefix + QString(pad, u'0') + digits;
            else
                body = QString(pad, u' ') + body;
        }
        out += body;
        done = true;
        i = j + 1;
    }
    return out;
}

QString formatTick(double value, const std::vector<double> &values) {
    if (std::fabs(value) >= 1e15 || (value != 0.0 && std::fabs(value) < 1e-6))
        return number(value);
    int decimals = 0;
    for (; decimals < 6; ++decimals) {
        const double scale = std::pow(10.0, decimals);
        bool exact = true;
        for (const double v : values) {
            const double s = v * scale;
            if (std::fabs(s - std::round(s)) > 1e-6 * std::max(1.0, std::fabs(s))) {
                exact = false;
                break;
            }
        }
        if (exact)
            break;
    }
    QString text = QString::number(value, 'f', decimals);
    bool allZero = true;
    for (const QChar c : text)
        if (c != u'-' && c != u'0' && c != u'.')
            allZero = false;
    if (allZero && text.startsWith(u'-'))
        text.remove(0, 1);
    return text;
}

// ----------------------------------------------------------------------------------------------------------------
// Scales and bounds

bool Scale::isValid() const {
    if (!std::isfinite(min) || !std::isfinite(max) || !(max > min))
        return false;
    return !log || (min > 0.0 && base > 0.0 && base != 1.0);
}

double Scale::toPixel(double value, double a0, double a1) const {
    double t = log ? (std::log(value) - std::log(min)) / (std::log(max) - std::log(min)) : (value - min) / (max - min);
    if (reverse)
        t = 1.0 - t;
    return a0 + (a1 - a0) * t;
}

double Scale::toValue(double pixel, double a0, double a1) const {
    double t = a1 != a0 ? (pixel - a0) / (a1 - a0) : 0.0;
    if (reverse)
        t = 1.0 - t;
    return log ? std::exp(std::log(min) + t * (std::log(max) - std::log(min))) : min + t * (max - min);
}

void Bounds::add(double x, double y) {
    if (!std::isfinite(x) || !std::isfinite(y))
        return;
    if (!valid) {
        minX = maxX = x;
        minY = maxY = y;
        valid = true;
    } else {
        minX = std::min(minX, x);
        maxX = std::max(maxX, x);
        minY = std::min(minY, y);
        maxY = std::max(maxY, y);
    }
    if (x > 0.0 && (minPositiveX <= 0.0 || x < minPositiveX))
        minPositiveX = x;
    if (y > 0.0 && (minPositiveY <= 0.0 || y < minPositiveY))
        minPositiveY = y;
}

Bounds ChartEngine::seriesBounds(const QAbstractSeries *series) {
    Bounds b;
    const QAbstractSeries::SeriesType type = series->type();
    if (isXYType(type)) {
        for (const QPointF &p : static_cast<const QXYSeries *>(series)->m_points)
            b.add(p.x(), p.y());
    } else if (type == QAbstractSeries::SeriesTypeArea) {
        const auto *area = static_cast<const QAreaSeries *>(series);
        const QList<QPointF> upper = areaUpper(area);
        for (const QPointF &p : upper)
            b.add(p.x(), p.y());
        if (area->lowerSeries()) {
            for (const QPointF &p : area->lowerSeries()->points())
                b.add(p.x(), p.y());
        } else if (!upper.isEmpty()) {
            b.add(upper.first().x(), 0.0);
        }
    } else if (isBarType(type)) {
        const auto *bars = static_cast<const QAbstractBarSeries *>(series);
        int n = 0;
        for (const QBarSet *set : bars->m_sets)
            n = std::max(n, set->count());
        if (n == 0)
            return b;
        double lo = 0.0, hi = 0.0;
        for (int c = 0; c < n; ++c) {
            double pos = 0.0, neg = 0.0;
            for (const QBarSet *set : bars->m_sets) {
                const double v = set->at(c);
                if (type == QAbstractSeries::SeriesTypeBar || type == QAbstractSeries::SeriesTypeHorizontalBar) {
                    lo = std::min(lo, v);
                    hi = std::max(hi, v);
                } else if (v >= 0.0) {
                    pos += v;
                } else {
                    neg += v;
                }
            }
            if (type == QAbstractSeries::SeriesTypeStackedBar ||
                type == QAbstractSeries::SeriesTypeHorizontalStackedBar) {
                lo = std::min(lo, neg);
                hi = std::max(hi, pos);
            }
        }
        if (type == QAbstractSeries::SeriesTypePercentBar || type == QAbstractSeries::SeriesTypeHorizontalPercentBar) {
            lo = 0.0;
            hi = 100.0;
        }
        const bool horizontal = isHorizontalBar(type);
        const double c0 = -0.5, c1 = n - 0.5;
        b.add(horizontal ? lo : c0, horizontal ? c0 : lo);
        b.add(horizontal ? hi : c1, horizontal ? c1 : hi);
    }
    return b;
}

QAbstractAxis *ChartEngine::seriesAxis(const QAbstractSeries *series, Qt::Orientation orientation) {
    for (QAbstractAxis *axis : series->m_axes)
        if (axis->m_orientation == orientation)
            return axis;
    return nullptr;
}

bool ChartEngine::axisRange(const QAbstractAxis *axis, double *min, double *max) {
    switch (axis->type()) {
    case QAbstractAxis::AxisTypeValue: {
        const auto *a = static_cast<const QValueAxis *>(axis);
        *min = a->min();
        *max = a->max();
        return true;
    }
    case QAbstractAxis::AxisTypeLogValue: {
        const auto *a = static_cast<const QLogValueAxis *>(axis);
        *min = a->min();
        *max = a->max();
        return true;
    }
    case QAbstractAxis::AxisTypeDateTime: {
        const auto *a = static_cast<const QDateTimeAxis *>(axis);
        *min = a->m_min;
        *max = a->m_max;
        return true;
    }
    case QAbstractAxis::AxisTypeBarCategory: {
        const auto *a = static_cast<const QBarCategoryAxis *>(axis);
        *min = a->m_min;
        *max = a->m_max;
        return true;
    }
    default:
        return false;
    }
}

void ChartEngine::setAxisRange(QAbstractAxis *axis, double min, double max, bool byUser) {
    const bool user = axis->m_userRange;
    switch (axis->type()) {
    case QAbstractAxis::AxisTypeValue:
        static_cast<QValueAxis *>(axis)->setRange(min, max);
        break;
    case QAbstractAxis::AxisTypeLogValue:
        static_cast<QLogValueAxis *>(axis)->setRange(min, max);
        break;
    case QAbstractAxis::AxisTypeDateTime:
        if (min <= max)
            static_cast<QDateTimeAxis *>(axis)->setRangeMSecs(min, max);
        break;
    case QAbstractAxis::AxisTypeBarCategory:
        if (min < max)
            static_cast<QBarCategoryAxis *>(axis)->setIndexRange(min, max);
        break;
    default:
        break;
    }
    axis->m_userRange = byUser || user;
}

void ChartEngine::rangeSetByUser(QAbstractAxis *axis) { axis->m_userRange = true; }

bool ChartEngine::hasUserRange(const QAbstractAxis *axis) { return axis->m_userRange; }

Scale ChartEngine::axisScale(const QAbstractAxis *axis) {
    Scale s;
    axisRange(axis, &s.min, &s.max);
    s.reverse = axis->m_reverse;
    if (axis->type() == QAbstractAxis::AxisTypeLogValue) {
        s.log = true;
        s.base = static_cast<const QLogValueAxis *>(axis)->base();
    }
    return s;
}

Scale ChartEngine::seriesScale(const QAbstractSeries *series, Qt::Orientation orientation) {
    if (const QAbstractAxis *axis = seriesAxis(series, orientation))
        return axisScale(axis);
    Scale s;
    const Bounds b = seriesBounds(series);
    if (!b.valid)
        return s;
    s.min = orientation == Qt::Horizontal ? b.minX : b.minY;
    s.max = orientation == Qt::Horizontal ? b.maxX : b.maxY;
    if (s.min == s.max) {
        s.min -= 1.0;
        s.max += 1.0;
    }
    return s;
}

QPointF ChartEngine::toPosition(const QAbstractSeries *series, const QPointF &value, const QRectF &plot) {
    const Scale sx = seriesScale(series, Qt::Horizontal), sy = seriesScale(series, Qt::Vertical);
    if (!sx.isValid() || !sy.isValid())
        return QPointF();
    return QPointF(sx.toPixel(value.x(), plot.left(), plot.right()), sy.toPixel(value.y(), plot.bottom(), plot.top()));
}

QPointF ChartEngine::toValue(const QAbstractSeries *series, const QPointF &position, const QRectF &plot) {
    const Scale sx = seriesScale(series, Qt::Horizontal), sy = seriesScale(series, Qt::Vertical);
    if (!sx.isValid() || !sy.isValid())
        return QPointF();
    return QPointF(sx.toValue(position.x(), plot.left(), plot.right()),
                   sy.toValue(position.y(), plot.bottom(), plot.top()));
}

void ChartEngine::fitAxis(QAbstractAxis *axis) {
    if (axis->m_userRange)
        return;
    double lo = 0.0, hi = 0.0, minPositive = 0.0;
    bool any = false;
    for (const QAbstractSeries *series : std::as_const(axis->m_series)) {
        const Bounds b = seriesBounds(series);
        if (!b.valid)
            continue;
        const bool h = axis->m_orientation == Qt::Horizontal;
        const double l = h ? b.minX : b.minY, u = h ? b.maxX : b.maxY, p = h ? b.minPositiveX : b.minPositiveY;
        lo = any ? std::min(lo, l) : l;
        hi = any ? std::max(hi, u) : u;
        if (p > 0.0)
            minPositive = minPositive > 0.0 ? std::min(minPositive, p) : p;
        any = true;
    }
    if (!any)
        return;
    switch (axis->type()) {
    case QAbstractAxis::AxisTypeValue:
        if (lo == hi) {
            lo -= 1.0;
            hi += 1.0;
        }
        setAxisRange(axis, lo, hi, false);
        break;
    case QAbstractAxis::AxisTypeLogValue: {
        if (!(minPositive > 0.0))
            return;
        const double base = static_cast<QLogValueAxis *>(axis)->base();
        lo = minPositive;
        hi = std::max(hi, lo);
        if (lo == hi) {
            lo /= base;
            hi *= base;
        }
        setAxisRange(axis, lo, hi, false);
        break;
    }
    case QAbstractAxis::AxisTypeDateTime:
        if (lo == hi) {
            lo -= 1000.0;
            hi += 1000.0;
        }
        setAxisRange(axis, lo, hi, false);
        break;
    case QAbstractAxis::AxisTypeBarCategory: {
        auto *categories = static_cast<QBarCategoryAxis *>(axis);
        if (categories->count() == 0) {
            const long n = std::lround(hi + 0.5);
            QStringList names;
            for (long i = 1; i <= std::min(n, 10000L); ++i)
                names.append(QString::number(i));
            categories->append(names);
        } else {
            setAxisRange(axis, -0.5, categories->count() - 0.5, false);
        }
        break;
    }
    default:
        break;
    }
}

AxisTicks ChartEngine::ticks(const QAbstractAxis *axis) {
    AxisTicks t;
    double lo = 0.0, hi = 0.0;
    if (!axisRange(axis, &lo, &hi) || !std::isfinite(lo) || !std::isfinite(hi) || hi < lo)
        return t;
    const double eps = (hi - lo) * 1e-9;
    const auto addMinor = [&](int count, bool log) {
        if (count <= 0)
            return;
        for (std::size_t i = 0; i + 1 < t.grid.size(); ++i)
            for (int k = 1; k <= count; ++k) {
                const double a = t.grid[i], b = t.grid[i + 1];
                (void)log;
                t.minor.push_back(a + (b - a) * k / (count + 1));
            }
    };
    switch (axis->type()) {
    case QAbstractAxis::AxisTypeValue: {
        const auto *a = static_cast<const QValueAxis *>(axis);
        if (hi == lo)
            return t;
        if (a->tickType() == QValueAxis::TicksDynamic && a->tickInterval() > 0.0) {
            const double step = a->tickInterval();
            const double first = a->tickAnchor() + std::ceil((lo - a->tickAnchor()) / step - 1e-9) * step;
            for (int i = 0; i < kMaxTicks; ++i) {
                const double v = first + i * step;
                if (v > hi + eps)
                    break;
                t.grid.push_back(std::fabs(v) < step * 1e-9 ? 0.0 : v);
            }
        } else {
            const int n = std::min(a->tickCount(), kMaxTicks);
            for (int i = 0; i < n; ++i)
                t.grid.push_back(lo + (hi - lo) * i / (n - 1));
        }
        addMinor(a->minorTickCount(), false);
        t.labelValues = t.grid;
        for (const double v : t.grid)
            t.labels.append(a->labelFormat().isEmpty() ? formatTick(v, t.grid) : formatNumber(a->labelFormat(), v));
        break;
    }
    case QAbstractAxis::AxisTypeLogValue: {
        const auto *a = static_cast<const QLogValueAxis *>(axis);
        if (!(lo > 0.0) || hi == lo)
            return t;
        const double lb = std::log(a->base());
        const double first = std::ceil(std::log(lo) / lb - 1e-9), last = std::floor(std::log(hi) / lb + 1e-9);
        for (double k = first; k <= last && t.grid.size() < static_cast<std::size_t>(kMaxTicks); k += 1.0)
            t.grid.push_back(std::pow(a->base(), k));
        addMinor(a->minorTickCount(), true);
        t.labelValues = t.grid;
        for (const double v : t.grid)
            t.labels.append(a->labelFormat().isEmpty() ? number(v) : formatNumber(a->labelFormat(), v));
        break;
    }
    case QAbstractAxis::AxisTypeDateTime: {
        const auto *a = static_cast<const QDateTimeAxis *>(axis);
        if (hi == lo)
            return t;
        const int n = std::min(a->tickCount(), kMaxTicks);
        for (int i = 0; i < n; ++i)
            t.grid.push_back(lo + (hi - lo) * i / (n - 1));
        t.labelValues = t.grid;
        for (const double v : t.grid)
            t.labels.append(QDateTime::fromMSecsSinceEpoch(std::llround(v)).toString(a->format()));
        break;
    }
    case QAbstractAxis::AxisTypeBarCategory: {
        const auto *a = static_cast<const QBarCategoryAxis *>(axis);
        if (hi <= lo)
            return t;
        const long first = std::lround(std::ceil(lo + 0.5 - 1e-9)), last = std::lround(std::floor(hi - 0.5 + 1e-9));
        for (long i = first; i <= last && t.labels.size() < kMaxTicks; ++i) {
            t.labelValues.push_back(static_cast<double>(i));
            const int index = static_cast<int>(i);
            t.labels.append(index < a->count() ? a->at(index) : QString::number(i + 1));
        }
        for (long i = first; i <= last + 1 && t.grid.size() < static_cast<std::size_t>(kMaxTicks); ++i)
            t.grid.push_back(static_cast<double>(i) - 0.5);
        break;
    }
    default:
        break;
    }
    return t;
}

// ----------------------------------------------------------------------------------------------------------------
// Structure

QLegend *ChartEngine::createLegend(QChart *chart) { return new QLegend(chart); }

void ChartEngine::setView(QChart *chart, QWidget *view) { chart->m_view = view; }

void ChartEngine::changed(QChart *chart) {
    if (chart && chart->m_view)
        chart->m_view->update();
}

void ChartEngine::changed(QAbstractSeries *series) { changed(series->m_chart); }

void ChartEngine::changed(QAbstractAxis *axis) { changed(axis->m_chart); }

void ChartEngine::changed(QBarSet *set) {
    if (set->m_series)
        changed(set->m_series->chart());
}

void ChartEngine::changed(QPieSlice *slice) {
    if (slice->m_series)
        changed(slice->m_series->chart());
}

void ChartEngine::forgetHits(QChart *chart, const QObject *object) {
    if (chart->m_hoverSeries == object || chart->m_hoverItem == object) {
        chart->m_hoverSeries = nullptr;
        chart->m_hoverItem = nullptr;
    }
    if (chart->m_pressSeries == object || chart->m_pressItem == object) {
        chart->m_pressSeries = nullptr;
        chart->m_pressItem = nullptr;
    }
}

void ChartEngine::addSeries(QChart *chart, QAbstractSeries *series) {
    if (!series || series->m_chart)
        return;
    chart->m_series.append(series);
    series->m_chart = chart;
    series->setParent(chart);
    changed(chart);
}

void ChartEngine::removeSeries(QChart *chart, QAbstractSeries *series) {
    if (!series || series->m_chart != chart)
        return;
    for (QAbstractAxis *axis : std::as_const(series->m_axes))
        axis->m_series.removeAll(series);
    series->m_axes.clear();
    chart->m_series.removeAll(series);
    series->m_chart = nullptr;
    forgetHits(chart, series);
    series->setParent(nullptr);
    changed(chart);
}

void ChartEngine::seriesDestroyed(QAbstractSeries *series) {
    QChart *chart = series->m_chart;
    for (QAbstractAxis *axis : std::as_const(series->m_axes))
        axis->m_series.removeAll(series);
    series->m_axes.clear();
    if (!chart)
        return;
    chart->m_series.removeAll(series);
    series->m_chart = nullptr;
    forgetHits(chart, series);
    changed(chart);
}

void ChartEngine::addAxis(QChart *chart, QAbstractAxis *axis, Qt::Alignment alignment) {
    if (!axis || (axis->m_chart && axis->m_chart != chart))
        return;
    Qt::Alignment side = Qt::AlignBottom;
    if (alignment & Qt::AlignLeft)
        side = Qt::AlignLeft;
    else if (alignment & Qt::AlignRight)
        side = Qt::AlignRight;
    else if (alignment & Qt::AlignTop)
        side = Qt::AlignTop;
    axis->m_alignment = side;
    axis->m_orientation = (side == Qt::AlignLeft || side == Qt::AlignRight) ? Qt::Vertical : Qt::Horizontal;
    if (!axis->m_chart) {
        chart->m_axes.append(axis);
        axis->m_chart = chart;
        axis->setParent(chart);
    }
    changed(chart);
}

void ChartEngine::removeAxis(QChart *chart, QAbstractAxis *axis) {
    if (!axis || axis->m_chart != chart)
        return;
    for (QAbstractSeries *series : std::as_const(axis->m_series))
        series->m_axes.removeAll(axis);
    axis->m_series.clear();
    chart->m_axes.removeAll(axis);
    chart->m_zoomOrigin.remove(axis);
    axis->m_chart = nullptr;
    axis->m_alignment = Qt::Alignment();
    axis->setParent(nullptr);
    changed(chart);
}

void ChartEngine::axisDestroyed(QAbstractAxis *axis) {
    for (QAbstractSeries *series : std::as_const(axis->m_series))
        series->m_axes.removeAll(axis);
    axis->m_series.clear();
    if (QChart *chart = axis->m_chart) {
        chart->m_axes.removeAll(axis);
        chart->m_zoomOrigin.remove(axis);
        axis->m_chart = nullptr;
        changed(chart);
    }
}

bool ChartEngine::attach(QAbstractSeries *series, QAbstractAxis *axis) {
    if (!series || !axis || !series->m_chart || axis->m_chart != series->m_chart || series->m_axes.contains(axis))
        return false;
    if (QAbstractAxis *previous = seriesAxis(series, axis->m_orientation))
        detach(series, previous);
    series->m_axes.append(axis);
    axis->m_series.append(series);
    fitAxis(axis);
    changed(series->m_chart);
    return true;
}

bool ChartEngine::detach(QAbstractSeries *series, QAbstractAxis *axis) {
    if (!series || !axis || !series->m_axes.contains(axis))
        return false;
    series->m_axes.removeAll(axis);
    axis->m_series.removeAll(series);
    changed(series->m_chart);
    return true;
}

void ChartEngine::createDefaultAxes(QChart *chart) {
    const QList<QAbstractAxis *> old = chart->m_axes;
    for (QAbstractAxis *axis : old) {
        removeAxis(chart, axis);
        delete axis;
    }
    bool any = false, categoryX = false, categoryY = false;
    for (const QAbstractSeries *series : std::as_const(chart->m_series)) {
        const QAbstractSeries::SeriesType type = series->type();
        if (type == QAbstractSeries::SeriesTypePie)
            continue;
        any = true;
        if (isBarType(type)) {
            categoryX = categoryX || !isHorizontalBar(type);
            categoryY = categoryY || isHorizontalBar(type);
        }
    }
    if (!any)
        return;
    // New axes are in no chart yet, so addAxis() takes them: the chart owns them.
    QAbstractAxis *x = categoryX ? static_cast<QAbstractAxis *>(new QBarCategoryAxis) : new QValueAxis;
    QAbstractAxis *y = categoryY ? static_cast<QAbstractAxis *>(new QBarCategoryAxis) : new QValueAxis;
    x->m_chart = nullptr;
    y->m_chart = nullptr;
    addAxis(chart, x, Qt::AlignBottom);
    addAxis(chart, y, Qt::AlignLeft);
    for (QAbstractSeries *series : std::as_const(chart->m_series)) {
        if (series->type() == QAbstractSeries::SeriesTypePie)
            continue;
        attach(series, x);
        attach(series, y);
    }
}

void ChartEngine::destroyChart(QChart *chart) {
    chart->m_view = nullptr;
    const QList<QAbstractSeries *> series = chart->m_series;
    const QList<QAbstractAxis *> axes = chart->m_axes;
    for (QAbstractSeries *s : series) {
        s->m_chart = nullptr;
        s->m_axes.clear();
    }
    for (QAbstractAxis *a : axes) {
        a->m_chart = nullptr;
        a->m_series.clear();
    }
    chart->m_series.clear();
    chart->m_axes.clear();
    qDeleteAll(series);
    qDeleteAll(axes);
    delete chart->m_legend;
    chart->m_legend = nullptr;
}

QAbstractBarSeries *ChartEngine::barSetSeries(const QBarSet *set) { return set->m_series; }

void ChartEngine::setBarSetSeries(QBarSet *set, QAbstractBarSeries *series) { set->m_series = series; }

void ChartEngine::releaseBarSet(QBarSet *set) {
    if (set->m_series && set->m_series->chart())
        forgetHits(set->m_series->chart(), set);
    set->m_series = nullptr;
}

void ChartEngine::barSetDestroyed(QBarSet *set) {
    QAbstractBarSeries *series = set->m_series;
    if (!series)
        return;
    releaseBarSet(set);
    series->m_sets.removeAll(set);
    emit series->barsetsRemoved(QList<QBarSet *>{set});
    emit series->countChanged();
    changed(series->chart());
}

void ChartEngine::sliceDestroyed(QPieSlice *slice) {
    QPieSeries *series = slice->m_series;
    if (!series)
        return;
    if (series->chart())
        forgetHits(series->chart(), slice);
    slice->m_series = nullptr;
    series->m_slices.removeAll(slice);
    series->updateAngles();
    emit series->removed(QList<QPieSlice *>{slice});
    emit series->countChanged();
    changed(series->chart());
}

void ChartEngine::forgetStyle(QChart *chart) {
    chart->m_titleFontSet = chart->m_titleBrushSet = false;
    chart->m_backgroundBrushSet = chart->m_backgroundPenSet = false;
    chart->m_plotBrushSet = chart->m_plotPenSet = false;
    QLegend *legend = chart->m_legend;
    legend->m_brushSet = legend->m_penSet = legend->m_labelBrushSet = false;
    for (QAbstractAxis *a : std::as_const(chart->m_axes)) {
        a->m_linePenSet = a->m_gridPenSet = a->m_minorGridPenSet = false;
        a->m_labelsBrushSet = a->m_titleBrushSet = a->m_shadesPenSet = a->m_shadesBrushSet = false;
    }
    for (QAbstractSeries *s : std::as_const(chart->m_series)) {
        const QAbstractSeries::SeriesType type = s->type();
        if (isXYType(type)) {
            auto *xy = static_cast<QXYSeries *>(s);
            xy->m_penSet = xy->m_brushSet = xy->m_pointLabelsColorSet = false;
        } else if (type == QAbstractSeries::SeriesTypeArea) {
            auto *area = static_cast<QAreaSeries *>(s);
            area->m_penSet = area->m_brushSet = false;
        } else if (isBarType(type)) {
            for (QBarSet *set : std::as_const(static_cast<QAbstractBarSeries *>(s)->m_sets))
                set->m_penSet = set->m_brushSet = set->m_labelBrushSet = false;
        } else if (type == QAbstractSeries::SeriesTypePie) {
            for (QPieSlice *slice : std::as_const(static_cast<QPieSeries *>(s)->m_slices))
                slice->m_penSet = slice->m_brushSet = slice->m_labelBrushSet = false;
        }
    }
}

// ----------------------------------------------------------------------------------------------------------------
// Theme lookups

const Theme &ChartEngine::themeOf(const QChart *chart) {
    return theme(chart ? chart->m_theme : QChart::ChartThemeLight);
}

static QColor paletteColor(const QChart *chart, qsizetype index) {
    const std::vector<QColor> &palette = ChartEngine::themeOf(chart).palette;
    return palette[static_cast<std::size_t>(std::max<qsizetype>(0, index)) % palette.size()];
}

// The first palette entry of a series: each bar set takes an entry of its own, and pie series take none, since
// their slices go through the palette from the start.
static qsizetype colorSlot(const QChart *chart, const QAbstractSeries *series) {
    if (!chart)
        return 0;
    qsizetype slot = 0;
    for (const QAbstractSeries *s : chart->series()) {
        if (s == series)
            break;
        const QAbstractSeries::SeriesType type = s->type();
        if (isBarType(type))
            slot += std::max(1, static_cast<const QAbstractBarSeries *>(s)->count());
        else if (type != QAbstractSeries::SeriesTypePie)
            ++slot;
    }
    return slot;
}

QColor ChartEngine::seriesColor(const QAbstractSeries *series) {
    const QChart *chart = series->m_chart;
    return paletteColor(chart, colorSlot(chart, series));
}

QColor ChartEngine::barSetColor(const QBarSet *set) {
    const QAbstractBarSeries *series = set->m_series;
    if (!series)
        return paletteColor(nullptr, 0);
    const QChart *chart = series->chart();
    return paletteColor(chart, colorSlot(chart, series) + series->m_sets.indexOf(const_cast<QBarSet *>(set)));
}

QColor ChartEngine::sliceColor(const QPieSlice *slice) {
    const QPieSeries *series = slice->m_series;
    if (!series)
        return paletteColor(nullptr, 0);
    return paletteColor(series->chart(), series->m_slices.indexOf(const_cast<QPieSlice *>(slice)));
}

QColor ChartEngine::backgroundColor(const QChart *chart) {
    return chart ? chart->backgroundBrush().color() : themeOf(nullptr).background;
}

// ----------------------------------------------------------------------------------------------------------------
// Layout

static std::vector<LegendEntry> legendEntries(const QChart *chart) {
    std::vector<LegendEntry> entries;
    for (QAbstractSeries *series : chart->series()) {
        if (!series->isVisible())
            continue;
        const QAbstractSeries::SeriesType type = series->type();
        if (isBarType(type)) {
            for (QBarSet *set : static_cast<QAbstractBarSeries *>(series)->barSets())
                entries.push_back({set->label(), set->brush().color(), series, {}, {}});
        } else if (type == QAbstractSeries::SeriesTypePie) {
            for (QPieSlice *slice : static_cast<QPieSeries *>(series)->slices())
                entries.push_back({slice->label(), slice->brush().color(), series, {}, {}});
        } else if (type == QAbstractSeries::SeriesTypeArea) {
            entries.push_back({series->name(), static_cast<QAreaSeries *>(series)->color(), series, {}, {}});
        } else {
            entries.push_back({series->name(), static_cast<QXYSeries *>(series)->color(), series, {}, {}});
        }
    }
    if (chart->legend()->reverseMarkers())
        std::reverse(entries.begin(), entries.end());
    return entries;
}

static void layoutLegend(const QChart *chart, QRectF *inner, Layout *out) {
    const QLegend *legend = chart->legend();
    if (!legend->isVisible())
        return;
    std::vector<LegendEntry> entries = legendEntries(chart);
    if (entries.empty())
        return;
    const QFontMetrics fm(legend->font());
    const double rowHeight = std::max<double>(fm.height(), 12.0);
    const double marker = std::round(rowHeight * 0.7);
    const double pad = 4.0, spacing = 16.0, gap = 6.0;
    const Qt::Alignment side = legend->alignment();
    const bool vertical = (side & (Qt::AlignLeft | Qt::AlignRight)) && !(side & (Qt::AlignTop | Qt::AlignBottom));
    const double maxText = vertical ? inner->width() / 3.0 : inner->width() - 2 * pad - marker - gap;
    std::vector<double> widths;
    for (LegendEntry &e : entries) {
        e.text = fm.elidedText(e.text, Qt::ElideRight, std::max(10, static_cast<int>(maxText)));
        widths.push_back(fm.horizontalAdvance(e.text));
    }
    if (vertical) {
        double width = 0.0;
        for (const double w : widths)
            width = std::max(width, marker + gap + w);
        width += 2 * pad;
        const auto count = static_cast<double>(entries.size());
        const double height = count * rowHeight + (count - 1.0) * 4.0 + 2 * pad;
        const double x = (side & Qt::AlignLeft) ? inner->left() : inner->right() - width;
        const double y = inner->center().y() - height / 2.0;
        out->legendRect = QRectF(x, y, width, height);
        double cy = y + pad;
        for (std::size_t i = 0; i < entries.size(); ++i) {
            entries[i].marker = QRectF(x + pad, cy + (rowHeight - marker) / 2.0, marker, marker);
            entries[i].textRect = QRectF(x + pad + marker + gap, cy, widths[i] + 2.0, rowHeight);
            cy += rowHeight + 4.0;
        }
        if (side & Qt::AlignLeft)
            inner->setLeft(inner->left() + width + kStack);
        else
            inner->setRight(inner->right() - width - kStack);
    } else {
        // Rows that wrap at the width of the chart, each centered.
        std::vector<std::vector<std::size_t>> rows(1);
        std::vector<double> rowWidths(1, 0.0);
        const double avail = inner->width() - 2 * pad;
        for (std::size_t i = 0; i < entries.size(); ++i) {
            const double w = marker + gap + widths[i];
            const double extra = rows.back().empty() ? w : spacing + w;
            if (!rows.back().empty() && rowWidths.back() + extra > avail) {
                rows.emplace_back();
                rowWidths.push_back(0.0);
            }
            rowWidths.back() += rows.back().empty() ? w : spacing + w;
            rows.back().push_back(i);
        }
        double width = 0.0;
        for (const double w : rowWidths)
            width = std::max(width, w);
        width += 2 * pad;
        const auto rowCount = static_cast<double>(rows.size());
        const double height = rowCount * rowHeight + (rowCount - 1.0) * 4.0 + 2 * pad;
        const double y = (side & Qt::AlignBottom) ? inner->bottom() - height : inner->top();
        out->legendRect = QRectF(inner->center().x() - width / 2.0, y, width, height);
        double cy = y + pad;
        for (std::size_t r = 0; r < rows.size(); ++r) {
            double cx = inner->center().x() - rowWidths[r] / 2.0;
            for (const std::size_t i : rows[r]) {
                entries[i].marker = QRectF(cx, cy + (rowHeight - marker) / 2.0, marker, marker);
                entries[i].textRect = QRectF(cx + marker + gap, cy, widths[i] + 2.0, rowHeight);
                cx += marker + gap + widths[i] + spacing;
            }
            cy += rowHeight + 4.0;
        }
        if (side & Qt::AlignBottom)
            inner->setBottom(inner->bottom() - height - kGap);
        else
            inner->setTop(inner->top() + height + kGap);
    }
    out->legend = std::move(entries);
}

Layout ChartEngine::layout(QChart *chart) {
    Layout L;
    const QSizeF size = chart->m_geometry.size();
    const QMargins &m = chart->m_margins;
    QRectF inner = QRectF(QPointF(0, 0), size).adjusted(m.left(), m.top(), -m.right(), -m.bottom());
    if (!chart->m_title.isEmpty()) {
        const QFontMetrics fm(chart->titleFont());
        L.titleRect = QRectF(inner.left(), inner.top(), inner.width(), fm.height());
        inner.setTop(inner.top() + fm.height() + kStack);
    }
    layoutLegend(chart, &inner, &L);

    // Room for the axes on each side, and for the titles of the vertical axes, above the plot area.
    double side[4] = {0, 0, 0, 0}; // left, right, top, bottom
    double verticalTitle = 0.0, halfLabelWidth = 0.0, halfLabelHeight = 0.0;
    for (QAbstractAxis *axis : std::as_const(chart->m_axes)) {
        if (!axis->isVisible())
            continue;
        const AxisTicks t = ticks(axis);
        const QFontMetrics fm(axis->labelsFont());
        double labelWidth = 0.0;
        if (axis->labelsVisible())
            for (const QString &label : t.labels)
                labelWidth = std::max<double>(labelWidth, fm.horizontalAdvance(label));
        const bool hasTitle = axis->isTitleVisible() && !axis->titleText().isEmpty();
        const double titleHeight = hasTitle ? QFontMetrics(axis->titleFont()).height() + kGap : 0.0;
        const int index = axis->m_alignment == Qt::AlignLeft    ? 0
                          : axis->m_alignment == Qt::AlignRight ? 1
                          : axis->m_alignment == Qt::AlignTop   ? 2
                                                                : 3;
        AxisPlace place;
        place.axis = axis;
        place.offset = side[index] > 0.0 ? side[index] + kStack : 0.0;
        if (index < 2) {
            place.extent = kTick + kGap + labelWidth;
            verticalTitle = std::max(verticalTitle, titleHeight);
            if (axis->labelsVisible() && axis->type() != QAbstractAxis::AxisTypeBarCategory)
                halfLabelHeight = std::max(halfLabelHeight, fm.height() / 2.0);
        } else {
            place.extent = kTick + kGap + (axis->labelsVisible() ? fm.height() : 0) + titleHeight;
            if (axis->labelsVisible() && axis->type() != QAbstractAxis::AxisTypeBarCategory && !t.labels.isEmpty())
                halfLabelWidth = std::max<double>({halfLabelWidth, fm.horizontalAdvance(t.labels.first()) / 2.0,
                                                   fm.horizontalAdvance(t.labels.last()) / 2.0});
        }
        side[index] = place.offset + place.extent;
        L.axes.push_back(place);
    }
    QRectF plot = inner.adjusted(side[0], side[2] + verticalTitle, -side[1], -side[3]);
    plot.setLeft(std::max(plot.left(), inner.left() + halfLabelWidth));
    plot.setRight(std::min(plot.right(), inner.right() - halfLabelWidth));
    plot.setTop(std::max(plot.top(), inner.top() + halfLabelHeight));
    plot.setBottom(std::min(plot.bottom(), inner.bottom() - halfLabelHeight));
    if (plot.width() < 1.0)
        plot.setWidth(1.0);
    if (plot.height() < 1.0)
        plot.setHeight(1.0);
    if (chart->m_fixedPlotArea)
        plot = chart->m_plotArea;
    L.plot = plot;
    if (plot != chart->m_plotArea) {
        chart->m_plotArea = plot;
        emit chart->plotAreaChanged(plot);
    }
    return L;
}

std::vector<BarRect> ChartEngine::bars(const QAbstractBarSeries *series, const QRectF &plot) {
    std::vector<BarRect> out;
    const QAbstractSeries::SeriesType type = series->type();
    const bool horizontal = isHorizontalBar(type);
    const bool grouped = type == QAbstractSeries::SeriesTypeBar || type == QAbstractSeries::SeriesTypeHorizontalBar;
    const bool percent =
        type == QAbstractSeries::SeriesTypePercentBar || type == QAbstractSeries::SeriesTypeHorizontalPercentBar;
    const Scale cat = seriesScale(series, horizontal ? Qt::Vertical : Qt::Horizontal);
    const Scale val = seriesScale(series, horizontal ? Qt::Horizontal : Qt::Vertical);
    if (!cat.isValid() || !val.isValid())
        return out;
    const QList<QBarSet *> &sets = series->m_sets;
    int n = 0;
    for (const QBarSet *set : sets)
        n = std::max(n, set->count());
    const double width = series->barWidth();
    const auto setCount = static_cast<double>(sets.size());
    const auto place = [&](QBarSet *set, int index, double value, double c0, double c1, double v0, double v1) {
        if (val.log) {
            v0 = std::max(v0, val.min);
            v1 = std::max(v1, val.min);
        }
        const double cp0 =
            horizontal ? cat.toPixel(c0, plot.bottom(), plot.top()) : cat.toPixel(c0, plot.left(), plot.right());
        const double cp1 =
            horizontal ? cat.toPixel(c1, plot.bottom(), plot.top()) : cat.toPixel(c1, plot.left(), plot.right());
        const double vp0 =
            horizontal ? val.toPixel(v0, plot.left(), plot.right()) : val.toPixel(v0, plot.bottom(), plot.top());
        const double vp1 =
            horizontal ? val.toPixel(v1, plot.left(), plot.right()) : val.toPixel(v1, plot.bottom(), plot.top());
        const QRectF r =
            horizontal ? QRectF(QPointF(clampCoord(vp0), clampCoord(cp0)), QPointF(clampCoord(vp1), clampCoord(cp1)))
                       : QRectF(QPointF(clampCoord(cp0), clampCoord(vp0)), QPointF(clampCoord(cp1), clampCoord(vp1)));
        out.push_back({set, index, value, r.normalized()});
    };
    for (int c = 0; c < n; ++c) {
        double pos = 0.0, neg = 0.0, total = 0.0;
        if (percent)
            for (const QBarSet *set : sets)
                total += std::max(0.0, set->at(c));
        for (qsizetype s = 0; s < sets.size(); ++s) {
            QBarSet *set = sets.at(s);
            if (c >= set->count())
                continue;
            const double v = set->at(c);
            if (grouped) {
                const double w = width / setCount;
                const double start = c - width / 2.0 + static_cast<double>(s) * w;
                place(set, c, v, start, start + w, 0.0, v);
            } else if (percent) {
                const double share = total > 0.0 ? std::max(0.0, v) / total * 100.0 : 0.0;
                place(set, c, v, c - width / 2.0, c + width / 2.0, pos, pos + share);
                pos += share;
            } else if (v >= 0.0) {
                place(set, c, v, c - width / 2.0, c + width / 2.0, pos, pos + v);
                pos += v;
            } else {
                place(set, c, v, c - width / 2.0, c + width / 2.0, neg, neg + v);
                neg += v;
            }
        }
    }
    return out;
}

std::vector<SliceGeometry> ChartEngine::slices(const QPieSeries *series, const QRectF &plot) {
    std::vector<SliceGeometry> out;
    const double side = std::min(plot.width(), plot.height());
    const QPointF center(plot.left() + plot.width() * series->m_horizontal,
                         plot.top() + plot.height() * series->m_vertical);
    // Outside labels must fit in the plot area: the pie shrinks to leave them room.
    double outer = side * series->m_size / 2.0;
    for (QPieSlice *slice : series->m_slices) {
        if (!slice->isLabelVisible() || slice->label().isEmpty() || slice->labelPosition() != QPieSlice::LabelOutside ||
            slice->angleSpan() == 0.0)
            continue;
        const QFontMetrics fm(slice->labelFont());
        const double mid = (slice->startAngle() + slice->angleSpan() / 2.0) * kPi / 180.0;
        const double reach =
            1.0 + slice->labelArmLengthFactor() + (slice->isExploded() ? slice->explodeDistanceFactor() : 0.0);
        const double w = fm.horizontalAdvance(slice->label()) + 15.0, h = fm.height() / 2.0;
        const double sx = std::fabs(std::sin(mid)), sy = std::fabs(std::cos(mid));
        const double roomX = std::min(center.x() - plot.left(), plot.right() - center.x()) - w;
        const double roomY = std::min(center.y() - plot.top(), plot.bottom() - center.y()) - h;
        if (sx > 1e-6)
            outer = std::min(outer, roomX / (reach * sx));
        if (sy > 1e-6)
            outer = std::min(outer, roomY / (reach * sy));
    }
    outer = std::max(outer, side * 0.1);
    for (QPieSlice *slice : series->m_slices) {
        SliceGeometry g;
        g.slice = slice;
        g.outer = outer;
        g.inner = std::min(outer, outer * series->m_holeSize / std::max(series->m_size, 1e-9));
        g.start = slice->startAngle();
        g.span = slice->angleSpan();
        g.center = slice->isExploded()
                       ? onCircle(center, g.outer * slice->explodeDistanceFactor(), g.start + g.span / 2.0)
                       : center;
        if (g.span != 0.0)
            out.push_back(g);
    }
    return out;
}

// ----------------------------------------------------------------------------------------------------------------
// Drawing

namespace {

void drawAxisBackground(const Canvas &c, const Layout &L) {
    for (const AxisPlace &place : L.axes) {
        const QAbstractAxis *axis = place.axis;
        const AxisTicks t = ChartEngine::ticks(axis);
        const Scale s = ChartEngine::axisScale(axis);
        if (!s.isValid())
            continue;
        const bool h = axis->orientation() == Qt::Horizontal;
        const QRectF &p = L.plot;
        const auto pixel = [&](double v) {
            return h ? s.toPixel(v, p.left(), p.right()) : s.toPixel(v, p.bottom(), p.top());
        };
        if (axis->shadesVisible()) {
            for (std::size_t i = 0; i + 1 < t.grid.size(); i += 2) {
                const double a = pixel(t.grid[i]), b = pixel(t.grid[i + 1]);
                const QRectF r = h ? QRectF(QPointF(a, p.top()), QPointF(b, p.bottom()))
                                   : QRectF(QPointF(p.left(), a), QPointF(p.right(), b));
                c.rect(r.normalized(), axis->shadesBrush(), axis->shadesPen());
            }
        }
        const auto gridLines = [&](const std::vector<double> &values, const QPen &pen) {
            for (const double v : values) {
                const double x = pixel(v);
                if (x < (h ? p.left() : p.top()) - 0.5 || x > (h ? p.right() : p.bottom()) + 0.5)
                    continue;
                const double at = c.snap(x, pen.widthF());
                if (h)
                    c.line(QPointF(at, p.top()), QPointF(at, p.bottom()), pen);
                else
                    c.line(QPointF(p.left(), at), QPointF(p.right(), at), pen);
            }
        };
        if (axis->isMinorGridLineVisible())
            gridLines(t.minor, axis->minorGridLinePen());
        if (axis->isGridLineVisible())
            gridLines(t.grid, axis->gridLinePen());
    }
}

void drawAxes(const Canvas &c, const Layout &L, std::vector<TextItem> *texts) {
    for (const AxisPlace &place : L.axes) {
        const QAbstractAxis *axis = place.axis;
        const Scale s = ChartEngine::axisScale(axis);
        const AxisTicks t = ChartEngine::ticks(axis);
        const QRectF &p = L.plot;
        const Qt::Alignment side = axis->alignment();
        const bool h = axis->orientation() == Qt::Horizontal;
        const QPen pen = axis->linePen();
        // The position of the axis line, and the direction away from the plot area.
        double at = 0.0, dir = 1.0;
        if (side == Qt::AlignLeft) {
            at = p.left() - place.offset;
            dir = -1.0;
        } else if (side == Qt::AlignRight) {
            at = p.right() + place.offset;
        } else if (side == Qt::AlignTop) {
            at = p.top() - place.offset;
            dir = -1.0;
        } else {
            at = p.bottom() + place.offset;
        }
        const double line = c.snap(at, pen.widthF());
        if (!s.isValid())
            continue;
        const auto pixel = [&](double v) {
            return h ? s.toPixel(v, p.left(), p.right()) : s.toPixel(v, p.bottom(), p.top());
        };
        if (axis->isLineVisible()) {
            if (h)
                c.line(QPointF(p.left(), line), QPointF(p.right(), line), pen);
            else
                c.line(QPointF(line, p.top()), QPointF(line, p.bottom()), pen);
            for (const double v : t.grid) {
                const double x = pixel(v);
                if (x < (h ? p.left() : p.top()) - 0.5 || x > (h ? p.right() : p.bottom()) + 0.5)
                    continue;
                const double sx = c.snap(x, pen.widthF());
                if (h)
                    c.line(QPointF(sx, line), QPointF(sx, line + dir * kTick), pen);
                else
                    c.line(QPointF(line, sx), QPointF(line + dir * kTick, sx), pen);
            }
        }
        const QFontMetrics fm(axis->labelsFont());
        const double labelHeight = fm.height();
        if (axis->labelsVisible()) {
            for (qsizetype i = 0; i < t.labels.size(); ++i) {
                const double x = pixel(t.labelValues[static_cast<std::size_t>(i)]);
                if (x < (h ? p.left() : p.top()) - 0.5 || x > (h ? p.right() : p.bottom()) + 0.5)
                    continue;
                const QString &text = t.labels.at(i);
                const double w = fm.horizontalAdvance(text) + 2.0;
                TextItem item{QRectF(), 0, text, axis->labelsFont(), axis->labelsColor()};
                const double start = at + dir * (kTick + kGap);
                if (h) {
                    item.rect = QRectF(x - w / 2.0, dir > 0 ? start : start - labelHeight, w, labelHeight);
                    item.flags = Qt::AlignCenter;
                } else {
                    item.rect = QRectF(dir > 0 ? start : start - w, x - labelHeight / 2.0, w, labelHeight);
                    item.flags = Qt::AlignVCenter | (dir > 0 ? Qt::AlignLeft : Qt::AlignRight);
                }
                texts->push_back(item);
            }
        }
        if (axis->isTitleVisible() && !axis->titleText().isEmpty()) {
            const QFontMetrics tfm(axis->titleFont());
            const double th = tfm.height(), tw = tfm.horizontalAdvance(axis->titleText()) + 2.0;
            TextItem item{QRectF(), Qt::AlignCenter, axis->titleText(), axis->titleFont(), axis->titleBrush().color()};
            if (h) {
                const double y = at + dir * (kTick + kGap + (axis->labelsVisible() ? labelHeight : 0.0) + kGap);
                item.rect = QRectF(p.center().x() - tw / 2.0, dir > 0 ? y : y - th, tw, th);
            } else {
                const double x = dir > 0 ? std::min(at, at + place.extent - tw) : std::max(at - place.extent, at - tw);
                item.rect = QRectF(x, p.top() - th - kGap, tw, th);
                item.flags = Qt::AlignVCenter | (dir > 0 ? Qt::AlignRight : Qt::AlignLeft);
            }
            texts->push_back(item);
        }
    }
}

void drawXY(const Canvas &c, const QXYSeries *series, const QRectF &plot, std::vector<TextItem> *texts) {
    const std::vector<QPointF> pts = mapPoints(series, series->points(), plot);
    const QAbstractSeries::SeriesType type = series->type();
    const double opacity = series->opacity();
    (void)opacity;
    if (type != QAbstractSeries::SeriesTypeScatter)
        c.stroke(polyline(pts, type == QAbstractSeries::SeriesTypeSpline), series->pen(), 1.0, true);
    const double size = series->markerSize();
    if (type == QAbstractSeries::SeriesTypeScatter) {
        const auto shape = static_cast<const QScatterSeries *>(series)->markerShape();
        for (const QPointF &p : pts) {
            if (!plot.adjusted(-size, -size, size, size).contains(p))
                continue;
            const svg::Path marker = markerPath(shape, p, size);
            c.fill(marker, series->brush());
            c.stroke(marker, series->pen());
        }
    } else if (series->pointsVisible()) {
        for (const QPointF &p : pts)
            if (plot.adjusted(-size, -size, size, size).contains(p))
                c.fill(markerPath(QScatterSeries::MarkerShapeCircle, p, size), series->color());
    }
    if (series->pointLabelsVisible()) {
        const QFontMetrics fm(series->pointLabelsFont());
        const QList<QPointF> values = series->points();
        std::size_t k = 0;
        for (const QPointF &v : values) {
            if (k >= pts.size())
                break;
            const QPointF p = pts[k++];
            if (series->pointLabelsClipping() && !plot.contains(p))
                continue;
            const QString text = pointLabel(series->pointLabelsFormat(), v);
            const double w = fm.horizontalAdvance(text) + 2.0;
            const double offset =
                (series->pointsVisible() || type == QAbstractSeries::SeriesTypeScatter) ? size / 2.0 : 2.0;
            texts->push_back({QRectF(p.x() - w / 2.0, p.y() - offset - 2.0 - fm.height(), w, fm.height()),
                              Qt::AlignCenter, text, series->pointLabelsFont(), series->pointLabelsColor()});
        }
    }
}

void drawArea(const Canvas &c, const QAreaSeries *series, const QRectF &plot, std::vector<TextItem> *texts) {
    if (!series->upperSeries())
        return;
    const std::vector<QPointF> upper = mapPoints(series, series->upperSeries()->points(), plot);
    if (upper.empty())
        return;
    std::vector<QPointF> lower;
    if (series->lowerSeries()) {
        lower = mapPoints(series, series->lowerSeries()->points(), plot);
    } else {
        const Scale sy = ChartEngine::seriesScale(series, Qt::Vertical);
        const double base = sy.log ? sy.min : std::clamp(0.0, sy.min, sy.max);
        const double y = sy.toPixel(base, plot.bottom(), plot.top());
        lower = {QPointF(upper.front().x(), y), QPointF(upper.back().x(), y)};
    }
    std::vector<QPointF> polygon = upper;
    polygon.insert(polygon.end(), lower.rbegin(), lower.rend());
    svg::Path area;
    for (std::size_t i = 0; i < polygon.size(); ++i) {
        if (i == 0)
            area.moveTo({polygon[i].x(), polygon[i].y()});
        else
            area.lineTo({polygon[i].x(), polygon[i].y()});
    }
    area.close();
    c.fill(area, series->brush());
    c.stroke(polyline(upper, false), series->pen(), 1.0, true);
    if (series->lowerSeries())
        c.stroke(polyline(lower, false), series->pen(), 1.0, true);
    if (series->pointsVisible())
        for (const QPointF &p : upper)
            c.fill(markerPath(QScatterSeries::MarkerShapeCircle, p, 6.0), series->borderColor());
    if (series->pointLabelsVisible()) {
        const QFont font;
        const QFontMetrics fm(font);
        const QList<QPointF> values = series->upperSeries()->points();
        for (std::size_t k = 0; k < upper.size() && k < static_cast<std::size_t>(values.size()); ++k) {
            const QString text = pointLabel(series->pointLabelsFormat(), values.at(static_cast<qsizetype>(k)));
            const double w = fm.horizontalAdvance(text) + 2.0;
            texts->push_back({QRectF(upper[k].x() - w / 2.0, upper[k].y() - 4.0 - fm.height(), w, fm.height()),
                              Qt::AlignCenter, text, font, ChartEngine::themeOf(series->chart()).labels});
        }
    }
}

void drawBars(const Canvas &c, const QAbstractBarSeries *series, const QRectF &plot, std::vector<TextItem> *texts) {
    const std::vector<BarRect> rects = ChartEngine::bars(series, plot);
    const bool horizontal = isHorizontalBar(series->type());
    for (const BarRect &bar : rects) {
        const QPen pen = bar.set->pen();
        const bool border = bar.rect.width() >= 3.0 && bar.rect.height() >= 3.0;
        c.rect(bar.rect, bar.set->brush(), border ? pen : QPen(Qt::NoPen));
    }
    if (!series->isLabelsVisible())
        return;
    for (const BarRect &bar : rects) {
        const QFont font = bar.set->labelFont();
        const QFontMetrics fm(font);
        QString text = series->labelsFormat();
        text.replace(QStringLiteral("@value"), QString::number(bar.value, 'g', series->labelsPrecision()));
        const double w = fm.horizontalAdvance(text) + 2.0, hgt = fm.height();
        const QRectF &r = bar.rect;
        const bool negative = bar.value < 0.0;
        QPointF center = r.center();
        // The end of the bar is where its value is: the top of an upright positive bar.
        const double endX = negative ? r.left() : r.right(), baseX = negative ? r.right() : r.left();
        const double endY = negative ? r.bottom() : r.top(), baseY = negative ? r.top() : r.bottom();
        const double sign = negative ? -1.0 : 1.0;
        switch (series->labelsPosition()) {
        case QAbstractBarSeries::LabelsInsideEnd:
            center = horizontal ? QPointF(endX - sign * (w / 2.0 + 2.0), center.y())
                                : QPointF(center.x(), endY + sign * (hgt / 2.0 + 2.0));
            break;
        case QAbstractBarSeries::LabelsInsideBase:
            center = horizontal ? QPointF(baseX + sign * (w / 2.0 + 2.0), center.y())
                                : QPointF(center.x(), baseY - sign * (hgt / 2.0 + 2.0));
            break;
        case QAbstractBarSeries::LabelsOutsideEnd:
            center = horizontal ? QPointF(endX + sign * (w / 2.0 + 2.0), center.y())
                                : QPointF(center.x(), endY - sign * (hgt / 2.0 + 2.0));
            break;
        case QAbstractBarSeries::LabelsCenter:
        default:
            break;
        }
        texts->push_back({QRectF(center.x() - w / 2.0, center.y() - hgt / 2.0, w, hgt), Qt::AlignCenter, text, font,
                          bar.set->labelBrush().color()});
    }
}

void drawPie(const Canvas &c, const QPieSeries *series, const QRectF &plot, std::vector<TextItem> *texts) {
    const std::vector<SliceGeometry> geometry = ChartEngine::slices(series, plot);
    for (const SliceGeometry &g : geometry) {
        const svg::Path path = slicePath(g);
        c.fill(path, g.slice->brush());
        c.stroke(path, g.slice->pen());
    }
    for (const SliceGeometry &g : geometry) {
        QPieSlice *slice = g.slice;
        if (!slice->isLabelVisible() || slice->label().isEmpty())
            continue;
        const QFont font = slice->labelFont();
        const QFontMetrics fm(font);
        const double w = fm.horizontalAdvance(slice->label()) + 2.0, hgt = fm.height();
        const double mid = g.start + g.span / 2.0;
        const QColor color = slice->labelBrush().color();
        if (slice->labelPosition() == QPieSlice::LabelOutside) {
            const QPointF a = onCircle(g.center, g.outer, mid);
            const QPointF b = onCircle(g.center, g.outer * (1.0 + slice->labelArmLengthFactor()), mid);
            const bool right = std::sin(mid * kPi / 180.0) >= 0.0;
            const QPointF e(b.x() + (right ? 10.0 : -10.0), b.y());
            svg::Path arm;
            arm.moveTo({a.x(), a.y()});
            arm.lineTo({b.x(), b.y()});
            arm.lineTo({e.x(), e.y()});
            c.stroke(arm, QPen(QBrush(color), 1.0));
            const QRectF r(right ? e.x() + 3.0 : e.x() - 3.0 - w, e.y() - hgt / 2.0, w, hgt);
            texts->push_back(
                {r, Qt::AlignVCenter | (right ? Qt::AlignLeft : Qt::AlignRight), slice->label(), font, color});
        } else {
            const QPointF p = onCircle(g.center, (g.outer + g.inner) / 2.0, mid);
            texts->push_back(
                {QRectF(p.x() - w / 2.0, p.y() - hgt / 2.0, w, hgt), Qt::AlignCenter, slice->label(), font, color});
        }
    }
}

void drawLegend(const Canvas &c, const QChart *chart, const Layout &L, std::vector<TextItem> *texts) {
    const QLegend *legend = chart->legend();
    if (L.legend.empty())
        return;
    if (legend->isBackgroundVisible())
        c.rect(L.legendRect, legend->brush(), legend->pen(), 4.0);
    const QColor labelColor = legend->labelColor();
    for (const LegendEntry &e : L.legend) {
        const QLegend::MarkerShape shape = legend->markerShape();
        const QAbstractSeries::SeriesType type = e.series->type();
        if (shape == QLegend::MarkerShapeCircle) {
            c.fill(markerPath(QScatterSeries::MarkerShapeCircle, e.marker.center(), e.marker.width()), e.color);
        } else if (shape == QLegend::MarkerShapeFromSeries &&
                   (type == QAbstractSeries::SeriesTypeLine || type == QAbstractSeries::SeriesTypeSpline)) {
            const double y = e.marker.center().y();
            c.line(QPointF(e.marker.left(), y), QPointF(e.marker.right(), y), QPen(QBrush(e.color), 2.0));
        } else if (shape == QLegend::MarkerShapeFromSeries && type == QAbstractSeries::SeriesTypeScatter) {
            c.fill(markerPath(static_cast<const QScatterSeries *>(e.series)->markerShape(), e.marker.center(),
                              e.marker.width()),
                   e.color);
        } else {
            c.rect(e.marker, QBrush(e.color), QPen(Qt::NoPen), 2.0);
        }
        texts->push_back({e.textRect, Qt::AlignLeft | Qt::AlignVCenter, e.text, legend->font(), labelColor});
    }
}

} // namespace

void ChartEngine::paint(QChart *chart, QPainter *painter) {
    if (!painter || !painter->isActive())
        return;
    const QRectF geometry = chart->m_geometry;
    if (geometry.isEmpty())
        return;
    const Layout L = layout(chart);
    const QRect target = geometry.toAlignedRect();
    qreal ratio = painter->device() ? painter->device()->devicePixelRatio() : 1.0;
    if (!(ratio > 0.0))
        ratio = 1.0;
    const double pw = std::ceil(target.width() * ratio), ph = std::ceil(target.height() * ratio);
    if (pw < 1.0 || ph < 1.0 || pw > 32768.0 || ph > 32768.0 || pw * ph > 64e6)
        return;
    svg::Surface surface(static_cast<int>(pw), static_cast<int>(ph));
    Canvas c;
    c.surface = &surface;
    c.ratio = ratio;
    c.m = svg::Mat::scaling(ratio, ratio) * svg::Mat::translate(geometry.x() - target.x(), geometry.y() - target.y());
    const QRectF whole(QPointF(0, 0), geometry.size());
    if (chart->isBackgroundVisible())
        c.rect(whole, chart->backgroundBrush(), chart->backgroundPen(), chart->backgroundRoundness() / 2.0);
    if (chart->isPlotAreaBackgroundVisible())
        c.rect(L.plot, chart->plotAreaBackgroundBrush(), chart->plotAreaBackgroundPen());
    drawAxisBackground(c, L);

    std::vector<TextItem> texts;
    // Each series is drawn on a layer of its own, cut to the plot area and blended with its opacity.
    svg::Surface layer(surface.width, surface.height);
    Canvas lc = c;
    lc.surface = &layer;
    const QRectF clip = L.plot.adjusted(-0.5, -0.5, 0.5, 0.5);
    const int cx0 =
        std::clamp(static_cast<int>(std::floor((clip.left() + geometry.x() - target.x()) * ratio)), 0, surface.width);
    const int cx1 =
        std::clamp(static_cast<int>(std::ceil((clip.right() + geometry.x() - target.x()) * ratio)), 0, surface.width);
    const int cy0 =
        std::clamp(static_cast<int>(std::floor((clip.top() + geometry.y() - target.y()) * ratio)), 0, surface.height);
    const int cy1 =
        std::clamp(static_cast<int>(std::ceil((clip.bottom() + geometry.y() - target.y()) * ratio)), 0, surface.height);
    for (QAbstractSeries *series : std::as_const(chart->m_series)) {
        if (!series->isVisible())
            continue;
        std::fill(layer.pixels.begin(), layer.pixels.end(), 0u);
        const QAbstractSeries::SeriesType type = series->type();
        if (isXYType(type))
            drawXY(lc, static_cast<QXYSeries *>(series), L.plot, &texts);
        else if (type == QAbstractSeries::SeriesTypeArea)
            drawArea(lc, static_cast<QAreaSeries *>(series), L.plot, &texts);
        else if (isBarType(type))
            drawBars(lc, static_cast<QAbstractBarSeries *>(series), L.plot, &texts);
        else if (type == QAbstractSeries::SeriesTypePie)
            drawPie(lc, static_cast<QPieSeries *>(series), L.plot, &texts);
        if (type != QAbstractSeries::SeriesTypePie) {
            for (int y = 0; y < layer.height; ++y) {
                std::uint32_t *row =
                    layer.pixels.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(layer.width);
                if (y < cy0 || y >= cy1) {
                    std::fill(row, row + layer.width, 0u);
                    continue;
                }
                std::fill(row, row + cx0, 0u);
                std::fill(row + cx1, row + layer.width, 0u);
            }
        }
        surface.composite(layer, series->opacity(), nullptr);
    }
    drawAxes(c, L, &texts);
    drawLegend(c, chart, L, &texts);

    const QImage image(reinterpret_cast<const uchar *>(surface.pixels.data()), surface.width, surface.height,
                       QImage::Format_ARGB32_Premultiplied);
    painter->drawImage(target, image);

    painter->save();
    const QPointF origin = geometry.topLeft();
    if (!chart->m_title.isEmpty()) {
        painter->setFont(chart->titleFont());
        painter->setPen(chart->titleBrush().color());
        painter->drawText(L.titleRect.translated(origin).toAlignedRect(), Qt::AlignCenter, chart->m_title);
    }
    for (const TextItem &t : texts) {
        painter->setFont(t.font);
        painter->setPen(t.color);
        painter->drawText(t.rect.translated(origin).toAlignedRect(), t.flags, t.text);
    }
    painter->restore();
}

// ----------------------------------------------------------------------------------------------------------------
// Zooming

void ChartEngine::zoomTo(QChart *chart, const QRectF &rect) {
    const QRectF plot = layout(chart).plot;
    if (!rect.isValid() || plot.isEmpty())
        return;
    for (QAbstractAxis *axis : std::as_const(chart->m_axes)) {
        double lo = 0.0, hi = 0.0;
        const Scale s = axisScale(axis);
        if (!axisRange(axis, &lo, &hi) || !s.isValid())
            continue;
        if (!chart->m_zoomOrigin.contains(axis))
            chart->m_zoomOrigin.insert(axis, qMakePair(lo, hi));
        double a = 0.0, b = 0.0;
        if (axis->m_orientation == Qt::Horizontal) {
            a = s.toValue(rect.left(), plot.left(), plot.right());
            b = s.toValue(rect.right(), plot.left(), plot.right());
        } else {
            a = s.toValue(rect.bottom(), plot.bottom(), plot.top());
            b = s.toValue(rect.top(), plot.bottom(), plot.top());
        }
        double n0 = std::min(a, b), n1 = std::max(a, b);
        if (axis->type() == QAbstractAxis::AxisTypeBarCategory) {
            n0 = std::round(n0 + 0.5) - 0.5;
            n1 = std::round(n1 - 0.5) + 0.5;
            if (n1 <= n0)
                n1 = n0 + 1.0;
        }
        if (std::isfinite(n0) && std::isfinite(n1) && n1 > n0)
            setAxisRange(axis, n0, n1, true);
    }
}

// ----------------------------------------------------------------------------------------------------------------
// The mouse

namespace {

struct Hit {
    QAbstractSeries *series = nullptr;
    QObject *item = nullptr;
    int index = -1;
    QPointF point;
};

enum class Event { Press, Release, Click, DoubleClick, Hover };

bool sameTarget(const Hit &h, const QAbstractSeries *series, const QObject *item, int index) {
    return h.series == series && h.item == item && h.index == index;
}

void emitEvent(const Hit &h, Event e, bool state) {
    if (!h.series)
        return;
    const QAbstractSeries::SeriesType type = h.series->type();
    if (isXYType(type)) {
        auto *s = static_cast<QXYSeries *>(h.series);
        switch (e) {
        case Event::Press:
            emit s->pressed(h.point);
            break;
        case Event::Release:
            emit s->released(h.point);
            break;
        case Event::Click:
            emit s->clicked(h.point);
            break;
        case Event::DoubleClick:
            emit s->doubleClicked(h.point);
            break;
        case Event::Hover:
            emit s->hovered(h.point, state);
            break;
        }
    } else if (type == QAbstractSeries::SeriesTypeArea) {
        auto *s = static_cast<QAreaSeries *>(h.series);
        switch (e) {
        case Event::Press:
            emit s->pressed(h.point);
            break;
        case Event::Release:
            emit s->released(h.point);
            break;
        case Event::Click:
            emit s->clicked(h.point);
            break;
        case Event::DoubleClick:
            emit s->doubleClicked(h.point);
            break;
        case Event::Hover:
            emit s->hovered(h.point, state);
            break;
        }
    } else if (isBarType(type)) {
        auto *s = static_cast<QAbstractBarSeries *>(h.series);
        auto *set = static_cast<QBarSet *>(h.item);
        switch (e) {
        case Event::Press:
            emit set->pressed(h.index);
            emit s->pressed(h.index, set);
            break;
        case Event::Release:
            emit set->released(h.index);
            emit s->released(h.index, set);
            break;
        case Event::Click:
            emit set->clicked(h.index);
            emit s->clicked(h.index, set);
            break;
        case Event::DoubleClick:
            emit set->doubleClicked(h.index);
            emit s->doubleClicked(h.index, set);
            break;
        case Event::Hover:
            emit set->hovered(state, h.index);
            emit s->hovered(state, h.index, set);
            break;
        }
    } else if (type == QAbstractSeries::SeriesTypePie) {
        auto *s = static_cast<QPieSeries *>(h.series);
        auto *slice = static_cast<QPieSlice *>(h.item);
        switch (e) {
        case Event::Press:
            emit slice->pressed();
            emit s->pressed(slice);
            break;
        case Event::Release:
            emit slice->released();
            emit s->released(slice);
            break;
        case Event::Click:
            emit slice->clicked();
            emit s->clicked(slice);
            break;
        case Event::DoubleClick:
            emit slice->doubleClicked();
            emit s->doubleClicked(slice);
            break;
        case Event::Hover:
            emit slice->hovered(state);
            emit s->hovered(slice, state);
            break;
        }
    }
}

} // namespace

static Hit hitTest(QChart *chart, const QPointF &pos) {
    const QRectF plot = ChartEngine::layout(chart).plot;
    const QList<QAbstractSeries *> all = chart->series();
    for (auto it = all.crbegin(); it != all.crend(); ++it) {
        QAbstractSeries *series = *it;
        if (!series->isVisible())
            continue;
        const QAbstractSeries::SeriesType type = series->type();
        if (type == QAbstractSeries::SeriesTypePie) {
            for (const SliceGeometry &g : ChartEngine::slices(static_cast<QPieSeries *>(series), plot)) {
                const double dx = pos.x() - g.center.x(), dy = pos.y() - g.center.y();
                const double r = std::hypot(dx, dy);
                if (r < g.inner || r > g.outer)
                    continue;
                double angle = std::atan2(dx, -dy) * 180.0 / kPi;
                double from = std::fmod(g.start, 360.0);
                if (from < 0.0)
                    from += 360.0;
                angle = std::fmod(angle - from + 720.0, 360.0);
                if (angle <= std::fabs(g.span))
                    return {series, g.slice, -1, pos};
            }
            continue;
        }
        if (isBarType(type)) {
            const std::vector<BarRect> rects = ChartEngine::bars(static_cast<QAbstractBarSeries *>(series), plot);
            for (auto bar = rects.crbegin(); bar != rects.crend(); ++bar)
                if (bar->rect.intersected(plot).contains(pos))
                    return {series, bar->set, bar->index, pos};
            continue;
        }
        if (!plot.contains(pos))
            continue;
        if (isXYType(type)) {
            auto *xy = static_cast<QXYSeries *>(series);
            const std::vector<QPointF> pts = mapPoints(series, xy->points(), plot);
            const bool markers = type == QAbstractSeries::SeriesTypeScatter || xy->pointsVisible();
            if (markers) {
                double best = xy->markerSize() / 2.0 + 2.0;
                int found = -1;
                for (std::size_t i = 0; i < pts.size(); ++i) {
                    const double d = std::hypot(pts[i].x() - pos.x(), pts[i].y() - pos.y());
                    if (d <= best) {
                        best = d;
                        found = static_cast<int>(i);
                    }
                }
                if (found >= 0 && found < xy->count())
                    return {series, series, -1, xy->at(found)};
            }
            if (type != QAbstractSeries::SeriesTypeScatter) {
                const double tolerance = std::max(3.0, xy->pen().widthF() / 2.0) + 2.0;
                for (std::size_t i = 1; i < pts.size(); ++i)
                    if (segmentDistance(pos, pts[i - 1], pts[i]) <= tolerance)
                        return {series, series, -1, ChartEngine::toValue(series, pos, plot)};
            }
        } else if (type == QAbstractSeries::SeriesTypeArea) {
            auto *area = static_cast<QAreaSeries *>(series);
            if (!area->upperSeries())
                continue;
            std::vector<QPointF> polygon = mapPoints(series, area->upperSeries()->points(), plot);
            if (polygon.empty())
                continue;
            if (area->lowerSeries()) {
                const std::vector<QPointF> lower = mapPoints(series, area->lowerSeries()->points(), plot);
                polygon.insert(polygon.end(), lower.rbegin(), lower.rend());
            } else {
                const Scale sy = ChartEngine::seriesScale(series, Qt::Vertical);
                const double y =
                    sy.toPixel(sy.log ? sy.min : std::clamp(0.0, sy.min, sy.max), plot.bottom(), plot.top());
                polygon.emplace_back(polygon.back().x(), y);
                polygon.emplace_back(polygon.front().x(), y);
            }
            if (insidePolygon(polygon, pos))
                return {series, series, -1, ChartEngine::toValue(series, pos, plot)};
        }
    }
    return {};
}

void ChartEngine::mouse(QChart *chart, Mouse what, const QPointF &position) {
    const Hit hit = what == Mouse::Leave ? Hit() : hitTest(chart, position);
    switch (what) {
    case Mouse::Move:
    case Mouse::Leave: {
        const bool same =
            chart->m_hoverSeries && sameTarget(hit, chart->m_hoverSeries, chart->m_hoverItem, chart->m_hoverIndex);
        if (same)
            return;
        if (chart->m_hoverSeries) {
            const Hit old{chart->m_hoverSeries, chart->m_hoverItem, chart->m_hoverIndex, chart->m_hoverPoint};
            chart->m_hoverSeries = nullptr;
            chart->m_hoverItem = nullptr;
            emitEvent(old, Event::Hover, false);
        }
        if (hit.series) {
            chart->m_hoverSeries = hit.series;
            chart->m_hoverItem = hit.item;
            chart->m_hoverIndex = hit.index;
            chart->m_hoverPoint = hit.point;
            emitEvent(hit, Event::Hover, true);
        }
        break;
    }
    case Mouse::Press:
        chart->m_pressSeries = hit.series;
        chart->m_pressItem = hit.item;
        chart->m_pressIndex = hit.index;
        emitEvent(hit, Event::Press, true);
        break;
    case Mouse::Release: {
        const bool click = hit.series && chart->m_pressSeries &&
                           sameTarget(hit, chart->m_pressSeries, chart->m_pressItem, chart->m_pressIndex);
        chart->m_pressSeries = nullptr;
        chart->m_pressItem = nullptr;
        emitEvent(hit, Event::Release, true);
        if (click)
            emitEvent(hit, Event::Click, true);
        break;
    }
    case Mouse::DoubleClick:
        emitEvent(hit, Event::DoubleClick, true);
        break;
    }
}

} // namespace QtChartsPrivate
