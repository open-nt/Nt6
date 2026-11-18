// MIT License - see LICENSE
#include "qperipheral_p.h"

#include <QtGpio/qpwmchannel.h>

#include <cmath>

using QtPrivate::HwError;
using QtPrivate::HwFailure;

class QPwmChannelPrivate {
public:
    explicit QPwmChannelPrivate(QPwmChannel *owner) : q(owner) {}

    void setError(HwError e, const QString &message) {
        error = static_cast<QPwmChannel::PwmError>(e);
        errorString = message;
        if (e != HwError::NoError)
            emit q->errorOccurred(error);
    }

    // Applies next to an open channel and keeps it, or keeps the settings that were in force.
    bool change(const QtPrivate::PwmSettings &next, double nextDuty) {
        if (backend) {
            HwFailure failure;
            if (!backend->apply(next, failure)) {
                if (failure.error == HwError::Resource)
                    q->close();
                setError(failure.error, failure.message);
                return false;
            }
        }
        settings = next;
        duty = nextDuty;
        return true;
    }

    static qint64 pulseFor(qint64 period, double duty) {
        return static_cast<qint64>(std::llround(static_cast<double>(period) * duty));
    }

    QPwmChannel *q;
    QString name;
    QtPrivate::PwmSettings settings;
    double duty = 0; // what pulseWidth is kept at when the period changes
    std::unique_ptr<QtPrivate::PwmBackend> backend;
    QPwmChannel::PwmError error = QPwmChannel::NoError;
    QString errorString;
};

/*!
    \class QPwmChannel
    \inmodule NtGpio
    \brief The QPwmChannel class drives one pulse-width modulation output.
    \since 6.0

    A PWM channel switches its output on for pulseWidth() nanoseconds out of every period(),
    to dim an LED or a backlight, set the speed of a fan or a motor, or position a servo:

    \code
    QPwmChannel servo(QStringLiteral("pwmchip0:1"));
    servo.setPeriod(20000000);    // 50 Hz
    servo.setPulseWidth(1500000); // centre
    servo.setEnabled(true);
    if (!servo.open())
        qWarning() << servo.errorString();
    \endcode

    Settings made before open() are applied by it; settings made while the channel is open
    take effect at once, and are kept only if the hardware accepts them. Changing the period
    keeps the duty cycle, so setFrequency() can follow setDutyCycle(). See
    \l{Using PWM, Analog I/O, I2C and SPI} for how channels are named.

    \differences
    Qt has no PWM class; QPwmChannel is an Nt addition. It works on Linux, through
    \c /sys/class/pwm, and on Zephyr, through \c pwm_set(). There is no capture mode. On Zephyr
    there is no separate enable: a disabled channel is driven with a pulse width of 0.
    \enddifferences

    \sa QGpioLine
*/

/*!
    \enum QPwmChannel::Polarity

    This enum describes which level the pulse is.

    \value Normal The output is high for the pulse width and low for the rest of the period.
    \value Inversed The output is low for the pulse width and high for the rest of the period.
*/

/*!
    \enum QPwmChannel::PwmError

    This enum describes the errors a QPwmChannel can report.

    \value NoError No error occurred.
    \value DeviceNotFoundError The PWM chip or channel does not exist, or the name is not valid.
    \value PermissionError The user may not use the channel.
    \value BusyError Another program or a driver holds the channel.
    \value OpenError The channel is already open in this object.
    \value UnsupportedOperationError The hardware cannot do what was asked, such as the period or
        the polarity, or the platform has no PWM support.
    \value ResourceError The device went away while the channel was open. The channel has been
        closed.
    \value NotOpenError The operation needs the channel to be open.
    \value TimeoutError The channel did not become ready in time.
    \value IoError The hardware reported an I/O error.
    \value UnknownError Any other error.
*/

/*!
    Constructs a channel object with the given \a parent. Set the channel with
    setChannelName() before open().
*/
QPwmChannel::QPwmChannel(QObject *parent) : QObject(parent), d(std::make_unique<QPwmChannelPrivate>(this)) {}

/*!
    Constructs a channel object for the channel called \a name, with the given \a parent. The
    channel is not opened.
*/
QPwmChannel::QPwmChannel(const QString &name, QObject *parent) : QPwmChannel(parent) { d->name = name; }

/*!
    Destroys the object, closing the channel first.
*/
QPwmChannel::~QPwmChannel() { close(); }

/*!
    Sets the channel this object opens to \a name, such as \c pwmchip0:1. It takes effect at
    the next open().
*/
void QPwmChannel::setChannelName(const QString &name) { d->name = name; }

/*!
    Returns the name of the channel.

    \sa setChannelName()
*/
QString QPwmChannel::channelName() const { return d->name; }

/*!
    Sets the period to \a nsecs nanoseconds, keeping the duty cycle. Returns \c true on success;
    returns \c false if \a nsecs is negative or the open channel does not accept it.

    \sa setFrequency(), setPulseWidth()
*/
bool QPwmChannel::setPeriod(qint64 nsecs) {
    if (nsecs < 0) {
        d->setError(HwError::Unsupported, QStringLiteral("A PWM period cannot be negative"));
        return false;
    }
    QtPrivate::PwmSettings next = d->settings;
    next.period = nsecs;
    next.pulseWidth = QPwmChannelPrivate::pulseFor(nsecs, d->duty);
    return d->change(next, d->duty);
}

/*!
    Returns the period in nanoseconds. The default is 0.

    \sa setPeriod()
*/
qint64 QPwmChannel::period() const { return d->settings.period; }

/*!
    Sets the period to that of \a hertz cycles a second, rounded to the nanosecond. Returns
    \c true on success; otherwise returns \c false.

    \sa setPeriod()
*/
bool QPwmChannel::setFrequency(double hertz) {
    if (!(hertz > 0) || hertz > 1e9) {
        d->setError(HwError::Unsupported, QStringLiteral("A PWM frequency must be between 0 and 1 GHz"));
        return false;
    }
    return setPeriod(static_cast<qint64>(std::llround(1e9 / hertz)));
}

/*!
    Returns the frequency in hertz, or 0 if no period is set.

    \sa setFrequency()
*/
double QPwmChannel::frequency() const {
    return d->settings.period > 0 ? 1e9 / static_cast<double>(d->settings.period) : 0.0;
}

/*!
    Sets the pulse width to \a nsecs nanoseconds, at most the period. Returns \c true on success;
    otherwise returns \c false.

    \sa setDutyCycle()
*/
bool QPwmChannel::setPulseWidth(qint64 nsecs) {
    if (nsecs < 0 || nsecs > d->settings.period) {
        d->setError(HwError::Unsupported, QStringLiteral("A PWM pulse width must be between 0 and the period"));
        return false;
    }
    QtPrivate::PwmSettings next = d->settings;
    next.pulseWidth = nsecs;
    const double duty =
        d->settings.period > 0 ? static_cast<double>(nsecs) / static_cast<double>(d->settings.period) : 0.0;
    return d->change(next, duty);
}

/*!
    Returns the pulse width in nanoseconds.

    \sa setPulseWidth()
*/
qint64 QPwmChannel::pulseWidth() const { return d->settings.pulseWidth; }

/*!
    Sets the pulse width to \a fraction of the period, from 0 to 1. The fraction is kept when
    the period changes, so it may be set before the period. Returns \c true on success;
    otherwise returns \c false.

    \sa setPulseWidth()
*/
bool QPwmChannel::setDutyCycle(double fraction) {
    if (!(fraction >= 0 && fraction <= 1)) {
        d->setError(HwError::Unsupported, QStringLiteral("A PWM duty cycle must be between 0 and 1"));
        return false;
    }
    QtPrivate::PwmSettings next = d->settings;
    next.pulseWidth = QPwmChannelPrivate::pulseFor(next.period, fraction);
    return d->change(next, fraction);
}

/*!
    Returns the pulse width as a fraction of the period.

    \sa setDutyCycle()
*/
double QPwmChannel::dutyCycle() const { return d->duty; }

/*!
    Sets the level of the pulse to \a polarity. Returns \c true on success; otherwise returns
    \c false, for example when the hardware cannot invert its output.
*/
bool QPwmChannel::setPolarity(Polarity polarity) {
    QtPrivate::PwmSettings next = d->settings;
    next.inversed = polarity == Inversed;
    return d->change(next, d->duty);
}

/*!
    Returns the polarity. The default is \c Normal.

    \sa setPolarity()
*/
QPwmChannel::Polarity QPwmChannel::polarity() const { return d->settings.inversed ? Inversed : Normal; }

/*!
    Turns the output on if \a enabled is \c true, and off otherwise. Returns \c true on success;
    otherwise returns \c false. A new channel is disabled.
*/
bool QPwmChannel::setEnabled(bool enabled) {
    QtPrivate::PwmSettings next = d->settings;
    next.enabled = enabled;
    return d->change(next, d->duty);
}

/*!
    Returns \c true if the output is on; otherwise returns \c false.

    \sa setEnabled()
*/
bool QPwmChannel::isEnabled() const { return d->settings.enabled; }

/*!
    Requests the channel and programs it with the settings made so far. Returns \c true on
    success; otherwise returns \c false and sets error().

    \sa close()
*/
bool QPwmChannel::open() {
    if (d->backend) {
        d->setError(HwError::Open, QStringLiteral("The PWM channel is already open"));
        return false;
    }
    if (d->name.isEmpty()) {
        d->setError(HwError::DeviceNotFound, QStringLiteral("No PWM channel name was set"));
        return false;
    }
    std::unique_ptr<QtPrivate::PwmBackend> backend = QtPrivate::createPwmBackend();
    HwFailure failure;
    if (!backend->open(d->name, failure) || !backend->apply(d->settings, failure)) {
        d->setError(failure.error, failure.message);
        return false;
    }
    d->backend = std::move(backend);
    d->error = NoError;
    d->errorString.clear();
    return true;
}

/*!
    Releases the channel. On Zephyr the output keeps running; on Linux it stops if open()
    exported the channel. Does nothing if the channel is not open.
*/
void QPwmChannel::close() { d->backend.reset(); }

/*!
    Returns \c true if the channel is open; otherwise returns \c false.
*/
bool QPwmChannel::isOpen() const { return d->backend != nullptr; }

/*!
    Returns the last error.

    \sa errorString(), clearError()
*/
QPwmChannel::PwmError QPwmChannel::error() const { return d->error; }

/*!
    Returns a description of the last error, or an empty string if there is none.
*/
QString QPwmChannel::errorString() const { return d->errorString; }

/*!
    Sets error() to \c NoError and clears errorString().
*/
void QPwmChannel::clearError() {
    d->error = NoError;
    d->errorString.clear();
}

#include "moc_qpwmchannel.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
