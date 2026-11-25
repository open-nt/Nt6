// MIT License - see LICENSE
#include <QtDevice/qfirmwareupdate.h>

#include "qdevice_p.h"

#include <QtCore/qiodevice.h>

using namespace QtDevicePrivate;

/*!
    \class QFirmwareUpdate
    \brief The QFirmwareUpdate class writes a new firmware image and has the boot loader start it.
    \since 6.0

    On Zephyr, QFirmwareUpdate writes an image for the MCUboot boot loader, signed with \c imgtool as
    \c west build makes it (\c zephyr.signed.bin), into the second slot of the flash, and asks
    MCUboot to boot it. MCUboot swaps the slots at the next reset. The new firmware then calls
    confirmRunningImage() once it has checked that it works, for example after it has reached its
    server; if it does not and resets, MCUboot goes back to the old image.

    \code
    QFirmwareUpdate update;
    update.begin(reply->header(QNetworkRequest::ContentLengthHeader).toLongLong());
    QObject::connect(reply, &QIODevice::readyRead, [&] { update.writeFrom(reply); });
    QObject::connect(reply, &QNetworkReply::finished, [&] {
        update.writeFrom(reply);
        if (update.finish() && update.requestUpgrade()) {
            qInfo() << "rebooting into" << update.imageVersion();
            QFirmwareUpdate::reboot();
        }
    });
    \endcode

    and in the new firmware:

    \code
    if (!QFirmwareUpdate::isRunningImageConfirmed() && selfTestPassed())
        QFirmwareUpdate::confirmRunningImage();
    \endcode

    finish() checks that what was written starts with the header of an MCUboot image and is as
    long as the header says, and imageVersion() returns the version from it. The signature is
    checked by MCUboot when it boots the image.

    On Zephyr, set \c CONFIG_BOOTLOADER_MCUBOOT, \c CONFIG_IMG_MANAGER, \c CONFIG_MCUBOOT_IMG_MANAGER,
    \c CONFIG_STREAM_FLASH and \c CONFIG_REBOOT, and build with sysbuild and MCUboot
    (\c{west build --sysbuild}); the target is the partition \c slot1_partition and cannot be
    changed. On Linux the target is a file or a partition, such as \c{/dev/mmcblk0p3}, which
    setTarget() names; requestUpgrade(), confirmRunningImage() and reboot() are left to the system
    there and fail with \c UnsupportedOperationError.

    \differences
    This class is part of Nt only; Qt has no class for firmware updates.
    \enddifferences

    \sa QWatchdog, {Using Nt Device}
*/

/*!
    \enum QFirmwareUpdate::UpdateError

    The errors of a firmware update.

    \value NoError No error occurred.
    \value DeviceNotFoundError The target does not exist.
    \value PermissionError The process may not write the target.
    \value BusyError The target is in use.
    \value OpenError The target could not be opened for another reason.
    \value UnsupportedOperationError The platform cannot do what was asked.
    \value ResourceError The image does not fit, or the system ran out of a resource.
    \value NotOpenError No update was begun.
    \value TimeoutError An operation timed out.
    \value IoError Writing the flash or the file failed.
    \value UnknownError An unknown error occurred.
    \value InvalidImageError What was written is not a complete MCUboot image.
*/

/*!
    \enum QFirmwareUpdate::State

    The stage of an update.

    \value Idle No update was begun, or it was aborted.
    \value Writing begin() succeeded and the image is being written.
    \value Written finish() found a complete image.
    \value Pending requestUpgrade() succeeded: the boot loader starts the image at the next reset.
    \value Failed The update failed; begin() starts again.
*/

class QFirmwareUpdatePrivate {
public:
    explicit QFirmwareUpdatePrivate(QFirmwareUpdate *owner) : q(owner) {}

    bool fail(const Failure &failure, bool toFailed = true) {
        lastError = static_cast<QFirmwareUpdate::UpdateError>(failure.error);
        message = failure.message;
        if (toFailed) {
            if (backend)
                backend->abort();
            backend.reset();
            setState(QFirmwareUpdate::Failed);
        }
        q->errorOccurred(lastError);
        return false;
    }

    bool invalid(const QString &why) {
        lastError = QFirmwareUpdate::InvalidImageError;
        message = why;
        if (backend)
            backend->abort();
        backend.reset();
        setState(QFirmwareUpdate::Failed);
        q->errorOccurred(lastError);
        return false;
    }

    void setState(QFirmwareUpdate::State s) {
        if (state == s)
            return;
        state = s;
        q->stateChanged(s);
    }

    QFirmwareUpdate *q;
    QString target;
    std::unique_ptr<FirmwareBackend> backend;
    QFirmwareUpdate::State state = QFirmwareUpdate::Idle;
    qint64 written = 0;
    qint64 expected = -1;
    QVersionNumber version;
    QFirmwareUpdate::UpdateError lastError = QFirmwareUpdate::NoError;
    QString message;
};

/*!
    Constructs an idle update with the given \a parent.
*/
QFirmwareUpdate::QFirmwareUpdate(QObject *parent)
    : QObject(parent), d(std::make_unique<QFirmwareUpdatePrivate>(this)) {}

/*!
    Destroys the update object, aborting an update that is being written.
*/
QFirmwareUpdate::~QFirmwareUpdate() {
    if (d->backend && d->state == Writing)
        d->backend->abort();
}

/*!
    Sets the target that the image is written to, a file or partition on Linux, to \a target. Zephyr always writes
    to \c slot1_partition and ignores it.
*/
void QFirmwareUpdate::setTarget(const QString &target) { d->target = target; }

/*!
    Returns the target.
*/
QString QFirmwareUpdate::target() const { return d->target; }

/*!
    Starts an update of \a size bytes, or of an unknown size if it is negative, and prepares the target, which on
    Zephyr erases the slot. Returns \c true on success; otherwise sets error() and returns \c false.
*/
bool QFirmwareUpdate::begin(qint64 size) {
    if (d->backend && d->state == Writing)
        d->backend->abort();
    clearError();
    d->written = 0;
    d->expected = size < 0 ? -1 : size;
    d->version = QVersionNumber();
    d->backend = createFirmwareBackend();
    Failure failure;
    if (!d->backend->begin(d->target, d->expected, failure))
        return d->fail(failure);
    d->setState(Writing);
    return true;
}

/*!
    \overload

    Writes the next \a data of the image.
*/
bool QFirmwareUpdate::write(const QByteArray &data) { return write(data.constData(), data.size()); }

/*!
    Writes the next \a size bytes of the image from \a data. Returns \c false, with error() set, if no update is
    being written, the image grows beyond the size given to begin(), or writing fails.
*/
bool QFirmwareUpdate::write(const char *data, qint64 size) {
    if (d->state != Writing || !d->backend)
        return d->fail({QtDevicePrivate::Error::NotOpen, QStringLiteral("No update is being written")}, false);
    if (size <= 0)
        return true;
    if (d->expected >= 0 && d->written + size > d->expected)
        return d->invalid(QStringLiteral("The image is longer than announced"));
    Failure failure;
    if (!d->backend->write(data, static_cast<qsizetype>(size), failure))
        return d->fail(failure);
    d->written += size;
    progress(d->written, d->expected);
    return true;
}

/*!
    Writes everything that \a device has to read now, such as the part of a download that has arrived. Call it again
    when more arrives. Returns \c false if writing fails.
*/
bool QFirmwareUpdate::writeFrom(QIODevice *device) {
    if (!device)
        return false;
    char buffer[4096];
    for (;;) {
        const qint64 n = device->read(buffer, sizeof buffer);
        if (n < 0)
            return d->fail({QtDevicePrivate::Error::Io, device->errorString()});
        if (n == 0)
            return true;
        if (!write(buffer, n))
            return false;
    }
}

/*!
    Ends the writing: flushes the target and checks that it holds a complete MCUboot image. Returns \c true and goes
    to \c Written if it does; otherwise goes to \c Failed, with \c InvalidImageError for an image that is not one.
*/
bool QFirmwareUpdate::finish() {
    if (d->state != Writing || !d->backend)
        return d->fail({QtDevicePrivate::Error::NotOpen, QStringLiteral("No update is being written")}, false);
    if (d->expected >= 0 && d->written != d->expected)
        return d->invalid(QStringLiteral("The image is shorter than announced"));
    QByteArray header;
    Failure failure;
    if (!d->backend->finish(header, failure))
        return d->fail(failure);
    ImageInfo info;
    if (!parseImageHeader(header, info))
        return d->invalid(QStringLiteral("The image has no MCUboot header"));
    if (static_cast<qint64>(info.headerSize) + info.imageSize > d->written)
        return d->invalid(QStringLiteral("The image is shorter than its header says"));
    d->version = info.version;
    d->setState(Written);
    return true;
}

/*!
    Aborts an update and goes back to \c Idle. What was written stays in the slot, but is not booted.
*/
void QFirmwareUpdate::abort() {
    if (d->backend)
        d->backend->abort();
    d->backend.reset();
    d->written = 0;
    d->setState(Idle);
}

/*!
    Asks the boot loader to start the written image at the next reset: once, to be confirmed by the new firmware
    with confirmRunningImage(), or for good if \a permanent is \c true. Returns \c true and goes to \c Pending on
    success. The image is only started after a reset, such as reboot().
*/
bool QFirmwareUpdate::requestUpgrade(bool permanent) {
    if (d->state != Written || !d->backend)
        return d->fail({QtDevicePrivate::Error::NotOpen, QStringLiteral("No complete image was written")}, false);
    Failure failure;
    if (!d->backend->requestUpgrade(permanent, failure))
        return d->fail(failure, false);
    d->setState(Pending);
    return true;
}

/*!
    Returns the stage of the update.
*/
QFirmwareUpdate::State QFirmwareUpdate::state() const { return d->state; }

/*!
    Returns the number of bytes written since begin().
*/
qint64 QFirmwareUpdate::bytesWritten() const { return d->written; }

/*!
    Returns the size given to begin(), or -1.
*/
qint64 QFirmwareUpdate::expectedSize() const { return d->expected; }

/*!
    Returns the version in the header of the written image, once finish() has succeeded.
*/
QVersionNumber QFirmwareUpdate::imageVersion() const { return d->version; }

/*!
    Returns the version of the running firmware from its MCUboot header, or a null version if it has none, as on
    Linux.
*/
QVersionNumber QFirmwareUpdate::runningVersion() {
    ImageInfo info;
    return parseImageHeader(createFirmwareBackend()->runningHeader(), info) ? info.version : QVersionNumber();
}

/*!
    Returns \c true if the running image is confirmed, so that MCUboot keeps it; \c false while it runs on trial
    after an update. On Linux it is always \c true.
*/
bool QFirmwareUpdate::isRunningImageConfirmed() { return createFirmwareBackend()->isConfirmed(); }

/*!
    Confirms the running image, so that MCUboot does not go back to the old one at the next reset. Returns \c true on
    success.
*/
bool QFirmwareUpdate::confirmRunningImage() {
    Failure failure;
    return createFirmwareBackend()->confirm(failure);
}

/*!
    Resets the system, which starts a pending image. Returns \c false if it cannot; on success it does not return.
*/
bool QFirmwareUpdate::reboot() {
    Failure failure;
    return createFirmwareBackend()->reboot(failure);
}

/*!
    Returns the last error.
*/
QFirmwareUpdate::UpdateError QFirmwareUpdate::error() const { return d->lastError; }

/*!
    Returns a description of the last error.
*/
QString QFirmwareUpdate::errorString() const { return d->message; }

/*!
    Sets the error to \c NoError.
*/
void QFirmwareUpdate::clearError() {
    d->lastError = NoError;
    d->message.clear();
}

/*!
    \fn void QFirmwareUpdate::stateChanged(QFirmwareUpdate::State state)

    This signal is emitted when the stage of the update changes to \a state.
*/

/*!
    \fn void QFirmwareUpdate::progress(qint64 bytesWritten, qint64 expectedSize)

    This signal is emitted after each write, with the \a bytesWritten so far and the \a expectedSize, which is -1
    when unknown.
*/

/*!
    \fn void QFirmwareUpdate::errorOccurred(QFirmwareUpdate::UpdateError error)

    This signal is emitted when \a error occurs.
*/

#include "moc_qfirmwareupdate.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
