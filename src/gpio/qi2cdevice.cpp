// MIT License - see LICENSE
#include "qperipheral_p.h"

#include <QtGpio/qi2cdevice.h>

using QtPrivate::HwError;
using QtPrivate::HwFailure;

class QI2cDevicePrivate {
public:
    explicit QI2cDevicePrivate(QI2cDevice *owner) : q(owner) {}

    void setError(HwError e, const QString &message) {
        error = static_cast<QI2cDevice::I2cError>(e);
        errorString = message;
        if (e != HwError::NoError)
            emit q->errorOccurred(error);
    }

    bool transfer(const QByteArray &data, qsizetype size, QByteArray &read) {
        if (!backend) {
            setError(HwError::NotOpen, QStringLiteral("The I2C device is not open"));
            return false;
        }
        if (size < 0 || size > 0xffff || data.size() > 0xffff || (data.isEmpty() && size == 0)) {
            setError(HwError::Unsupported,
                     QStringLiteral("An I2C transfer must move between 1 and 65535 bytes each way"));
            return false;
        }
        HwFailure failure;
        if (!backend->transfer(static_cast<quint16>(address), data, size, read, failure)) {
            if (failure.error == HwError::Resource)
                q->close();
            setError(failure.error, failure.message);
            read.clear();
            return false;
        }
        return true;
    }

    QI2cDevice *q;
    QString bus;
    int address = 0;
    std::unique_ptr<QtPrivate::I2cBackend> backend;
    QI2cDevice::I2cError error = QI2cDevice::NoError;
    QString errorString;
};

/*!
    \class QI2cDevice
    \inmodule NtGpio
    \brief The QI2cDevice class talks to one device on an I2C bus.
    \since 6.0

    A QI2cDevice is a bus and the address of a device on it, such as a sensor, an EEPROM or a
    port expander. Each call is one transaction on the bus:

    \code
    QI2cDevice rtc(QStringLiteral("i2c-1"), 0x68);
    if (!rtc.open())
        qFatal("%s", qPrintable(rtc.errorString()));
    const QByteArray time = rtc.readRegister(0x00, 7);
    rtc.writeRegister(0x0e, QByteArray(1, '\x1c'));
    \endcode

    writeRead() and readRegister() send, then read, with a repeated start in between, as most
    devices expect for reading a register. See \l{Using PWM, Analog I/O, I2C and SPI} for how
    buses are named.

    \differences
    Qt has no I2C class; QI2cDevice is an Nt addition. It works on Linux, through \c /dev/i2c-N,
    and on Zephyr, through the I2C driver API. Addresses are 7-bit; there is no 10-bit
    addressing, no SMBus protocol and no target (slave) mode, and the bus speed is set by the
    platform, not by this class. Transfers are synchronous.
    \enddifferences

    \sa QSpiDevice
*/

/*!
    \enum QI2cDevice::I2cError

    This enum describes the errors a QI2cDevice can report.

    \value NoError No error occurred.
    \value DeviceNotFoundError The bus does not exist, or its name is not valid.
    \value PermissionError The user may not open the bus.
    \value BusyError The bus is busy, or arbitration was lost to another controller.
    \value OpenError The device is already open in this object.
    \value UnsupportedOperationError The address or the length of a transfer is out of range,
        the adapter cannot do plain I2C transfers, or the platform has no I2C support.
    \value ResourceError The bus went away while the device was open. The device has been
        closed.
    \value NotOpenError The operation needs the device to be open.
    \value TimeoutError The transfer did not finish in time, for example because a device held
        the clock low.
    \value IoError No device answered at the address, or it did not acknowledge a byte. The
        device stays open.
    \value UnknownError Any other error.
*/

/*!
    Constructs a device object with the given \a parent. Set the bus with setBusName() and the
    address with setAddress() before open().
*/
QI2cDevice::QI2cDevice(QObject *parent) : QObject(parent), d(std::make_unique<QI2cDevicePrivate>(this)) {}

/*!
    Constructs a device object for the device at \a address on the bus called \a bus, with the
    given \a parent. The device is not opened.
*/
QI2cDevice::QI2cDevice(const QString &bus, int address, QObject *parent) : QI2cDevice(parent) {
    d->bus = bus;
    d->address = address;
}

/*!
    Destroys the object, closing the device first.
*/
QI2cDevice::~QI2cDevice() { close(); }

/*!
    Sets the bus this object opens to \a bus, such as \c i2c-1. It takes effect at the next
    open().
*/
void QI2cDevice::setBusName(const QString &bus) { d->bus = bus; }

/*!
    Returns the name of the bus.

    \sa setBusName()
*/
QString QI2cDevice::busName() const { return d->bus; }

/*!
    Sets the 7-bit address of the device to \a address, from 0x00 to 0x7f. It takes effect at
    once; one object can talk to several devices on the same bus by changing it.
*/
void QI2cDevice::setAddress(int address) { d->address = address; }

/*!
    Returns the address of the device.

    \sa setAddress()
*/
int QI2cDevice::address() const { return d->address; }

/*!
    Opens the bus. Returns \c true on success; otherwise returns \c false and sets error(). No
    transfer is made, so open() succeeds even if no device answers at address().

    \sa close()
*/
bool QI2cDevice::open() {
    if (d->backend) {
        d->setError(HwError::Open, QStringLiteral("The I2C device is already open"));
        return false;
    }
    if (d->bus.isEmpty()) {
        d->setError(HwError::DeviceNotFound, QStringLiteral("No I2C bus name was set"));
        return false;
    }
    if (d->address < 0 || d->address > 0x7f) {
        d->setError(HwError::Unsupported, QStringLiteral("I2C address %1 is not a 7-bit address").arg(d->address));
        return false;
    }
    std::unique_ptr<QtPrivate::I2cBackend> backend = QtPrivate::createI2cBackend();
    HwFailure failure;
    if (!backend->open(d->bus, failure)) {
        d->setError(failure.error, failure.message);
        return false;
    }
    d->backend = std::move(backend);
    d->error = NoError;
    d->errorString.clear();
    return true;
}

/*!
    Closes the bus. Does nothing if the device is not open.
*/
void QI2cDevice::close() { d->backend.reset(); }

/*!
    Returns \c true if the device is open; otherwise returns \c false.
*/
bool QI2cDevice::isOpen() const { return d->backend != nullptr; }

/*!
    Writes \a data to the device in one transaction. Returns \c true on success; otherwise
    returns \c false and sets error().
*/
bool QI2cDevice::write(const QByteArray &data) {
    QByteArray unused;
    return d->transfer(data, 0, unused);
}

/*!
    Reads \a size bytes from the device in one transaction. Returns the bytes, or an empty
    QByteArray on failure, when error() tells why.
*/
QByteArray QI2cDevice::read(qsizetype size) {
    QByteArray result;
    d->transfer(QByteArray(), size, result);
    return result;
}

/*!
    Writes \a data to the device, then reads \a size bytes from it, with a repeated start in
    between. Returns the bytes read, or an empty QByteArray on failure.

    \sa readRegister()
*/
QByteArray QI2cDevice::writeRead(const QByteArray &data, qsizetype size) {
    QByteArray result;
    d->transfer(data, size, result);
    return result;
}

/*!
    Writes \a data to the register \a reg of the device: the register number, then the data, in
    one transaction. Returns \c true on success; otherwise returns \c false.
*/
bool QI2cDevice::writeRegister(quint8 reg, const QByteArray &data) {
    QByteArray message(1, static_cast<char>(reg));
    message.append(data);
    return write(message);
}

/*!
    Reads \a size bytes from the register \a reg of the device, the register number written and
    the data read with a repeated start in between. Returns the bytes, or an empty QByteArray on
    failure.
*/
QByteArray QI2cDevice::readRegister(quint8 reg, qsizetype size) {
    return writeRead(QByteArray(1, static_cast<char>(reg)), size);
}

/*!
    Returns the last error.

    \sa errorString(), clearError()
*/
QI2cDevice::I2cError QI2cDevice::error() const { return d->error; }

/*!
    Returns a description of the last error, or an empty string if there is none.
*/
QString QI2cDevice::errorString() const { return d->errorString; }

/*!
    Sets error() to \c NoError and clears errorString().
*/
void QI2cDevice::clearError() {
    d->error = NoError;
    d->errorString.clear();
}

#include "moc_qi2cdevice.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
