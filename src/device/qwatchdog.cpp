// MIT License - see LICENSE
#include <QtDevice/qwatchdog.h>

#include "qdevice_p.h"

#include <QtCore/qpointer.h>
#include <QtCore/qtimer.h>

using namespace QtDevicePrivate;

/*!
    \class QWatchdog
    \brief The QWatchdog class arms the hardware watchdog, which resets the system when it is not fed in time.
    \since 6.0

    A watchdog is a timer in hardware that resets the system when it runs out. Once start() has armed
    it, the application calls feed() more often than timeout() to show that it still works. With
    autoFeed, the watchdog feeds itself from the event loop, so a loop that hangs, or a slot that
    never returns, resets the device:

    \code
    QWatchdog watchdog;                 // /dev/watchdog on Linux, the alias watchdog0 on Zephyr
    watchdog.setTimeout(5000);
    watchdog.setAutoFeed(true);
    if (watchdog.causedLastReset())
        qWarning() << "recovered from a hang";
    if (!watchdog.start())
        qWarning() << watchdog.errorString();
    \endcode

    On Linux the device is \c{/dev/watchdog}, or \c{/dev/<name>} for another name, which needs
    root or membership of the group that owns it; the timeout is rounded up to whole seconds.
    stop() works where the driver allows the magic close; a driver built with \c nowayout keeps
    running and resets the system unless something else feeds it. On Zephyr the device is the one
    that the devicetree alias \c watchdog0 names, or any device by its device name; set
    \c CONFIG_WATCHDOG. Many microcontrollers cannot stop or reprogram their watchdog once it runs,
    so stop() then fails with \c UnsupportedOperationError, and start() can be called once.

    \differences
    This class is part of Nt only; Qt has no watchdog class.
    \enddifferences

    \sa QFirmwareUpdate
*/

/*!
    \enum QWatchdog::WatchdogError

    The errors of a watchdog.

    \value NoError No error occurred.
    \value DeviceNotFoundError The watchdog device does not exist.
    \value PermissionError The process may not open the device.
    \value BusyError Another process has the device open.
    \value OpenError The device could not be opened for another reason.
    \value UnsupportedOperationError The hardware or driver cannot do what was asked, such as stopping.
    \value ResourceError The system ran out of a resource, or the device went away.
    \value NotOpenError The watchdog is not active.
    \value TimeoutError An operation timed out.
    \value IoError The device reported an I/O error.
    \value UnknownError An unknown error occurred.
*/

/*!
    \enum QWatchdog::Option

    Options that Zephyr passes to the watchdog driver; Linux ignores them.

    \value NoOption No option.
    \value PauseInDebugger The watchdog pauses while a debugger halts the CPU.
    \value PauseWhileSleeping The watchdog pauses while the CPU sleeps.
*/

class QWatchdogPrivate {
public:
    explicit QWatchdogPrivate(QWatchdog *owner) : q(owner) {
        feedTimer.setTimerType(Qt::PreciseTimer);
        QObject::connect(&feedTimer, &QTimer::timeout, q, [this] { q->feed(); });
    }

    bool fail(const Failure &failure) {
        lastError = static_cast<QWatchdog::WatchdogError>(failure.error);
        message = failure.message;
        q->errorOccurred(lastError);
        return false;
    }

    void updateFeedTimer() {
        if (active && autoFeed && config.timeout > 0)
            feedTimer.start(qMax(1, config.timeout / 3)); // three chances before the watchdog runs out
        else
            feedTimer.stop();
    }

    QWatchdog *q;
    WatchdogConfig config;
    QWatchdog::Options options;
    std::unique_ptr<WatchdogBackend> backend;
    QTimer feedTimer;
    bool active = false;
    bool autoFeed = false;
    QWatchdog::WatchdogError lastError = QWatchdog::NoError;
    QString message;
};

/*!
    Constructs a watchdog with the given \a parent, for the default device and a timeout of 10 seconds.
*/
QWatchdog::QWatchdog(QObject *parent) : QObject(parent), d(std::make_unique<QWatchdogPrivate>(this)) {
    d->config.timeout = 10000;
}

/*!
    Constructs a watchdog for the device \a name with the given \a parent.
*/
QWatchdog::QWatchdog(const QString &name, QObject *parent) : QWatchdog(parent) { d->config.name = name; }

/*!
    Destroys the watchdog object. An active watchdog is left running, as stop() might reset the system or fail;
    call stop() first to disarm it.
*/
QWatchdog::~QWatchdog() = default;

/*!
    Sets the device to \a name: on Linux a device in \c{/dev}, such as \c watchdog1, and on Zephyr an alias or a
    device name. An empty name, the default, is \c watchdog on Linux and \c watchdog0 on Zephyr. It takes effect at
    the next start().
*/
void QWatchdog::setDeviceName(const QString &name) { d->config.name = name; }

/*!
    Returns the name of the device.
*/
QString QWatchdog::deviceName() const { return d->config.name; }

/*!
    Sets the timeout to \a msecs milliseconds, the time after which the watchdog resets the system unless it is fed.
    It takes effect at the next start().

    \sa timeout()
*/
void QWatchdog::setTimeout(int msecs) { d->config.timeout = qMax(1, msecs); }

/*!
    Returns the timeout in milliseconds. While the watchdog is active it is the one the hardware uses, which can be
    longer than the one that was set.
*/
int QWatchdog::timeout() const { return d->config.timeout; }

/*!
    Sets the options of the watchdog to \a options. They take effect at the next start().
*/
void QWatchdog::setOptions(Options options) {
    d->options = options;
    d->config.pauseInDebugger = options.testFlag(PauseInDebugger);
    d->config.pauseWhileSleeping = options.testFlag(PauseWhileSleeping);
}

/*!
    Returns the options of the watchdog.
*/
QWatchdog::Options QWatchdog::options() const { return d->options; }

/*!
    Sets whether the watchdog feeds itself, from a timer of the event loop at a third of the timeout, to \a enable.

    \sa autoFeed()
*/
void QWatchdog::setAutoFeed(bool enable) {
    d->autoFeed = enable;
    d->updateFeedTimer();
}

/*!
    Returns whether the watchdog feeds itself from the event loop.

    \sa setAutoFeed()
*/
bool QWatchdog::autoFeed() const { return d->autoFeed; }

/*!
    Opens and arms the watchdog. Returns \c true on success; otherwise sets error() and returns \c false.
*/
bool QWatchdog::start() {
    if (d->active)
        return true;
    Failure failure;
    d->backend = createWatchdogBackend();
    if (!d->backend->start(d->config, failure)) {
        d->backend.reset();
        return d->fail(failure);
    }
    d->active = true;
    d->updateFeedTimer();
    activeChanged(true);
    return true;
}

/*!
    Feeds the watchdog, which starts its timeout again. Returns \c false, with error() set, if the watchdog is not
    active or feeding failed.
*/
bool QWatchdog::feed() {
    if (!d->active)
        return d->fail({QtDevicePrivate::Error::NotOpen, QStringLiteral("The watchdog is not active")});
    Failure failure;
    return d->backend->feed(failure) || d->fail(failure);
}

/*!
    Disarms the watchdog. Returns \c false, with error() set, if the hardware cannot be stopped; the watchdog then
    stays active and has to be fed.
*/
bool QWatchdog::stop() {
    if (!d->active)
        return true;
    Failure failure;
    if (!d->backend->stop(failure))
        return d->fail(failure);
    d->backend.reset();
    d->active = false;
    d->updateFeedTimer();
    activeChanged(false);
    return true;
}

/*!
    Returns \c true if the watchdog is armed.
*/
bool QWatchdog::isActive() const { return d->active; }

/*!
    Returns \c true if the watchdog reset the system the last time, as the driver (Linux) or the reset cause of the
    hardware (Zephyr, with \c CONFIG_HWINFO) tells; \c false if it did not, or that is not known.
*/
bool QWatchdog::causedLastReset() const {
    if (d->backend)
        return d->backend->causedLastReset(d->config.name);
    return createWatchdogBackend()->causedLastReset(d->config.name);
}

/*!
    Returns the last error.
*/
QWatchdog::WatchdogError QWatchdog::error() const { return d->lastError; }

/*!
    Returns a description of the last error.
*/
QString QWatchdog::errorString() const { return d->message; }

/*!
    Sets the error to \c NoError.
*/
void QWatchdog::clearError() {
    d->lastError = NoError;
    d->message.clear();
}

/*!
    \fn void QWatchdog::activeChanged(bool active)

    This signal is emitted when the watchdog is armed or disarmed; \a active tells which.
*/

/*!
    \fn void QWatchdog::errorOccurred(QWatchdog::WatchdogError error)

    This signal is emitted when \a error occurs.
*/

#include "moc_qwatchdog.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
