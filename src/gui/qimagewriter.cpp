// MIT License - see LICENSE
#include <QtGui/qimagewriter.h>

#include "qimagecodecs_p.h"

#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qiodevice.h>

#include <algorithm>

/*!
    \class QImageWriter
    \brief The QImageWriter class writes an image to a file or a device in PNG, BMP or JPEG format.

    Give the writer a file name or an open device and a format, then call write(). With a file name and no format,
    the suffix of the file name decides:

    \code
    QImageWriter writer("snapshot.jpg");
    writer.setQuality(90);
    if (!writer.write(image))
        qWarning() << writer.errorString();
    \endcode

    The image is encoded completely before anything is written, so a failed write leaves an existing file as it was.

    \differences
    The formats are those of QImage::save(): BMP, PNG when Nt GUI is built with zlib and JPEG when it is built with
    libjpeg. There are no plugins, no \c setSubType(), \c setOptimizedWrite(), \c setProgressiveScanWrite(),
    \c setTransformation(), \c setText(), \c supportsOption() or \c imageFormatsForMimeType(). Each write() to a file
    name opens, writes and closes the file, so device() returns only a device set by the caller. canWrite() checks the
    format and the device but does not create the file. JPEG files drop the alpha channel.
    \enddifferences

    \sa QImageReader, QImage::save()
*/

/*!
    \enum QImageWriter::ImageWriterError

    Why the last write() failed.

    \value UnknownError Something else, or nothing has gone wrong yet.
    \value DeviceError The file could not be opened or the device could not be written.
    \value UnsupportedFormatError The format is not one the writer knows.
    \value InvalidImageError The image is null.
*/

struct QImageWriter::Data {
    QIODevice *device = nullptr;
    QString fileName;
    QByteArray format;
    int quality = -1;
    int compression = -1;
    ImageWriterError error = UnknownError;
    QString errorString;

    // The format to write: the one set, or the suffix of the file name.
    QByteArray targetFormat() const {
        if (!format.isEmpty() || device)
            return format;
        return QFileInfo(fileName).suffix().toLower().toLatin1();
    }

    bool fail(ImageWriterError code, const char *message) {
        error = code;
        errorString = QString::fromLatin1(message);
        return false;
    }

    bool checkFormat() {
        if (!NtImageCodecs::writableFormat(targetFormat()))
            return fail(UnsupportedFormatError, "Unsupported image format");
        return true;
    }

    bool checkDevice() {
        if (device ? !device->isWritable() : fileName.isEmpty())
            return fail(DeviceError, "The device is not writable");
        return true;
    }

    // The quality handed to the codec: for PNG an unset quality follows the compression level 0 to 9.
    int effectiveQuality() const {
        if (quality >= 0 || compression < 0 || QByteArray(NtImageCodecs::writableFormat(targetFormat())) != "png")
            return quality;
        return 100 - (std::min(compression, 9) * 100) / 9;
    }
};

/*!
    Constructs a writer with no device, file name or format.
*/
QImageWriter::QImageWriter() : d(std::make_unique<Data>()) {}

/*!
    Constructs a writer that writes to \a device, which must be open for writing, in the given \a format.
*/
QImageWriter::QImageWriter(QIODevice *device, const QByteArray &format) : QImageWriter() {
    d->device = device;
    d->format = format.toLower();
}

/*!
    Constructs a writer that writes to the file \a fileName in the given \a format, or in the format named by the
    suffix of \a fileName if \a format is empty. The file is opened by write().
*/
QImageWriter::QImageWriter(const QString &fileName, const QByteArray &format) : QImageWriter() {
    d->fileName = fileName;
    d->format = format.toLower();
}

/*!
    Destroys the writer. A device set by the caller is left open.
*/
QImageWriter::~QImageWriter() = default;

/*!
    Sets the format to write to \a format: \c "png", \c "bmp", \c "jpg" or \c "jpeg", in any case.

    \sa supportedImageFormats()
*/
void QImageWriter::setFormat(const QByteArray &format) { d->format = format.toLower(); }

/*!
    Returns the format set with setFormat() or the constructor, in lower case, or an empty array if none was set.
*/
QByteArray QImageWriter::format() const { return d->format; }

/*!
    Sets the device to write to to \a device, which must be open for writing, and forgets the file name.
*/
void QImageWriter::setDevice(QIODevice *device) {
    d->device = device;
    d->fileName.clear();
}

/*!
    Returns the device set with setDevice() or the constructor, or \c nullptr if the writer writes to a file name.
*/
QIODevice *QImageWriter::device() const { return d->device; }

/*!
    Sets the file to write to to \a fileName and forgets the device.
*/
void QImageWriter::setFileName(const QString &fileName) {
    d->device = nullptr;
    d->fileName = fileName;
}

/*!
    Returns the file name set with setFileName() or the constructor, or an empty string.
*/
QString QImageWriter::fileName() const { return d->fileName; }

/*!
    Sets the quality of the image to \a quality, from 0 to 100, or -1 for the format's default. For JPEG, 100 is the
    best quality and the largest file; for PNG, 0 is the strongest compression and 100 none. BMP ignores it.

    \sa setCompression()
*/
void QImageWriter::setQuality(int quality) { d->quality = quality; }

/*!
    Returns the quality set with setQuality(); -1 by default.
*/
int QImageWriter::quality() const { return d->quality; }

/*!
    Sets the PNG compression level to \a compression, from 0 (none) to 9 (strongest), or -1 for the default. It is
    used only while quality() is -1. Other formats ignore it.
*/
void QImageWriter::setCompression(int compression) { d->compression = compression; }

/*!
    Returns the compression level set with setCompression(); -1 by default.
*/
int QImageWriter::compression() const { return d->compression; }

/*!
    Returns \c true if the format is one the writer knows and there is a writable device or a file name; otherwise
    sets error() and returns \c false. The file itself is not opened.
*/
bool QImageWriter::canWrite() const { return d->checkFormat() && d->checkDevice(); }

/*!
    Writes \a image and returns \c true if it succeeded; otherwise returns \c false and error() says why. A file name
    is opened and truncated only once the image has been encoded, and closed again before write() returns.
*/
bool QImageWriter::write(const QImage &image) {
    d->error = UnknownError;
    d->errorString.clear();
    if (image.isNull())
        return d->fail(InvalidImageError, "Image is empty");
    if (!d->checkFormat() || !d->checkDevice())
        return false;
    QByteArray bytes;
    if (!NtImageCodecs::encode(image, d->targetFormat(), d->effectiveQuality(), &bytes))
        return d->fail(UnknownError, "Unable to write image data");
    if (d->device)
        return d->device->write(bytes) == bytes.size() || d->fail(DeviceError, "The device could not be written");
    QFile file(d->fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return d->fail(DeviceError, "Cannot open the file for writing");
    return file.write(bytes) == bytes.size() || d->fail(DeviceError, "The file could not be written");
}

/*!
    Returns why the last write() or canWrite() failed, or UnknownError if it did not.
*/
QImageWriter::ImageWriterError QImageWriter::error() const { return d->error; }

/*!
    Returns a sentence about the last error, or an empty string.
*/
QString QImageWriter::errorString() const { return d->errorString; }

/*!
    Returns the formats the writer can write, as lower-case names: \c "bmp", \c "jpeg" and \c "jpg" when Nt GUI is
    built with libjpeg, and \c "png" when it is built with zlib.
*/
QList<QByteArray> QImageWriter::supportedImageFormats() {
    QList<QByteArray> formats;
    for (const char *name : {"bmp", "jpeg", "jpg", "png"}) {
        if (NtImageCodecs::writableFormat(QByteArray(name)))
            formats.append(QByteArray(name));
    }
    return formats;
}

/*!
    Returns the MIME types of the formats in supportedImageFormats(): \c "image/bmp", \c "image/jpeg" and
    \c "image/png".
*/
QList<QByteArray> QImageWriter::supportedMimeTypes() {
    QList<QByteArray> types;
    for (const char *name : {"bmp", "jpeg", "png"}) {
        if (NtImageCodecs::writableFormat(QByteArray(name)))
            types.append(QByteArray("image/") + name);
    }
    return types;
}
