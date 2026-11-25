// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qstring.h>
#include <QtSerialBus/qcanbusdeviceinfo.h>

class QCanBusDevice;

class QCanBusFactory {
public:
    virtual QCanBusDevice *createDevice(const QString &interfaceName, QString *errorMessage) const = 0;
    virtual QList<QCanBusDeviceInfo> availableDevices(QString *errorMessage) const = 0;

protected:
    QCanBusFactory();
    virtual ~QCanBusFactory();
    QCanBusFactory(const QCanBusFactory &) = delete;
    QCanBusFactory &operator=(const QCanBusFactory &) = delete;
};
