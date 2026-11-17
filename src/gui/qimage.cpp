// MIT License - see LICENSE
#include <QtGui/qimage.h>

#include "qimageformat_p.h"
#include <QtGui/qpainter.h>
#include <cmath>

#include "qrasterpaintengine_p.h"

#include <QtGui/qpaintengine.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <limits>
#include <vector>

namespace {

uint *asPixels(uchar *p) { return reinterpret_cast<uint *>(p); }

using QtImageFormat::bytesPerPixel;
using QtImageFormat::isGray;

// Lines start on 32-bit boundaries, as in Qt.
qsizetype alignedLine(int width, QImage::Format format) {
    return (static_cast<qsizetype>(width) * bytesPerPixel(format) + 3) / 4 * 4;
}

using QtImageFormat::fromArgb;
using QtImageFormat::toArgb;

} // namespace

struct QImage::Data {
    int width = 0;
    int height = 0;
    Format format = Format_Invalid;
    qsizetype bytesPerLine = 0;
    std::vector<uchar> storage; // empty when the image wraps external memory
    uchar *ptr = nullptr;
    bool readOnly = false; // wraps const external memory
    qreal devicePixelRatio = 1.0;
    // cacheKey(): the serial number tells the pixels apart from those of other images, and the write count goes up
    // each time someone may write to them.
    quint32 serial = nextSerial();
    std::atomic<quint32> writes{0};

    static quint32 nextSerial() {
        static std::atomic<quint32> counter{0};
        return counter.fetch_add(1, std::memory_order_relaxed) + 1;
    }

    uchar *row(int y) const { return ptr + static_cast<qsizetype>(y) * bytesPerLine; }
};

/*!
    \class QImage
    \brief The QImage class provides a hardware-independent image of 32-bit pixels.

    QImage stores an image as an array of pixels and doubles as a paint
    device: draw on it with QPainter. Windows that paint with a
    QRasterWindow draw into a QImage as well.

    \code
    QImage image(200, 100, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.fillRect(10, 10, 50, 30, Qt::red);
    painter.end();
    QRgb pixel = image.pixel(20, 20);
    \endcode

    Copies of an image share their pixels until one of them is written to
    (implicit sharing). Constructing an image on external memory with the
    pointer constructors does not copy the data; the data must outlive the
    image and all of its copies, and changing the image changes the memory.

    The width(), height() and depth() of an image come from QPaintDevice. The
    depth is 32 (8 for Format_Grayscale8), or 0 for a null image.

    \differences
    The formats are the 32-bit Format_RGB32, Format_ARGB32, Format_ARGB32_Premultiplied and the byte-ordered
    Format_RGBX8888, Format_RGBA8888 and Format_RGBA8888_Premultiplied, and the 8-bit Format_Grayscale8, which
    a QPainter cannot paint on; Format_RGB888 data is
    converted to Format_RGB32 when an image is made from it; files with other pixel formats are
    converted when they load. Image files are PNG, BMP and JPEG (JPEG only if Nt
    was built with libjpeg), found by their contents; there is no QImageWriter and no
    plugins for other formats (QImageReader reads the same formats). There is no
    \c transformed(), \c mirrored(), \c rgbSwapped(), no color tables and no
    metadata.
    The paint engine of an image is not shared between copies. Implicit sharing
    is not thread-safe for concurrent writers.
    \enddifferences

    \sa QPainter, QColor, QPaintDevice
*/

/*!
    \enum QImage::Format

    This enum describes how the pixels of an image are stored. Every format but
    Format_Grayscale8 has 32 bits per pixel. The ARGB formats are written in the byte order of the machine as 0xAARRGGBB;
    the RGBA8888 formats are the bytes R, G, B, A in that order, the layout OpenGL reads as
    \c GL_RGBA with \c GL_UNSIGNED_BYTE.

    \value Format_Invalid The image is invalid.
    \value Format_RGB32 The alpha byte is ignored and reads as 0xff.
    \value Format_ARGB32 The pixels have a straight, non-premultiplied alpha.
    \value Format_ARGB32_Premultiplied The color components are multiplied by the alpha; painting is fastest in this format.
    \value Format_RGB888 Three bytes R, G, B per pixel. Nt accepts it only as input: data in this format is
           converted to a Format_RGB32 image, and so is an image made or converted to it.
    \value Format_RGBX8888 The bytes R, G, B, X; the fourth byte reads as 0xff.
    \value Format_RGBA8888 The bytes R, G, B, A with a straight alpha.
    \value Format_RGBA8888_Premultiplied The bytes R, G, B, A with the colors multiplied by the alpha.
    \value Format_Grayscale8 One byte of gray per pixel, the only format that is not 32 bits. It can be read and
           written pixel by pixel or line by line, converted, scaled, drawn and saved, but a QPainter cannot paint
           on it.
*/

/*!
    Constructs a null image.

    \sa isNull()
*/
QImage::QImage() noexcept = default;

/*!
    Constructs an image of the given \a size and \a format. The pixels are not
    initialized; call fill(). If \a format is invalid or the size is not
    positive, the image is null.
*/
QImage::QImage(const QSize &size, Format format) : QImage(size.width(), size.height(), format) {}

/*!
    \overload

    Constructs an image that is \a width pixels wide and \a height pixels high.
*/
QImage::QImage(int width, int height, Format format) {
    if (width <= 0 || height <= 0 || format == Format_Invalid)
        return;
    if (format == Format_RGB888)
        format = Format_RGB32; // stored in 32 bits
    const qsizetype bpl = alignedLine(width, format);
    if (static_cast<std::size_t>(bpl) > std::numeric_limits<std::size_t>::max() / static_cast<std::size_t>(height))
        return;
    auto data = std::make_shared<Data>();
    data->width = width;
    data->height = height;
    data->format = format;
    data->bytesPerLine = bpl;
    data->storage.assign(static_cast<std::size_t>(bpl) * static_cast<std::size_t>(height), 0);
    data->ptr = data->storage.data();
    d = std::move(data);
}

/*!
    \overload

    Constructs an image of \a width by \a height pixels in \a format that uses
    the memory at \a data, which must be 32-bit aligned, hold \a height lines of
    \c{width * 4} bytes (\c{width * 3} for Format_RGB888 and \c{width} for Format_Grayscale8, rounded up to a
    multiple of 4), and outlive the image.
*/
QImage::QImage(uchar *data, int width, int height, Format format)
    : QImage(data, width, height,
             format == Format_RGB888 ? (static_cast<qsizetype>(width) * 3 + 3) / 4 * 4 : alignedLine(width, format),
             format) {}

/*!
    \overload

    Constructs an image on the memory at \a data, which holds \a height lines
    that start \a bytesPerLine bytes apart. \a bytesPerLine must be at least
    \c{width * 4} (\c width for Format_Grayscale8) and a multiple of 4.

    Format_RGB888 data, lines of \c{width * 3} bytes or more, is copied into a new Format_RGB32 image instead
    of being used in place.
*/
QImage::QImage(uchar *data, int width, int height, qsizetype bytesPerLine, Format format) {
    if (format == Format_RGB888) {
        if (!data || width <= 0 || height <= 0 || bytesPerLine < static_cast<qsizetype>(width) * 3)
            return;
        QImage copy(width, height, Format_RGB32);
        if (copy.isNull())
            return;
        for (int y = 0; y < height; ++y) {
            const uchar *src = data + static_cast<qsizetype>(y) * bytesPerLine;
            uint *dst = asPixels(copy.d->row(y));
            for (int x = 0; x < width; ++x, src += 3)
                dst[x] = qRgb(src[0], src[1], src[2]);
        }
        d = std::move(copy.d);
        return;
    }
    if (!data || width <= 0 || height <= 0 || format == Format_Invalid ||
        bytesPerLine < static_cast<qsizetype>(width) * bytesPerPixel(format) || bytesPerLine % 4 != 0)
        return;
    auto p = std::make_shared<Data>();
    p->width = width;
    p->height = height;
    p->format = format;
    p->bytesPerLine = bytesPerLine;
    p->ptr = data;
    d = std::move(p);
}

/*!
    \overload

    Constructs a read-only image on the constant memory at \a data. The first
    write detaches the image, which copies the pixels.
*/
QImage::QImage(const uchar *data, int width, int height, Format format)
    : QImage(const_cast<uchar *>(data), width, height, format) {
    if (d && format != Format_RGB888)
        d->readOnly = true;
}

/*!
    \overload

    Constructs a read-only image on the constant memory at \a data, with lines
    \a bytesPerLine bytes apart.
*/
QImage::QImage(const uchar *data, int width, int height, qsizetype bytesPerLine, Format format)
    : QImage(const_cast<uchar *>(data), width, height, bytesPerLine, format) {
    if (d && format != Format_RGB888)
        d->readOnly = true;
}

/*!
    Constructs a copy of \a other. The pixels are shared until one image is
    changed.
*/
QImage::QImage(const QImage &other) : QPaintDevice(), d(other.d) {}

/*!
    Moves \a other into the new image, which leaves \a other null.
*/
QImage::QImage(QImage &&other) noexcept : QPaintDevice(), d(std::move(other.d)), m_engine(std::move(other.m_engine)) {}

/*!
    Destroys the image.
*/
QImage::~QImage() = default;

/*!
    Assigns \a other to this image; the pixels are shared.
*/
QImage &QImage::operator=(const QImage &other) {
    if (this != &other) {
        d = other.d;
        m_engine.reset();
    }
    return *this;
}

/*!
    Moves \a other into this image, which leaves \a other null.
*/
QImage &QImage::operator=(QImage &&other) noexcept {
    d = std::move(other.d);
    m_engine = std::move(other.m_engine);
    return *this;
}

/*!
    Swaps this image with \a other.
*/
void QImage::swap(QImage &other) noexcept {
    d.swap(other.d);
    m_engine.swap(other.m_engine);
}

/*!
    Returns \c true if the image has no pixels.
*/
bool QImage::isNull() const noexcept { return !d; }

/*!
    Returns the format of the image.
*/
QImage::Format QImage::format() const noexcept { return d ? d->format : Format_Invalid; }

/*!
    \fn QSize QImage::size() const

    Returns the size of the image in pixels.
*/

/*!
    \fn QRect QImage::rect() const

    Returns the rectangle that covers the image, with its top left at the
    origin.
*/

/*!
    Returns the number of bytes in one scan line, which can be larger than four
    times the width.
*/
qsizetype QImage::bytesPerLine() const noexcept { return d ? d->bytesPerLine : 0; }

/*!
    Returns the number of bytes the pixels occupy.
*/
qsizetype QImage::sizeInBytes() const noexcept { return d ? d->bytesPerLine * d->height : 0; }

/*!
    Returns \c true if the format has an alpha channel, that is, unless it is
    Format_RGB32.
*/
bool QImage::hasAlphaChannel() const noexcept { return d && !QtImageFormat::isOpaque(d->format); }

/*!
    \fn bool QImage::valid(int x, int y) const

    Returns \c true if the pixel at (\a x, \a y) lies inside the image.
*/

/*!
    \fn bool QImage::valid(const QPoint &pt) const
    \overload

    Returns \c true if \a pt lies inside the image.
*/

/*!
    Returns a pointer to the first pixel, detaching the image from any copies
    first. Returns \c nullptr for a null image.

    \sa constBits(), scanLine()
*/
uchar *QImage::bits() {
    if (!d)
        return nullptr;
    detach();
    return d->ptr;
}

/*!
    \fn const uchar *QImage::bits() const
    \overload

    Returns a pointer to the first pixel without detaching.
*/

/*!
    Returns a pointer to the first pixel. It does not detach; use it only to
    read.

    \sa bits()
*/
const uchar *QImage::constBits() const noexcept { return d ? d->ptr : nullptr; }

/*!
    Returns a pointer to the pixels of line \a y, detaching the image first.
    \a y must be in range.

    \sa constScanLine(), bits()
*/
uchar *QImage::scanLine(int y) {
    if (!d || y < 0 || y >= d->height)
        return nullptr;
    detach();
    return d->row(y);
}

/*!
    \fn const uchar *QImage::scanLine(int y) const
    \overload

    Returns a pointer to the pixels of line \a y without detaching.
*/

/*!
    Returns a pointer to the pixels of line \a y, or \c nullptr if \a y is out
    of range. It does not detach; use it only to read.

    \sa scanLine()
*/
const uchar *QImage::constScanLine(int y) const noexcept {
    if (!d || y < 0 || y >= d->height)
        return nullptr;
    return d->row(y);
}

/*!
    Fills the whole image with the pixel value \a pixel, which is stored as it
    is (so premultiplied in Format_ARGB32_Premultiplied).

    \sa setPixel()
*/
void QImage::fill(uint pixel) {
    if (!d)
        return;
    detach();
    for (int y = 0; y < d->height; ++y) {
        if (isGray(d->format)) {
            std::memset(d->row(y), static_cast<int>(pixel & 0xffu), static_cast<std::size_t>(d->width));
            continue;
        }
        uint *line = asPixels(d->row(y));
        std::fill(line, line + d->width, pixel);
    }
}

/*!
    \overload

    Fills the image with \a color, converted to the format of the image.
*/
void QImage::fill(const QColor &color) {
    if (d)
        fill(isGray(d->format) ? static_cast<uint>(qGray(color.rgb()))
                               : static_cast<uint>(fromArgb(color.rgba(), d->format)));
}

/*!
    \overload

    Fills the image with \a color.
*/
void QImage::fill(Qt::GlobalColor color) { fill(QColor(color)); }

/*!
    Returns the color of the pixel at (\a x, \a y) as a straight (not
    premultiplied) QRgb. Returns 0 if the position is outside the image.

    \sa setPixel(), pixelColor()
*/
QRgb QImage::pixel(int x, int y) const {
    if (!valid(x, y))
        return 0;
    if (isGray(d->format)) {
        const int v = d->row(y)[x];
        return qRgb(v, v, v);
    }
    return toArgb(asPixels(d->row(y))[x], d->format);
}

/*!
    \fn QRgb QImage::pixel(const QPoint &pt) const
    \overload

    Returns the color of the pixel at \a pt.
*/

/*!
    Sets the pixel at (\a x, \a y) to \a index_or_rgb, a straight QRgb that is
    converted to the format of the image. Does nothing outside the image.

    \sa pixel()
*/
void QImage::setPixel(int x, int y, uint index_or_rgb) {
    if (!valid(x, y))
        return;
    detach();
    if (isGray(d->format))
        d->row(y)[x] = static_cast<uchar>(qGray(index_or_rgb));
    else
        asPixels(d->row(y))[x] = fromArgb(index_or_rgb, d->format);
}

/*!
    \fn void QImage::setPixel(const QPoint &pt, uint index_or_rgb)
    \overload

    Sets the pixel at \a pt to \a index_or_rgb.
*/

/*!
    Returns the pixel at (\a x, \a y) as a QColor, which is invalid when the
    position is outside the image.

    \sa setPixelColor(), pixel()
*/
QColor QImage::pixelColor(int x, int y) const {
    if (!valid(x, y))
        return QColor();
    return QColor::fromRgba(pixel(x, y));
}

/*!
    Sets the pixel at (\a x, \a y) to \a color.

    \sa pixelColor(), setPixel()
*/
void QImage::setPixelColor(int x, int y, const QColor &color) {
    if (color.isValid())
        setPixel(x, y, color.rgba());
}

/*!
    Returns a deep copy of the part of the image inside \a rect. A null
    \a rect copies the whole image. Areas of \a rect outside the image are
    transparent black (or opaque black in formats without alpha).
*/
QImage QImage::copy(const QRect &rect) const {
    if (!d)
        return QImage();
    const QRect r = rect.isNull() ? this->rect() : rect.normalized();
    QImage out(r.width(), r.height(), d->format);
    if (out.isNull())
        return out;
    out.d->devicePixelRatio = d->devicePixelRatio;
    out.fill(isGray(d->format) ? 0u : QtImageFormat::clearValue(d->format));
    const QRect common = r.intersected(this->rect());
    const int bpp = bytesPerPixel(d->format);
    for (int y = common.top(); y <= common.bottom(); ++y) {
        const uchar *src = d->row(y) + static_cast<qsizetype>(common.left()) * bpp;
        uchar *dst = out.d->row(y - r.top()) + static_cast<qsizetype>(common.left() - r.left()) * bpp;
        std::memcpy(dst, src, static_cast<std::size_t>(common.width()) * static_cast<std::size_t>(bpp));
    }
    return out;
}

/*!
    \fn QImage QImage::copy(int x, int y, int w, int h) const
    \overload

    Returns a copy of the part of the image at (\a x, \a y) that is \a w by
    \a h pixels.
*/

/*!
    Returns a copy of the image in the given \a format, or a null image if
    \a format is invalid. Converting to Format_RGB32, Format_RGBX8888 or Format_Grayscale8 drops the alpha
    channel; converting to Format_Grayscale8 keeps the luminance, as qGray() gives it.
*/
QImage QImage::convertToFormat(Format format) const {
    if (!d || format == Format_Invalid)
        return QImage();
    if (format == Format_RGB888)
        format = Format_RGB32;
    if (format == d->format)
        return *this;
    QImage out(d->width, d->height, format);
    out.d->devicePixelRatio = d->devicePixelRatio;
    for (int y = 0; y < d->height; ++y) {
        if (isGray(d->format)) {
            const uchar *src = d->row(y);
            uint *dst = asPixels(out.d->row(y));
            for (int x = 0; x < d->width; ++x)
                dst[x] = fromArgb(qRgb(src[x], src[x], src[x]), format);
        } else if (isGray(format)) {
            const uint *src = asPixels(d->row(y));
            uchar *dst = out.d->row(y);
            for (int x = 0; x < d->width; ++x)
                dst[x] = static_cast<uchar>(qGray(toArgb(src[x], d->format)));
        } else {
            const uint *src = asPixels(d->row(y));
            uint *dst = asPixels(out.d->row(y));
            for (int x = 0; x < d->width; ++x)
                dst[x] = fromArgb(toArgb(src[x], d->format), format);
        }
    }
    return out;
}

/*!
    Returns a copy of the image scaled to \a width by \a height pixels, resampled as \a mode says. \a aspectMode
    decides whether the image fills that size (Qt::IgnoreAspectRatio), fits inside it (Qt::KeepAspectRatio) or
    covers it (Qt::KeepAspectRatioByExpanding), keeping its proportions. Returns a null image for a null image or an
    empty size.
*/
QImage QImage::scaled(int width, int height, Qt::AspectRatioMode aspectMode, Qt::TransformationMode mode) const {
    return scaled(QSize(width, height), aspectMode, mode);
}

/*!
    \overload

    Returns a copy of the image scaled to \a size with \a aspectMode and \a mode.
*/
QImage QImage::scaled(const QSize &size, Qt::AspectRatioMode aspectMode, Qt::TransformationMode mode) const {
    if (!d || size.isEmpty())
        return QImage();
    const QSize target = this->size().scaled(size, aspectMode);
    if (target.isEmpty())
        return QImage();
    if (target == this->size())
        return *this;
    if (isGray(d->format))
        return convertToFormat(Format_RGB32).scaled(target, Qt::IgnoreAspectRatio, mode).convertToFormat(d->format);
    const bool opaque = QtImageFormat::isOpaque(d->format);
    QImage out(target, opaque ? Format_RGB32 : Format_ARGB32_Premultiplied);
    out.d->devicePixelRatio = d->devicePixelRatio;
    out.fill(opaque ? 0xff000000u : 0u);
    QPainter painter(&out);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, mode == Qt::SmoothTransformation);
    painter.drawImage(QRectF(0, 0, target.width(), target.height()), *this, QRectF(0, 0, d->width, d->height));
    painter.end();
    return out.format() == d->format ? out : out.convertToFormat(d->format);
}

/*!
    Returns a copy of the image scaled to \a width pixels wide, keeping its proportions, resampled as \a mode says.
*/
QImage QImage::scaledToWidth(int width, Qt::TransformationMode mode) const {
    if (!d || width <= 0 || d->width == 0)
        return QImage();
    const int height = std::max(1, static_cast<int>(std::lround(static_cast<double>(d->height) * width / d->width)));
    return scaled(QSize(width, height), Qt::IgnoreAspectRatio, mode);
}

/*!
    Returns a copy of the image scaled to \a height pixels high, keeping its proportions, resampled as \a mode says.
*/
QImage QImage::scaledToHeight(int height, Qt::TransformationMode mode) const {
    if (!d || height <= 0 || d->height == 0)
        return QImage();
    const int width = std::max(1, static_cast<int>(std::lround(static_cast<double>(d->width) * height / d->height)));
    return scaled(QSize(width, height), Qt::IgnoreAspectRatio, mode);
}

/*!
    Makes the image own its pixels, copying them if they are shared with
    another image or live in external memory that the image does not own.

    \sa isDetached()
*/
void QImage::detach() {
    if (!d)
        return;
    if (d.use_count() == 1 && !d->readOnly) {
        d->writes.fetch_add(1, std::memory_order_relaxed); // the caller is about to write
        return;
    }
    auto copy = std::make_shared<Data>();
    copy->width = d->width;
    copy->height = d->height;
    copy->format = d->format;
    copy->devicePixelRatio = d->devicePixelRatio;
    copy->bytesPerLine = alignedLine(d->width, d->format);
    copy->storage.resize(static_cast<std::size_t>(copy->bytesPerLine) * static_cast<std::size_t>(d->height));
    copy->ptr = copy->storage.data();
    for (int y = 0; y < d->height; ++y)
        std::memcpy(copy->row(y), d->row(y),
                    static_cast<std::size_t>(d->width) * static_cast<std::size_t>(bytesPerPixel(d->format)));
    d = std::move(copy);
}

/*!
    Sets the device pixel ratio of the image to \a ratio: the number of pixels of the image that
    stand for one unit of the coordinates of a QPainter that paints on it. A ratio of 2 makes
    a painter draw everything twice as large, and drawImage() show the image at half its size.
    Values below 1 are ignored. The backing store of a window on a high-resolution screen is an
    image with this set.

    \sa QPaintDevice::devicePixelRatio()
*/
void QImage::setDevicePixelRatio(qreal ratio) {
    if (!d || ratio < 1.0 || ratio == d->devicePixelRatio)
        return;
    if (!isDetached())
        detach();
    d->devicePixelRatio = ratio;
}

/*!
    Returns \c true if no other image shares the pixels and the image does not
    wrap read-only memory.

    \sa detach()
*/
bool QImage::isDetached() const noexcept { return !d || (d.use_count() == 1 && !d->readOnly); }

/*!
    \since 4.0
    Returns a number that identifies the pixels of the image: two images with the same key have the same pixels.
    Copies share the key until one of them is written to, and writing to an image, through bits(), scanLine(),
    fill(), setPixel() or a QPainter, changes it. A null image returns 0.

    Caches of converted images, such as the textures of the OpenGL paint engine, are keyed on it.

    \sa detach()
*/
qint64 QImage::cacheKey() const {
    if (!d)
        return 0;
    return static_cast<qint64>((static_cast<quint64>(d->serial) << 32) | d->writes.load(std::memory_order_relaxed));
}

/*!
    Returns the paint engine of the image: a raster engine. Returns
    \c nullptr for a null image, and for a Format_Grayscale8 image, which a
    painter cannot paint on.
*/
QPaintEngine *QImage::paintEngine() const {
    if (!d || isGray(d->format))
        return nullptr; // the raster engine paints 32-bit pixels
    if (!m_engine)
        m_engine = std::make_unique<QRasterPaintEngine>();
    return m_engine.get();
}

/*!
    Returns \c true if \a other has the same size and format and every pixel
    is equal.
*/
bool QImage::operator==(const QImage &other) const {
    if (d == other.d)
        return true;
    if (!d || !other.d || d->width != other.d->width || d->height != other.d->height || d->format != other.d->format)
        return false;
    for (int y = 0; y < d->height; ++y) {
        if (std::memcmp(d->row(y), other.d->row(y),
                        static_cast<std::size_t>(d->width) * static_cast<std::size_t>(bytesPerPixel(d->format))) != 0)
            return false;
    }
    return true;
}

/*!
    \fn bool QImage::operator!=(const QImage &other) const

    Returns \c true if the image differs from \a other.
*/

/*!
    \reimp
*/
int QImage::metric(PaintDeviceMetric metric) const {
    switch (metric) {
    case PdmWidth:
        return d ? d->width : 0;
    case PdmHeight:
        return d ? d->height : 0;
    case PdmDepth:
        return d ? 8 * bytesPerPixel(d->format) : 0;
    case PdmDevicePixelRatio:
        return d ? static_cast<int>(d->devicePixelRatio) : 1;
    case PdmDevicePixelRatioScaled:
        return d ? qRound(d->devicePixelRatio * 65536.0) : 65536;
    default:
        return QPaintDevice::metric(metric);
    }
}
