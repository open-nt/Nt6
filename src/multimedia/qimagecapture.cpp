// MIT License - see LICENSE
#include <QtMultimedia/qimagecapture.h>

#include "qmediacapture_p.h"

#include <QtCore/qdir.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qstandardpaths.h>
#include <QtGui/qrgb.h>

#include <algorithm>
#include <cmath>

namespace {

// Scales image to size, bilinearly.
QImage scaledTo(const QImage &image, const QSize &size) {
    if (size.isEmpty() || size == image.size())
        return image;
    QImage out(size, image.format());
    const double sx = double(image.width()) / size.width();
    const double sy = double(image.height()) / size.height();
    for (int y = 0; y < size.height(); ++y) {
        const double fy = std::max(0.0, (y + 0.5) * sy - 0.5);
        const int y0 = std::min(int(fy), image.height() - 1);
        const int y1 = std::min(y0 + 1, image.height() - 1);
        const double ty = fy - y0;
        const auto *a = reinterpret_cast<const QRgb *>(image.constScanLine(y0));
        const auto *b = reinterpret_cast<const QRgb *>(image.constScanLine(y1));
        auto *dst = reinterpret_cast<QRgb *>(out.scanLine(y));
        for (int x = 0; x < size.width(); ++x) {
            const double fx = std::max(0.0, (x + 0.5) * sx - 0.5);
            const int x0 = std::min(int(fx), image.width() - 1);
            const int x1 = std::min(x0 + 1, image.width() - 1);
            const double tx = fx - x0;
            auto mix = [&](int (*channel)(QRgb)) {
                const double top = channel(a[x0]) * (1 - tx) + channel(a[x1]) * tx;
                const double bottom = channel(b[x0]) * (1 - tx) + channel(b[x1]) * tx;
                return int(std::lround(top * (1 - ty) + bottom * ty));
            };
            dst[x] = qRgba(mix(qRed), mix(qGreen), mix(qBlue), mix(qAlpha));
        }
    }
    return out;
}

const char *writerFormat(QImageCapture::FileFormat format) {
    switch (format) {
    case QImageCapture::UnspecifiedFormat:
    case QImageCapture::JPEG:
        return "JPG";
    case QImageCapture::PNG:
        return "PNG";
    case QImageCapture::WebP:
    case QImageCapture::Tiff:
        break;
    }
    return nullptr;
}

int qualityPercent(QImageCapture::Quality quality) {
    static const int percent[] = {25, 50, 75, 90, 98};
    return percent[std::clamp(int(quality), 0, 4)];
}

} // namespace

void QImageCapturePrivate::setError(int id, QImageCapture::Error e, const QString &message) {
    error = e;
    errorString = message;
    emit q->errorChanged();
    emit q->errorOccurred(id, e, message);
}

void QImageCapturePrivate::updateReady() {
    const bool now = session && session->camera() && session->camera()->isActive();
    if (now == ready)
        return;
    ready = now;
    emit q->readyForCaptureChanged(now);
}

int QImageCapturePrivate::enqueue(bool toFile, const QString &location) {
    if (!ready) {
        setError(-1, QImageCapture::NotReadyError, QStringLiteral("the camera is not ready"));
        return -1;
    }
    QString fileName;
    if (toFile) {
        const char *format = writerFormat(fileFormat);
        if (!format) {
            setError(-1, QImageCapture::FormatError, QStringLiteral("only JPEG and PNG files can be written"));
            return -1;
        }
        const QString suffix = fileFormat == QImageCapture::PNG ? QStringLiteral("png") : QStringLiteral("jpg");
        QString dir;
        if (location.isEmpty()) {
            dir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
            if (dir.isEmpty())
                dir = QDir::currentPath();
        } else if (QFileInfo(location).isDir()) {
            dir = location;
        } else {
            fileName = location;
            if (QFileInfo(fileName).suffix().isEmpty())
                fileName += QLatin1Char('.') + suffix;
        }
        if (fileName.isEmpty()) {
            for (int n = 1; n < 100000; ++n) {
                const QString candidate =
                    QDir(dir).filePath(QStringLiteral("image_%1.%2").arg(n, 4, 10, QLatin1Char('0')).arg(suffix));
                if (!QFileInfo::exists(candidate)) {
                    fileName = candidate;
                    break;
                }
            }
        }
    }
    const int id = nextId++;
    requests.append({id, toFile, fileName});
    return id;
}

void QImageCapturePrivate::onFrame(const QVideoFrame &frame) {
    if (requests.isEmpty())
        return;
    const QList<Request> taken = std::exchange(requests, {});
    QImage image;
    for (const Request &r : taken) {
        emit q->imageExposed(r.id);
        if (image.isNull()) {
            image = scaledTo(frame.toImage(), resolution);
            if (image.isNull()) {
                setError(r.id, QImageCapture::ResourceError, QStringLiteral("the frame cannot be converted"));
                continue;
            }
        }
        emit q->imageCaptured(r.id, image);
        emit q->imageAvailable(r.id, frame);
        if (!r.toFile)
            continue;
        QDir().mkpath(QFileInfo(r.fileName).absolutePath());
        if (image.save(r.fileName, writerFormat(fileFormat), qualityPercent(quality)))
            emit q->imageSaved(r.id, r.fileName);
        else
            setError(r.id, QImageCapture::ResourceError, QStringLiteral("cannot write %1").arg(r.fileName));
    }
}

/*!
    \class QImageCapture
    \inmodule NtMultimedia
    \brief The QImageCapture class takes pictures from a camera.

    Put it in a QMediaCaptureSession with a QCamera. Once the camera streams, isReadyForCapture() is
    \c true, and capture() takes the next frame as a QImage, or captureToFile() saves it:

    \snippet multimedia/camera.cpp capture

    \differences
    Pictures are frames of the video stream, at its resolution, scaled to resolution() if one is
    set. Files can be written as JPEG (when Nt GUI was built with JPEG support) or PNG, not WebP or
    TIFF. There is no metadata.
    \enddifferences

    \sa QCamera, QMediaCaptureSession
*/

/*!
    \enum QImageCapture::Error

    \value NoError Nothing went wrong.
    \value NotReadyError The camera is not streaming.
    \value ResourceError The picture could not be converted or written.
    \value OutOfSpaceError There is no room for the file. Nt reports ResourceError instead.
    \value NotSupportedFeatureError Not supported. Nt does not report this.
    \value FormatError The file format cannot be written.
*/

/*!
    \enum QImageCapture::Quality

    The quality of JPEG files.

    \value VeryLowQuality 25%.
    \value LowQuality 50%.
    \value NormalQuality 75%, the default.
    \value HighQuality 90%.
    \value VeryHighQuality 98%.
*/

/*!
    \enum QImageCapture::FileFormat

    \value UnspecifiedFormat The default, JPEG.
    \value JPEG JPEG.
    \value PNG PNG.
    \value WebP WebP. Nt cannot write it.
    \value Tiff TIFF. Nt cannot write it.
    \omitvalue LastFileFormat
*/

/*!
    Constructs an image capture with the given \a parent.
*/
QImageCapture::QImageCapture(QObject *parent) : QObject(parent), d(std::make_unique<QImageCapturePrivate>(this)) {}

/*!
    Destroys the image capture, taking it out of its session.
*/
QImageCapture::~QImageCapture() {
    if (d->session)
        d->session->setImageCapture(nullptr);
}

/*!
    Returns \c true if the capture is in a session with a camera; otherwise returns \c false.
*/
bool QImageCapture::isAvailable() const { return d->session && d->session->camera(); }

/*!
    Returns the session the capture is in, or \c nullptr.
*/
QMediaCaptureSession *QImageCapture::captureSession() const { return d->session; }

/*!
    Returns the last error.
*/
QImageCapture::Error QImageCapture::error() const { return d->error; }

/*!
    Returns a description of the last error, or an empty string.
*/
QString QImageCapture::errorString() const { return d->errorString; }

/*!
    Returns \c true while the session's camera streams; otherwise returns \c false.
*/
bool QImageCapture::isReadyForCapture() const { return d->ready; }

/*!
    Returns the format of the files captureToFile() writes.
*/
QImageCapture::FileFormat QImageCapture::fileFormat() const { return d->fileFormat; }

/*!
    Sets the format of the files captureToFile() writes to \a format.
*/
void QImageCapture::setFileFormat(FileFormat format) {
    if (format == d->fileFormat)
        return;
    d->fileFormat = format;
    emit fileFormatChanged();
}

/*!
    Returns the file formats captureToFile() can write.
*/
QList<QImageCapture::FileFormat> QImageCapture::supportedFormats() { return {JPEG, PNG}; }

/*!
    Returns the name of the file format \a c, such as \c JPEG.
*/
QString QImageCapture::fileFormatName(FileFormat c) {
    switch (c) {
    case UnspecifiedFormat:
        return QStringLiteral("Unspecified image format");
    case JPEG:
        return QStringLiteral("JPEG");
    case PNG:
        return QStringLiteral("PNG");
    case WebP:
        return QStringLiteral("WebP");
    case Tiff:
        return QStringLiteral("Tiff");
    }
    return QString();
}

/*!
    Returns a description of the file format \a c.
*/
QString QImageCapture::fileFormatDescription(FileFormat c) {
    switch (c) {
    case UnspecifiedFormat:
        return QStringLiteral("Unspecified image format");
    case JPEG:
        return QStringLiteral("JPEG");
    case PNG:
        return QStringLiteral("PNG");
    case WebP:
        return QStringLiteral("WebP");
    case Tiff:
        return QStringLiteral("TIFF");
    }
    return QString();
}

/*!
    Returns the size pictures are scaled to, or an empty size for the size of the frames.
*/
QSize QImageCapture::resolution() const { return d->resolution; }

/*!
    Sets the size pictures are scaled to to \a resolution; an empty size keeps the size of the frames.
*/
void QImageCapture::setResolution(const QSize &resolution) {
    if (resolution == d->resolution)
        return;
    d->resolution = resolution;
    emit resolutionChanged();
}

/*!
    \fn void QImageCapture::setResolution(int width, int height)

    \overload

    Sets the size pictures are scaled to to \a width by \a height.
*/

/*!
    Returns the quality of JPEG files.
*/
QImageCapture::Quality QImageCapture::quality() const { return d->quality; }

/*!
    Sets the quality of JPEG files to \a quality.
*/
void QImageCapture::setQuality(Quality quality) {
    if (quality == d->quality)
        return;
    d->quality = quality;
    emit qualityChanged();
}

/*!
    Takes the next frame and saves it to \a location, and returns the id of the capture, or -1 if the
    camera is not ready. An empty \a location saves in the pictures folder, and a folder saves in
    it, as \c{image_0001.jpg} and onwards; a file name without a suffix gets one. imageSaved() reports
    the file.
*/
int QImageCapture::captureToFile(const QString &location) { return d->enqueue(true, location); }

/*!
    Takes the next frame, and returns the id of the capture, or -1 if the camera is not ready.
    imageCaptured() delivers it as a QImage, and imageAvailable() as a QVideoFrame.
*/
int QImageCapture::capture() { return d->enqueue(false, QString()); }

#include "moc_qimagecapture.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
