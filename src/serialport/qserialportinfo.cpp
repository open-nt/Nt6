// MIT License - see LICENSE
#include "qserialportengine_p.h"
#include "qserialportscanner_p.h"

#include <QtSerialPort/qserialportinfo.h>

#include <utility>

/*!
    \class QSerialPortInfo
    \brief The QSerialPortInfo class describes a serial port on the system.
    \since 6.0

    availablePorts() lists the ports of the machine. Each entry gives the name
    of the port, which can be passed to QSerialPort, and what is known about
    the hardware behind it:

    \code
    const QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : ports) {
        qDebug() << info.portName() << info.description();
        if (info.hasVendorIdentifier())
            qDebug() << "USB vendor" << info.vendorIdentifier();
    }
    \endcode

    \differences
    Ports are found through sysfs, so availablePorts() returns nothing on other
    systems, and only USB and PCI hardware reports identifiers. Pseudo
    terminals are not listed, because they have no entry in sysfs. Objects of
    this class are plain values, not implicitly shared.
    \enddifferences

    \sa QSerialPort
*/

/*!
    Constructs a null object.

    \sa isNull()
*/
QSerialPortInfo::QSerialPortInfo() = default;

/*!
    Constructs an object describing the same port as \a port. The object is
    null if the port is not one of availablePorts().

    \sa isNull()
*/
QSerialPortInfo::QSerialPortInfo(const QSerialPort &port) : QSerialPortInfo(port.portName()) {}

/*!
    Constructs an object describing the port called \a name, as returned by
    portName(). The object is null if no such port is in availablePorts().

    \sa isNull()
*/
QSerialPortInfo::QSerialPortInfo(const QString &name) {
    const QList<QSerialPortInfo> ports = availablePorts();
    for (const QSerialPortInfo &port : ports) {
        if (port.portName() == name) {
            *this = port;
            return;
        }
    }
}

/*!
    Swaps this object with \a other.
*/
void QSerialPortInfo::swap(QSerialPortInfo &other) noexcept { std::swap(*this, other); }

/*!
    Returns the name of the port, such as \c ttyUSB0.

    \sa systemLocation()
*/
QString QSerialPortInfo::portName() const { return m_portName; }

/*!
    Returns the path of the device node, such as \c{/dev/ttyUSB0}.

    \sa portName()
*/
QString QSerialPortInfo::systemLocation() const { return m_systemLocation; }

/*!
    Returns the description of the hardware, or an empty string if it has none.
    For a USB device this is its product string.
*/
QString QSerialPortInfo::description() const { return m_description; }

/*!
    Returns the manufacturer of the hardware, or an empty string if it is not
    known.
*/
QString QSerialPortInfo::manufacturer() const { return m_manufacturer; }

/*!
    Returns the serial number of the hardware, or an empty string if it has
    none.
*/
QString QSerialPortInfo::serialNumber() const { return m_serialNumber; }

/*!
    Returns the vendor identifier, or \c 0 if there is none.

    \sa hasVendorIdentifier()
*/
quint16 QSerialPortInfo::vendorIdentifier() const { return m_vendor; }

/*!
    Returns the product identifier, or \c 0 if there is none.

    \sa hasProductIdentifier()
*/
quint16 QSerialPortInfo::productIdentifier() const { return m_product; }

/*!
    Returns \c true if the port has a vendor identifier; otherwise returns
    \c false.

    \sa vendorIdentifier()
*/
bool QSerialPortInfo::hasVendorIdentifier() const { return m_hasVendor; }

/*!
    Returns \c true if the port has a product identifier; otherwise returns
    \c false.

    \sa productIdentifier()
*/
bool QSerialPortInfo::hasProductIdentifier() const { return m_hasProduct; }

/*!
    Returns \c true if the object does not describe a port; otherwise returns
    \c false.
*/
bool QSerialPortInfo::isNull() const { return m_portName.isEmpty(); }

/*!
    Returns the baud rates the system has a named constant for. Other rates may
    still work; see QSerialPort::setBaudRate().
*/
QList<qint32> QSerialPortInfo::standardBaudRates() { return QtPrivate::serialStandardBaudRates(); }

/*!
    Returns the serial ports of the machine, sorted by name.
*/
QList<QSerialPortInfo> QSerialPortInfo::availablePorts() {
    return QtPrivate::SerialPortScanner::scan(QString("/sys"), QString("/dev"));
}
