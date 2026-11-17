// MIT License - see LICENSE
#include "qpdfengine_p.h"

#include "qfontengine_p.h"

#include <QtCore/qdatetime.h>
#include <QtCore/qfile.h>
#include <QtCore/qiodevice.h>
#include <QtGui/qimage.h>
#include <QtGui/qpainterpath.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

#ifdef NT_HAVE_ZLIB
#include <zlib.h>
#endif

namespace QtPrivate {

namespace {

// A number with up to `decimals` decimals and no exponent, whatever the locale.
std::string num(double v, int decimals = 3) {
    if (!std::isfinite(v))
        v = 0.0;
    char buffer[64];
    std::snprintf(buffer, sizeof buffer, "%.*f", decimals, v);
    std::string s(buffer);
    if (s.find('.') != std::string::npos) {
        while (s.back() == '0')
            s.pop_back();
        if (s.back() == '.')
            s.pop_back();
    }
    return s == "-0" ? "0" : s;
}

std::string matrix(const QTransform &t) {
    return num(t.m11(), 6) + " " + num(t.m12(), 6) + " " + num(t.m21(), 6) + " " + num(t.m22(), 6) + " " +
           num(t.dx(), 4) + " " + num(t.dy(), 4);
}

std::string rgb(const QColor &c) { return num(c.redF(), 4) + " " + num(c.greenF(), 4) + " " + num(c.blueF(), 4); }

// A PDF text string: plain ASCII in parentheses, anything else as UTF-16BE in hex.
std::string textString(const QString &text) {
    bool ascii = true;
    for (QChar c : text)
        ascii = ascii && c.unicode() >= 0x20 && c.unicode() < 0x7f;
    if (ascii) {
        std::string out = "(";
        for (QChar c : text) {
            const char ch = static_cast<char>(c.unicode());
            if (ch == '(' || ch == ')' || ch == '\\')
                out += '\\';
            out += ch;
        }
        return out + ")";
    }
    static const char hex[] = "0123456789ABCDEF";
    std::string out = "<FEFF";
    for (QChar c : text) {
        const unsigned u = c.unicode();
        for (int shift = 12; shift >= 0; shift -= 4)
            out += hex[(u >> static_cast<unsigned>(shift)) & 0xfu];
    }
    return out + ">";
}

std::string rectOperators(double x, double y, double w, double h) {
    return num(x) + " " + num(y) + " " + num(w) + " " + num(h) + " re\n";
}

// The four Bezier curves of an ellipse in `x`, `y`, `w`, `h`.
std::string ellipseOperators(double x, double y, double w, double h) {
    const double k = 0.5522847498;
    const double rx = w / 2, ry = h / 2, cx = x + rx, cy = y + ry;
    const auto p = [](double a, double b) { return num(a) + " " + num(b) + " "; };
    std::string out = p(cx + rx, cy) + "m\n";
    out += p(cx + rx, cy + k * ry) + p(cx + k * rx, cy + ry) + p(cx, cy + ry) + "c\n";
    out += p(cx - k * rx, cy + ry) + p(cx - rx, cy + k * ry) + p(cx - rx, cy) + "c\n";
    out += p(cx - rx, cy - k * ry) + p(cx - k * rx, cy - ry) + p(cx, cy - ry) + "c\n";
    out += p(cx + k * rx, cy - ry) + p(cx + rx, cy - k * ry) + p(cx + rx, cy) + "c\nh\n";
    return out;
}

double alphaOf(const QColor &color, qreal opacity) { return std::clamp(color.alphaF() * opacity, 0.0, 1.0); }

} // namespace

PdfEngine::PdfEngine(const PdfSettings *settings, const QPagedPaintDevice *pages)
    : m_settings(settings), m_pages(pages) {}

PdfEngine::~PdfEngine() = default;

int PdfEngine::addObject(std::string body) {
    m_objects.push_back(std::move(body));
    return static_cast<int>(m_objects.size());
}

int PdfEngine::reserveObject() { return addObject(std::string()); }

void PdfEngine::setObject(int id, std::string body) { m_objects[static_cast<std::size_t>(id - 1)] = std::move(body); }

std::string PdfEngine::streamObject(const std::string &dictionary, const std::string &data, bool compress) const {
    (void)compress;
    std::string payload = data;
    std::string filter;
#ifdef NT_HAVE_ZLIB
    if (compress && data.size() > 64) {
        uLongf size = compressBound(static_cast<uLong>(data.size()));
        std::string packed(size, '\0');
        if (compress2(
                reinterpret_cast<Bytef *>(packed.data()), // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                &size,
                reinterpret_cast<const Bytef *>(data.data()), // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                static_cast<uLong>(data.size()), 6) == Z_OK) {
            packed.resize(size);
            payload = std::move(packed);
            filter = " /Filter /FlateDecode";
        }
    }
#endif
    return "<< " + dictionary + filter + " /Length " + std::to_string(payload.size()) + " >>\nstream\n" + payload +
           "\nendstream";
}

bool PdfEngine::begin(QPaintDevice *device) {
    if (isActive())
        return false;
    m_objects.clear();
    m_pageIds.clear();
    m_alphaStates.clear();
    m_images.clear();
    m_glyphs.clear();
    m_imageIds.clear();
    m_glyphIds.clear();
    m_patternIds.clear();
    addObject(std::string()); // 1: the catalog
    m_pagesId = reserveObject();
    m_resourcesId = reserveObject();
    setPaintDevice(device);
    setActive(true);
    m_state = QPaintEngineState();
    startPage();
    return true;
}

void PdfEngine::startPage() {
    m_layout = m_pages->pageLayout();
    const int res = std::max(1, m_settings->resolution);
    const double s = 72.0 / res;
    const QRectF full = m_layout.fullRect(QPageLayout::Point);
    const QRect paint = m_layout.paintRectPixels(res);
    m_base = QTransform(s, 0, 0, -s, paint.x() * s, full.height() - paint.y() * s);
    m_content = "q\n" + matrix(m_base) + " cm\n";
}

void PdfEngine::finishPage() {
    m_content += "Q\n";
    const int contents = addObject(streamObject(std::string(), m_content));
    m_content.clear();
    const QRectF full = m_layout.fullRect(QPageLayout::Point);
    const int page =
        addObject("<< /Type /Page /Parent " + std::to_string(m_pagesId) + " 0 R /MediaBox [0 0 " + num(full.width()) +
                  " " + num(full.height()) + "] /Resources " + std::to_string(m_resourcesId) + " 0 R /Contents " +
                  std::to_string(contents) + " 0 R >>");
    m_pageIds.push_back(page);
}

bool PdfEngine::newPage() {
    if (!isActive())
        return false;
    finishPage();
    startPage();
    return true;
}

bool PdfEngine::end() {
    if (!isActive())
        return false;
    finishPage();
    setActive(false);
    setPaintDevice(nullptr);

    std::string kids;
    for (int id : m_pageIds)
        kids += std::to_string(id) + " 0 R ";
    setObject(m_pagesId, "<< /Type /Pages /Kids [" + kids + "] /Count " + std::to_string(m_pageIds.size()) + " >>");
    setObject(1, "<< /Type /Catalog /Pages " + std::to_string(m_pagesId) + " 0 R >>");

    std::string resources = "<< /ProcSet [/PDF /ImageB /ImageC]";
    const auto dictionary = [](const char *key, const char *prefix, const std::vector<int> &ids) {
        if (ids.empty())
            return std::string();
        std::string out = std::string(" /") + key + " <<";
        for (int id : ids)
            out += std::string(" /") + prefix + std::to_string(id) + " " + std::to_string(id) + " 0 R";
        return out + " >>";
    };
    std::vector<int> xobjects = m_imageIds;
    xobjects.insert(xobjects.end(), m_glyphIds.begin(), m_glyphIds.end());
    resources += dictionary("XObject", "X", xobjects);
    std::vector<int> states;
    states.reserve(m_alphaStates.size());
    for (const auto &entry : m_alphaStates)
        states.push_back(entry.second);
    resources += dictionary("ExtGState", "GS", states);
    resources += dictionary("Pattern", "P", m_patternIds);
    setObject(m_resourcesId, resources + " >>");

    std::string info = "<< /Producer (Nt)";
    if (!m_settings->title.isEmpty())
        info += " /Title " + textString(m_settings->title);
    if (!m_settings->creator.isEmpty())
        info += " /Creator " + textString(m_settings->creator);
    const QDateTime now = QDateTime::currentDateTimeUtc();
    info += " /CreationDate (D:" + now.toString(QStringLiteral("yyyyMMddHHmmss")).toStdString() + "Z)";
    const int infoId = addObject(info + " >>");

    const char *version = m_settings->version == QPagedPaintDevice::PdfVersion_1_6 ? "1.6" : "1.4";
    std::string out = std::string("%PDF-") + version + "\n%\xe2\xe3\xcf\xd3\n";
    std::vector<std::size_t> offsets;
    offsets.reserve(m_objects.size());
    for (std::size_t i = 0; i < m_objects.size(); ++i) {
        offsets.push_back(out.size());
        out += std::to_string(i + 1) + " 0 obj\n" + m_objects[i] + "\nendobj\n";
    }
    const std::size_t xref = out.size();
    out += "xref\n0 " + std::to_string(m_objects.size() + 1) + "\n0000000000 65535 f \n";
    for (std::size_t offset : offsets) {
        char line[24];
        std::snprintf(line, sizeof line, "%010zu 00000 n \n", offset);
        out += line;
    }
    out += "trailer\n<< /Size " + std::to_string(m_objects.size() + 1) + " /Root 1 0 R /Info " +
           std::to_string(infoId) + " 0 R >>\nstartxref\n" + std::to_string(xref) + "\n%%EOF\n";
    m_objects.clear();

    QIODevice *target = m_settings->device;
    std::unique_ptr<QFile> file;
    if (!target) {
        file = std::make_unique<QFile>(m_settings->fileName);
        target = file.get();
    }
    bool opened = false;
    if (!target->isOpen()) {
        if (!target->open(QIODevice::WriteOnly))
            return false;
        opened = true;
    }
    const bool ok = target->write(out.data(), static_cast<qint64>(out.size())) == static_cast<qint64>(out.size());
    if (opened)
        target->close();
    return ok;
}

void PdfEngine::updateState(const QPaintEngineState &state) { m_state = state; }

std::string PdfEngine::alphaState(double fillAlpha, double strokeAlpha) {
    if (fillAlpha >= 1.0 && strokeAlpha >= 1.0)
        return std::string();
    const std::string key = num(fillAlpha) + " " + num(strokeAlpha);
    auto it = m_alphaStates.find(key);
    if (it == m_alphaStates.end())
        it = m_alphaStates
                 .emplace(key,
                          addObject("<< /Type /ExtGState /ca " + num(fillAlpha) + " /CA " + num(strokeAlpha) + " >>"))
                 .first;
    return "/GS" + std::to_string(it->second) + " gs\n";
}

bool PdfEngine::open(double fillAlpha, double strokeAlpha) {
    std::string clip;
    if (m_state.hasClipping()) {
        const QRegion region = m_state.clipRegion();
        if (region.isEmpty())
            return false;
        for (const QRect &r : region.rects())
            clip += rectOperators(r.x(), r.y(), r.width(), r.height());
        clip += "W n\n";
    }
    m_content += "q\n" + clip + alphaState(fillAlpha, strokeAlpha);
    return true;
}

void PdfEngine::writeStroke(const QPen &pen, double width) {
    std::string out = num(width) + " w\n";
    out += pen.capStyle() == Qt::RoundCap ? "1 J\n" : pen.capStyle() == Qt::SquareCap ? "2 J\n" : "0 J\n";
    out += pen.joinStyle() == Qt::RoundJoin ? "1 j\n" : pen.joinStyle() == Qt::BevelJoin ? "2 j\n" : "0 j\n";
    out += num(std::max<qreal>(1.0, pen.miterLimit())) + " M\n";
    if (pen.style() != Qt::SolidLine && pen.style() != Qt::NoPen) {
        const QList<qreal> dashes = pen.dashPattern();
        const double unit = std::max(1.0, width);
        std::string array;
        for (qreal d : dashes)
            array += num(d * unit) + " ";
        out += "[" + array + "] " + num(pen.dashOffset() * unit) + " d\n";
    }
    m_content += out + rgb(pen.color()) + " RG\n";
}

std::string PdfEngine::pathOperators(const QPainterPath &path) {
    std::string d;
    QPointF start;
    for (int i = 0; i < path.elementCount(); ++i) {
        const QPainterPath::Element e = path.elementAt(i);
        if (e.type == QPainterPath::MoveToElement) {
            start = QPointF(e.x, e.y);
            d += num(e.x) + " " + num(e.y) + " m\n";
        } else if (e.type == QPainterPath::LineToElement) {
            const bool closes =
                QPointF(e.x, e.y) == start &&
                (i + 1 == path.elementCount() || path.elementAt(i + 1).type == QPainterPath::MoveToElement);
            d += closes ? std::string("h\n") : num(e.x) + " " + num(e.y) + " l\n";
        } else if (e.type == QPainterPath::CurveToElement && i + 2 < path.elementCount()) {
            const QPainterPath::Element c2 = path.elementAt(i + 1), end = path.elementAt(i + 2);
            d += num(e.x) + " " + num(e.y) + " " + num(c2.x) + " " + num(c2.y) + " " + num(end.x) + " " + num(end.y) +
                 " c\n";
            i += 2;
        }
    }
    return d;
}

std::string PdfEngine::gradientPattern(const QBrush &brush, const QTransform &userToDevice) {
    const QGradient *g = brush.gradient();
    QGradientStops stops = g->stops();
    if (stops.isEmpty())
        return std::string();
    if (stops.first().first > 0)
        stops.prepend(QGradientStop(0.0, stops.first().second));
    if (stops.last().first < 1)
        stops.append(QGradientStop(1.0, stops.last().second));
    const auto color = [](const QColor &c) { return "[" + rgb(c) + "]"; };
    std::string function;
    if (stops.size() == 2) {
        function = "<< /FunctionType 2 /Domain [0 1] /C0 " + color(stops[0].second) + " /C1 " + color(stops[1].second) +
                   " /N 1 >>";
    } else {
        std::string functions, bounds, encode;
        for (qsizetype i = 0; i + 1 < stops.size(); ++i) {
            functions += "<< /FunctionType 2 /Domain [0 1] /C0 " + color(stops[i].second) + " /C1 " +
                         color(stops[i + 1].second) + " /N 1 >> ";
            if (i > 0)
                bounds += num(stops[i].first, 5) + " ";
            encode += "0 1 ";
        }
        function = "<< /FunctionType 3 /Domain [0 1] /Functions [" + functions + "] /Bounds [" + bounds +
                   "] /Encode [" + encode + "] >>";
    }
    std::string shading;
    if (g->type() == QGradient::LinearGradient) {
        const auto *l = static_cast<const QLinearGradient *>(g);
        shading = "<< /ShadingType 2 /ColorSpace /DeviceRGB /Coords [" + num(l->start().x()) + " " +
                  num(l->start().y()) + " " + num(l->finalStop().x()) + " " + num(l->finalStop().y()) + "] /Function " +
                  function + " /Extend [true true] >>";
    } else {
        const auto *r = static_cast<const QRadialGradient *>(g);
        shading = "<< /ShadingType 3 /ColorSpace /DeviceRGB /Coords [" + num(r->focalPoint().x()) + " " +
                  num(r->focalPoint().y()) + " " + num(r->focalRadius()) + " " + num(r->center().x()) + " " +
                  num(r->center().y()) + " " + num(r->radius()) + "] /Function " + function + " /Extend [true true] >>";
    }
    // Pattern space is the default space of the page, whatever the current transformation is.
    const QTransform toPage = brush.transform() * userToDevice * m_base;
    const int id = addObject("<< /PatternType 2 /Shading " + shading + " /Matrix [" + matrix(toPage) + "] >>");
    m_patternIds.push_back(id);
    return "/Pattern cs /P" + std::to_string(id) + " scn\n";
}

void PdfEngine::drawPath(const QPainterPath &path) {
    const QPen pen = m_state.pen();
    const QBrush brush = m_state.brush();
    const bool stroked = pen.style() != Qt::NoPen;
    const bool filled = brush.style() != Qt::NoBrush;
    if (path.isEmpty() || (!stroked && !filled))
        return;
    const qreal opacity = m_state.opacity();
    const QTransform t = m_state.transform();
    const QGradient *g = filled ? brush.gradient() : nullptr;
    const bool shaded = g && (g->type() == QGradient::LinearGradient || g->type() == QGradient::RadialGradient);
    const QColor fillColor = g ? (g->stops().isEmpty() ? QColor(Qt::black) : g->stops().first().second) : brush.color();
    if (!open(filled ? (shaded ? opacity : alphaOf(fillColor, opacity)) : 1.0,
              stroked ? alphaOf(pen.color(), opacity) : 1.0))
        return;
    // A cosmetic pen keeps its width in device pixels: the path is mapped rather than the space.
    const bool cosmetic = stroked && pen.isCosmetic();
    const std::string ops = pathOperators(cosmetic ? t.map(path) : path);
    if (!cosmetic && !t.isIdentity())
        m_content += matrix(t) + " cm\n";
    const bool evenOdd = path.fillRule() == Qt::OddEvenFill;
    if (filled) {
        m_content += shaded ? gradientPattern(brush, t) : rgb(fillColor) + " rg\n";
    }
    if (stroked)
        writeStroke(pen, pen.widthF() > 0 ? pen.widthF() : 1.0);
    m_content += ops;
    if (filled && stroked)
        m_content += evenOdd ? "B*\n" : "B\n";
    else if (filled)
        m_content += evenOdd ? "f*\n" : "f\n";
    else
        m_content += "S\n";
    close();
}

void PdfEngine::drawRects(const QRect *rects, int rectCount) {
    const QPen pen = m_state.pen();
    const QBrush brush = m_state.brush();
    const bool stroke = pen.style() != Qt::NoPen;
    const bool fill = brush.style() != Qt::NoBrush;
    if ((!stroke && !fill) || !open(alphaOf(brush.color(), m_state.opacity()), alphaOf(pen.color(), m_state.opacity())))
        return;
    // The raster engine's rules: the outline covers `pw` pixels around the rectangle, and the fill is inside it.
    const int pw = std::max(1, pen.width());
    const int half = (pw - 1) / 2;
    std::string fills, strokes;
    for (int i = 0; i < rectCount; ++i) {
        const QRect r = rects[i].normalized();
        if (fill) {
            const QRect area =
                stroke ? QRect(r.x() - half + pw, r.y() - half + pw, r.width() - pw, r.height() - pw) : r;
            if (!area.isEmpty())
                fills += rectOperators(area.x(), area.y(), area.width(), area.height());
        }
        if (stroke) {
            const double c = pw / 2.0 - half;
            strokes += rectOperators(r.x() + c, r.y() + c, r.width(), r.height());
        }
    }
    if (!fills.empty())
        m_content += rgb(brush.color()) + " rg\n" + fills + "f\n";
    if (!strokes.empty()) {
        writeStroke(QPen(pen.color(), pw), pw);
        m_content += strokes + "S\n";
    }
    close();
}

void PdfEngine::drawLines(const QLine *lines, int lineCount) {
    const QPen pen = m_state.pen();
    if (pen.style() == Qt::NoPen || !open(1.0, alphaOf(pen.color(), m_state.opacity())))
        return;
    const int pw = std::max(1, pen.width());
    const int half = (pw - 1) / 2; // whole pixels, as the raster engine
    const double c = pw / 2.0 - half;
    QPen square(pen);
    square.setCapStyle(Qt::SquareCap);
    writeStroke(square, pw);
    for (int i = 0; i < lineCount; ++i)
        m_content += num(lines[i].x1() + c) + " " + num(lines[i].y1() + c) + " m " + num(lines[i].x2() + c) + " " +
                     num(lines[i].y2() + c) + " l\n";
    m_content += "S\n";
    close();
}

void PdfEngine::drawPoints(const QPoint *points, int pointCount) {
    const QPen pen = m_state.pen();
    if (pen.style() == Qt::NoPen || !open(alphaOf(pen.color(), m_state.opacity()), 1.0))
        return;
    const int pw = std::max(1, pen.width());
    const int half = (pw - 1) / 2;
    m_content += rgb(pen.color()) + " rg\n";
    for (int i = 0; i < pointCount; ++i)
        m_content += rectOperators(points[i].x() - half, points[i].y() - half, pw, pw);
    m_content += "f\n";
    close();
}

void PdfEngine::drawEllipse(const QRect &rectIn) {
    const QRect rect = rectIn.normalized();
    const QPen pen = m_state.pen();
    const QBrush brush = m_state.brush();
    const bool stroke = pen.style() != Qt::NoPen;
    const bool filled = brush.style() != Qt::NoBrush;
    if (rect.isEmpty() || (!stroke && !filled))
        return;
    if (!open(alphaOf(brush.color(), m_state.opacity()), alphaOf(pen.color(), m_state.opacity())))
        return;
    const int pw = std::max(1, pen.width());
    double x = rect.x(), y = rect.y(), w = rect.width(), h = rect.height();
    if (stroke && w > 2 * pw && h > 2 * pw) {
        // The outline lies inside the rectangle, as the raster engine draws it.
        x += pw / 2.0;
        y += pw / 2.0;
        w -= pw;
        h -= pw;
        if (filled)
            m_content += rgb(brush.color()) + " rg\n";
        writeStroke(QPen(pen.color(), pw), pw);
        m_content += ellipseOperators(x, y, w, h) + (filled ? "B\n" : "S\n");
    } else {
        m_content += rgb(stroke ? pen.color() : brush.color()) + " rg\n" + ellipseOperators(x, y, w, h) + "f\n";
    }
    close();
}

std::string PdfEngine::imageObject(const QImage &source) {
    const QImage image = source.convertToFormat(QImage::Format_ARGB32);
    const int width = image.width(), height = image.height();
    std::string colors, alpha;
    colors.reserve(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3);
    bool translucent = false;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const QRgb p = image.pixel(x, y);
            colors += static_cast<char>(qRed(p));
            colors += static_cast<char>(qGreen(p));
            colors += static_cast<char>(qBlue(p));
            alpha += static_cast<char>(qAlpha(p));
            translucent = translucent || qAlpha(p) != 255;
        }
    }
    const std::string size = " /Width " + std::to_string(width) + " /Height " + std::to_string(height);
    std::string mask;
    if (translucent) {
        const int maskId = addObject(streamObject(
            "/Type /XObject /Subtype /Image" + size + " /ColorSpace /DeviceGray /BitsPerComponent 8", alpha));
        mask = " /SMask " + std::to_string(maskId) + " 0 R";
    }
    const int id = addObject(streamObject(
        "/Type /XObject /Subtype /Image" + size + " /ColorSpace /DeviceRGB /BitsPerComponent 8" + mask, colors));
    m_imageIds.push_back(id);
    return "/X" + std::to_string(id);
}

void PdfEngine::drawImage(const QRect &rect, const QImage &image, const QRect &sourceRect) {
    if (image.isNull())
        return;
    const QRect sr = sourceRect.isNull() ? image.rect() : sourceRect.normalized().intersected(image.rect());
    const QRect target = rect.isNull() ? QRect(rect.topLeft(), sr.size()) : rect.normalized();
    if (sr.isEmpty() || target.isEmpty() || !open(m_state.opacity(), 1.0))
        return;
    const auto key = std::make_tuple(image.cacheKey(), sr.x(), sr.y(), sr.width(), sr.height());
    std::string name;
    const auto it = m_images.find(key);
    if (it != m_images.end()) {
        name = "/X" + std::to_string(it->second);
    } else {
        name = imageObject(sr == image.rect() ? image : image.copy(sr));
        m_images.emplace(key, m_imageIds.back());
    }
    m_content += num(target.width()) + " 0 0 " + num(-target.height()) + " " + num(target.x()) + " " +
                 num(target.y() + target.height()) + " cm\n" + name + " Do\n";
    close();
}

void PdfEngine::drawTransformedImage(const QRectF &rect, const QImage &image, const QRectF &sourceRect) {
    if (image.isNull() || rect.isEmpty() || !open(m_state.opacity(), 1.0))
        return;
    const QRect sr = sourceRect.toAlignedRect().intersected(image.rect());
    if (sr.isEmpty()) {
        close();
        return;
    }
    const std::string name = imageObject(sr == image.rect() ? image : image.copy(sr));
    m_content += matrix(m_state.transform()) + " cm\n" + num(rect.width()) + " 0 0 " + num(-rect.height()) + " " +
                 num(rect.x()) + " " + num(rect.y() + rect.height()) + " cm\n" + name + " Do\n";
    close();
}

void PdfEngine::drawAlphaMask(const QPoint &topLeft, const uchar *mask, int width, int height, qsizetype stride) {
    if (width <= 0 || height <= 0)
        return;
    const QColor color = m_state.pen().color();
    QImage image(width, height, QImage::Format_ARGB32);
    for (int y = 0; y < height; ++y) {
        const uchar *row = mask + static_cast<qsizetype>(y) * stride;
        for (int x = 0; x < width; ++x)
            image.setPixel(x, y, qRgba(color.red(), color.green(), color.blue(), (row[x] * color.alpha() + 127) / 255));
    }
    drawImage(QRect(topLeft, QSize(width, height)), image, QRect());
}

std::string PdfEngine::glyphObject(QFontEngine &font, char32_t codePoint) {
    const auto key = std::make_pair(static_cast<const QFontEngine *>(&font), codePoint);
    auto it = m_glyphs.find(key);
    if (it == m_glyphs.end()) {
        QPainterPath outline;
        font.addOutline(codePoint, 0.0, 0.0, &outline);
        int id = 0;
        if (!outline.isEmpty()) {
            const QRectF box = outline.boundingRect().adjusted(-1, -1, 1, 1);
            id = addObject(streamObject("/Type /XObject /Subtype /Form /BBox [" + num(box.left()) + " " +
                                            num(box.top()) + " " + num(box.right()) + " " + num(box.bottom()) + "]",
                                        pathOperators(outline) + "f\n"));
            m_glyphIds.push_back(id);
        }
        it = m_glyphs.emplace(key, id).first;
    }
    return it->second ? "/X" + std::to_string(it->second) : std::string();
}

void PdfEngine::drawGlyphs(QFontEngine &font, const std::vector<PdfGlyph> &glyphs) {
    const QColor color = m_state.pen().color();
    if (glyphs.empty() || !open(alphaOf(color, m_state.opacity()), 1.0))
        return;
    m_content += rgb(color) + " rg\n";
    for (const PdfGlyph &g : glyphs) {
        const std::string name = glyphObject(font, g.codePoint);
        if (!name.empty())
            m_content += "q 1 0 0 1 " + num(g.origin.x()) + " " + num(g.origin.y()) + " cm " + name + " Do Q\n";
    }
    close();
}

} // namespace QtPrivate
