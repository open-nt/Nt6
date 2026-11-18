// MIT License - see LICENSE
#include "qperipheral_p.h"

#include <QtGpio/qspidevice.h>

using QtPrivate::HwError;
using QtPrivate::HwFailure;

class QSpiDevicePrivate {
public:
    explicit QSpiDevicePrivate(QSpiDevice *owner) : q(owner) {}

    void setError(HwError e, const QString &message) {
        error = static_cast<QSpiDevice::SpiError>(e);
        errorString = message;
        if (e != HwError::NoError)
            emit q->errorOccurred(error);
    }

    void fail(const HwFailure &failure) {
        if (failure.error == HwError::Resource)
            q->close();
        setError(failure.error, failure.message);
    }

    // Applies next to an open device and keeps it, or keeps the configuration in force.
    bool change(const QtPrivate::SpiConfig &next) {
        if (backend) {
            HwFailure failure;
            if (!backend->configure(next, failure)) {
                fail(failure);
                return false;
            }
        }
        config = next;
        return true;
    }

    bool transfer(const QByteArray &data, qsizetype size, bool duplex, QByteArray &read) {
        if (!backend) {
            setError(HwError::NotOpen, QStringLiteral("The SPI device is not open"));
            return false;
        }
        if (size < 0 || (data.isEmpty() && size == 0)) {
            setError(HwError::Unsupported, QStringLiteral("An SPI transfer must move at least one byte"));
            return false;
        }
        HwFailure failure;
        if (!backend->transfer(data, size, duplex, read, failure)) {
            fail(failure);
            read.clear();
            return false;
        }
        return true;
    }

    QSpiDevice *q;
    QtPrivate::SpiConfig config;
    std::unique_ptr<QtPrivate::SpiBackend> backend;
    QSpiDevice::SpiError error = QSpiDevice::NoError;
    QString errorString;
};

/*!
    \class QSpiDevice
    \inmodule NtGpio
    \brief The QSpiDevice class talks to one device on an SPI bus.
    \since 6.0

    A QSpiDevice is a bus and the chip select of a device on it, such as a flash memory, a
    display controller or a converter. Each call is one transaction with chip select held:

    \code
    QSpiDevice adc(QStringLiteral("spidev0.0"));
    adc.setMode(QSpiDevice::Mode0);
    adc.setSpeed(1000000);
    if (!adc.open())
        qFatal("%s", qPrintable(adc.errorString()));
    const QByteArray in = adc.transfer(QByteArray("\x01\x80\x00", 3)); // MCP3008, channel 0
    \endcode

    transfer() clocks bytes out and in at once; write(), read() and writeRead() use one
    direction at a time. See \l{Using PWM, Analog I/O, I2C and SPI} for how devices are named.

    \differences
    Qt has no SPI class; QSpiDevice is an Nt addition. It works on Linux, through
    \c /dev/spidevB.C, and on Zephyr, through \c spi_transceive(). Only the controller (master)
    side is supported, with one data line each way. On Zephyr, chip select is the native one of
    the controller; chip selects on GPIO pins are not used. Transfers are synchronous.
    \enddifferences

    \sa QI2cDevice
*/

/*!
    \enum QSpiDevice::Mode

    This enum describes the clock polarity (CPOL) and phase (CPHA) of the bus.

    \value Mode0 The clock idles low; data is sampled on the rising edge.
    \value Mode1 The clock idles low; data is sampled on the falling edge.
    \value Mode2 The clock idles high; data is sampled on the falling edge.
    \value Mode3 The clock idles high; data is sampled on the rising edge.
*/

/*!
    \enum QSpiDevice::BitOrder

    This enum describes the order in which the bits of a word are sent.

    \value MsbFirst The most significant bit first, as most devices expect.
    \value LsbFirst The least significant bit first.
*/

/*!
    \enum QSpiDevice::SpiError

    This enum describes the errors a QSpiDevice can report.

    \value NoError No error occurred.
    \value DeviceNotFoundError The bus or chip select does not exist, or the name is not valid.
    \value PermissionError The user may not open the device.
    \value BusyError The bus is busy.
    \value OpenError The device is already open in this object.
    \value UnsupportedOperationError The controller cannot do what was asked, such as the mode,
        the word size or the length of a transfer, or the platform has no SPI support.
    \value ResourceError The bus went away while the device was open. The device has been
        closed.
    \value NotOpenError The operation needs the device to be open.
    \value TimeoutError The transfer did not finish in time.
    \value IoError The controller reported an I/O error.
    \value UnknownError Any other error.
*/

/*!
    Constructs a device object with the given \a parent. Set the device with setDeviceName()
    before open().
*/
QSpiDevice::QSpiDevice(QObject *parent) : QObject(parent), d(std::make_unique<QSpiDevicePrivate>(this)) {}

/*!
    Constructs a device object for the device called \a name, with the given \a parent. The
    device is not opened.
*/
QSpiDevice::QSpiDevice(const QString &name, QObject *parent) : QSpiDevice(parent) { d->config.name = name; }

/*!
    Destroys the object, closing the device first.
*/
QSpiDevice::~QSpiDevice() { close(); }

/*!
    Sets the device this object opens to \a name, such as \c spidev0.1. It takes effect at the
    next open().
*/
void QSpiDevice::setDeviceName(const QString &name) { d->config.name = name; }

/*!
    Returns the name of the device.

    \sa setDeviceName()
*/
QString QSpiDevice::deviceName() const { return d->config.name; }

/*!
    Sets the clock polarity and phase to \a mode. Returns \c true on success; otherwise returns
    \c false. The default is \c Mode0.
*/
bool QSpiDevice::setMode(Mode mode) {
    QtPrivate::SpiConfig next = d->config;
    next.mode = static_cast<int>(mode);
    return d->change(next);
}

/*!
    Returns the clock polarity and phase.

    \sa setMode()
*/
QSpiDevice::Mode QSpiDevice::mode() const { return static_cast<Mode>(d->config.mode); }

/*!
    Sets the highest clock rate to \a hertz. The controller may use a lower rate it can make.
    Returns \c true on success; otherwise returns \c false. The default is 1 MHz.
*/
bool QSpiDevice::setSpeed(quint32 hertz) {
    if (hertz == 0) {
        d->setError(HwError::Unsupported, QStringLiteral("An SPI clock rate cannot be 0"));
        return false;
    }
    QtPrivate::SpiConfig next = d->config;
    next.speed = hertz;
    return d->change(next);
}

/*!
    Returns the highest clock rate in hertz.

    \sa setSpeed()
*/
quint32 QSpiDevice::speed() const { return d->config.speed; }

/*!
    Sets the size of a word to \a bits, from 1 to 32. Returns \c true on success; otherwise
    returns \c false. The default is 8. With more than 8 bits, each word takes two or four bytes
    of the data, in the byte order of the processor.
*/
bool QSpiDevice::setBitsPerWord(int bits) {
    if (bits < 1 || bits > 32) {
        d->setError(HwError::Unsupported, QStringLiteral("An SPI word must have between 1 and 32 bits"));
        return false;
    }
    QtPrivate::SpiConfig next = d->config;
    next.bitsPerWord = bits;
    return d->change(next);
}

/*!
    Returns the size of a word in bits.

    \sa setBitsPerWord()
*/
int QSpiDevice::bitsPerWord() const { return d->config.bitsPerWord; }

/*!
    Sets the order in which the bits of a word are sent to \a order. Returns \c true on success;
    otherwise returns \c false. The default is \c MsbFirst.
*/
bool QSpiDevice::setBitOrder(BitOrder order) {
    QtPrivate::SpiConfig next = d->config;
    next.lsbFirst = order == LsbFirst;
    return d->change(next);
}

/*!
    Returns the order in which the bits of a word are sent.

    \sa setBitOrder()
*/
QSpiDevice::BitOrder QSpiDevice::bitOrder() const { return d->config.lsbFirst ? LsbFirst : MsbFirst; }

/*!
    Opens the device and configures the bus. Returns \c true on success; otherwise returns
    \c false and sets error().

    \sa close()
*/
bool QSpiDevice::open() {
    if (d->backend) {
        d->setError(HwError::Open, QStringLiteral("The SPI device is already open"));
        return false;
    }
    if (d->config.name.isEmpty()) {
        d->setError(HwError::DeviceNotFound, QStringLiteral("No SPI device name was set"));
        return false;
    }
    std::unique_ptr<QtPrivate::SpiBackend> backend = QtPrivate::createSpiBackend();
    HwFailure failure;
    if (!backend->open(d->config, failure)) {
        d->setError(failure.error, failure.message);
        return false;
    }
    d->backend = std::move(backend);
    d->error = NoError;
    d->errorString.clear();
    return true;
}

/*!
    Closes the device. Does nothing if the device is not open.
*/
void QSpiDevice::close() { d->backend.reset(); }

/*!
    Returns \c true if the device is open; otherwise returns \c false.
*/
bool QSpiDevice::isOpen() const { return d->backend != nullptr; }

/*!
    Sends \a data and returns the bytes received at the same time, as many as were sent. Returns
    an empty QByteArray on failure, when error() tells why.
*/
QByteArray QSpiDevice::transfer(const QByteArray &data) {
    QByteArray result;
    d->transfer(data, 0, true, result);
    return result;
}

/*!
    Sends \a data, ignoring what comes back. Returns \c true on success; otherwise returns
    \c false and sets error().
*/
bool QSpiDevice::write(const QByteArray &data) {
    QByteArray unused;
    return d->transfer(data, 0, false, unused);
}

/*!
    Reads \a size bytes, sending zeros. Returns the bytes, or an empty QByteArray on failure.
*/
QByteArray QSpiDevice::read(qsizetype size) {
    QByteArray result;
    d->transfer(QByteArray(), size, false, result);
    return result;
}

/*!
    Sends \a data, then reads \a size bytes, with chip select held across both, as a command to a
    flash memory and its answer. Returns the bytes read, or an empty QByteArray on failure.
*/
QByteArray QSpiDevice::writeRead(const QByteArray &data, qsizetype size) {
    QByteArray result;
    d->transfer(data, size, false, result);
    return result;
}

/*!
    Returns the last error.

    \sa errorString(), clearError()
*/
QSpiDevice::SpiError QSpiDevice::error() const { return d->error; }

/*!
    Returns a description of the last error, or an empty string if there is none.
*/
QString QSpiDevice::errorString() const { return d->errorString; }

/*!
    Sets error() to \c NoError and clears errorString().
*/
void QSpiDevice::clearError() {
    d->error = NoError;
    d->errorString.clear();
}

#include "moc_qspidevice.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
