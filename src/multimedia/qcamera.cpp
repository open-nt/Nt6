// MIT License - see LICENSE
#include <QtMultimedia/qcamera.h>
#include <QtMultimedia/qmediadevices.h>

#include "qmediacapture_p.h"

#include <QtCore/qmetaobject.h>

using QtMultimediaPrivate::videoBackend;

namespace {

// The format a camera starts in when none was chosen: the largest picture up to 1080p that keeps
// at least 24 frames a second, preferring uncompressed frames, which need no decoding.
QCameraFormat defaultFormat(const QCameraDevice &device) {
    QCameraFormat best;
    qint64 bestScore = -1;
    for (const QCameraFormat &f : device.videoFormats()) {
        const QSize r = f.resolution();
        if (r.width() > 1920 || r.height() > 1080)
            continue;
        qint64 score = qint64(r.width()) * r.height() * 4;
        if (f.maxFrameRate() < 24.0f)
            score /= 4;
        if (f.pixelFormat() != QVideoFrameFormat::Format_Jpeg)
            score += 1;
        if (score > bestScore) {
            best = f;
            bestScore = score;
        }
    }
    if (best.isNull() && !device.videoFormats().isEmpty())
        best = device.videoFormats().first();
    return best;
}

} // namespace

void QCameraPrivate::setError(QCamera::Error e, const QString &message) {
    error = e;
    errorString = message;
    emit q->errorChanged();
    if (e != QCamera::NoError)
        emit q->errorOccurred(e, message);
}

bool QCameraPrivate::startStream() {
    if (device.isNull()) {
        setError(QCamera::CameraError, QStringLiteral("no camera"));
        return false;
    }
    const QCameraFormat chosen = format.isNull() ? defaultFormat(device) : format;
    const quint64 gen = ++generation;
    QString message;
    stream = videoBackend()->open(
        device, chosen,
        [this, gen](const QVideoFrame &frame) {
            // On the stream's thread: keep the newest frame, and wake the camera's thread once.
            {
                const std::lock_guard<std::mutex> lock(frameMutex);
                latest = frame;
            }
            if (!deliveryPending.exchange(true))
                QMetaObject::invokeMethod(
                    q,
                    [this, gen] {
                        if (gen == generation.load())
                            deliver();
                    },
                    Qt::QueuedConnection);
        },
        [this, gen](const QString &why) {
            QMetaObject::invokeMethod(
                q,
                [this, gen, why] {
                    if (gen != generation.load() || !active)
                        return;
                    stopStream();
                    active = false;
                    emit q->activeChanged(false);
                    notifySession();
                    setError(QCamera::CameraError, why);
                },
                Qt::QueuedConnection);
        },
        &message);
    if (!stream) {
        setError(QCamera::CameraError, message);
        return false;
    }
    return true;
}

void QCameraPrivate::stopStream() {
    ++generation;
    stream.reset(); // joins the stream's thread
    deliveryPending.store(false);
    const std::lock_guard<std::mutex> lock(frameMutex);
    latest = QVideoFrame();
}

void QCameraPrivate::notifySession() {
    if (session)
        session->d->cameraActiveChanged();
}

void QCameraPrivate::deliver() {
    QVideoFrame frame;
    {
        const std::lock_guard<std::mutex> lock(frameMutex);
        frame = std::exchange(latest, QVideoFrame());
        deliveryPending.store(false);
    }
    if (frame.isValid() && session && active)
        session->d->deliverFrame(frame);
}

/*!
    \class QCamera
    \inmodule NtMultimedia
    \brief The QCamera class streams video from a camera.

    Put a camera in a QMediaCaptureSession with a QVideoSink to get its frames, or a QImageCapture to
    take pictures, then start it:

    \snippet multimedia/camera.cpp camera

    Without a format set, the camera streams the largest picture up to 1080p that it can send at
    24 frames a second or more.

    \differences
    There are no focus, zoom, exposure, white balance, flash or torch controls, and no
    \c supportedFeatures(). Frames are delivered on the camera's thread as fast as its event loop
    takes them; when it falls behind, older frames are dropped. On Linux the camera is opened with
    Video4Linux2 and streams YUYV, UYVY, NV12, NV21, YUV420, YVU420, YUV422P, MJPEG, grey and 32-bit
    RGB formats.
    \enddifferences

    \sa QMediaCaptureSession, QVideoSink, QImageCapture, QCameraDevice
*/

/*!
    \enum QCamera::Error

    \value NoError Nothing went wrong.
    \value CameraError The camera could not be opened, or failed while streaming.
*/

/*!
    Constructs a camera on the default camera device, with the given \a parent.
*/
QCamera::QCamera(QObject *parent) : QCamera(QMediaDevices::defaultVideoInput(), parent) {}

/*!
    Constructs a camera on \a cameraDevice, with the given \a parent.
*/
QCamera::QCamera(const QCameraDevice &cameraDevice, QObject *parent)
    : QObject(parent), d(std::make_unique<QCameraPrivate>(this)) {
    d->device = cameraDevice;
}

/*!
    Constructs a camera on the first camera at \a position, or on the default one if none is, with
    the given \a parent.
*/
QCamera::QCamera(QCameraDevice::Position position, QObject *parent)
    : QObject(parent), d(std::make_unique<QCameraPrivate>(this)) {
    d->device = QMediaDevices::defaultVideoInput();
    for (const QCameraDevice &device : QMediaDevices::videoInputs()) {
        if (device.position() == position) {
            d->device = device;
            break;
        }
    }
}

/*!
    Destroys the camera, stopping it.
*/
QCamera::~QCamera() {
    d->stopStream();
    if (d->session)
        d->session->setCamera(nullptr);
}

/*!
    Returns \c true if the camera has a device; otherwise returns \c false.
*/
bool QCamera::isAvailable() const { return !d->device.isNull(); }

/*!
    Returns \c true while the camera streams; otherwise returns \c false.
*/
bool QCamera::isActive() const { return d->active; }

/*!
    Returns the capture session the camera is in, or \c nullptr.
*/
QMediaCaptureSession *QCamera::captureSession() const { return d->session; }

/*!
    Returns the camera device.
*/
QCameraDevice QCamera::cameraDevice() const { return d->device; }

/*!
    Changes the camera device to \a cameraDevice, and the format to none. A camera that is streaming
    goes on with the new device.
*/
void QCamera::setCameraDevice(const QCameraDevice &cameraDevice) {
    if (cameraDevice == d->device)
        return;
    const bool wasActive = d->active;
    if (wasActive)
        setActive(false);
    d->device = cameraDevice;
    const bool hadFormat = !d->format.isNull();
    d->format = QCameraFormat();
    emit cameraDeviceChanged();
    if (hadFormat)
        emit cameraFormatChanged();
    if (wasActive)
        setActive(true);
}

/*!
    Returns the format chosen with setCameraFormat(), or a null format.
*/
QCameraFormat QCamera::cameraFormat() const { return d->format; }

/*!
    Sets the format to stream in to \a format, one of the device's videoFormats(). A camera that is
    streaming goes on in the new format.
*/
void QCamera::setCameraFormat(const QCameraFormat &format) {
    if (format == d->format)
        return;
    d->format = format;
    emit cameraFormatChanged();
    if (d->active) {
        d->stopStream();
        if (!d->startStream()) {
            d->active = false;
            emit activeChanged(false);
            d->notifySession();
        }
    }
}

/*!
    Returns the last error.
*/
QCamera::Error QCamera::error() const { return d->error; }

/*!
    Returns a description of the last error, or an empty string.
*/
QString QCamera::errorString() const { return d->errorString; }

/*!
    Starts streaming if \a active is \c true, or stops. If the camera cannot be started, it stays
    inactive and errorOccurred() is emitted.

    \sa start(), stop()
*/
void QCamera::setActive(bool active) {
    if (active == d->active)
        return;
    if (active) {
        if (!d->startStream())
            return;
        if (d->error != NoError)
            d->setError(NoError, QString());
    } else {
        d->stopStream();
    }
    d->active = active;
    emit activeChanged(active);
    d->notifySession();
}

/*!
    \fn void QCamera::start()

    Starts streaming; the same as setActive(true).
*/

/*!
    \fn void QCamera::stop()

    Stops streaming; the same as setActive(false).
*/

#include "moc_qcamera.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
