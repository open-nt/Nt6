// MIT License - see LICENSE
#include "qperipheral_p.h"

#include <QtCore/qtimer.h>
#include <QtGpio/qanaloginput.h>

using QtPrivate::HwError;
using QtPrivate::HwFailure;

class QAnalogInputPrivate {
public:
    explicit QAnalogInputPrivate(QAnalogInput *owner) : q(owner) {}

    void setError(HwError e, const QString &message) {
        error = static_cast<QAnalogInput::AnalogError>(e);
        errorString = message;
        if (e != HwError::NoError)
            emit q->errorOccurred(error);
    }

    QAnalogInput *q;
    QtPrivate::AnalogConfig config;
    int sampleInterval = 0;
    int raw = 0;
    double volts = 0;
    bool hasVolts = false;
    std::unique_ptr<QtPrivate::AnalogInputBackend> backend;
    QTimer sampleTimer;
    QAnalogInput::AnalogError error = QAnalogInput::NoError;
    QString errorString;
};

/*!
    \class QAnalogInput
    \inmodule NtGpio
    \brief The QAnalogInput class reads one channel of an analog to digital converter.
    \since 6.0

    A QAnalogInput samples a voltage, such as that of a battery, a potentiometer or a sensor,
    on request with read(), or every sampleInterval() milliseconds from the event loop:

    \code
    QAnalogInput pot(QStringLiteral("iio:device0:1"));
    pot.setSampleInterval(100);
    QObject::connect(&pot, &QAnalogInput::valueChanged, [&](int raw) {
        qDebug() << raw << pot.voltage();
    });
    if (!pot.open())
        qWarning() << pot.errorString();
    \endcode

    rawValue() is the number the converter returned; voltage() is that number in volts, where
    the scale is known. See \l{Using PWM, Analog I/O, I2C and SPI} for how channels are named and
    how the scale is found on each platform.

    \differences
    Qt has no ADC class; QAnalogInput is an Nt addition. It works on Linux, through the sysfs
    files of Industrial I/O, and on Zephyr, through \c adc_read(). Samples are read one at a time;
    the buffered, triggered mode of IIO and the sequences of Zephyr are not used. On Zephyr the
    channel always has a gain of 1 and the internal reference.
    \enddifferences

    \sa QAnalogOutput
*/

/*!
    \enum QAnalogInput::AnalogError

    This enum describes the errors a QAnalogInput can report.

    \value NoError No error occurred.
    \value DeviceNotFoundError The converter or the channel does not exist, or the name is not
        valid.
    \value PermissionError The user may not read the channel.
    \value BusyError The converter is busy.
    \value OpenError The channel is already open in this object.
    \value UnsupportedOperationError The converter cannot be set up as asked, such as the
        resolution, or the platform has no ADC support.
    \value ResourceError The converter went away while the channel was open. The channel has
        been closed.
    \value NotOpenError The operation needs the channel to be open.
    \value TimeoutError The conversion did not finish in time.
    \value IoError The converter reported an I/O error.
    \value UnknownError Any other error.
*/

/*!
    Constructs an input object with the given \a parent. Set the channel with setChannelName()
    before open().
*/
QAnalogInput::QAnalogInput(QObject *parent) : QObject(parent), d(std::make_unique<QAnalogInputPrivate>(this)) {
    QObject::connect(&d->sampleTimer, &QTimer::timeout, this, [this] { read(); });
}

/*!
    Constructs an input object for the channel called \a name, with the given \a parent. The
    channel is not opened.
*/
QAnalogInput::QAnalogInput(const QString &name, QObject *parent) : QAnalogInput(parent) { d->config.name = name; }

/*!
    Destroys the object, closing the channel first.
*/
QAnalogInput::~QAnalogInput() { close(); }

/*!
    Sets the channel this object opens to \a name, such as \c iio:device0:2. It takes effect
    at the next open().
*/
void QAnalogInput::setChannelName(const QString &name) { d->config.name = name; }

/*!
    Returns the name of the channel.

    \sa setChannelName()
*/
QString QAnalogInput::channelName() const { return d->config.name; }

/*!
    Sets the resolution of the conversion to \a bits, from 1 to 24. The default is 12. It takes
    effect at the next open(). Zephyr uses it to set up the channel; Linux takes the resolution
    from the driver and ignores it.
*/
void QAnalogInput::setResolution(int bits) { d->config.resolution = bits < 1 ? 1 : (bits > 24 ? 24 : bits); }

/*!
    Returns the resolution in bits.

    \sa setResolution()
*/
int QAnalogInput::resolution() const { return d->config.resolution; }

/*!
    Sets the reference voltage of the converter to \a volts. Zephyr uses it to convert to volts
    when the driver does not report a reference; Linux ignores it. The default, 0, means unknown.
    It takes effect at the next open().

    \sa voltage()
*/
void QAnalogInput::setReferenceVoltage(double volts) { d->config.referenceVolts = volts > 0 ? volts : 0; }

/*!
    Returns the reference voltage, or 0 if none was set.

    \sa setReferenceVoltage()
*/
double QAnalogInput::referenceVoltage() const { return d->config.referenceVolts; }

/*!
    Sets the input to be read every \a msecs milliseconds while it is open. The default, 0,
    reads only when read() is called. It takes effect at once.

    \sa valueChanged()
*/
void QAnalogInput::setSampleInterval(int msecs) {
    d->sampleInterval = msecs > 0 ? msecs : 0;
    if (d->sampleInterval > 0 && d->backend)
        d->sampleTimer.start(d->sampleInterval);
    else
        d->sampleTimer.stop();
}

/*!
    Returns the sample interval in milliseconds, or 0 if the input is only read on request.

    \sa setSampleInterval()
*/
int QAnalogInput::sampleInterval() const { return d->sampleInterval; }

/*!
    Opens the channel and takes a first sample, so rawValue() is valid when it returns. Returns
    \c true on success; otherwise returns \c false and sets error().

    \sa close(), read()
*/
bool QAnalogInput::open() {
    if (d->backend) {
        d->setError(HwError::Open, QStringLiteral("The analog input is already open"));
        return false;
    }
    if (d->config.name.isEmpty()) {
        d->setError(HwError::DeviceNotFound, QStringLiteral("No analog input channel name was set"));
        return false;
    }
    std::unique_ptr<QtPrivate::AnalogInputBackend> backend = QtPrivate::createAnalogInputBackend();
    HwFailure failure;
    if (!backend->open(d->config, failure)) {
        d->setError(failure.error, failure.message);
        return false;
    }
    d->backend = std::move(backend);
    d->error = NoError;
    d->errorString.clear();
    if (!read())
        return false;
    if (d->backend && d->sampleInterval > 0)
        d->sampleTimer.start(d->sampleInterval);
    return d->backend != nullptr;
}

/*!
    Closes the channel and stops sampling. Does nothing if the channel is not open.
*/
void QAnalogInput::close() {
    d->sampleTimer.stop();
    d->backend.reset();
}

/*!
    Returns \c true if the channel is open; otherwise returns \c false.
*/
bool QAnalogInput::isOpen() const { return d->backend != nullptr; }

/*!
    Takes one sample now. Returns \c true on success, after emitting valueChanged() if the
    sample differs from the one before; otherwise returns \c false and sets error().

    \sa rawValue(), voltage()
*/
bool QAnalogInput::read() {
    if (!d->backend) {
        d->setError(HwError::NotOpen, QStringLiteral("The analog input is not open"));
        return false;
    }
    qint32 raw = 0;
    double volts = 0;
    bool hasVolts = false;
    HwFailure failure;
    if (!d->backend->read(raw, volts, hasVolts, failure)) {
        if (failure.error == HwError::Resource)
            close();
        d->setError(failure.error, failure.message);
        return false;
    }
    const bool changed = raw != d->raw;
    d->raw = raw;
    d->volts = hasVolts ? volts : 0.0;
    d->hasVolts = hasVolts;
    if (changed)
        emit valueChanged(raw);
    return true;
}

/*!
    Returns the last sample as the converter returned it.

    \sa voltage(), read()
*/
int QAnalogInput::rawValue() const { return d->raw; }

/*!
    Returns the last sample in volts, or 0 if hasVoltage() is \c false.

    \sa rawValue()
*/
double QAnalogInput::voltage() const { return d->volts; }

/*!
    Returns \c true if the last sample could be converted to volts; otherwise returns \c false.
    On Zephyr, a driver that reports no reference voltage needs setReferenceVoltage().
*/
bool QAnalogInput::hasVoltage() const { return d->hasVolts; }

/*!
    Returns the last error.

    \sa errorString(), clearError()
*/
QAnalogInput::AnalogError QAnalogInput::error() const { return d->error; }

/*!
    Returns a description of the last error, or an empty string if there is none.
*/
QString QAnalogInput::errorString() const { return d->errorString; }

/*!
    Sets error() to \c NoError and clears errorString().
*/
void QAnalogInput::clearError() {
    d->error = NoError;
    d->errorString.clear();
}

#include "moc_qanaloginput.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
