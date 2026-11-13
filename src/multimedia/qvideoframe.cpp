// MIT License - see LICENSE
#include <QtMultimedia/qvideoframe.h>

#include "qvideobackend_p.h"

#include <QtGui/qrgb.h>

#include <algorithm>
#include <cstring>

using Format = QVideoFrameFormat;

namespace {

// Bytes per pixel of the packed formats, or 0.
int packedBytesPerPixel(Format::PixelFormat format) {
    switch (format) {
    case Format::Format_ARGB8888:
    case Format::Format_ARGB8888_Premultiplied:
    case Format::Format_XRGB8888:
    case Format::Format_BGRA8888:
    case Format::Format_BGRA8888_Premultiplied:
    case Format::Format_BGRX8888:
    case Format::Format_ABGR8888:
    case Format::Format_XBGR8888:
    case Format::Format_RGBA8888:
    case Format::Format_RGBX8888:
    case Format::Format_AYUV:
    case Format::Format_AYUV_Premultiplied:
        return 4;
    case Format::Format_UYVY:
    case Format::Format_YUYV:
    case Format::Format_Y16:
        return 2;
    case Format::Format_Y8:
        return 1;
    default:
        return 0;
    }
}

// Fixed-point YUV to RGB, scaled by 256.
struct YuvMatrix {
    int ky, rv, gu, gv, bu, yOffset;

    QRgb rgb(int y, int u, int v, int a = 255) const {
        const int c = ky * (y - yOffset);
        const int d = u - 128;
        const int e = v - 128;
        const int r = (c + rv * e + 128) >> 8;
        const int g = (c - gu * d - gv * e + 128) >> 8;
        const int b = (c + bu * d + 128) >> 8;
        return qRgba(std::clamp(r, 0, 255), std::clamp(g, 0, 255), std::clamp(b, 0, 255), a);
    }
};

YuvMatrix matrixFor(const QVideoFrameFormat &format) {
    const bool bt709 =
        format.colorSpace() == Format::ColorSpace_BT709 || format.colorSpace() == Format::ColorSpace_BT2020;
    if (format.colorRange() == Format::ColorRange_Full)
        return bt709 ? YuvMatrix{256, 403, 48, 120, 475, 0} : YuvMatrix{256, 359, 88, 183, 454, 0};
    return bt709 ? YuvMatrix{298, 459, 55, 136, 541, 16} : YuvMatrix{298, 409, 100, 208, 516, 16};
}

quint16 read16(const uchar *p) {
    quint16 v;
    std::memcpy(&v, p, 2);
    return v;
}

// Converts the pixels of a mapped frame to one line of RGB32 or ARGB32.
void convertLine(const QVideoFramePrivate &f, const YuvMatrix &m, int y, QRgb *out) {
    const auto *data = reinterpret_cast<const uchar *>(f.data.constData());
    const int w = f.format.frameWidth();
    const uchar *line = data + f.offset[0] + qsizetype(y) * f.stride[0];
    switch (f.format.pixelFormat()) {
    case Format::Format_ARGB8888:
    case Format::Format_ARGB8888_Premultiplied:
    case Format::Format_XRGB8888: {
        const bool alpha = f.format.pixelFormat() != Format::Format_XRGB8888;
        for (int x = 0; x < w; ++x, line += 4)
            out[x] = qRgba(line[1], line[2], line[3], alpha ? line[0] : 255);
        break;
    }
    case Format::Format_BGRA8888:
    case Format::Format_BGRA8888_Premultiplied:
    case Format::Format_BGRX8888: {
        const bool alpha = f.format.pixelFormat() != Format::Format_BGRX8888;
        for (int x = 0; x < w; ++x, line += 4)
            out[x] = qRgba(line[2], line[1], line[0], alpha ? line[3] : 255);
        break;
    }
    case Format::Format_ABGR8888:
    case Format::Format_XBGR8888: {
        const bool alpha = f.format.pixelFormat() == Format::Format_ABGR8888;
        for (int x = 0; x < w; ++x, line += 4)
            out[x] = qRgba(line[3], line[2], line[1], alpha ? line[0] : 255);
        break;
    }
    case Format::Format_RGBA8888:
    case Format::Format_RGBX8888: {
        const bool alpha = f.format.pixelFormat() == Format::Format_RGBA8888;
        for (int x = 0; x < w; ++x, line += 4)
            out[x] = qRgba(line[0], line[1], line[2], alpha ? line[3] : 255);
        break;
    }
    case Format::Format_AYUV:
    case Format::Format_AYUV_Premultiplied:
        for (int x = 0; x < w; ++x, line += 4)
            out[x] = m.rgb(line[1], line[2], line[3], line[0]);
        break;
    case Format::Format_YUYV:
    case Format::Format_UYVY: {
        const bool yuyv = f.format.pixelFormat() == Format::Format_YUYV;
        for (int x = 0; x < w; ++x) {
            const uchar *pair = line + (x / 2) * 4;
            const int luma = yuyv ? pair[(x % 2) * 2] : pair[1 + (x % 2) * 2];
            const int u = yuyv ? pair[1] : pair[0];
            const int v = yuyv ? pair[3] : pair[2];
            out[x] = m.rgb(luma, u, v);
        }
        break;
    }
    case Format::Format_Y8:
        for (int x = 0; x < w; ++x)
            out[x] = qRgb(line[x], line[x], line[x]);
        break;
    case Format::Format_Y16:
        for (int x = 0; x < w; ++x) {
            const int g = read16(line + x * 2) >> 8;
            out[x] = qRgb(g, g, g);
        }
        break;
    case Format::Format_NV12:
    case Format::Format_NV21: {
        const uchar *uv = data + f.offset[1] + qsizetype(y / 2) * f.stride[1];
        const bool nv12 = f.format.pixelFormat() == Format::Format_NV12;
        for (int x = 0; x < w; ++x) {
            const uchar *c = uv + (x / 2) * 2;
            out[x] = m.rgb(line[x], nv12 ? c[0] : c[1], nv12 ? c[1] : c[0]);
        }
        break;
    }
    case Format::Format_P010:
    case Format::Format_P016: {
        const uchar *uv = data + f.offset[1] + qsizetype(y / 2) * f.stride[1];
        for (int x = 0; x < w; ++x) {
            const uchar *c = uv + (x / 2) * 4;
            out[x] = m.rgb(read16(line + x * 2) >> 8, read16(c) >> 8, read16(c + 2) >> 8);
        }
        break;
    }
    case Format::Format_YUV420P:
    case Format::Format_YV12:
    case Format::Format_YUV422P: {
        const int cy = f.format.pixelFormat() == Format::Format_YUV422P ? y : y / 2;
        const bool yv12 = f.format.pixelFormat() == Format::Format_YV12;
        const uchar *u = data + f.offset[yv12 ? 2 : 1] + qsizetype(cy) * f.stride[1];
        const uchar *v = data + f.offset[yv12 ? 1 : 2] + qsizetype(cy) * f.stride[2];
        for (int x = 0; x < w; ++x)
            out[x] = m.rgb(line[x], u[x / 2], v[x / 2]);
        break;
    }
    case Format::Format_YUV420P10: {
        const uchar *u = data + f.offset[1] + qsizetype(y / 2) * f.stride[1];
        const uchar *v = data + f.offset[2] + qsizetype(y / 2) * f.stride[2];
        for (int x = 0; x < w; ++x)
            out[x] = m.rgb(read16(line + x * 2) >> 2, read16(u + (x / 2) * 2) >> 2, read16(v + (x / 2) * 2) >> 2);
        break;
    }
    default:
        std::fill(out, out + w, qRgb(0, 0, 0));
        break;
    }
}

bool hasAlpha(Format::PixelFormat format) {
    switch (format) {
    case Format::Format_ARGB8888:
    case Format::Format_ARGB8888_Premultiplied:
    case Format::Format_BGRA8888:
    case Format::Format_BGRA8888_Premultiplied:
    case Format::Format_ABGR8888:
    case Format::Format_RGBA8888:
    case Format::Format_AYUV:
    case Format::Format_AYUV_Premultiplied:
        return true;
    default:
        return false;
    }
}

bool isPremultiplied(Format::PixelFormat format) {
    return format == Format::Format_ARGB8888_Premultiplied || format == Format::Format_BGRA8888_Premultiplied ||
           format == Format::Format_AYUV_Premultiplied;
}

// Applies the scan line direction, mirroring and rotation of a frame to its image.
QImage orient(const QImage &image, bool flipVertically, bool mirror, QtVideo::Rotation rotation) {
    if (!flipVertically && !mirror && rotation == QtVideo::Rotation::None)
        return image;
    const int w = image.width();
    const int h = image.height();
    const bool swap = rotation == QtVideo::Rotation::Clockwise90 || rotation == QtVideo::Rotation::Clockwise270;
    QImage out(swap ? h : w, swap ? w : h, image.format());
    for (int y = 0; y < h; ++y) {
        const auto *src = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        const int sy = flipVertically ? h - 1 - y : y;
        for (int x = 0; x < w; ++x) {
            const int sx = mirror ? w - 1 - x : x;
            int dx = sx;
            int dy = sy;
            switch (rotation) {
            case QtVideo::Rotation::None:
                break;
            case QtVideo::Rotation::Clockwise90:
                dx = h - 1 - sy;
                dy = sx;
                break;
            case QtVideo::Rotation::Clockwise180:
                dx = w - 1 - sx;
                dy = h - 1 - sy;
                break;
            case QtVideo::Rotation::Clockwise270:
                dx = sy;
                dy = w - 1 - sx;
                break;
            }
            reinterpret_cast<QRgb *>(out.scanLine(dy))[dx] = src[x];
        }
    }
    return out;
}

} // namespace

bool QVideoFramePrivate::layOut(int bytesPerLine, bool keepSize) {
    const int w = format.frameWidth();
    const int h = format.frameHeight();
    const int cw = (w + 1) / 2;
    const int ch = (h + 1) / 2;
    const Format::PixelFormat pf = format.pixelFormat();
    planes = 0;
    offset = {};
    stride = {};
    size = {};
    if (w <= 0 || h <= 0)
        return false;
    if (pf == Format::Format_Jpeg) {
        planes = 1;
        size[0] = int(data.size());
        return !data.isEmpty();
    }
    if (const int bpp = packedBytesPerPixel(pf)) {
        planes = 1;
        stride[0] = bytesPerLine > 0 ? bytesPerLine : w * bpp;
        size[0] = stride[0] * h;
    } else {
        switch (pf) {
        case Format::Format_NV12:
        case Format::Format_NV21:
        case Format::Format_P010:
        case Format::Format_P016: {
            const int sample = pf == Format::Format_NV12 || pf == Format::Format_NV21 ? 1 : 2;
            planes = 2;
            stride[0] = bytesPerLine > 0 ? bytesPerLine : w * sample;
            stride[1] = bytesPerLine > 0 ? bytesPerLine : cw * 2 * sample;
            size[0] = stride[0] * h;
            size[1] = stride[1] * ch;
            break;
        }
        case Format::Format_YUV420P:
        case Format::Format_YV12:
        case Format::Format_YUV422P:
        case Format::Format_YUV420P10: {
            const int sample = pf == Format::Format_YUV420P10 ? 2 : 1;
            const int rows = pf == Format::Format_YUV422P ? h : ch;
            planes = 3;
            stride[0] = bytesPerLine > 0 ? bytesPerLine : w * sample;
            stride[1] = stride[2] = bytesPerLine > 0 ? (bytesPerLine + 1) / 2 : cw * sample;
            size[0] = stride[0] * h;
            size[1] = size[2] = stride[1] * rows;
            break;
        }
        default:
            return false;
        }
    }
    int total = 0;
    for (int i = 0; i < planes; ++i) {
        offset[std::size_t(i)] = total;
        total += size[std::size_t(i)];
    }
    if (keepSize)
        return data.size() >= total;
    data = QByteArray(total, '\0');
    return true;
}

namespace QtMultimediaPrivate {

QVideoFrame makeVideoFrame(const QVideoFrameFormat &format, const QByteArray &data, int bytesPerLine) {
    auto *d = new QVideoFramePrivate;
    d->format = format;
    d->data = data;
    if (!d->layOut(bytesPerLine, true)) {
        delete d;
        return QVideoFrame();
    }
    return QVideoFrame(d);
}

} // namespace QtMultimediaPrivate

/*!
    \class QVideoFrame
    \inmodule NtMultimedia
    \brief The QVideoFrame class holds the pixels of one frame of video.

    A camera delivers frames to a QVideoSink. Map a frame to read its planes, or turn it into a
    QImage:

    \snippet multimedia/camera.cpp frames

    Copies share their pixels; mapping for writing makes a copy of its own first.

    \differences
    Frames are always in memory: there are no GPU textures, so handleType() is always NoHandle and
    there is no \c videoBuffer(). There is no \c paint() and no subtitle text. toImage() gives
    QImage::Format_RGB32, or QImage::Format_ARGB32 (or its premultiplied form) for formats with alpha,
    since Nt's QImage has no other formats; the IMC formats convert to black.
    \enddifferences

    \sa QVideoFrameFormat, QVideoSink, QCamera
*/

/*!
    \enum QVideoFrame::HandleType

    \value NoHandle The frame is in memory.
    \value RhiTextureHandle The frame is a GPU texture. Nt has none.
*/

/*!
    \enum QVideoFrame::MapMode

    How a frame is mapped.

    \value NotMapped Not mapped.
    \value ReadOnly Mapped for reading.
    \value WriteOnly Mapped for writing.
    \value ReadWrite Mapped for reading and writing.
*/

/*!
    \namespace QtVideo
    \inmodule NtMultimedia
    \brief The QtVideo namespace holds the rotation of video frames.

    \differences
    None.
    \enddifferences
*/

/*!
    \enum QtVideo::Rotation

    A clockwise rotation applied when a frame is shown.

    \value None No rotation.
    \value Clockwise90 A quarter turn.
    \value Clockwise180 A half turn.
    \value Clockwise270 Three quarter turns.
*/

/*!
    Constructs an invalid frame.
*/
QVideoFrame::QVideoFrame() noexcept = default;

/*!
    Constructs a frame of \a format with its pixels set to zero. The frame is invalid if \a format
    has no layout in memory.
*/
QVideoFrame::QVideoFrame(const QVideoFrameFormat &format) {
    auto *p = new QVideoFramePrivate;
    p->format = format;
    if (format.pixelFormat() == Format::Format_Jpeg || !p->layOut()) {
        delete p;
        return;
    }
    d = QExplicitlySharedDataPointer<QVideoFramePrivate>(p);
}

/*!
    Constructs a frame with a copy of the pixels of \a image.
*/
QVideoFrame::QVideoFrame(const QImage &image) {
    if (image.isNull())
        return;
    const QImage &source = image;
    const Format::PixelFormat pf = Format::pixelFormatFromImageFormat(image.format());
    if (pf == Format::Format_Invalid)
        return;
    auto *p = new QVideoFramePrivate;
    p->format = QVideoFrameFormat(source.size(), pf);
    p->layOut();
    const int line = source.width() * 4;
    for (int y = 0; y < source.height(); ++y)
        std::memcpy(p->data.data() + qsizetype(y) * line, source.constScanLine(y), std::size_t(line));
    d = QExplicitlySharedDataPointer<QVideoFramePrivate>(p);
}

/*!
    \internal
*/
QVideoFrame::QVideoFrame(QVideoFramePrivate *data) : d(data) {}

/*!
    Constructs a copy of \a other, sharing its pixels.
*/
QVideoFrame::QVideoFrame(const QVideoFrame &other) noexcept = default;

/*!
    Move-constructs a frame from \a other, which becomes invalid.
*/
QVideoFrame::QVideoFrame(QVideoFrame &&other) noexcept = default;

/*!
    Destroys the frame.
*/
QVideoFrame::~QVideoFrame() = default;

/*!
    Makes this frame share the pixels of \a other and returns a reference to it.
*/
QVideoFrame &QVideoFrame::operator=(const QVideoFrame &other) = default;

/*!
    Move-assigns \a other to this frame and returns a reference to it.
*/
QVideoFrame &QVideoFrame::operator=(QVideoFrame &&other) noexcept = default;

/*!
    \fn void QVideoFrame::swap(QVideoFrame &other)

    Swaps this frame with \a other.
*/

/*!
    \fn bool QVideoFrame::operator==(const QVideoFrame &other) const

    Returns \c true if this frame shares its pixels with \a other; otherwise returns \c false.
*/

/*!
    \fn bool QVideoFrame::operator!=(const QVideoFrame &other) const

    Returns \c true if this frame does not share its pixels with \a other; otherwise returns \c false.
*/

/*!
    \fn QVideoFrame::HandleType QVideoFrame::handleType() const

    Returns NoHandle: Nt keeps every frame in memory.
*/

/*!
    Returns \c true if the frame has pixels; otherwise returns \c false.
*/
bool QVideoFrame::isValid() const { return d; }

/*!
    Returns how the pixels are stored.
*/
QVideoFrameFormat::PixelFormat QVideoFrame::pixelFormat() const {
    return d ? d->format.pixelFormat() : Format::Format_Invalid;
}

/*!
    Returns the format of the frame.
*/
QVideoFrameFormat QVideoFrame::surfaceFormat() const { return d ? d->format : QVideoFrameFormat(); }

/*!
    Returns the size of the frame in pixels.
*/
QSize QVideoFrame::size() const { return d ? d->format.frameSize() : QSize(); }

/*!
    Returns the width of the frame in pixels.
*/
int QVideoFrame::width() const { return size().width(); }

/*!
    Returns the height of the frame in pixels.
*/
int QVideoFrame::height() const { return size().height(); }

/*!
    Returns \c true if the frame is mapped; otherwise returns \c false.
*/
bool QVideoFrame::isMapped() const { return d && d->mapMode != NotMapped; }

/*!
    Returns \c true if the frame is mapped for reading; otherwise returns \c false.
*/
bool QVideoFrame::isReadable() const { return d && (d->mapMode & ReadOnly); }

/*!
    Returns \c true if the frame is mapped for writing; otherwise returns \c false.
*/
bool QVideoFrame::isWritable() const { return d && (d->mapMode & WriteOnly); }

/*!
    Returns how the frame is mapped.
*/
QVideoFrame::MapMode QVideoFrame::mapMode() const { return d ? d->mapMode : NotMapped; }

/*!
    Maps the frame in \a mode so that bits() can be used, and returns \c true. Mapping for writing
    gives the frame pixels of its own if it shares them. A frame that is already mapped can be mapped
    again only in the same mode.

    \sa unmap()
*/
bool QVideoFrame::map(MapMode mode) {
    if (!d || mode == NotMapped)
        return false;
    if (d->mapMode != NotMapped)
        return d->mapMode == mode;
    if (mode & WriteOnly)
        d.detach();
    d->mapMode = mode;
    return true;
}

/*!
    Unmaps the frame.
*/
void QVideoFrame::unmap() {
    if (d)
        d->mapMode = NotMapped;
}

/*!
    Returns the number of bytes in a line of \a plane, or 0 if the frame is not mapped. A JPEG frame
    has no lines.
*/
int QVideoFrame::bytesPerLine(int plane) const {
    if (!isMapped() || plane < 0 || plane >= d->planes)
        return 0;
    return d->stride[std::size_t(plane)];
}

/*!
    Returns the pixels of \a plane, or \c nullptr if the frame is not mapped for writing.
*/
uchar *QVideoFrame::bits(int plane) {
    if (!isWritable() || plane < 0 || plane >= d->planes)
        return nullptr;
    return reinterpret_cast<uchar *>(d->data.data()) + d->offset[std::size_t(plane)];
}

/*!
    \overload

    Returns the pixels of \a plane, or \c nullptr if the frame is not mapped.
*/
const uchar *QVideoFrame::bits(int plane) const {
    if (!isMapped() || plane < 0 || plane >= d->planes)
        return nullptr;
    return reinterpret_cast<const uchar *>(d->data.constData()) + d->offset[std::size_t(plane)];
}

/*!
    Returns the size of \a plane in bytes, or 0 if the frame is not mapped.
*/
int QVideoFrame::mappedBytes(int plane) const {
    if (!isMapped() || plane < 0 || plane >= d->planes)
        return 0;
    return d->size[std::size_t(plane)];
}

/*!
    Returns the number of planes, or 0 if the frame is not mapped.
*/
int QVideoFrame::planeCount() const { return isMapped() ? d->planes : 0; }

/*!
    Returns when the frame is shown, in microseconds from the start of the stream, or -1.
*/
qint64 QVideoFrame::startTime() const { return d ? d->startTime : -1; }

/*!
    Sets when the frame is shown to \a time microseconds.
*/
void QVideoFrame::setStartTime(qint64 time) {
    if (d)
        d->startTime = time;
}

/*!
    Returns when the frame stops being shown, in microseconds, or -1.
*/
qint64 QVideoFrame::endTime() const { return d ? d->endTime : -1; }

/*!
    Sets when the frame stops being shown to \a time microseconds.
*/
void QVideoFrame::setEndTime(qint64 time) {
    if (d)
        d->endTime = time;
}

/*!
    Sets the clockwise rotation to apply when the frame is shown to \a angle.
*/
void QVideoFrame::setRotation(QtVideo::Rotation angle) {
    if (d)
        d->rotation = angle;
}

/*!
    Returns the clockwise rotation to apply when the frame is shown.
*/
QtVideo::Rotation QVideoFrame::rotation() const { return d ? d->rotation : QtVideo::Rotation::None; }

/*!
    Sets whether the frame is mirrored left to right when shown to \a mirrored.
*/
void QVideoFrame::setMirrored(bool mirrored) {
    if (d)
        d->mirrored = mirrored;
}

/*!
    Returns \c true if the frame is mirrored left to right when shown; otherwise returns \c false.
*/
bool QVideoFrame::mirrored() const { return d && d->mirrored; }

/*!
    Returns the frame as an image, converted from its pixel format, mirrored and rotated as it is to
    be shown. Returns a null image for an invalid frame, or a JPEG frame when Nt GUI was built without
    JPEG support.
*/
QImage QVideoFrame::toImage() const {
    if (!d)
        return QImage();
    const Format::PixelFormat pf = d->format.pixelFormat();
    QImage image;
    if (pf == Format::Format_Jpeg) {
        image = QImage::fromData(QByteArrayView(d->data), "JPG");
        if (image.isNull())
            return image;
        image = image.convertToFormat(QImage::Format_RGB32);
    } else {
        const QImage::Format target = !hasAlpha(pf)         ? QImage::Format_RGB32
                                      : isPremultiplied(pf) ? QImage::Format_ARGB32_Premultiplied
                                                            : QImage::Format_ARGB32;
        image = QImage(d->format.frameSize(), target);
        const YuvMatrix matrix = matrixFor(d->format);
        for (int y = 0; y < image.height(); ++y)
            convertLine(*d, matrix, y, reinterpret_cast<QRgb *>(image.scanLine(y)));
    }
    const bool mirror = d->mirrored != d->format.isMirrored();
    return orient(image, d->format.scanLineDirection() == Format::BottomToTop, mirror, d->rotation);
}
