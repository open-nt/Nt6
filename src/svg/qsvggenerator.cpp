// MIT License - see LICENSE
#include <QtSvg/qsvggenerator.h>

#include <QtCore/qbytearray.h>
#include <QtCore/qfile.h>
#include <QtCore/qiodevice.h>
#include <QtGui/qimage.h>
#include <QtGui/qpaintengine.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

/*!
    \class QSvgGenerator
    \brief The QSvgGenerator class is a paint device that writes what is painted on it as an SVG document.

    Set where the document goes and how large the drawing is, then paint on the generator as on any other
    device. The document is complete when the painter ends:

    \snippet svg/svg_viewer.cpp generate

    Rectangles, lines, points and ellipses are written as the SVG elements of the same names, with the
    coordinates and pen widths in pixels that the painter gives them, so that the document looks like the
    same painting on a QImage. Images are embedded as PNG data.

    \differences
    Text is written as an image of its glyphs in the color of the pen, not as \c text elements, because a
    paint engine of Nt GUI receives text as coverage masks; it looks the same but cannot be selected or
    scaled without loss. Clipping is written, as \c clipPath elements, only for \c{SvgVersion::Svg11}: SVG
    Tiny 1.2 has none, so a \c SvgTiny12 document is not clipped. The composition modes other than
    SourceOver are written as SourceOver. The properties are plain functions, as QSvgGenerator is not a
    QObject.
    \enddifferences

    \sa QSvgRenderer, QPainter, QPaintDevice
*/

/*!
    \enum QSvgGenerator::SvgVersion

    This enum describes the version of SVG that a generator writes.

    \value SvgTiny12 SVG Tiny 1.2, the default.
    \value Svg11 SVG 1.1, which can clip.
*/

namespace {

// Writes a coordinate with up to three decimals, whatever the locale.
std::string number(double v) {
    if (!std::isfinite(v))
        v = 0.0;
    const bool negative = v < 0.0;
    const double clamped = std::min(std::fabs(v), 1e15);
    const auto thousandths = static_cast<long long>(clamped * 1000.0 + 0.5);
    std::string out = std::to_string(thousandths / 1000);
    int fraction = static_cast<int>(thousandths % 1000);
    if (fraction != 0) {
        char digits[5] = {'.', static_cast<char>('0' + fraction / 100), static_cast<char>('0' + fraction / 10 % 10),
                          static_cast<char>('0' + fraction % 10), '\0'};
        out += digits;
        while (out.back() == '0')
            out.pop_back();
    }
    return negative && thousandths != 0 ? "-" + out : out;
}

std::string escaped(const QString &text) {
    std::string out;
    for (const char c : text.toUtf8().toStdString()) {
        switch (c) {
        case '&':
            out += "&amp;";
            break;
        case '<':
            out += "&lt;";
            break;
        case '>':
            out += "&gt;";
            break;
        case '"':
            out += "&quot;";
            break;
        default:
            out += c;
        }
    }
    return out;
}

void appendBigEndian(std::string &out, std::uint32_t v) {
    for (int shift = 24; shift >= 0; shift -= 8)
        out += static_cast<char>((v >> static_cast<unsigned>(shift)) & 0xffu);
}

std::uint32_t crc32(const std::string &data, std::size_t from) {
    std::uint32_t crc = 0xffffffffu;
    for (std::size_t i = from; i < data.size(); ++i) {
        crc ^= static_cast<unsigned char>(data[i]);
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1u) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

void appendChunk(std::string &png, const char *type, const std::string &body) {
    appendBigEndian(png, static_cast<std::uint32_t>(body.size()));
    const std::size_t start = png.size();
    png += type;
    png += body;
    appendBigEndian(png, crc32(png, start));
}

// Encodes an image as a PNG with 8 bits of red, green, blue and alpha. The data is stored, not compressed:
// that needs no compression library, and the documents stay small for the icons and text this is used for.
std::string encodePng(const QImage &source) {
    const QImage image = source.convertToFormat(QImage::Format_ARGB32);
    const int width = image.width(), height = image.height();
    std::string raw;
    raw.reserve((static_cast<std::size_t>(width) * 4 + 1) * static_cast<std::size_t>(height));
    for (int y = 0; y < height; ++y) {
        raw += '\0'; // no filter
        for (int x = 0; x < width; ++x) {
            const QRgb p = image.pixel(x, y);
            raw += static_cast<char>(qRed(p));
            raw += static_cast<char>(qGreen(p));
            raw += static_cast<char>(qBlue(p));
            raw += static_cast<char>(qAlpha(p));
        }
    }
    std::string zlib("\x78\x01", 2);
    std::uint32_t a = 1, b = 0;
    for (const char c : raw) {
        a = (a + static_cast<unsigned char>(c)) % 65521u;
        b = (b + a) % 65521u;
    }
    std::size_t pos = 0;
    do {
        const std::size_t n = std::min<std::size_t>(65535, raw.size() - pos);
        zlib += static_cast<char>(pos + n == raw.size() ? 1 : 0);
        zlib += static_cast<char>(n & 0xffu);
        zlib += static_cast<char>(n >> 8u);
        zlib += static_cast<char>(~n & 0xffu);
        zlib += static_cast<char>((~n >> 8u) & 0xffu);
        zlib.append(raw, pos, n);
        pos += n;
    } while (pos < raw.size());
    appendBigEndian(zlib, (b << 16u) | a);

    std::string png("\x89PNG\r\n\x1a\n", 8);
    std::string header;
    appendBigEndian(header, static_cast<std::uint32_t>(width));
    appendBigEndian(header, static_cast<std::uint32_t>(height));
    header += std::string("\x08\x06\x00\x00\x00", 5); // 8 bits, RGBA, deflate, adaptive filtering, not interlaced
    appendChunk(png, "IHDR", header);
    appendChunk(png, "IDAT", zlib);
    appendChunk(png, "IEND", std::string());
    return png;
}

struct Settings {
    QString title;
    QString description;
    QSize size;
    QRectF viewBox;
    QString fileName;
    QIODevice *device = nullptr;
    int resolution = 72;
    QSvgGenerator::SvgVersion version = QSvgGenerator::SvgVersion::SvgTiny12;
};

class SvgPaintEngine : public QPaintEngine {
public:
    explicit SvgPaintEngine(const Settings *settings) : m_settings(settings) {}

    bool begin(QPaintDevice *device) override {
        if (isActive())
            return false;
        m_out = m_settings->device;
        m_opened = false;
        if (!m_out && !m_settings->fileName.isEmpty()) {
            m_file = std::make_unique<QFile>(m_settings->fileName);
            m_out = m_file.get();
        }
        if (!m_out)
            return false;
        if (!m_out->isOpen()) {
            if (!m_out->open(QIODevice::WriteOnly)) {
                m_file.reset();
                return false;
            }
            m_opened = true;
        } else if (!m_out->isWritable()) {
            return false;
        }
        setPaintDevice(device);
        setActive(true);
        m_state = QPaintEngineState();
        m_clipOpen = false;
        m_clipCount = 0;
        m_clipRects.clear();

        const Settings &s = *m_settings;
        std::string head = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n<svg";
        if (s.size.isValid()) {
            const double mm = 25.4 / std::max(1, s.resolution);
            head += " width=\"" + number(s.size.width() * mm) + "mm\" height=\"" + number(s.size.height() * mm) +
                    "mm\"";
        }
        QRectF box = s.viewBox;
        if (box.isEmpty() && s.size.isValid())
            box = QRectF(0.0, 0.0, s.size.width(), s.size.height());
        if (!box.isEmpty())
            head += " viewBox=\"" + number(box.x()) + " " + number(box.y()) + " " + number(box.width()) + " " +
                    number(box.height()) + "\"";
        head += "\n xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\"";
        head += s.version == QSvgGenerator::SvgVersion::Svg11 ? " version=\"1.1\">\n"
                                                              : " version=\"1.2\" baseProfile=\"tiny\">\n";
        if (!s.title.isEmpty())
            head += "<title>" + escaped(s.title) + "</title>\n";
        if (!s.description.isEmpty())
            head += "<desc>" + escaped(s.description) + "</desc>\n";
        write(head);
        return true;
    }

    bool end() override {
        if (m_out) {
            closeClip();
            write("</svg>\n");
            if (m_opened)
                m_out->close();
        }
        m_file.reset();
        m_out = nullptr;
        setPaintDevice(nullptr);
        setActive(false);
        return true;
    }

    void updateState(const QPaintEngineState &state) override {
        m_state = state;
        if (m_settings->version != QSvgGenerator::SvgVersion::Svg11)
            return;
        std::vector<QRect> rects;
        if (state.hasClipping())
            rects = state.clipRegion().rects();
        if (state.hasClipping() == m_clipOpen && rects == m_clipRects)
            return;
        closeClip();
        if (!state.hasClipping())
            return;
        const std::string id = "clip" + std::to_string(++m_clipCount);
        std::string out = "<clipPath id=\"" + id + "\">";
        for (const QRect &r : rects)
            out += "<rect" + box(r.x(), r.y(), r.width(), r.height()) + "/>";
        out += "</clipPath>\n<g clip-path=\"url(#" + id + ")\">\n";
        write(out);
        m_clipOpen = true;
        m_clipRects = rects;
    }

    void drawRects(const QRect *rects, int rectCount) override {
        const QPen pen = m_state.pen();
        const QBrush brush = m_state.brush();
        const bool stroke = pen.style() != Qt::NoPen;
        const int pw = std::max(1, pen.width());
        const int half = (pw - 1) / 2;
        std::string out;
        for (int i = 0; i < rectCount; ++i) {
            const QRect r = rects[i].normalized();
            if (brush.style() != Qt::NoBrush) {
                // Inside the outline, as the raster engine fills it.
                const QRect area = stroke ? QRect(r.x() - half + pw, r.y() - half + pw, r.width() - pw, r.height() - pw)
                                          : r;
                if (!area.isEmpty())
                    out += "<rect" + box(area.x(), area.y(), area.width(), area.height()) + fill(brush.color()) +
                           "/>\n";
            }
            if (stroke) {
                // The outline covers `pw` pixels around the rectangle; its center line is what SVG strokes.
                const double c = pw / 2.0 - half;
                out += "<rect" + box(r.x() + c, r.y() + c, r.width(), r.height()) + " fill=\"none\"" +
                       this->stroke(pen.color(), pw) + "/>\n";
            }
        }
        write(out);
    }

    void drawLines(const QLine *lines, int lineCount) override {
        const QPen pen = m_state.pen();
        if (pen.style() == Qt::NoPen)
            return;
        const int pw = std::max(1, pen.width());
        const double c = pw / 2.0 - (pw - 1) / 2;
        std::string out;
        for (int i = 0; i < lineCount; ++i) {
            out += "<line x1=\"" + number(lines[i].x1() + c) + "\" y1=\"" + number(lines[i].y1() + c) + "\" x2=\"" +
                   number(lines[i].x2() + c) + "\" y2=\"" + number(lines[i].y2() + c) + "\"" +
                   stroke(pen.color(), pw) + " stroke-linecap=\"square\"/>\n";
        }
        write(out);
    }

    void drawPoints(const QPoint *points, int pointCount) override {
        const QPen pen = m_state.pen();
        if (pen.style() == Qt::NoPen)
            return;
        const int pw = std::max(1, pen.width());
        const int half = (pw - 1) / 2;
        std::string out;
        for (int i = 0; i < pointCount; ++i)
            out += "<rect" + box(points[i].x() - half, points[i].y() - half, pw, pw) + fill(pen.color()) + "/>\n";
        write(out);
    }

    void drawEllipse(const QRect &rectIn) override {
        const QRect rect = rectIn.normalized();
        const QPen pen = m_state.pen();
        const QBrush brush = m_state.brush();
        const bool stroke = pen.style() != Qt::NoPen;
        const bool filled = brush.style() != Qt::NoBrush;
        if (rect.isEmpty() || (!stroke && !filled))
            return;
        const int pw = std::max(1, pen.width());
        const double cx = rect.x() + rect.width() / 2.0, cy = rect.y() + rect.height() / 2.0;
        double rx = rect.width() / 2.0, ry = rect.height() / 2.0;
        std::string paint;
        if (!stroke) {
            paint = fill(brush.color());
        } else if (rx <= pw || ry <= pw) {
            paint = fill(pen.color()); // the outline, which lies inside the rectangle, leaves no room
        } else {
            rx -= pw / 2.0;
            ry -= pw / 2.0;
            paint = (filled ? fill(brush.color()) : std::string(" fill=\"none\"")) + this->stroke(pen.color(), pw);
        }
        write("<ellipse cx=\"" + number(cx) + "\" cy=\"" + number(cy) + "\" rx=\"" + number(rx) + "\" ry=\"" +
              number(ry) + "\"" + paint + "/>\n");
    }

    void drawImage(const QRect &rect, const QImage &image, const QRect &sourceRect) override {
        if (image.isNull())
            return;
        const QRect sr = sourceRect.isNull() ? image.rect() : sourceRect.normalized().intersected(image.rect());
        if (sr.isEmpty())
            return;
        const QRect target = rect.isNull() ? QRect(rect.topLeft(), sr.size()) : rect.normalized();
        if (target.isEmpty())
            return;
        writeImage(target, sr == image.rect() ? image : image.copy(sr), m_state.opacity());
    }

    void drawAlphaMask(const QPoint &topLeft, const uchar *mask, int width, int height, qsizetype stride) override {
        if (width <= 0 || height <= 0)
            return;
        const QColor color = m_state.pen().color();
        QImage image(width, height, QImage::Format_ARGB32);
        bool any = false;
        for (int y = 0; y < height; ++y) {
            const uchar *row = mask + static_cast<qsizetype>(y) * stride;
            for (int x = 0; x < width; ++x) {
                const int alpha = (row[x] * color.alpha() + 127) / 255;
                any = any || alpha != 0;
                image.setPixel(x, y, qRgba(color.red(), color.green(), color.blue(), alpha));
            }
        }
        if (any)
            writeImage(QRect(topLeft, QSize(width, height)), image, m_state.opacity());
    }

    Type type() const override { return User; }

private:
    void write(const std::string &text) {
        if (m_out && !text.empty())
            m_out->write(text.data(), static_cast<qint64>(text.size()));
    }

    void closeClip() {
        if (m_clipOpen)
            write("</g>\n");
        m_clipOpen = false;
        m_clipRects.clear();
    }

    static std::string box(double x, double y, double w, double h) {
        return " x=\"" + number(x) + "\" y=\"" + number(y) + "\" width=\"" + number(w) + "\" height=\"" + number(h) +
               "\"";
    }

    std::string paint(const char *property, const QColor &color) const {
        std::string out = std::string(" ") + property + "=\"" + color.name().toUtf8().toStdString() + "\"";
        const double alpha = color.alpha() / 255.0 * m_state.opacity();
        if (alpha < 1.0)
            out += std::string(" ") + property + "-opacity=\"" + number(std::max(0.0, alpha)) + "\"";
        return out;
    }

    std::string fill(const QColor &color) const { return paint("fill", color); }

    std::string stroke(const QColor &color, int width) const {
        return paint("stroke", color) + " stroke-width=\"" + std::to_string(width) + "\"";
    }

    void writeImage(const QRect &target, const QImage &image, qreal opacity) {
        const std::string png = encodePng(image);
        const QByteArray base64 = QByteArray(png.data(), static_cast<qsizetype>(png.size())).toBase64();
        std::string out = "<image" + box(target.x(), target.y(), target.width(), target.height()) +
                          " preserveAspectRatio=\"none\"";
        if (opacity < 1.0)
            out += " opacity=\"" + number(std::max<qreal>(0.0, opacity)) + "\"";
        out += " xlink:href=\"data:image/png;base64,";
        out.append(base64.constData(), static_cast<std::size_t>(base64.size()));
        out += "\"/>\n";
        write(out);
    }

    const Settings *m_settings;
    std::unique_ptr<QFile> m_file;
    QIODevice *m_out = nullptr;
    bool m_opened = false;
    QPaintEngineState m_state;
    bool m_clipOpen = false;
    int m_clipCount = 0;
    std::vector<QRect> m_clipRects;
};

} // namespace

struct QSvgGenerator::Data {
    Settings settings;
    SvgPaintEngine engine{&settings};
};

/*!
    Constructs a generator that writes SVG Tiny 1.2.
*/
QSvgGenerator::QSvgGenerator() : QSvgGenerator(SvgVersion::SvgTiny12) {}

/*!
    Constructs a generator that writes the given \a version of SVG.

    \since 6.5
*/
QSvgGenerator::QSvgGenerator(SvgVersion version) : d(std::make_unique<Data>()) { d->settings.version = version; }

/*!
    Destroys the generator. A painter that paints on it must have ended.
*/
QSvgGenerator::~QSvgGenerator() = default;

/*!
    Returns the title of the document.

    \sa setTitle()
*/
QString QSvgGenerator::title() const { return d->settings.title; }

/*!
    Sets the title of the document, written as its \c title element, to \a title.

    \sa title()
*/
void QSvgGenerator::setTitle(const QString &title) { d->settings.title = title; }

/*!
    Returns the description of the document.

    \sa setDescription()
*/
QString QSvgGenerator::description() const { return d->settings.description; }

/*!
    Sets the description of the document, written as its \c desc element, to \a description.

    \sa description()
*/
void QSvgGenerator::setDescription(const QString &description) { d->settings.description = description; }

/*!
    Returns the size of the drawing in pixels, which is invalid until setSize() is called.

    \sa setSize()
*/
QSize QSvgGenerator::size() const { return d->settings.size; }

/*!
    Sets the size of the drawing to \a size, in pixels. It is the size that the painter sees, and the
    \c width and \c height of the document, written in millimeters at the resolution(). Set it before
    painting begins.

    \sa size(), setViewBox(), setResolution()
*/
void QSvgGenerator::setSize(const QSize &size) { d->settings.size = size; }

/*!
    Returns the view box as a rectangle of integers.

    \sa viewBoxF()
*/
QRect QSvgGenerator::viewBox() const {
    const QRectF &box = d->settings.viewBox;
    return QRect(qRound(box.x()), qRound(box.y()), qRound(box.width()), qRound(box.height()));
}

/*!
    Returns the view box of the document, which is null until setViewBox() is called.

    \sa setViewBox()
*/
QRectF QSvgGenerator::viewBoxF() const { return d->settings.viewBox; }

/*!
    \overload
    Sets the view box to \a viewBox.
*/
void QSvgGenerator::setViewBox(const QRect &viewBox) { setViewBox(QRectF(viewBox)); }

/*!
    Sets the view box of the document to \a viewBox: the part of the painted coordinates that a viewer
    shows in the size of the document. Without one, the document shows the rectangle of its size().

    \sa viewBoxF(), setSize()
*/
void QSvgGenerator::setViewBox(const QRectF &viewBox) { d->settings.viewBox = viewBox; }

/*!
    Returns the name of the file the document is written to.

    \sa setFileName()
*/
QString QSvgGenerator::fileName() const { return d->settings.fileName; }

/*!
    Sets the file that the document is written to, to \a fileName, and forgets the output device. The file
    is created, or replaced, when painting begins.

    \sa fileName(), setOutputDevice()
*/
void QSvgGenerator::setFileName(const QString &fileName) {
    if (paintingActive())
        return;
    d->settings.fileName = fileName;
    d->settings.device = nullptr;
}

/*!
    Returns the device the document is written to, or \c nullptr if there is none.

    \sa setOutputDevice()
*/
QIODevice *QSvgGenerator::outputDevice() const { return d->settings.device; }

/*!
    Sets the device that the document is written to, to \a outputDevice, and forgets the file name. The
    generator does not own the device. A device that is not open is opened for writing when painting
    begins and closed when it ends; an open one must be writable and stays open.

    \sa outputDevice(), setFileName()
*/
void QSvgGenerator::setOutputDevice(QIODevice *outputDevice) {
    if (paintingActive())
        return;
    d->settings.device = outputDevice;
    d->settings.fileName.clear();
}

/*!
    Returns the resolution in dots per inch. The default is 72.

    \sa setResolution()
*/
int QSvgGenerator::resolution() const { return d->settings.resolution; }

/*!
    Sets the resolution to \a dpi dots per inch. It is what the logical DPI of the device reports, and
    converts the size() to the millimeters of the document. Values below 1 are ignored.

    \sa resolution()
*/
void QSvgGenerator::setResolution(int dpi) {
    if (dpi > 0)
        d->settings.resolution = dpi;
}

/*!
    Returns the version of SVG that the generator writes.

    \since 6.5
*/
QSvgGenerator::SvgVersion QSvgGenerator::svgVersion() const { return d->settings.version; }

/*!
    \reimp
    Returns the engine that writes the document.
*/
QPaintEngine *QSvgGenerator::paintEngine() const { return &d->engine; }

/*!
    \reimp
    Returns the value of \a metric: the size() of the drawing and its resolution().
*/
int QSvgGenerator::metric(PaintDeviceMetric metric) const {
    const Settings &s = d->settings;
    switch (metric) {
    case PdmWidth:
        return s.size.isValid() ? s.size.width() : 0;
    case PdmHeight:
        return s.size.isValid() ? s.size.height() : 0;
    case PdmWidthMM:
        return s.size.isValid() ? qRound(s.size.width() * 25.4 / s.resolution) : 0;
    case PdmHeightMM:
        return s.size.isValid() ? qRound(s.size.height() * 25.4 / s.resolution) : 0;
    case PdmDpiX:
    case PdmDpiY:
        return s.resolution;
    default:
        return QPaintDevice::metric(metric);
    }
}
