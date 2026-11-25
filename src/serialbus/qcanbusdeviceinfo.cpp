// MIT License - see LICENSE
#include <QtSerialBus/qcanbusdeviceinfo.h>

#include "qcanbus_p.h"

/*!
    \class QCanBusDeviceInfo
    \inmodule NtSerialBus
    \brief The QCanBusDeviceInfo class describes a CAN interface that a backend can open.
    \since 6.0

    QCanBus::availableDevices() lists them.

    \differences
    None.
    \enddifferences

    \sa QCanBus
*/

QCanBusDeviceInfo::QCanBusDeviceInfo(QCanBusDeviceInfoPrivate *d) : d_ptr(d) {}

/*!
    Constructs a copy of \a other.
*/
QCanBusDeviceInfo::QCanBusDeviceInfo(const QCanBusDeviceInfo &other) = default;

/*!
    Constructs an info by moving \a other.
*/
QCanBusDeviceInfo::QCanBusDeviceInfo(QCanBusDeviceInfo &&other) noexcept = default;

/*!
    Assigns \a other to this info.
*/
QCanBusDeviceInfo &QCanBusDeviceInfo::operator=(const QCanBusDeviceInfo &other) = default;

/*!
    Moves \a other into this info.
*/
QCanBusDeviceInfo &QCanBusDeviceInfo::operator=(QCanBusDeviceInfo &&other) noexcept = default;

/*!
    Destroys the info.
*/
QCanBusDeviceInfo::~QCanBusDeviceInfo() = default;

/*!
    Returns the name of the backend, such as \c socketcan.
*/
QString QCanBusDeviceInfo::plugin() const { return d_ptr->plugin; }

/*!
    Returns the name of the interface, such as \c can0, as QCanBus::createDevice() takes it.
*/
QString QCanBusDeviceInfo::name() const { return d_ptr->name; }

/*!
    Returns a description of the interface, or an empty string.
*/
QString QCanBusDeviceInfo::description() const { return d_ptr->description; }

/*!
    Returns the serial number of the adapter, or an empty string.
*/
QString QCanBusDeviceInfo::serialNumber() const { return d_ptr->serialNumber; }

/*!
    Returns another name of the interface, or an empty string.
*/
QString QCanBusDeviceInfo::alias() const { return d_ptr->alias; }

/*!
    Returns the channel of the adapter, or 0.
*/
int QCanBusDeviceInfo::channel() const { return d_ptr->channel; }

/*!
    Returns \c true if the interface supports CAN FD.
*/
bool QCanBusDeviceInfo::hasFlexibleDataRate() const { return d_ptr->hasFlexibleDataRate; }

/*!
    Returns \c true if the interface is virtual rather than a controller on a bus.
*/
bool QCanBusDeviceInfo::isVirtual() const { return d_ptr->isVirtual; }
