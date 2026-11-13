// MIT License - see LICENSE
#include <QtMultimedia/qcameradevice.h>

#include "qvideobackend_p.h"

#include <mutex>

namespace QtMultimediaPrivate {

namespace {

std::mutex &backendMutex() {
    static std::mutex mutex;
    return mutex;
}

std::unique_ptr<VideoBackend> &backendInstance() {
    static std::unique_ptr<VideoBackend> backend;
    return backend;
}

} // namespace

VideoBackend *videoBackend() {
    const std::lock_guard<std::mutex> lock(backendMutex());
    auto &backend = backendInstance();
    if (!backend)
        backend = createPlatformVideoBackend();
    return backend.get();
}

void setVideoBackend(std::unique_ptr<VideoBackend> backend) {
    const std::lock_guard<std::mutex> lock(backendMutex());
    backendInstance() = std::move(backend);
}

QCameraFormat makeCameraFormat(QVideoFrameFormat::PixelFormat format, const QSize &resolution, float minRate,
                               float maxRate) {
    auto *d = new QCameraFormatPrivate;
    d->pixelFormat = format;
    d->resolution = resolution;
    d->minFrameRate = minRate;
    d->maxFrameRate = maxRate;
    return QCameraFormat(d);
}

QCameraDevice makeCameraDevice(const QCameraDevicePrivate &info) {
    return QCameraDevice(new QCameraDevicePrivate(info));
}

} // namespace QtMultimediaPrivate

/*!
    \class QCameraFormat
    \inmodule NtMultimedia
    \brief The QCameraFormat class describes a video format a camera can stream in.

    QCameraDevice::videoFormats() lists them; QCamera::setCameraFormat() chooses one.

    \differences
    None.
    \enddifferences

    \sa QCameraDevice, QCamera
*/

/*!
    Constructs a null format.
*/
QCameraFormat::QCameraFormat() noexcept = default;

/*!
    \internal
*/
QCameraFormat::QCameraFormat(QCameraFormatPrivate *data) : d(data) {}

/*!
    Constructs a copy of \a other.
*/
QCameraFormat::QCameraFormat(const QCameraFormat &other) noexcept = default;

/*!
    Makes this format a copy of \a other and returns a reference to it.
*/
QCameraFormat &QCameraFormat::operator=(const QCameraFormat &other) noexcept = default;

/*!
    Destroys the format.
*/
QCameraFormat::~QCameraFormat() = default;

/*!
    \fn bool QCameraFormat::isNull() const

    Returns \c true if this is a null format; otherwise returns \c false.
*/

/*!
    \fn bool QCameraFormat::operator!=(const QCameraFormat &other) const

    Returns \c true if this format differs from \a other; otherwise returns \c false.
*/

/*!
    Returns the pixel format of the frames.
*/
QVideoFrameFormat::PixelFormat QCameraFormat::pixelFormat() const noexcept {
    return d ? d->pixelFormat : QVideoFrameFormat::Format_Invalid;
}

/*!
    Returns the size of the frames.
*/
QSize QCameraFormat::resolution() const noexcept { return d ? d->resolution : QSize(); }

/*!
    Returns the lowest frame rate the camera streams at in this format.
*/
float QCameraFormat::minFrameRate() const noexcept { return d ? d->minFrameRate : 0.0f; }

/*!
    Returns the highest frame rate the camera streams at in this format.
*/
float QCameraFormat::maxFrameRate() const noexcept { return d ? d->maxFrameRate : 0.0f; }

/*!
    Returns \c true if this format equals \a other; otherwise returns \c false.
*/
bool QCameraFormat::operator==(const QCameraFormat &other) const {
    if (d == other.d)
        return true;
    if (!d || !other.d)
        return false;
    return d->pixelFormat == other.d->pixelFormat && d->resolution == other.d->resolution &&
           d->minFrameRate == other.d->minFrameRate && d->maxFrameRate == other.d->maxFrameRate;
}

/*!
    \class QCameraDevice
    \inmodule NtMultimedia
    \brief The QCameraDevice class describes a camera.

    QMediaDevices::videoInputs() lists the cameras:

    \snippet multimedia/camera.cpp devices

    \differences
    Linux cameras report no position, so position() is UnspecifiedPosition. photoResolutions() lists
    the resolutions of the video formats, since photos are taken from the video stream.
    \enddifferences

    \sa QCamera, QCameraFormat, QMediaDevices
*/

/*!
    \enum QCameraDevice::Position

    Where a camera points.

    \value UnspecifiedPosition Not known.
    \value BackFace Away from the user, on the back of a device.
    \value FrontFace At the user.
*/

/*!
    Constructs a null camera device.
*/
QCameraDevice::QCameraDevice() = default;

/*!
    \internal
*/
QCameraDevice::QCameraDevice(QCameraDevicePrivate *data) : d(data) {}

/*!
    Constructs a copy of \a other.
*/
QCameraDevice::QCameraDevice(const QCameraDevice &other) = default;

/*!
    Makes this device a copy of \a other and returns a reference to it.
*/
QCameraDevice &QCameraDevice::operator=(const QCameraDevice &other) = default;

/*!
    Destroys the device.
*/
QCameraDevice::~QCameraDevice() = default;

/*!
    \fn bool QCameraDevice::operator!=(const QCameraDevice &other) const

    Returns \c true if this is not the same camera as \a other; otherwise returns \c false.
*/

/*!
    Returns \c true if this is the same camera as \a other; otherwise returns \c false.
*/
bool QCameraDevice::operator==(const QCameraDevice &other) const {
    if (d == other.d)
        return true;
    if (!d || !other.d)
        return false;
    return d->id == other.d->id;
}

/*!
    Returns \c true if this is a null device; otherwise returns \c false.
*/
bool QCameraDevice::isNull() const { return !d; }

/*!
    Returns the id of the camera: on Linux its device node, such as \c{/dev/video0}.
*/
QByteArray QCameraDevice::id() const { return d ? d->id : QByteArray(); }

/*!
    Returns a description of the camera for the user.
*/
QString QCameraDevice::description() const { return d ? d->description : QString(); }

/*!
    Returns \c true if this is the default camera; otherwise returns \c false.
*/
bool QCameraDevice::isDefault() const { return d && d->isDefault; }

/*!
    Returns where the camera points.
*/
QCameraDevice::Position QCameraDevice::position() const { return d ? d->position : UnspecifiedPosition; }

/*!
    Returns the resolutions photos can be taken at.
*/
QList<QSize> QCameraDevice::photoResolutions() const { return d ? d->photoResolutions : QList<QSize>(); }

/*!
    Returns the formats the camera can stream in.
*/
QList<QCameraFormat> QCameraDevice::videoFormats() const { return d ? d->videoFormats : QList<QCameraFormat>(); }
