// MIT License - see LICENSE
//
// What the parts of Nt SerialBus share: the data of a QCanBusDeviceInfo and the factories of the built-in backends,
// each defined in its own file (qcanbus_virtual.cpp, qcanbus_socketcan.cpp, src/zephyr/qcanbus_zephyr.cpp).
#pragma once

#include <QtCore/qshareddata.h>
#include <QtCore/qstring.h>

class QCanBusFactory;

class QCanBusDeviceInfoPrivate : public QSharedData {
public:
    QString plugin;
    QString name;
    QString description;
    QString serialNumber;
    QString alias;
    int channel = 0;
    bool hasFlexibleDataRate = false;
    bool isVirtual = false;
};

namespace QtSerialBusPrivate {
QCanBusFactory *virtualCanFactory();
#if NT_SERIALBUS_SOCKETCAN
QCanBusFactory *socketCanFactory();
#endif
#if NT_SERIALBUS_ZEPHYR
QCanBusFactory *zephyrCanFactory();
#endif
// Whether frame passes filters (a QList<QCanBusDevice::Filter>); every frame passes an empty list.
bool passesFilters(const class QCanBusFrame &frame, const class QVariant &filters);
} // namespace QtSerialBusPrivate
