// MIT License - see LICENSE
#include <QtGui/qimagereader.h>

#include "qimagecodecs_p.h"

#include <QtCore/qfile.h>
#include <QtCore/qiodevice.h>

/*!
    \class QImageReader
    \brief The QImageReader class reads an image from a file or a device, with its format and size known first.

    A reader opens the file or device once and keeps what it read, so the format, the size and the image itself can
    be asked for in any order:

    \code
    QImageReader reader(":/images/map.png");
    if (reader.canRead()) {
        const QSize size = reader.size();
        QImage image = reader.read();
    }
    \endcode

    \differences
    The reader decodes the whole image the first time size() or imageFormat() is called, as Nt's codecs have no
    header-only path. There is no \c setClipRect(), \c setScaledClipRect(), \c setBackgroundColor(),
    \c setQuality(), \c setDecideFormatFromContent(), image sequences (\c jumpToNextImage(), \c imageCount(),
    \c loopCount()), text keys, \c subType() or \c transformation(); scaledSize() scales the decoded image
    with QImage::scaled(), and autoTransform() is remembered but has no EXIF data to act on. The formats are those
    of QImage::load(): PNG, BMP and, when Nt GUI is built with libjpeg, JPEG.
    \enddifferences

    \sa QImage::load()
*/

/*!
    \enum QImageReader::ImageReaderError

    Why the last read() failed.

    \value UnknownError Something else, or nothing has gone wrong yet.
    \value FileNotFoundError The file could not be opened.
    \value DeviceError The device could not be read.
    \value UnsupportedFormatError The data is not in a format the reader knows.
    \value InvalidDataError The data is in a known format but is damaged.
*/

struct QImageReader::Data {
    QIODevice *device = nullptr;
    std::unique_ptr<QFile> file; // when the reader opened the file itself
    QString fileName;
    QByteArray format;
    QSize scaledSize;
    bool autoTransform = false;
    bool loaded = false;
    QByteArray bytes;
    QImage image;
    ImageReaderError error = UnknownError;
    QString errorString;

    void reset() {
        loaded = false;
        bytes.clear();
        image = QImage();
        error = UnknownError;
        errorString.clear();
    }

    // Reads the device and decodes it, once.
    bool load() {
        if (loaded)
            return !image.isNull();
        loaded = true;
        if (!device && !fileName.isEmpty()) {
            file = std::make_unique<QFile>(fileName);
            if (!file->open(QIODevice::ReadOnly)) {
                error = FileNotFoundError;
                errorString = QString::fromLatin1("Cannot open ") + fileName;
                file.reset();
                return false;
            }
            device = file.get();
        }
        if (!device || !device->isReadable()) {
            error = DeviceError;
            errorString = QString::fromLatin1("The device is not readable");
            return false;
        }
        bytes = device->readAll();
        const auto *data = reinterpret_cast<const uchar *>(bytes.constData());
        const char *sniffed = NtImageCodecs::formatOf(data, static_cast<std::size_t>(bytes.size()));
        if (!sniffed) {
            error = UnsupportedFormatError;
            errorString = QString::fromLatin1("Unsupported image format");
            return false;
        }
        if (format.isEmpty())
            format = sniffed;
        image = NtImageCodecs::decodeAny(data, static_cast<std::size_t>(bytes.size()));
        if (image.isNull()) {
            error = InvalidDataError;
            errorString = QString::fromLatin1("Unable to read image data");
            return false;
        }
        return true;
    }
};

/*!
    Constructs a reader with no device or file name.
*/
QImageReader::QImageReader() : d(std::make_unique<Data>()) {}

/*!
    Constructs a reader for \a device, which must be open for reading. The \a format is remembered and returned by
    format() when the data cannot be sniffed; the reader decides from the data otherwise.
*/
QImageReader::QImageReader(QIODevice *device, const QByteArray &format) : QImageReader() {
    d->device = device;
    d->format = format.toLower();
}

/*!
    Constructs a reader for the file \a fileName (a resource path works too), with \a format as in the device
    constructor. The file is opened on the first read.
*/
QImageReader::QImageReader(const QString &fileName, const QByteArray &format) : QImageReader() {
    d->fileName = fileName;
    d->format = format.toLower();
}

/*!
    Destroys the reader, closing a file it opened itself.
*/
QImageReader::~QImageReader() = default;

/*!
    Sets the format the reader expects to \a format. The data decides what is read; the format is what format()
    returns until the data is seen.
*/
void QImageReader::setFormat(const QByteArray &format) { d->format = format.toLower(); }

/*!
    Returns the format of the image: the one set with setFormat(), or the one sniffed from the data
    (\c "png", \c "jpeg" or \c "bmp"), or empty if the data is not an image the reader knows.
*/
QByteArray QImageReader::format() const {
    if (d->format.isEmpty())
        d->load();
    return d->format;
}

/*!
    Sets the device to read from to \a device, which must be open for reading, and forgets the file name and anything
    read so far.
*/
void QImageReader::setDevice(QIODevice *device) {
    d->file.reset();
    d->fileName.clear();
    d->device = device;
    d->reset();
}

/*!
    Returns the device the reader reads from, or \c nullptr if none was set and no file has been opened yet.
*/
QIODevice *QImageReader::device() const { return d->device; }

/*!
    Sets the file to read to \a fileName and forgets the device and anything read so far.
*/
void QImageReader::setFileName(const QString &fileName) {
    d->file.reset();
    d->device = nullptr;
    d->fileName = fileName;
    d->reset();
}

/*!
    Returns the file name set with setFileName() or the constructor, or an empty string.
*/
QString QImageReader::fileName() const { return d->fileName; }

/*!
    Returns the size of the image, or an invalid size if it cannot be read.
*/
QSize QImageReader::size() const { return d->load() ? d->image.size() : QSize(); }

/*!
    Returns the QImage::Format the image is decoded to, or QImage::Format_Invalid if it cannot be read.
*/
QImage::Format QImageReader::imageFormat() const { return d->load() ? d->image.format() : QImage::Format_Invalid; }

/*!
    Sets the size that read() scales the image to, to \a size. An invalid size, the default, leaves the image as it is.
*/
void QImageReader::setScaledSize(const QSize &size) { d->scaledSize = size; }

/*!
    Returns the size set with setScaledSize().
*/
QSize QImageReader::scaledSize() const { return d->scaledSize; }

/*!
    Sets whether read() applies the orientation stored in the image to \a enabled. Nt reads no orientation data,
    so the value is only remembered.
*/
void QImageReader::setAutoTransform(bool enabled) { d->autoTransform = enabled; }

/*!
    Returns the value set with setAutoTransform(); \c false by default.
*/
bool QImageReader::autoTransform() const { return d->autoTransform; }

/*!
    Returns \c true if the data is an image the reader can decode.
*/
bool QImageReader::canRead() const { return d->load(); }

/*!
    Reads the image and returns it, scaled to scaledSize() if that is valid, or a null image if it cannot be read;
    error() then says why.
*/
QImage QImageReader::read() {
    if (!d->load())
        return QImage();
    if (d->scaledSize.isValid() && d->scaledSize != d->image.size())
        return d->image.scaled(d->scaledSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    return d->image;
}

/*!
    \overload

    Reads the image into \a image and returns \c true if it succeeded.
*/
bool QImageReader::read(QImage *image) {
    const QImage result = read();
    if (image)
        *image = result;
    return !result.isNull();
}

/*!
    Returns why the last read failed, or UnknownError if it did not.
*/
QImageReader::ImageReaderError QImageReader::error() const { return d->error; }

/*!
    Returns a sentence about the last error, or an empty string.
*/
QString QImageReader::errorString() const { return d->errorString; }

/*!
    Returns the format of the image in the file \a fileName, sniffed from its first bytes, or an empty array if the
    file cannot be opened or is not an image the reader knows.
*/
QByteArray QImageReader::imageFormat(const QString &fileName) {
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
        return QByteArray();
    return imageFormat(&file);
}

/*!
    \overload

    Returns the format of the image that \a device holds, from the bytes at its current position, which is left where
    it was if the device supports seeking.
*/
QByteArray QImageReader::imageFormat(QIODevice *device) {
    if (!device || !device->isReadable())
        return QByteArray();
    const QByteArray head = device->peek(16);
    const char *sniffed = NtImageCodecs::formatOf(reinterpret_cast<const uchar *>(head.constData()),
                                                  static_cast<std::size_t>(head.size()));
    return sniffed ? QByteArray(sniffed) : QByteArray();
}

/*!
    Returns the formats the reader can decode, as lower-case names: \c "bmp", \c "png", and \c "jpeg" and \c "jpg"
    when Nt GUI is built with libjpeg.
*/
QList<QByteArray> QImageReader::supportedImageFormats() {
    QList<QByteArray> formats;
    formats.append(QByteArray("bmp"));
#ifdef NT_HAVE_JPEG
    formats.append(QByteArray("jpeg"));
    formats.append(QByteArray("jpg"));
#endif
    formats.append(QByteArray("png"));
    return formats;
}
