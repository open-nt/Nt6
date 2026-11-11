// MIT License - see LICENSE
#include <QtGui/qimage.h>
#include <QtGui/qpaintengine.h>

#include "qimagecodecs_p.h"

#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qiodevice.h>

#include <cstring>

namespace {

enum class Kind { Unknown, Png, Bmp, Jpeg };

Kind sniff(const uchar *data, std::size_t size) {
    if (size >= 8 && data[0] == 0x89 && std::memcmp(data + 1, "PNG", 3) == 0)
        return Kind::Png;
    if (size >= 3 && data[0] == 0xff && data[1] == 0xd8 && data[2] == 0xff)
        return Kind::Jpeg;
    if (size >= 2 && data[0] == 'B' && data[1] == 'M')
        return Kind::Bmp;
    return Kind::Unknown;
}

Kind kindOfName(const QString &name) {
    const QString lower = name.toLower();
    if (lower == QLatin1String("png"))
        return Kind::Png;
    if (lower == QLatin1String("bmp"))
        return Kind::Bmp;
    if (lower == QLatin1String("jpg") || lower == QLatin1String("jpeg"))
        return Kind::Jpeg;
    return Kind::Unknown;
}

QImage decode(const uchar *data, std::size_t size) {
    switch (sniff(data, size)) {
    case Kind::Png:
        return NtImageCodecs::readPng(data, size);
    case Kind::Bmp:
        return NtImageCodecs::readBmp(data, size);
    case Kind::Jpeg:
        return NtImageCodecs::readJpeg(data, size);
    default:
        return QImage();
    }
}

bool encode(const QImage &image, Kind kind, int quality, QByteArray *out) {
    switch (kind) {
    case Kind::Png:
        return NtImageCodecs::writePng(image, out, quality);
    case Kind::Bmp:
        return NtImageCodecs::writeBmp(image, out);
    case Kind::Jpeg:
        return NtImageCodecs::writeJpeg(image, out, quality);
    default:
        return false;
    }
}

} // namespace

/*!
    Constructs an image from the file \a fileName. If the file cannot be read or is not an image that Nt knows, the
    image is null.

    The \a format is accepted for compatibility with Qt and is ignored: the format is found from the contents of the
    file.

    \sa load(), isNull()
*/
QImage::QImage(const QString &fileName, const char *format) { load(fileName, format); }

/*!
    Loads the image in the file \a fileName and returns \c true if it succeeded; otherwise returns \c false and leaves
    the image null. The formats are PNG, BMP and JPEG. The format is found from the contents of the file, so the
    \a format is ignored.

    PNG files of every color type, bit depth and interlacing load, with transparency. BMP files of 1, 4, 8, 16, 24 and
    32 bits without compression load. JPEG files, including progressive and CMYK ones, load as opaque images. Images
    without transparency load as Format_RGB32 and the others as Format_ARGB32.

    \sa save(), loadFromData()
*/
bool QImage::load(const QString &fileName, const char *format) {
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        *this = QImage();
        return false;
    }
    return load(&file, format);
}

/*!
    \overload

    Loads the image from \a device, which must be open for reading, and reads it to its end.
*/
bool QImage::load(QIODevice *device, const char *format) {
    if (!device || !device->isReadable()) {
        *this = QImage();
        return false;
    }
    const QByteArray bytes = device->readAll();
    return loadFromData(QByteArrayView(bytes), format);
}

/*!
    Loads the image from the \a len bytes at \a data and returns \c true if it succeeded. The \a format is ignored, as
    in load().
*/
bool QImage::loadFromData(const uchar *data, int len, const char *format) {
    return loadFromData(QByteArrayView(reinterpret_cast<const char *>(data), len), format);
}

/*!
    \overload

    Loads the image from \a data.
*/
bool QImage::loadFromData(QByteArrayView data, const char *format) {
    (void)format;
    QImage image = decode(reinterpret_cast<const uchar *>(data.constData()), static_cast<std::size_t>(data.size()));
    *this = image;
    return !image.isNull();
}

/*!
    Returns an image loaded from the \a size bytes at \a data, or a null image if they are not a supported image. The
    \a format is ignored.

    \sa loadFromData()
*/
QImage QImage::fromData(const uchar *data, int size, const char *format) {
    QImage image;
    image.loadFromData(data, size, format);
    return image;
}

/*!
    \overload

    Returns an image loaded from \a data.
*/
QImage QImage::fromData(QByteArrayView data, const char *format) {
    QImage image;
    image.loadFromData(data, format);
    return image;
}

/*!
    Saves the image to the file \a fileName and returns \c true if it succeeded. The \a format is \c "PNG", \c "BMP",
    \c "JPG" or \c "JPEG", in any case; if it is \c nullptr, the suffix of the file name decides.

    The \a quality is between 0 and 100, or -1 for the default. For JPEG, 100 is the best quality and the largest file.
    For PNG, 0 is the strongest compression and 100 stores the pixels without it. BMP ignores it.

    PNG and BMP files keep the alpha channel. JPEG files cannot, and the transparency is dropped.

    \sa load()
*/
bool QImage::save(const QString &fileName, const char *format, int quality) const {
    const Kind kind = kindOfName(format ? QString::fromLatin1(format) : QFileInfo(fileName).suffix());
    QByteArray bytes;
    if (isNull() || !encode(*this, kind, quality, &bytes))
        return false;
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    return file.write(bytes) == bytes.size();
}

/*!
    \overload

    Writes the image to \a device, which must be open for writing. The \a format must be given.
*/
bool QImage::save(QIODevice *device, const char *format, int quality) const {
    if (!device || !device->isWritable() || !format)
        return false;
    QByteArray bytes;
    if (isNull() || !encode(*this, kindOfName(QString::fromLatin1(format)), quality, &bytes))
        return false;
    return device->write(bytes) == bytes.size();
}
