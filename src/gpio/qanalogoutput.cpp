// MIT License - see LICENSE
#include "qperipheral_p.h"

#include <QtGpio/qanalogoutput.h>

using QtPrivate::HwError;
using QtPrivate::HwFailure;

class QAnalogOutputPrivate {
public:
    explicit QAnalogOutputPrivate(QAnalogOutput *owner) : q(owner) {}

    void setError(HwError e, const QString &message) {
        error = static_cast<QAnalogOutput::AnalogError>(e);
        errorString = message;
        if (e != HwError::NoError)
            emit q->errorOccurred(error);
    }

    bool write(qint32 value) {
        if (backend) {
            HwFailure failure;
            if (!backend->write(value, failure)) {
                if (failure.error == HwError::Resource)
                    q->close();
                setError(failure.error, failure.message);
                return false;
            }
        }
        raw = value;
        return true;
    }

    QAnalogOutput *q;
    QtPrivate::AnalogConfig config;
    int raw = 0;
    std::unique_ptr<QtPrivate::AnalogOutputBackend> backend;
    QAnalogOutput::AnalogError error = QAnalogOutput::NoError;
    QString errorString;
};

/*!
    \class QAnalogOutput
    \inmodule NtGpio
    \brief The QAnalogOutput class drives one channel of a digital to analog converter.
    \since 6.0

    A QAnalogOutput sets a voltage, such as a bias, a reference or a control input:

    \code
    QAnalogOutput level(QStringLiteral("iio:device1:0"));
    if (level.open())
        level.setVoltage(0.8);
    \endcode

    setRawValue() writes the number the converter takes; setVoltage() works it out from the
    scale of the channel. See \l{Using PWM, Analog I/O, I2C and SPI} for how channels are named.

    \differences
    Qt has no DAC class; QAnalogOutput is an Nt addition. It works on Linux, through the sysfs
    files of Industrial I/O, and on Zephyr, through \c dac_write_value(). Waveforms and the
    buffered mode of IIO are not supported.
    \enddifferences

    \sa QAnalogInput
*/

/*!
    \enum QAnalogOutput::AnalogError

    This enum describes the errors a QAnalogOutput can report.

    \value NoError No error occurred.
    \value DeviceNotFoundError The converter or the channel does not exist, or the name is not
        valid.
    \value PermissionError The user may not write the channel.
    \value BusyError The converter is busy.
    \value OpenError The channel is already open in this object.
    \value UnsupportedOperationError The converter cannot do what was asked, such as the
        resolution or a voltage when the scale is unknown, or the platform has no DAC support.
    \value ResourceError The converter went away while the channel was open. The channel has
        been closed.
    \value NotOpenError The operation needs the channel to be open.
    \value TimeoutError The converter did not answer in time.
    \value IoError The converter reported an I/O error.
    \value UnknownError Any other error.
*/

/*!
    Constructs an output object with the given \a parent. Set the channel with
    setChannelName() before open().
*/
QAnalogOutput::QAnalogOutput(QObject *parent) : QObject(parent), d(std::make_unique<QAnalogOutputPrivate>(this)) {}

/*!
    Constructs an output object for the channel called \a name, with the given \a parent. The
    channel is not opened.
*/
QAnalogOutput::QAnalogOutput(const QString &name, QObject *parent) : QAnalogOutput(parent) { d->config.name = name; }

/*!
    Destroys the object, closing the channel first. The output keeps its last value.
*/
QAnalogOutput::~QAnalogOutput() { close(); }

/*!
    Sets the channel this object opens to \a name, such as \c iio:device1:0. It takes effect
    at the next open().
*/
void QAnalogOutput::setChannelName(const QString &name) { d->config.name = name; }

/*!
    Returns the name of the channel.

    \sa setChannelName()
*/
QString QAnalogOutput::channelName() const { return d->config.name; }

/*!
    Sets the resolution of the converter to \a bits, from 1 to 24. The default is 12. It takes
    effect at the next open(). Zephyr uses it to set up the channel; Linux ignores it.
*/
void QAnalogOutput::setResolution(int bits) { d->config.resolution = bits < 1 ? 1 : (bits > 24 ? 24 : bits); }

/*!
    Returns the resolution in bits.

    \sa setResolution()
*/
int QAnalogOutput::resolution() const { return d->config.resolution; }

/*!
    Sets the reference voltage of the converter to \a volts, which Zephyr needs for setVoltage();
    Linux takes the scale from the driver and ignores it. It takes effect at the next open().
*/
void QAnalogOutput::setReferenceVoltage(double volts) { d->config.referenceVolts = volts > 0 ? volts : 0; }

/*!
    Returns the reference voltage, or 0 if none was set.

    \sa setReferenceVoltage()
*/
double QAnalogOutput::referenceVoltage() const { return d->config.referenceVolts; }

/*!
    Opens the channel and writes rawValue() to it. Returns \c true on success; otherwise returns
    \c false and sets error().

    \sa close()
*/
bool QAnalogOutput::open() {
    if (d->backend) {
        d->setError(HwError::Open, QStringLiteral("The analog output is already open"));
        return false;
    }
    if (d->config.name.isEmpty()) {
        d->setError(HwError::DeviceNotFound, QStringLiteral("No analog output channel name was set"));
        return false;
    }
    std::unique_ptr<QtPrivate::AnalogOutputBackend> backend = QtPrivate::createAnalogOutputBackend();
    HwFailure failure;
    if (!backend->open(d->config, failure) || !backend->write(d->raw, failure)) {
        d->setError(failure.error, failure.message);
        return false;
    }
    d->backend = std::move(backend);
    d->error = NoError;
    d->errorString.clear();
    return true;
}

/*!
    Closes the channel. The output keeps its last value. Does nothing if the channel is not
    open.
*/
void QAnalogOutput::close() { d->backend.reset(); }

/*!
    Returns \c true if the channel is open; otherwise returns \c false.
*/
bool QAnalogOutput::isOpen() const { return d->backend != nullptr; }

/*!
    Writes \a value, as the converter takes it, to the channel. Returns \c true on success;
    otherwise returns \c false and sets error(). On a closed channel, \a value is kept and
    written by the next open().

    \sa setVoltage()
*/
bool QAnalogOutput::setRawValue(int value) {
    if (value < 0) {
        d->setError(HwError::Unsupported, QStringLiteral("An analog output value cannot be negative"));
        return false;
    }
    return d->write(value);
}

/*!
    Returns the value last written.

    \sa setRawValue()
*/
int QAnalogOutput::rawValue() const { return d->raw; }

/*!
    Sets the output to \a volts, rounded to the nearest step of the converter. Returns \c true on
    success; otherwise returns \c false and sets error(). The channel must be open, since the
    scale comes from it.

    \sa setRawValue(), setReferenceVoltage()
*/
bool QAnalogOutput::setVoltage(double volts) {
    if (!d->backend) {
        d->setError(HwError::NotOpen, QStringLiteral("The analog output is not open"));
        return false;
    }
    qint32 raw = 0;
    if (!(volts >= 0) || !d->backend->rawForVolts(volts, raw)) {
        d->setError(HwError::Unsupported, QStringLiteral("The analog output cannot be set to %1 V").arg(volts));
        return false;
    }
    return d->write(raw);
}

/*!
    Returns the last error.

    \sa errorString(), clearError()
*/
QAnalogOutput::AnalogError QAnalogOutput::error() const { return d->error; }

/*!
    Returns a description of the last error, or an empty string if there is none.
*/
QString QAnalogOutput::errorString() const { return d->errorString; }

/*!
    Sets error() to \c NoError and clears errorString().
*/
void QAnalogOutput::clearError() {
    d->error = NoError;
    d->errorString.clear();
}

#include "moc_qanalogoutput.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
